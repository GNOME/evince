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

#define FLIP_DIMENSIONS(rc) ((rc)->rotation == 90 || (rc)->rotation == 270)

static void ev_render_context_init (EvRenderContext *rc) { /* Do Nothing */ }

static void
ev_render_context_dispose (GObject *object)
{
	EvRenderContext *rc;

	rc = (EvRenderContext *) object;

	g_clear_object (&rc->page);

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
	rc->target_width = -1;
	rc->target_height = -1;

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

void
ev_render_context_set_target_size (EvRenderContext *rc,
				   int		    target_width,
				   int		    target_height)
{
	g_return_if_fail (rc != NULL);

	rc->target_width = target_width;
	rc->target_height = target_height;
}

void
ev_render_context_compute_scaled_size (EvRenderContext *rc,
				       double		width_points,
				       double		height_points,
				       int	       *scaled_width,
				       int	       *scaled_height)
{
	g_return_if_fail (rc != NULL);

	if (scaled_width) {
		if (rc->target_width >= 0) {
			*scaled_width = FLIP_DIMENSIONS (rc) ? rc->target_height : rc->target_width;
		} else {
			*scaled_width = (int) (width_points * rc->scale + 0.5);
		}
	}

	if (scaled_height) {
		if (rc->target_height >= 0) {
			*scaled_height = FLIP_DIMENSIONS (rc) ? rc->target_width : rc->target_height;
		} else {
			*scaled_height = (int) (height_points * rc->scale + 0.5);
		}
	}
}

void
ev_render_context_compute_transformed_size (EvRenderContext *rc,
					    double	     width_points,
					    double	     height_points,
					    int	            *transformed_width,
					    int	            *transformed_height)
{
	int scaled_width, scaled_height;

	g_return_if_fail (rc != NULL);

	ev_render_context_compute_scaled_size (rc, width_points, height_points,
					       &scaled_width, &scaled_height);

	if (transformed_width)
		*transformed_width = FLIP_DIMENSIONS (rc) ? scaled_height : scaled_width;

	if (transformed_height)
		*transformed_height = FLIP_DIMENSIONS (rc) ? scaled_width : scaled_height;
}

void
ev_render_context_compute_scales (EvRenderContext *rc,
				  double	   width_points,
				  double	   height_points,
				  double	  *scale_x,
				  double	  *scale_y)
{
	int scaled_width, scaled_height;

	g_return_if_fail (rc != NULL);

	ev_render_context_compute_scaled_size (rc, width_points, height_points,
					       &scaled_width, &scaled_height);

	if (scale_x)
		*scale_x = scaled_width / width_points;

	if (scale_y)
		*scale_y = scaled_height / height_points;
}
