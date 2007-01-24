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
#include "ev-job-queue.h"
#include "ev-file-helpers.h"

static gchar   *ev_page_label;
static gboolean preview_mode = FALSE;
static gboolean fullscren_mode = FALSE;
static gboolean presentation_mode = FALSE;
static gboolean unlink_temp_file = FALSE;
static const char **file_arguments = NULL;

static const GOptionEntry goption_options[] =
{
	{ "page-label", 'p', 0, G_OPTION_ARG_STRING, &ev_page_label, N_("The page of the document to display."), N_("PAGE")},
	{ "fullscreen", 'f', 0, G_OPTION_ARG_NONE, &fullscren_mode, N_("Run evince in fullscreen mode"), NULL },
	{ "presentation", 's', 0, G_OPTION_ARG_NONE, &presentation_mode, N_("Run evince in presentation mode"), NULL },
	{ "preview", 'w', 0, G_OPTION_ARG_NONE, &preview_mode, N_("Run evince as a previewer"), NULL },
	{ "unlink-temp-file", 'u', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &unlink_temp_file, NULL, NULL },
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &file_arguments, NULL, N_("[FILE...]") },
	{ NULL }
};

static void
value_free (GValue *value)
{
	g_value_unset (value);
	g_free (value);
}

static GHashTable *
arguments_parse (void)
{
	GHashTable      *args;
	GValue          *value;
	EvWindowRunMode  mode;
	GdkScreen       *screen;
	GdkDisplay      *display;
	const gchar     *display_name;
	gint             screen_number;

	args = g_hash_table_new_full (g_str_hash,
				      g_str_equal,
				      (GDestroyNotify)g_free,
				      (GDestroyNotify)value_free);
	
	screen = gdk_screen_get_default ();
	display = gdk_screen_get_display (screen);

	display_name = gdk_display_get_name (display);
	screen_number = gdk_screen_get_number (screen);

	value = g_new0 (GValue, 1);
	g_value_init (value, G_TYPE_STRING);
	g_value_set_string (value, display_name);
	g_hash_table_insert (args, g_strdup ("display"), value);

	value = g_new0 (GValue, 1);
	g_value_init (value, G_TYPE_INT);
	g_value_set_int (value, screen_number);
	g_hash_table_insert (args, g_strdup ("screen"), value);

	if (ev_page_label) {
		value = g_new0 (GValue, 1);
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, ev_page_label);

		g_hash_table_insert (args, g_strdup ("page-label"), value);
	}

	if (fullscren_mode)
		mode = EV_WINDOW_MODE_FULLSCREEN;
	else if (presentation_mode)
		mode = EV_WINDOW_MODE_PRESENTATION;
	else if (preview_mode)
		mode = EV_WINDOW_MODE_PREVIEW;
	else
		return args;

	value = g_new0 (GValue, 1);
	g_value_init (value, G_TYPE_UINT);
	g_value_set_uint (value, mode);

	g_hash_table_insert (args, g_strdup ("mode"), value);

	if (mode == EV_WINDOW_MODE_PREVIEW && unlink_temp_file) {
		value = g_new0 (GValue, 1);
		g_value_init (value, G_TYPE_BOOLEAN);
		g_value_set_boolean (value, unlink_temp_file);

		g_hash_table_insert (args,
				     g_strdup ("unlink-temp-file"),
				     value);
	}

	return args;
}

static void
load_files (const char **files,
	    GHashTable  *args)
{
	int i;

	if (!files) {
		ev_application_open_window (EV_APP, args, GDK_CURRENT_TIME, NULL);
		return;
	}

	for (i = 0; files[i]; i++) {
		char   *uri;
		char   *label;
		GValue *old = NULL;

		uri = gnome_vfs_make_uri_from_shell_arg (files[i]);
		
		label = strchr (uri, GNOME_VFS_URI_MAGIC_CHR);

		if (label) {
			GValue *new;

			*label = 0; label++;
			
			old = g_hash_table_lookup (args, "page-label");
			
			new = g_new0 (GValue, 1);
			g_value_init (new, G_TYPE_STRING);
			g_value_set_string (new, label);

			g_hash_table_insert (args, g_strdup ("page-label"), new);

		}

		ev_application_open_uri (EV_APP, uri, args,
					 GDK_CURRENT_TIME, NULL);

		if (old)
			g_hash_table_insert (args, g_strdup ("page-label"), old);
		
		g_free (uri);
        }
}

#ifdef ENABLE_DBUS
static gboolean
load_files_remote (const char **files,
		   GHashTable  *args)
{
	int i;
	GError *error = NULL;
	DBusGConnection *connection;
	gboolean result = FALSE;
	DBusGProxy *remote_object;
	GdkDisplay *display;
	guint32 timestamp;

	display = gdk_display_get_default ();
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
		if (!dbus_g_proxy_call (remote_object, "OpenWindow", &error,
					dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE), args,
					G_TYPE_UINT, timestamp,
					G_TYPE_INVALID,
					G_TYPE_INVALID)) {
			g_warning (error->message);
			g_clear_error (&error);
			g_object_unref (remote_object);
			dbus_g_connection_unref (connection);
			return FALSE;
		}

		g_object_unref (remote_object);
		dbus_g_connection_unref (connection);
		
		return TRUE;
	}

	for (i = 0; files[i]; i++) {
		const char *page_label;
		char *uri;

		uri = gnome_vfs_make_uri_from_shell_arg (files[i]);
		page_label = ev_page_label ? ev_page_label : "";

		if (!dbus_g_proxy_call (remote_object, "OpenURI", &error,
					G_TYPE_STRING, uri,
					dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE), args,
					G_TYPE_UINT, timestamp,
					G_TYPE_INVALID,
					G_TYPE_INVALID)) {
			g_warning (error->message);
			g_clear_error (&error);
			g_free (uri);
			continue;
		}

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
	GHashTable *args;
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

	args = arguments_parse ();

#ifdef ENABLE_DBUS
	if (!ev_application_register_service (EV_APP)) {
		if (load_files_remote (file_arguments, args)) {
			g_hash_table_destroy (args);
			g_object_unref (program);
			
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
	ev_stock_icons_init ();
	gtk_window_set_default_icon_name ("evince");

	load_files (file_arguments, args);
	g_hash_table_destroy (args);

	gtk_main ();

	gnome_accelerators_sync ();
	ev_file_helpers_shutdown ();

	if (enable_metadata) {
		ev_metadata_manager_shutdown ();
	}
 	g_object_unref (program);

	return 0;
}
