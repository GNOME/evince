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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#include <glib-object.h>
#include <glib.h>
#include <gdk/gdk.h>

#include "ev-macros.h"
#include "ev-document.h"

G_BEGIN_DECLS

#define EV_TYPE_SELECTION            (ev_selection_get_type ())

EV_PUBLIC
G_DECLARE_INTERFACE (EvSelection, ev_selection, EV, SELECTION, GObject)

typedef enum {
	EV_SELECTION_STYLE_GLYPH,
	EV_SELECTION_STYLE_WORD,
	EV_SELECTION_STYLE_LINE
} EvSelectionStyle;

struct _EvSelectionInterface
{
	GTypeInterface base_iface;

	void             (* render_selection)     (EvSelection      *selection,
						   EvRenderContext  *rc,
						   cairo_surface_t **surface,
						   EvRectangle      *points,
						   EvRectangle      *old_points,
						   EvSelectionStyle  style,
						   GdkRGBA          *text,
						   GdkRGBA          *base);
	gchar          * (* get_selected_text)    (EvSelection      *selection,
						   EvPage           *page,
						   EvSelectionStyle  style,
						   EvRectangle      *points);
	cairo_region_t * (* get_selection_region) (EvSelection      *selection,
						   EvRenderContext  *rc,
						   EvSelectionStyle  style,
						   EvRectangle      *points);
};

EV_PUBLIC
void            ev_selection_render_selection     (EvSelection      *selection,
						   EvRenderContext  *rc,
						   cairo_surface_t **surface,
						   EvRectangle      *points,
						   EvRectangle      *old_points,
						   EvSelectionStyle  style,
						   GdkRGBA          *text,
						   GdkRGBA          *base);
EV_PUBLIC
gchar          *ev_selection_get_selected_text    (EvSelection      *selection,
						   EvPage           *page,
						   EvSelectionStyle  style,
						   EvRectangle      *points);
EV_PUBLIC
cairo_region_t *ev_selection_get_selection_region (EvSelection      *selection,
						   EvRenderContext  *rc,
						   EvSelectionStyle  style,
						   EvRectangle      *points);

G_END_DECLS
