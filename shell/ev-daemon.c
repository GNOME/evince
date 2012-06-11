/* ev-daemon.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2009 Carlos Garcia Campos  <carlosgc@gnome.org>
 * Copyright © 2010 Christian Persch
 *
 * Evince is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Evince is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "ev-daemon-gdbus-generated.h"

#define EV_DBUS_DAEMON_NAME             "org.gnome.evince.Daemon"
#define EV_DBUS_DAEMON_INTERFACE_NAME   "org.gnome.evince.Daemon"
#define EV_DBUS_DAEMON_OBJECT_PATH      "/org/gnome/evince/Daemon"

#define EV_DBUS_WINDOW_INTERFACE_NAME   "org.gnome.evince.Window"

#define DAEMON_TIMEOUT (30) /* seconds */

#define LOG g_printerr

static GList *ev_daemon_docs = NULL;
static guint kill_timer_id;
static GHashTable *pending_invocations = NULL;

typedef struct {
	gchar *dbus_name;
	gchar *uri;
        guint  watch_id;
	guint  loaded_id;
} EvDoc;

static void
ev_doc_free (EvDoc *doc)
{
	if (!doc)
		return;

	g_free (doc->dbus_name);
	g_free (doc->uri);

        g_bus_unwatch_name (doc->watch_id);

	g_free (doc);
}

static EvDoc *
ev_daemon_find_doc (const gchar *uri)
{
	GList *l;

	for (l = ev_daemon_docs; l != NULL; l = l->next) {
		EvDoc *doc = (EvDoc *)l->data;

		if (strcmp (doc->uri, uri) == 0)
			return doc;
	}

	return NULL;
}

static void
ev_daemon_stop_killtimer (void)
{
	if (kill_timer_id != 0)
		g_source_remove (kill_timer_id);
	kill_timer_id = 0;
}

static gboolean
ev_daemon_shutdown (gpointer user_data)
{
        GMainLoop *loop = (GMainLoop *) user_data;

        LOG ("Timeout; exiting daemon.\n");

        if (g_main_loop_is_running (loop))
                g_main_loop_quit (loop);

        return FALSE;
}

static void
ev_daemon_maybe_start_killtimer (gpointer data)
{
	ev_daemon_stop_killtimer ();
        if (ev_daemon_docs != NULL)
                return;

	kill_timer_id = g_timeout_add_seconds (DAEMON_TIMEOUT,
                                               (GSourceFunc) ev_daemon_shutdown,
                                               data);
}

static gboolean
spawn_evince (const gchar *uri)
{
	gchar   *argv[3];
	gboolean retval;
	GError  *error = NULL;

	/* TODO Check that the uri exists */
	argv[0] = g_build_filename (BINDIR, "evince", NULL);
	argv[1] = (gchar *) uri;
	argv[2] = NULL;

	retval = g_spawn_async (NULL /* wd */, argv, NULL /* env */,
				0, NULL, NULL, NULL, &error);
	if (!retval) {
		g_printerr ("Error spawning evince for uri %s: %s\n", uri, error->message);
		g_error_free (error);
	}
	g_free (argv[0]);

	return retval;
}

static void
name_appeared_cb (GDBusConnection *connection,
                  const gchar     *name,
                  const gchar     *name_owner,
                  gpointer         user_data)
{
        LOG ("Watch name'%s' appeared with owner '%s'\n", name, name_owner);
}

static void
name_vanished_cb (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
	GList *l;

        LOG ("Watch name'%s' disappeared\n", name);

        for (l = ev_daemon_docs; l != NULL; l = l->next) {
                EvDoc *doc = (EvDoc *) l->data;

                if (strcmp (doc->dbus_name, name) != 0)
                        continue;

                LOG ("Watch found URI '%s' for name; removing\n", doc->uri);

                ev_daemon_docs = g_list_delete_link (ev_daemon_docs, l);
                ev_doc_free (doc);
                
                ev_daemon_maybe_start_killtimer (user_data);
                return;
        }
}

