/* ev-transition-animation.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2007 Carlos Garnacho <carlos@imendio.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 * Boston, MA 02110-1301, USA.
 */

#if !defined (EVINCE_COMPILATION)
#error "This is a private header."
#endif

#ifndef __EV_TRANSITION_ANIMATION_H__
#define __EV_TRANSITION_ANIMATION_H__

#include <evince-document.h>
#include "ev-timeline.h"
#include "ev-transition-effect.h"

G_BEGIN_DECLS

#define EV_TYPE_TRANSITION_ANIMATION                 (ev_transition_animation_get_type ())
#define EV_TRANSITION_ANIMATION(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_TRANSITION_ANIMATION, EvTransitionAnimation))
#define EV_TRANSITION_ANIMATION_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass),  EV_TYPE_TRANSITION_ANIMATION, EvTransitionAnimationClass))
#define EV_IS_TRANSITION_ANIMATION(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_TRANSITION_ANIMATION))
#define EV_IS_TRANSITION_ANIMATION_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass),  EV_TYPE_TRANSITION_ANIMATION))
#define EV_TRANSITION_ANIMATION_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj),  EV_TYPE_TRANSITION_ANIMATION, EvTransitionAnimationClass))

typedef struct _EvTransitionAnimation      EvTransitionAnimation;
typedef struct _EvTransitionAnimationClass EvTransitionAnimationClass;

struct _EvTransitionAnimation {
	EvTimeline parent_instance;
};

struct _EvTransitionAnimationClass {
	EvTimelineClass parent_class;
};


GType                   ev_transition_animation_get_type           (void) G_GNUC_CONST;

EvTransitionAnimation * ev_transition_animation_new                (EvTransitionEffect    *effect);

void                    ev_transition_animation_set_origin_surface (EvTransitionAnimation *animation,
								    cairo_surface_t       *origin_surface);
void                    ev_transition_animation_set_dest_surface   (EvTransitionAnimation *animation,
								    cairo_surface_t       *origin_surface);
gint                    ev_transition_animation_get_page_from      (EvTransitionAnimation *animation);
gint                    ev_transition_animation_get_page_to        (EvTransitionAnimation *animation);

void                    ev_transition_animation_paint              (EvTransitionAnimation *animation,
								    cairo_t               *cr,
								    GdkRectangle           page_area);
gboolean                ev_transition_animation_ready              (EvTransitionAnimation *animation);


G_END_DECLS

#endif /* __EV_TRANSITION_ANIMATION_H__ */
