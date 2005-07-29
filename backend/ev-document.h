/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/*
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
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
 *  $Id$
 */

#ifndef EV_DOCUMENT_H
#define EV_DOCUMENT_H

#include <glib-object.h>
#include <glib.h>
#include <gdk/gdk.h>

#include "ev-link.h"
#include "ev-document-info.h"
#include "ev-render-context.h"

G_BEGIN_DECLS

#define EV_TYPE_DOCUMENT	    (ev_document_get_type ())
#define EV_DOCUMENT(o)		    (G_TYPE_CHECK_INSTANCE_CAST ((o), EV_TYPE_DOCUMENT, EvDocument))
#define EV_DOCUMENT_IFACE(k)	    (G_TYPE_CHECK_CLASS_CAST((k), EV_TYPE_DOCUMENT, EvDocumentIface))
#define EV_IS_DOCUMENT(o)	    (G_TYPE_CHECK_INSTANCE_TYPE ((o), EV_TYPE_DOCUMENT))
#define EV_IS_DOCUMENT_IFACE(k)	    (G_TYPE_CHECK_CLASS_TYPE ((k), EV_TYPE_DOCUMENT))
#define EV_DOCUMENT_GET_IFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EV_TYPE_DOCUMENT, EvDocumentIface))

typedef struct _EvDocument	  EvDocument;
typedef struct _EvDocumentIface   EvDocumentIface;
typedef struct _EvPageCache       EvPageCache;
typedef struct _EvPageCacheClass  EvPageCacheClass;



#define EV_DOCUMENT_ERROR ev_document_error_quark ()
#define EV_DOC_MUTEX_LOCK (ev_document_doc_mutex_lock ())
#define EV_DOC_MUTEX_UNLOCK (ev_document_doc_mutex_unlock ())

typedef enum
{
	EV_DOCUMENT_ERROR_INVALID,
	EV_DOCUMENT_ERROR_ENCRYPTED
} EvDocumentError;

typedef struct {
	double x1;
	double y1;
	double x2;
	double y2;
} EvRectangle;

struct _EvDocumentIface
{
	GTypeInterface base_iface;

	/* Methods  */
	gboolean         (* load)	     (EvDocument   *document,
					      const char   *uri,
					      GError      **error);
	gboolean         (* save)	     (EvDocument   *document,
					      const char   *uri,
					      GError      **error);
	int              (* get_n_pages)     (EvDocument   *document);
	void	         (* get_page_size)   (EvDocument   *document,
					      int           page,
					      double       *width,
					      double       *height);
	char	       * (* get_page_label)  (EvDocument   *document,
					      int           page);
	gboolean         (* can_get_text)    (EvDocument   *document);
	char	       * (* get_text)	     (EvDocument   *document,
					      int           page,
					      EvRectangle  *rect);
	GList          * (* get_links)	     (EvDocument   *document,
					      int           page);
	GdkPixbuf      * (* render_pixbuf)   (EvDocument      *document,
					      EvRenderContext *rc);
	EvOrientation    (* get_orientation) (EvDocument   *document);
	EvDocumentInfo * (* get_info)        (EvDocument   *document);
};

GType		ev_document_get_type       (void);
GQuark		ev_document_error_quark    (void);
GMutex	       *ev_document_get_doc_mutex  (void);
void            ev_document_doc_mutex_lock (void);
void            ev_document_doc_mutex_unlock (void);

EvDocumentInfo *ev_document_get_info	    (EvDocument     *document);
gboolean	ev_document_load	    (EvDocument     *document,
					     const char     *uri,
					     GError        **error);
gboolean	ev_document_save	    (EvDocument     *document,
					     const char     *uri,
					     GError        **error);
int		ev_document_get_n_pages	    (EvDocument     *document);
void		ev_document_get_page_size   (EvDocument     *document,
					     int             page,
					     double         *width,
					     double         *height);
char	       *ev_document_get_page_label  (EvDocument     *document,
					     int             page);
gboolean	ev_document_can_get_text    (EvDocument     *document);
char	       *ev_document_get_text        (EvDocument     *document,
					     int             page,
					     EvRectangle    *rect);
GList	       *ev_document_get_links       (EvDocument     *document,
				             int             page);
GdkPixbuf      *ev_document_render_pixbuf   (EvDocument     *document,
					     EvRenderContext *rc);
EvOrientation   ev_document_get_orientation (EvDocument     *document);
void	        ev_document_set_orientation (EvDocument     *document,
					     EvOrientation   orientation);


gint            ev_rect_cmp                 (EvRectangle    *a,
					     EvRectangle    *b);


G_END_DECLS

#endif
