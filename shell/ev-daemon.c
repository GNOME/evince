/* ev-metadata.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2009 Carlos Garcia Campos  <carlosgc@gnome.org>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <dbus/dbus-glib-bindings.h>
#include <dbus/dbus-glib-lowlevel.h>

#define EV_DBUS_DAEMON_NAME        "org.gnome.evince.Daemon"
#define EV_DBUS_DAEMON_OBJECT_PATH "/org/gnome/evince/Daemon"

#define EV_TYPE_DAEMON                     (ev_daemon_get_type ())
#define EV_DAEMON(object)                  (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_DAEMON, EvDaemon))
#define EV_DAEMON_CLASS(klass)             (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_DAEMON, EvDaemonClass))
#define EV_IS_DAEMON(object)               (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_DAEMON))

typedef struct _EvDaemon      EvDaemon;
typedef struct _EvDaemonClass EvDaemonClass;

struct _EvDaemon {
	GObject base;

	DBusGProxy *bus_proxy;

	GList      *docs;
	guint       n_docs;

	guint       timer_id;
};

struct _EvDaemonClass {
	GObjectClass base_class;
};

static GType    ev_daemon_get_type            (void) G_GNUC_CONST;
static gboolean ev_daemon_register_document   (EvDaemon              *ev_daemon,
					       const gchar           *uri,
					       DBusGMethodInvocation *context);
static gboolean ev_daemon_unregister_document (EvDaemon              *ev_daemon,
					       const gchar           *uri,
					       DBusGMethodInvocation *context);
#include "ev-daemon-service.h"

static EvDaemon *ev_daemon = NULL;

G_DEFINE_TYPE(EvDaemon, ev_daemon, G_TYPE_OBJECT)

typedef struct {
	gchar *dbus_name;
	gchar *uri;
} EvDoc;

static void
ev_doc_free (EvDoc *doc)
{
	if (!doc)
		return;

	g_free (doc->dbus_name);
	g_free (doc->uri);

	g_free (doc);
}

static EvDoc *
ev_daemon_find_doc (EvDaemon    *ev_daemon,
		    const gchar *uri)
{
	GList *l;

	for (l = ev_daemon->docs; l; l = g_list_next (l)) {
		EvDoc *doc = (EvDoc *)l->data;

		if (strcmp (doc->uri, uri) == 0)
			return doc;
	}

	return NULL;
}

static void
ev_daemon_finalize (GObject *object)
{
	EvDaemon *ev_daemon = EV_DAEMON (object);

	if (ev_daemon->docs) {
		g_list_foreach (ev_daemon->docs, (GFunc)ev_doc_free, NULL);
		g_list_free (ev_daemon->docs);
		ev_daemon->docs = NULL;
	}

	if (ev_daemon->bus_proxy) {
		g_object_unref (ev_daemon->bus_proxy);
		ev_daemon->bus_proxy = NULL;
	}

	G_OBJECT_CLASS (ev_daemon_parent_class)->finalize (object);
}

static void
ev_daemon_init (EvDaemon *ev_daemon)
{
}

static void
ev_daemon_class_init (EvDaemonClass *klass)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

	g_object_class->finalize = ev_daemon_finalize;

	dbus_g_object_type_install_info (EV_TYPE_DAEMON,
					 &dbus_glib_ev_daemon_object_info);
}

static gboolean
ev_daemon_shutdown (EvDaemon *ev_daemon)
{
	g_object_unref (ev_daemon);

	return FALSE;
}

static void
ev_daemon_stop_killtimer (EvDaemon *ev_daemon)
{
	if (ev_daemon->timer_id != 0)
		g_source_remove (ev_daemon->timer_id);
	ev_daemon->timer_id = 0;
}

static void
ev_daemon_start_killtimer (EvDaemon *ev_daemon)
{
	ev_daemon_stop_killtimer (ev_daemon);
	ev_daemon->timer_id =
		g_timeout_add_seconds (30,
				       (GSourceFunc) ev_daemon_shutdown,
				       ev_daemon);
}

static void
ev_daemon_name_owner_changed (DBusGProxy  *proxy,
			      const gchar *name,
			      const gchar *old_owner,
			      const gchar *new_owner,
			      EvDaemon    *ev_daemon)
{
	GList *l, *next = NULL;

	if (*name == ':' && *new_owner == '\0') {
		for (l = ev_daemon->docs; l; l = next) {
			EvDoc *doc = (EvDoc *)l->data;

			next = l->next;
			if (strcmp (doc->dbus_name, name) == 0) {
				ev_doc_free (doc);
				ev_daemon->docs = g_list_delete_link (ev_daemon->docs, l);
				if (--ev_daemon->n_docs == 0)
					ev_daemon_start_killtimer (ev_daemon);
			}
		}
	}
}

static EvDaemon *
ev_daemon_get (void)
{
	DBusGConnection *connection;
	guint            request_name_result;
	GError          *error = NULL;

	if (ev_daemon)
		return ev_daemon;

	connection = dbus_g_bus_get (DBUS_BUS_STARTER, &error);
	if (!connection) {
		g_printerr ("Failed to connect to the D-BUS daemon: %s\n", error->message);
		g_error_free (error);

		return NULL;
	}

	ev_daemon = g_object_new (EV_TYPE_DAEMON, NULL);

	ev_daemon->bus_proxy = dbus_g_proxy_new_for_name (connection,
						       DBUS_SERVICE_DBUS,
						       DBUS_PATH_DBUS,
						       DBUS_INTERFACE_DBUS);
	if (!org_freedesktop_DBus_request_name (ev_daemon->bus_proxy,
						EV_DBUS_DAEMON_NAME,
						DBUS_NAME_FLAG_DO_NOT_QUEUE,
						&request_name_result, &error)) {
		g_printerr ("Failed to acquire daemon name: %s", error->message);
		g_error_free (error);
		g_object_unref (ev_daemon);

		return NULL;
	}

	switch (request_name_result) {
	case DBUS_REQUEST_NAME_REPLY_EXISTS:
		g_printerr ("Evince daemon already running, exiting.\n");
		g_object_unref (ev_daemon);

		return NULL;
	case DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER:
		dbus_g_connection_register_g_object (connection,
						     EV_DBUS_DAEMON_OBJECT_PATH,
						     G_OBJECT (ev_daemon));
		break;
	default:
		g_printerr ("Not primary owner of the service, exiting.\n");
		g_object_unref (ev_daemon);

		return NULL;
	}


	dbus_g_proxy_add_signal (ev_daemon->bus_proxy,
				 "NameOwnerChanged",
				 G_TYPE_STRING,
				 G_TYPE_STRING,
				 G_TYPE_STRING,
				 G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (ev_daemon->bus_proxy, "NameOwnerChanged",
				     G_CALLBACK (ev_daemon_name_owner_changed),
				     ev_daemon, NULL);
	ev_daemon_start_killtimer (ev_daemon);

	return ev_daemon;
}



static gboolean
ev_daemon_register_document (EvDaemon              *ev_daemon,
			     const gchar           *uri,
			     DBusGMethodInvocation *method)
{
	EvDoc       *doc;
	const gchar *owner = NULL;

	doc = ev_daemon_find_doc (ev_daemon, uri);
	if (doc) {
		/* Already registered */
		owner = doc->dbus_name;
	} else {
		doc = g_new (EvDoc, 1);
		doc->dbus_name = dbus_g_method_get_sender (method);
		doc->uri = g_strdup (uri);
		ev_daemon->docs = g_list_prepend (ev_daemon->docs, doc);
		if (ev_daemon->n_docs++ == 0)
			ev_daemon_stop_killtimer (ev_daemon);
	}

	dbus_g_method_return (method, owner);

	return TRUE;
}

