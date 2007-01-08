/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/*
 * Copyright (C) 2005, Teemu Tervo <teemu.tervo@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <unistd.h>
#include <string.h>
#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>

#include "comics-document.h"
#include "ev-document-misc.h"
#include "ev-document-thumbnails.h"

struct _ComicsDocumentClass
{
	GObjectClass parent_class;
};

struct _ComicsDocument
{
	GObject parent_instance;

	gchar  *archive;
	GSList *page_names;
	int     n_pages;
	char   *extract_command;
	gboolean regex_arg;
};

typedef struct _ComicsDocumentClass ComicsDocumentClass;

static void       comics_document_document_iface_init (EvDocumentIface *iface);
static void       comics_document_document_thumbnails_iface_init (EvDocumentThumbnailsIface *iface);

static GSList*    get_supported_image_extensions (void);
static void       get_page_size_area_prepared_cb (GdkPixbufLoader *loader,
						  gpointer data);
static void       render_pixbuf_size_prepared_cb (GdkPixbufLoader *loader,
						  gint width,
						  gint height,
						  gpointer data);
static char**     extract_argv                   (EvDocument *document,
						  gint page);


G_DEFINE_TYPE_WITH_CODE (
	ComicsDocument, comics_document, G_TYPE_OBJECT,
	{
		G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT,
				       comics_document_document_iface_init);
		G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_THUMBNAILS,
				       comics_document_document_thumbnails_iface_init);
	} );

static char *
comics_regex_quote (const char *s)
{
    char *ret, *d;

    d = ret = g_malloc (strlen (s) * 2 + 3);
    
    *d++ = '\'';

    for (; *s; s++, d++) {
	switch (*s) {
	case '?':
	case '|':
	case '[':
	case ']':
	case '*':
	case '\\':
	case '\'':
	    *d++ = '\\';
	    break;
	}
	*d = *s;
    }
    
    *d++ = '\'';
    *d = '\0';

    return ret;
}

static gboolean
comics_document_load (EvDocument *document,
		      const char *uri,
		      GError    **error)
{
	ComicsDocument *comics_document = COMICS_DOCUMENT (document);
	GSList *supported_extensions;
	gchar *list_files_command = NULL, *stdout, *quoted_file, *mime_type;
	gchar **cbr_files;
	gboolean success;
	int i, retval;

	comics_document->archive = g_filename_from_uri (uri, NULL, error);
	g_return_val_if_fail (comics_document->archive != NULL, FALSE);

	quoted_file = g_shell_quote (comics_document->archive);
	mime_type = gnome_vfs_get_mime_type (uri);

	/* FIXME, use proper cbr/cbz mime types once they're
	 * included in shared-mime-info */
	if (!strcmp (mime_type, "application/x-cbr")) {
		comics_document->extract_command =
			g_strdup ("unrar p -c- -ierr");
		list_files_command =
			g_strdup_printf ("unrar vb -c- -- %s", quoted_file);
		comics_document->regex_arg = FALSE;
	} else if (!strcmp (mime_type, "application/x-cbz")) {
		comics_document->extract_command =
			g_strdup ("unzip -p -C");
		list_files_command = 
			g_strdup_printf ("zipinfo -1 -- %s", quoted_file);
		comics_document->regex_arg = TRUE;
	}

	g_free (quoted_file);

	/* Get list of files in archive */
	success = g_spawn_command_line_sync (list_files_command,
					     &stdout, NULL, &retval, error);
	g_free (list_files_command);

	if (!success) {
		g_free (mime_type);
		return FALSE;
	} else if (retval != 0) {
		g_set_error (error,
			     EV_DOCUMENT_ERROR,
			     EV_DOCUMENT_ERROR_INVALID,
			     _("File corrupted."));
		g_free (mime_type);
		return FALSE;
	}

	cbr_files = g_strsplit (stdout, "\n", 0);
	supported_extensions = get_supported_image_extensions ();
	for (i = 0; cbr_files[i] != NULL; i++) {
		gchar *suffix = g_strrstr (cbr_files[i], ".");
		if (!suffix)
			continue;
		suffix = g_ascii_strdown (suffix + 1, -1);

		if (g_slist_find_custom (supported_extensions, suffix,
					 (GCompareFunc) strcmp) != NULL) {
			comics_document->page_names =
				g_slist_insert_sorted (
					comics_document->page_names,
					g_strdup (g_strstrip (cbr_files[i])),
					(GCompareFunc) strcmp);
			comics_document->n_pages++;
		}

		g_free (suffix);
	}

	g_free (stdout);
	g_free (mime_type);
	g_strfreev (cbr_files);
	g_slist_foreach (supported_extensions, (GFunc) g_free, NULL);
	g_slist_free (supported_extensions);

	if (comics_document->n_pages == 0) {
		g_set_error (error,
			     EV_DOCUMENT_ERROR,
			     EV_DOCUMENT_ERROR_INVALID,
			     _("No images found in archive %s"),
			     uri);
		return FALSE;
	}

	return TRUE;
}


