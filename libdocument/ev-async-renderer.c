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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include "ev-async-renderer.h"
#include "ev-document.h"

enum
{
	RENDER_FINISHED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_INTERFACE (EvAsyncRenderer, ev_async_renderer, 0)

static void
ev_async_renderer_default_init (EvAsyncRendererInterface *klass)
{
	static gboolean initialized = FALSE;

	if (!initialized) {
		signals[RENDER_FINISHED] =
			g_signal_new ("render_finished",
				      EV_TYPE_ASYNC_RENDERER,
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (EvAsyncRendererInterface, render_finished),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__OBJECT,
				      G_TYPE_NONE,
				      1,
				      GDK_TYPE_PIXBUF);
		initialized = TRUE;
	}
}

void
ev_async_renderer_render_pixbuf (EvAsyncRenderer *async_renderer,
			         int              page,
			         double           scale,
				 int              rotation)
{
	EvAsyncRendererInterface *iface = EV_ASYNC_RENDERER_GET_IFACE (async_renderer);

	iface->render_pixbuf (async_renderer, page, scale, rotation);
}