static void
process_pending_invocations (const gchar *uri,
			     const gchar *dbus_name)
{
	GList *l;
	GList *uri_invocations;

	LOG ("RegisterDocument process pending invocations for URI %s\n", uri);
	uri_invocations = g_hash_table_lookup (pending_invocations, uri);

	for (l = uri_invocations; l != NULL; l = l->next) {
		GDBusMethodInvocation *invocation;

		invocation = (GDBusMethodInvocation *)l->data;
		g_dbus_method_invocation_return_value (invocation,
						       g_variant_new ("(s)", dbus_name));
	}

	g_list_free (uri_invocations);
	g_hash_table_remove (pending_invocations, uri);
}

static void
document_loaded_cb (GDBusConnection *connection,
		    const gchar     *sender_name,
		    const gchar     *object_path,
		    const gchar     *interface_name,
		    const gchar     *signal_name,
		    GVariant        *parameters,
		    EvDoc           *doc)
{
	const gchar *uri;

	g_variant_get (parameters, "(&s)", &uri);
	if (strcmp (uri, doc->uri) == 0)
		process_pending_invocations (uri, sender_name);
	g_dbus_connection_signal_unsubscribe (connection, doc->loaded_id);
}

static gboolean
handle_register_document_cb (EvDaemon *object,
                             GDBusMethodInvocation *invocation,
                             const gchar *uri,
                             gpointer user_data)
{
        GDBusConnection *connection;
        const char *sender;
        EvDoc       *doc;

        doc = ev_daemon_find_doc (uri);
        if (doc != NULL) {
                LOG ("RegisterDocument found owner '%s' for URI '%s'\n", doc->dbus_name, uri);
                ev_daemon_complete_register_document (object, invocation, doc->dbus_name);

                return TRUE;
        }

        ev_daemon_stop_killtimer ();

        sender = g_dbus_method_invocation_get_sender (invocation);
        connection = g_dbus_method_invocation_get_connection (invocation);

        doc = g_new (EvDoc, 1);
        doc->dbus_name = g_strdup (sender);
        doc->uri = g_strdup (uri);

        doc->loaded_id = g_dbus_connection_signal_subscribe (connection,
                                                             doc->dbus_name,
                                                             EV_DBUS_WINDOW_INTERFACE_NAME,
                                                             "DocumentLoaded",
                                                             NULL,
                                                             NULL,
                                                             0,
                                                             (GDBusSignalCallback) document_loaded_cb,
                                                             doc,
                                                             NULL);
        doc->watch_id = g_bus_watch_name_on_connection (connection,
                                                        sender,
                                                        G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                        name_appeared_cb,
                                                        name_vanished_cb,
                                                        user_data, NULL);

        LOG ("RegisterDocument registered owner '%s' for URI '%s'\n", doc->dbus_name, uri);
        ev_daemon_docs = g_list_prepend (ev_daemon_docs, doc);

        ev_daemon_complete_register_document (object, invocation, "");

        return TRUE;
}

static gboolean
handle_unregister_document_cb (EvDaemon *object,
                               GDBusMethodInvocation *invocation,
                               const gchar *uri,
                               gpointer user_data)
{
        EvDoc *doc;
        const char *sender;

        LOG ("UnregisterDocument URI '%s'\n", uri);

        doc = ev_daemon_find_doc (uri);
        if (doc == NULL) {
                LOG ("UnregisterDocument URI was not registered!\n");
                g_dbus_method_invocation_return_error_literal (invocation,
                                                               G_DBUS_ERROR,
                                                               G_DBUS_ERROR_INVALID_ARGS,
                                                               "URI not registered");
                return TRUE;
        }

        sender = g_dbus_method_invocation_get_sender (invocation);
        if (strcmp (doc->dbus_name, sender) != 0) {
                LOG ("UnregisterDocument called by non-owner (owner '%s' sender '%s')\n",
                     doc->dbus_name, sender);

                g_dbus_method_invocation_return_error_literal (invocation,
                                                               G_DBUS_ERROR,
                                                               G_DBUS_ERROR_BAD_ADDRESS,
                                                               "Only owner can call this method");
                return TRUE;
        }

        ev_daemon_docs = g_list_remove (ev_daemon_docs, doc);
        ev_doc_free (doc);
        ev_daemon_maybe_start_killtimer (user_data);

        ev_daemon_complete_unregister_document (object, invocation);

        return TRUE;
}

