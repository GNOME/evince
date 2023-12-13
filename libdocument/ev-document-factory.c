/*
 *  Copyright (C) 2005, Red Hat, Inc.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "ev-backend-info.h"
#include "ev-document-factory.h"
#include "ev-file-helpers.h"

/* Backends manager */

#define BACKEND_DATA_KEY "ev-backend-info"

static GList *ev_backends_list = NULL;
static GHashTable *ev_module_hash = NULL;
static gchar *ev_backends_dir = NULL;

static EvDocument* ev_document_factory_new_document_for_mime_type (const char *mime_type,
                                                                   GError **error);

static EvBackendInfo *
get_backend_info_for_mime_type (const gchar *mime_type)
{
        GList *l;
        gchar *content_type = g_content_type_from_mime_type (mime_type);

        for (l = ev_backends_list; l; l = l->next) {
                EvBackendInfo *info = (EvBackendInfo *) l->data;
                char **mime_types = info->mime_types;
                guint i;

                for (i = 0; mime_types[i] != NULL; ++i) {
                        if (g_content_type_is_mime_type (content_type, mime_types[i])) {
                                g_free (content_type);
                                return info;
                        }
                }
        }

        g_free (content_type);
        return NULL;
}

static EvBackendInfo *
get_backend_info_for_document (EvDocument *document)
{
        EvBackendInfo *info;

        info = g_object_get_data (G_OBJECT (document), BACKEND_DATA_KEY);

        g_warn_if_fail (info != NULL);
        return info;
}

static EvDocument *
ev_document_factory_new_document_for_mime_type (const gchar *mime_type,
                                                GError **error)
{
        EvDocument    *document;
        EvBackendInfo *info;
        GModule *module = NULL;
	GType backend_type;
	GType (*query_type_function) (void) = NULL;

        g_return_val_if_fail (mime_type != NULL, NULL);

        info = get_backend_info_for_mime_type (mime_type);
        if (info == NULL) {
                char *content_type, *mime_desc = NULL;

                content_type = g_content_type_from_mime_type (mime_type);
                if (content_type)
                        mime_desc = g_content_type_get_description (content_type);

                g_set_error (error,
                             EV_DOCUMENT_ERROR,
                             EV_DOCUMENT_ERROR_INVALID,
                             _("File type %s (%s) is not supported"),
                             mime_desc ? mime_desc : "(unknown)", mime_type);
                g_free (mime_desc);
                g_free (content_type);

                return NULL;
        }

        if (ev_module_hash != NULL) {
                module = g_hash_table_lookup (ev_module_hash, info->module_name);
        }
        if (module == NULL) {
		g_autofree gchar *path = NULL;

		path = g_strconcat (ev_backends_dir, G_DIR_SEPARATOR_S,
				    info->module_name, NULL);

		module = g_module_open (path, 0);

		if (!g_module_symbol (module, "ev_backend_query_type",
				      (void *) &query_type_function)) {
			const char *err = g_module_error ();
			g_set_error (error, EV_DOCUMENT_ERROR, EV_DOCUMENT_ERROR_INVALID,
				     "Failed to load backend for '%s': %s",
				     mime_type, err ? err : "unknown error");
			g_module_close (module);
			return NULL;
		}

		/* Make the module resident so it can’t be unloaded: without using a
		 * full #GTypePlugin implementation for the modules, it’s not safe to
		 * re-load a module and re-register its types with GObject, as that will
		 * confuse the GType system. */
		g_module_make_resident (module);

                if (ev_module_hash == NULL) {
                        ev_module_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                                g_free,
                                                                NULL /* leaked on purpose */);
                }
                g_hash_table_insert (ev_module_hash, g_strdup (info->module_name), module);
        }

	if (!query_type_function && !g_module_symbol (module, "ev_backend_query_type",
			      (void *) &query_type_function)) {
		const char *err = g_module_error ();
		g_set_error (error, EV_DOCUMENT_ERROR, EV_DOCUMENT_ERROR_INVALID,
			     "Failed to load backend for '%s': %s",
			     mime_type, err ? err : "unknown error");
		return NULL;
	}

	backend_type = query_type_function ();
	g_assert (g_type_is_a (backend_type, EV_TYPE_DOCUMENT));

	document = g_object_new (backend_type, NULL);

        g_object_set_data_full (G_OBJECT (document), BACKEND_DATA_KEY,
                                _ev_backend_info_ref (info),
                                (GDestroyNotify) _ev_backend_info_unref);

        return document;
}

