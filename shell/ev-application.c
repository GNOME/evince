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

#include <config.h>

#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "totem-scrsaver.h"
#include "eggsmclient.h"

#include "ev-application.h"
#include "ev-document-factory.h"
#include "ev-file-helpers.h"
#include "ev-utils.h"

#ifdef ENABLE_DBUS
#include "ev-media-player-keys.h"
#endif /* ENABLE_DBUS */

#ifdef ENABLE_DBUS
#include <dbus/dbus-glib-bindings.h>
#include "ev-application-service.h"
#endif

static void ev_application_add_icon_path_for_screen (GdkScreen     *screen);
static void ev_application_save_print_settings      (EvApplication *application);

struct _EvApplication {
	GObject base_instance;

	gchar *accel_map_file;
	
	gchar *toolbars_file;

	EggToolbarsModel *toolbars_model;

	TotemScrsaver *scr_saver;

	EggSMClient *smclient;

	gchar *last_chooser_uri;

#ifdef ENABLE_DBUS
	EvMediaPlayerKeys *keys;
#endif /* ENABLE_DBUS */

	GtkPrintSettings *print_settings;
	GtkPageSetup     *page_setup;
	GKeyFile         *print_settings_file;
};

struct _EvApplicationClass {
	GObjectClass base_class;
};

G_DEFINE_TYPE (EvApplication, ev_application, G_TYPE_OBJECT);

#define APPLICATION_SERVICE_NAME "org.gnome.evince.ApplicationService"

#define EV_PRINT_SETTINGS_FILE "print-settings"
#define EV_PRINT_SETTINGS_GROUP "Print Settings"
#define EV_PAGE_SETUP_GROUP "Page Setup"

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

/* Session */
gboolean
ev_application_load_session (EvApplication *application)
{
	GKeyFile *state_file;
	gchar   **uri_list;
	
	if (!egg_sm_client_is_resumed (application->smclient))
		return FALSE;

	state_file = egg_sm_client_get_state_file (application->smclient);
	if (!state_file)
		return FALSE;

	uri_list = g_key_file_get_string_list (state_file,
					       "Evince",
					       "documents",
					       NULL, NULL);
	if (uri_list) {
		gint i;

		for (i = 0; uri_list[i]; i++) {
			if (g_ascii_strcasecmp (uri_list[i], "empty-window") == 0)
				ev_application_open_window (application, NULL, GDK_CURRENT_TIME, NULL);
			else
				ev_application_open_uri (application, uri_list[i], NULL, GDK_CURRENT_TIME, NULL);
		}
		g_strfreev (uri_list);
	}
	g_key_file_free (state_file);

	return TRUE;
}

static void
smclient_save_state_cb (EggSMClient   *client,
			GKeyFile      *state_file,
			EvApplication *application)
{
	GList *windows, *l;
	gint i;
	const gchar **uri_list;
	const gchar *empty = "empty-window";

	windows = ev_application_get_windows (application);
	if (!windows)
		return;

	uri_list = g_new (const gchar *, g_list_length (windows));
	for (l = windows, i = 0; l != NULL; l = g_list_next (l), i++) {
		EvWindow *window = EV_WINDOW (l->data);

		if (ev_window_is_empty (window))
			uri_list[i] = empty;
		else
			uri_list[i] = ev_window_get_uri (window);
	}
	g_key_file_set_string_list (state_file,
				    "Evince",
				    "documents", 
				    (const char **)uri_list,
				    i);
	g_free (uri_list);
}

static void
smclient_quit_cb (EggSMClient   *client,
		  EvApplication *application)
{
	ev_application_shutdown (application);
}

static void
ev_application_init_session (EvApplication *application)
{
	application->smclient = egg_sm_client_get ();
	g_signal_connect (application->smclient, "save_state",
			  G_CALLBACK (smclient_save_state_cb),
			  application);
	g_signal_connect (application->smclient, "quit",
			  G_CALLBACK (smclient_quit_cb),
			  application);
}

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

