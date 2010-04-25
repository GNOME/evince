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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "ev-application.h"
#include "ev-debug.h"
#include "ev-init.h"
#include "ev-file-helpers.h"
#include "ev-stock-icons.h"
#include "ev-metadata.h"

#ifdef WITH_SMCLIENT
#include "eggsmclient.h"
#ifdef GDK_WINDOWING_X11
#include "eggdesktopfile.h"
#endif
#endif /* WITH_SMCLIENT */

#ifdef G_OS_WIN32
#ifdef DATADIR
#undef DATADIR
#endif
#include <io.h>
#include <conio.h>
#if !(_WIN32_WINNT >= 0x0500)
#error "_WIN32_WINNT must be defined >= 0x0500"
#endif
#include <windows.h>
#endif

static gchar   *ev_page_label;
static gchar   *ev_find_string;
static gint     ev_page_index = 0;
static gboolean preview_mode = FALSE;
static gboolean fullscreen_mode = FALSE;
static gboolean presentation_mode = FALSE;
static gboolean unlink_temp_file = FALSE;
static gchar   *print_settings;
static const char **file_arguments = NULL;


static gboolean
option_version_cb (const gchar *option_name,
                   const gchar *value,
                   gpointer     data,
                   GError     **error)
{
  g_print ("%s %s\n", _("GNOME Document Viewer"), VERSION);

  exit (0);
  return FALSE;
}

static const GOptionEntry goption_options[] =
{
	{ "page-label", 'p', 0, G_OPTION_ARG_STRING, &ev_page_label, N_("The page label of the document to display."), N_("PAGE")},
	{ "page-index", 'i', 0, G_OPTION_ARG_INT, &ev_page_index, N_("The page number of the document to display."), N_("NUMBER")},
	{ "fullscreen", 'f', 0, G_OPTION_ARG_NONE, &fullscreen_mode, N_("Run evince in fullscreen mode"), NULL },
	{ "presentation", 's', 0, G_OPTION_ARG_NONE, &presentation_mode, N_("Run evince in presentation mode"), NULL },
	{ "preview", 'w', 0, G_OPTION_ARG_NONE, &preview_mode, N_("Run evince as a previewer"), NULL },
	{ "find", 'l', 0, G_OPTION_ARG_STRING, &ev_find_string, N_("The word or phrase to find in the document"), N_("STRING")},
	{ "unlink-tempfile", 'u', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &unlink_temp_file, NULL, NULL },
	{ "print-settings", 't', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_FILENAME, &print_settings, NULL, NULL },
	{ "version", 0, G_OPTION_FLAG_NO_ARG | G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_CALLBACK, option_version_cb, NULL, NULL },
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &file_arguments, NULL, N_("[FILE…]") },
	{ NULL }
};

static gboolean
launch_previewer (void)
{
	GString *cmd_str;
	gchar   *cmd;
	gint     argc;
	gchar  **argv;
	gboolean retval = FALSE;
	GError  *error = NULL;

	/* Rebuild the command line, ignoring options
	 * not supported by the previewer and taking only
	 * the first path given
	 */
	cmd_str = g_string_new ("evince-previewer");
		
	if (print_settings) {
		gchar *quoted;

		quoted = g_shell_quote (print_settings);
		g_string_append_printf (cmd_str, " --print-settings %s", quoted);
		g_free (quoted);
	}

	if (unlink_temp_file)
		g_string_append (cmd_str, " --unlink-tempfile");

	if (file_arguments) {
		gchar *quoted;
		
		quoted = g_shell_quote (file_arguments[0]);
		g_string_append_printf (cmd_str, " %s", quoted);
		g_free (quoted);
	}

	cmd = g_string_free (cmd_str, FALSE);
	g_shell_parse_argv (cmd, &argc, &argv, &error);
	g_free (cmd);
	
	if (!error) {
		retval = gdk_spawn_on_screen (gdk_screen_get_default (),
					      NULL, argv, NULL,
					      G_SPAWN_SEARCH_PATH,
					      NULL, NULL, NULL,
					      &error);
		g_strfreev (argv);
	}

	if (error) {
		g_warning ("Error launching previewer: %s\n", error->message);
		g_error_free (error);
	}

	return retval;
}

