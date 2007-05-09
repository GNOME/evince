/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2004 Martin Kretzschmar
 *
 *  Author:
 *    Martin Kretzschmar <martink@gnome.org>
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

#include "ev-application.h"
#include "ev-utils.h"
#include "ev-file-helpers.h"
#include "ev-document-factory.h"
#include "totem-scrsaver.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gtk/gtkfilechooserdialog.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkmain.h>

#if WITH_GNOME
#include <libgnomeui/gnome-client.h>
#endif

#ifdef ENABLE_DBUS
#include "ev-application-service.h"
#include <dbus/dbus-glib-bindings.h>
#endif

#include <string.h>

static void ev_application_add_icon_path_for_screen (GdkScreen *screen);

struct _EvApplication {
	GObject base_instance;

	gchar *toolbars_file;

	EggToolbarsModel *toolbars_model;

#ifndef HAVE_GTK_RECENT
	EggRecentModel  *recent_model;
#endif

	TotemScrsaver *scr_saver;

	gchar *last_chooser_uri;

#ifdef WITH_GTK_PRINT
	GtkPrintSettings *print_settings;
#if GTK_CHECK_VERSION (2, 11, 0)
	gchar            *print_settings_file;
#endif
#endif
};

struct _EvApplicationClass {
	GObjectClass base_class;
};

G_DEFINE_TYPE (EvApplication, ev_application, G_TYPE_OBJECT);

#define APPLICATION_SERVICE_NAME "org.gnome.evince.ApplicationService"

#ifdef ENABLE_DBUS
gboolean
ev_application_register_service (EvApplication *application)
{
	static DBusGConnection *connection = NULL;
	DBusGProxy *driver_proxy;
	GError *err = NULL;
	guint request_name_result;

	if (connection) {
		g_warning ("Service already registered.");
		return FALSE;
	}
	
	connection = dbus_g_bus_get (DBUS_BUS_STARTER, &err);
	if (connection == NULL) {
		g_warning ("Service registration failed.");
		g_error_free (err);

		return FALSE;
	}

	driver_proxy = dbus_g_proxy_new_for_name (connection,
						  DBUS_SERVICE_DBUS,
						  DBUS_PATH_DBUS,
						  DBUS_INTERFACE_DBUS);

	if (!org_freedesktop_DBus_request_name (driver_proxy,
                                        	APPLICATION_SERVICE_NAME,
						DBUS_NAME_FLAG_DO_NOT_QUEUE,
						&request_name_result, &err)) {
		g_warning ("Service registration failed.");
		g_clear_error (&err);
	}

	g_object_unref (driver_proxy);
	
	if (request_name_result == DBUS_REQUEST_NAME_REPLY_EXISTS) {
		return FALSE;
	}

	dbus_g_object_type_install_info (EV_TYPE_APPLICATION,
					 &dbus_glib_ev_application_object_info);
	dbus_g_connection_register_g_object (connection,
					     "/org/gnome/evince/Evince",
                                             G_OBJECT (application));
	
	application->scr_saver = totem_scrsaver_new (connection);
	
	return TRUE;
}
#endif /* ENABLE_DBUS */

/**
 * ev_application_get_instance:
 *
 * Checks for #EvApplication instance, if it doesn't exist it does create it.
 *
 * Returns: an instance of the #EvApplication data.
 */
EvApplication *
ev_application_get_instance (void)
{
	static EvApplication *instance;

	if (!instance) {
		instance = EV_APPLICATION (g_object_new (EV_TYPE_APPLICATION, NULL));
	}

	return instance;
}

#if WITH_GNOME
static void
removed_from_session (GnomeClient *client, EvApplication *application)
{
	ev_application_shutdown (application);
}

static gint
save_session (GnomeClient *client, gint phase, GnomeSaveStyle save_style, gint shutdown,
	      GnomeInteractStyle interact_style, gint fast, EvApplication *application)
{
	GList *windows, *l;
	char **restart_argv;
	int argc = 0, k;

	windows = ev_application_get_windows (application);
	restart_argv = g_new (char *, g_list_length (windows) + 1);
	restart_argv[argc++] = g_strdup ("evince");

	for (l = windows; l != NULL; l = l->next) {
		EvWindow *window = EV_WINDOW (l->data);
		restart_argv[argc++] = g_strdup (ev_window_get_uri (window));
	}

	gnome_client_set_restart_command (client, argc, restart_argv);

	for (k = 0; k < argc; k++) {
		g_free (restart_argv[k]);
	}

	g_list_free (windows);
	g_free (restart_argv);
	
	return TRUE;
}

