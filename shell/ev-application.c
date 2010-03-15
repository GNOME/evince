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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#include <config.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
#include <unistd.h>

#include "totem-scrsaver.h"

#ifdef WITH_SMCLIENT
#include "eggsmclient.h"
#endif

#include "ev-application.h"
#include "ev-file-helpers.h"
#include "ev-stock-icons.h"

#ifdef ENABLE_DBUS
#include "ev-media-player-keys.h"
#endif /* ENABLE_DBUS */

#ifdef ENABLE_DBUS
#include <dbus/dbus-glib-bindings.h>
static gboolean ev_application_open_uri (EvApplication  *application,
					 const char     *uri,
					 GHashTable     *args,
					 guint           timestamp,
					 GError        **error);
#include "ev-application-service.h"
#endif

struct _EvApplication {
	GObject base_instance;

	gchar *uri;

	gchar *dot_dir;
	gchar *data_dir;

#ifdef ENABLE_DBUS
	DBusGConnection *connection;
	EvMediaPlayerKeys *keys;
#endif

	TotemScrsaver *scr_saver;

#ifdef WITH_SMCLIENT
	EggSMClient *smclient;
#endif

	gchar *filechooser_open_uri;
	gchar *filechooser_save_uri;
};

struct _EvApplicationClass {
	GObjectClass base_class;
};

static EvApplication *instance;

G_DEFINE_TYPE (EvApplication, ev_application, G_TYPE_OBJECT);

#ifdef ENABLE_DBUS
#define APPLICATION_DBUS_OBJECT_PATH "/org/gnome/evince/Evince"
#define APPLICATION_DBUS_INTERFACE   "org.gnome.evince.Application"
#endif

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
	gchar    *uri;

#ifdef WITH_SMCLIENT
	if (egg_sm_client_is_resumed (application->smclient)) {
		state_file = egg_sm_client_get_state_file (application->smclient);
		if (!state_file)
			return FALSE;
	} else
#endif /* WITH_SMCLIENT */
		return FALSE;

	uri = g_key_file_get_string (state_file, "Evince", "uri", NULL);
	if (!uri)
		return FALSE;

	ev_application_open_uri_at_dest (application, uri,
					 gdk_screen_get_default (),
					 NULL, 0, NULL,
					 GDK_CURRENT_TIME);
	g_free (uri);
	g_key_file_free (state_file);

	return TRUE;
}

#ifdef WITH_SMCLIENT

static void
smclient_save_state_cb (EggSMClient   *client,
			GKeyFile      *state_file,
			EvApplication *application)
{
	if (!application->uri)
		return;

	g_key_file_set_string (state_file, "Evince", "uri", application->uri);
}

static void
smclient_quit_cb (EggSMClient   *client,
		  EvApplication *application)
{
	ev_application_shutdown (application);
}
#endif /* WITH_SMCLIENT */

static void
ev_application_init_session (EvApplication *application)
{
#ifdef WITH_SMCLIENT
	application->smclient = egg_sm_client_get ();
	g_signal_connect (application->smclient, "save_state",
			  G_CALLBACK (smclient_save_state_cb),
			  application);
	g_signal_connect (application->smclient, "quit",
			  G_CALLBACK (smclient_quit_cb),
			  application);
#endif
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

static void
value_free (GValue *value)
{
	g_value_unset (value);
	g_free (value);
}

static GHashTable *
build_args (GdkScreen      *screen,
	    EvLinkDest     *dest,
	    EvWindowRunMode mode,
	    const gchar    *search_string)
{
	GHashTable  *args;
	GValue      *value;
	GdkDisplay  *display;
	const gchar *display_name;
	gint         screen_number;

	args = g_hash_table_new_full (g_str_hash,
				      g_str_equal,
				      (GDestroyNotify)g_free,
				      (GDestroyNotify)value_free);

	/* Display */
	display = gdk_screen_get_display (screen);
	display_name = gdk_display_get_name (display);
	value = g_new0 (GValue, 1);
	g_value_init (value, G_TYPE_STRING);
	g_value_set_string (value, display_name);
	g_hash_table_insert (args, g_strdup ("display"), value);

	/* Screen */
	screen_number = gdk_screen_get_number (screen);
	value = g_new0 (GValue, 1);
	g_value_init (value, G_TYPE_INT);
	g_value_set_int (value, screen_number);
	g_hash_table_insert (args, g_strdup ("screen"), value);

	/* Page label */
	if (dest) {
		value = g_new0 (GValue, 1);
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, ev_link_dest_get_page_label (dest));

		g_hash_table_insert (args, g_strdup ("page-label"), value);
	}

	/* Find string */
	if (search_string) {
		value = g_new0 (GValue, 1);
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, search_string);

		g_hash_table_insert (args, g_strdup ("find-string"), value);
	}

	/* Mode */
	if (mode != EV_WINDOW_MODE_NORMAL) {
		value = g_new0 (GValue, 1);
		g_value_init (value, G_TYPE_UINT);
		g_value_set_uint (value, mode);

		g_hash_table_insert (args, g_strdup ("mode"), value);
	}

	return args;
}

