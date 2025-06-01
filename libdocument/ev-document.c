/*
 *  Copyright (C) 2009 Carlos Garcia Campos
 *  Copyright (C) 2004 Marco Pesenti Gritti
 *  Copyright © 2018 Christian Persch
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
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#include "ev-document.h"
#include "ev-document-misc.h"
#include "synctex_parser.h"

enum {
	PROP_0,
	PROP_MODIFIED
};

typedef struct _EvPageSize
{
	gdouble width;
	gdouble height;
} EvPageSize;

struct _EvDocumentPrivate
{
	gchar          *uri;
	guint64         file_size;

	gboolean        cache_loaded;
	gint            n_pages;
	gboolean        modified;

	gboolean        uniform;
	gdouble         uniform_width;
	gdouble         uniform_height;

	gdouble         max_width;
	gdouble         max_height;
	gdouble         min_width;
	gdouble         min_height;
	gint            max_label;

	gchar         **page_labels;
	EvPageSize     *page_sizes;
	EvDocumentInfo *info;

	synctex_scanner_p synctex_scanner;
};

static guint64         _ev_document_get_size_gfile  (GFile      *file);
static guint64         _ev_document_get_size        (const char *uri);
static gint            _ev_document_get_n_pages     (EvDocument *document);
static void            _ev_document_get_page_size   (EvDocument *document,
						     EvPage     *page,
						     double     *width,
						     double     *height);
static gchar          *_ev_document_get_page_label  (EvDocument *document,
						     EvPage     *page);
static EvDocumentInfo *_ev_document_get_info        (EvDocument *document);
static gboolean        _ev_document_support_synctex (EvDocument *document);

static GMutex ev_doc_mutex;
static GMutex ev_fc_mutex;

typedef struct _EvDocumentPrivate EvDocumentPrivate;

#define GET_PRIVATE(o) ev_document_get_instance_private (o)

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (EvDocument, ev_document, G_TYPE_OBJECT)

GQuark
ev_document_error_quark (void)
{
  static GQuark q = 0;
  if (q == 0)
    q = g_quark_from_static_string ("ev-document-error-quark");

  return q;
}

static EvPage *
ev_document_impl_get_page (EvDocument *document,
			   gint        index)
{
	return ev_page_new (index);
}

static EvDocumentInfo *
ev_document_impl_get_info (EvDocument *document)
{
	return ev_document_info_new ();
}

static void
ev_document_finalize (GObject *object)
{
	EvDocument *document = EV_DOCUMENT (object);
	EvDocumentPrivate *priv = GET_PRIVATE (document);

	g_clear_pointer (&priv->uri, g_free);
	g_clear_pointer (&priv->page_sizes, g_free);
	g_clear_pointer (&priv->page_labels, g_strfreev);
	g_clear_pointer (&priv->info, ev_document_info_free);
	g_clear_pointer (&priv->synctex_scanner, synctex_scanner_free);

	G_OBJECT_CLASS (ev_document_parent_class)->finalize (object);
}

static void
ev_document_set_property (GObject      *object,
			  guint         prop_id,
			  const GValue *value,
			  GParamSpec   *pspec)
{
	switch (prop_id) {
	case PROP_MODIFIED:
		ev_document_set_modified (EV_DOCUMENT (object),
					  g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ev_document_get_property (GObject     *object,
			  guint        prop_id,
			  GValue      *value,
			  GParamSpec  *pspec)
{
	EvDocument *document = EV_DOCUMENT (object);
	EvDocumentPrivate *priv = GET_PRIVATE (document);

	switch (prop_id) {
	case PROP_MODIFIED:
		g_value_set_boolean (value, priv->modified);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ev_document_init (EvDocument *document)
{
	EvDocumentPrivate *priv = GET_PRIVATE (document);

	/* Assume all pages are the same size until proven otherwise */
	priv->uniform = TRUE;
}

static void
ev_document_class_init (EvDocumentClass *klass)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

	klass->get_page = ev_document_impl_get_page;
	klass->get_info = ev_document_impl_get_info;
	klass->get_backend_info = NULL;

	g_object_class->get_property = ev_document_get_property;
	g_object_class->set_property = ev_document_set_property;
	g_object_class->finalize = ev_document_finalize;

	g_object_class_install_property (g_object_class,
					 PROP_MODIFIED,
					 g_param_spec_boolean ("modified",
							       "Is modified",
							       "Whether the document has been modified",
							       FALSE,
							       G_PARAM_READWRITE |
							       G_PARAM_STATIC_STRINGS));
}

/**
 * ev_document_get_modified:
 * @document: an #EvDocument
 *
 * Returns: %TRUE iff the document has been modified.
 *
 * You can monitor changes to the modification state by connecting to the
 * notify::modified signal on @document.
 *
 * Since: 3.28
 */
