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

#include "ev-document-types.h"

/* The various document type backends: */
#include "ev-poppler.h"
#include "pixbuf-document.h"
#include "tiff-document.h"
#include "ps-document.h"
#ifdef ENABLE_DVI
#include "dvi-document.h"
#endif
#ifdef ENABLE_DJVU
#include "djvu-document.h"
#endif

#include <string.h>
#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <gtk/gtkfilechooserdialog.h>

typedef struct _EvDocumentType EvDocumentType;
struct _EvDocumentType
{
	const char *mime_type;
	GType (*document_type_factory_callback)();
};

const EvDocumentType document_types[] = {
	/* PDF: */
	{"application/pdf",            pdf_document_get_type},

	/* Postscript: */
	{"application/postscript",     ps_document_get_type},
	{"application/x-gzpostscript", ps_document_get_type},
	{"image/x-eps",                ps_document_get_type},

#ifdef ENABLE_TIFF
	/* Tiff: */
	{"image/tiff",                 tiff_document_get_type},
#endif

#ifdef ENABLE_DJVU
	/* djvu: */
	{"image/vnd.djvu",             djvu_document_get_type},
#endif		

#ifdef ENABLE_DVI
	/* dvi: */
	{"application/x-dvi",          dvi_document_get_type},
#endif
};

/* Would be nice to have this in gdk-pixbuf */
static gboolean
mime_type_supported_by_gdk_pixbuf (const gchar *mime_type)
{
	GSList *formats, *list;
	gboolean retval = FALSE;

	formats = gdk_pixbuf_get_formats ();

	list = formats;
	while (list) {
		GdkPixbufFormat *format = list->data;
		int i;
		gchar **mime_types;

		if (gdk_pixbuf_format_is_disabled (format))
			continue;

		mime_types = gdk_pixbuf_format_get_mime_types (format);

		for (i = 0; mime_types[i] != NULL; i++) {
			if (strcmp (mime_types[i], mime_type) == 0) {
				retval = TRUE;
				break;
			}
		}

		if (retval)
			break;

		list = list->next;
	}

	g_slist_free (formats);

	return retval;
}


static GType
ev_document_type_from_from_mime (const char *mime_type)
{
	int i;
	
	g_return_val_if_fail (mime_type, G_TYPE_INVALID);

	for (i = 0; i < G_N_ELEMENTS (document_types); i++) {
		if (strcmp (mime_type, document_types[i].mime_type) == 0) {
			g_assert (document_types[i].document_type_factory_callback != NULL);
			return document_types[i].document_type_factory_callback();
		}
	}

	if (mime_type_supported_by_gdk_pixbuf (mime_type)) {
		return pixbuf_document_get_type ();
	}

	return G_TYPE_INVALID;
}

/**
 * ev_document_type_get_type:
 * @uri: String with uri
 * @slow: Do we need to check slow gnome-vfs mime type
 * @mime_type: If we've found handled type, the mime_type string is returned here.
 * @error: Information about error occured
 * 
 * Return value: G_TYPE_INVALID on error, G_TYPE_NONE when we are not sure about
 * mime type, and type of EvDocument implementation when we've found document.
 **/
static GType
ev_document_type_get_type (const char *uri, gboolean slow, gchar **mime_type, GError **error)
{
        GnomeVFSFileInfo *info;
        GnomeVFSResult result;

        GType type = G_TYPE_INVALID;
	
        info = gnome_vfs_file_info_new ();
        result = gnome_vfs_get_file_info (uri, info,
	    			          GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
		                          GNOME_VFS_FILE_INFO_FOLLOW_LINKS | 
					  (slow ? GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE : 0));
        if (result != GNOME_VFS_OK) {
		g_set_error (error,
			     EV_DOCUMENT_ERROR,
			     0,
			     gnome_vfs_result_to_string (result));			
		gnome_vfs_file_info_unref (info);
		return G_TYPE_INVALID;
        } 
	
	if (info->mime_type == NULL) {
		g_set_error (error,
			     EV_DOCUMENT_ERROR,	
    			     0,
			     _("Unknown MIME Type"));
		gnome_vfs_file_info_unref (info);
		return slow ? G_TYPE_INVALID : G_TYPE_NONE;
	}
	
	type = ev_document_type_from_from_mime (info->mime_type);
	
	if (type == G_TYPE_INVALID) {
		g_set_error (error,
			     EV_DOCUMENT_ERROR,	
			     0,
			     _("Unhandled MIME type: '%s'"), info->mime_type);
		gnome_vfs_file_info_unref (info);
		return slow ? G_TYPE_INVALID : G_TYPE_NONE;
	}			

	if (mime_type != NULL) {
		    *mime_type = g_strdup (info->mime_type);
	}
        gnome_vfs_file_info_unref (info);
	
        return type;
}

GType
ev_document_type_lookup (const char *uri, gchar **mime_type, GError **error)
{
	GType type = G_TYPE_INVALID;
	
	type = ev_document_type_get_type (uri, FALSE, mime_type, error);

	if (type != G_TYPE_NONE)
		return type;
		
	if (error) {
		g_error_free (*error);
		*error = NULL;
	}

	type = ev_document_type_get_type (uri, TRUE, mime_type, error);

	return type;
}