static void
load_files (const char **files)
{
	GdkScreen       *screen = gdk_screen_get_default ();
	EvWindowRunMode  mode = EV_WINDOW_MODE_NORMAL;
	gint             i;
	EvLinkDest      *global_dest = NULL;

	if (!files) {
		if (!ev_application_has_window (EV_APP))
			ev_application_open_window (EV_APP, screen, GDK_CURRENT_TIME);
		return;
	}

	if (ev_page_label)
		global_dest = ev_link_dest_new_page_label (ev_page_label);
	else if (ev_page_index)
		global_dest = ev_link_dest_new_page (MAX (0, ev_page_index - 1));

	if (fullscreen_mode)
		mode = EV_WINDOW_MODE_FULLSCREEN;
	else if (presentation_mode)
		mode = EV_WINDOW_MODE_PRESENTATION;

	for (i = 0; files[i]; i++) {
		const gchar *filename;
		gchar       *uri;
		gchar       *label;
		GFile       *file;
		EvLinkDest  *dest = NULL;
		const gchar *app_uri;

		filename = files[i];
		label = strchr (filename, '#');
		if (label) {
			*label = 0;
			label++;
			dest = ev_link_dest_new_page_label (label);
		} else if (global_dest) {
			dest = g_object_ref (global_dest);
		}

		file = g_file_new_for_commandline_arg (filename);
		uri = g_file_get_uri (file);
		g_object_unref (file);

		app_uri = ev_application_get_uri (EV_APP);
		if (app_uri && strcmp (app_uri, uri) == 0) {
			g_free (uri);
			continue;
		}



		ev_application_open_uri_at_dest (EV_APP, uri, screen, dest,
						 mode, ev_find_string,
						 GDK_CURRENT_TIME);

		if (dest)
			g_object_unref (dest);
		g_free (uri);
        }
}

int
main (int argc, char *argv[])
{
	GOptionContext *context;
	GError         *error = NULL;

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

	/* Init glib threads asap */
	if (!g_thread_supported ())
		g_thread_init (NULL);

#ifdef ENABLE_NLS
	/* Initialize the i18n stuff */
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	context = g_option_context_new (N_("GNOME Document Viewer"));
	g_option_context_set_translation_domain(context, GETTEXT_PACKAGE);
	g_option_context_add_main_entries (context, goption_options, GETTEXT_PACKAGE);

#ifdef WITH_SMCLIENT
	g_option_context_add_group (context, egg_sm_client_get_option_group ());
#endif

	g_option_context_add_group (context, gtk_get_option_group (TRUE));

	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_printerr ("Cannot parse arguments: %s\n", error->message);
		g_error_free (error);
		g_option_context_free (context);

		return 1;
	}
	g_option_context_free (context);

	if (preview_mode) {
		gboolean retval;
		
		retval = launch_previewer ();
		
		return retval ? 0 : 1;
	}

        if (!ev_init ())
                return 1;

	ev_stock_icons_init ();

#if defined(WITH_SMCLIENT) && defined(GDK_WINDOWING_X11)
	egg_set_desktop_file (GNOMEDATADIR "/applications/evince.desktop");
#else
	/* Manually set name and icon */
	g_set_application_name (_("Document Viewer"));
	gtk_window_set_default_icon_name ("evince");
#endif /* WITH_SMCLIENT && GDK_WINDOWING_X11 */

	ev_application_load_session (EV_APP);
	load_files (file_arguments);
	if (ev_application_has_window (EV_APP)) {
		/* Change directory so we don't prevent unmounting in case the initial cwd
		 * is on an external device (see bug #575436)
		 */
		g_chdir (g_get_home_dir ());

		gtk_main ();
	}

	ev_shutdown ();
	ev_stock_icons_shutdown ();

	return 0;
}