gboolean
ev_document_get_modified (EvDocument *document)
{
	g_return_val_if_fail (EV_IS_DOCUMENT (document), FALSE);
	EvDocumentPrivate *priv = GET_PRIVATE (document);

	return priv->modified;
}

/**
 * ev_document_set_modified:
 * @document: an #EvDocument
 * @modified: a boolean value to set the document as modified or not.
 *
 * Set the @document modification state as @modified.
 *
 * Since: 3.28
 */
void
ev_document_set_modified (EvDocument *document,
			  gboolean    modified)
{
	g_return_if_fail (EV_IS_DOCUMENT (document));
	EvDocumentPrivate *priv = GET_PRIVATE (document);

	if (priv->modified != modified) {
		priv->modified = modified;
		g_object_notify (G_OBJECT (document), "modified");
	}
}

void
ev_document_doc_mutex_lock (void)
{
	g_mutex_lock (&ev_doc_mutex);
}

void
ev_document_doc_mutex_unlock (void)
{
	g_mutex_unlock (&ev_doc_mutex);
}

gboolean
ev_document_doc_mutex_trylock (void)
{
	return g_mutex_trylock (&ev_doc_mutex);
}

void
ev_document_fc_mutex_lock (void)
{
	g_mutex_lock (&ev_fc_mutex);
}

void
ev_document_fc_mutex_unlock (void)
{
	g_mutex_unlock (&ev_fc_mutex);
}

gboolean
ev_document_fc_mutex_trylock (void)
{
	return g_mutex_trylock (&ev_fc_mutex);
}

static void
ev_document_setup_cache (EvDocument *document)
{
        EvDocumentPrivate *priv = GET_PRIVATE (document);
        gboolean custom_page_labels = FALSE;
        gint i;

        /* Cache some info about the document to avoid
         * going to the backends since it requires locks
         */
	priv->cache_loaded = TRUE;

        for (i = 0; i < priv->n_pages; i++) {
                EvPage     *page = ev_document_get_page (document, i);
                gdouble     page_width = 0;
                gdouble     page_height = 0;
                EvPageSize *page_size;
                gchar      *page_label;

                _ev_document_get_page_size (document, page, &page_width, &page_height);

                if (i == 0) {
                        priv->uniform_width = page_width;
                        priv->uniform_height = page_height;
                        priv->max_width = priv->uniform_width;
                        priv->max_height = priv->uniform_height;
                        priv->min_width = priv->uniform_width;
                        priv->min_height = priv->uniform_height;
                } else if (priv->uniform &&
                            (priv->uniform_width != page_width ||
                            priv->uniform_height != page_height)) {
                        /* It's a different page size.  Backfill the array. */
                        int j;

                        /* Check for potential integer overflow in allocation - Issue #2094 */
                        if ((gsize)priv->n_pages > G_MAXSIZE / sizeof(EvPageSize))
                                g_error ("Exiting program due to abnormal page count detected: %d", priv->n_pages);

                        priv->page_sizes = g_new0 (EvPageSize, priv->n_pages);

                        for (j = 0; j < i; j++) {
                                page_size = &(priv->page_sizes[j]);
                                page_size->width = priv->uniform_width;
                                page_size->height = priv->uniform_height;
                        }
                        priv->uniform = FALSE;
                }
                if (!priv->uniform) {
                        page_size = &(priv->page_sizes[i]);

                        page_size->width = page_width;
                        page_size->height = page_height;

                        if (page_width > priv->max_width)
                                priv->max_width = page_width;
                        if (page_width < priv->min_width)
                                priv->min_width = page_width;

                        if (page_height > priv->max_height)
                                priv->max_height = page_height;
                        if (page_height < priv->min_height)
                                priv->min_height = page_height;
                }

                page_label = _ev_document_get_page_label (document, page);
                if (page_label) {
                        if (!priv->page_labels)
                                priv->page_labels = g_new0 (gchar *, priv->n_pages + 1);

                        if (!custom_page_labels) {
                                gchar *real_page_label;

                                real_page_label = g_strdup_printf ("%d", i + 1);
                                custom_page_labels = g_strcmp0 (real_page_label, page_label) != 0;
                                g_free (real_page_label);
                        }

                        priv->page_labels[i] = page_label;
                        priv->max_label = MAX (priv->max_label,
                                                g_utf8_strlen (page_label, 256));
                }

                g_object_unref (page);
        }

	if (!custom_page_labels)
		g_clear_pointer (&priv->page_labels, g_strfreev);
}

