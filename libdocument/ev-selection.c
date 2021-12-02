/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/*
 *  Copyright (C) 2005 Red Hat, Inc.
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

#include "config.h"

#include "ev-selection.h"

G_DEFINE_INTERFACE (EvSelection, ev_selection, 0)

static void
ev_selection_default_init (EvSelectionInterface *klass)
{
}

void
ev_selection_render_selection (EvSelection      *selection,
			       EvRenderContext  *rc,
			       cairo_surface_t **surface,
			       EvRectangle      *points,
			       EvRectangle      *old_points,
			       EvSelectionStyle  style,
			       GdkRGBA          *text,
			       GdkRGBA          *base)
{
	EvSelectionInterface *iface = EV_SELECTION_GET_IFACE (selection);

	if (!iface->render_selection)
		return;

	iface->render_selection (selection, rc,
				 surface,
				 points, old_points,
				 style,
				 text, base);
}

gchar *
ev_selection_get_selected_text (EvSelection      *selection,
				EvPage           *page,
				EvSelectionStyle  style,
				EvRectangle      *points)
{
	EvSelectionInterface *iface = EV_SELECTION_GET_IFACE (selection);

	return iface->get_selected_text (selection, page, style, points);
}

cairo_region_t *
ev_selection_get_selection_region (EvSelection     *selection,
				   EvRenderContext *rc,
				   EvSelectionStyle style,
				   EvRectangle     *points)
{
	EvSelectionInterface *iface = EV_SELECTION_GET_IFACE (selection);

	if (!iface->get_selection_region)
		return NULL;

	return iface->get_selection_region (selection, rc, style, points);
}