static gboolean
ev_daemon_unregister_document (EvDaemon              *ev_daemon,
			       const gchar           *uri,
			       DBusGMethodInvocation *method)
{
	EvDoc *doc;
	gchar *sender;

	doc = ev_daemon_find_doc (ev_daemon, uri);
	if (!doc) {
		g_warning ("Document %s is not registered\n", uri);
		dbus_g_method_return (method);

		return TRUE;
	}

	sender = dbus_g_method_get_sender (method);
	if (strcmp (doc->dbus_name, sender) != 0) {
		g_warning ("Failed to unregister document %s: invalid owner %s, expected %s\n",
			   uri, sender, doc->dbus_name);
		g_free (sender);
		dbus_g_method_return (method);

		return TRUE;
	}
	g_free (sender);

	ev_daemon->docs = g_list_remove (ev_daemon->docs, doc);
	ev_doc_free (doc);
	if (--ev_daemon->n_docs == 0)
		ev_daemon_start_killtimer (ev_daemon);

	dbus_g_method_return (method);

	return TRUE;
}

static void
do_exit (GMainLoop *loop,
	 GObject   *object)
{
	if (g_main_loop_is_running (loop))
		g_main_loop_quit (loop);
}

static gboolean
convert_metadata (const gchar *metadata)
{
	GFile   *file;
	char    *argv[3];
	gint     exit_status;
	GFileAttributeInfoList *namespaces;
	gboolean supported = FALSE;
	GError  *error = NULL;
	gboolean retval;

	/* If metadata is not supported for a local file
	 * is likely because and old gvfs version is running.
	 */
	file = g_file_new_for_path (metadata);
	namespaces = g_file_query_writable_namespaces (file, NULL, NULL);
	if (namespaces) {
		gint i;

		for (i = 0; i < namespaces->n_infos; i++) {
			if (strcmp (namespaces->infos[i].name, "metadata") == 0) {
				supported = TRUE;
				break;
			}
		}
		g_file_attribute_info_list_unref (namespaces);
	}
	if (!supported) {
		g_warning ("GVFS metadata not supported. "
			   "Evince will run without metadata support.\n");
		g_object_unref (file);
		return FALSE;
	}
	g_object_unref (file);

	argv[0] = g_build_filename (LIBEXECDIR, "evince-convert-metadata", NULL);
	argv[1] = (char *) metadata;
	argv[2] = NULL;

	retval = g_spawn_sync (NULL /* wd */, argv, NULL /* env */,
			       0, NULL, NULL, NULL, NULL,
			       &exit_status, &error);
	g_free (argv[0]);

	if (!retval) {
		g_printerr ("Error migrating metadata: %s\n", error->message);
		g_error_free (error);
	}

	return retval && WIFEXITED (exit_status) && WEXITSTATUS (exit_status) == 0;
}