static void
ev_document_initialize_synctex (EvDocument  *document,
				const gchar *uri)
{
	EvDocumentPrivate *priv = GET_PRIVATE (document);

	if (_ev_document_support_synctex (document)) {
		gchar *filename;

		filename = g_filename_from_uri (uri, NULL, NULL);
		if (filename != NULL) {
			priv->synctex_scanner =
				synctex_scanner_new_with_output_file (filename, NULL, 1);
			g_free (filename);
		}
	}
}

/**
 * ev_document_load_full:
 * @document: a #EvDocument
 * @uri: the document's URI
 * @flags: flags from #EvDocumentLoadFlags
 * @error: a #GError location to store an error, or %NULL
 *
 * Loads @document from @uri.
 *
 * On failure, %FALSE is returned and @error is filled in.
 * If the document is encrypted, EV_DEFINE_ERROR_ENCRYPTED is returned.
 * If the backend cannot load the specific document, EV_DOCUMENT_ERROR_INVALID
 * is returned. Other errors are possible too, depending on the backend
 * used to load the document and the URI, e.g. #GIOError, #GFileError, and
 * #GConvertError.
 *
 * Returns: %TRUE on success, or %FALSE on failure.
 */
gboolean
ev_document_load_full (EvDocument           *document,
		       const char           *uri,
		       EvDocumentLoadFlags   flags,
		       GError              **error)
{
	EvDocumentClass *klass = EV_DOCUMENT_GET_CLASS (document);
	gboolean retval;
	GError *err = NULL;
	EvDocumentPrivate *priv = GET_PRIVATE (document);

	retval = klass->load (document, uri, &err);
	if (!retval) {
		if (err) {
			g_propagate_error (error, err);
		} else {
			g_warning ("%s::EvDocument::load returned FALSE but did not fill in @error; fix the backend!\n",
				   G_OBJECT_TYPE_NAME (document));

			/* So upper layers don't crash */
			g_set_error_literal (error,
					     EV_DOCUMENT_ERROR,
					     EV_DOCUMENT_ERROR_INVALID,
					     "Internal error in backend");
		}
	} else {
		priv->info = _ev_document_get_info (document);
		priv->n_pages = _ev_document_get_n_pages (document);
		if (!(flags & EV_DOCUMENT_LOAD_FLAG_NO_CACHE))
			ev_document_setup_cache (document);
		priv->uri = g_strdup (uri);
		priv->file_size = _ev_document_get_size (uri);
		ev_document_initialize_synctex (document, uri);
        }

	return retval;
}

/**
 * ev_document_load:
 * @document: a #EvDocument
 * @uri: the document's URI
 * @error: a #GError location to store an error, or %NULL
 *
 * Loads @document from @uri.
 *
 * On failure, %FALSE is returned and @error is filled in.
 * If the document is encrypted, EV_DEFINE_ERROR_ENCRYPTED is returned.
 * If the backend cannot load the specific document, EV_DOCUMENT_ERROR_INVALID
 * is returned. If the backend does not support the format for the document's
 * contents, EV_DOCUMENT_ERROR_UNSUPPORTED_CONTENT is returned. Other errors
 * are possible too, depending on the backend used to load the document and
 * the URI, e.g. #GIOError, #GFileError, and #GConvertError.
 *
 * Returns: %TRUE on success, or %FALSE on failure.
 */
gboolean
ev_document_load (EvDocument  *document,
		  const char  *uri,
		  GError     **error)
{
	return ev_document_load_full (document, uri,
				      EV_DOCUMENT_LOAD_FLAG_NONE, error);
}

/**
 * ev_document_load_stream:
 * @document: a #EvDocument
 * @stream: a #GInputStream
 * @flags: flags from #EvDocumentLoadFlags
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @error: (allow-none): a #GError location to store an error, or %NULL
 *
 * Synchronously loads the document from @stream.
 * See ev_document_load() for more information.
 *
 * Returns: %TRUE if loading succeeded, or %FALSE on error with @error filled in
 *
 * Since: 3.6
 */
gboolean
ev_document_load_stream (EvDocument         *document,
                         GInputStream       *stream,
                         EvDocumentLoadFlags flags,
                         GCancellable       *cancellable,
                         GError            **error)
{
        EvDocumentClass *klass;

        g_return_val_if_fail (EV_IS_DOCUMENT (document), FALSE);
        g_return_val_if_fail (G_IS_INPUT_STREAM (stream), FALSE);
        g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
        g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
	EvDocumentPrivate *priv = GET_PRIVATE (document);

        klass = EV_DOCUMENT_GET_CLASS (document);
        if (!klass->load_stream) {
                g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                                     "Backend does not support loading from stream");
                return FALSE;
        }

        if (!klass->load_stream (document, stream, flags, cancellable, error))
                return FALSE;

	priv->info = _ev_document_get_info (document);
	priv->n_pages = _ev_document_get_n_pages (document);

        if (!(flags & EV_DOCUMENT_LOAD_FLAG_NO_CACHE))
                ev_document_setup_cache (document);

        return TRUE;
}

