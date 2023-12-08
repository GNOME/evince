/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/*
 *  Copyright (C) 2010 Yaco Sistemas, Daniel Garcia <danigm@yaco.es>
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
 */

#pragma once

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#include <glib-object.h>
#include <pango/pango.h>
#include <glib.h>

#include "ev-macros.h"
#include "ev-document.h"

G_BEGIN_DECLS

#define EV_TYPE_DOCUMENT_TEXT               (ev_document_text_get_type ())

EV_PUBLIC
G_DECLARE_INTERFACE (EvDocumentText, ev_document_text, EV, DOCUMENT_TEXT, GObject)

struct _EvDocumentTextInterface
{
        GTypeInterface base_iface;

        /* Methods */
        cairo_region_t *(* get_text_mapping) (EvDocumentText   *document_text,
					      EvPage           *page);
        gchar          *(* get_text)         (EvDocumentText   *document_text,
					      EvPage           *page);
        gboolean        (* get_text_layout)  (EvDocumentText   *document_text,
					      EvPage           *page,
					      EvRectangle     **areas,
					      guint            *n_areas);
	PangoAttrList  *(* get_text_attrs)   (EvDocumentText   *document_text,
					      EvPage           *page);
};

EV_PUBLIC
gchar          *ev_document_text_get_text         (EvDocumentText  *document_text,
						   EvPage          *page);
EV_PUBLIC
gboolean        ev_document_text_get_text_layout  (EvDocumentText  *document_text,
						   EvPage          *page,
						   EvRectangle    **areas,
						   guint           *n_areas);
EV_PUBLIC
cairo_region_t *ev_document_text_get_text_mapping (EvDocumentText  *document_text,
						   EvPage          *page);
EV_PUBLIC
PangoAttrList  *ev_document_text_get_text_attrs   (EvDocumentText  *document_text,
						   EvPage          *page);
G_END_DECLS
