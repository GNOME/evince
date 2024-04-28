/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/*
 * Copyright (C) 2009-2010 Juanjo Marín <juanj.marin@juntadeandalucia.es>
 * Copyright (C) 2005, Teemu Tervo <teemu.tervo@gmx.net>
 * Copyright (C) 2016-2017, Bastien Nocera <hadess@hadess.net>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include "comics-document.h"
#include "ev-document-misc.h"
#include "ev-file-helpers.h"
#include "ev-archive.h"

#define BLOCK_SIZE 10240

typedef struct _ComicsDocumentClass ComicsDocumentClass;

struct _ComicsDocumentClass
{
	EvDocumentClass parent_class;
};

struct _ComicsDocument
{
	EvDocument     parent_instance;
	EvArchive     *archive;
	gchar         *archive_path;
	gchar         *archive_uri;
	GPtrArray     *page_names; /* elem: char * */
	GHashTable    *page_positions; /* key: char *, value: uint + 1 */
};

G_DEFINE_TYPE (ComicsDocument, comics_document, EV_TYPE_DOCUMENT)

#define FORMAT_UNKNOWN     0
#define FORMAT_SUPPORTED   1
#define FORMAT_UNSUPPORTED 2

/* Returns a GHashTable of:
 * <key>: file extensions
 * <value>: degree of support in gdk-pixbuf */
static GHashTable *
get_image_extensions(void)
{
	GHashTable *extensions;
	GSList *formats = gdk_pixbuf_get_formats ();
	GSList *l;
	guint i;
	const char *known_image_formats[] = {
		"png",
		"jpg",
		"jpeg",
		"webp"
	};

	extensions = g_hash_table_new_full (g_str_hash, g_str_equal,
					    g_free, NULL);
	for (l = formats; l != NULL; l = l->next) {
		int i;
		gchar **ext = gdk_pixbuf_format_get_extensions (l->data);

		for (i = 0; ext[i] != NULL; i++) {
			g_hash_table_insert (extensions,
					     g_strdup (ext[i]),
					     GINT_TO_POINTER (FORMAT_SUPPORTED));
		}

		g_strfreev (ext);
	}

	g_slist_free (formats);

	/* Add known image formats that aren't supported by gdk-pixbuf */
	for (i = 0; i < G_N_ELEMENTS (known_image_formats); i++) {
		if (!g_hash_table_lookup (extensions, known_image_formats[i])) {
			g_hash_table_insert (extensions,
					     g_strdup (known_image_formats[i]),
					     GINT_TO_POINTER (FORMAT_UNSUPPORTED));
		}
	}

	return extensions;
}

static int
has_supported_extension (const char *name,
			 GHashTable *supported_extensions)
{
	gboolean ret = FALSE;
	gchar *suffix;

	suffix = g_strrstr (name, ".");
	if (!suffix)
		return ret;

	suffix = g_ascii_strdown (suffix + 1, -1);
	ret = GPOINTER_TO_INT (g_hash_table_lookup (supported_extensions, suffix));
	g_free (suffix);

	return ret;
}

#define APPLE_DOUBLE_PREFIX "._"
static gboolean
is_apple_double (const char *name)
{
	char *basename;
	gboolean ret = FALSE;

	basename = g_path_get_basename (name);
	if (basename == NULL) {
		g_debug ("Filename '%s' doesn't have a basename?", name);
		return ret;
	}

	ret = g_str_has_prefix (basename, APPLE_DOUBLE_PREFIX);
	g_free (basename);

	return ret;
}