/**
 * ev_document_load_gfile:
 * @document: a #EvDocument
 * @file: a #GFile
 * @flags: flags from #EvDocumentLoadFlags
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @error: (allow-none): a #GError location to store an error, or %NULL
 *
 * Synchronously loads the document from @file.
 * See ev_document_load() for more information.
 *
 * Returns: %TRUE if loading succeeded, or %FALSE on error with @error filled in
 *
 * Since: 3.6
 */
gboolean
ev_document_load_gfile (EvDocument         *document,
                        GFile              *file,
                        EvDocumentLoadFlags flags,
                        GCancellable       *cancellable,
                        GError            **error)
{
        EvDocumentClass *klass;

        g_return_val_if_fail (EV_IS_DOCUMENT (document), FALSE);
        g_return_val_if_fail (G_IS_FILE (file), FALSE);
        g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
        g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
	EvDocumentPrivate *priv = GET_PRIVATE (document);

        klass = EV_DOCUMENT_GET_CLASS (document);
        if (!klass->load_gfile) {
                g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                                     "Backend does not support loading from GFile");
                return FALSE;
        }

        if (!klass->load_gfile (document, file, flags, cancellable, error))
                return FALSE;

	priv->info = _ev_document_get_info (document);
	priv->n_pages = _ev_document_get_n_pages (document);

        if (!(flags & EV_DOCUMENT_LOAD_FLAG_NO_CACHE))
                ev_document_setup_cache (document);

	priv->uri = g_file_get_uri (file);
	priv->file_size = _ev_document_get_size_gfile (file);
	ev_document_initialize_synctex (document, priv->uri);

        return TRUE;
}

/**
 * ev_document_load_fd:
 * @document: a #EvDocument
 * @fd: a file descriptor
 * @flags: flags from #EvDocumentLoadFlags
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @error: (allow-none): a #GError location to store an error, or %NULL
 *
 * Synchronously loads the document from @fd, which must refer to
 * a regular file.
 *
 * Note that this function takes ownership of @fd; you must not ever
 * operate on it again. It will be closed automatically if the document
 * is destroyed, or if this function returns %NULL.
 *
 * See ev_document_load() for more information.
 *
 * Returns: %TRUE if loading succeeded, or %FALSE on error with @error filled in
 *
 * Since: 42.0
 */
gboolean
ev_document_load_fd (EvDocument         *document,
                     int                 fd,
                     EvDocumentLoadFlags flags,
                     GCancellable       *cancellable,
                     GError            **error)
{
        EvDocumentClass *klass;
        struct stat statbuf;
        int fd_flags;

        g_return_val_if_fail (EV_IS_DOCUMENT (document), FALSE);
        g_return_val_if_fail (fd != -1, FALSE);
        g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
        g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
	EvDocumentPrivate *priv = GET_PRIVATE (document);

        klass = EV_DOCUMENT_GET_CLASS (document);
        if (!klass->load_fd) {
                g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                                     "Backend does not support loading from file descriptor");
                close (fd);
                return FALSE;
        }

        if (fstat(fd, &statbuf) == -1 ||
            (fd_flags = fcntl (fd, F_GETFL, &flags)) == -1) {
                int errsv = errno;
                g_set_error_literal (error, G_FILE_ERROR,
                                     g_file_error_from_errno (errsv),
                                     g_strerror (errsv));
                close (fd);
                return FALSE;
        }

        if (!S_ISREG(statbuf.st_mode)) {
                g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_BADF,
                                     "Not a regular file.");
                close (fd);
                return FALSE;
        }

        switch (fd_flags & O_ACCMODE) {
        case O_RDONLY:
        case O_RDWR:
                break;
        case O_WRONLY:
        default:
                g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_BADF,
                                     "Not a readable file descriptor.");
                close (fd);
                return FALSE;
        }

        if (!klass->load_fd (document, fd, flags, cancellable, error))
                return FALSE;

        priv->info = _ev_document_get_info (document);
        priv->n_pages = _ev_document_get_n_pages (document);

        if (!(flags & EV_DOCUMENT_LOAD_FLAG_NO_CACHE))
                ev_document_setup_cache (document);

        return TRUE;
}

/**
 * ev_document_save:
 * @document: a #EvDocument
 * @uri: the target URI
 * @error: a #GError location to store an error, or %NULL
 *
 * Saves @document to @uri.
 *
 * Returns: %TRUE on success, or %FALSE on error with @error filled in
 */
