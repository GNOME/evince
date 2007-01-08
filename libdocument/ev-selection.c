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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include "ev-selection.h"

static void ev_selection_base_init (gpointer g_class);

GType
ev_selection_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo our_info =
		{
			sizeof (EvSelectionIface),
			ev_selection_base_init,
			NULL,
		};

		type = g_type_register_static (G_TYPE_INTERFACE,
					       "EvSelection",
					       &our_info, (GTypeFlags)0);
	}

	return type;
}

static void
ev_selection_base_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (!initialized) {
	}
}


void
ev_selection_render_selection (EvSelection      *selection,
			       EvRenderContext  *rc,
			       GdkPixbuf       **pixbuf,
			       EvRectangle      *points,
			       EvRectangle      *old_points,
			       GdkColor        *text,
			       GdkColor        *base)
{
	EvSelectionIface *iface = EV_SELECTION_GET_IFACE (selection);

	iface->render_selection (selection, rc,
				 pixbuf,
				 points, old_points,
				 text, base);
}

GdkRegion *
ev_selection_get_selection_region (EvSelection     *selection,
				   EvRenderContext *rc,
				   EvRectangle     *points)
{
	EvSelectionIface *iface = EV_SELECTION_GET_IFACE (selection);

	return iface->get_selection_region (selection, rc, points);
}

GdkRegion *
ev_selection_get_selection_map (EvSelection     *selection,
				EvRenderContext *rc)
{
	EvSelectionIface *iface = EV_SELECTION_GET_IFACE (selection);

	return iface->get_selection_map (selection, rc);
}