static gboolean
archive_reopen_if_needed (ComicsDocument  *comics_document,
			  const char      *page_wanted,
			  GError         **error)
{
	const char *current_page;
	guint current_page_idx, page_wanted_idx;

	if (ev_archive_at_entry (comics_document->archive)) {
		current_page = ev_archive_get_entry_pathname (comics_document->archive);
		if (current_page) {
			current_page_idx = GPOINTER_TO_UINT (g_hash_table_lookup (comics_document->page_positions, current_page));
			page_wanted_idx = GPOINTER_TO_UINT (g_hash_table_lookup (comics_document->page_positions, page_wanted));

			if (current_page_idx != 0 &&
			    page_wanted_idx != 0 &&
			    page_wanted_idx > current_page_idx)
				return TRUE;
		}

		ev_archive_reset (comics_document->archive);
	}

	return ev_archive_open_filename (comics_document->archive, comics_document->archive_path, error);
}

static GPtrArray *
comics_document_list (ComicsDocument  *comics_document,
		      GError         **error)
{
	GPtrArray *array = NULL;
	gboolean has_encrypted_files, has_unsupported_images, has_archive_errors;
	GHashTable *supported_extensions = NULL;

	if (!ev_archive_open_filename (comics_document->archive, comics_document->archive_path, error)) {
		if (*error != NULL) {
			g_warning ("Fatal error handling archive (%s): %s", G_STRFUNC, (*error)->message);
			g_clear_error (error);
		}

		g_set_error_literal (error,
				     EV_DOCUMENT_ERROR,
				     EV_DOCUMENT_ERROR_INVALID,
				     _("File is corrupted"));
		goto out;
	}

	supported_extensions = get_image_extensions ();

	has_encrypted_files = FALSE;
	has_unsupported_images = FALSE;
	has_archive_errors = FALSE;
	array = g_ptr_array_sized_new (64);
	g_ptr_array_set_free_func (array, g_free);

	while (1) {
		const char *name;
		int supported;

		if (!ev_archive_read_next_header (comics_document->archive, error)) {
			if (*error != NULL) {
				g_debug ("Fatal error handling archive (%s): %s", G_STRFUNC, (*error)->message);
				g_clear_error (error);
				has_archive_errors = TRUE;
				goto out;
			}
			break;
		}

		name = ev_archive_get_entry_pathname (comics_document->archive);
		/* Ignore https://en.wikipedia.org/wiki/AppleSingle_and_AppleDouble_formats */
		if (is_apple_double (name)) {
			g_debug ("Not adding AppleDouble file '%s' to the list of files in the comics", name);
			continue;
		}

		supported = has_supported_extension (name, supported_extensions);
		if (supported == FORMAT_UNKNOWN) {
			g_debug ("Not adding unsupported file '%s' to the list of files in the comics", name);
			continue;
		} else if (supported == FORMAT_UNSUPPORTED) {
			g_debug ("Not adding unsupported image '%s' to the list of files in the comics", name);
			has_unsupported_images = TRUE;
			continue;
		}

		if (ev_archive_get_entry_is_encrypted (comics_document->archive)) {
			g_debug ("Not adding encrypted file '%s' to the list of files in the comics", name);
			has_encrypted_files = TRUE;
			continue;
		}

		g_debug ("Adding '%s' to the list of files in the comics", name);
		g_ptr_array_add (array, g_strdup (name));
	}

out:
	if (array->len == 0) {
		g_ptr_array_free (array, TRUE);
		array = NULL;

		if (has_encrypted_files) {
			g_set_error_literal (error,
					     EV_DOCUMENT_ERROR,
					     EV_DOCUMENT_ERROR_ENCRYPTED,
					     _("Archive is encrypted"));
		} else if (has_unsupported_images) {
			g_set_error_literal (error,
					     EV_DOCUMENT_ERROR,
					     EV_DOCUMENT_ERROR_UNSUPPORTED_CONTENT,
					     _("No supported images in archive"));
		} else if (has_archive_errors) {
			g_set_error_literal (error,
					     EV_DOCUMENT_ERROR,
					     EV_DOCUMENT_ERROR_INVALID,
					     _("File is corrupted"));
		} else {
			g_set_error_literal (error,
					     EV_DOCUMENT_ERROR,
					     EV_DOCUMENT_ERROR_INVALID,
					     _("No files in archive"));
		}
	}

	if (supported_extensions)
		g_hash_table_destroy (supported_extensions);
	ev_archive_reset (comics_document->archive);
	return array;
}