gboolean
ev_document_save (EvDocument  *document,
		  const char  *uri,
		  GError     **error)
{
	EvDocumentClass *klass = EV_DOCUMENT_GET_CLASS (document);

	return klass->save (document, uri, error);
}

/**
 * ev_document_get_page:
 * @document: a #EvDocument
 * @index: index of page
 *
 * Returns: (transfer full): Newly created #EvPage for the given index.
 */
EvPage *
ev_document_get_page (EvDocument *document,
		      gint        index)
{
	EvDocumentClass *klass = EV_DOCUMENT_GET_CLASS (document);

	return klass->get_page (document, index);
}

static gboolean
_ev_document_support_synctex (EvDocument *document)
{
	EvDocumentClass *klass = EV_DOCUMENT_GET_CLASS (document);

	return klass->support_synctex ? klass->support_synctex (document) : FALSE;
}

gboolean
ev_document_has_synctex (EvDocument *document)
{
	g_return_val_if_fail (EV_IS_DOCUMENT (document), FALSE);
	EvDocumentPrivate *priv = GET_PRIVATE (document);

	return priv->synctex_scanner != NULL;
}

/**
 * ev_document_synctex_backward_search:
 * @document: a #EvDocument
 * @page_index: the target page
 * @x: X coordinate
 * @y: Y coordinate
 *
 * Peforms a Synctex backward search to obtain the TeX input file, line and
 * (possibly) column  corresponding to the  position (@x,@y) (in 72dpi
 * coordinates) in the  @page of @document.
 *
 * Returns: A pointer to the EvSourceLink structure that holds the result. @NULL if synctex
 * is not enabled for the document or no result is found.
 * The EvSourceLink pointer should be freed with g_free after it is used.
 */
EvSourceLink *
ev_document_synctex_backward_search (EvDocument *document,
                                     gint        page_index,
                                     gfloat      x,
                                     gfloat      y)
{
        EvSourceLink *result = NULL;
        synctex_scanner_p scanner;

        g_return_val_if_fail (EV_IS_DOCUMENT (document), NULL);
	EvDocumentPrivate *priv = GET_PRIVATE (document);

        scanner = priv->synctex_scanner;
        if (!scanner)
                return NULL;

        if (synctex_edit_query (scanner, page_index + 1, x, y) > 0) {
                synctex_node_p node;

                /* We assume that a backward search returns either zero or one result_node */
                node = synctex_scanner_next_result (scanner);
                if (node != NULL) {
			const gchar *filename;

			filename = synctex_scanner_get_name (scanner, synctex_node_tag (node));

			if (filename) {
				result = ev_source_link_new (filename,
							     synctex_node_line (node),
							     synctex_node_column (node));
			}
                }
        }

        return result;
}

/**
 * ev_document_synctex_forward_search: (skip)
 * @document: a #EvDocument
 * @source_link: a #EvSourceLink
 *
 * Peforms a Synctex forward search to obtain the area in the document
 * corresponding to the position (line and column in @source_link) in
 * the source Tex file.
 *
 * Returns: An EvMapping with the page number and area corresponding to
 * the given line in the source file. It must be free with g_free when done
 */
EvMapping *
ev_document_synctex_forward_search (EvDocument   *document,
				    EvSourceLink *link)
{
        EvMapping        *result = NULL;
        synctex_scanner_p scanner;

        g_return_val_if_fail (EV_IS_DOCUMENT (document), NULL);
	EvDocumentPrivate *priv = GET_PRIVATE (document);

        scanner = priv->synctex_scanner;
        if (!scanner)
                return NULL;

	/* Since 1.19, synctex_display_query has a fourth parameter,
	 * page-hint, which we set into a dummy number to not break the
	 * API. In synctex it is used to set the best results first
	 * given the page-hint
	 */
        if (synctex_display_query (scanner, link->filename, link->line, link->col, 0) > 0) {
                synctex_node_p node;
                gint           page;

                if ((node = synctex_scanner_next_result (scanner))) {
                        result = g_new (EvMapping, 1);

                        page = synctex_node_page (node) - 1;
                        result->data = GINT_TO_POINTER (page);

                        result->area.x1 = synctex_node_box_visible_h (node);
                        result->area.y1 = synctex_node_box_visible_v (node) -
                                synctex_node_box_visible_height (node);
                        result->area.x2 = synctex_node_box_visible_width (node) + result->area.x1;
                        result->area.y2 = synctex_node_box_visible_depth (node) +
                                synctex_node_box_visible_height (node) + result->area.y1;
                }
        }

        return result;
}

static guint64
_ev_document_get_size_gfile (GFile *file)
{
	goffset    size = 0;
	GFileInfo *info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                             G_FILE_QUERY_INFO_NONE, NULL, NULL);
	if (info) {
		size = g_file_info_get_size (info);

		g_object_unref (info);
	}

	return size;
}

