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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-init.h>
#include <libgnomevfs/gnome-vfs-ops.h>

#include <ev-document.h>
#include <ev-document-thumbnails.h>
#include <ev-async-renderer.h>
#include <ev-document-factory.h>
#include <ev-backends-manager.h>

#include <stdlib.h>
#include <string.h>

#define THUMBNAIL_SIZE 128

struct AsyncData {
	EvDocument  *document;
	const gchar *output;
	gint         size;
	gboolean     success;
};

static EvDocument *
evince_thumbnailer_get_document (const gchar *uri)
{
	EvDocument *document = NULL;
	GError     *error = NULL;

	document = ev_document_factory_get_document  (uri, &error);
	if (error) {
		if (error->domain == EV_DOCUMENT_ERROR &&
		    error->code == EV_DOCUMENT_ERROR_ENCRYPTED) {
			/* FIXME: Create a thumb for cryp docs */
			g_error_free (error);
			return NULL;
		}
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

	ev_document_get_page_size (document, 0, &width, &height);

	rc = ev_render_context_new (0, 0, size / width);
	pixbuf = ev_document_thumbnails_get_thumbnail (EV_DOCUMENT_THUMBNAILS (document),
						       rc, FALSE);
	g_object_unref (rc);
	
	if (pixbuf != NULL) {
		const char *overlaid_icon_name = NULL;

		if (overlaid_icon_name) {
			GdkPixbuf *overlaid_pixbuf;

			gchar *overlaid_icon_path = g_strdup_printf ("%s/%s", DATADIR, overlaid_icon_name);
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

int
main (int argc, char *argv[])
{
	EvDocument *document;
	const char *input;
	const char *output;
	int         size;
	char       *uri;

	if (argc <= 2 || argc > 5 || strcmp (argv[1], "-h") == 0 ||
	    strcmp (argv[1], "--help") == 0) {
		g_print ("Usage: %s [-s <size>] <input> <output>\n", argv[0]);
		return -1;
	}

	if (!strcmp (argv[1], "-s")) {
		input = argv[3];
		output = argv[4];
		size = atoi (argv[2]);
	} else {
		input = argv[1];
		output = argv[2];
		size = THUMBNAIL_SIZE;
	}

	if (size < 40) {
		g_print ("Size cannot be smaller than 40 pixels\n");
		return -1;
	}

	if (!g_thread_supported ())
		g_thread_init (NULL);
	
	gnome_vfs_init ();

	ev_backends_manager_init ();

	uri = gnome_vfs_make_uri_from_shell_arg (input);
	document = evince_thumbnailer_get_document (uri);
	g_free (uri);

	if (!document) {
		ev_backends_manager_shutdown ();
		return -2;
	}

	if (!EV_IS_DOCUMENT_THUMBNAILS (document)) {
		g_object_unref (document);
		ev_backends_manager_shutdown ();
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
		ev_backends_manager_shutdown ();

		return data.success ? 0 : -2;
	}

	if (!evince_thumbnail_pngenc_get (document, output, size)) {
		g_object_unref (document);
		ev_backends_manager_shutdown ();
		return -2;
	}

	g_object_unref (document);
	ev_backends_manager_shutdown ();

	return 0;
}