static void
init_session (EvApplication *application)
{
	GnomeClient *client;

	client = gnome_master_client ();

	g_signal_connect (client, "save_yourself",
			  G_CALLBACK (save_session), application);	
	g_signal_connect (client, "die",
			  G_CALLBACK (removed_from_session), application);
}
#endif

/**
 * ev_display_open_if_needed:
 * @name: the name of the display to be open if it's needed.
 *
 * Search among all the open displays if any of them have the same name as the
 * passed name. If the display isn't found it tries the open it.
 *
 * Returns: a #GdkDisplay of the display with the passed name.
 */
static GdkDisplay *
ev_display_open_if_needed (const gchar *name)
{
	GSList     *displays;
	GSList     *l;
	GdkDisplay *display = NULL;

	displays = gdk_display_manager_list_displays (gdk_display_manager_get ());

	for (l = displays; l != NULL; l = l->next) {
		const gchar *display_name = gdk_display_get_name ((GdkDisplay *) l->data);

		if (g_ascii_strcasecmp (display_name, name) == 0) {
			display = l->data;
			break;
		}
	}

	g_slist_free (displays);

	return display != NULL ? display : gdk_display_open (name);
}

/**
 * get_screen_from_args:
 * @args: a #GHashTable with data passed to the application.
 *
 * Looks for the screen in the display available in the hash table passed to the
 * application. If the display isn't opened, it's opened and the #GdkScreen
 * assigned to the screen in that display returned.
 *
 * Returns: the #GdkScreen assigned to the screen on the display indicated by
 *          the data on the #GHashTable.
 */
static GdkScreen *
get_screen_from_args (GHashTable *args)
{
	GValue     *value = NULL;
	GdkDisplay *display = NULL;
	GdkScreen  *screen = NULL;

	g_assert (args != NULL);
	
	value = g_hash_table_lookup (args, "display");
	if (value) {
		const gchar *display_name;
		
		display_name = g_value_get_string (value);
		display = ev_display_open_if_needed (display_name);
	}
	
	value = g_hash_table_lookup (args, "screen");
	if (value) {
		gint screen_number;
		
		screen_number = g_value_get_int (value);
		screen = gdk_display_get_screen (display, screen_number);
	}

	return screen;
}

/**
 * get_window_run_mode_from_args:
 * @args: a #GHashTable with data passed to the application.
 *
 * It does look if the mode option has been passed from command line, using it
 * as the window run mode, otherwise the run mode will be the normal mode.
 *
 * Returns: The window run mode passed from command line or
 *          EV_WINDOW_MODE_NORMAL in other case.
 */
static EvWindowRunMode
get_window_run_mode_from_args (GHashTable *args)
{
	EvWindowRunMode  mode = EV_WINDOW_MODE_NORMAL;
	GValue          *value = NULL;

	g_assert (args != NULL);

	value = g_hash_table_lookup (args, "mode");
	if (value) {
		mode = g_value_get_uint (value);
	}

	return mode;
}

/**
 * get_destination_from_args:
 * @args: a #GHashTable with data passed to the application.
 *
 * It does look for the page-label argument parsed from the command line and
 * if it does exist, it returns an #EvLinkDest.
 *
 * Returns: An #EvLinkDest to page-label if it has been passed from the command
 *          line, NULL in other case.
 */
static EvLinkDest *
get_destination_from_args (GHashTable *args)
{
	EvLinkDest *dest = NULL;
	GValue     *value = NULL;
	
	g_assert (args != NULL);
	
	value = g_hash_table_lookup (args, "page-label");
	if (value) {
		const gchar *page_label;

		page_label = g_value_get_string (value);
		dest = ev_link_dest_new_page_label (page_label);
	}

	return dest;
}

/**
 * get_unlink_temp_file_from_args:
 * @args: a #GHashTable with data passed to the application.
 *
 * It does look if the unlink-temp-file option has been passed from the command
 * line returning it's boolean representation, otherwise it does return %FALSE.
 *
 * Returns: the boolean representation of the unlink-temp-file value or %FALSE
 *          in other case.
 */
