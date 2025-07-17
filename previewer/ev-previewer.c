/* ev-previewer.c:
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2009 Carlos Garcia Campos <carlosgc@gnome.org>
 * Copyright © 2012, 2018, 2021, 2022 Christian Persch
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <adwaita.h>
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
static int input_fd = -1;
static char *input_file = NULL;
static char *input_mime_type = NULL;
static int print_settings_fd = -1;
static gchar *print_settings_file = NULL;
static EvPreviewerWindow *window = NULL;

static const GOptionEntry goption_options[] = {
	{ "unlink-tempfile", 'u', 0, G_OPTION_ARG_NONE, &unlink_temp_file,
          N_("Delete the temporary file"), NULL },
	{ "print-settings", 'p', 0, G_OPTION_ARG_FILENAME, &print_settings_file,
          N_("File specifying print settings"), N_("FILE") },
	{ "fd", 0, 0, G_OPTION_ARG_INT, &input_fd,
          N_("File descriptor of input file"), N_("FD") },
        { "mime-type", 0, 0, G_OPTION_ARG_STRING, &input_mime_type,
          N_("MIME type of input file"), N_("TYPE") },
	{ "print-settings-fd", 0, 0, G_OPTION_ARG_INT, &print_settings_fd,
          N_("File descriptor of print settings file"), N_("FD") },
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
activate_cb (GApplication *application,
             gpointer user_data)
{
        if (window) {
                gtk_window_present (GTK_WINDOW (window));
        }
}

static void
startup_cb (GApplication *application,
            gpointer      data)
{
	EvJob *job;
        GError *error = NULL;
        gboolean ps_ok = TRUE;

        g_assert (input_fd != -1 || input_file != NULL);

        window = ev_previewer_window_new ();

        if (print_settings_fd != -1) {
                ps_ok = ev_previewer_window_set_print_settings_fd (EV_PREVIEWER_WINDOW (window), print_settings_fd, &error);
                print_settings_fd = -1;
        } else if (print_settings_file != NULL) {
                ps_ok = ev_previewer_window_set_print_settings (EV_PREVIEWER_WINDOW (window), print_settings_file, &error);
		g_clear_pointer (&print_settings_file, g_free);
        }
        if (!ps_ok) {
                g_printerr ("Failed to load print settings: %s\n", error->message);
                g_clear_error (&error);
        }

        if (input_fd != -1) {
                if (!ev_previewer_window_set_source_fd (EV_PREVIEWER_WINDOW (window), input_fd, &error)) {
                        g_printerr ("Failed to set source FD: %s\n", error->message);
                        g_clear_error (&error);

                        g_application_quit (application);
                        return;
                }

                job = ev_job_load_fd_new_take (input_fd, input_mime_type,
                                               EV_DOCUMENT_LOAD_FLAG_NO_CACHE);

                input_fd = -1;
        } else {
                GFile *file;
                char *uri;
                char *path;

                file = g_file_new_for_commandline_arg (input_file);
                uri = g_file_get_uri (file);
                path = g_file_get_path (file);

                ev_previewer_window_set_source_file (EV_PREVIEWER_WINDOW (window), path);
                job = ev_job_load_new (uri);

                g_free (uri);
                g_free (path);
                g_object_unref (file);

		g_clear_pointer (&input_file, g_free);
        }

        ev_previewer_window_set_job (window, job);
        g_object_unref (job);

        /* Window will be presented by 'activate' signal */
}

