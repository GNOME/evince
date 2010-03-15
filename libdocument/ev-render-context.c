/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2005 Jonathan Blandford <jrb@gnome.org>
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

#include <config.h>
#include "ev-render-context.h"

static void ev_render_context_init       (EvRenderContext      *rc);
static void ev_render_context_class_init (EvRenderContextClass *class);


G_DEFINE_TYPE (EvRenderContext, ev_render_context, G_TYPE_OBJECT);

static void ev_render_context_init (EvRenderContext *rc) { /* Do Nothing */ }

static void
ev_render_context_dispose (GObject *object)
{
	EvRenderContext *rc;

	rc = (EvRenderContext *) object;

	if (rc->page) {
		g_object_unref (rc->page);
		rc->page = NULL;
	}

	(* G_OBJECT_CLASS (ev_render_context_parent_class)->dispose) (object);
}

static void
ev_render_context_class_init (EvRenderContextClass *class)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (class);

	oclass->dispose = ev_render_context_dispose;
}

EvRenderContext *
ev_render_context_new (EvPage *page,
		       gint    rotation,
		       gdouble scale)
{
	EvRenderContext *rc;

	rc = (EvRenderContext *) g_object_new (EV_TYPE_RENDER_CONTEXT, NULL);

	rc->page = page ? g_object_ref (page) : NULL;
	rc->rotation = rotation;
	rc->scale = scale;

	return rc;
}

void
ev_render_context_set_page (EvRenderContext *rc,
			    EvPage          *page)
{
	g_return_if_fail (rc != NULL);
	g_return_if_fail (EV_IS_PAGE (page));

	if (rc->page)
		g_object_unref (rc->page);
	rc->page = g_object_ref (page);
}

void
ev_render_context_set_rotation (EvRenderContext *rc,
				int              rotation)
{
	g_return_if_fail (rc != NULL);

	rc->rotation = rotation;
}

void
ev_render_context_set_scale (EvRenderContext *rc,
			     gdouble          scale)
{
	g_return_if_fail (rc != NULL);

	rc->scale = scale;
}

