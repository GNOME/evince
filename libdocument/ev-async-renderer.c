/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/*
 *  Copyright (C) 2004 Marco Pesenti Gritti
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

#include "ev-async-renderer.h"

static void ev_async_renderer_class_init (gpointer g_class);

enum
{
	RENDER_FINISHED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

GType
ev_async_renderer_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo our_info =
		{
			sizeof (EvAsyncRendererIface),
			NULL,
			NULL,
			(GClassInitFunc)ev_async_renderer_class_init
		};

		type = g_type_register_static (G_TYPE_INTERFACE,
					       "EvAsyncRenderer",
					       &our_info, (GTypeFlags)0);
	}

	return type;
}

static void
ev_async_renderer_class_init (gpointer g_class)
{
	signals[RENDER_FINISHED] =
		g_signal_new ("render_finished",
			      EV_TYPE_ASYNC_RENDERER,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EvAsyncRendererIface, render_finished),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      GDK_TYPE_PIXBUF);
}

void
ev_async_renderer_render_pixbuf (EvAsyncRenderer *async_renderer,
			         int              page,
			         double           scale,
				 int              rotation)
{
	EvAsyncRendererIface *iface = EV_ASYNC_RENDERER_GET_IFACE (async_renderer);

	iface->render_pixbuf (async_renderer, page, scale, rotation);
}