static void
child_setup (gpointer user_data)
{
	gchar *startup_id;

	startup_id = g_strdup_printf ("_TIME%lu",
				      (unsigned long)GPOINTER_TO_INT (user_data));
	g_setenv ("DESKTOP_STARTUP_ID", startup_id, TRUE);
	g_free (startup_id);
}

static void
ev_spawn (const char     *uri,
	  GdkScreen      *screen,
	  EvLinkDest     *dest,
	  EvWindowRunMode mode,
	  const gchar    *search_string,
	  guint           timestamp)
{
	gchar   *argv[6];
	guint    arg = 0;
	gint     i;
	gboolean res;
	GError  *error = NULL;

#ifdef G_OS_WIN32
{
	gchar *dir;

	dir = g_win32_get_package_installation_directory_of_module (NULL);
	argv[arg++] = g_build_filename (dir, "bin", "evince", NULL);
	g_free (dir);
}
#else
	argv[arg++] = g_build_filename (BINDIR, "evince", NULL);
#endif

	/* Page label */
	if (dest) {
		const gchar *page_label;

		page_label = ev_link_dest_get_page_label (dest);
		if (page_label)
			argv[arg++] = g_strdup_printf ("--page-label=%s", page_label);
		else
			argv[arg++] = g_strdup_printf ("--page-label=%d",
						       ev_link_dest_get_page (dest));
	}

	/* Find string */
	if (search_string) {
		argv[arg++] = g_strdup_printf ("--find=%s", search_string);
	}

	/* Mode */
	switch (mode) {
	case EV_WINDOW_MODE_FULLSCREEN:
		argv[arg++] = g_strdup ("-f");
		break;
	case EV_WINDOW_MODE_PRESENTATION:
		argv[arg++] = g_strdup ("-s");
		break;
	default:
		break;
	}

	argv[arg++] = (gchar *)uri;
	argv[arg] = NULL;

	res = gdk_spawn_on_screen (screen, NULL /* wd */, argv, NULL /* env */,
				   0,
				   child_setup,
				   GINT_TO_POINTER(timestamp),
				   NULL, &error);
	if (!res) {
		g_warning ("Error launching evince %s: %s\n", uri, error->message);
		g_error_free (error);
	}

	for (i = 0; i < arg - 1; i++) {
		g_free (argv[i]);
	}
}

static GList *
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

static EvWindow *
ev_application_get_empty_window (EvApplication *application,
				 GdkScreen     *screen)
{
	EvWindow *empty_window = NULL;
	GList    *windows = ev_application_get_windows (application);
	GList    *l;

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


#ifdef ENABLE_DBUS
static gboolean
ev_application_register_uri (EvApplication *application,
			     const gchar   *uri,
			     GHashTable    *args,
			     guint          timestamp)
{
	DBusGProxy *proxy;
	gchar      *owner;
	gboolean    retval = TRUE;
	GError     *error = NULL;

	if (!application->connection)
		return TRUE;

	proxy = dbus_g_proxy_new_for_name (application->connection,
					   "org.gnome.evince.Daemon",
					   "/org/gnome/evince/Daemon",
					   "org.gnome.evince.Daemon");
	if (!dbus_g_proxy_call (proxy, "RegisterDocument", &error,
				G_TYPE_STRING, uri,
				G_TYPE_INVALID,
				G_TYPE_STRING, &owner,
				G_TYPE_INVALID)) {
		g_warning ("Error registering document: %s\n", error->message);
		g_error_free (error);
		g_object_unref (proxy);

		return TRUE;
	}
	g_object_unref (proxy);

	if (*owner == ':') {
		/* Already registered */
		proxy = dbus_g_proxy_new_for_name_owner (application->connection,
							 owner,
							 APPLICATION_DBUS_OBJECT_PATH,
							 APPLICATION_DBUS_INTERFACE,
							 &error);
		if (proxy) {
			if (!dbus_g_proxy_call (proxy, "OpenURI", &error,
						G_TYPE_STRING, uri,
						dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE), args,
						G_TYPE_UINT, timestamp,
						G_TYPE_INVALID,
						G_TYPE_INVALID)) {
				g_warning ("%s", error->message);
				g_error_free (error);
			}
			g_object_unref (proxy);
		} else {
			g_warning ("Error creating proxy: %s\n", error->message);
			g_error_free (error);
		}

		/* Do not continue opening this document */
		retval = FALSE;
	}

	g_free (owner);

	return retval;
}

