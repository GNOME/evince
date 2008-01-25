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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gtk/gtkfilechooserdialog.h>

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

static EvDocument *
get_document_from_uri (const char        *uri,
		       gboolean           slow,
		       EvCompressionType *compression,
		       GError           **error)
{
	EvDocument *document = NULL;
	GFile *file;
	GFileInfo *file_info;
	const gchar *mime_type;

	*compression = EV_COMPRESSION_NONE;

	file = g_file_new_for_uri (uri);
	file_info = g_file_query_info (file,
				       G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				       0, NULL, NULL);
	g_object_unref (file);

	if (file_info == NULL) {
		g_set_error (error,
			     EV_DOCUMENT_ERROR,
			     0,
			     _("Failed to get info for document"));			
		return NULL;
	}
	mime_type = g_file_info_get_content_type (file_info);

	if (mime_type == NULL) {
		g_set_error (error,
			     EV_DOCUMENT_ERROR,	
    			     0,
			     _("Unknown MIME Type"));
		g_object_unref (file_info);
		return NULL;
	}

#ifdef ENABLE_PIXBUF
	if (mime_type_supported_by_gdk_pixbuf (mime_type))
		document = ev_backends_manager_get_document ("image/*");
	else
		document = ev_backends_manager_get_document (mime_type);
#else
	document = ev_backends_manager_get_document (mime_type);
#endif /* ENABLE_PIXBUF */

	if (document == NULL) {
		g_set_error (error,
			     EV_DOCUMENT_ERROR,	
			     0,
			     _("Unhandled MIME type: “%s”"), mime_type);
		g_object_unref (file_info);
		return NULL;
	}

	*compression = get_compression_from_mime_type (mime_type);

        g_object_unref (file_info);
	
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

EvDocument *
ev_document_factory_get_document (const char *uri, GError **error)
{
	EvDocument *document;
	int result;
	EvCompressionType compression;
	gchar *uri_unc = NULL;

	document = get_document_from_uri (uri, FALSE, &compression, error);
	if (*error == NULL) {
		uri_unc = ev_file_uncompress (uri, compression, error);
		if (uri_unc) {
			g_object_set_data_full (G_OBJECT (document),
						"uri-uncompressed",
						uri_unc,
						(GDestroyNotify) free_uncompressed_uri);
		}

		if (*error != NULL) {
			/* Error uncompressing file */
			if (document)
				g_object_unref (document);
			return NULL;
		}

		result = ev_document_load (document, uri_unc ? uri_unc : uri, error);

		if (result == FALSE || *error) {
			if (*error &&
			    (*error)->domain == EV_DOCUMENT_ERROR &&
			    (*error)->code == EV_DOCUMENT_ERROR_ENCRYPTED)
				return document;
		} else {
			return document;
		}
	}
	
	/* Try again with slow mime detection */
	if (document)
		g_object_unref (document);
	document = NULL;

	if (*error)
		g_error_free (*error);
	*error = NULL;

	uri_unc = NULL;

	document = get_document_from_uri (uri, TRUE, &compression, error);

	if (*error != NULL) {
		return NULL;
	}

	uri_unc = ev_file_uncompress (uri, compression, error);
	if (uri_unc) {
		g_object_set_data_full (G_OBJECT (document),
					"uri-uncompressed",
					uri_unc,
					(GDestroyNotify) free_uncompressed_uri);
	}

	if (*error != NULL) {
		/* Error uncompressing file */
		if (document)
			g_object_unref (document);
		return NULL;
	}
	
	result = ev_document_load (document, uri_unc ? uri_unc : uri, error);

	if (result == FALSE) {
		if (*error == NULL) {
			g_set_error (error,
				     EV_DOCUMENT_ERROR,
				     0,
				     _("Unknown MIME Type"));
		} else if ((*error)->domain == EV_DOCUMENT_ERROR &&
			   (*error)->code == EV_DOCUMENT_ERROR_ENCRYPTED) {
			return document;
		}

		if (document)
			g_object_unref (document);
		document = NULL;
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

void 
ev_document_factory_add_filters (GtkWidget *chooser, EvDocument *document)
{
	GList         *all_types;
	GtkFileFilter *filter;
	GtkFileFilter *default_filter;
	GtkFileFilter *document_filter;

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