static gboolean
comics_document_save (EvDocument *document,
		      const char *uri,
		      GError    **error)
{
	ComicsDocument *comics_document = COMICS_DOCUMENT (document);

	return ev_xfer_uri_simple (comics_document->archive, uri, error);
}

static int
comics_document_get_n_pages (EvDocument *document)
{
	return COMICS_DOCUMENT (document)->n_pages;
}

static void
comics_document_get_page_size (EvDocument *document,
			       int         page,
			       double     *width,
			       double     *height)
{
	GdkPixbufLoader *loader;
	char **argv;
	guchar buf[1024];
	gboolean success, got_size = FALSE;
	gint outpipe = -1;
	GPid child_pid = -1;

	argv = extract_argv (document, page);
	success = g_spawn_async_with_pipes (NULL, argv, NULL,
					    G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
					    NULL, NULL,
					    &child_pid,
					    NULL, &outpipe, NULL, NULL);
	g_strfreev (argv);
	g_return_if_fail (success == TRUE);

	loader = gdk_pixbuf_loader_new ();
	g_signal_connect (loader, "area-prepared",
			  G_CALLBACK (get_page_size_area_prepared_cb),
			  &got_size);

	while (outpipe >= 0) {
		gssize bytes = read (outpipe, buf, 1024);
		
		if (bytes > 0)
			gdk_pixbuf_loader_write (loader, buf, bytes, NULL);
		if (bytes <= 0 || got_size) {
			close (outpipe);
			outpipe = -1;
			gdk_pixbuf_loader_close (loader, NULL);
		}
	}

	if (gdk_pixbuf_loader_get_pixbuf (loader)) {
		GdkPixbuf *pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
		if (width)
			*width = gdk_pixbuf_get_width (pixbuf);
		if (height)
			*height = gdk_pixbuf_get_height (pixbuf);
	}

	g_spawn_close_pid (child_pid);
	g_object_unref (loader);
}

static void
get_page_size_area_prepared_cb (GdkPixbufLoader *loader,
				gpointer         data)
{
	gboolean *got_size = data;
	*got_size = TRUE;
}

static GdkPixbuf *
comics_document_render_pixbuf (EvDocument  *document,
			       EvRenderContext *rc)
{
	GdkPixbufLoader *loader;
	GdkPixbuf *rotated_pixbuf;
	char **argv;
	guchar buf[4096];
	gboolean success;
	gint outpipe = -1;
	GPid child_pid = -1;

	argv = extract_argv (document, rc->page);
	success = g_spawn_async_with_pipes (NULL, argv, NULL,
					    G_SPAWN_SEARCH_PATH
					    | G_SPAWN_STDERR_TO_DEV_NULL,
					    NULL, NULL,
					    &child_pid,
					    NULL, &outpipe, NULL, NULL);
	g_strfreev (argv);
	g_return_val_if_fail (success == TRUE, NULL);

	loader = gdk_pixbuf_loader_new ();
	g_signal_connect (loader, "size-prepared",
			  G_CALLBACK (render_pixbuf_size_prepared_cb), &rc->scale);

	while (outpipe >= 0) {
		gssize bytes = read (outpipe, buf, 4096);

		if (bytes > 0) {
			gdk_pixbuf_loader_write (loader, buf, bytes, NULL);
		} else if (bytes <= 0) {
			close (outpipe);
			gdk_pixbuf_loader_close (loader, NULL);
			outpipe = -1;
		}
	}

	rotated_pixbuf = gdk_pixbuf_rotate_simple (gdk_pixbuf_loader_get_pixbuf (loader),
						   360 - rc->rotation);
	g_spawn_close_pid (child_pid);
	g_object_unref (loader);
	return rotated_pixbuf;
}

static void
render_pixbuf_size_prepared_cb (GdkPixbufLoader *loader,
				gint             width,
				gint             height,
				gpointer         data)
{
	double *scale = data;
	int w = width  * (*scale);
	int h = height * (*scale);

	gdk_pixbuf_loader_set_size (loader, w, h);
}

static void
comics_document_finalize (GObject *object)
{
	ComicsDocument *comics_document = COMICS_DOCUMENT (object);

	if (comics_document->archive)
		g_free (comics_document->archive);

	if (comics_document->page_names) {
		g_slist_foreach (comics_document->page_names,
				 (GFunc) g_free, NULL);
		g_slist_free (comics_document->page_names);
	}

	if (comics_document->extract_command)
		g_free (comics_document->extract_command);

	G_OBJECT_CLASS (comics_document_parent_class)->finalize (object);
}

