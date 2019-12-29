/* ev-document-annotations.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2009 Carlos Garcia Campos <carlosgc@gnome.org>
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

#include "ev-document-annotations.h"

G_DEFINE_INTERFACE (EvDocumentAnnotations, ev_document_annotations, 0)

static void
ev_document_annotations_default_init (EvDocumentAnnotationsInterface *klass)
{
}

EvMappingList *
ev_document_annotations_get_annotations (EvDocumentAnnotations *document_annots,
					 EvPage                *page)
{
	EvDocumentAnnotationsInterface *iface = EV_DOCUMENT_ANNOTATIONS_GET_IFACE (document_annots);

	return iface->get_annotations (document_annots, page);
}

gboolean
ev_document_annotations_document_is_modified (EvDocumentAnnotations *document_annots)
{
	EvDocumentAnnotationsInterface *iface = EV_DOCUMENT_ANNOTATIONS_GET_IFACE (document_annots);

	return (iface->document_is_modified) ? iface->document_is_modified (document_annots) : FALSE;
}

void
ev_document_annotations_save_annotation (EvDocumentAnnotations *document_annots,
					 EvAnnotation          *annot,
					 EvAnnotationsSaveMask  mask)
{
	EvDocumentAnnotationsInterface *iface = EV_DOCUMENT_ANNOTATIONS_GET_IFACE (document_annots);

	iface->save_annotation (document_annots, annot, mask);
}

void
ev_document_annotations_add_annotation (EvDocumentAnnotations *document_annots,
					EvAnnotation          *annot,
					EvRectangle           *rect)
{
	EvDocumentAnnotationsInterface *iface = EV_DOCUMENT_ANNOTATIONS_GET_IFACE (document_annots);

	if (iface->add_annotation)
		iface->add_annotation (document_annots, annot, rect);
}

gboolean
ev_document_annotations_can_add_annotation (EvDocumentAnnotations *document_annots)
{
	EvDocumentAnnotationsInterface *iface = EV_DOCUMENT_ANNOTATIONS_GET_IFACE (document_annots);

	return iface->add_annotation != NULL;
}

void
ev_document_annotations_remove_annotation (EvDocumentAnnotations *document_annots,
					   EvAnnotation          *annot)
{
	EvDocumentAnnotationsInterface *iface = EV_DOCUMENT_ANNOTATIONS_GET_IFACE (document_annots);

	if (iface->remove_annotation)
		iface->remove_annotation (document_annots, annot);
}

gboolean
ev_document_annotations_can_remove_annotation (EvDocumentAnnotations *document_annots)
{
        EvDocumentAnnotationsInterface *iface = EV_DOCUMENT_ANNOTATIONS_GET_IFACE (document_annots);

	return iface->remove_annotation != NULL;
}

EvAnnotationsOverMarkup
ev_document_annotations_over_markup (EvDocumentAnnotations *document_annots,
				     EvAnnotation          *annot,
				     gdouble                x,
				     gdouble                y)
{
	EvDocumentAnnotationsInterface *iface = EV_DOCUMENT_ANNOTATIONS_GET_IFACE (document_annots);

	if (iface->over_markup)
		return iface->over_markup (document_annots, annot, x, y);

	return EV_ANNOTATION_OVER_MARKUP_NOT_IMPLEMENTED;
}