static guint64
_ev_document_get_size (const char  *uri)
{
	GFile  *file = g_file_new_for_uri (uri);
	guint64 size = _ev_document_get_size_gfile (file);

	g_object_unref (file);

	return size;
}

static gint
_ev_document_get_n_pages (EvDocument  *document)
{
	EvDocumentClass *klass = EV_DOCUMENT_GET_CLASS (document);

	return klass->get_n_pages (document);
}

gint
ev_document_get_n_pages (EvDocument  *document)
{
	g_return_val_if_fail (EV_IS_DOCUMENT (document), 0);
	EvDocumentPrivate *priv = GET_PRIVATE (document);

	return priv->n_pages;
}

static void
_ev_document_get_page_size (EvDocument *document,
			    EvPage     *page,
			    double     *width,
			    double     *height)
{
	EvDocumentClass *klass = EV_DOCUMENT_GET_CLASS (document);

	klass->get_page_size (document, page, width, height);
}

/**
 * ev_document_get_page_size:
 * @document: a #EvDocument
 * @page_index: index of page
 * @width: (out) (allow-none): return location for the width of the page, or %NULL
 * @height: (out) (allow-none): return location for the height of the page, or %NULL
 */
void
ev_document_get_page_size (EvDocument *document,
			   gint        page_index,
			   double     *width,
			   double     *height)
{
	EvDocumentPrivate *priv;

	g_return_if_fail (EV_IS_DOCUMENT (document));
	priv = GET_PRIVATE (document);
	g_return_if_fail (page_index >= 0 || page_index < priv->n_pages);

	if (priv->cache_loaded) {
		if (width)
			*width = priv->uniform ?
				priv->uniform_width :
				priv->page_sizes[page_index].width;
		if (height)
			*height = priv->uniform ?
				priv->uniform_height :
				priv->page_sizes[page_index].height;
	} else {
		EvPage *page;

		g_mutex_lock (&ev_doc_mutex);
		page = ev_document_get_page (document, page_index);
		_ev_document_get_page_size (document, page, width, height);
		g_object_unref (page);
		g_mutex_unlock (&ev_doc_mutex);
	}
}

static gchar *
_ev_document_get_page_label (EvDocument *document,
			     EvPage     *page)
{
	EvDocumentClass *klass = EV_DOCUMENT_GET_CLASS (document);

	return klass->get_page_label ?
		klass->get_page_label (document, page) : NULL;
}

gchar *
ev_document_get_page_label (EvDocument *document,
			    gint        page_index)
{
	EvDocumentPrivate *priv;
	g_return_val_if_fail (EV_IS_DOCUMENT (document), NULL);
	priv = GET_PRIVATE (document);
	g_return_val_if_fail (page_index >= 0 || page_index < priv->n_pages, NULL);

	if (!priv->cache_loaded) {
		EvPage *page;
		gchar *page_label;

		g_mutex_lock (&ev_doc_mutex);
		page = ev_document_get_page (document, page_index);
		page_label = _ev_document_get_page_label (document, page);
		g_object_unref (page);
		g_mutex_unlock (&ev_doc_mutex);

		return page_label ? page_label : g_strdup_printf ("%d", page_index + 1);
	}

	return (priv->page_labels && priv->page_labels[page_index]) ?
		g_strdup (priv->page_labels[page_index]) :
		g_strdup_printf ("%d", page_index + 1);
}

static EvDocumentInfo *
_ev_document_get_info (EvDocument *document)
{
	EvDocumentClass *klass = EV_DOCUMENT_GET_CLASS (document);

	return klass->get_info (document);
}

/**
 * ev_document_get_info:
 * @document: a #EvDocument
 *
 * Returns the #EvDocumentInfo for the document.
 *
 * Returns: (transfer none): a #EvDocumentInfo
 */
EvDocumentInfo *
ev_document_get_info (EvDocument *document)
{
	g_return_val_if_fail (EV_IS_DOCUMENT (document), NULL);
	EvDocumentPrivate *priv = GET_PRIVATE (document);

	return priv->info;
}

gboolean
ev_document_get_backend_info (EvDocument *document, EvDocumentBackendInfo *info)
{
	EvDocumentClass *klass;

	g_return_val_if_fail (EV_IS_DOCUMENT (document), FALSE);

	klass = EV_DOCUMENT_GET_CLASS (document);
	if (klass->get_backend_info == NULL)
		return FALSE;

	return klass->get_backend_info (document, info);
}