static gboolean
get_unlink_temp_file_from_args (GHashTable *args)
{
	gboolean unlink_temp_file = FALSE;
	GValue  *value = NULL;

	g_assert (args != NULL);

	value = g_hash_table_lookup (args, "unlink-temp-file");
	if (value) {
		unlink_temp_file = g_value_get_boolean (value);
	}
	
	return unlink_temp_file;
}

/**
 * ev_application_open_window:
 * @application: The instance of the application.
 * @args: A #GHashTable with the arguments data.
 * @timestamp: Current time value.
 * @error: The #GError facility.
 * 
 * Creates a new window and if the args are available, it's not NULL, it gets
 * the screen from them and assigns the just created window to it. At last it
 * does show it.
 *
 * Returns: %TRUE.
 */
gboolean
ev_application_open_window (EvApplication  *application,
			    GHashTable     *args,
			    guint32         timestamp,
			    GError        **error)
{
	GtkWidget *new_window = ev_window_new ();
	GdkScreen *screen = NULL;

	if (args) {
		screen = get_screen_from_args (args);
	}
	
	if (screen) {
		gtk_window_set_screen (GTK_WINDOW (new_window), screen);
	}
	ev_application_add_icon_path_for_screen (screen);
	
	gtk_widget_show (new_window);
	
	gtk_window_present_with_time (GTK_WINDOW (new_window),
				      timestamp);
	return TRUE;
}

/**
 * ev_application_get_empty_window:
 * @application: The instance of the application.
 * @screen: The screen where the empty window will be search.
 *
 * It does look if there is any empty window in the indicated screen.
 *
 * Returns: The first empty #EvWindow in the passed #GdkScreen or NULL in other
 *          case.
 */
static EvWindow *
ev_application_get_empty_window (EvApplication *application,
				 GdkScreen     *screen)
{
	EvWindow *empty_window = NULL;
	GList *windows = ev_application_get_windows (application);
	GList *l;

	for (l = windows; l != NULL; l = l->next) {
		EvWindow *window = EV_WINDOW (l->data);

		if (ev_window_is_empty (window) &&
		    gtk_window_get_screen (GTK_WINDOW (window)) == screen) {
			empty_window = window;
			break;
		}
	}

	g_list_free (windows);
	
	return empty_window;
}

/**
 * ev_application_get_uri_window:
 * @application: The instance of the application.
 * @uri: The uri to be opened.
 *
 * It looks in the list of the windows for the one with the document represented
 * by the passed uri on it. If the window is empty or the document isn't present
 * on any window, it will return NULL.
 *
 * Returns: The #EvWindow where the document represented by the passed uri is
 *          shown, NULL in other case.
 */
static EvWindow *
ev_application_get_uri_window (EvApplication *application, const char *uri)
{
	EvWindow *uri_window = NULL;
	GList *windows = gtk_window_list_toplevels ();
	GList *l;

	g_return_val_if_fail (uri != NULL, NULL);

	for (l = windows; l != NULL; l = l->next) {
		if (EV_IS_WINDOW (l->data)) {
			EvWindow *window = EV_WINDOW (l->data);
			const char *window_uri = ev_window_get_uri (window);

			if (window_uri && strcmp (window_uri, uri) == 0 && !ev_window_is_empty (window)) {
				uri_window = window;
				break;
			}
		}
	}

	g_list_free (windows);
	
	return uri_window;
}

static void
ev_application_add_icon_path_for_screen (GdkScreen *screen)
{
	GtkIconTheme *icon_theme;

	icon_theme = gtk_icon_theme_get_for_screen (screen);
	if (icon_theme) {
		gchar **path = NULL;
		gint    n_paths;
		gint    i;
		gchar  *ev_icons_path;

		/* GtkIconTheme will then look in Evince custom hicolor dir
		 * for icons as well as the standard search paths
		 */
		ev_icons_path = g_build_filename (DATADIR, "icons", NULL);
		gtk_icon_theme_get_search_path (icon_theme, &path, &n_paths);
		for (i = n_paths - 1; i >= 0; i--) {
			if (g_ascii_strcasecmp (ev_icons_path, path[i]) == 0)
				break;
		}

		if (i < 0)
			gtk_icon_theme_append_search_path (icon_theme,
							   ev_icons_path);

		g_free (ev_icons_path);
		g_strfreev (path);
	}	
}