static GHashTable *
save_positions (GPtrArray *page_names)
{
	guint i;
	GHashTable *ht;

	ht = g_hash_table_new (g_str_hash, g_str_equal);
	for (i = 0; i < page_names->len; i++)
		g_hash_table_insert (ht, page_names->pdata[i], GUINT_TO_POINTER(i + 1));
	return ht;
}

/* This function chooses the archive decompression support
 * book based on its mime type. */
static gboolean
comics_check_decompress_support	(gchar          *mime_type,
				 ComicsDocument *comics_document,
				 GError         **error)
{
	if (g_content_type_is_a (mime_type, "application/x-cbr") ||
	    g_content_type_is_a (mime_type, "application/x-rar")) {
		if (ev_archive_set_archive_type (comics_document->archive, EV_ARCHIVE_TYPE_RAR))
			return TRUE;
	} else if (g_content_type_is_a (mime_type, "application/x-cbz") ||
		   g_content_type_is_a (mime_type, "application/zip")) {
		if (ev_archive_set_archive_type (comics_document->archive, EV_ARCHIVE_TYPE_ZIP))
			return TRUE;
	} else if (g_content_type_is_a (mime_type, "application/x-cb7") ||
		   g_content_type_is_a (mime_type, "application/x-7z-compressed")) {
		if (ev_archive_set_archive_type (comics_document->archive, EV_ARCHIVE_TYPE_7Z))
			return TRUE;
	} else if (g_content_type_is_a (mime_type, "application/x-cbt") ||
		   g_content_type_is_a (mime_type, "application/x-tar")) {
		if (ev_archive_set_archive_type (comics_document->archive, EV_ARCHIVE_TYPE_TAR))
			return TRUE;
	} else {
		g_set_error (error,
			     EV_DOCUMENT_ERROR,
			     EV_DOCUMENT_ERROR_INVALID,
			     _("Not a comic book MIME type: %s"),
			     mime_type);
			     return FALSE;
	}
	g_set_error_literal (error,
			     EV_DOCUMENT_ERROR,
			     EV_DOCUMENT_ERROR_INVALID,
			     _("libarchive lacks support for this comic book’s "
			     "compression, please contact your distributor"));
	return FALSE;
}

static int
sort_page_names (gconstpointer a,
                 gconstpointer b)
{
  gchar *temp1, *temp2;
  gint ret;

  temp1 = g_utf8_collate_key_for_filename (* (const char **) a, -1);
  temp2 = g_utf8_collate_key_for_filename (* (const char **) b, -1);

  ret = strcmp (temp1, temp2);

  g_free (temp1);
  g_free (temp2);

  return ret;
}

static gboolean
comics_document_load (EvDocument *document,
		      const char *uri,
		      GError    **error)
{
	ComicsDocument *comics_document = COMICS_DOCUMENT (document);
	gchar *mime_type;
	GFile *file;

	file = g_file_new_for_uri (uri);
	comics_document->archive_path = g_file_get_path (file);
	g_object_unref (file);

	if (!comics_document->archive_path) {
		g_set_error_literal (error,
                                     EV_DOCUMENT_ERROR,
                                     EV_DOCUMENT_ERROR_INVALID,
                                     _("Can not get local path for archive"));
		return FALSE;
	}

	comics_document->archive_uri = g_strdup (uri);

	mime_type = ev_file_get_mime_type (uri, FALSE, error);
	if (mime_type == NULL)
		return FALSE;

	if (!comics_check_decompress_support (mime_type, comics_document, error)) {
		g_free (mime_type);
		return FALSE;
	}
	g_free (mime_type);

	/* Get list of files in archive */
	comics_document->page_names = comics_document_list (comics_document, error);
	if (!comics_document->page_names)
		return FALSE;

	/* Keep an index */
	comics_document->page_positions = save_positions (comics_document->page_names);

        /* Now sort the pages */
        g_ptr_array_sort (comics_document->page_names, sort_page_names);

	return TRUE;
}

