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
#include "ps-document.h"
#ifdef ENABLE_DVI
#include "dvi-document.h"
#endif
#ifdef ENABLE_DJVU
#include "djvu-document.h"
#endif

#include <string.h>

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

GType
ev_document_type_lookup (const char *mime_type)
{
	int i;

	g_return_val_if_fail (mime_type, G_TYPE_INVALID);

	for (i=0;i<G_N_ELEMENTS (document_types);i++) {
		if (0==strcmp(mime_type, document_types[i].mime_type)) {
			g_assert (document_types[i].document_type_factory_callback!=NULL);
			return document_types[i].document_type_factory_callback();
		}
	}

	if (mime_type_supported_by_gdk_pixbuf (mime_type)) {
		return pixbuf_document_get_type ();
	}

	return G_TYPE_INVALID;
}
