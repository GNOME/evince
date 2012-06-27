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
#include "ev-mapping-list.h"

G_BEGIN_DECLS

#define EV_TYPE_DOCUMENT_ANNOTATIONS            (ev_document_annotations_get_type ())
#define EV_DOCUMENT_ANNOTATIONS(o)              (G_TYPE_CHECK_INSTANCE_CAST ((o), EV_TYPE_DOCUMENT_ANNOTATIONS, EvDocumentAnnotations))
#define EV_DOCUMENT_ANNOTATIONS_IFACE(k)        (G_TYPE_CHECK_CLASS_CAST((k), EV_TYPE_DOCUMENT_ANNOTATIONS, EvDocumentAnnotationsInterface))
#define EV_IS_DOCUMENT_ANNOTATIONS(o)           (G_TYPE_CHECK_INSTANCE_TYPE ((o), EV_TYPE_DOCUMENT_ANNOTATIONS))
#define EV_IS_DOCUMENT_ANNOTATIONS_IFACE(k)     (G_TYPE_CHECK_CLASS_TYPE ((k), EV_TYPE_DOCUMENT_ANNOTATIONS))
#define EV_DOCUMENT_ANNOTATIONS_GET_IFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EV_TYPE_DOCUMENT_ANNOTATIONS, EvDocumentAnnotationsInterface))

typedef enum {
	EV_ANNOTATIONS_SAVE_NONE          = 0,
	EV_ANNOTATIONS_SAVE_CONTENTS      = 1 << 0,
	EV_ANNOTATIONS_SAVE_COLOR         = 1 << 1,

	/* Markup Annotations */
	EV_ANNOTATIONS_SAVE_LABEL         = 1 << 2,
	EV_ANNOTATIONS_SAVE_OPACITY       = 1 << 3,
	EV_ANNOTATIONS_SAVE_POPUP_RECT    = 1 << 4,
	EV_ANNOTATIONS_SAVE_POPUP_IS_OPEN = 1 << 5,

	/* Text Annotations */
	EV_ANNOTATIONS_SAVE_TEXT_IS_OPEN  = 1 << 6,
	EV_ANNOTATIONS_SAVE_TEXT_ICON     = 1 << 7,

	/* Attachment Annotations */
	EV_ANNOTATIONS_SAVE_ATTACHMENT    = 1 << 8,

	/* Save all */
	EV_ANNOTATIONS_SAVE_ALL           = (1 << 9) - 1
} EvAnnotationsSaveMask;

typedef struct _EvDocumentAnnotations          EvDocumentAnnotations;
typedef struct _EvDocumentAnnotationsInterface EvDocumentAnnotationsInterface;

struct _EvDocumentAnnotationsInterface
{
	GTypeInterface base_iface;

	/* Methods  */
	EvMappingList *(* get_annotations)      (EvDocumentAnnotations *document_annots,
						 EvPage                *page);
	gboolean       (* document_is_modified) (EvDocumentAnnotations *document_annots);
	void           (* add_annotation)       (EvDocumentAnnotations *document_annots,
						 EvAnnotation          *annot,
						 EvRectangle           *rect);
	void           (* save_annotation)      (EvDocumentAnnotations *document_annots,
						 EvAnnotation          *annot,
						 EvAnnotationsSaveMask  mask);
	void	       (* remove_annotation)    (EvDocumentAnnotations *document_annots,
						 EvAnnotation          *annot);
};

GType          ev_document_annotations_get_type             (void) G_GNUC_CONST;
EvMappingList *ev_document_annotations_get_annotations      (EvDocumentAnnotations *document_annots,
							     EvPage                *page);
gboolean       ev_document_annotations_document_is_modified (EvDocumentAnnotations *document_annots);
void           ev_document_annotations_add_annotation       (EvDocumentAnnotations *document_annots,
							     EvAnnotation          *annot,
							     EvRectangle           *rect);
void           ev_document_annotations_remove_annotation    (EvDocumentAnnotations *document_annots,
                                                             EvAnnotation          *annot);

void           ev_document_annotations_save_annotation      (EvDocumentAnnotations *document_annots,
							     EvAnnotation          *annot,
							     EvAnnotationsSaveMask  mask);
gboolean       ev_document_annotations_can_add_annotation    (EvDocumentAnnotations *document_annots);
gboolean       ev_document_annotations_can_remove_annotation (EvDocumentAnnotations *document_annots);

G_END_DECLS

#endif /* EV_DOCUMENT_ANNOTATIONS_H */

