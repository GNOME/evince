/* ev-daemon.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2009 Carlos Garcia Campos  <carlosgc@gnome.org>
 * Copyright © 2010, 2012 Christian Persch
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

#define G_LOG_DOMAIN "EvinceDaemon"
#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "ev-daemon-gdbus-generated.h"

#define DAEMON_TIMEOUT (30) /* seconds */

#define LOG g_debug

#define EV_TYPE_DAEMON_APPLICATION              (ev_daemon_application_get_type ())
#define EV_DAEMON_APPLICATION(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), EV_TYPE_DAEMON_APPLICATION, EvDaemonApplication))
#define EV_DAEMON_APPLICATION_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_DAEMON_APPLICATION, EvDaemonApplicationClass))
#define EV_IS_DAEMON_APPLICATION(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), EV_TYPE_DAEMON_APPLICATION))
#define EV_IS_DAEMON_APPLICATION_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_DAEMON_APPLICATION))
#define EV_DAEMON_APPLICATION_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_DAEMON_APPLICATION, EvDaemonApplicationClass))

typedef struct _EvDaemonApplication        EvDaemonApplication;
typedef struct _EvDaemonApplicationClass   EvDaemonApplicationClass;

struct _EvDaemonApplicationClass {
        GApplicationClass parent_class;
};

struct _EvDaemonApplication
{
        GApplication parent_instance;

        EvDaemon   *daemon;
        GHashTable *pending_invocations;
        GList      *docs;
};

static GType ev_daemon_application_get_type (void);
G_DEFINE_TYPE (EvDaemonApplication, ev_daemon_application, G_TYPE_APPLICATION)

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
ev_daemon_application_find_doc (EvDaemonApplication *application,
                                const gchar *uri)
{
	GList *l;

	for (l = application->docs; l != NULL; l = l->next) {
		EvDoc *doc = (EvDoc *)l->data;

		if (strcmp (doc->uri, uri) == 0)
			return doc;
	}

	return NULL;
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
        LOG ("Watch name'%s' appeared with owner '%s'", name, name_owner);
}

static void
name_vanished_cb (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
        EvDaemonApplication *application = EV_DAEMON_APPLICATION (user_data);
	GList *l;

        LOG ("Watch name'%s' disappeared", name);

        for (l = application->docs; l != NULL; l = l->next) {
                EvDoc *doc = (EvDoc *) l->data;

                if (strcmp (doc->dbus_name, name) != 0)
                        continue;

                LOG ("Watch found URI '%s' for name; removing", doc->uri);

                application->docs = g_list_delete_link (application->docs, l);
                ev_doc_free (doc);

                g_application_release (G_APPLICATION (application));

                return;
        }
}

static void
process_pending_invocations (EvDaemonApplication *application,
                             const gchar *uri,
			     const gchar *dbus_name)
{
	GList *l;
	GList *uri_invocations;

	LOG ("RegisterDocument process pending invocations for URI %s", uri);
	uri_invocations = g_hash_table_lookup (application->pending_invocations, uri);

	for (l = uri_invocations; l != NULL; l = l->next) {
		GDBusMethodInvocation *invocation;

		invocation = (GDBusMethodInvocation *)l->data;
		g_dbus_method_invocation_return_value (invocation,
						       g_variant_new ("(s)", dbus_name));
	}

	g_list_free (uri_invocations);
	g_hash_table_remove (application->pending_invocations, uri);
}

static void
document_loaded_cb (GDBusConnection *connection,
		    const gchar     *sender_name,
		    const gchar     *object_path,
		    const gchar     *interface_name,
		    const gchar     *signal_name,
		    GVariant        *parameters,
		    gpointer         user_data)
{
        EvDaemonApplication *application = EV_DAEMON_APPLICATION (user_data);
        const gchar *uri;
        EvDoc *doc;

	g_variant_get (parameters, "(&s)", &uri);
        doc = ev_daemon_application_find_doc (application, uri);

	if (doc == NULL)
		return;
        if (strcmp (uri, doc->uri) == 0) {
		process_pending_invocations (application, uri, sender_name);
        }

	g_dbus_connection_signal_unsubscribe (connection, doc->loaded_id);
        doc->loaded_id = 0;
}

