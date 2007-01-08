/* ev-document-images.h
 *  this file is part of evince, a gnome document viewer
 * 
 * Copyright (C) 2006 Carlos Garcia Campos <carlosgc@gnome.org>
 *
 * Evince is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Evince is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef EV_DOCUMENT_IMAGES_H
#define EV_DOCUMENT_IMAGES_H

#include <glib-object.h>
#include <glib.h>

#include "ev-document.h"

G_BEGIN_DECLS

#define EV_TYPE_DOCUMENT_IMAGES            (ev_document_images_get_type ())
#define EV_DOCUMENT_IMAGES(o)              (G_TYPE_CHECK_INSTANCE_CAST ((o), EV_TYPE_DOCUMENT_IMAGES, EvDocumentImages))
#define EV_DOCUMENT_IMAGES_IFACE(k)        (G_TYPE_CHECK_CLASS_CAST((k), EV_TYPE_DOCUMENT_IMAGES, EvDocumentImagesIface))
#define EV_IS_DOCUMENT_IMAGES(o)           (G_TYPE_CHECK_INSTANCE_TYPE ((o), EV_TYPE_DOCUMENT_IMAGES))
#define EV_IS_DOCUMENT_IMAGES_IFACE(k)     (G_TYPE_CHECK_CLASS_TYPE ((k), EV_TYPE_DOCUMENT_IMAGES))
#define EV_DOCUMENT_IMAGES_GET_IFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EV_TYPE_DOCUMENT_IMAGES, EvDocumentImagesIface))

typedef struct _EvDocumentImages      EvDocumentImages;
typedef struct _EvDocumentImagesIface EvDocumentImagesIface;

struct _EvDocumentImagesIface {
        GTypeInterface base_iface;

        /* Methods  */
        GList *(* get_images) (EvDocumentImages *document_images,
                               gint              page);
};

GType  ev_document_images_get_type   (void) G_GNUC_CONST;
GList *ev_document_images_get_images (EvDocumentImages *document_images,
                                      gint              page);

G_END_DECLS

#endif /* EV_DOCUMENT_IMAGES_H */
