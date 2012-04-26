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

#include "ev-backends-manager.h"
#include "ev-document-factory.h"
#include "ev-file-helpers.h"

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
 * get_document_from_uri:
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
get_document_from_uri (const char        *uri,
		       gboolean           fast,
		       EvCompressionType *compression,
		       GError           **error)
{
	EvDocument *document = NULL;
	gchar      *mime_type = NULL;
	GError     *err = NULL;

	*compression = EV_COMPRESSION_NONE;

	mime_type = ev_file_get_mime_type (uri, fast, &err);

	if (mime_type == NULL) {
		g_free (mime_type);

		if (err == NULL) {
			g_set_error_literal (error,
                                             EV_DOCUMENT_ERROR,
                                             EV_DOCUMENT_ERROR_INVALID,
                                             _("Unknown MIME Type"));
		} else {
			g_propagate_error (error, err);
		}
		
		return NULL;
	}

	document = ev_backends_manager_get_document (mime_type);
	if (document == NULL) {
		gchar *content_type, *mime_desc = NULL;

		content_type = g_content_type_from_mime_type (mime_type);
		if (content_type)
			mime_desc = g_content_type_get_description (content_type);

		g_set_error (error,
			     EV_DOCUMENT_ERROR,	
			     EV_DOCUMENT_ERROR_INVALID,
			     _("File type %s (%s) is not supported"),
			     mime_desc ? mime_desc : "-", mime_type);
		g_free (mime_desc);
		g_free (content_type);
		g_free (mime_type);

		return NULL;
	}

	*compression = get_compression_from_mime_type (mime_type);

	g_free (mime_type);
	
        return document;
}

/*
 * get_document_from_data:
 * @data: The contents of a file.
 * @length: The number of bytes in @data.
 * @compression: a location to store the document's compression type
 * @error: a #GError location to store an error, or %NULL
 *
 * Creates a #EvDocument instance for the document contents.
 * If a document could be created,
 * @compression is filled in with the document's compression type.
 * On error, %NULL is returned and @error filled in.
 *
 * Returns: a new #EvDocument instance, or %NULL on error with @error filled in
 */
