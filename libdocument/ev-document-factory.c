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

#ifdef ENABLE_PIXBUF
static GList*
gdk_pixbuf_mime_type_list ()
{
	GSList *formats, *list;
	GList *result = NULL;

	formats = gdk_pixbuf_get_formats ();
	for (list = formats; list != NULL; list = list->next) {
		GdkPixbufFormat *format = list->data;
		gchar          **mime_types;

		if (gdk_pixbuf_format_is_disabled (format))
			continue;

		mime_types = gdk_pixbuf_format_get_mime_types (format);
		result = g_list_prepend (result, mime_types); 
	}
	g_slist_free (formats);

	return result;
}

/* Would be nice to have this in gdk-pixbuf */
static gboolean
mime_type_supported_by_gdk_pixbuf (const gchar *mime_type)
{
	GList *mime_types;
	GList *list;
	gboolean retval = FALSE;

	mime_types = gdk_pixbuf_mime_type_list ();
	for (list = mime_types; list; list = list->next) {
		gchar      **mtypes = (gchar **)list->data;
		const gchar *mtype;
		gint         i = 0;

		while ((mtype = mtypes[i++])) {
			if (strcmp (mtype, mime_type) == 0) {
				retval = TRUE;
				break;
			}
		}
	}

	g_list_foreach (mime_types, (GFunc)g_strfreev, NULL);
	g_list_free (mime_types);

	return retval;
}
#endif /* ENABLE_PIXBUF */

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
	
#ifdef ENABLE_PIXBUF
	if (!document && mime_type_supported_by_gdk_pixbuf (mime_type))
		document = ev_backends_manager_get_document ("image/*");
#endif /* ENABLE_PIXBUF */

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

static void
free_uncompressed_uri (gchar *uri_unc)
{
	if (!uri_unc)
		return;

	ev_tmp_uri_unlink (uri_unc);
	g_free (uri_unc);
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
 * Returns: a new #EvDocument, or %NULL.
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

	document = get_document_from_uri (uri, TRUE, &compression, &err);
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

	document = get_document_from_uri (uri, FALSE, &compression, &err);
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

static void
file_filter_add_mime_types (EvTypeInfo *info, GtkFileFilter *filter)
{
	const gchar *mime_type;
	gint         i = 0;

#ifdef ENABLE_PIXBUF
	if (g_ascii_strcasecmp (info->mime_types[0], "image/*") == 0) {
		GList *pixbuf_types, *l;

		pixbuf_types = gdk_pixbuf_mime_type_list ();
		for (l = pixbuf_types; l; l = g_list_next (l)) {
			gchar **mime_types = (gchar **)l->data;
			gint    j = 0;
			
			while ((mime_type = mime_types[j++]))
				gtk_file_filter_add_mime_type (filter, mime_type);
			
			g_strfreev (mime_types);
		}
		g_list_free (pixbuf_types);

		return;
	}
#endif /* ENABLE_PIXBUF */
	
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