static void
ev_application_unregister_uri (EvApplication *application,
			       const gchar   *uri)
{
	DBusGProxy *proxy;
	GError     *error = NULL;

	if (!application->connection)
		return;

	proxy = dbus_g_proxy_new_for_name (application->connection,
					   "org.gnome.evince.Daemon",
					   "/org/gnome/evince/Daemon",
					   "org.gnome.evince.Daemon");
	if (!dbus_g_proxy_call (proxy, "UnregisterDocument", &error,
				G_TYPE_STRING, uri,
				G_TYPE_INVALID,
				G_TYPE_INVALID)) {
		g_warning ("Error unregistering document: %s\n", error->message);
		g_error_free (error);
	}

	g_object_unref (proxy);
}
#endif /* ENABLE_DBUS */

static void
ev_application_open_uri_in_window (EvApplication  *application,
				   const char     *uri,
				   EvWindow       *ev_window,
				   GdkScreen      *screen,
				   EvLinkDest     *dest,
				   EvWindowRunMode mode,
				   const gchar    *search_string,
				   guint           timestamp)
{
#ifdef GDK_WINDOWING_X11
	GdkWindow *gdk_window;
#endif

	if (screen) {
		ev_stock_icons_set_screen (screen);
		gtk_window_set_screen (GTK_WINDOW (ev_window), screen);
	}

	/* We need to load uri before showing the window, so
	   we can restore window size without flickering */
	ev_window_open_uri (ev_window, uri, dest, mode, search_string);

	if (!gtk_widget_get_realized (GTK_WIDGET (ev_window)))
		gtk_widget_realize (GTK_WIDGET (ev_window));

#ifdef GDK_WINDOWING_X11
	gdk_window = gtk_widget_get_window (GTK_WIDGET (ev_window));

	if (timestamp <= 0)
		timestamp = gdk_x11_get_server_time (gdk_window);
	gdk_x11_window_set_user_time (gdk_window, timestamp);

	ev_document_fc_mutex_lock ();
	gtk_window_present (GTK_WINDOW (ev_window));
	ev_document_fc_mutex_unlock ();
#else
	ev_document_fc_mutex_lock ();
	gtk_window_present_with_time (GTK_WINDOW (ev_window), timestamp);
	ev_document_fc_mutex_unlock ();
#endif /* GDK_WINDOWING_X11 */
}

/**
 * ev_application_open_uri_at_dest:
 * @application: The instance of the application.
 * @uri: The uri to be opened.
 * @screen: Thee screen where the link will be shown.
 * @dest: The #EvLinkDest of the document.
 * @mode: The run mode of the window.
 * @timestamp: Current time value.
 */
void
ev_application_open_uri_at_dest (EvApplication  *application,
				 const char     *uri,
				 GdkScreen      *screen,
				 EvLinkDest     *dest,
				 EvWindowRunMode mode,
				 const gchar    *search_string,
				 guint           timestamp)
{
	EvWindow *ev_window;

	g_return_if_fail (uri != NULL);

	if (application->uri && strcmp (application->uri, uri) != 0) {
		/* spawn a new evince process */
		ev_spawn (uri, screen, dest, mode, search_string, timestamp);
		return;
	} else {
#ifdef ENABLE_DBUS
		GHashTable *args = build_args (screen, dest, mode, search_string);
		gboolean    ret;

		/* Register the uri or send OpenURI to
		 * remote instance if already registered
		 */
		ret = ev_application_register_uri (application, uri, args, timestamp);
		g_hash_table_destroy (args);
		if (!ret)
			return;
#endif /* ENABLE_DBUS */

		ev_window = ev_application_get_empty_window (application, screen);
		if (!ev_window)
			ev_window = EV_WINDOW (ev_window_new ());
	}

	application->uri = g_strdup (uri);

	ev_application_open_uri_in_window (application, uri, ev_window,
					   screen, dest, mode,
					   search_string,
					   timestamp);
}

