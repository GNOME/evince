/*
 *  Copyright (C) 2004 Marco Pesenti Gritti
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#include "config.h"

#include "ev-application.h"
#include "ev-metadata-manager.h"

#include <glib/gi18n.h>
#include <gdk/gdkx.h>
#include <gtk/gtkmain.h>
#include <stdlib.h>
#include <string.h>
#include <libgnome/gnome-program.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomeui/gnome-app-helper.h>
#include <libgnomeui/gnome-authentication-manager.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#ifdef ENABLE_DBUS
#include <dbus/dbus-glib-bindings.h>
#endif

#include "ev-stock-icons.h"
#include "ev-debug.h"
#include "ev-job-queue.h"
#include "ev-file-helpers.h"

static char *ev_page_label;
static const char **file_arguments = NULL;

static const GOptionEntry goption_options[] =
{
	{ "page-label", 'p', 0, G_OPTION_ARG_STRING, &ev_page_label, N_("The page of the document to display."), N_("PAGE")},
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &file_arguments, NULL, N_("[FILE...]") },
	{ NULL }
};

static void
load_files (const char **files)
{
	int i;

	if (!files) {
		ev_application_open_window (EV_APP, GDK_CURRENT_TIME, NULL);
		return;
	}

	for (i = 0; files[i]; i++) {
		char *uri;
		char *label;

		uri = gnome_vfs_make_uri_from_shell_arg (files[i]);
		
		label = strchr (uri, GNOME_VFS_URI_MAGIC_CHR);
		
		if (label) {
			*label = 0; label++;
			ev_application_open_uri (EV_APP, uri, label,
						 GDK_CURRENT_TIME, NULL);
		} else {	
			ev_application_open_uri (EV_APP, uri, ev_page_label,
						 GDK_CURRENT_TIME, NULL);
		}
		g_free (uri);
        }
}

#ifdef ENABLE_DBUS

static gboolean
load_files_remote (const char **files)
{
	int i;
	GError *error = NULL;
	DBusGConnection *connection;
	gboolean result = FALSE;
#if DBUS_VERSION < 35
	DBusGPendingCall *call;
#endif
	DBusGProxy *remote_object;
	GdkDisplay *display;
	guint32 timestamp;

	display = gdk_display_get_default();
	timestamp = gdk_x11_display_get_user_time (display);
	connection = dbus_g_bus_get (DBUS_BUS_STARTER, &error);

	if (connection == NULL) {
		g_warning (error->message);
		g_error_free (error);	

		return FALSE;
	}

	remote_object = dbus_g_proxy_new_for_name (connection,
						   "org.gnome.evince.ApplicationService",
                                                   "/org/gnome/evince/Evince",
                                                   "org.gnome.evince.Application");
	if (!files) {
#if DBUS_VERSION <= 33
		call = dbus_g_proxy_begin_call (remote_object, "OpenWindow",
						DBUS_TYPE_UINT32, &timestamp,
						DBUS_TYPE_INVALID);

		if (!dbus_g_proxy_end_call (remote_object, call, &error, DBUS_TYPE_INVALID)) {
			g_warning (error->message);
			g_clear_error (&error);
			g_object_unref (remote_object);
			dbus_g_connection_unref (connection);
			return FALSE;
		}
#elif DBUS_VERSION == 34
		call = dbus_g_proxy_begin_call (remote_object, "OpenWindow",
						G_TYPE_UINT, timestamp,
						G_TYPE_INVALID);

		if (!dbus_g_proxy_end_call (remote_object, call, &error, G_TYPE_INVALID)) {
			g_warning (error->message);
			g_clear_error (&error);
			g_object_unref (remote_object);
			dbus_g_connection_unref (connection);
			return FALSE;
		}
#else
		if (!dbus_g_proxy_call (remote_object, "OpenWindow", &error,
					G_TYPE_UINT, timestamp,
					G_TYPE_INVALID,
					G_TYPE_INVALID)) {
			g_warning (error->message);
			g_clear_error (&error);
			g_object_unref (remote_object);
			dbus_g_connection_unref (connection);
			return FALSE;
		}
#endif
		g_object_unref (remote_object);
		dbus_g_connection_unref (connection);
		
		return TRUE;
	}

	for (i = 0; files[i]; i++) {
		const char *page_label;
		char *uri;

		uri = gnome_vfs_make_uri_from_shell_arg (files[i]);
		page_label = ev_page_label ? ev_page_label : "";
#if DBUS_VERSION <= 33
		call = dbus_g_proxy_begin_call (remote_object, "OpenURI",
						DBUS_TYPE_STRING, &uri,
						DBUS_TYPE_STRING, &page_label,
						DBUS_TYPE_UINT32, &timestamp,
						DBUS_TYPE_INVALID);

		if (!dbus_g_proxy_end_call (remote_object, call, &error, DBUS_TYPE_INVALID)) {
			g_warning (error->message);
			g_clear_error (&error);
			g_free (uri);
			continue;
		}
#elif DBUS_VERSION == 34
		call = dbus_g_proxy_begin_call (remote_object, "OpenURI",
						G_TYPE_STRING, uri,
						G_TYPE_STRING, page_label,
						G_TYPE_UINT, timestamp,
						G_TYPE_INVALID);

		if (!dbus_g_proxy_end_call (remote_object, call, &error, G_TYPE_INVALID)) {
			g_warning (error->message);
			g_clear_error (&error);
			g_free (uri);
			continue;
		}
#else
		if (!dbus_g_proxy_call (remote_object, "OpenURI", &error,
					G_TYPE_STRING, uri,
					G_TYPE_STRING, page_label,
					G_TYPE_UINT, timestamp,
					G_TYPE_INVALID,
					G_TYPE_INVALID)) {
			g_warning (error->message);
			g_clear_error (&error);
			g_free (uri);
			continue;
		}
#endif
		g_free (uri);
		result = TRUE;
        }

	g_object_unref (remote_object);
	dbus_g_connection_unref (connection);

	gdk_notify_startup_complete ();

	return result;
}
#endif /* ENABLE_DBUS */