cairo_surface_t *
ev_document_render (EvDocument      *document,
		    EvRenderContext *rc)
{
	EvDocumentClass *klass = EV_DOCUMENT_GET_CLASS (document);

	return klass->render (document, rc);
}

static GdkPixbuf *
_ev_document_get_thumbnail (EvDocument      *document,
			    EvRenderContext *rc)
{
	cairo_surface_t *surface;
	GdkPixbuf       *pixbuf = NULL;

	surface = ev_document_render (document, rc);
	if (surface != NULL) {
		pixbuf = ev_document_misc_pixbuf_from_surface (surface);
		cairo_surface_destroy (surface);
	}

	return pixbuf;
}

/**
 * ev_document_get_thumbnail:
 * @document: an #EvDocument
 * @rc: an #EvRenderContext
 *
 * Returns: (transfer full): a #GdkPixbuf
 */
GdkPixbuf *
ev_document_get_thumbnail (EvDocument      *document,
			   EvRenderContext *rc)
{
	EvDocumentClass *klass = EV_DOCUMENT_GET_CLASS (document);

	if (klass->get_thumbnail)
		return klass->get_thumbnail (document, rc);

	return _ev_document_get_thumbnail (document, rc);
}

/**
 * ev_document_get_thumbnail_surface:
 * @document: an #EvDocument
 * @rc: an #EvRenderContext
 *
 * Returns: (transfer full): a #cairo_surface_t
 *
 * Since: 3.14
 */
cairo_surface_t *
ev_document_get_thumbnail_surface (EvDocument      *document,
				   EvRenderContext *rc)
{
	EvDocumentClass *klass = EV_DOCUMENT_GET_CLASS (document);

	if (klass->get_thumbnail_surface)
		return klass->get_thumbnail_surface (document, rc);

	return ev_document_render (document, rc);
}


const gchar *
ev_document_get_uri (EvDocument *document)
{
	g_return_val_if_fail (EV_IS_DOCUMENT (document), NULL);
	EvDocumentPrivate *priv = GET_PRIVATE (document);

	return priv->uri;
}

const gchar *
ev_document_get_title (EvDocument *document)
{
	g_return_val_if_fail (EV_IS_DOCUMENT (document), NULL);
	EvDocumentPrivate *priv = GET_PRIVATE (document);

	return (priv->info->fields_mask & EV_DOCUMENT_INFO_TITLE) ?
		priv->info->title : NULL;
}

gboolean
ev_document_is_page_size_uniform (EvDocument *document)
{
	g_return_val_if_fail (EV_IS_DOCUMENT (document), TRUE);
	EvDocumentPrivate *priv = GET_PRIVATE (document);

	if (!priv->cache_loaded) {
		g_mutex_lock (&ev_doc_mutex);
		ev_document_setup_cache (document);
		g_mutex_unlock (&ev_doc_mutex);
	}

	return priv->uniform;
}

void
ev_document_get_max_page_size (EvDocument *document,
			       gdouble    *width,
			       gdouble    *height)
{
	g_return_if_fail (EV_IS_DOCUMENT (document));
	EvDocumentPrivate *priv = GET_PRIVATE (document);

	if (!priv->cache_loaded) {
		g_mutex_lock (&ev_doc_mutex);
		ev_document_setup_cache (document);
		g_mutex_unlock (&ev_doc_mutex);
	}

	if (width)
		*width = priv->max_width;
	if (height)
		*height = priv->max_height;
}

void
ev_document_get_min_page_size (EvDocument *document,
			       gdouble    *width,
			       gdouble    *height)
{
	g_return_if_fail (EV_IS_DOCUMENT (document));
	EvDocumentPrivate *priv = GET_PRIVATE (document);

	if (!priv->cache_loaded) {
		g_mutex_lock (&ev_doc_mutex);
		ev_document_setup_cache (document);
		g_mutex_unlock (&ev_doc_mutex);
	}

	if (width)
		*width = priv->min_width;
	if (height)
		*height = priv->min_height;
}

gboolean
ev_document_check_dimensions (EvDocument *document)
{
	g_return_val_if_fail (EV_IS_DOCUMENT (document), FALSE);
	EvDocumentPrivate *priv = GET_PRIVATE (document);

	if (!priv->cache_loaded) {
		g_mutex_lock (&ev_doc_mutex);
		ev_document_setup_cache (document);
		g_mutex_unlock (&ev_doc_mutex);
	}

	return (priv->max_width > 0 && priv->max_height > 0);
}

guint64
ev_document_get_size (EvDocument *document)
{
	g_return_val_if_fail (EV_IS_DOCUMENT (document), 0);
	EvDocumentPrivate *priv = GET_PRIVATE (document);

	return priv->file_size;
}

