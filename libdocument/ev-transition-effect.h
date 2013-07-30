/* ev-transition-effect.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2007 Carlos Garnacho <carlos@imendio.com>
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

#ifndef __EV_TRANSITION_EFFECT_H__
#define __EV_TRANSITION_EFFECT_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define EV_TYPE_TRANSITION_EFFECT		  (ev_transition_effect_get_type ())
#define EV_TRANSITION_EFFECT(obj)		  (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_TRANSITION_EFFECT, EvTransitionEffect))
#define EV_TRANSITION_EFFECT_CLASS(klass)	  (G_TYPE_CHECK_CLASS_CAST ((klass),  EV_TYPE_TRANSITION_EFFECT, EvTransitionEffectClass))
#define EV_IS_TRANSITION_EFFECT(obj)		  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_TRANSITION_EFFECT))
#define EV_IS_TRANSITION_EFFECT_CLASS(klass)	  (G_TYPE_CHECK_CLASS_TYPE ((klass),  EV_TYPE_TRANSITION_EFFECT))
#define EV_TRANSITION_EFFECT_GET_CLASS(obj)	  (G_TYPE_INSTANCE_GET_CLASS ((obj),  EV_TYPE_TRANSITION_EFFECT, EvTransitionEffectClass))

typedef enum {
	EV_TRANSITION_EFFECT_REPLACE,
	EV_TRANSITION_EFFECT_SPLIT,
	EV_TRANSITION_EFFECT_BLINDS,
	EV_TRANSITION_EFFECT_BOX,
	EV_TRANSITION_EFFECT_WIPE,
	EV_TRANSITION_EFFECT_DISSOLVE,
	EV_TRANSITION_EFFECT_GLITTER,
	EV_TRANSITION_EFFECT_FLY,
	EV_TRANSITION_EFFECT_PUSH,
	EV_TRANSITION_EFFECT_COVER,
	EV_TRANSITION_EFFECT_UNCOVER,
	EV_TRANSITION_EFFECT_FADE
} EvTransitionEffectType;

typedef enum {
	EV_TRANSITION_ALIGNMENT_HORIZONTAL,
	EV_TRANSITION_ALIGNMENT_VERTICAL
} EvTransitionEffectAlignment;

typedef enum {
	EV_TRANSITION_DIRECTION_INWARD,
	EV_TRANSITION_DIRECTION_OUTWARD
} EvTransitionEffectDirection;


typedef struct _EvTransitionEffect      EvTransitionEffect;
typedef struct _EvTransitionEffectClass EvTransitionEffectClass;

struct _EvTransitionEffect
{
	GObject parent_instance;
};

struct _EvTransitionEffectClass
{
	GObjectClass parent_class;
};

GType                 ev_transition_effect_get_type           (void) G_GNUC_CONST;

EvTransitionEffect   *ev_transition_effect_new                (EvTransitionEffectType  type,
							       const gchar            *first_property_name,
							       ...);

G_END_DECLS

#endif /* __EV_TRANSITION_EFFECT_H__ */
