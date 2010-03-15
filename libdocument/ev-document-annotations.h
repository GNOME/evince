/* ev-document-annotations.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2007 Iñigo Martinez <inigomartinez@gmail.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#ifndef EV_DOCUMENT_ANNOTATIONS_H
#define EV_DOCUMENT_ANNOTATIONS_H

#include <glib-object.h>

#include "ev-document.h"
#include "ev-annotation.h"

G_BEGIN_DECLS

#define EV_TYPE_DOCUMENT_ANNOTATIONS            (ev_document_annotations_get_type ())
#define EV_DOCUMENT_ANNOTATIONS(o)              (G_TYPE_CHECK_INSTANCE_CAST ((o), EV_TYPE_DOCUMENT_ANNOTATIONS, EvDocumentAnnotations))
#define EV_DOCUMENT_ANNOTATIONS_IFACE(k)        (G_TYPE_CHECK_CLASS_CAST((k), EV_TYPE_DOCUMENT_ANNOTATIONS, EvDocumentAnnotationsIface))
#define EV_IS_DOCUMENT_ANNOTATIONS(o)           (G_TYPE_CHECK_INSTANCE_TYPE ((o), EV_TYPE_DOCUMENT_ANNOTATIONS))
#define EV_IS_DOCUMENT_ANNOTATIONS_IFACE(k)     (G_TYPE_CHECK_CLASS_TYPE ((k), EV_TYPE_DOCUMENT_ANNOTATIONS))
#define EV_DOCUMENT_ANNOTATIONS_GET_IFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EV_TYPE_DOCUMENT_ANNOTATIONS, EvDocumentAnnotationsIface))

typedef struct _EvDocumentAnnotations      EvDocumentAnnotations;
typedef struct _EvDocumentAnnotationsIface EvDocumentAnnotationsIface;

struct _EvDocumentAnnotationsIface
{
	GTypeInterface base_iface;

	/* Methods  */
	GList *(* get_annotations)         (EvDocumentAnnotations *document_annots,
					    EvPage                *page);
	void   (* annotation_set_contents) (EvDocumentAnnotations *document_annots,
					    EvAnnotation          *annot,
					    const gchar           *contents);
};

GType  ev_document_annotations_get_type                (void) G_GNUC_CONST;
GList *ev_document_annotations_get_annotations         (EvDocumentAnnotations *document_annots,
							EvPage                *page);

void   ev_document_annotations_annotation_set_contents (EvDocumentAnnotations *document_annots,
							EvAnnotation          *annot,
							const gchar           *contents);

G_END_DECLS

#endif /* EV_DOCUMENT_ANNOTATIONS_H */

