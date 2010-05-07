/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/*
 *  Copyright (C) 2004 Anders Carlsson
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

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#ifndef EV_DOCUMENT_THUMBNAILS_H
#define EV_DOCUMENT_THUMBNAILS_H

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "ev-render-context.h"

G_BEGIN_DECLS

#define EV_TYPE_DOCUMENT_THUMBNAILS            (ev_document_thumbnails_get_type ())
#define EV_DOCUMENT_THUMBNAILS(o)              (G_TYPE_CHECK_INSTANCE_CAST ((o), EV_TYPE_DOCUMENT_THUMBNAILS, EvDocumentThumbnails))
#define EV_DOCUMENT_THUMBNAILS_IFACE(k)        (G_TYPE_CHECK_CLASS_CAST((k), EV_TYPE_DOCUMENT_THUMBNAILS, EvDocumentThumbnailsInterface))
#define EV_IS_DOCUMENT_THUMBNAILS(o)           (G_TYPE_CHECK_INSTANCE_TYPE ((o), EV_TYPE_DOCUMENT_THUMBNAILS))
#define EV_IS_DOCUMENT_THUMBNAILS_IFACE(k)     (G_TYPE_CHECK_CLASS_TYPE ((k), EV_TYPE_DOCUMENT_THUMBNAILS))
#define EV_DOCUMENT_THUMBNAILS_GET_IFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EV_TYPE_DOCUMENT_THUMBNAILS, EvDocumentThumbnailsInterface))

typedef struct _EvDocumentThumbnails          EvDocumentThumbnails;
typedef struct _EvDocumentThumbnailsInterface EvDocumentThumbnailsInterface;

struct _EvDocumentThumbnailsInterface {
        GTypeInterface base_iface;

        /* Methods  */
        GdkPixbuf *  (* get_thumbnail)  (EvDocumentThumbnails *document,
                                         EvRenderContext      *rc, 
                                         gboolean              border);
        void         (* get_dimensions) (EvDocumentThumbnails *document,
                                         EvRenderContext      *rc,
                                         gint                 *width,
                                         gint                 *height);
};

GType      ev_document_thumbnails_get_type       (void) G_GNUC_CONST;

GdkPixbuf *ev_document_thumbnails_get_thumbnail  (EvDocumentThumbnails *document,
                                                  EvRenderContext      *rc, 
                                                  gboolean              border);
void       ev_document_thumbnails_get_dimensions (EvDocumentThumbnails *document,
                                                  EvRenderContext      *rc,
                                                  gint                 *width,
                                                  gint                 *height);

G_END_DECLS

#endif /* EV_DOCUMENT_THUMBNAILS_H */