static gboolean
comics_document_save (EvDocument *document,
		      const char *uri,
		      GError    **error)
{
	ComicsDocument *comics_document = COMICS_DOCUMENT (document);

	return ev_xfer_uri_simple (comics_document->archive_uri, uri, error);
}

static int
comics_document_get_n_pages (EvDocument *document)
{
	ComicsDocument *comics_document = COMICS_DOCUMENT (document);

        if (comics_document->page_names == NULL)
                return 0;

	return comics_document->page_names->len;
}

typedef struct {
	gboolean got_info;
	int height;
	int width;
} PixbufInfo;

static void
get_page_size_prepared_cb (GdkPixbufLoader *loader,
			   int              width,
			   int              height,
			   PixbufInfo      *info)
{
	info->got_info = TRUE;
	info->height = height;
	info->width = width;
}

static void
comics_document_get_page_size (EvDocument *document,
			       EvPage     *page,
			       double     *width,
			       double     *height)
{
	GdkPixbufLoader *loader;
	ComicsDocument *comics_document = COMICS_DOCUMENT (document);
	const char *page_path;
	PixbufInfo info;
	GError *error = NULL;

	page_path = g_ptr_array_index (comics_document->page_names, page->index);

	if (!archive_reopen_if_needed (comics_document, page_path, &error)) {
		g_warning ("Fatal error opening archive: %s", error->message);
		g_error_free (error);
		return;
	}

	loader = gdk_pixbuf_loader_new ();
	info.got_info = FALSE;
	g_signal_connect (loader, "size-prepared",
			  G_CALLBACK (get_page_size_prepared_cb),
			  &info);

	while (1) {
		const char *name;
		GError *error = NULL;

		if (!ev_archive_read_next_header (comics_document->archive, &error)) {
			if (error != NULL) {
				g_warning ("Fatal error handling archive (%s): %s", G_STRFUNC, error->message);
				g_error_free (error);
			}
			break;
		}

		name = ev_archive_get_entry_pathname (comics_document->archive);
		if (g_strcmp0 (name, page_path) == 0) {
			char buf[BLOCK_SIZE];
			gssize read;
			gint64 left;

			left = ev_archive_get_entry_size (comics_document->archive);
			read = ev_archive_read_data (comics_document->archive, buf,
						     MIN(BLOCK_SIZE, left), &error);
			while (read > 0 && !info.got_info) {
				if (!gdk_pixbuf_loader_write (loader, (guchar *) buf, read, &error)) {
					read = -1;
					break;
				}
				left -= read;
				read = ev_archive_read_data (comics_document->archive, buf,
							     MIN(BLOCK_SIZE, left), &error);
			}
			if (read < 0) {
				g_warning ("Fatal error reading '%s' in archive: %s", name, error->message);
				g_error_free (error);
			}
			break;
		}
	}

	gdk_pixbuf_loader_close (loader, NULL);
	g_object_unref (loader);

	if (info.got_info) {
		if (width)
			*width = info.width;
		if (height)
			*height = info.height;
	}
}

static void
render_pixbuf_size_prepared_cb (GdkPixbufLoader *loader,
				gint             width,
				gint             height,
				EvRenderContext *rc)
{
	int scaled_width, scaled_height;

	ev_render_context_compute_scaled_size (rc, width, height, &scaled_width, &scaled_height);
	gdk_pixbuf_loader_set_size (loader, scaled_width, scaled_height);
}