static const gchar *
get_find_string_from_args (GHashTable *args)
{
	GValue *value = NULL;

	g_assert (args != NULL);

	value = g_hash_table_lookup (args, "find-string");
	
	return value ? g_value_get_string (value) : NULL;
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

static const gchar *
get_print_settings_from_args (GHashTable *args)
{
	const gchar *print_settings = NULL;
	GValue      *value = NULL;

	g_assert (args != NULL);

	value = g_hash_table_lookup (args, "print-settings");
	if (value) {
		print_settings = g_value_get_string (value);
	}

	return print_settings;
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

	if (!GTK_WIDGET_REALIZED (new_window))
		gtk_widget_realize (new_window);
	
#ifdef GDK_WINDOWING_X11
	if (timestamp <= 0)
		timestamp = gdk_x11_get_server_time (GTK_WIDGET (new_window)->window);
	gdk_x11_window_set_user_time (GTK_WIDGET (new_window)->window, timestamp);
	
	gtk_window_present (GTK_WINDOW (new_window));
#else
	gtk_window_present_with_time (GTK_WINDOW (new_window), timestamp);
#endif /* GDK_WINDOWING_X11 */

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

	icon_theme = screen ? gtk_icon_theme_get_for_screen (screen) : gtk_icon_theme_get_default ();
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
				 const gchar    *search_string,
				 gboolean        unlink_temp_file,
				 const gchar    *print_settings, 
				 guint           timestamp)
{
	EvWindow *new_window;

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
	ev_window_open_uri (new_window, uri, dest, mode, search_string, 
			    unlink_temp_file, print_settings);

	if (!GTK_WIDGET_REALIZED (GTK_WIDGET (new_window)))
		gtk_widget_realize (GTK_WIDGET (new_window));

#ifdef GDK_WINDOWING_X11
	if (timestamp <= 0)
		timestamp = gdk_x11_get_server_time (GTK_WIDGET (new_window)->window);
	gdk_x11_window_set_user_time (GTK_WIDGET (new_window)->window, timestamp);

	ev_document_fc_mutex_lock ();
	gtk_window_present (GTK_WINDOW (new_window));
	ev_document_fc_mutex_unlock ();
#else
	ev_document_fc_mutex_lock ();
	gtk_window_present_with_time (GTK_WINDOW (new_window), timestamp);
	ev_document_fc_mutex_unlock ();
#endif /* GDK_WINDOWING_X11 */
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
	const gchar     *search_string = NULL;
	gboolean         unlink_temp_file = FALSE;
	const gchar     *print_settings = NULL;
	GdkScreen       *screen = NULL;

	if (args) {
		screen = get_screen_from_args (args);
		dest = get_destination_from_args (args);
		mode = get_window_run_mode_from_args (args);
		search_string = get_find_string_from_args (args);
		unlink_temp_file = (mode == EV_WINDOW_MODE_PREVIEW &&
				    get_unlink_temp_file_from_args (args));
		print_settings = get_print_settings_from_args (args);
	}
	
	ev_application_open_uri_at_dest (application, uri, screen,
					 dest, mode, search_string,
					 unlink_temp_file,
					 print_settings, timestamp);

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
						 screen, NULL, 0, NULL,
						 FALSE, NULL, timestamp);
	}
}