/**
 * ev_application_open_window:
 * @application: The instance of the application.
 * @timestamp: Current time value.
 *
 * Creates a new window
 */
void
ev_application_open_window (EvApplication *application,
			    GdkScreen     *screen,
			    guint32        timestamp)
{
	GtkWidget *new_window = ev_window_new ();
#ifdef GDK_WINDOWING_X11
	GdkWindow *gdk_window;
#endif

	if (screen) {
		ev_stock_icons_set_screen (screen);
		gtk_window_set_screen (GTK_WINDOW (new_window), screen);
	}

	if (!gtk_widget_get_realized (new_window))
		gtk_widget_realize (new_window);

#ifdef GDK_WINDOWING_X11
	gdk_window = gtk_widget_get_window (GTK_WIDGET (new_window));

	if (timestamp <= 0)
		timestamp = gdk_x11_get_server_time (gdk_window);
	gdk_x11_window_set_user_time (gdk_window, timestamp);

	gtk_window_present (GTK_WINDOW (new_window));
#else
	gtk_window_present_with_time (GTK_WINDOW (new_window), timestamp);
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
static gboolean
ev_application_open_uri (EvApplication  *application,
			 const char     *uri,
			 GHashTable     *args,
			 guint           timestamp,
			 GError        **error)
{
	GList           *windows, *l;
	EvLinkDest      *dest = NULL;
	EvWindowRunMode  mode = EV_WINDOW_MODE_NORMAL;
	const gchar     *search_string = NULL;
	GdkScreen       *screen = NULL;

	g_assert (application->uri != NULL);

	/* FIXME: we don't need uri anymore,
	 * maybe this method should be renamed
	 * as reload, refresh or something like that
	 */
	if (!application->uri || strcmp (application->uri, uri)) {
		g_warning ("Invalid uri: %s, expected %s\n",
			   uri, application->uri);
		return TRUE;
	}

	if (args) {
		screen = get_screen_from_args (args);
		dest = get_destination_from_args (args);
		mode = get_window_run_mode_from_args (args);
		search_string = get_find_string_from_args (args);
	}

	windows = ev_application_get_windows (application);
	for (l = windows; l != NULL; l = g_list_next (l)) {
		EvWindow *ev_window = EV_WINDOW (l->data);

		ev_application_open_uri_in_window (application, uri, ev_window,
						   screen, dest, mode,
						   search_string,
						   timestamp);
	}
	g_list_free (windows);

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
						 timestamp);
	}
}

static void
ev_application_accel_map_save (EvApplication *application)
{
	gchar *accel_map_file;
	gchar *tmp_filename;
	gint   fd;

	accel_map_file = g_build_filename (g_get_home_dir (),
					   ".gnome2", "accels",
					   "evince", NULL);

	tmp_filename = g_strdup_printf ("%s.XXXXXX", accel_map_file);

	fd = g_mkstemp (tmp_filename);
	if (fd == -1) {
		g_free (accel_map_file);
		g_free (tmp_filename);

		return;
	}
	gtk_accel_map_save_fd (fd);
	close (fd);

	if (g_rename (tmp_filename, accel_map_file) == -1) {
		/* FIXME: win32? */
		g_unlink (tmp_filename);
	}

	g_free (accel_map_file);
	g_free (tmp_filename);
}

static void
ev_application_accel_map_load (EvApplication *application)
{
	gchar *accel_map_file;

	accel_map_file = g_build_filename (g_get_home_dir (),
					   ".gnome2", "accels",
					   "evince", NULL);
	gtk_accel_map_load (accel_map_file);
	g_free (accel_map_file);
}