gint
ev_document_get_max_label_len (EvDocument *document)
{
	g_return_val_if_fail (EV_IS_DOCUMENT (document), -1);
	EvDocumentPrivate *priv = GET_PRIVATE (document);

	if (!priv->cache_loaded) {
		g_mutex_lock (&ev_doc_mutex);
		ev_document_setup_cache (document);
		g_mutex_unlock (&ev_doc_mutex);
	}

	return priv->max_label;
}

gboolean
ev_document_has_text_page_labels (EvDocument *document)
{
	g_return_val_if_fail (EV_IS_DOCUMENT (document), FALSE);
	EvDocumentPrivate *priv = GET_PRIVATE (document);

	if (!priv->cache_loaded) {
		g_mutex_lock (&ev_doc_mutex);
		ev_document_setup_cache (document);
		g_mutex_unlock (&ev_doc_mutex);
	}

	return priv->page_labels != NULL;
}

gboolean
ev_document_find_page_by_label (EvDocument  *document,
				const gchar *page_label,
				gint        *page_index)
{
	gint i, page;
	glong value;
	gchar *endptr = NULL;
	EvDocumentPrivate *priv = GET_PRIVATE (document);

	g_return_val_if_fail (EV_IS_DOCUMENT (document), FALSE);
	g_return_val_if_fail (page_label != NULL, FALSE);
	g_return_val_if_fail (page_index != NULL, FALSE);

	if (!priv->cache_loaded) {
		g_mutex_lock (&ev_doc_mutex);
		ev_document_setup_cache (document);
		g_mutex_unlock (&ev_doc_mutex);
	}

        /* First, look for a literal label match */
	for (i = 0; priv->page_labels && i < priv->n_pages; i ++) {
		if (priv->page_labels[i] != NULL &&
		    ! strcmp (page_label, priv->page_labels[i])) {
			*page_index = i;
			return TRUE;
		}
	}

	/* Second, look for a match with case insensitively */
	for (i = 0; priv->page_labels && i < priv->n_pages; i++) {
		if (priv->page_labels[i] != NULL &&
		    ! strcasecmp (page_label, priv->page_labels[i])) {
			*page_index = i;
			return TRUE;
		}
	}

	/* Next, parse the label, and see if the number fits */
	value = strtol (page_label, &endptr, 10);
	if (endptr[0] == '\0') {
		/* Page number is an integer */
		page = MIN (G_MAXINT, value);

		/* convert from a page label to a page offset */
		page --;
		if (page >= 0 && page < priv->n_pages) {
			*page_index = page;
			return TRUE;
		}
	}

	return FALSE;
}

/* EvSourceLink */
G_DEFINE_BOXED_TYPE (EvSourceLink, ev_source_link, ev_source_link_copy, ev_source_link_free)

EvSourceLink *
ev_source_link_new (const gchar *filename,
		    gint 	 line,
		    gint 	 col)
{
	EvSourceLink *link = g_slice_new (EvSourceLink);

	link->filename = g_strdup (filename);
	link->line = line;
	link->col = col;

	return link;
}

EvSourceLink *
ev_source_link_copy (EvSourceLink *link)
{
	EvSourceLink *copy;

	g_return_val_if_fail (link != NULL, NULL);

	copy = g_slice_new (EvSourceLink);

	*copy = *link;
	copy->filename = g_strdup (link->filename);

	return copy;
}

void
ev_source_link_free (EvSourceLink *link)
{
	if (link == NULL)
		return;

	g_free (link->filename);
	g_slice_free (EvSourceLink, link);
}

/* EvRectangle */
G_DEFINE_BOXED_TYPE (EvRectangle, ev_rectangle, ev_rectangle_copy, ev_rectangle_free)

EvRectangle *
ev_rectangle_new (void)
{
	return g_new0 (EvRectangle, 1);
}

EvRectangle *
ev_rectangle_copy (EvRectangle *rectangle)
{
	EvRectangle *new_rectangle;

	g_return_val_if_fail (rectangle != NULL, NULL);

	new_rectangle = g_new (EvRectangle, 1);
	*new_rectangle = *rectangle;

	return new_rectangle;
}

void
ev_rectangle_free (EvRectangle *rectangle)
{
	g_free (rectangle);
}

/* Compares two rects.  returns 0 if they're equal */
#define EPSILON 0.0000001

gint
ev_rect_cmp (EvRectangle *a,
	     EvRectangle *b)
{
	if (a == b)
		return 0;
	if (a == NULL || b == NULL)
		return 1;

	return ! ((ABS (a->x1 - b->x1) < EPSILON) &&
		  (ABS (a->y1 - b->y1) < EPSILON) &&
		  (ABS (a->x2 - b->x2) < EPSILON) &&
		  (ABS (a->y2 - b->y2) < EPSILON));
}