void
ev_application_shutdown (EvApplication *application)
{
	if (application->accel_map_file) {
		gtk_accel_map_save (application->accel_map_file);
		g_free (application->accel_map_file);
		application->accel_map_file = NULL;
	}
	
	if (application->toolbars_model) {
		g_object_unref (application->toolbars_model);
		g_free (application->toolbars_file);
		application->toolbars_model = NULL;
		application->toolbars_file = NULL;
	}

	ev_application_save_print_settings (application);
	
	if (application->print_settings_file) {
		g_key_file_free (application->print_settings_file);
		application->print_settings_file = NULL;
	}

	if (application->print_settings) {
		g_object_unref (application->print_settings);
		application->print_settings = NULL;
	}

	if (application->page_setup) {
		g_object_unref (application->page_setup);
		application->page_setup = NULL;
	}

#ifdef ENABLE_DBUS
	if (application->keys) {
		g_object_unref (application->keys);
		application->keys = NULL;
	}
#endif /* ENABLE_DBUS */
	
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
	gint i;
	const gchar *home_dir;
	
	ev_application_init_session (ev_application);

	home_dir = g_get_home_dir ();
	if (home_dir) {
		ev_application->accel_map_file = g_build_filename (home_dir,
								   ".gnome2",
								   "accels"
								   "evince",
								   NULL);
		gtk_accel_map_load (ev_application->accel_map_file);
	}
	
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

	/* Open item doesn't exist anymore,
	 * convert it to OpenRecent for compatibility
	 */
	for (i = 0; i < egg_toolbars_model_n_items (ev_application->toolbars_model, 0); i++) {
		const gchar *item;
		
		item = egg_toolbars_model_item_nth (ev_application->toolbars_model, 0, i);
		if (g_ascii_strcasecmp (item, "FileOpen") == 0) {
			egg_toolbars_model_remove_item (ev_application->toolbars_model, 0, i);
			egg_toolbars_model_add_item (ev_application->toolbars_model, 0, i,
						     "FileOpenRecent");
			ev_application_save_toolbars_model (ev_application);
			break;
		}
	}

	egg_toolbars_model_set_flags (ev_application->toolbars_model, 0,
				      EGG_TB_MODEL_NOT_REMOVABLE);

#ifdef ENABLE_DBUS
	ev_application->keys = ev_media_player_keys_new ();
#endif /* ENABLE_DBUS */
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

/**
 * ev_application_get_media_keys:
 * @application: The instance of the application.
 *
 * It gives you access to the media player keys handler object.
 *
 * Returns: A #EvMediaPlayerKeys.
 */
GObject *
ev_application_get_media_keys (EvApplication *application)
{
#ifdef ENABLE_DBUS
	return G_OBJECT (application->keys);
#else
	return NULL;
#endif /* ENABLE_DBUS */
}

EggToolbarsModel *
ev_application_get_toolbars_model (EvApplication *application)
{
	return application->toolbars_model;
}

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

static GKeyFile *
ev_application_get_print_settings_file (EvApplication *application)
{
	gchar *filename;
	
	if (application->print_settings_file)
		return application->print_settings_file;

	application->print_settings_file = g_key_file_new ();
	
	filename = g_build_filename (ev_dot_dir (), EV_PRINT_SETTINGS_FILE, NULL);
	if (g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
		GError *error = NULL;

		g_key_file_load_from_file (application->print_settings_file,
					   filename,
					   G_KEY_FILE_KEEP_COMMENTS |
					   G_KEY_FILE_KEEP_TRANSLATIONS,
					   &error);
		if (error) {
			g_warning ("%s", error->message);
			g_error_free (error);
		}
	}
	g_free (filename);

	return application->print_settings_file;
}

static void
ev_application_save_print_settings (EvApplication *application)
{
	GKeyFile *key_file;
	gchar    *filename;
	gchar    *data;
	gssize    data_length;
	GError   *error = NULL;

	if (!application->print_settings && !application->page_setup)
		return;
	
	key_file = ev_application_get_print_settings_file (application);
	if (application->print_settings)
		gtk_print_settings_to_key_file (application->print_settings,
						key_file,
						EV_PRINT_SETTINGS_GROUP);
	if (application->page_setup)
		gtk_page_setup_to_key_file (application->page_setup,
					    key_file,
					    EV_PAGE_SETUP_GROUP);
	
	filename = g_build_filename (ev_dot_dir (), EV_PRINT_SETTINGS_FILE, NULL);
	data = g_key_file_to_data (key_file, (gsize *)&data_length, NULL);
	g_file_set_contents (filename, data, data_length, &error);
	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
	g_free (data);
	g_free (filename);
}

GtkPrintSettings *
ev_application_get_print_settings (EvApplication *application)
{
	GKeyFile         *key_file;
	GtkPrintSettings *print_settings;
	
	if (application->print_settings)
		return application->print_settings;

	key_file = ev_application_get_print_settings_file (application);
	print_settings = g_key_file_has_group (key_file, EV_PRINT_SETTINGS_GROUP) ? 
		gtk_print_settings_new_from_key_file (key_file, EV_PRINT_SETTINGS_GROUP, NULL) :
		gtk_print_settings_new ();

	application->print_settings = print_settings ? print_settings : gtk_print_settings_new ();

	return application->print_settings;
}

void
ev_application_set_print_settings (EvApplication    *application,
				   GtkPrintSettings *settings)
{
	GKeyFile *key_file;
	
	g_return_if_fail (GTK_IS_PRINT_SETTINGS (settings));
	
	if (settings == application->print_settings)
		return;

	key_file = ev_application_get_print_settings_file (application);
	
	if (application->print_settings)
		g_object_unref (application->print_settings);
	
	application->print_settings = g_object_ref (settings);
	gtk_print_settings_to_key_file (settings, key_file, EV_PRINT_SETTINGS_GROUP);
}

GtkPageSetup *
ev_application_get_page_setup (EvApplication *application)
{
	GKeyFile     *key_file;
	GtkPageSetup *page_setup;
	
	if (application->page_setup)
		return application->page_setup;

	key_file = ev_application_get_print_settings_file (application);
	page_setup = g_key_file_has_group (key_file, EV_PAGE_SETUP_GROUP) ? 
		gtk_page_setup_new_from_key_file (key_file, EV_PAGE_SETUP_GROUP, NULL) :
		gtk_page_setup_new ();

	application->page_setup = page_setup ? page_setup : gtk_page_setup_new ();

	return application->page_setup;
}

void
ev_application_set_page_setup (EvApplication *application,
			       GtkPageSetup  *page_setup)
{
	GKeyFile *key_file;
	
	g_return_if_fail (GTK_IS_PAGE_SETUP (page_setup));
	
	if (page_setup == application->page_setup)
		return;

	key_file = ev_application_get_print_settings_file (application);
	
	if (application->page_setup)
		g_object_unref (application->page_setup);
	
	application->page_setup = g_object_ref (page_setup);
	gtk_page_setup_to_key_file (page_setup, key_file, EV_PAGE_SETUP_GROUP);
}