static GdkPixbuf *
comics_document_render_pixbuf (EvDocument      *document,
			       EvRenderContext *rc)
{
	GdkPixbufLoader *loader;
	GdkPixbuf *tmp_pixbuf;
	GdkPixbuf *rotated_pixbuf = NULL;
	ComicsDocument *comics_document = COMICS_DOCUMENT (document);
	const char *page_path;
	GError *error = NULL;

	page_path = g_ptr_array_index (comics_document->page_names, rc->page->index);

	if (!archive_reopen_if_needed (comics_document, page_path, &error)) {
		g_warning ("Fatal error opening archive: %s", error->message);
		g_error_free (error);
		return NULL;
	}

	loader = gdk_pixbuf_loader_new ();
	g_signal_connect (loader, "size-prepared",
			  G_CALLBACK (render_pixbuf_size_prepared_cb),
			  rc);

	while (1) {
		const char *name;

		if (!ev_archive_read_next_header (comics_document->archive, &error)) {
			if (error != NULL) {
				g_warning ("Fatal error handling archive (%s): %s", G_STRFUNC, error->message);
				g_error_free (error);
			}
			break;
		}

		name = ev_archive_get_entry_pathname (comics_document->archive);
		if (g_strcmp0 (name, page_path) == 0) {
			size_t size = ev_archive_get_entry_size (comics_document->archive);
			char *buf;
			ssize_t read;

			buf = g_malloc (size);
			read = ev_archive_read_data (comics_document->archive, buf, size, &error);
			if (read <= 0) {
				if (read < 0) {
					g_warning ("Fatal error reading '%s' in archive: %s", name, error->message);
					g_error_free (error);
				} else {
					g_warning ("Read an empty file from the archive");
				}
			} else {
				gdk_pixbuf_loader_write (loader, (guchar *) buf, size, NULL);
			}
			g_free (buf);
			gdk_pixbuf_loader_close (loader, NULL);
			break;
		}
	}

	tmp_pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
	if (tmp_pixbuf) {
		if ((rc->rotation % 360) == 0)
			rotated_pixbuf = g_object_ref (tmp_pixbuf);
		else
			rotated_pixbuf = gdk_pixbuf_rotate_simple (tmp_pixbuf,
								   360 - rc->rotation);
	}
	g_object_unref (loader);

	return rotated_pixbuf;
}

static cairo_surface_t *
comics_document_render (EvDocument      *document,
			EvRenderContext *rc)
{
	GdkPixbuf       *pixbuf;
	cairo_surface_t *surface;

	pixbuf = comics_document_render_pixbuf (document, rc);
	if (!pixbuf)
		return NULL;
	surface = ev_document_misc_surface_from_pixbuf (pixbuf);
	g_clear_object (&pixbuf);

	return surface;
}

static void
comics_document_finalize (GObject *object)
{
	ComicsDocument *comics_document = COMICS_DOCUMENT (object);

	if (comics_document->page_names)
                g_ptr_array_free (comics_document->page_names, TRUE);

	g_clear_pointer (&comics_document->page_positions, g_hash_table_destroy);
	g_clear_object (&comics_document->archive);
	g_free (comics_document->archive_path);
	g_free (comics_document->archive_uri);

	G_OBJECT_CLASS (comics_document_parent_class)->finalize (object);
}

static void
comics_document_class_init (ComicsDocumentClass *klass)
{
	GObjectClass    *gobject_class = G_OBJECT_CLASS (klass);
	EvDocumentClass *ev_document_class = EV_DOCUMENT_CLASS (klass);

	gobject_class->finalize = comics_document_finalize;

	ev_document_class->load = comics_document_load;
	ev_document_class->save = comics_document_save;
	ev_document_class->get_n_pages = comics_document_get_n_pages;
	ev_document_class->get_page_size = comics_document_get_page_size;
	ev_document_class->render = comics_document_render;
}

static void
comics_document_init (ComicsDocument *comics_document)
{
	comics_document->archive = ev_archive_new ();
}

GType
ev_backend_query_type (void)
{
	return COMICS_TYPE_DOCUMENT;
}
