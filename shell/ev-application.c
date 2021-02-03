/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2004 Martin Kretzschmar
 *  Copyright © 2010, 2012 Christian Persch
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
#include <libhandy-1/handy.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
#include <unistd.h>

#include "ev-application.h"
#include "ev-file-helpers.h"
#include "ev-stock-icons.h"

#ifdef ENABLE_DBUS
#include "ev-gdbus-generated.h"
#include "ev-media-player-keys.h"
#endif /* ENABLE_DBUS */

struct _EvApplication {
	GtkApplication base_instance;

	gchar *uri;

	gchar *dot_dir;

#ifdef ENABLE_DBUS
        EvEvinceApplication *skeleton;
	EvMediaPlayerKeys *keys;
	gboolean doc_registered;
#endif
};

struct _EvApplicationClass {
	GtkApplicationClass base_class;
};

G_DEFINE_TYPE (EvApplication, ev_application, GTK_TYPE_APPLICATION)

#define APPLICATION_NAME             "org.gnome.Evince"
#ifdef ENABLE_DBUS
#define APPLICATION_DBUS_OBJECT_PATH "/org/gnome/evince/Evince"
#define APPLICATION_DBUS_INTERFACE   "org.gnome.evince.Application"

#define EVINCE_DAEMON_SERVICE        "org.gnome.evince.Daemon"
#define EVINCE_DAEMON_OBJECT_PATH    "/org/gnome/evince/Daemon"
#define EVINCE_DAEMON_INTERFACE      "org.gnome.evince.Daemon"
#endif

static void _ev_application_open_uri_at_dest (EvApplication  *application,
					      const gchar    *uri,
					      GdkScreen      *screen,
					      EvLinkDest     *dest,
					      EvWindowRunMode mode,
					      const gchar    *search_string,
					      guint           timestamp);
static void ev_application_open_uri_in_window (EvApplication  *application,
					       const char     *uri,
					       EvWindow       *ev_window,
					       GdkScreen      *screen,
					       EvLinkDest     *dest,
					       EvWindowRunMode mode,
					       const gchar    *search_string,
					       guint           timestamp);

/**
 * ev_application_new:
 *
 * Creates a new #EvApplication instance.
 *
 * Returns: (transfer full): a newly created #EvApplication
 */
EvApplication *
ev_application_new (void)
{
  const GApplicationFlags flags = G_APPLICATION_NON_UNIQUE;

  return g_object_new (EV_TYPE_APPLICATION,
                       "application-id", APPLICATION_NAME,
                       "flags", flags,
                       NULL);
}

#ifdef ENABLE_DBUS
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

                /* The given name might come with the screen number, because GdkAppLaunchContext
                 * uses gdk_screen_make_display_name().
                 */
                if (g_str_has_prefix (name, display_name)) {
			display = l->data;
			break;
		}
	}

	g_slist_free (displays);

	return display != NULL ? display : gdk_display_open (name);
}
#endif