static EvCompressionType
get_compression_from_mime_type (const gchar *mime_type)
{
	gchar type[3];
	gchar *p;

	if (!(p = g_strrstr (mime_type, "/")))
		return EV_COMPRESSION_NONE;

	if (sscanf (++p, "x-%2s%*s", type) == 1) {
		if (g_ascii_strcasecmp (type, "gz") == 0)
			return EV_COMPRESSION_GZIP;
		else if (g_ascii_strcasecmp (type, "bz") == 0)
			return EV_COMPRESSION_BZIP2;
                else if (g_ascii_strcasecmp (type, "xz") == 0)
                        return EV_COMPRESSION_LZMA;
	}

	return EV_COMPRESSION_NONE;
}


/*
 * new_document_for_uri:
 * @uri: the document URI
 * @fast: whether to use fast MIME type detection
 * @compression: a location to store the document's compression type
 * @error: a #GError location to store an error, or %NULL
 *
 * Creates a #EvDocument instance for the document at @uri, using either
 * fast or slow MIME type detection. If a document could be created,
 * @compression is filled in with the document's compression type.
 * On error, %NULL is returned and @error filled in.
 *
 * Returns: a new #EvDocument instance, or %NULL on error with @error filled in
 */
static EvDocument *
new_document_for_uri (const char        *uri,
                      gboolean           fast,
                      EvCompressionType *compression,
                      GError           **error)
{
	EvDocument *document = NULL;
	gchar      *mime_type = NULL;

	*compression = EV_COMPRESSION_NONE;

	mime_type = ev_file_get_mime_type (uri, fast, error);
	if (mime_type == NULL)
		return NULL;

	document = ev_document_factory_new_document_for_mime_type (mime_type, error);
	if (document == NULL)
                return NULL;

	*compression = get_compression_from_mime_type (mime_type);

	g_free (mime_type);

        return document;
}

static void
free_uncompressed_uri (gchar *uri_unc)
{
	if (!uri_unc)
		return;

	ev_tmp_uri_unlink (uri_unc);
	g_free (uri_unc);
}

/*
 * _ev_document_factory_init:
 *
 * Initializes the evince document factory.
 *
 * Returns: %TRUE if there were any backends found; %FALSE otherwise
 */
gboolean
_ev_document_factory_init (void)
{
	if (ev_backends_list)
		return TRUE;

        if (g_getenv ("EV_BACKENDS_DIR") != NULL)
                ev_backends_dir = g_strdup (g_getenv ("EV_BACKENDS_DIR"));

	if (!ev_backends_dir) {
#ifdef G_OS_WIN32
{
        gchar *dir;

        dir = g_win32_get_package_installation_directory_of_module (NULL);
        ev_backends_dir = g_build_filename (dir, "lib", "evince",
                                        EV_BACKENDSBINARYVERSION,
                                        "backends", NULL);
        g_free (dir);
}
#else
        ev_backends_dir = g_strdup (EV_BACKENDSDIR);
#endif
	}

        ev_backends_list = _ev_backend_info_load_from_dir (ev_backends_dir);

        return ev_backends_list != NULL;
}

/*
 * _ev_document_factory_shutdown:
 *
 * Shuts the evince document factory down.
 */
void
_ev_document_factory_shutdown (void)
{
	g_list_free_full (g_steal_pointer (&ev_backends_list), (GDestroyNotify) _ev_backend_info_unref);

	g_clear_pointer (&ev_module_hash, g_hash_table_unref);
	g_clear_pointer (&ev_backends_dir, g_free);
}