static EvDocument *
get_document_from_data (const guchar      *data,
			gsize		   length,
		        EvCompressionType *compression,
		        GError           **error)
{
	EvDocument *document = NULL;
	gchar	   *content_type = NULL;
	gchar      *mime_type = NULL;

	*compression = EV_COMPRESSION_NONE;

	content_type = g_content_type_guess (NULL, data, length, NULL);
	if (!content_type) {
		g_set_error_literal (error,
                                     EV_DOCUMENT_ERROR,
                                     EV_DOCUMENT_ERROR_INVALID,
                                     _("Unknown MIME Type"));

		return NULL;
	}

	mime_type = g_content_type_get_mime_type (content_type);
	if (!mime_type) {
		g_set_error_literal (error,
                                     EV_DOCUMENT_ERROR,
                                     EV_DOCUMENT_ERROR_INVALID,
                                     _("Unknown MIME Type"));
		g_free (content_type);

		return NULL;
	}

	document = ev_backends_manager_get_document (mime_type);
	if (document == NULL) {
		gchar *mime_desc = NULL;

		mime_desc = g_content_type_get_description (content_type);

		g_set_error (error,
			     EV_DOCUMENT_ERROR,
			     EV_DOCUMENT_ERROR_INVALID,
			     _("File type %s (%s) is not supported"),
			     mime_desc ? mime_desc : "-", mime_type);
		g_free (mime_desc);
		g_free (content_type);
		g_free (mime_type);

		return NULL;
	}

	*compression = get_compression_from_mime_type (mime_type);

	g_free (content_type);
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

/* Try to get and load the document from a file, dealing with errors 
 * differently depending on whether we are using slow or fast mime detection.
 */
static EvDocument *
ev_document_factory_load_uri (const char *uri, gboolean fast, GError **error)
{
	EvDocument *document;
	int result;
	EvCompressionType compression;
	gchar *uri_unc = NULL;
	GError *err = NULL;

	document = get_document_from_uri (uri, fast, &compression, &err);
	g_assert (document != NULL || err != NULL);

	if (document == NULL) {
		if (fast) {
			/* Try again with slow mime detection */
			g_clear_error (&err);
		} else {
			/* This should have worked, and is usually the second try,
			 * so set an error for the caller. */
			g_assert (err != NULL);
			g_propagate_error (error, err);
		}
		
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
		g_object_unref (document);
		g_propagate_error (error, err);

		return NULL;
	}

	result = ev_document_load (document, uri_unc ? uri_unc : uri, &err);
	if (result)
		return document;
		
	if (err) {
		if (g_error_matches (err, EV_DOCUMENT_ERROR, EV_DOCUMENT_ERROR_ENCRYPTED)) {
			g_propagate_error (error, err);
			return document;

			/* else fall through to slow mime code detection. */
		}
	} else if (!fast) {
		/* FIXME: this really should not happen; the backend should
		 * always return a meaningful error.
		 */
		g_set_error_literal (&err,
			EV_DOCUMENT_ERROR,
			EV_DOCUMENT_ERROR_INVALID,
			_("Unknown MIME Type"));
		g_propagate_error (error, err);

		/* else fall through to slow mime code detection. */
	}

	g_object_unref (document);

	return NULL;
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
 * Returns: (transfer full): a new #EvDocument, or %NULL.
 */
EvDocument *
ev_document_factory_get_document (const char *uri, GError **error)
{
	EvDocument *document;

	g_return_val_if_fail (uri != NULL, NULL);

	document = ev_document_factory_load_uri (uri, TRUE, error);
	if (document)
		return document;

	/* Try again with slow mime detection */
	g_clear_error (error); /* Though this should always be NULL here. */
	document = ev_document_factory_load_uri (uri, FALSE, error);
	return document;
}

static EvDocument *
ev_document_factory_load_data (const guchar *data, gsize length, GError **error)
{
	EvDocument *document;
	int result;
	guchar* data_unc = NULL;
	gsize data_unc_length = 0;
	EvCompressionType compression;
	GError *err = NULL;

	document = get_document_from_data (data, length, &compression, &err);
	g_assert (document != NULL || err != NULL);

	if (document == NULL) {
		/* Set an error for the caller. */
		g_assert (err != NULL);
		g_propagate_error (error, err);

		return NULL;
	}

	/* TODO: Implement uncompress for data,
	 * returning data, not a URI.
	 * This currently uses command-line utilities on files.
	 * data_unc = ev_file_uncompress (data, length, compression, &data_unc_length, &err);
	 */
	if (data_unc) {
		g_object_set_data_full (G_OBJECT (document),
					"data-uncompressed",
					data_unc,
					g_free);
		g_object_set_data_full (G_OBJECT (document),
					"data-length-uncompressed",
					GSIZE_TO_POINTER(data_unc_length),
					(GDestroyNotify) NULL);
	} else if (err != NULL) {
		/* Error uncompressing file */
		g_object_unref (document);
		g_propagate_error (error, err);
		return NULL;
	}

	result = ev_document_load_from_data (document,
					     data_unc ? data_unc : data,
					     data_unc ? data_unc_length : length,
					     &err);
	if (result)
		return document;

	if (err) {
		if (g_error_matches (err, EV_DOCUMENT_ERROR, EV_DOCUMENT_ERROR_ENCRYPTED)) {
			g_propagate_error (error, err);
			return document;
		}
	} else {
		/* FIXME: this really should not happen; the backend should
		 * always return a meaningful error.
		 */
		g_set_error_literal (&err,
				     EV_DOCUMENT_ERROR,
				     EV_DOCUMENT_ERROR_INVALID,
				     _("Unknown MIME Type"));
		g_propagate_error (error, err);
	}

	g_object_unref (document);

	return NULL;
}

/**
 * ev_document_factory_get_document_from_data:
 * @data: The contents of a file.
 * @length: The number of bytes in @data.
 * @error: a #GError location to store an error, or %NULL
 *
 * Creates an #EvDocument for the document contents; or, if no backend handling
 * the document's type is found, or an error occurred on opening the document,
 * returns %NULL and fills in @error.
 * If the document is encrypted, it is returned but also @error is set to
 * %EV_DOCUMENT_ERROR_ENCRYPTED.
 *
 * Returns: (transfer full): a new #EvDocument, or %NULL.
 */
EvDocument *
ev_document_factory_get_document_from_data (const guchar *data, gsize length, GError **error)
{
	g_return_val_if_fail (data != NULL, NULL);
	g_return_val_if_fail (length != 0, NULL);

	/* Note that, unlike ev_document_factory_get_document() we can't use
	 * both slow and fast mime-type detection, so this is simpler.
	 */
	return ev_document_factory_load_data (data, length, error);
}

static void
file_filter_add_mime_types (EvTypeInfo *info, GtkFileFilter *filter)
{
	const gchar *mime_type;
	gint         i = 0;

	while ((mime_type = info->mime_types[i++]))
		gtk_file_filter_add_mime_type (filter, mime_type);
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
	GList         *all_types;
	GtkFileFilter *filter;
	GtkFileFilter *default_filter;
	GtkFileFilter *document_filter;

        g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));
        g_return_if_fail (document == NULL || EV_IS_DOCUMENT (document));

	all_types = ev_backends_manager_get_all_types_info ();
	
	default_filter = document_filter = filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("All Documents"));
	g_list_foreach (all_types, (GFunc)file_filter_add_mime_types, filter);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

	if (document) {
		EvTypeInfo *info;

		info = ev_backends_manager_get_document_type_info (document);
		default_filter = filter = gtk_file_filter_new ();
		gtk_file_filter_set_name (filter, info->desc);
		file_filter_add_mime_types (info, filter);
		g_free (info);
		gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);
	} else {
		GList *l;

		for (l = all_types; l; l = g_list_next (l)){
			EvTypeInfo *info;

			info = (EvTypeInfo *)l->data;

			default_filter = filter = gtk_file_filter_new ();
			gtk_file_filter_set_name (filter, info->desc);
			file_filter_add_mime_types (info, filter);
			gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);
		}
	}

	g_list_foreach (all_types, (GFunc)g_free, NULL);
	g_list_free (all_types);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("All Files"));
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (chooser),
				     document == NULL ? document_filter : default_filter);
}
