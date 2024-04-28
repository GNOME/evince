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

#pragma once

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#include <glib-object.h>

#include "ev-macros.h"
#include "ev-page.h"

G_BEGIN_DECLS

#define EV_TYPE_RENDER_CONTEXT		(ev_render_context_get_type())

EV_PUBLIC
G_DECLARE_FINAL_TYPE (EvRenderContext, ev_render_context, EV, RENDER_CONTEXT, GObject)

struct _EvRenderContext
{
	GObject parent;

	EvPage *page;
	gint    rotation;
	gdouble scale;
	gint	target_width;
	gint	target_height;
};


EV_PUBLIC
EvRenderContext *ev_render_context_new             (EvPage          *page,
						    gint             rotation,
						    gdouble          scale);
EV_PUBLIC
void             ev_render_context_set_page        (EvRenderContext *rc,
						    EvPage          *page);
EV_PUBLIC
void             ev_render_context_set_rotation    (EvRenderContext *rc,
						    gint             rotation);
EV_PUBLIC
void             ev_render_context_set_scale       (EvRenderContext *rc,
						    gdouble          scale);
EV_PUBLIC
void             ev_render_context_set_target_size (EvRenderContext *rc,
                                                    int              target_width,
                                                    int              target_height);
EV_PUBLIC
void             ev_render_context_compute_scaled_size      (EvRenderContext *rc,
                                                             double           width_points,
                                                             double           height_points,
                                                             int             *scaled_width,
                                                             int             *scaled_height);
EV_PUBLIC
void             ev_render_context_compute_transformed_size (EvRenderContext *rc,
                                                             double	      width_points,
                                                             double	      height_points,
                                                             int	     *transformed_width,
                                                             int	     *transformed_height);
EV_PUBLIC
void             ev_render_context_compute_scales  (EvRenderContext *rc,
                                                    double           width_points,
                                                    double           height_points,
                                                    double          *scale_x,
                                                    double          *scale_y);

G_END_DECLS