static gboolean
handle_register_document_cb (EvDaemon              *object,
                             GDBusMethodInvocation *invocation,
                             const gchar           *uri,
                             EvDaemonApplication   *application)
{
        GDBusConnection *connection;
        const char *sender;
        EvDoc       *doc;

        doc = ev_daemon_application_find_doc (application, uri);
        if (doc != NULL) {
                LOG ("RegisterDocument found owner '%s' for URI '%s'", doc->dbus_name, uri);
                ev_daemon_complete_register_document (object, invocation, doc->dbus_name);

                return TRUE;
        }

        sender = g_dbus_method_invocation_get_sender (invocation);
        connection = g_dbus_method_invocation_get_connection (invocation);

        LOG ("RegisterDocument registered owner '%s' for URI '%s'", sender, uri);

        doc = g_new (EvDoc, 1);
        doc->dbus_name = g_strdup (sender);
        doc->uri = g_strdup (uri);

        application->docs = g_list_prepend (application->docs, doc);

        doc->loaded_id = g_dbus_connection_signal_subscribe (connection,
                                                             doc->dbus_name,
                                                             EV_DBUS_WINDOW_INTERFACE_NAME,
                                                             "DocumentLoaded",
                                                             NULL,
                                                             NULL,
                                                             0,
                                                             document_loaded_cb,
                                                             application, NULL);
        doc->watch_id = g_bus_watch_name_on_connection (connection,
                                                        sender,
                                                        G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                        name_appeared_cb,
                                                        name_vanished_cb,
                                                        application, NULL);

        ev_daemon_complete_register_document (object, invocation, "");

        g_application_hold (G_APPLICATION (application));

        return TRUE;
}

static gboolean
handle_unregister_document_cb (EvDaemon              *object,
                               GDBusMethodInvocation *invocation,
                               const gchar           *uri,
                               EvDaemonApplication   *application)
{
        EvDoc *doc;
        const char *sender;

        LOG ("UnregisterDocument URI '%s'", uri);

        doc = ev_daemon_application_find_doc (application, uri);
        if (doc == NULL) {
                LOG ("UnregisterDocument URI was not registered!");
                g_dbus_method_invocation_return_error_literal (invocation,
                                                               G_DBUS_ERROR,
                                                               G_DBUS_ERROR_INVALID_ARGS,
                                                               "URI not registered");
                return TRUE;
        }

        sender = g_dbus_method_invocation_get_sender (invocation);
        if (strcmp (doc->dbus_name, sender) != 0) {
                LOG ("UnregisterDocument called by non-owner (owner '%s' sender '%s')",
                     doc->dbus_name, sender);

                g_dbus_method_invocation_return_error_literal (invocation,
                                                               G_DBUS_ERROR,
                                                               G_DBUS_ERROR_BAD_ADDRESS,
                                                               "Only owner can call this method");
                return TRUE;
        }

        application->docs = g_list_remove (application->docs, doc);

        if (doc->loaded_id != 0) {
                g_dbus_connection_signal_unsubscribe (g_dbus_method_invocation_get_connection (invocation),
                                                      doc->loaded_id);
                doc->loaded_id = 0;
        }

        ev_doc_free (doc);

        ev_daemon_complete_unregister_document (object, invocation);

        g_application_release (G_APPLICATION (application));

        return TRUE;
}

static gboolean
handle_find_document_cb (EvDaemon              *object,
                         GDBusMethodInvocation *invocation,
                         const gchar           *uri,
                         gboolean               spawn,
                         EvDaemonApplication   *application)
{
        EvDoc *doc;

        LOG ("FindDocument URI '%s'", uri);

        doc = ev_daemon_application_find_doc (application, uri);
        if (doc != NULL) {
                ev_daemon_complete_find_document (object, invocation, doc->dbus_name);

                return TRUE;
        }

        if (spawn) {
                GList *uri_invocations;
                gboolean ret_val = TRUE;

                uri_invocations = g_hash_table_lookup (application->pending_invocations, uri);

                if (uri_invocations == NULL) {
                        /* Only spawn once. */
                        ret_val = spawn_evince (uri);
                }

                if (ret_val) {
                        /* Only defer DBUS answer if evince was succesfully spawned */
                        uri_invocations = g_list_prepend (uri_invocations, invocation);
                        g_hash_table_insert (application->pending_invocations,
                                             g_strdup (uri),
                                             uri_invocations);
                        return TRUE;
                }
        }

        LOG ("FindDocument URI '%s' was not registered!", uri);
        // FIXME: shouldn't this return an error then?
        ev_daemon_complete_find_document (object, invocation, "");

        return TRUE;
}