void
ev_application_shutdown (EvApplication *application)
{
	if (application->uri) {
#ifdef ENABLE_DBUS
		ev_application_unregister_uri (application,
					       application->uri);
#endif
		g_free (application->uri);
		application->uri = NULL;
	}

	ev_application_accel_map_save (application);

	g_object_unref (application->scr_saver);
	application->scr_saver = NULL;

#ifdef ENABLE_DBUS
	if (application->keys) {
		g_object_unref (application->keys);
		application->keys = NULL;
	}
#endif /* ENABLE_DBUS */
	
        g_free (application->dot_dir);
        application->dot_dir = NULL;
        g_free (application->data_dir);
        application->data_dir = NULL;
	g_free (application->filechooser_open_uri);
        application->filechooser_open_uri = NULL;
	g_free (application->filechooser_save_uri);
	application->filechooser_save_uri = NULL;

	g_object_unref (application);
        instance = NULL;
	
	gtk_main_quit ();
}

static void
ev_application_class_init (EvApplicationClass *ev_application_class)
{
#ifdef ENABLE_DBUS
	dbus_g_object_type_install_info (EV_TYPE_APPLICATION,
					 &dbus_glib_ev_application_object_info);
#endif
}

static void
ev_application_init (EvApplication *ev_application)
{
	GError *error = NULL;

        ev_application->dot_dir = g_build_filename (g_get_home_dir (),
                                                    ".gnome2",
                                                    "evince",
                                                    NULL);

#ifdef G_OS_WIN32
{
	gchar *dir;

	dir = g_win32_get_package_installation_directory_of_module (NULL);
	ev_application->data_dir = g_build_filename (dir, "share", "evince", NULL);
	g_free (dir);
}
#else
	ev_application->data_dir = g_strdup (DATADIR);
#endif

	ev_application_init_session (ev_application);

	ev_application_accel_map_load (ev_application);

	ev_application->scr_saver = totem_scrsaver_new ();

#ifdef ENABLE_DBUS
	ev_application->connection = dbus_g_bus_get (DBUS_BUS_STARTER, &error);
	if (ev_application->connection) {
		dbus_g_connection_register_g_object (ev_application->connection,
						     APPLICATION_DBUS_OBJECT_PATH,
						     G_OBJECT (ev_application));
	} else {
		g_warning ("Error connection to DBus: %s\n", error->message);
		g_error_free (error);
	}
	ev_application->keys = ev_media_player_keys_new ();
#endif /* ENABLE_DBUS */
}

gboolean
ev_application_has_window (EvApplication *application)
{
	GList   *windows = ev_application_get_windows (application);
	gboolean retval = windows != NULL;

	g_list_free (windows);

	return retval;
}

const gchar *
ev_application_get_uri (EvApplication *application)
{
	return application->uri;
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

void
ev_application_set_filechooser_uri (EvApplication       *application,
				    GtkFileChooserAction action,
				    const gchar         *uri)
{
	if (action == GTK_FILE_CHOOSER_ACTION_OPEN) {
		g_free (application->filechooser_open_uri);
		application->filechooser_open_uri = g_strdup (uri);
	} else if (action == GTK_FILE_CHOOSER_ACTION_SAVE) {
		g_free (application->filechooser_save_uri);
		application->filechooser_save_uri = g_strdup (uri);
	}
}

const gchar *
ev_application_get_filechooser_uri (EvApplication       *application,
				    GtkFileChooserAction action)
{
	if (action == GTK_FILE_CHOOSER_ACTION_OPEN) {
		if (application->filechooser_open_uri)
			return application->filechooser_open_uri;
	} else if (action == GTK_FILE_CHOOSER_ACTION_SAVE) {
		if (application->filechooser_save_uri)
			return application->filechooser_save_uri;
	}

	return NULL;
}

void
ev_application_screensaver_enable (EvApplication *application)
{
	totem_scrsaver_enable (application->scr_saver);
}

void
ev_application_screensaver_disable (EvApplication *application)
{
	totem_scrsaver_disable (application->scr_saver);
}

const gchar *
ev_application_get_dot_dir (EvApplication *application,
                            gboolean create)
{
        if (create)
                g_mkdir_with_parents (application->dot_dir, 0700);

	return application->dot_dir;
}

const gchar *
ev_application_get_data_dir (EvApplication   *application)
{
	return application->data_dir;
}