static gboolean
check_arguments (int argc,
                 char** argv,
                 GError **error)
{
        if (input_fd != -1) {
                struct stat statbuf;
                int flags;

                if (fstat (input_fd, &statbuf) == -1 ||
                    (flags = fcntl (input_fd, F_GETFL, &flags)) == -1) {
                        int errsv = errno;
                        g_set_error_literal (error, G_FILE_ERROR,
                                             g_file_error_from_errno(errsv),
                                             g_strerror(errsv));
                        return FALSE;
                }

                if (!S_ISREG (statbuf.st_mode)) {
                        g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_BADF,
                                             "Not a regular file.");
                        return FALSE;
                }

                switch (flags & O_ACCMODE) {
                case O_RDONLY:
                case O_RDWR:
                        break;
                case O_WRONLY:
                default:
                        g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_BADF,
                                            "Not a readable file descriptor.");
                        return FALSE;
                }

                if (argc > 1) {
                        g_set_error_literal (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                             "Too many arguments");
                        return FALSE;
                }
                if (input_mime_type == NULL) {
                        input_mime_type = ev_file_get_mime_type_from_fd (input_fd, error);
                        if (input_mime_type == NULL) {
                                g_prefix_error (error, "Must specify --mime-type: ");
                                return FALSE;
                        }
                }
                if (unlink_temp_file) {
                        g_set_error_literal (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                             "Must not specify --unlink-tempfile");
                        return FALSE;
                }

	} else {
                char *path;

                if (argc != 2) {
                        g_set_error_literal (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                             "Need exactly one argument");
                        return FALSE;
                }
                if (input_mime_type != NULL) {
                        g_set_error_literal (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                                             "Must not specify --mime-type");
                        return FALSE;
                }

                path = g_filename_from_uri (argv[1], NULL, NULL);
                if (!g_file_test (argv[1], G_FILE_TEST_IS_REGULAR) && !g_file_test (path, G_FILE_TEST_IS_REGULAR)) {
                        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOENT,
                                     "File \"%s\" does not exist or is not a regular file\n", argv[1]);
                        g_free (path);
                        return FALSE;
                }
                g_free (path);

                input_file = g_strdup (argv[1]);
        }

        return TRUE;
}

gint
main (gint argc, gchar **argv)
{
        AdwApplication  *application;
	GOptionContext  *context;
	GError          *error = NULL;
        int              status = 1;

	const gchar *action_accels[] = {
		"win.select-page",      "<Ctrl>L", NULL,
		"win.go-previous-page", "p", "<Ctrl>Page_Up", NULL,
		"win.go-next-page",     "n", "<Ctrl>Page_Down", NULL,
		"win.print",            "<Ctrl>P", NULL,
		"win.zoom-in",          "plus", "<Ctrl>plus", "KP_Add", "<Ctrl>KP_Add", "equal", "<Ctrl>equal", NULL,
		"win.zoom-out",         "minus", "<Ctrl>minus", "KP_Subtract", "<Ctrl>KP_Subtract", NULL,
		"win.zoom-default",     "a", NULL,
		NULL,
	};
        const char **it;

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

	/* Initialize the i18n stuff */
	bindtextdomain (GETTEXT_PACKAGE, ev_get_locale_dir());
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

        g_set_prgname ("evince-previewer");

	context = g_option_context_new (_("GNOME Document Previewer"));
	g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
	g_option_context_add_main_entries (context, goption_options, GETTEXT_PACKAGE);

	if (!g_option_context_parse (context, &argc, &argv, &error) ||
            !check_arguments (argc, argv, &error)) {
		g_printerr ("Error parsing command line arguments: %s\n", error->message);
		g_error_free (error);
		g_option_context_free (context);
                return 1;
	}
	g_option_context_free (context);

	if (!ev_init ())
                return 1;

	g_set_application_name (_("GNOME Document Previewer"));
	gtk_window_set_default_icon_name (APPLICATION_ID);

        application = adw_application_new (APPLICATION_ID "-previewer", G_APPLICATION_NON_UNIQUE);
	g_application_set_resource_base_path (G_APPLICATION (application),
			"/org/gnome/evince/previewer");
        g_signal_connect (application, "startup", G_CALLBACK (startup_cb), NULL);
        g_signal_connect (application, "activate", G_CALLBACK (activate_cb), NULL);

        for (it = action_accels; it[0]; it += g_strv_length ((gchar **)it) + 1)
                gtk_application_set_accels_for_action (GTK_APPLICATION (application), it[0], &it[1]);

        status = g_application_run (G_APPLICATION (application), 0, NULL);

        if (unlink_temp_file)
                ev_previewer_unlink_tempfile (argv[1]);
        if (print_settings_file)
                ev_previewer_unlink_tempfile (print_settings_file);

	ev_job_scheduler_wait ();
	ev_shutdown ();

	return status;
}
