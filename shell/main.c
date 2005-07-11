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
#include <gtk/gtkmain.h>
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

static struct poptOption popt_options[] =
{
	{ "page-label", 'p', POPT_ARG_STRING, &ev_page_label, 0, N_("The page of the document to display."), N_("PAGE")},
	{ NULL, 0, 0, NULL, 0, NULL, NULL }
};

static void
load_files (const char **files)
{
	int i;

	if (!files) {
		ev_application_open_window (EV_APP, NULL);
		return;
	}

	for (i = 0; files[i]; i++) {
		char *uri;

		uri = gnome_vfs_make_uri_from_shell_arg (files[i]);
		ev_application_open_uri (EV_APP, uri, ev_page_label, NULL);		
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

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
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
		call = dbus_g_proxy_begin_call (remote_object, "OpenWindow", DBUS_TYPE_INVALID);

		if (!dbus_g_proxy_end_call (remote_object, call, &error, DBUS_TYPE_INVALID)) {
			g_warning (error->message);
			g_clear_error (&error);
			return FALSE;
		}
#elif DBUS_VERSION == 34
		call = dbus_g_proxy_begin_call (remote_object, "OpenWindow", G_TYPE_INVALID);

		if (!dbus_g_proxy_end_call (remote_object, call, &error, G_TYPE_INVALID)) {
			g_warning (error->message);
			g_clear_error (&error);
			return FALSE;
		}
#else
		if (!dbus_g_proxy_call (remote_object, "OpenWindow", &error, G_TYPE_INVALID)) {
			g_warning (error->message);
			g_clear_error (&error);
			return FALSE;
		}
#endif
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

	return result;
}
#endif /* ENABLE_DBUS */

int
main (int argc, char *argv[])
{
	gboolean enable_metadata = FALSE;
	poptContext context;
        GValue context_as_value = { 0 };
	GnomeProgram *program;

#ifdef ENABLE_NLS
	/* Initialize the i18n stuff */
	bindtextdomain(GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
#endif

	program = gnome_program_init (PACKAGE, VERSION,
                                      LIBGNOMEUI_MODULE, argc, argv,
                                      GNOME_PARAM_POPT_TABLE, popt_options,
                                      GNOME_PARAM_HUMAN_READABLE_NAME, _("Evince"),
				      GNOME_PARAM_APP_DATADIR, GNOMEDATADIR,
                                      NULL);
	g_object_get_property (G_OBJECT (program),
                               GNOME_PARAM_POPT_CONTEXT,
                               g_value_init (&context_as_value, G_TYPE_POINTER));
        context = g_value_get_pointer (&context_as_value);


#ifdef ENABLE_DBUS
	if (!ev_application_register_service (EV_APP)) {
		if (load_files_remote (poptGetArgs (context))) {
			g_warning ("Another process was running.");
			return 0;
		}
	} else {
		g_warning ("Starting evince process.");
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
	gtk_window_set_default_icon_name ("postscript-viewer");

	load_files (poptGetArgs (context));

	gtk_main ();

	gnome_accelerators_sync ();
	poptFreeContext (context);
	ev_file_helpers_shutdown ();

	if (enable_metadata) {
		ev_metadata_manager_shutdown ();
	}

	return 0;
}