/* ------------------------------------------------------------------------- */

static gboolean
ev_daemon_application_dbus_register (GApplication    *gapplication,
                                     GDBusConnection *connection,
                                     const gchar     *object_path,
                                     GError         **error)
{
        EvDaemonApplication *application = EV_DAEMON_APPLICATION (gapplication);
        EvDaemon *skeleton;

        if (!G_APPLICATION_CLASS (ev_daemon_application_parent_class)->dbus_register (gapplication,
                                                                                      connection,
                                                                                      object_path,
                                                                                      error))
                return FALSE;

        skeleton = ev_daemon_skeleton_new ();
        if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (skeleton),
                                               connection,
                                               EV_DBUS_DAEMON_OBJECT_PATH,
                                               error)) {
                g_object_unref (skeleton);
                return FALSE;
        }

        application->daemon = skeleton;
        g_signal_connect (skeleton, "handle-register-document",
                          G_CALLBACK (handle_register_document_cb), application);
        g_signal_connect (skeleton, "handle-unregister-document",
                          G_CALLBACK (handle_unregister_document_cb), application);
        g_signal_connect (skeleton, "handle-find-document",
                          G_CALLBACK (handle_find_document_cb), application);
        return TRUE;
}

static void
ev_daemon_application_dbus_unregister (GApplication    *gapplication,
                                       GDBusConnection *connection,
                                       const gchar     *object_path)
{
        EvDaemonApplication *application = EV_DAEMON_APPLICATION (gapplication);

        if (application->daemon) {
                g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (application->daemon));
		g_clear_object (&application->daemon);
        }

        G_APPLICATION_CLASS (ev_daemon_application_parent_class)->dbus_unregister (gapplication,
                                                                                   connection,
                                                                                   object_path);
}

static void
ev_daemon_application_init (EvDaemonApplication *application)
{
        application->pending_invocations = g_hash_table_new_full (g_str_hash,
                                                                  g_str_equal,
                                                                  (GDestroyNotify) g_free,
                                                                  NULL);
}

static void
ev_daemon_application_finalize (GObject *object)
{
        EvDaemonApplication *application = EV_DAEMON_APPLICATION (object);

        g_warn_if_fail (g_hash_table_size (application->pending_invocations) == 0);
        g_hash_table_destroy (application->pending_invocations);

        g_list_free_full (application->docs, (GDestroyNotify) ev_doc_free);

        G_OBJECT_CLASS (ev_daemon_application_parent_class)->finalize (object);
}

static void
ev_daemon_application_class_init (EvDaemonApplicationClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GApplicationClass *g_application_class = G_APPLICATION_CLASS (klass);

        object_class->finalize = ev_daemon_application_finalize;

        g_application_class->dbus_register = ev_daemon_application_dbus_register;
        g_application_class->dbus_unregister = ev_daemon_application_dbus_unregister;
}

/* ------------------------------------------------------------------------- */

gint
main (gint argc, gchar **argv)
{
        GApplication *application;
        const GApplicationFlags flags = G_APPLICATION_IS_SERVICE;
        GError *error = NULL;
        int status;

        g_set_prgname ("evince-daemon");

        application = g_object_new (EV_TYPE_DAEMON_APPLICATION,
                                    "application-id", EV_DBUS_DAEMON_NAME,
                                    "flags", flags,
                                    NULL);
        g_application_set_inactivity_timeout (application, DAEMON_TIMEOUT);

        if (!g_application_register (application, NULL, &error)) {
                g_printerr ("Failed to register: %s\n", error->message);
                g_error_free (error);
                g_object_unref (application);

                return 1;
        }

        status = g_application_run (application, 0, NULL);
        g_object_unref (application);

	return status;
}
