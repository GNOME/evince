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

#include <ev-poppler.h>

#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-init.h>

#include <ev-document.h>
#include <ev-document-thumbnails.h>

#include <string.h>

#define THUMBNAIL_SIZE 128

static gboolean
evince_thumbnail_pngenc_get (const char *uri, const char *thumbnail, int size)
{
	EvDocument *document = NULL;
	char *mime_type;
	GError *error;
	GdkPixbuf *pixbuf;

	mime_type = gnome_vfs_get_mime_type (uri);
	if (mime_type == NULL)
		return FALSE;

	if (!strcmp (mime_type, "application/pdf"))
		document = g_object_new (PDF_TYPE_DOCUMENT, NULL);
	else
		return FALSE;

	if (!ev_document_load (document, uri, &error)) {
		if (error->domain == EV_DOCUMENT_ERROR &&
            	    error->code == EV_DOCUMENT_ERROR_ENCRYPTED) {
			/* FIXME: Create a thumb for cryp docs */
		}
		g_error_free (error);
		return FALSE;
	}

	pixbuf = ev_document_thumbnails_get_thumbnail
			(EV_DOCUMENT_THUMBNAILS (document), 1, size, FALSE);
	
	if (pixbuf != NULL) {
		GdkPixbuf *pdflogo;

		pdflogo = gdk_pixbuf_new_from_file (DATADIR"/pdf-icon.png", NULL);
		if (pdflogo != NULL) {
			int delta_height, delta_width;

			delta_width = gdk_pixbuf_get_width (pixbuf) -
			              gdk_pixbuf_get_width (pdflogo);
			delta_height = gdk_pixbuf_get_height (pixbuf) -
			               gdk_pixbuf_get_height (pdflogo);

			gdk_pixbuf_composite (pdflogo, pixbuf,
					      delta_width, delta_height,
					      gdk_pixbuf_get_width (pdflogo),
					      gdk_pixbuf_get_height (pdflogo),
					      delta_width, delta_height,
					      1, 1,
					      GDK_INTERP_NEAREST, 100);

			gdk_pixbuf_unref  (pdflogo);
		}
		if (gdk_pixbuf_save (pixbuf, thumbnail, "png", NULL, NULL)) {
			gdk_pixbuf_unref  (pixbuf);
			g_object_unref (document);
			return TRUE;
		} else {
			gdk_pixbuf_unref  (pixbuf);
			g_object_unref (document);
		}
	}
	return FALSE;
}

int
main (int argc, char *argv[])
{
	int res;
	char *input, *output;
	int size;
	char *uri;

	if (argc <= 2 || argc > 5 || strcmp (argv[1], "-h") == 0 ||
	    strcmp (argv[1], "--help") == 0) {
		g_print ("Usage: %s [-s <size>] <input> <output>\n", argv[0]);
		return -1;
	}

	res = gnome_vfs_init ();

	if (!strcmp (argv[1], "-s")) {
		input = argv[3];
		output = argv[4];
		size = g_strtod (argv[2], NULL);
	} else {
		input = argv[1];
		output = argv[2];
		size = THUMBNAIL_SIZE;
	}

	if (size < 40) {
		g_print ("Size cannot be smaller than 40 pixels\n");
		return -1;
	}

	uri = gnome_vfs_make_uri_from_shell_arg (input);

	if (evince_thumbnail_pngenc_get (uri, output, size)) {
		g_free (uri);
		return 0;
	} else {
		g_free (uri);
		return -2;
	}
}
