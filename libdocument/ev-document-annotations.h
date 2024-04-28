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

#pragma once

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#include <glib-object.h>

#include "ev-macros.h"
#include "ev-document.h"
#include "ev-annotation.h"
#include "ev-mapping-list.h"

G_BEGIN_DECLS

#define EV_TYPE_DOCUMENT_ANNOTATIONS            (ev_document_annotations_get_type ())

EV_PUBLIC
G_DECLARE_INTERFACE (EvDocumentAnnotations, ev_document_annotations, EV, DOCUMENT_ANNOTATIONS, GObject)

typedef enum {
	EV_ANNOTATIONS_SAVE_NONE             = 0,
	EV_ANNOTATIONS_SAVE_CONTENTS         = 1 << 0,
	EV_ANNOTATIONS_SAVE_COLOR            = 1 << 1,
        EV_ANNOTATIONS_SAVE_AREA             = 1 << 2,

	/* Markup Annotations */
	EV_ANNOTATIONS_SAVE_LABEL            = 1 << 3,
	EV_ANNOTATIONS_SAVE_OPACITY          = 1 << 4,
	EV_ANNOTATIONS_SAVE_POPUP_RECT       = 1 << 5,
	EV_ANNOTATIONS_SAVE_POPUP_IS_OPEN    = 1 << 6,

	/* Text Annotations */
	EV_ANNOTATIONS_SAVE_TEXT_IS_OPEN     = 1 << 7,
	EV_ANNOTATIONS_SAVE_TEXT_ICON        = 1 << 8,

	/* Attachment Annotations */
	EV_ANNOTATIONS_SAVE_ATTACHMENT       = 1 << 9,

        /* Text Markup Annotations */
        EV_ANNOTATIONS_SAVE_TEXT_MARKUP_TYPE = 1 << 10,

	/* Save all */
	EV_ANNOTATIONS_SAVE_ALL              = (1 << 11) - 1
} EvAnnotationsSaveMask;

typedef enum {
	EV_ANNOTATION_OVER_MARKUP_NOT_IMPLEMENTED = 0,
	EV_ANNOTATION_OVER_MARKUP_UNKNOWN,
	EV_ANNOTATION_OVER_MARKUP_YES,
	EV_ANNOTATION_OVER_MARKUP_NOT
} EvAnnotationsOverMarkup;

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
	EvAnnotationsOverMarkup (* over_markup) (EvDocumentAnnotations *document_annots,
						 EvAnnotation          *annot,
						 gdouble                 x,
						 gdouble                 y);
};

EV_PUBLIC
EvMappingList *ev_document_annotations_get_annotations      (EvDocumentAnnotations *document_annots,
							     EvPage                *page);
EV_PUBLIC
gboolean       ev_document_annotations_document_is_modified (EvDocumentAnnotations *document_annots);
EV_PUBLIC
void           ev_document_annotations_add_annotation       (EvDocumentAnnotations *document_annots,
							     EvAnnotation          *annot,
							     EvRectangle           *rect);
EV_PUBLIC
void           ev_document_annotations_remove_annotation    (EvDocumentAnnotations *document_annots,
                                                             EvAnnotation          *annot);

EV_PUBLIC
void           ev_document_annotations_save_annotation      (EvDocumentAnnotations *document_annots,
							     EvAnnotation          *annot,
							     EvAnnotationsSaveMask  mask);
EV_PUBLIC
gboolean       ev_document_annotations_can_add_annotation    (EvDocumentAnnotations *document_annots);
EV_PUBLIC
gboolean       ev_document_annotations_can_remove_annotation (EvDocumentAnnotations *document_annots);
EV_PUBLIC
EvAnnotationsOverMarkup ev_document_annotations_over_markup  (EvDocumentAnnotations *document_annots,
							      EvAnnotation          *annot,
							      gdouble                x,
							      gdouble                y);

G_END_DECLS