/**
 * ev_document_factory_get_document_full:
 * @uri: an URI
 * @flags: flags from #EvDocumentLoadFlags
 * @error: a #GError location to store an error, or %NULL
 *
 * Creates a #EvDocument for the document at @uri; or, if no backend handling
 * the document's type is found, or an error occurred on opening the document,
 * returns %NULL and fills in @error.
 * If the document is encrypted, it is returned but also @error is set to
 * %EV_DOCUMENT_ERROR_ENCRYPTED.
 *
 * Returns: (transfer full): a new #EvDocument, or %NULL
 */
EvDocument *
ev_document_factory_get_document_full (const char           *uri,
				       EvDocumentLoadFlags   flags,
				       GError              **error)
{
	EvDocument *document;
	int result;
	EvCompressionType compression;
	gchar *uri_unc = NULL;
	GError *err = NULL;

	g_return_val_if_fail (uri != NULL, NULL);

	document = new_document_for_uri (uri, TRUE, &compression, &err);
	g_assert (document != NULL || err != NULL);

	if (document != NULL) {
		uri_unc = ev_file_uncompress (uri, compression, &err);
		if (uri_unc) {
			g_object_set_data_full (G_OBJECT (document),
						"uri-uncompressed",
						uri_unc,
						(GDestroyNotify) free_uncompressed_uri);
		} else if (err != NULL) {
			/* Error uncompressing file */
			g_object_unref (document);
			g_propagate_error (error, err);
			return NULL;
		}

		result = ev_document_load_full (document, uri_unc ? uri_unc : uri, flags, &err);

		if (result == FALSE || err) {
			if (err &&
			    (g_error_matches (err, EV_DOCUMENT_ERROR, EV_DOCUMENT_ERROR_ENCRYPTED) ||
			     g_error_matches (err, EV_DOCUMENT_ERROR, EV_DOCUMENT_ERROR_UNSUPPORTED_CONTENT))) {
				g_propagate_error (error, err);
				return document;
			    }
			/* else fall through to slow mime code section below */
		} else {
			return document;
		}

		g_clear_object (&document);
	}

	/* Try again with slow mime detection */
	g_clear_error (&err);
	uri_unc = NULL;

	document = new_document_for_uri (uri, FALSE, &compression, &err);
	if (document == NULL) {
		g_assert (err != NULL);
		g_propagate_error (error, err);
		return NULL;
	}

	uri_unc = ev_file_uncompress (uri, compression, &err);
	if (uri_unc) {
		g_object_set_data_full (G_OBJECT (document),
					"uri-uncompressed",
					uri_unc,
					(GDestroyNotify) free_uncompressed_uri);
	} else if (err != NULL) {
		/* Error uncompressing file */
		g_propagate_error (error, err);

		g_object_unref (document);
		return NULL;
	}

	result = ev_document_load_full (document, uri_unc ? uri_unc : uri,
					EV_DOCUMENT_LOAD_FLAG_NONE, &err);
	if (result == FALSE) {
		if (err == NULL) {
			/* FIXME: this really should not happen; the backend should
			 * always return a meaningful error.
			 */
			g_set_error_literal (&err,
                                             EV_DOCUMENT_ERROR,
                                             EV_DOCUMENT_ERROR_INVALID,
                                             _("Unknown MIME Type"));
		} else if (g_error_matches (err, EV_DOCUMENT_ERROR, EV_DOCUMENT_ERROR_ENCRYPTED)) {
			g_propagate_error (error, err);
			return document;
		}

		g_clear_object (&document);
		g_propagate_error (error, err);
	}

	return document;
}

/**
 * ev_document_factory_get_document:
 * @uri: an URI
 * @error: a #GError location to store an error, or %NULL
 *
 * Creates a #EvDocument for the document at @uri; or, if no backend handling
 * the document's type is found, or an error occurred on opening the document,
 * returns %NULL and fills in @error.
 * If the document is encrypted, it is returned but also @error is set to
 * %EV_DOCUMENT_ERROR_ENCRYPTED.
 *
 * Returns: (transfer full): a new #EvDocument, or %NULL
 */
EvDocument *
ev_document_factory_get_document (const char *uri, GError **error)
{
	return ev_document_factory_get_document_full (uri,
						      EV_DOCUMENT_LOAD_FLAG_NONE,
						      error);
}

