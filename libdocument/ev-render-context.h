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

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#ifndef EV_RENDER_CONTEXT_H
#define EV_RENDER_CONTEXT_H

#include <glib-object.h>

#include "ev-page.h"

G_BEGIN_DECLS

typedef struct _EvRenderContext EvRenderContext;
typedef struct _EvRenderContextClass EvRenderContextClass;

#define EV_TYPE_RENDER_CONTEXT		(ev_render_context_get_type())
#define EV_RENDER_CONTEXT(object)	(G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_RENDER_CONTEXT, EvRenderContext))
#define EV_RENDER_CONTEXT_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_RENDER_CONTEXT, EvRenderContextClass))
#define EV_IS_RENDER_CONTEXT(object)	(G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_RENDER_CONTEXT))

struct _EvRenderContextClass
{
	GObjectClass klass;
};

struct _EvRenderContext
{
	GObject parent;
	
	EvPage *page;
	gint    rotation;
	gdouble scale;
	gint	target_width;
	gint	target_height;
};


GType            ev_render_context_get_type        (void) G_GNUC_CONST;
EvRenderContext *ev_render_context_new             (EvPage          *page,
						    gint             rotation,
						    gdouble          scale);
void             ev_render_context_set_page        (EvRenderContext *rc,
						    EvPage          *page);
void             ev_render_context_set_rotation    (EvRenderContext *rc,
						    gint             rotation);
void             ev_render_context_set_scale       (EvRenderContext *rc,
						    gdouble          scale);
void             ev_render_context_set_target_size (EvRenderContext *rc,
                                                    int              target_width,
                                                    int              target_height);
void             ev_render_context_compute_scaled_size      (EvRenderContext *rc,
                                                             double           width_points,
                                                             double           height_points,
                                                             int             *scaled_width,
                                                             int             *scaled_height);
void             ev_render_context_compute_transformed_size (EvRenderContext *rc,
                                                             double	      width_points,
                                                             double	      height_points,
                                                             int	     *transformed_width,
                                                             int	     *transformed_height);
void             ev_render_context_compute_scales  (EvRenderContext *rc,
                                                    double           width_points,
                                                    double           height_points,
                                                    double          *scale_x,
                                                    double          *scale_y);

G_END_DECLS

#endif /* !EV_RENDER_CONTEXT */
