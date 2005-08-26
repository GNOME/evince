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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ev-application.h"
#include "ev-utils.h"
#include "ev-document-types.h"
#include "ev-file-helpers.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gtk/gtkfilechooserdialog.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkmain.h>
#include <string.h>

#ifdef ENABLE_DBUS
#include "ev-application-service.h"
#include <dbus/dbus-glib-bindings.h>
#endif

G_DEFINE_TYPE (EvApplication, ev_application, G_TYPE_OBJECT);

#define EV_APPLICATION_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_APPLICATION, EvApplicationPrivate))

#define APPLICATION_SERVICE_NAME "org.gnome.evince.ApplicationService"

#ifdef ENABLE_DBUS
gboolean
ev_application_register_service (EvApplication *application)
{
	DBusGConnection *connection;
	DBusGProxy *driver_proxy;
	GError *err = NULL;
	guint request_name_result;

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
						0, &request_name_result, &err)) {
		g_warning ("Service registration failed.");
		g_clear_error (&err);
	}

	if (request_name_result == DBUS_REQUEST_NAME_REPLY_EXISTS) {
		return FALSE;
	}

#if DBUS_VERSION == 33
	dbus_g_object_class_install_info (G_OBJECT_GET_CLASS (application),
					  &dbus_glib_ev_application_object_info);
#else
	dbus_g_object_type_install_info (EV_TYPE_APPLICATION,
					 &dbus_glib_ev_application_object_info);
#endif

	dbus_g_connection_register_g_object (connection,
					     "/org/gnome/evince/Evince",
                                             G_OBJECT (application));

	return TRUE;
}
#endif /* ENABLE_DBUS */

EvApplication *
ev_application_get_instance (void)
{
	static EvApplication *instance;

	if (!instance) {
		instance = EV_APPLICATION (g_object_new (EV_TYPE_APPLICATION, NULL));
	}

	return instance;
}

gboolean
ev_application_open_window (EvApplication  *application,
			    guint32         timestamp,
			    GError        **error)
{
	GtkWidget *new_window = ev_window_new ();

	gtk_widget_show (new_window);
	
#ifdef HAVE_GTK_WINDOW_PRESENT_WITH_TIME
	gtk_window_present_with_time (GTK_WINDOW (new_window),
				      timestamp);
#else
	gtk_window_present (GTK_WINDOW (new_window));
#endif

	return TRUE;
}

static EvWindow *
ev_application_get_empty_window (EvApplication *application)
{
	EvWindow *empty_window = NULL;
	GList *windows = gtk_window_list_toplevels ();
	GList *l;

	for (l = windows; l != NULL; l = l->next) {
		if (EV_IS_WINDOW (l->data)) {
			EvWindow *window = EV_WINDOW (l->data);

			if (ev_window_is_empty (window)) {
				empty_window = window;
				break;
			}
		}
	}

	g_list_free (windows);
	
	return empty_window;
}

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

			if (window_uri && strcmp (window_uri, uri) == 0) {
				uri_window = window;
				break;
			}
		}
	}

	g_list_free (windows);
	
	return uri_window;
}

gboolean
ev_application_open_uri (EvApplication  *application,
			 const char     *uri,
			 const char     *page_label,
			 guint           timestamp,
			 GError        **error)
{
	EvWindow *new_window;

	g_return_val_if_fail (uri != NULL, FALSE);

	new_window = ev_application_get_uri_window (application, uri);
	if (new_window != NULL) {
#ifdef HAVE_GTK_WINDOW_PRESENT_WITH_TIME
		gtk_window_present_with_time (GTK_WINDOW (new_window),
					      timestamp);
#else
		gtk_window_present (GTK_WINDOW (new_window));
#endif	
		return TRUE;
	}

	new_window = ev_application_get_empty_window (application);

	if (new_window == NULL) {
		new_window = EV_WINDOW (ev_window_new ());
		gtk_widget_show (GTK_WIDGET (new_window));
	}
	
	ev_window_open_uri (new_window, uri);

#ifdef HAVE_GTK_WINDOW_PRESENT_WITH_TIME
	gtk_window_present_with_time (GTK_WINDOW (new_window),
				      timestamp);
#else
	gtk_window_present (GTK_WINDOW (new_window));
#endif

	if (page_label != NULL) {
		ev_window_open_page_label (new_window, page_label);
	}

	return TRUE;
}

void
ev_application_open_uri_list (EvApplication *application,
			      GSList        *uri_list,
			      guint          timestamp)
{
	GSList *l;

	for (l = uri_list; l != NULL; l = l->next) {
		ev_application_open_uri (application, (char *)l->data,
					 NULL,
					 timestamp,
					 NULL);
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

	if (application->recent_model) {
		g_object_unref (application->recent_model);
		application->recent_model = NULL;
	}

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
	ev_application->toolbars_model = egg_toolbars_model_new ();

	ev_application->toolbars_file = g_build_filename
			(ev_dot_dir (), "evince_toolbar.xml", NULL);

	if (!egg_toolbars_model_load (ev_application->toolbars_model,
				      ev_application->toolbars_file)) {
		egg_toolbars_model_load (ev_application->toolbars_model,
					 DATADIR"/evince-toolbar.xml");
	}

	egg_toolbars_model_set_flags (ev_application->toolbars_model, 0,
				      EGG_TB_MODEL_NOT_REMOVABLE); 
				      
	ev_application->recent_model = egg_recent_model_new (EGG_RECENT_MODEL_SORT_MRU);
	/* FIXME we should add a mime type filter but current eggrecent
           has only a varargs style api which does not work well when
	   the list of mime types is dynamic */
	egg_recent_model_set_limit (ev_application->recent_model, 5);	
	egg_recent_model_set_filter_groups (ev_application->recent_model,
    	    	    			    "Evince", NULL);
}

EggToolbarsModel *ev_application_get_toolbars_model (EvApplication *application)
{
	return application->toolbars_model;
}

EggRecentModel *ev_application_get_recent_model (EvApplication *application)
{
	return application->recent_model;
}

void ev_application_save_toolbars_model (EvApplication *application)
{
        egg_toolbars_model_save (application->toolbars_model,
				 application->toolbars_file, "1.0");
}


