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

#include <glib/gi18n.h>
#include <gtk/gtkmain.h>
#include <libgnome/gnome-program.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomeui/gnome-app-helper.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include "ev-stock-icons.h"
#include "ev-debug.h"
#include "ev-job-queue.h"

static struct poptOption popt_options[] =
{
	{ NULL, 0, 0, NULL, 0, NULL, NULL }
};

static void
load_files (const char **files)
{
	GtkWidget *window;
	int i;

	if (!files) {
		window = GTK_WIDGET (ev_application_new_window (EV_APP));
		gtk_widget_show (window);
		return;
	}

	for (i = 0; files[i]; i++) {
		char *uri;

		uri = gnome_vfs_make_uri_from_shell_arg (files[i]);		

		window = GTK_WIDGET (ev_application_new_window (EV_APP));
		gtk_widget_show (window);
		ev_window_open (EV_WINDOW (window), uri);

		g_free (uri);
        }
}

int
main (int argc, char *argv[])
{
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

	ev_job_queue_init ();
	g_set_application_name (_("Evince Document Viewer"));

	ev_debug_init ();
	ev_stock_icons_init ();
	gtk_window_set_default_icon_name ("postscript-viewer");

	g_object_get_property (G_OBJECT (program),
                               GNOME_PARAM_POPT_CONTEXT,
                               g_value_init (&context_as_value, G_TYPE_POINTER));
        context = g_value_get_pointer (&context_as_value);

	load_files (poptGetArgs (context));

	gtk_main ();

	gnome_accelerators_sync ();
	poptFreeContext (context);

	return 0;
}