static gboolean
handle_find_document_cb (EvDaemon *object,
                         GDBusMethodInvocation *invocation,
                         const gchar *uri,
                         gboolean spawn,
                         gpointer user_data)
{
        EvDoc *doc;

        LOG ("FindDocument URI '%s' \n", uri);

        doc = ev_daemon_find_doc (uri);
        if (doc != NULL) {
                ev_daemon_complete_find_document (object, invocation, doc->dbus_name);

                return TRUE;
        }

        if (spawn) {
                GList *uri_invocations;
                gboolean ret_val = TRUE;

                uri_invocations = g_hash_table_lookup (pending_invocations, uri);

                if (uri_invocations == NULL) {
                        /* Only spawn once. */
                        ret_val = spawn_evince (uri);
                }

                if (ret_val) {
                        /* Only defer DBUS answer if evince was succesfully spawned */
                        uri_invocations = g_list_prepend (uri_invocations, invocation);
                        g_hash_table_insert (pending_invocations,
                                             g_strdup (uri),
                                             uri_invocations);
                        return TRUE;
                }
        }

        LOG ("FindDocument URI '%s' was not registered!\n", uri);
        // FIXME: shouldn't this return an error then?
        ev_daemon_complete_find_document (object, invocation, "");

        return TRUE;
}

static void
bus_acquired_cb (GDBusConnection *connection,
		 const gchar     *name,
		 gpointer         user_data)
{
	GMainLoop *loop = (GMainLoop *) user_data;
        EvDaemon *skeleton;
	GError    *error = NULL;

        skeleton = ev_daemon_skeleton_new ();
        if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (skeleton),
                                               connection,
                                               EV_DBUS_DAEMON_OBJECT_PATH,
                                               &error)) {
                g_printerr ("Failed to export object: %s\n", error->message);
		g_error_free (error);

		if (g_main_loop_is_running (loop))
			g_main_loop_quit (loop);
	}

        g_signal_connect (skeleton, "handle-register-document",
                          G_CALLBACK (handle_register_document_cb), loop);
        g_signal_connect (skeleton, "handle-unregister-document",
                          G_CALLBACK (handle_unregister_document_cb), loop);
        g_signal_connect (skeleton, "handle-find-document",
                          G_CALLBACK (handle_find_document_cb), loop);
}

static void
name_acquired_cb (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
	ev_daemon_maybe_start_killtimer (user_data);
}

static void
name_lost_cb (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
          GMainLoop *loop = (GMainLoop *) user_data;

          /* Failed to acquire the name; exit daemon */
          if (g_main_loop_is_running (loop))
                  g_main_loop_quit (loop);
}

gint
main (gint argc, gchar **argv)
{
	GMainLoop *loop;
        guint owner_id;

        g_set_prgname ("evince-daemon");

	g_type_init ();

	loop = g_main_loop_new (NULL, FALSE);

	pending_invocations = g_hash_table_new_full (g_str_hash,
						     g_str_equal,
						     (GDestroyNotify)g_free,
						     NULL);

        owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
				   EV_DBUS_DAEMON_NAME,
				   G_BUS_NAME_OWNER_FLAGS_NONE,
				   bus_acquired_cb,
				   name_acquired_cb,
				   name_lost_cb,
				   g_main_loop_ref (loop),
				   (GDestroyNotify) g_main_loop_unref);

        g_main_loop_run (loop);

        g_bus_unown_name (owner_id);

        g_main_loop_unref (loop);
        g_list_foreach (ev_daemon_docs, (GFunc)ev_doc_free, NULL);
        g_list_free (ev_daemon_docs);
        g_hash_table_destroy (pending_invocations);

	return 0;
}
