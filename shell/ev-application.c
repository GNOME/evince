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
	}

	driver_proxy = dbus_g_proxy_new_for_name (connection,
						  DBUS_SERVICE_DBUS,
						  DBUS_PATH_DBUS,
						  DBUS_INTERFACE_DBUS);

	if (!org_freedesktop_DBus_request_name (driver_proxy,
                                        	APPLICATION_SERVICE_NAME,
						0, &request_name_result, &err))
	{
		g_warning ("Service registration failed.");
	}

	if (request_name_result == DBUS_REQUEST_NAME_REPLY_EXISTS) {
		return FALSE;
	}

	dbus_g_object_class_install_info (G_OBJECT_GET_CLASS (application),
                                          &dbus_glib_ev_application_object_info);
	dbus_g_connection_register_g_object (connection,
					     "/org/gnome/evince/Evince",
                                             G_OBJECT (application));

	return TRUE;
}
#endif

EvApplication *
ev_application_get_instance (void)
{
	static EvApplication *instance;

	if (!instance) {
		instance = EV_APPLICATION (g_object_new (EV_TYPE_APPLICATION, NULL));
	}

	return instance;
}

void
ev_application_open_window (EvApplication *application)
{
	gtk_widget_show (ev_window_new ());
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

void
ev_application_open_uri (EvApplication *application,
			 const char    *uri,
			 const char    *page_label)
{
	EvWindow *new_window;

	g_return_if_fail (uri != NULL);

	new_window = ev_application_get_uri_window (application, uri);
	if (new_window != NULL) {
		gtk_window_present (GTK_WINDOW (new_window));
		return;
	}

	new_window = ev_application_get_empty_window (application);

	if (new_window == NULL) {
		new_window = EV_WINDOW (ev_window_new ());
	}
	
	ev_window_open_uri (new_window, uri);

	gtk_window_present (GTK_WINDOW (new_window));

	if (page_label != NULL) {
		ev_window_open_page_label (new_window, page_label);
	}
}

void
ev_application_open_uri_list (EvApplication *application, GSList *uri_list)
{
	GSList *l;

	for (l = uri_list; l != NULL; l = l->next) {
		ev_application_open_uri (application, (char *)l->data, NULL);
	}
}

void
ev_application_shutdown (EvApplication *application)
{
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
}