static void
ev_spawn (const char     *uri,
	  GdkScreen      *screen,
	  EvLinkDest     *dest,
	  EvWindowRunMode mode,
	  const gchar    *search_string,
	  guint           timestamp)
{
	GString *cmd;
	gchar *path, *cmdline;
	GAppInfo *app;
	GError  *error = NULL;

	cmd = g_string_new (NULL);

#ifdef G_OS_WIN32
{
	gchar *dir;

	dir = g_win32_get_package_installation_directory_of_module (NULL);
	path = g_build_filename (dir, "bin", "evince", NULL);

	g_free (dir);
}
#else
	path = g_build_filename (BINDIR, "evince", NULL);
#endif

	g_string_append_printf (cmd, " %s", path);
	g_free (path);
	
	/* Page label */
	if (dest) {
                switch (ev_link_dest_get_dest_type (dest)) {
                case EV_LINK_DEST_TYPE_PAGE_LABEL:
                        g_string_append_printf (cmd, " --page-label=%s",
                                                ev_link_dest_get_page_label (dest));
                        break;
                case EV_LINK_DEST_TYPE_PAGE:
                        g_string_append_printf (cmd, " --page-index=%d",
                                                ev_link_dest_get_page (dest) + 1);
                        break;
                case EV_LINK_DEST_TYPE_NAMED:
                        g_string_append_printf (cmd, " --named-dest=%s",
                                                ev_link_dest_get_named_dest (dest));
                        break;
                default:
                        break;
                }
	}

	/* Find string */
	if (search_string) {
		g_string_append_printf (cmd, " --find=%s", search_string);
	}

	/* Mode */
	switch (mode) {
	case EV_WINDOW_MODE_FULLSCREEN:
		g_string_append (cmd, " -f");
		break;
	case EV_WINDOW_MODE_PRESENTATION:
		g_string_append (cmd, " -s");
		break;
	default:
		break;
	}

	cmdline = g_string_free (cmd, FALSE);
	app = g_app_info_create_from_commandline (cmdline, NULL, G_APP_INFO_CREATE_SUPPORTS_URIS, &error);

	if (app != NULL) {
                GList uri_list;
                GList *uris = NULL;
		GdkAppLaunchContext *ctx;

		ctx = gdk_display_get_app_launch_context (gdk_screen_get_display (screen));
		gdk_app_launch_context_set_screen (ctx, screen);
		gdk_app_launch_context_set_timestamp (ctx, timestamp);

                /* Some URIs can be changed when passed through a GFile
                 * (for instance unsupported uris with strange formats like mailto:),
                 * so if you have a textual uri you want to pass in as argument,
                 * consider using g_app_info_launch_uris() instead.
                 * See https://bugzilla.gnome.org/show_bug.cgi?id=644604
                 */
                if (uri) {
                        uri_list.data = (gchar *)uri;
                        uri_list.prev = uri_list.next = NULL;
                        uris = &uri_list;
                }
		g_app_info_launch_uris (app, uris, G_APP_LAUNCH_CONTEXT (ctx), &error);

		g_object_unref (app);
		g_object_unref (ctx);
	}

	if (error != NULL) {
		g_printerr ("Error launching evince %s: %s\n", uri, error->message);
		g_error_free (error);
	}

	g_free (cmdline);
}

static EvWindow *
ev_application_get_empty_window (EvApplication *application,
				 GdkScreen     *screen)
{
	EvWindow *empty_window = NULL;
	GList    *windows, *l;

        windows = gtk_application_get_windows (GTK_APPLICATION (application));
	for (l = windows; l != NULL; l = l->next) {
		EvWindow *window;

                if (!EV_IS_WINDOW (l->data))
                          continue;

                window = EV_WINDOW (l->data);

		if (ev_window_is_empty (window) &&
		    gtk_window_get_screen (GTK_WINDOW (window)) == screen) {
			empty_window = window;
			break;
		}
	}

	return empty_window;
}


#ifdef ENABLE_DBUS
typedef struct {
	gchar          *uri;
	GdkScreen      *screen;
	EvLinkDest     *dest;
	EvWindowRunMode mode;
	gchar          *search_string;
	guint           timestamp;
} EvRegisterDocData;

static void
ev_register_doc_data_free (EvRegisterDocData *data)
{
	if (!data)
		return;

	g_free (data->uri);
	if (data->search_string)
		g_free (data->search_string);
	if (data->dest)
		g_object_unref (data->dest);

	g_free (data);
}

static void
on_reload_cb (GObject      *source_object,
	      GAsyncResult *res,
	      gpointer      user_data)
{
	GDBusConnection *connection = G_DBUS_CONNECTION (source_object);
	GVariant        *value;
	GError          *error = NULL;

        g_application_release (g_application_get_default ());

	value = g_dbus_connection_call_finish (connection, res, &error);
	if (value != NULL) {
                g_variant_unref (value);
        } else {
		g_printerr ("Failed to Reload: %s\n", error->message);
		g_error_free (error);
	}

	/* We did not open a window, so manually clear the startup
	 * notification. */
	gdk_notify_startup_complete ();
}