/**
 * ev_application_open_uri_at_dest:
 * @application: The instance of the application.
 * @uri: The uri to be opened.
 * @screen: Thee screen where the link will be shown.
 * @dest: The #EvLinkDest of the document.
 * @mode: The run mode of the window.
 * @unlink_temp_file: The unlink_temp_file option value.
 * @timestamp: Current time value.
 */
void
ev_application_open_uri_at_dest (EvApplication  *application,
				 const char     *uri,
				 GdkScreen      *screen,
				 EvLinkDest     *dest,
				 EvWindowRunMode mode,
				 gboolean        unlink_temp_file,
				 guint           timestamp)
{
	EvWindow     *new_window;

	g_return_if_fail (uri != NULL);
	
	ev_application_add_icon_path_for_screen (screen);

	new_window = ev_application_get_uri_window (application, uri);
	
	if (new_window == NULL) {
		new_window = ev_application_get_empty_window (application, screen);
	}

	if (new_window == NULL) {
		new_window = EV_WINDOW (ev_window_new ());
	}

	if (screen)
		gtk_window_set_screen (GTK_WINDOW (new_window), screen);

	/* We need to load uri before showing the window, so
	   we can restore window size without flickering */	
	ev_window_open_uri (new_window, uri, dest, mode, unlink_temp_file);

	ev_document_fc_mutex_lock ();
	gtk_widget_show (GTK_WIDGET (new_window));
	ev_document_fc_mutex_unlock ();

	gtk_window_present_with_time (GTK_WINDOW (new_window),
				      timestamp);
}

/**
 * ev_application_open_uri:
 * @application: The instance of the application.
 * @uri: The uri to be opened
 * @args: A #GHashTable with the arguments data.
 * @timestamp: Current time value.
 * @error: The #GError facility.
 */
gboolean
ev_application_open_uri (EvApplication  *application,
			 const char     *uri,
			 GHashTable     *args,
			 guint           timestamp,
			 GError        **error)
{
	EvLinkDest      *dest = NULL;
	EvWindowRunMode  mode = EV_WINDOW_MODE_NORMAL;
	gboolean         unlink_temp_file = FALSE;
	GdkScreen       *screen = NULL;

	if (args) {
		screen = get_screen_from_args (args);
		dest = get_destination_from_args (args);
		mode = get_window_run_mode_from_args (args);
		unlink_temp_file = (mode == EV_WINDOW_MODE_PREVIEW &&
				    get_unlink_temp_file_from_args (args));
	}
	
	ev_application_open_uri_at_dest (application, uri, screen,
					 dest, mode, unlink_temp_file, 
					 timestamp);

	if (dest)
		g_object_unref (dest);

	return TRUE;
}

void
ev_application_open_uri_list (EvApplication *application,
			      GSList        *uri_list,
			      GdkScreen     *screen,
			      guint          timestamp)
{
	GSList *l;

	for (l = uri_list; l != NULL; l = l->next) {
		ev_application_open_uri_at_dest (application, (char *)l->data,
						 screen, NULL, 0, FALSE, 
						 timestamp);
	}
}

void
ev_application_shutdown (EvApplication *application)
{
	if (application->toolbars_model) {
		g_object_unref (application->toolbars_model);
		g_free (application->toolbars_file);
		application->toolbars_model = NULL;
		application->toolbars_file = NULL;
	}

#ifndef HAVE_GTK_RECENT
	if (application->recent_model) {
		g_object_unref (application->recent_model);
		application->recent_model = NULL;
	}
#endif

#ifdef WITH_GTK_PRINT
#if GTK_CHECK_VERSION (2, 11, 0)
	if (application->print_settings_file) {
		if (application->print_settings) {
			GError *error = NULL;
			
			gtk_print_settings_to_file (application->print_settings,
						    application->print_settings_file,
						    &error);
			if (error) {
				g_warning (error->message);
				g_error_free (error);
			}

			g_object_unref (application->print_settings);
			application->print_settings = NULL;
		}

		g_free (application->print_settings_file);
		application->print_settings_file = NULL;
	}
#else /* ! GTK 2.11.0 */
	if (application->print_settings) {
		g_object_unref (application->print_settings);
		application->print_settings = NULL;
	}
#endif /* GTK 2.11.0 */
#endif /* WITH_GTK_PRINT */
	
	g_free (application->last_chooser_uri);
	g_object_unref (application);
	
	gtk_main_quit ();
}

static void
ev_application_class_init (EvApplicationClass *ev_application_class)
{
}

