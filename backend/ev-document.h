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

G_BEGIN_DECLS

#define EV_TYPE_DOCUMENT	    (ev_document_get_type ())
#define EV_DOCUMENT(o)		    (G_TYPE_CHECK_INSTANCE_CAST ((o), EV_TYPE_DOCUMENT, EvDocument))
#define EV_DOCUMENT_IFACE(k)	    (G_TYPE_CHECK_CLASS_CAST((k), EV_TYPE_DOCUMENT, EvDocumentIface))
#define EV_IS_DOCUMENT(o)	    (G_TYPE_CHECK_INSTANCE_TYPE ((o), EV_TYPE_DOCUMENT))
#define EV_IS_DOCUMENT_IFACE(k)	    (G_TYPE_CHECK_CLASS_TYPE ((k), EV_TYPE_DOCUMENT))
#define EV_DOCUMENT_GET_IFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EV_TYPE_DOCUMENT, EvDocumentIface))

typedef struct _EvDocument	EvDocument;
typedef struct _EvDocumentIface	EvDocumentIface;

#define EV_DOCUMENT_ERROR ev_document_error_quark ()

typedef enum
{
	EV_DOCUMENT_ERROR_INVALID,
	EV_DOCUMENT_ERROR_ENCRYPTED
} EvDocumentError;

struct _EvDocumentIface
{
	GTypeInterface base_iface;

	/* Signals */
	void	    (* changed)		(EvDocument *document);

	/* Methods  */
	gboolean    (* load)	        (EvDocument   *document,
					 const char   *uri,
					 GError      **error);
	gboolean    (* save)	        (EvDocument   *document,
					 const char   *uri,
					 GError      **error);
	int         (* get_n_pages)     (EvDocument   *document);
	void	    (* set_page)	(EvDocument   *document,
					 int           page);
	int	    (* get_page)	(EvDocument   *document);
	void	    (* set_target)      (EvDocument   *document,
					 GdkDrawable  *target);
	void	    (* set_scale)       (EvDocument   *document,
					 double        scale);
	void	    (* set_page_offset) (EvDocument   *document,
					 int           x,
					 int           y);
	void	    (* get_page_size)   (EvDocument   *document,
					 int           page,
					 int          *width,
					 int          *height);
	char	  * (* get_text)	(EvDocument   *document,
					 GdkRectangle *rect);
	EvLink    * (* get_link)	(EvDocument   *document,
					 int           x,
					 int	       y);
	void	    (* render)          (EvDocument   *document,
					 int           clip_x,
					 int           clip_y,
					 int           clip_width,
					 int           clip_height);


};

GType    ev_document_get_type    (void);
GQuark   ev_document_error_quark (void);

gboolean ev_document_load            (EvDocument   *document,
				      const char   *uri,
				      GError      **error);
gboolean ev_document_save            (EvDocument   *document,
				      const char   *uri,
				      GError      **error);
char    *ev_document_get_title       (EvDocument   *document);
int      ev_document_get_n_pages     (EvDocument   *document);
void     ev_document_set_page        (EvDocument   *document,
				      int           page);
int      ev_document_get_page        (EvDocument   *document);
void     ev_document_set_target      (EvDocument   *document,
				      GdkDrawable  *target);
void     ev_document_set_scale       (EvDocument   *document,
				      double        scale);
void     ev_document_set_page_offset (EvDocument   *document,
				      int           x,
				      int           y);
void     ev_document_get_page_size   (EvDocument   *document,
				      int           page,
				      int          *width,
				      int          *height);
char    *ev_document_get_text	     (EvDocument   *document,
				      GdkRectangle *rect);
EvLink  *ev_document_get_link	     (EvDocument   *document,
				      int           x,
				      int	    y);
void     ev_document_render          (EvDocument   *document,
				      int           clip_x,
				      int           clip_y,
				      int           clip_width,
				      int           clip_height);
void	 ev_document_changed         (EvDocument *document);

G_END_DECLS

#endif
