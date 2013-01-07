/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/*
 *  Copyright (C) 2005, Red Hat, Inc. 
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
#include "ev-module.h"

#include "ev-backends-manager.h"

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

        for (l = ev_backends_list; l; l = l->next) {
                EvBackendInfo *info = (EvBackendInfo *) l->data;
                char **mime_types = info->mime_types;
                guint i;

                for (i = 0; mime_types[i] != NULL; ++i) {
                        if (g_ascii_strcasecmp (mime_type, mime_types[i]) == 0)
                                return info;
                }
        }

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
        GTypeModule *module = NULL;

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
                gchar *path;

                path = g_module_build_path (ev_backends_dir, info->module_name);
                module = G_TYPE_MODULE (_ev_module_new (path, info->resident));
                g_free (path);

                if (ev_module_hash == NULL) {
                        ev_module_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                                g_free,
                                                                NULL /* leaked on purpose */);
                }
                g_hash_table_insert (ev_module_hash, g_strdup (info->module_name), module);
        }

        if (!g_type_module_use (module)) {
                const char *err;

                err = g_module_error ();
                g_set_error (error, EV_DOCUMENT_ERROR, EV_DOCUMENT_ERROR_INVALID,
                             "Failed to load backend for '%s': %s",
                             mime_type, err ? err : "unknown error");
                return NULL;
        }

        document = EV_DOCUMENT (_ev_module_new_object (EV_MODULE (module)));
        g_type_module_unuse (module);

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
	g_list_foreach (ev_backends_list, (GFunc) _ev_backend_info_unref, NULL);
	g_list_free (ev_backends_list);
	ev_backends_list = NULL;

	if (ev_module_hash != NULL) {
		g_hash_table_unref (ev_module_hash);
		ev_module_hash = NULL;
	}

	g_free (ev_backends_dir);
        ev_backends_dir = NULL;
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

		result = ev_document_load (document, uri_unc ? uri_unc : uri, &err);

		if (result == FALSE || err) {
			if (err &&
			    g_error_matches (err, EV_DOCUMENT_ERROR, EV_DOCUMENT_ERROR_ENCRYPTED)) {
				g_propagate_error (error, err);
				return document;
			    }
			/* else fall through to slow mime code section below */
		} else {
			return document;
		}

		g_object_unref (document);
		document = NULL;
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

	result = ev_document_load (document, uri_unc ? uri_unc : uri, &err);
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

		g_object_unref (document);
		document = NULL;

		g_propagate_error (error, err);
	}

	return document;
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
ev_document_factory_add_filters (GtkWidget *chooser, EvDocument *document)
{
	GtkFileFilter *filter;
	GtkFileFilter *default_filter;
	GtkFileFilter *document_filter;

        g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
        g_return_if_fail (document == NULL || EV_IS_DOCUMENT (document));

	default_filter = document_filter = filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("All Documents"));
	g_list_foreach (ev_backends_list, (GFunc)file_filter_add_mime_types, filter);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

	if (document) {
		EvBackendInfo *info;

		info = get_backend_info_for_document (document);
                g_assert (info != NULL);
		default_filter = filter = gtk_file_filter_new ();
		gtk_file_filter_set_name (filter, info->type_desc);
		file_filter_add_mime_types (info, filter);
		gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);
	} else {
		GList *l;

		for (l = ev_backends_list; l; l = l->next) {
                        EvBackendInfo *info = (EvBackendInfo *) l->data;

			default_filter = filter = gtk_file_filter_new ();
			gtk_file_filter_set_name (filter, info->type_desc);
			file_filter_add_mime_types (info, filter);
			gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);
		}
	}

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("All Files"));
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (chooser),
				     document == NULL ? document_filter : default_filter);
}

/* Deprecated API/ABI compatibility wrappers */

/**
 * ev_backends_manager_get_document:
 * @mime_type: a mime type hint
 *
 * Returns: (transfer full): a new #EvDocument
 */
EvDocument  *ev_backends_manager_get_document (const gchar *mime_type)
{
        return ev_document_factory_new_document_for_mime_type (mime_type, NULL);
}

const gchar *
ev_backends_manager_get_document_module_name (EvDocument  *document)
{
        EvBackendInfo *info = get_backend_info_for_document (document);
        if (info == NULL)
                return NULL;

        return info->module_name;
}

EvTypeInfo *ev_backends_manager_get_document_type_info (EvDocument  *document)
{
        return (EvTypeInfo *) get_backend_info_for_document (document);
}

GList *
ev_backends_manager_get_all_types_info       (void)
{
        return g_list_copy (ev_backends_list);
}