static void
on_register_uri_cb (GObject      *source_object,
		    GAsyncResult *res,
		    gpointer      user_data)
{
	GDBusConnection   *connection = G_DBUS_CONNECTION (source_object);
	EvRegisterDocData *data = (EvRegisterDocData *)user_data;
	EvApplication     *application = EV_APP;
	GVariant          *value;
	const gchar       *owner;
	GVariantBuilder    builder;
	GError            *error = NULL;

        g_application_release (G_APPLICATION (application));

	value = g_dbus_connection_call_finish (connection, res, &error);
	if (!value) {
		g_printerr ("Error registering document: %s\n", error->message);
		g_error_free (error);

		_ev_application_open_uri_at_dest (application,
						  data->uri,
						  data->screen,
						  data->dest,
						  data->mode,
						  data->search_string,
						  data->timestamp);
		ev_register_doc_data_free (data);

		return;
	}

	g_variant_get (value, "(&s)", &owner);

	/* This means that the document wasn't already registered; go
         * ahead with opening it.
         */
	if (owner[0] == '\0') {
                g_variant_unref (value);

		application->doc_registered = TRUE;

		_ev_application_open_uri_at_dest (application,
						  data->uri,
						  data->screen,
						  data->dest,
						  data->mode,
						  data->search_string,
						  data->timestamp);
		ev_register_doc_data_free (data);

                return;
        }

	/* Already registered */
	g_variant_builder_init (&builder, G_VARIANT_TYPE ("(a{sv}u)"));
        g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{sv}"));
        g_variant_builder_add (&builder, "{sv}",
                               "display",
                               g_variant_new_string (gdk_display_get_name (gdk_screen_get_display (data->screen))));
	if (data->dest) {
                switch (ev_link_dest_get_dest_type (data->dest)) {
                case EV_LINK_DEST_TYPE_PAGE_LABEL:
                        g_variant_builder_add (&builder, "{sv}", "page-label",
                                               g_variant_new_string (ev_link_dest_get_page_label (data->dest)));
                        break;
                case EV_LINK_DEST_TYPE_PAGE:
                        g_variant_builder_add (&builder, "{sv}", "page-index",
                                               g_variant_new_uint32 (ev_link_dest_get_page (data->dest)));
                        break;
                case EV_LINK_DEST_TYPE_NAMED:
                        g_variant_builder_add (&builder, "{sv}", "named-dest",
                                               g_variant_new_string (ev_link_dest_get_named_dest (data->dest)));
                        break;
                default:
                        break;
                }
	}
	if (data->search_string) {
                g_variant_builder_add (&builder, "{sv}",
                                       "find-string",
                                       g_variant_new_string (data->search_string));
	}
	if (data->mode != EV_WINDOW_MODE_NORMAL) {
                g_variant_builder_add (&builder, "{sv}",
                                       "mode",
                                       g_variant_new_uint32 (data->mode));
	}
        g_variant_builder_close (&builder);

        g_variant_builder_add (&builder, "u", data->timestamp);

        g_dbus_connection_call (connection,
				owner,
				APPLICATION_DBUS_OBJECT_PATH,
				APPLICATION_DBUS_INTERFACE,
				"Reload",
				g_variant_builder_end (&builder),
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				on_reload_cb,
				NULL);
        g_application_hold (G_APPLICATION (application));
	g_variant_unref (value);
	ev_register_doc_data_free (data);
}

/*
 * ev_application_register_uri:
 * @application: The instance of the application.
 * @uri: The uri to be opened.
 * @screen: The screen where the link will be shown.
 * @dest: The #EvLinkDest of the document.
 * @mode: The run mode of the window.
 * @search_string: The word or phrase to find in the document.
 * @timestamp: Current time value.
 *
 * Registers @uri with evince-daemon.
 *
 */
