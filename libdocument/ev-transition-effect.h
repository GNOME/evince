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

#pragma once

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#include <glib-object.h>

#include "ev-macros.h"

G_BEGIN_DECLS

#define EV_TYPE_TRANSITION_EFFECT		  (ev_transition_effect_get_type ())

EV_PUBLIC
G_DECLARE_FINAL_TYPE (EvTransitionEffect, ev_transition_effect, EV, TRANSITION_EFFECT, GObject)

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

struct _EvTransitionEffect
{
	GObject parent_instance;
};

EV_PUBLIC
EvTransitionEffect   *ev_transition_effect_new                (EvTransitionEffectType  type,
							       const gchar            *first_property_name,
							       ...);

G_END_DECLS
