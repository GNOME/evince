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
#include "ev-document-factory.h"

#include <string.h>
#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <gtk/gtkfilechooserdialog.h>

static EvDocument *
get_document_from_uri (const char *uri, gboolean slow, gchar **mime_type, GError **error)
{
	EvDocument *document;
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
	
	document = ev_document_factory_get_document (info->mime_type);
	
	if (document == NULL) {
		g_set_error (error,
			     EV_DOCUMENT_ERROR,	
			     0,
			     _("Unhandled MIME type: '%s'"), info->mime_type);
		gnome_vfs_file_info_unref (info);
		return NULL;
	}			

	if (mime_type != NULL) {
		    *mime_type = g_strdup (info->mime_type);
	}

        gnome_vfs_file_info_unref (info);
	
        return document;
}

EvDocument *
ev_document_types_get_document (const char *uri, gchar **mime_type, GError **error)
{
	EvDocument *document;
	
	document = get_document_from_uri (uri, FALSE, mime_type, error);

	if (document != NULL) {
		return document;
	}
		
	if (error) {
		g_error_free (*error);
		*error = NULL;
	}

	document = get_document_from_uri (uri, TRUE, mime_type, error);

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
ev_document_types_add_filters (GtkWidget *chooser, EvDocument *document)
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
	gtk_file_filter_add_pixbuf_formats (filter);
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
		gtk_file_filter_add_pixbuf_formats (filter);
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