static void
ev_application_register_uri (EvApplication  *application,
			     const gchar    *uri,
                             GdkScreen      *screen,
                             EvLinkDest     *dest,
                             EvWindowRunMode mode,
                             const gchar    *search_string,
			     guint           timestamp)
{
	EvRegisterDocData *data;

	/* If connection hasn't been made fall back to opening without D-BUS features */
	if (!application->skeleton) {
		_ev_application_open_uri_at_dest (application, uri, screen, dest, mode, search_string, timestamp);
		return;
	}

	if (application->doc_registered) {
		/* Already registered, reload */
		GList *windows, *l;

		windows = gtk_application_get_windows (GTK_APPLICATION (application));
		for (l = windows; l != NULL; l = g_list_next (l)) {
                        if (!EV_IS_WINDOW (l->data))
                                continue;

			ev_application_open_uri_in_window (application, uri,
                                                           EV_WINDOW (l->data),
							   screen, dest, mode,
							   search_string,
							   timestamp);
		}

		return;
	}

	data = g_new (EvRegisterDocData, 1);
	data->uri = g_strdup (uri);
	data->screen = screen;
	data->dest = dest ? g_object_ref (dest) : NULL;
	data->mode = mode;
	data->search_string = search_string ? g_strdup (search_string) : NULL;
	data->timestamp = timestamp;

        g_dbus_connection_call (g_application_get_dbus_connection (G_APPLICATION (application)),
				EVINCE_DAEMON_SERVICE,
				EVINCE_DAEMON_OBJECT_PATH,
				EVINCE_DAEMON_INTERFACE,
				"RegisterDocument",
				g_variant_new ("(s)", uri),
				G_VARIANT_TYPE ("(s)"),
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				on_register_uri_cb,
				data);

        g_application_hold (G_APPLICATION (application));
}

static void
ev_application_unregister_uri (EvApplication *application,
			       const gchar   *uri)
{
        GVariant *value;
	GError   *error = NULL;

	if (!application->doc_registered)
		return;

	/* This is called from ev_application_shutdown(),
	 * so it's safe to use the sync api
	 */
        value = g_dbus_connection_call_sync (
		g_application_get_dbus_connection (G_APPLICATION (application)),
		EVINCE_DAEMON_SERVICE,
		EVINCE_DAEMON_OBJECT_PATH,
		EVINCE_DAEMON_INTERFACE,
		"UnregisterDocument",
		g_variant_new ("(s)", uri),
		NULL,
		G_DBUS_CALL_FLAGS_NO_AUTO_START,
		-1,
		NULL,
		&error);
        if (value == NULL) {
		g_printerr ("Error unregistering document: %s\n", error->message);
		g_error_free (error);
	} else {
                g_variant_unref (value);
	}
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

        if (uri == NULL)
                uri = application->uri;

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
	if (GDK_IS_X11_WINDOW (gdk_window)) {
		if (timestamp <= 0)
			timestamp = gdk_x11_get_server_time (gdk_window);
		gdk_x11_window_set_user_time (gdk_window, timestamp);

		gtk_window_present (GTK_WINDOW (ev_window));
	} else
#endif /* GDK_WINDOWING_X11 */
	{
		gtk_window_present_with_time (GTK_WINDOW (ev_window), timestamp);
	}
}

static void
_ev_application_open_uri_at_dest (EvApplication  *application,
				  const gchar    *uri,
				  GdkScreen      *screen,
				  EvLinkDest     *dest,
				  EvWindowRunMode mode,
				  const gchar    *search_string,
				  guint           timestamp)
{
	EvWindow *ev_window;

	ev_window = ev_application_get_empty_window (application, screen);
	if (!ev_window)
		ev_window = EV_WINDOW (ev_window_new ());

	ev_application_open_uri_in_window (application, uri, ev_window,
					   screen, dest, mode,
					   search_string,
					   timestamp);
}

/**
 * ev_application_open_uri_at_dest:
 * @application: The instance of the application.
 * @uri: The uri to be opened.
 * @screen: Thee screen where the link will be shown.
 * @dest: The #EvLinkDest of the document.
 * @mode: The run mode of the window.
 * @search_string: The word or phrase to find in the document.
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
	g_return_if_fail (uri != NULL);

	if (application->uri && strcmp (application->uri, uri) != 0) {
		/* spawn a new evince process */
		ev_spawn (uri, screen, dest, mode, search_string, timestamp);
		return;
	} else if (!application->uri) {
		application->uri = g_strdup (uri);
	}

#ifdef ENABLE_DBUS
	/* Register the uri or send Reload to
	 * remote instance if already registered
	 */
	ev_application_register_uri (application, uri, screen, dest, mode, search_string, timestamp);