static void
ev_application_init (EvApplication *ev_application)
{
#if WITH_GNOME
	init_session (ev_application);
#endif

	ev_application->toolbars_model = egg_toolbars_model_new ();

	ev_application->toolbars_file = g_build_filename
			(ev_dot_dir (), "evince_toolbar.xml", NULL);

	egg_toolbars_model_load_names (ev_application->toolbars_model,
				       DATADIR "/evince-toolbar.xml");

	if (!egg_toolbars_model_load_toolbars (ev_application->toolbars_model,
					       ev_application->toolbars_file)) {
		egg_toolbars_model_load_toolbars (ev_application->toolbars_model,
						  DATADIR"/evince-toolbar.xml");
	}

	egg_toolbars_model_set_flags (ev_application->toolbars_model, 0,
				      EGG_TB_MODEL_NOT_REMOVABLE); 

#ifndef HAVE_GTK_RECENT
	ev_application->recent_model = egg_recent_model_new (EGG_RECENT_MODEL_SORT_MRU);
	/* FIXME we should add a mime type filter but current eggrecent
           has only a varargs style api which does not work well when
	   the list of mime types is dynamic */
	egg_recent_model_set_limit (ev_application->recent_model, 5);	
	egg_recent_model_set_filter_groups (ev_application->recent_model,
    	    	    			    "Evince", NULL);
#endif /* HAVE_GTK_RECENT */
}

/**
 * ev_application_get_windows:
 * @application: The instance of the application.
 *
 * It creates a list of the top level windows.
 *
 * Returns: A #GList of the top level windows.
 */
GList *
ev_application_get_windows (EvApplication *application)
{
	GList *l, *toplevels;
	GList *windows = NULL;

	toplevels = gtk_window_list_toplevels ();

	for (l = toplevels; l != NULL; l = l->next) {
		if (EV_IS_WINDOW (l->data)) {
			windows = g_list_append (windows, l->data);
		}
	}

	g_list_free (toplevels);

	return windows;
}

EggToolbarsModel *
ev_application_get_toolbars_model (EvApplication *application)
{
	return application->toolbars_model;
}

#ifndef HAVE_GTK_RECENT
EggRecentModel *
ev_application_get_recent_model (EvApplication *application)
{
	return application->recent_model;
}
#endif

void
ev_application_save_toolbars_model (EvApplication *application)
{
        egg_toolbars_model_save_toolbars (application->toolbars_model,
			 	          application->toolbars_file, "1.0");
}

void
ev_application_set_chooser_uri (EvApplication *application, const gchar *uri)
{
	g_free (application->last_chooser_uri);
	application->last_chooser_uri = g_strdup (uri);
}

const gchar *
ev_application_get_chooser_uri (EvApplication *application)
{
	return application->last_chooser_uri;
}

void
ev_application_screensaver_enable (EvApplication *application)
{
	if (application->scr_saver)
		totem_scrsaver_enable (application->scr_saver);	
}

void
ev_application_screensaver_disable (EvApplication *application)
{
	if (application->scr_saver)
		totem_scrsaver_disable (application->scr_saver);	
}

#ifdef WITH_GTK_PRINT
GtkPrintSettings *
ev_application_get_print_settings (EvApplication *application)
{
	if (application->print_settings)
		return application->print_settings;
	
#if GTK_CHECK_VERSION (2, 11, 0)
	if (!application->print_settings_file) {
		application->print_settings_file =
			g_build_filename (ev_dot_dir (), "print-settings", NULL);
	}

	if (g_file_test (application->print_settings_file, G_FILE_TEST_IS_REGULAR)) {
		GError *error = NULL;
		
		application->print_settings =
			gtk_print_settings_new_from_file (application->print_settings_file, &error);
		
		if (error) {
			g_warning (error->message);
			g_error_free (error);
		} else {
			return application->print_settings;
		}
	}
#endif /* GTK 2.11.0 */
	
	application->print_settings = gtk_print_settings_new ();

	return application->print_settings;
}

void
ev_application_set_print_settings (EvApplication    *application,
				   GtkPrintSettings *settings)
{
	g_return_if_fail (GTK_IS_PRINT_SETTINGS (settings));
	
	if (settings == application->print_settings)
		return;

	if (application->print_settings)
		g_object_unref (application->print_settings);
	
	application->print_settings = g_object_ref (settings);
}
#endif /* WITH_GTK_PRINT */