void 
ev_document_types_add_filters (GtkWidget *chooser)
{
	GtkFileFilter *documents_filter;
	GtkFileFilter *pdf_filter;
	GtkFileFilter *ps_filter;
	GtkFileFilter *pixbuf_filter;
	GtkFileFilter *all_filter;
#ifdef ENABLE_DJVU
	GtkFileFilter *djvu_filter;
#endif
#ifdef ENABLE_DVI
	GtkFileFilter *dvi_filter;
#endif

	documents_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (documents_filter,
				  _("All Documents"));
	gtk_file_filter_add_mime_type (documents_filter, "application/postscript");
	gtk_file_filter_add_mime_type (documents_filter, "application/x-gzpostscript");
	gtk_file_filter_add_mime_type (documents_filter, "image/x-eps");
	gtk_file_filter_add_mime_type (documents_filter, "application/pdf");
#ifdef ENABLE_DVI
	gtk_file_filter_add_mime_type (documents_filter, "application/x-dvi");
#endif
	gtk_file_filter_add_pixbuf_formats (documents_filter);
#ifdef ENABLE_DJVU
	gtk_file_filter_add_mime_type (documents_filter, "image/vnd.djvu");
#endif
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), documents_filter);

	ps_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (ps_filter, _("PostScript Documents"));
	gtk_file_filter_add_mime_type (ps_filter, "application/postscript");
	gtk_file_filter_add_mime_type (ps_filter, "application/x-gzpostscript");
	gtk_file_filter_add_mime_type (ps_filter, "image/x-eps");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), ps_filter);

	pdf_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (pdf_filter, _("PDF Documents"));
	gtk_file_filter_add_mime_type (pdf_filter, "application/pdf");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), pdf_filter);

#ifdef ENABLE_DVI
	dvi_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (dvi_filter, _("DVI Documents"));
	gtk_file_filter_add_mime_type (dvi_filter, "application/x-dvi");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), dvi_filter);
#endif

	pixbuf_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (pixbuf_filter, _("Images"));
	gtk_file_filter_add_pixbuf_formats (pixbuf_filter);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), pixbuf_filter);

#ifdef ENABLE_DJVU
	djvu_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (djvu_filter, _("Djvu Documents"));
	gtk_file_filter_add_mime_type (djvu_filter, "image/vnd.djvu");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), djvu_filter);
#endif	
	
	all_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (all_filter, _("All Files"));
	gtk_file_filter_add_pattern (all_filter, "*");

	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), all_filter);
	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (chooser), documents_filter);
}

void 
ev_document_types_add_filters_for_type (GtkWidget *chooser, GType type)
{
	GtkFileFilter *documents_filter;
	GtkFileFilter *pdf_filter;
	GtkFileFilter *ps_filter;
	GtkFileFilter *pixbuf_filter;
	GtkFileFilter *all_filter;
#ifdef ENABLE_DJVU
	GtkFileFilter *djvu_filter;
#endif
#ifdef ENABLE_DVI
	GtkFileFilter *dvi_filter;
#endif
	GtkFileFilter *default_filter;

	documents_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (documents_filter,
				  _("All Documents"));
	gtk_file_filter_add_mime_type (documents_filter, "application/postscript");
	gtk_file_filter_add_mime_type (documents_filter, "application/x-gzpostscript");
	gtk_file_filter_add_mime_type (documents_filter, "image/x-eps");
	gtk_file_filter_add_mime_type (documents_filter, "application/pdf");
#ifdef ENABLE_DVI
	gtk_file_filter_add_mime_type (documents_filter, "application/x-dvi");
#endif
	gtk_file_filter_add_pixbuf_formats (documents_filter);
#ifdef ENABLE_DJVU
	gtk_file_filter_add_mime_type (documents_filter, "image/vnd.djvu");
#endif
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), documents_filter);

	if (type == PS_TYPE_DOCUMENT) {
        	ps_filter = gtk_file_filter_new ();
		gtk_file_filter_set_name (ps_filter, _("PostScript Documents"));
		gtk_file_filter_add_mime_type (ps_filter, "application/postscript");
		gtk_file_filter_add_mime_type (ps_filter, "application/x-gzpostscript");
		gtk_file_filter_add_mime_type (ps_filter, "image/x-eps");
		gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), ps_filter);
		default_filter = ps_filter;
	} else if (type == PDF_TYPE_DOCUMENT) {
		pdf_filter = gtk_file_filter_new ();
		gtk_file_filter_set_name (pdf_filter, _("PDF Documents"));
		gtk_file_filter_add_mime_type (pdf_filter, "application/pdf");
		gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), pdf_filter);
		default_filter = pdf_filter;
#ifdef ENABLE_DVI
	} else if (type == DVI_TYPE_DOCUMENT) {
    	    	dvi_filter = gtk_file_filter_new ();
		gtk_file_filter_set_name (dvi_filter, _("DVI Documents"));
		gtk_file_filter_add_mime_type (dvi_filter, "application/x-dvi");
		gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), dvi_filter);
		default_filter = dvi_filter;
#endif
	} else if (type == PIXBUF_TYPE_DOCUMENT) {
		pixbuf_filter = gtk_file_filter_new ();
		gtk_file_filter_set_name (pixbuf_filter, _("Images"));
		gtk_file_filter_add_pixbuf_formats (pixbuf_filter);
		gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), pixbuf_filter);
		default_filter = pixbuf_filter;
#ifdef ENABLE_DJVU
	} else if (type == DJVU_TYPE_DOCUMENT) {
		djvu_filter = gtk_file_filter_new ();
		gtk_file_filter_set_name (djvu_filter, _("Djvu Documents"));
	    	gtk_file_filter_add_mime_type (djvu_filter, "image/vnd.djvu");
	    	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), djvu_filter);
		default_filter = djvu_filter;
#endif	
	} else {
		default_filter = documents_filter;
	}

	all_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (all_filter, _("All Files"));
	gtk_file_filter_add_pattern (all_filter, "*");

	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), all_filter);
	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (chooser), default_filter);
}