int
main (int argc, char *argv[])
{
	gboolean enable_metadata = FALSE;
	GOptionContext *context;
	GnomeProgram *program;

	context = g_option_context_new (_("GNOME Document Viewer"));

#ifdef ENABLE_NLS
	/* Initialize the i18n stuff */
	bindtextdomain(GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
	g_option_context_add_main_entries (context, goption_options, GETTEXT_PACKAGE);
#else
	g_option_context_add_main_entries (context, goption_options, NULL);
#endif

	program = gnome_program_init (PACKAGE, VERSION,
                                      LIBGNOMEUI_MODULE, argc, argv,
                                      GNOME_PARAM_GOPTION_CONTEXT, context,
                                      GNOME_PARAM_HUMAN_READABLE_NAME, _("Evince"),
				      GNOME_PARAM_APP_DATADIR, GNOMEDATADIR,
                                      NULL);

#ifdef ENABLE_DBUS
	if (!ev_application_register_service (EV_APP)) {
		if (load_files_remote (file_arguments)) {
			return 0;
		}
	} else {
		enable_metadata = TRUE;
	}
#endif

	gnome_authentication_manager_init ();


	if (enable_metadata) {
		ev_metadata_manager_init ();
	}

	ev_job_queue_init ();
	g_set_application_name (_("Evince Document Viewer"));

	ev_file_helpers_init ();
	ev_debug_init ();
	ev_stock_icons_init ();
	gtk_window_set_default_icon_name ("evince");

	load_files (file_arguments);

	gtk_main ();

	gnome_accelerators_sync ();
	ev_file_helpers_shutdown ();

	if (enable_metadata) {
		ev_metadata_manager_shutdown ();
	}
 	g_object_unref(program);

	return 0;
}
