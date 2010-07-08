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

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#ifndef EV_SELECTION_H
#define EV_SELECTION_H

#include <glib-object.h>
#include <glib.h>
#include <gdk/gdk.h>
#include "ev-document.h"

G_BEGIN_DECLS

#define EV_TYPE_SELECTION            (ev_selection_get_type ())
#define EV_SELECTION(o)		     (G_TYPE_CHECK_INSTANCE_CAST ((o), EV_TYPE_SELECTION, EvSelection))
#define EV_SELECTION_IFACE(k)	     (G_TYPE_CHECK_CLASS_CAST((k), EV_TYPE_SELECTION, EvSelectionInterface))
#define EV_IS_SELECTION(o)	     (G_TYPE_CHECK_INSTANCE_TYPE ((o), EV_TYPE_SELECTION))
#define EV_IS_SELECTION_IFACE(k)     (G_TYPE_CHECK_CLASS_TYPE ((k), EV_TYPE_SELECTION))
#define EV_SELECTION_GET_IFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EV_TYPE_SELECTION, EvSelectionInterface))

typedef enum {
	EV_SELECTION_STYLE_GLYPH,
	EV_SELECTION_STYLE_WORD,
	EV_SELECTION_STYLE_LINE
} EvSelectionStyle;

typedef struct _EvSelection	       EvSelection;
typedef struct _EvSelectionInterface   EvSelectionInterface;

struct _EvSelectionInterface
{
	GTypeInterface base_iface;

	void             (* render_selection)     (EvSelection      *selection,
						   EvRenderContext  *rc,
						   cairo_surface_t **surface,
						   EvRectangle      *points,
						   EvRectangle      *old_points,
						   EvSelectionStyle  style,
						   GdkColor         *text,
						   GdkColor         *base);
	gchar          * (* get_selected_text)    (EvSelection      *selection,
						   EvPage           *page,
						   EvSelectionStyle  style,
						   EvRectangle      *points);
	cairo_region_t * (* get_selection_region) (EvSelection      *selection,
						   EvRenderContext  *rc,
						   EvSelectionStyle  style,
						   EvRectangle      *points);
};

GType           ev_selection_get_type             (void) G_GNUC_CONST;
void            ev_selection_render_selection     (EvSelection      *selection,
						   EvRenderContext  *rc,
						   cairo_surface_t **surface,
						   EvRectangle      *points,
						   EvRectangle      *old_points,
						   EvSelectionStyle  style,
						   GdkColor         *text,
						   GdkColor         *base);
gchar          *ev_selection_get_selected_text    (EvSelection      *selection,
						   EvPage           *page,
						   EvSelectionStyle  style,
						   EvRectangle      *points);
cairo_region_t *ev_selection_get_selection_region (EvSelection      *selection,
						   EvRenderContext  *rc,
						   EvSelectionStyle  style,
						   EvRectangle      *points);

G_END_DECLS

#endif
