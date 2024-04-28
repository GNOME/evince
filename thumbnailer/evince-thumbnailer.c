/*
   Copyright (C) 2005 Fernando Herrera <fherrera@onirica.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#include <config.h>

#include <evince-document.h>

#include <gio/gio.h>

#include <locale.h>
#include <stdlib.h>
#include <string.h>

#ifdef G_OS_WIN32
#include <io.h>
#include <conio.h>
#if !(_WIN32_WINNT >= 0x0500)
#error "_WIN32_WINNT must be defined >= 0x0500"
#endif
#include <windows.h>
#endif

#define THUMBNAIL_SIZE 128
#define DEFAULT_SLEEP_TIME (15 * G_USEC_PER_SEC) /* 15 seconds */

static gboolean finished = TRUE;

static gint size = THUMBNAIL_SIZE;
static gboolean time_limit = TRUE;
static const gchar **file_arguments;

static const GOptionEntry goption_options[] = {
	{ "size", 's', 0, G_OPTION_ARG_INT, &size, NULL, "SIZE" },
        { "no-limit", 'l', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &time_limit, "Don't limit the thumbnailing time to 15 seconds", NULL },
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &file_arguments, NULL, "<input> <output>" },
	{ NULL }
};

/* Time monitor: copied from totem */
G_GNUC_NORETURN static gpointer
time_monitor (gpointer data)
{
        const gchar *app_name;

        g_usleep (DEFAULT_SLEEP_TIME);

        if (finished)
                g_thread_exit (NULL);

        app_name = g_get_application_name ();
        if (app_name == NULL)
                app_name = g_get_prgname ();
        g_printerr ("%s couldn't process file: '%s'\n"
                    "Reason: Took too much time to process.\n",
                    app_name,
                    (const char *) data);

        exit (0);
}

static void
time_monitor_start (const char *input)
{
        finished = FALSE;

        g_thread_new ("ThumbnailerTimer", time_monitor, (gpointer) input);
}

static void
time_monitor_stop (void)
{
        finished = TRUE;
}

static void
delete_temp_file (gpointer data, GObject *object)
{
	GFile *file = G_FILE (data);

	ev_tmp_file_unlink (file);
	g_object_unref (file);
}

static char *
get_target_uri (GFile *file)
{
	GFileInfo *info;
	char *target;

	info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI, G_FILE_QUERY_INFO_NONE, NULL, NULL);
	if (info == NULL)
		return NULL;
	target = g_strdup (g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI));
	g_object_unref (info);

	return target;
}

static char *
get_local_path (GFile *file)
{
	if (g_file_has_uri_scheme (file, "trash") != FALSE ||
	    g_file_has_uri_scheme (file, "recent") != FALSE) {
		return get_target_uri (file);
	}
	return g_file_get_path (file);
}

static EvDocument *
evince_thumbnailer_get_document (GFile *file)
{
	EvDocument *document = NULL;
	gchar      *uri, *path;
	GFile      *tmp_file = NULL;
	GError     *error = NULL;

	path = get_local_path (file);

	if (!path) {
		gchar *base_name, *template;

		base_name = g_file_get_basename (file);
		template = g_strdup_printf ("document.XXXXXX-%s", base_name);
		g_free (base_name);

		tmp_file = ev_mkstemp_file (template, &error);
		g_free (template);
		if (!tmp_file) {
			g_printerr ("Error loading remote document: %s\n", error->message);
			g_error_free (error);

			return NULL;
		}

		g_file_copy (file, tmp_file, G_FILE_COPY_OVERWRITE,
			     NULL, NULL, NULL, &error);
		if (error) {
			g_printerr ("Error loading remote document: %s\n", error->message);
			g_error_free (error);
			g_object_unref (tmp_file);

			return NULL;
		}
		uri = g_file_get_uri (tmp_file);
	} else {
		uri = g_filename_to_uri (path, NULL, NULL);
		g_free (path);
	}

	document = ev_document_factory_get_document_full (uri, EV_DOCUMENT_LOAD_FLAG_NO_CACHE, &error);
	if (tmp_file) {
		if (document) {
			g_object_weak_ref (G_OBJECT (document),
					   delete_temp_file,
					   tmp_file);
		} else {
			ev_tmp_file_unlink (tmp_file);
			g_object_unref (tmp_file);
		}
	}
	g_free (uri);
	if (error) {
		if (error->domain == EV_DOCUMENT_ERROR &&
		    error->code == EV_DOCUMENT_ERROR_ENCRYPTED) {
			/* FIXME: Create a thumb for cryp docs */
			g_error_free (error);
			return NULL;
		}
		g_printerr ("Error loading document: %s\n", error->message);
		g_error_free (error);
		return NULL;
	}

	return document;
}

static gboolean
evince_thumbnail_pngenc_get (EvDocument *document, const char *thumbnail, int size)
{
	EvRenderContext *rc;
	double width, height;
	GdkPixbuf *pixbuf;
	EvPage *page;

	page = ev_document_get_page (document, 0);

	ev_document_get_page_size (document, 0, &width, &height);

	rc = ev_render_context_new (page, 0, size / MAX (height, width));
	pixbuf = ev_document_get_thumbnail (document, rc);
	g_object_unref (rc);
	g_object_unref (page);

	if (pixbuf != NULL) {
		if (gdk_pixbuf_save (pixbuf, thumbnail, "png", NULL, NULL)) {
			g_object_unref  (pixbuf);
			return TRUE;
		}

		g_object_unref  (pixbuf);
	}

	return FALSE;
}

static void
print_usage (GOptionContext *context)
{
	gchar *help;

	help = g_option_context_get_help (context, TRUE, NULL);
	g_print ("%s", help);
	g_free (help);
}

int
main (int argc, char *argv[])
{
	EvDocument     *document;
	GOptionContext *context;
	const char     *input;
	const char     *output;
	GFile          *file;
	GError         *error = NULL;

	setlocale (LC_ALL, "");

	context = g_option_context_new ("- GNOME Document Thumbnailer");
	g_option_context_add_main_entries (context, goption_options, NULL);

	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_printerr ("%s\n", error->message);
		g_error_free (error);
		print_usage (context);
		g_option_context_free (context);

		return -1;
	}

	input = file_arguments ? file_arguments[0] : NULL;
	output = input ? file_arguments[1] : NULL;
	if (!input || !output) {
		print_usage (context);
		g_option_context_free (context);

		return -1;
	}

	g_option_context_free (context);

	if (size < 1) {
		g_printerr ("Size cannot be smaller than 1 pixel\n");
		return -1;
	}

	input = file_arguments[0];
	output = file_arguments[1];

        if (!ev_init ())
                return -1;

	file = g_file_new_for_commandline_arg (input);
	document = evince_thumbnailer_get_document (file);
	g_object_unref (file);

	if (!document) {
		ev_shutdown ();
		return -2;
	}

        if (time_limit)
                time_monitor_start (input);

	if (!evince_thumbnail_pngenc_get (document, output, size)) {
		g_object_unref (document);
		ev_shutdown ();
		return -2;
	}

        time_monitor_stop ();
	g_object_unref (document);
        ev_shutdown ();

	return 0;
}
