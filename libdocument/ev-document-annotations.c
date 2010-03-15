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

EV_DEFINE_INTERFACE (EvDocumentAnnotations, ev_document_annotations, 0)

static void
ev_document_annotations_class_init (EvDocumentAnnotationsIface *klass)
{
}

GList *
ev_document_annotations_get_annotations (EvDocumentAnnotations *document_annots,
					 EvPage                *page)
{
	EvDocumentAnnotationsIface *iface = EV_DOCUMENT_ANNOTATIONS_GET_IFACE (document_annots);

	return iface->get_annotations (document_annots, page);
}

void
ev_document_annotations_annotation_set_contents (EvDocumentAnnotations *document_annots,
						 EvAnnotation          *annot,
						 const gchar           *contents)
{
	EvDocumentAnnotationsIface *iface = EV_DOCUMENT_ANNOTATIONS_GET_IFACE (document_annots);

	iface->annotation_set_contents (document_annots, annot, contents);
}