#else
	_ev_application_open_uri_at_dest (application, uri, screen, dest, mode, search_string, timestamp);
#endif /* ENABLE_DBUS */
}

void
ev_application_new_window (EvApplication *application,
			   GdkScreen     *screen,
			   guint32        timestamp)
{
        /* spawn an empty window */
	ev_spawn (NULL, screen, NULL, EV_WINDOW_MODE_NORMAL, NULL, timestamp);
}

/**
 * ev_application_open_recent_view:
 * @application: The instance of the application.
 * @timestamp: Current time value.
 *
 * Creates a new window showing the recent view
 */
void
ev_application_open_recent_view (EvApplication *application,
                                 GdkScreen     *screen,
                                 guint32        timestamp)
{
	GtkWidget *new_window = ev_window_new ();

	ev_window_open_recent_view (EV_WINDOW (new_window));

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
	if (GDK_IS_X11_WINDOW (gdk_window)) {
		if (timestamp <= 0)
			timestamp = gdk_x11_get_server_time (gdk_window);
		gdk_x11_window_set_user_time (gdk_window, timestamp);

		gtk_window_present (GTK_WINDOW (new_window));
	} else
#endif /* GDK_WINDOWING_X11 */
	{
		gtk_window_present_with_time (GTK_WINDOW (new_window), timestamp);
	}
}

#ifdef ENABLE_DBUS
static gboolean
handle_get_window_list_cb (EvEvinceApplication   *object,
                           GDBusMethodInvocation *invocation,
                           EvApplication         *application)
{
        GList     *windows, *l;
        GPtrArray *paths;

        paths = g_ptr_array_new ();

        windows = gtk_application_get_windows (GTK_APPLICATION (application));
        for (l = windows; l; l = g_list_next (l)) {
                if (!EV_IS_WINDOW (l->data))
                        continue;

                g_ptr_array_add (paths, (gpointer) ev_window_get_dbus_object_path (EV_WINDOW (l->data)));
        }

        g_ptr_array_add (paths, NULL);
        ev_evince_application_complete_get_window_list (object, invocation,
                                                        (const char * const *) paths->pdata);

        g_ptr_array_free (paths, TRUE);

        return TRUE;
}

static gboolean
handle_reload_cb (EvEvinceApplication   *object,
                  GDBusMethodInvocation *invocation,
                  GVariant              *args,
                  guint                  timestamp,
                  EvApplication         *application)
{
        GList           *windows, *l;
        GVariantIter     iter;
        const gchar     *key;
        GVariant        *value;
        GdkDisplay      *display = NULL;
        EvLinkDest      *dest = NULL;
        EvWindowRunMode  mode = EV_WINDOW_MODE_NORMAL;
        const gchar     *search_string = NULL;
        GdkScreen       *screen = NULL;

        g_variant_iter_init (&iter, args);

        while (g_variant_iter_loop (&iter, "{&sv}", &key, &value)) {
                if (strcmp (key, "display") == 0 && g_variant_classify (value) == G_VARIANT_CLASS_STRING) {
                        display = ev_display_open_if_needed (g_variant_get_string (value, NULL));
                } else if (strcmp (key, "mode") == 0 && g_variant_classify (value) == G_VARIANT_CLASS_UINT32) {
                        mode = g_variant_get_uint32 (value);
                } else if (strcmp (key, "page-label") == 0 && g_variant_classify (value) == G_VARIANT_CLASS_STRING) {
                        dest = ev_link_dest_new_page_label (g_variant_get_string (value, NULL));
                } else if (strcmp (key, "named-dest") == 0 && g_variant_classify (value) == G_VARIANT_CLASS_STRING) {
                        dest = ev_link_dest_new_named (g_variant_get_string (value, NULL));
                } else if (strcmp (key, "page-index") == 0 && g_variant_classify (value) == G_VARIANT_CLASS_UINT32) {
                        dest = ev_link_dest_new_page (g_variant_get_uint32 (value));
                } else if (strcmp (key, "find-string") == 0 && g_variant_classify (value) == G_VARIANT_CLASS_STRING) {
                        search_string = g_variant_get_string (value, NULL);
                }
        }

        if (display != NULL)
                screen = gdk_display_get_default_screen (display);
        else
                screen = gdk_screen_get_default ();

        windows = gtk_application_get_windows (GTK_APPLICATION ((application)));
        for (l = windows; l != NULL; l = g_list_next (l)) {
                if (!EV_IS_WINDOW (l->data))
                        continue;

                ev_application_open_uri_in_window (application, NULL,
                                                   EV_WINDOW (l->data),
                                                   screen, dest, mode,
                                                   search_string,
                                                   timestamp);
        }

        if (dest)
                g_object_unref (dest);

        ev_evince_application_complete_reload (object, invocation);

        return TRUE;
}
#endif /* ENABLE_DBUS */

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

        accel_map_file = g_build_filename (application->dot_dir, "accels", NULL);
	tmp_filename = g_strdup_printf ("%s.XXXXXX", accel_map_file);

	fd = g_mkstemp (tmp_filename);
	if (fd == -1) {
		g_free (accel_map_file);
		g_free (tmp_filename);

		return;
	}
	gtk_accel_map_save_fd (fd);
	close (fd);

        g_mkdir_with_parents (application->dot_dir, 0700);
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

        accel_map_file = g_build_filename (application->dot_dir, "accels", NULL);
	gtk_accel_map_load (accel_map_file);
	g_free (accel_map_file);
}