static void
comics_document_class_init (ComicsDocumentClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->finalize = comics_document_finalize;
}

static gboolean
comics_document_can_get_text (EvDocument *document)
{
	return FALSE;
}

static EvDocumentInfo *
comics_document_get_info (EvDocument *document)
{
	EvDocumentInfo *info;
	info = g_new0 (EvDocumentInfo, 1);
	return info;
}

static void
comics_document_document_iface_init (EvDocumentIface *iface)
{
	iface->load = comics_document_load;
	iface->save = comics_document_save;
	iface->can_get_text  = comics_document_can_get_text;
	iface->get_n_pages   = comics_document_get_n_pages;
	iface->get_page_size = comics_document_get_page_size;
	iface->render_pixbuf = comics_document_render_pixbuf;
	iface->get_info      = comics_document_get_info;
}

static void
comics_document_init (ComicsDocument *comics_document)
{
	comics_document->archive = NULL;
	comics_document->page_names = NULL;
	comics_document->extract_command = NULL;
	comics_document->n_pages = 0;
}

/* Returns a list of file extensions supported by gdk-pixbuf */
static GSList*
get_supported_image_extensions()
{
	GSList *extensions = NULL;
	GSList *formats = gdk_pixbuf_get_formats ();
	GSList *l;

	for (l = formats; l != NULL; l = l->next) {
		int i;
		gchar **ext = gdk_pixbuf_format_get_extensions (l->data);

		for (i = 0; ext[i] != NULL; i++) {
			extensions = g_slist_append (extensions,
						     g_strdup (ext[i]));
		}

		g_strfreev (ext);
	}

	g_slist_free (formats);
	return extensions;
}

static void 
comics_document_thumbnails_get_geometry (EvDocumentThumbnails *document,
					 gint                  page,
					 gint                  suggested_width,
					 gint                 *width,
					 gint                 *height,
					 gdouble              *scale_factor)
{
	gdouble orig_width, orig_height, scale;

	comics_document_get_page_size (EV_DOCUMENT (document), page,
				       &orig_width, &orig_height);
	scale = suggested_width / orig_width;

	if (width)
		*width = suggested_width;
	if (height)
		*height = orig_height * scale;
	if (scale_factor)
		*scale_factor = scale;
}

static GdkPixbuf *
comics_document_thumbnails_get_thumbnail (EvDocumentThumbnails *document,
					gint 		      page,
					gint	              rotation,
					gint                  size,
					gboolean              border)
{
	GdkPixbuf *thumbnail, *framed;
	gint thumb_width, thumb_height;
	gdouble scale;
        EvRenderContext *rc;

	comics_document_thumbnails_get_geometry (document, page, size,
						 &thumb_width, &thumb_height,
						 &scale);

	rc = ev_render_context_new (rotation, page, scale);
	thumbnail = comics_document_render_pixbuf (EV_DOCUMENT (document),
						   rc);
	g_object_unref (G_OBJECT (rc));

	if (border) {
	      GdkPixbuf *tmp_pixbuf = thumbnail;
	      thumbnail = ev_document_misc_get_thumbnail_frame (-1, -1, 0, tmp_pixbuf);
	      g_object_unref (tmp_pixbuf);
	}

	return thumbnail;
}

static void
comics_document_thumbnails_get_dimensions (EvDocumentThumbnails *document,
					   gint                  page,
					   gint                  suggested_width,
					   gint                  *width,
					   gint                  *height)
{
	comics_document_thumbnails_get_geometry (document, page,
						 suggested_width,
						 width, height, NULL);
}

static void
comics_document_document_thumbnails_iface_init (EvDocumentThumbnailsIface *iface)
{
	iface->get_thumbnail = comics_document_thumbnails_get_thumbnail;
	iface->get_dimensions = comics_document_thumbnails_get_dimensions;
}

static char**
extract_argv (EvDocument *document, gint page)
{
	ComicsDocument *comics_document = COMICS_DOCUMENT (document);
	char **argv;
	char *command_line, *quoted_archive, *quoted_filename;

	quoted_archive = g_shell_quote (comics_document->archive);
	if (comics_document->regex_arg) {
		quoted_filename = comics_regex_quote (
			g_slist_nth_data (comics_document->page_names, page));
	} else {
		quoted_filename = g_shell_quote (
			g_slist_nth_data (comics_document->page_names, page));
	}

	command_line = g_strdup_printf ("%s -- %s %s",
					comics_document->extract_command,
					quoted_archive,
					quoted_filename);
	g_shell_parse_argv (command_line, NULL, &argv, NULL);

	g_free (command_line);
	g_free (quoted_archive);
	g_free (quoted_filename);
	return argv;
}
