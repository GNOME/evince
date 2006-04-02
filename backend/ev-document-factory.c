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

#include "ev-document-factory.h"

/* The various document type backends: */
#include "ev-poppler.h"
#include "pixbuf-document.h"
#include "tiff-document.h"
#ifdef ENABLE_PS
#include "ps-document.h"
#endif
#ifdef ENABLE_DVI
#include "dvi-document.h"
#endif
#ifdef ENABLE_DJVU
#include "djvu-document.h"
#endif
#ifdef ENABLE_COMICS
#include "comics-document.h"
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
	EvBackend backend;
	GType (*document_type_factory_callback)();
};

const EvDocumentType document_types[] = {
	/* PDF: */
	{"application/pdf",            EV_BACKEND_PDF,  pdf_document_get_type},

#ifdef ENABLE_PS
	/* Postscript: */
	{"application/postscript",     EV_BACKEND_PS,   ps_document_get_type},
	{"application/x-gzpostscript", EV_BACKEND_PS,   ps_document_get_type},
	{"image/x-eps",                EV_BACKEND_PS,   ps_document_get_type},
#endif

#ifdef ENABLE_TIFF
	/* Tiff: */
	{"image/tiff",                 EV_BACKEND_TIFF, tiff_document_get_type},
#endif

#ifdef ENABLE_DJVU
	/* djvu: */
	{"image/vnd.djvu",             EV_BACKEND_DJVU, djvu_document_get_type},
#endif		

#ifdef ENABLE_DVI
	/* dvi: */
	{"application/x-dvi",          EV_BACKEND_DVI,  dvi_document_get_type},
#endif

#ifdef ENABLE_COMICS
	/* cbr/cbz: */
	{"application/x-cbr",           EV_BACKEND_COMICS,  comics_document_get_type},
	{"application/x-cbz",           EV_BACKEND_COMICS,  comics_document_get_type},
#endif
};

#ifdef ENABLE_PIXBUF

static GList*
gdk_pixbuf_mime_type_list ()
{
	GSList *formats, *list;
	GList *result;

	formats = gdk_pixbuf_get_formats ();
	result = NULL;

	for (list = formats; list != NULL; list = list->next) {
		GdkPixbufFormat *format = list->data;
		int i;
		gchar **mime_types;

		if (gdk_pixbuf_format_is_disabled (format))
			continue;

		mime_types = gdk_pixbuf_format_get_mime_types (format);

		for (i = 0; mime_types[i] != NULL; i++) {
			result = g_list_append (result, mime_types[i]);
		}
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
		if (strcmp ((char *)list->data, mime_type) == 0) {
			retval = TRUE;
			break;
		}
	}
	
	g_list_foreach (mime_types, (GFunc)g_free, NULL);
	g_list_free (mime_types);

	return retval;
}
#endif

static EvDocument*
ev_document_factory_get_from_mime (const char *mime_type)
{
	int i;
	GType type = G_TYPE_INVALID;
	EvDocument *document = NULL;
	
	g_return_val_if_fail (mime_type, G_TYPE_INVALID);

	for (i = 0; i < G_N_ELEMENTS (document_types); i++) {
		if (strcmp (mime_type, document_types[i].mime_type) == 0) {
			g_assert (document_types[i].document_type_factory_callback != NULL);
			type = document_types[i].document_type_factory_callback();
			break;
		}
	}
#ifdef ENABLE_PIXBUF
	if (type == G_TYPE_INVALID && mime_type_supported_by_gdk_pixbuf (mime_type)) {
		type = pixbuf_document_get_type ();
	}
#endif
	if (type != G_TYPE_INVALID) {
		document = g_object_new (type, NULL);
	} 

	return document;
}

EvBackend
ev_document_factory_get_backend (EvDocument *document)
{
	int i;

	for (i = 0; i < G_N_ELEMENTS (document_types); i++) {
		GType type = document_types[i].document_type_factory_callback ();
		if (type == G_TYPE_FROM_INSTANCE (document)) {
			return  document_types[i].backend;
		}
	}

#ifdef ENABLE_PIXBUF
	if (G_TYPE_FROM_INSTANCE (document) == pixbuf_document_get_type ())
	        return EV_BACKEND_PIXBUF;
#endif
	g_assert_not_reached ();
	
	return 0;
}

static GList *
ev_document_factory_get_mime_types (EvBackend backend)
{
	GList *types = NULL;
	int i;
	
#ifdef ENABLE_PIXBUF
	if (backend == EV_BACKEND_PIXBUF) {
		return gdk_pixbuf_mime_type_list ();
	}
#endif
	
	for (i = 0; i < G_N_ELEMENTS (document_types); i++) {
		if (document_types[i].backend == backend) {
			types = g_list_append (types, g_strdup (document_types[i].mime_type));
		}
	}

	return types;
}

static GList *
ev_document_factory_get_all_mime_types (void)
{
	GList *types = NULL;
	int i;
	
	for (i = 0; i < G_N_ELEMENTS (document_types); i++) {
		types = g_list_append (types, g_strdup (document_types[i].mime_type));
	}
	
#ifdef ENABLE_PIXBUF
	types = g_list_concat (types, gdk_pixbuf_mime_type_list ());
#endif

	return types;
}