static void
ev_application_migrate_config_dir (EvApplication *application)
{
        const gchar        *userdir;
        gchar              *old_dot_dir;
        gchar              *old_accels;
        GError             *error;
        gint                i;
        gboolean            dir_created = FALSE;
        static const gchar *config_files[] = {
                "evince_toolbar.xml",
                "print-settings",
                NULL
        };

        userdir = g_getenv ("GNOME22_USER_DIR");
        if (userdir) {
                old_dot_dir = g_build_filename (userdir, "evince", NULL);
                old_accels = g_build_filename (userdir, "accels", "evince", NULL);
        } else {
                old_dot_dir = g_build_filename (g_get_home_dir (),
                                                ".gnome2",
                                                "evince",
                                                NULL);
                old_accels = g_build_filename (g_get_home_dir (),
                                               ".gnome2", "accels",
                                               "evince", NULL);
        }

        if (g_file_test (old_dot_dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
                for (i = 0; config_files[i]; i++) {
                        gchar   *old_filename;
                        gchar   *new_filename;
                        GFile   *old_file;
                        GFile   *new_file;

                        old_filename = g_build_filename (old_dot_dir, config_files[i], NULL);
                        if (!g_file_test (old_filename, G_FILE_TEST_EXISTS)) {
                                g_free (old_filename);
                                continue;
                        }

                        if (!dir_created) {
                                g_mkdir_with_parents (application->dot_dir, 0700);
                                dir_created = TRUE;
                        }

                        new_filename = g_build_filename (application->dot_dir, config_files[i], NULL);
                        old_file = g_file_new_for_path (old_filename);
                        new_file = g_file_new_for_path (new_filename);

                        error = NULL;
                        g_file_move (old_file, new_file, 0, NULL, NULL, NULL, &error);
                        if (error) {
                                g_printerr ("Error migrating config file %s: %s\n",
                                            old_filename, error->message);
                                g_error_free (error);
                        }

                        g_free (old_filename);
                        g_free (new_filename);
                        g_object_unref (old_file);
                        g_object_unref (new_file);
                }
        }

        g_free (old_dot_dir);

        if (g_file_test (old_accels, G_FILE_TEST_EXISTS)) {
                gchar *new_accels;
                GFile *old_accels_file;
                GFile *new_accels_file;

                if (!dir_created)
                        g_mkdir_with_parents (application->dot_dir, 0700);

                new_accels = g_build_filename (application->dot_dir, "accels", NULL);
                old_accels_file = g_file_new_for_path (old_accels);
                new_accels_file = g_file_new_for_path (new_accels);

                error = NULL;
                g_file_move (old_accels_file, new_accels_file, 0, NULL, NULL, NULL, &error);
                if (error) {
                        g_printerr ("Error migrating accelerator specifications file %s: %s\n",
                                    old_accels, error->message);
                        g_error_free (error);
                }

                g_free (new_accels);
                g_object_unref (old_accels_file);
                g_object_unref (new_accels_file);
        }

        g_free (old_accels);
}

static void
ev_application_startup (GApplication *gapplication)
{
        const gchar *action_accels[] = {
          "win.open",                   "<Ctrl>O", NULL,
          "win.open-copy",              "<Ctrl>N", NULL,
          "win.save-as",                "<Ctrl>S", NULL,
          "win.print",                  "<Ctrl>P", NULL,
          "win.show-properties",        "<alt>Return", NULL,
          "win.copy",                   "<Ctrl>C", "<Ctrl>Insert", NULL,
          "win.select-all",             "<Ctrl>A", NULL,
          "win.save-settings",          "<Ctrl>T", NULL,
          "win.add-bookmark",           "<Ctrl>D", NULL,
          "win.delete-bookmark",        "<Ctrl><Shift>D", NULL,
          "win.close",                  "<Ctrl>W", NULL,
          "win.escape",                 "Escape", NULL,
          "win.find",                   "<Ctrl>F", "slash", NULL,
          "win.find-next",              "<Ctrl>G", "F3", NULL,
          "win.find-previous",          "<Ctrl><Shift>G", "<Shift>F3", NULL,
          "win.select-page",            "<Ctrl>L", NULL,
          "win.go-backwards",           "<Shift>Page_Up", NULL,
          "win.go-forward",             "<Shift>Page_Down", NULL,
          "win.go-next-page",           "n", "<Ctrl>Page_Down", NULL,
          "win.go-previous-page",       "p", "<Ctrl>Page_Up", NULL,
          "win.go-back-history",        "<alt>P", "Back", NULL,
          "win.go-forward-history",     "<alt>N", "Forward", NULL,
          "win.sizing-mode::fit-page",  "f", NULL,
          "win.sizing-mode::fit-width", "w", NULL,
          "win.sizing-mode::automatic", "a", NULL,
          "win.default-zoom",           "<Ctrl>0", NULL,
          "win.toggle-menu",            "F10", NULL,
          "win.caret-navigation",       "F7", NULL,
          "win.zoom-in",                "plus", "<Ctrl>plus", "KP_Add", "<Ctrl>KP_Add", "equal", "<Ctrl>equal", NULL,
          "win.zoom-out",               "minus", "<Ctrl>minus", "KP_Subtract", "<Ctrl>KP_Subtract", NULL,
          "win.show-side-pane",         "F9", NULL,
          "win.fullscreen",             "F11", NULL,
          "win.presentation",           "F5", "<Shift>F5", NULL,
          "win.continuous",             "c", NULL,
          "win.dual-page",              "d", NULL,
          "win.rotate-left",            "<Ctrl>Left", NULL,
          "win.rotate-right",           "<Ctrl>Right", NULL,
          "win.inverted-colors",        "<Ctrl>I", NULL,
          "win.reload",                 "<Ctrl>R", NULL,
          "win.add-annotation",         "s", NULL,
          "win.highlight-annotation",   "<Ctrl>H", NULL,
          "win.help",                   "F1", NULL,
          "win.about",                  NULL, NULL,
          NULL
        };

        EvApplication *application = EV_APPLICATION (gapplication);
        const gchar **it;

	g_application_set_resource_base_path (gapplication, "/org/gnome/evince");

        G_APPLICATION_CLASS (ev_application_parent_class)->startup (gapplication);

        hdy_init ();

        for (it = action_accels; it[0]; it += g_strv_length ((gchar **)it) + 1)
                gtk_application_set_accels_for_action (GTK_APPLICATION (application), it[0], &it[1]);
}

static void
ev_application_shutdown (GApplication *gapplication)
{
        EvApplication *application = EV_APPLICATION (gapplication);

	if (application->uri) {
#ifdef ENABLE_DBUS
		ev_application_unregister_uri (application,
					       application->uri);
#endif
		g_free (application->uri);
		application->uri = NULL;
	}

	ev_application_accel_map_save (application);

        g_free (application->dot_dir);
        application->dot_dir = NULL;

        G_APPLICATION_CLASS (ev_application_parent_class)->shutdown (gapplication);
}

static void
ev_application_activate (GApplication *gapplication)
{
        EvApplication *application = EV_APPLICATION (gapplication);
        GList *windows, *l;

        windows = gtk_application_get_windows (GTK_APPLICATION (application));
        for (l = windows; l != NULL; l = l->next) {
                if (!EV_IS_WINDOW (l->data))
                        continue;

                gtk_window_present (GTK_WINDOW (l->data));
        }
}

#ifdef ENABLE_DBUS
static gboolean
ev_application_dbus_register (GApplication    *gapplication,
                              GDBusConnection *connection,
                              const gchar     *object_path,
                              GError         **error)
{
        EvApplication *application = EV_APPLICATION (gapplication);
        EvEvinceApplication *skeleton;

        if (!G_APPLICATION_CLASS (ev_application_parent_class)->dbus_register (gapplication,
                                                                               connection,
                                                                               object_path,
                                                                               error))
                return FALSE;

        skeleton = ev_evince_application_skeleton_new ();
        if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (skeleton),
                                               connection,
                                               APPLICATION_DBUS_OBJECT_PATH,
                                               error)) {
                g_object_unref (skeleton);

                return FALSE;
        }

        application->skeleton = skeleton;
        g_signal_connect (skeleton, "handle-get-window-list",
                          G_CALLBACK (handle_get_window_list_cb),
                          application);
        g_signal_connect (skeleton, "handle-reload",
                          G_CALLBACK (handle_reload_cb),
                          application);
        application->keys = ev_media_player_keys_new ();

        return TRUE;
}