static void
ev_migrate_metadata (void)
{
	gchar *updated;
	gchar *metadata;
	gchar *dot_dir;

	dot_dir = g_build_filename (g_get_home_dir (),
				    ".gnome2",
				    "evince",
				    NULL);

	updated = g_build_filename (dot_dir, "migrated-to-gvfs", NULL);
	if (g_file_test (updated, G_FILE_TEST_EXISTS)) {
		/* Already migrated */
		g_free (updated);
		g_free (dot_dir);
		return;
	}

	metadata = g_build_filename (dot_dir, "ev-metadata.xml", NULL);
	if (g_file_test (metadata, G_FILE_TEST_EXISTS)) {
		if (convert_metadata (metadata)) {
			gint fd;

			fd = g_creat (updated, 0600);
			if (fd != -1) {
				close (fd);
			}
		}
	}

	g_free (dot_dir);
	g_free (updated);
	g_free (metadata);
}

gint
main (gint argc, gchar **argv)
{
	GMainLoop *loop;

	/* Init glib threads asap */
	if (!g_thread_supported ())
		g_thread_init (NULL);

	g_type_init ();

	if (!ev_daemon_get ())
		return 1;

	ev_migrate_metadata ();

	loop = g_main_loop_new (NULL, FALSE);
	g_object_weak_ref (G_OBJECT (ev_daemon),
			   (GWeakNotify) do_exit,
			   loop);
	g_main_loop_run (loop);
	g_main_loop_unref (loop);

	return 0;
}
