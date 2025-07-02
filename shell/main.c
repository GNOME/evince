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
#include "ev-metadata.h"

#ifdef G_OS_WIN32
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
static gchar   *ev_named_dest;
static gboolean new_window = FALSE;
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
  g_print ("%s %s\n", "Evince", VERSION);

  exit (0);
  return FALSE;
}

static const GOptionEntry goption_options[] =
{
	{ "new-window", 'o', 0, G_OPTION_ARG_NONE, &new_window, N_("Open a new window."), NULL },
	{ "page-label", 'p', 0, G_OPTION_ARG_STRING, &ev_page_label, N_("The page label of the document to display."), N_("PAGE")},
	{ "page-index", 'i', 0, G_OPTION_ARG_INT, &ev_page_index, N_("The page number of the document to display."), N_("NUMBER")},
	{ "named-dest", 'n', 0, G_OPTION_ARG_STRING, &ev_named_dest, N_("Named destination to display."), N_("DEST")},
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

	if (!error) {
		GAppInfo *app;

		app = g_app_info_create_from_commandline (cmd, NULL, 0, &error);

		if (app != NULL) {
			retval = g_app_info_launch (app, NULL, NULL, &error);
			g_object_unref (app);
		}
	}

	if (error) {
		g_warning ("Error launching previewer: %s\n", error->message);
		g_error_free (error);
	}

	g_free (cmd);

	return retval;
}

static const gchar *
get_fragment_from_filename (const gchar *filename)
{
	GFile   *file;
	gchar   *label;
	gboolean exists;

	label = g_strrstr (filename, "#");
	if (!label)
		return NULL;

	/* Filename contains a #, check
	 * whether it's part of the path
	 * or a label
	 */
	file = g_file_new_for_commandline_arg (filename);
	exists = g_file_query_exists (file, NULL);
	g_object_unref (file);

	if (exists)
		return NULL;

	/* skip past '#' char */
	*label = '\0';
	label++;

	return label;
}

/* Returns TRUE if all characters of @str are digits, FALSE otherwise */
static gboolean
is_number (const gchar *str)
{
	int len = strlen (str);
	if (!len)
		return FALSE;

	for (gint i = 0; i < len; i++) {
		if (!g_ascii_isdigit (str[i]))
			return FALSE;
	}
	return TRUE;
}

/* Searches in @fragment for the passed @key, search is case insensitive.
 *
 *    If found, TRUE is returned and @value (if provided) will contain the
 *              value of key. The caller should free @value with g_free().
 *    If not found, FALSE is returned.
 */
static gboolean
fragment_extract_key (const gchar *fragment,
		      const gchar *key,
		      gchar **value)
{
	g_autoptr(GHashTable) params = NULL;
	g_autoptr(GList) keys = NULL;

	if (!fragment || !key)
		return FALSE;

	/* Evince interprets a fragment of solely a number (like 'doc.pdf#12')
	 * as page number, so we handle it here when asked for the 'page' key */
	if (strcmp (key, "page") == 0) {
		if (is_number (fragment)) {
			if (value)
				*value = g_strdup (fragment);

			return TRUE;
		}
	}

	params = g_uri_parse_params (fragment, -1, "&", G_URI_PARAMS_CASE_INSENSITIVE, NULL);
	if (!params)
		return FALSE;

	keys = g_hash_table_get_keys (params);

	GList* iter = keys;
	while (iter) {
		gchar* cur_key = iter->data;
		if (g_ascii_strcasecmp (key, cur_key) == 0) {
			gpointer tmp = g_hash_table_lookup (params, cur_key);
			if (value && tmp)
				*value = g_strdup (tmp);

			return TRUE;
		}
		iter = g_list_next (iter);
	}

	return FALSE;
}

static void
load_files (const char **files)
{
	GdkDisplay *display = gdk_display_get_default();
	EvWindowRunMode  mode = EV_WINDOW_MODE_NORMAL;
	gint             i;
	EvLinkDest      *global_dest = NULL;

	if (!files || new_window) {
		if (!ev_application_has_window (EV_APP) || new_window)
			ev_application_open_recent_view (EV_APP, display);
		return;
	}

	if (ev_page_label)
		global_dest = ev_link_dest_new_page_label (ev_page_label);
	else if (ev_page_index)
		global_dest = ev_link_dest_new_page (MAX (0, ev_page_index - 1));
	else if (ev_named_dest)
		global_dest = ev_link_dest_new_named (ev_named_dest);

	if (fullscreen_mode)
		mode = EV_WINDOW_MODE_FULLSCREEN;
	else if (presentation_mode)
		mode = EV_WINDOW_MODE_PRESENTATION;

	for (i = 0; files[i]; i++) {
		const gchar *filename;
		gchar       *uri;
		const gchar *fragment;
		GFile       *file;
		EvLinkDest  *dest = NULL;
		const gchar *app_uri;
		g_autoptr(GUri) try_uri = NULL;

		filename = files[i];
		/* We may receive a URI when eg. opening a link to external pdf from inside Evince itself */
		try_uri = g_uri_parse (filename, G_URI_FLAGS_ENCODED | G_URI_FLAGS_NON_DNS, NULL);
		if (try_uri && g_uri_get_fragment (try_uri))
			fragment = g_uri_get_fragment (try_uri);
		else
			fragment = get_fragment_from_filename (filename);

		if (fragment) {
			g_autofree gchar *val = NULL;
			if (fragment_extract_key (fragment, "nameddest", &val))
				dest = ev_link_dest_new_named (val);
			else if (fragment_extract_key (fragment, "page", &val))
				dest = ev_link_dest_new_page_label (val);
		} else if (global_dest)
			dest = g_object_ref (global_dest);

		file = g_file_new_for_commandline_arg (filename);
		uri = g_file_get_uri (file);
		g_object_unref (file);

		app_uri = ev_application_get_uri (EV_APP);
		if (app_uri && strcmp (app_uri, uri) == 0) {
			g_free (uri);
			continue;
		}



		ev_application_open_uri_at_dest (EV_APP, uri, display, dest,
						 mode, ev_find_string,
						 GDK_CURRENT_TIME);

		g_clear_object (&dest);
		g_free (uri);
        }
}

int
main (int argc, char *argv[])
{
        EvApplication  *application;
	GOptionContext *context;
	GError         *error = NULL;
        int             status;

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

	context = g_option_context_new ("Evince");
	g_option_context_set_translation_domain(context, GETTEXT_PACKAGE);
	g_option_context_add_main_entries (context, goption_options, GETTEXT_PACKAGE);

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

	gtk_init ();

	/* Manually set name and icon */
	g_set_application_name ("Evince");
	gtk_window_set_default_icon_name (APPLICATION_ID);

        application = ev_application_new ();
        if (!g_application_register (G_APPLICATION (application), NULL, &error)) {
                g_printerr ("Failed to register: %s\n", error->message);
                g_error_free (error);
                status = 1;
                goto done;
        }

	load_files (file_arguments);

	/* Change directory so we don't prevent unmounting in case the initial cwd
	 * is on an external device (see bug #575436)
	 */
	g_chdir (g_get_home_dir ());

	status = g_application_run (G_APPLICATION (application), 0, NULL);

    done:
	ev_job_scheduler_wait ();
	ev_shutdown ();

        g_object_unref (application);
	return status;
}