static void
ev_application_dbus_unregister (GApplication    *gapplication,
                                GDBusConnection *connection,
                                const gchar     *object_path)
{
        EvApplication *application = EV_APPLICATION (gapplication);

        if (application->keys) {
                g_object_unref (application->keys);
                application->keys = NULL;
        }
        if (application->skeleton != NULL) {
                g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (application->skeleton));
                g_object_unref (application->skeleton);
                application->skeleton = NULL;
        }

        G_APPLICATION_CLASS (ev_application_parent_class)->dbus_unregister (gapplication,
                                                                            connection,
                                                                            object_path);
}

#endif /* ENABLE_DBUS */

static void
ev_application_class_init (EvApplicationClass *ev_application_class)
{
        GApplicationClass *g_application_class = G_APPLICATION_CLASS (ev_application_class);

        g_application_class->startup = ev_application_startup;
        g_application_class->activate = ev_application_activate;
        g_application_class->shutdown = ev_application_shutdown;

#ifdef ENABLE_DBUS
        g_application_class->dbus_register = ev_application_dbus_register;
        g_application_class->dbus_unregister = ev_application_dbus_unregister;
#endif
}

static void
ev_application_init (EvApplication *ev_application)
{
        ev_application->dot_dir = g_build_filename (g_get_user_config_dir (),
                                                    "evince", NULL);
        if (!g_file_test (ev_application->dot_dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
                ev_application_migrate_config_dir (ev_application);

	ev_application_accel_map_load (ev_application);
}

gboolean
ev_application_has_window (EvApplication *application)
{
	GList *l, *windows;

	windows = gtk_application_get_windows (GTK_APPLICATION (application));
	for (l = windows; l != NULL; l = l->next) {
		if (!EV_IS_WINDOW (l->data))
                        continue;

                return TRUE;
	}

	return FALSE;
}

guint
ev_application_get_n_windows (EvApplication *application)
{
        GList *l, *windows;
        guint retval = 0;

        windows = gtk_application_get_windows (GTK_APPLICATION (application));
        for (l = windows; l != NULL && !retval; l = l->next) {
                if (!EV_IS_WINDOW (l->data))
                        continue;

                retval++;
	}

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

const gchar *
ev_application_get_dot_dir (EvApplication *application,
                            gboolean create)
{
        if (create)
                g_mkdir_with_parents (application->dot_dir, 0700);

	return application->dot_dir;
}