static EvDocument *
get_document_from_uri (const char *uri, gboolean slow, GError **error)
{
	EvDocument *document = NULL;

        GnomeVFSFileInfo *info;
        GnomeVFSResult result;

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
		return NULL;
        } 
	
	if (info->mime_type == NULL) {
		g_set_error (error,
			     EV_DOCUMENT_ERROR,	
    			     0,
			     _("Unknown MIME Type"));
		gnome_vfs_file_info_unref (info);
		return NULL;
	}
	
	document = ev_document_factory_get_from_mime (info->mime_type);
		
	if (document == NULL) {
		g_set_error (error,
			     EV_DOCUMENT_ERROR,	
			     0,
			     _("Unhandled MIME type: '%s'"), info->mime_type);
		gnome_vfs_file_info_unref (info);
		return NULL;
	}			

        gnome_vfs_file_info_unref (info);
	
        return document;
}

EvDocument *
ev_document_factory_get_document (const char *uri, GError **error)
{
	EvDocument *document;
	
	document = get_document_from_uri (uri, FALSE, error);

	if (*error != NULL) {
		return NULL;
	}

	ev_document_load (document, uri, error);
		
	if (*error) {
		g_error_free (*error);
		*error = NULL;
	}

	document = get_document_from_uri (uri, TRUE, error);

	if (*error != NULL) {
		return NULL;
	}

	ev_document_load (document, uri, error);

	return document;
}

static void
file_filter_add_mime_list_and_free (GtkFileFilter *filter, GList *mime_types)
{
	GList *l;

	for (l = mime_types; l != NULL; l = l->next) {
		gtk_file_filter_add_mime_type (filter, l->data);
	}

	g_list_foreach (mime_types, (GFunc)g_free, NULL);
	g_list_free (mime_types);
}

void 
ev_document_factory_add_filters (GtkWidget *chooser, EvDocument *document)
{
	EvBackend backend = 0;
	GList *mime_types;
	GtkFileFilter *filter;
	GtkFileFilter *default_filter;
	GtkFileFilter *document_filter;

	if (document != NULL) {
		backend = ev_document_factory_get_backend (document);
	}

	default_filter = document_filter = filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("All Documents"));
	mime_types = ev_document_factory_get_all_mime_types ();
	file_filter_add_mime_list_and_free (filter, mime_types);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

#ifdef ENABLE_PS
	if (document == NULL || backend == EV_BACKEND_PS) {
		default_filter = filter = gtk_file_filter_new ();
		gtk_file_filter_set_name (filter, _("PostScript Documents"));
		mime_types = ev_document_factory_get_mime_types (EV_BACKEND_PS);
		file_filter_add_mime_list_and_free (filter, mime_types);
		gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);
	}
#endif

	if (document == NULL || backend == EV_BACKEND_PDF) {
		default_filter = filter = gtk_file_filter_new ();
		gtk_file_filter_set_name (filter, _("PDF Documents"));
		mime_types = ev_document_factory_get_mime_types (EV_BACKEND_PDF);
		file_filter_add_mime_list_and_free (filter, mime_types);
		gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);
	}

#ifdef ENABLE_PIXBUF
	if (document == NULL || backend == EV_BACKEND_PIXBUF) {
		default_filter = filter = gtk_file_filter_new ();
		gtk_file_filter_set_name (filter, _("Images"));
		mime_types = ev_document_factory_get_mime_types (EV_BACKEND_PIXBUF);
		file_filter_add_mime_list_and_free (filter, mime_types);
		gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);
	}
#endif

#ifdef ENABLE_DVI
	if (document == NULL || backend == EV_BACKEND_DVI) {
		default_filter = filter = gtk_file_filter_new ();
		gtk_file_filter_set_name (filter, _("DVI Documents"));
		mime_types = ev_document_factory_get_mime_types (EV_BACKEND_DVI);
		file_filter_add_mime_list_and_free (filter, mime_types);
		gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);
	}
#endif

#ifdef ENABLE_DJVU
	if (document == NULL || backend == EV_BACKEND_DJVU) {
		default_filter = filter = gtk_file_filter_new ();
		gtk_file_filter_set_name (filter, _("Djvu Documents"));
		mime_types = ev_document_factory_get_mime_types (EV_BACKEND_DJVU);
		file_filter_add_mime_list_and_free (filter, mime_types);
		gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);
	}
#endif	

#ifdef ENABLE_COMICS
	if (document == NULL || backend == EV_BACKEND_COMICS) {
		default_filter = filter = gtk_file_filter_new ();
		gtk_file_filter_set_name (filter, _("Comic Books"));
		mime_types = ev_document_factory_get_mime_types (EV_BACKEND_COMICS);
		file_filter_add_mime_list_and_free (filter, mime_types);
		gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);
	}
#endif	

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("All Files"));
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (chooser),
				     document == NULL ? document_filter : default_filter);
}