/**
 * ev_document_factory_get_document_for_gfile:
 * @file: a #GFile
 * @flags: flags from #EvDocumentLoadFlags
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @error: (allow-none): a #GError location to store an error, or %NULL
 *
 * Synchronously creates a #EvDocument for the document at @file; or, if no
 * backend handling the document's type is found, or an error occurred on
 * opening the document, returns %NULL and fills in @error.
 * If the document is encrypted, it is returned but also @error is set to
 * %EV_DOCUMENT_ERROR_ENCRYPTED.
 *
 * Returns: (transfer full): a new #EvDocument, or %NULL
 *
 * Since: 3.6
 */
EvDocument*
ev_document_factory_get_document_for_gfile (GFile *file,
                                            EvDocumentLoadFlags flags,
                                            GCancellable *cancellable,
                                            GError **error)
{
        EvDocument *document;
        GFileInfo *file_info;
        const char *content_type;
        char *mime_type = NULL;

        g_return_val_if_fail (G_IS_FILE (file), NULL);
        g_return_val_if_fail (error == NULL || *error == NULL, NULL);


        file_info = g_file_query_info (file,
                                       G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                       G_FILE_QUERY_INFO_NONE,
                                       cancellable,
                                       error);
        if (file_info == NULL)
                return NULL;

        content_type = g_file_info_get_content_type (file_info);
        if (content_type == NULL) {
                g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                                     "Failed to query file mime type");
                return NULL;
        }

        mime_type = g_content_type_get_mime_type (content_type);
        g_object_unref (file_info);

        document = ev_document_factory_new_document_for_mime_type (mime_type, error);
        g_free (mime_type);
        if (document == NULL)
                return NULL;

        if (!ev_document_load_gfile (document, file, flags, cancellable, error)) {
                g_object_unref (document);
                return NULL;
        }

        return document;
}

/**
 * ev_document_factory_get_document_for_stream:
 * @stream: a #GInputStream
 * @mime_type: (allow-none): a mime type hint
 * @flags: flags from #EvDocumentLoadFlags
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @error: (allow-none): a #GError location to store an error, or %NULL
 *
 * Synchronously creates a #EvDocument for the document from @stream; or, if no
 * backend handling the document's type is found, or an error occurred
 * on opening the document, returns %NULL and fills in @error.
 * If the document is encrypted, it is returned but also @error is set to
 * %EV_DOCUMENT_ERROR_ENCRYPTED.
 *
 * If @mime_type is non-%NULL, this overrides any type inferred from the stream.
 * If the mime type cannot be inferred from the stream, and @mime_type is %NULL,
 * an error is returned.
 *
 * Returns: (transfer full): a new #EvDocument, or %NULL
 *
 * Since: 3.6
 */
EvDocument*
ev_document_factory_get_document_for_stream (GInputStream *stream,
                                             const char *mime_type,
                                             EvDocumentLoadFlags flags,
                                             GCancellable *cancellable,
                                             GError **error)
{
        EvDocument *document;
        char *mime = NULL;

        g_return_val_if_fail (G_IS_INPUT_STREAM (stream), NULL);
        g_return_val_if_fail (error == NULL || *error == NULL, NULL);

        if (mime_type == NULL && G_IS_FILE_INPUT_STREAM (stream)) {
                GFileInfo *file_info;
                const char *content_type;

                file_info = g_file_input_stream_query_info (G_FILE_INPUT_STREAM (stream),
                                                            G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                                            cancellable,
                                                            error);
                if (file_info != NULL) {
                        content_type = g_file_info_get_content_type (file_info);
                        if (content_type)
                                mime_type = mime = g_content_type_get_mime_type (content_type);
                        g_object_unref (file_info);
                }
        }

        if (mime_type == NULL) {
                g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                                     "Cannot query mime type from stream");
                return NULL;
        }

        document = ev_document_factory_new_document_for_mime_type (mime_type, error);
        g_free (mime);

        if (document == NULL)
                return NULL;

        if (!ev_document_load_stream (document, stream, flags, cancellable, error)) {
                g_object_unref (document);
                return NULL;
        }

        return document;
}

