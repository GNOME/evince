/* ev-previewer.c: 
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2009 Carlos Garcia Campos <carlosgc@gnome.org>
 * Copyright © 2012 Christian Persch
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

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <evince-document.h>
#include <evince-view.h>

#include "ev-previewer-window.h"

#ifdef G_OS_WIN32
#include <io.h>
#include <conio.h>
#if !(_WIN32_WINNT >= 0x0500)
#error "_WIN32_WINNT must be defined >= 0x0500"
#endif
#include <windows.h>
#endif

static gboolean unlink_temp_file = FALSE;
static gchar *print_settings = NULL;
static EvPreviewerWindow *window = NULL;

static const GOptionEntry goption_options[] = {
	{ "unlink-tempfile", 'u', 0, G_OPTION_ARG_NONE, &unlink_temp_file, N_("Delete the temporary file"), NULL },
	{ "print-settings", 'p', 0, G_OPTION_ARG_FILENAME, &print_settings, N_("Print settings file"), N_("FILE") },
	{ NULL }
};

static void
ev_previewer_unlink_tempfile (const gchar *filename)
{
        GFile *file, *tempdir;

        file = g_file_new_for_path (filename);
        tempdir = g_file_new_for_path (g_get_tmp_dir ());

        if (g_file_has_prefix (file, tempdir)) {
                g_file_delete (file, NULL, NULL);
        }

        g_object_unref (file);
        g_object_unref (tempdir);
}

static void
ev_previewer_load_job_finished (EvJob           *job,
				EvDocumentModel *model)
{
	if (ev_job_is_failed (job)) {
		g_object_unref (job);
		return;
	}
	ev_document_model_set_document (model, job->document);
	g_object_unref (job);
}

static void
ev_previewer_load_document (GFile           *file,
			    EvDocumentModel *model)
{
	EvJob *job;
	gchar *uri;

	uri = g_file_get_uri (file);

	job = ev_job_load_new (uri);
	g_signal_connect (job, "finished",
			  G_CALLBACK (ev_previewer_load_job_finished),
			  model);
	ev_job_scheduler_push_job (job, EV_JOB_PRIORITY_NONE);
	g_free (uri);
}

static void
activate_cb (GApplication *application,
             gpointer user_data)
{
        if (window) {
                gtk_window_present (GTK_WINDOW (window));
        }
}

static void
open_cb (GApplication *application,
         GFile **files,
         gint n_files,
         const gchar *hint,
         gpointer user_data)
{
        EvDocumentModel *model;
        GFile           *file;
        char            *path;

        if (n_files != 1) {
                g_application_quit (application);
                return;
        }

        file = files[0];

        model = ev_document_model_new ();
        ev_previewer_load_document (file, model);

        window = ev_previewer_window_new (model);
        g_object_unref (model);

        ev_previewer_window_set_print_settings (EV_PREVIEWER_WINDOW (window), print_settings);
        path = g_file_get_path (file);
        ev_previewer_window_set_source_file (EV_PREVIEWER_WINDOW (window), path);
        g_free (path);

        gtk_window_present (GTK_WINDOW (window));
}

gint
main (gint argc, gchar **argv)
{
        GtkApplication  *application;
	GOptionContext  *context;
	GError          *error = NULL;
        int              status = 1;

#ifdef G_OS_WIN32
    if (fileno (stdout) != -1 &&
 	  _get_osfhandle (fileno (stdout)) != -1)
	{
	  /* stdout is fine, presumably redirected to a file or pipe */
	}
    else
    {
	  typedef BOOL (* WINAPI AttachConsole_t) (DWORD);

	  AttachConsole_t p_AttachConsole =
	    (AttachConsole_t) GetProcAddress (GetModuleHandle ("kernel32.dll"), "AttachConsole");

	  if (p_AttachConsole != NULL && p_AttachConsole (ATTACH_PARENT_PROCESS))
      {
	      freopen ("CONOUT$", "w", stdout);
	      dup2 (fileno (stdout), 1);
	      freopen ("CONOUT$", "w", stderr);
	      dup2 (fileno (stderr), 2);

      }
	}
#endif

#ifdef ENABLE_NLS
	/* Initialize the i18n stuff */
	bindtextdomain (GETTEXT_PACKAGE, ev_get_locale_dir());
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	context = g_option_context_new (_("GNOME Document Previewer"));
	g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
	g_option_context_add_main_entries (context, goption_options, GETTEXT_PACKAGE);

	g_option_context_add_group (context, gtk_get_option_group (TRUE));

	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_printerr ("Error parsing command line arguments: %s\n", error->message);
		g_error_free (error);
		g_option_context_free (context);
                return 1;
	}
	g_option_context_free (context);

	if (argc < 2) {
		g_printerr ("File argument is required\n");
                return 1;
	} else if (argc > 2) {
                g_printerr ("Too many files\n");
                return 1;
        }

	if (!g_file_test (argv[1], G_FILE_TEST_IS_REGULAR)) {
		g_printerr ("Filename \"%s\" does not exist or is not a regular file\n", argv[1]);
                return 1;
	}

	if (!ev_init ())
                return 1;

	ev_stock_icons_init ();

	g_set_application_name (_("GNOME Document Previewer"));
	gtk_window_set_default_icon_name ("evince");

        application = gtk_application_new (NULL,
                                           G_APPLICATION_NON_UNIQUE |
                                           G_APPLICATION_HANDLES_OPEN);
        g_signal_connect (application, "activate", G_CALLBACK (activate_cb), NULL);
        g_signal_connect (application, "open", G_CALLBACK (open_cb), NULL);

        status = g_application_run (G_APPLICATION (application), argc, argv);

        if (unlink_temp_file)
                ev_previewer_unlink_tempfile (argv[1]);
        if (print_settings)
                ev_previewer_unlink_tempfile (print_settings);

	ev_shutdown ();
	ev_stock_icons_shutdown ();

	return status;
}
