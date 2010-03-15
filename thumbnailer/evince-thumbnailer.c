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

#include <stdlib.h>
#include <string.h>

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

#define THUMBNAIL_SIZE 128

static gint size = THUMBNAIL_SIZE;
static const gchar **file_arguments;

static const GOptionEntry goption_options[] = {
	{ "size", 's', 0, G_OPTION_ARG_INT, &size, NULL, "SIZE" },
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &file_arguments, NULL, "<input> <ouput>" },
	{ NULL }
};

struct AsyncData {
	EvDocument  *document;
	const gchar *output;
	gint         size;
	gboolean     success;
};

static void
delete_temp_file (GFile *file)
{
	ev_tmp_file_unlink (file);
	g_object_unref (file);
}

static EvDocument *
evince_thumbnailer_get_document (GFile *file)
{
	EvDocument *document = NULL;
	gchar      *uri;
	GFile      *tmp_file = NULL;
	GError     *error = NULL;

	if (!g_file_is_native (file)) {
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
		uri = g_file_get_uri (file);
	}

	document = ev_document_factory_get_document (uri, &error);
	if (tmp_file) {
		if (document) {
			g_object_weak_ref (G_OBJECT (document),
					   (GWeakNotify)delete_temp_file,
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

	rc = ev_render_context_new (page, 0, size / width);
	pixbuf = ev_document_thumbnails_get_thumbnail (EV_DOCUMENT_THUMBNAILS (document),
						       rc, FALSE);
	g_object_unref (rc);
	g_object_unref (page);
	
	if (pixbuf != NULL) {
		const char *overlaid_icon_name = NULL;

		if (overlaid_icon_name) {
			GdkPixbuf *overlaid_pixbuf;

#ifdef G_OS_WIN32
			gchar *dir = g_win32_get_package_installation_directory_of_module (NULL);
			gchar *overlaid_icon_path = g_build_filename (dir, "share", "evince", overlaid_icon_name, NULL);
			g_free (dir);
#else
			gchar *overlaid_icon_path = g_strdup_printf ("%s/%s", DATADIR, overlaid_icon_name);
#endif
			overlaid_pixbuf = gdk_pixbuf_new_from_file (overlaid_icon_path, NULL);
			g_free (overlaid_icon_path);
			if (overlaid_pixbuf != NULL) {
				int delta_height, delta_width;
				
				delta_width = gdk_pixbuf_get_width (pixbuf) -
					gdk_pixbuf_get_width (overlaid_pixbuf);
				delta_height = gdk_pixbuf_get_height (pixbuf) -
					gdk_pixbuf_get_height (overlaid_pixbuf);
				
				gdk_pixbuf_composite (overlaid_pixbuf, pixbuf,
						      delta_width, delta_height,
						      gdk_pixbuf_get_width (overlaid_pixbuf),
						      gdk_pixbuf_get_height (overlaid_pixbuf),
						      delta_width, delta_height,
						      1, 1,
						      GDK_INTERP_NEAREST, 100);
				
				g_object_unref  (overlaid_pixbuf);
			}
		}
		
		if (gdk_pixbuf_save (pixbuf, thumbnail, "png", NULL, NULL)) {
			g_object_unref  (pixbuf);
			return TRUE;
		}

		g_object_unref  (pixbuf);
	}
	
	return FALSE;
}

static gpointer
evince_thumbnail_pngenc_get_async (struct AsyncData *data)
{
	ev_document_doc_mutex_lock ();
	data->success = evince_thumbnail_pngenc_get (data->document,
						     data->output,
						     data->size);
	ev_document_doc_mutex_unlock ();
	
	g_idle_add ((GSourceFunc)gtk_main_quit, NULL);
	
	return NULL;
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
		g_print ("Size cannot be smaller than 1 pixel\n");
		return -1;
	}

	input = file_arguments[0];
	output = file_arguments[1];

	g_type_init ();

	if (!g_thread_supported ())
		g_thread_init (NULL);

        if (!ev_init ())
                return -1;

	file = g_file_new_for_commandline_arg (input);
	document = evince_thumbnailer_get_document (file);
	g_object_unref (file);

	if (!document) {
		ev_shutdown ();
		return -2;
	}

	if (!EV_IS_DOCUMENT_THUMBNAILS (document)) {
		g_object_unref (document);
		ev_shutdown ();
		return -2;
	}

	if (EV_IS_ASYNC_RENDERER (document)) {
		struct AsyncData data;

		gtk_init (&argc, &argv);
		
		data.document = document;
		data.output = output;
		data.size = size;

		g_thread_create ((GThreadFunc) evince_thumbnail_pngenc_get_async,
				 &data, FALSE, NULL);
		
		gtk_main ();

		g_object_unref (document);
		ev_shutdown ();

		return data.success ? 0 : -2;
	}

	if (!evince_thumbnail_pngenc_get (document, output, size)) {
		g_object_unref (document);
		ev_shutdown ();
		return -2;
	}

	g_object_unref (document);
        ev_shutdown ();

	return 0;
}