/**
 * ev_document_factory_get_document_for_fd:
 * @fd: a file descriptor
 * @mime_type: the mime type
 * @flags: flags from #EvDocumentLoadFlags
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @error: (allow-none): a #GError location to store an error, or %NULL
 *
 * Synchronously creates a #EvDocument for the document from @fd using the backend
 * for loading documents of type @mime_type; or, if the backend does not support
 * loading from file descriptors, or an error occurred on opening the document,
 * returns %NULL and fills in @error.
 * If the document is encrypted, it is returned but also @error is set to
 * %EV_DOCUMENT_ERROR_ENCRYPTED.
 *
 * If the mime type cannot be inferred from the file descriptor, and @mime_type is %NULL,
 * an error is returned.
 *
 * Note that this function takes ownership of @fd; you must not ever
 * operate on it again. It will be closed automatically if the document
 * is destroyed, or if this function returns %NULL.
 *
 * Returns: (transfer full): a new #EvDocument, or %NULL
 *
 * Since: 42.0
 */
EvDocument*
ev_document_factory_get_document_for_fd (int fd,
                                         const char *mime_type,
                                         EvDocumentLoadFlags flags,
                                         GCancellable *cancellable,
                                         GError **error)
{
        EvDocument *document;

        g_return_val_if_fail (fd != -1, NULL);
        g_return_val_if_fail (error == NULL || *error == NULL, NULL);

        if (mime_type == NULL) {
                g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                                     "Cannot query mime type from file descriptor");
                close (fd);
                return NULL;
        }

        document = ev_document_factory_new_document_for_mime_type (mime_type, error);
        if (document == NULL) {
                close (fd);
                return NULL;
        }

        if (!ev_document_load_fd (document, fd, flags, cancellable, error)) {
                /* fd is now consumed */
                g_object_unref (document);
                return NULL;
        }

        return document;
}

static void
file_filter_add_mime_types (EvBackendInfo *info, GtkFileFilter *filter)
{
        char **mime_types;
	guint i;

        mime_types = info->mime_types;
        if (mime_types == NULL)
                return;

        for (i = 0; mime_types[i] != NULL; ++i)
                gtk_file_filter_add_mime_type (filter, mime_types[i]);
}

/**
 * ev_document_factory_add_filters:
 * @chooser: a #GtkFileChooser
 * @document: a #EvDocument, or %NULL
 *
 * Adds some file filters to @chooser.

 * Always add a "All documents" format.
 *
 * If @document is not %NULL, adds a #GtkFileFilter for @document's MIME type.
 *
 * If @document is %NULL, adds a #GtkFileFilter for each document type that evince
 * can handle.
 */
void
ev_document_factory_add_filters (GtkFileChooser *chooser, EvDocument *document)
{
	GtkFileFilter *filter;
	GtkFileFilter *default_filter;
	GtkFileFilter *document_filter;

        g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
        g_return_if_fail (document == NULL || EV_IS_DOCUMENT (document));

	default_filter = document_filter = filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("All Documents"));
	g_list_foreach (ev_backends_list, (GFunc)file_filter_add_mime_types, filter);
	gtk_file_chooser_add_filter (chooser, filter);

	if (document) {
		EvBackendInfo *info;

		info = get_backend_info_for_document (document);
                g_assert (info != NULL);
		default_filter = filter = gtk_file_filter_new ();
		gtk_file_filter_set_name (filter, info->type_desc);
		file_filter_add_mime_types (info, filter);
		gtk_file_chooser_add_filter (chooser, filter);
	} else {
		GList *l;

		for (l = ev_backends_list; l; l = l->next) {
                        EvBackendInfo *info = (EvBackendInfo *) l->data;

			default_filter = filter = gtk_file_filter_new ();
			gtk_file_filter_set_name (filter, info->type_desc);
			file_filter_add_mime_types (info, filter);
			gtk_file_chooser_add_filter (chooser, filter);
		}
	}

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("All Files"));
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_chooser_add_filter (chooser, filter);

	gtk_file_chooser_set_filter (chooser,
				     document == NULL ? document_filter : default_filter);
}
