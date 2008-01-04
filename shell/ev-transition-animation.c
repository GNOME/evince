/* ev-transition-animation.c
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <cairo.h>
#include <gdk/gdk.h>
#include "ev-transition-animation.h"
#include "ev-timeline.h"

#define EV_TRANSITION_ANIMATION_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), EV_TYPE_TRANSITION_ANIMATION, EvTransitionAnimationPriv))
#define N_BLINDS 6

typedef struct EvTransitionAnimationPriv EvTransitionAnimationPriv;

struct EvTransitionAnimationPriv {
	EvTransitionEffect *effect;
	cairo_surface_t *origin_surface;
	cairo_surface_t *dest_surface;
};

enum {
	PROP_0,
	PROP_EFFECT,
	PROP_ORIGIN_SURFACE,
	PROP_DEST_SURFACE
};


G_DEFINE_TYPE (EvTransitionAnimation, ev_transition_animation, EV_TYPE_TIMELINE)


static void
ev_transition_animation_init (EvTransitionAnimation *animation)
{
}

static void
ev_transition_animation_set_property (GObject      *object,
				      guint         prop_id,
				      const GValue *value,
				      GParamSpec   *pspec)
{
	EvTransitionAnimationPriv *priv;

	priv = EV_TRANSITION_ANIMATION_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_EFFECT:
		if (priv->effect)
			g_object_unref (priv->effect);

		priv->effect = g_value_dup_object (value);
		break;
	case PROP_ORIGIN_SURFACE:
		ev_transition_animation_set_origin_surface (EV_TRANSITION_ANIMATION (object),
							    g_value_get_pointer (value));
		break;
	case PROP_DEST_SURFACE:
		ev_transition_animation_set_dest_surface (EV_TRANSITION_ANIMATION (object),
							  g_value_get_pointer (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
ev_transition_animation_get_property (GObject      *object,
				      guint         prop_id,
				      GValue       *value,
				      GParamSpec   *pspec)
{
	EvTransitionAnimationPriv *priv;

	priv = EV_TRANSITION_ANIMATION_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_EFFECT:
		g_value_set_object (value, priv->effect);
		break;
	case PROP_ORIGIN_SURFACE:
		g_value_set_pointer (value, priv->origin_surface);
		break;
	case PROP_DEST_SURFACE:
		g_value_set_pointer (value, priv->dest_surface);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
ev_transition_animation_finalize (GObject *object)
{
	EvTransitionAnimationPriv *priv;

	priv = EV_TRANSITION_ANIMATION_GET_PRIVATE (object);

	if (priv->effect)
		g_object_unref (priv->effect);

	if (priv->origin_surface)
		cairo_surface_destroy (priv->origin_surface);

	if (priv->dest_surface)
		cairo_surface_destroy (priv->dest_surface);

	G_OBJECT_CLASS (ev_transition_animation_parent_class)->finalize (object);
}

static GObject *
ev_transition_animation_constructor (GType                  type,
				     guint                  n_construct_properties,
				     GObjectConstructParam *construct_params)
{
	GObject *object;
	EvTransitionAnimationPriv *priv;
	EvTransitionEffect *effect;
	gint duration;

	object = G_OBJECT_CLASS (ev_transition_animation_parent_class)->constructor (type,
										     n_construct_properties,
										     construct_params);

	priv = EV_TRANSITION_ANIMATION_GET_PRIVATE (object);
	effect = priv->effect;

	g_object_get (effect, "duration", &duration, NULL);
	ev_timeline_set_duration (EV_TIMELINE (object), duration * 1000);

	return object;
}

static void
ev_transition_animation_class_init (EvTransitionAnimationClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = ev_transition_animation_set_property;
	object_class->get_property = ev_transition_animation_get_property;
	object_class->finalize = ev_transition_animation_finalize;
	object_class->constructor = ev_transition_animation_constructor;

	g_object_class_install_property (object_class,
					 PROP_EFFECT,
					 g_param_spec_object ("effect",
							      "Effect",
							      "Transition effect description",
							      EV_TYPE_TRANSITION_EFFECT,
							      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_ORIGIN_SURFACE,
					 g_param_spec_pointer ("origin-surface",
							       "Origin surface",
							       "Cairo surface from which the animation will happen",
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_DEST_SURFACE,
					 g_param_spec_pointer ("dest-surface",
							       "Destination surface",
							       "Cairo surface to which the animation will happen",
							       G_PARAM_READWRITE));

	g_type_class_add_private (klass, sizeof (EvTransitionAnimationPriv));
}

static void
paint_surface (cairo_t         *cr,
	       cairo_surface_t *surface,
	       gdouble          x_offset,
	       gdouble          y_offset,
	       gdouble          alpha,
	       GdkRectangle     page_area)
{
	gint width, height;

	gdk_cairo_rectangle (cr, &page_area);
	cairo_clip (cr);

	width = cairo_image_surface_get_width (surface);
	height = cairo_image_surface_get_height (surface);

	cairo_save (cr);

	if (width != page_area.width || height != page_area.height) {
		cairo_pattern_set_filter (cairo_get_source (cr), CAIRO_FILTER_FAST);
		cairo_scale (cr,
			     (gdouble) page_area.width / width,
			     (gdouble) page_area.height / height);
	}

	cairo_surface_set_device_offset (surface, x_offset, y_offset);
	cairo_set_source_surface (cr, surface, 0, 0);

	if (alpha == 0.)
		cairo_paint (cr);
	else
		cairo_paint_with_alpha (cr, alpha);

	cairo_restore (cr);
}

/* animations */
static void
ev_transition_animation_split (cairo_t               *cr,
			       EvTransitionAnimation *animation,
			       EvTransitionEffect    *effect,
			       gdouble                progress,
			       GdkRectangle           page_area)
{
	EvTransitionAnimationPriv *priv;
	EvTransitionEffectAlignment alignment;
	EvTransitionEffectDirection direction;
	gint width, height;

	priv = EV_TRANSITION_ANIMATION_GET_PRIVATE (animation);
	width = page_area.width;
	height = page_area.height;

	g_object_get (effect,
		      "alignment", &alignment,
		      "direction", &direction,
		      NULL);

	if (direction == EV_TRANSITION_DIRECTION_INWARD) {
		paint_surface (cr, priv->dest_surface, 0, 0, 0, page_area);

		if (alignment == EV_TRANSITION_ALIGNMENT_HORIZONTAL) {
			cairo_rectangle (cr,
					 0,
					 height * progress / 2,
					 width,
					 height * (1 - progress));
		} else {
			cairo_rectangle (cr,
					 width * progress / 2,
					 0,
					 width * (1 - progress),
					 height);
		}

		cairo_clip (cr);

		paint_surface (cr, priv->origin_surface, 0, 0, 0, page_area);
	} else {
		paint_surface (cr, priv->origin_surface, 0, 0, 0, page_area);

		if (alignment == EV_TRANSITION_ALIGNMENT_HORIZONTAL) {
			cairo_rectangle (cr,
					 0,
					 (height / 2) - (height * progress / 2),
					 width,
					 height * progress);
		} else {
			cairo_rectangle (cr,
					 (width / 2) - (width * progress / 2),
					 0,
					 width * progress,
					 height);
		}

		cairo_clip (cr);

		paint_surface (cr, priv->dest_surface, 0, 0, 0, page_area);
	}
}

void
ev_transition_animation_paint (EvTransitionAnimation *animation,
			       cairo_t               *cr,
			       GdkRectangle           page_area)
{
	EvTransitionAnimationPriv *priv;
	EvTransitionEffectType type;
	gdouble progress;

	g_return_if_fail (EV_IS_TRANSITION_ANIMATION (animation));

	priv = EV_TRANSITION_ANIMATION_GET_PRIVATE (animation);
	g_object_get (priv->effect, "type", &type, NULL);
	progress = ev_timeline_get_progress (EV_TIMELINE (animation));

	if (!priv->dest_surface) {
		/* animation is still not ready, paint the origin surface */
		paint_surface (cr, priv->origin_surface, 0, 0, 0, page_area);
		return;
	}

	switch (type) {
	case EV_TRANSITION_EFFECT_REPLACE:
		/* just paint the destination slide */
		paint_surface (cr, priv->dest_surface, 0, 0, 0, page_area);
		break;
	case EV_TRANSITION_EFFECT_SPLIT:
		ev_transition_animation_split (cr, animation, priv->effect, progress, page_area);
		break;
	default: {
		GEnumValue *enum_value;

		enum_value = g_enum_get_value (g_type_class_peek (EV_TYPE_TRANSITION_EFFECT_TYPE), type);

		g_warning ("Unimplemented transition animation: '%s', "
			   "please post a bug report in Evince bugzilla "
			   "(http://bugzilla.gnome.org) with a testcase.",
			   enum_value->value_nick);

		/* just paint the destination slide */
		paint_surface (cr, priv->dest_surface, 0, 0, 0, page_area);
		}
	}
}

EvTransitionAnimation *
ev_transition_animation_new (EvTransitionEffect *effect)
{
	g_return_val_if_fail (EV_IS_TRANSITION_EFFECT (effect), NULL);

	return g_object_new (EV_TYPE_TRANSITION_ANIMATION,
			     "effect", effect,
			     NULL);
}

void
ev_transition_animation_set_origin_surface (EvTransitionAnimation *animation,
					    cairo_surface_t       *origin_surface)
{
	EvTransitionAnimationPriv *priv;
	cairo_surface_t *surface;

	g_return_if_fail (EV_IS_TRANSITION_ANIMATION (animation));

	priv = EV_TRANSITION_ANIMATION_GET_PRIVATE (animation);

	surface = cairo_surface_reference (origin_surface);

	if (priv->origin_surface)
		cairo_surface_destroy (priv->origin_surface);

	priv->origin_surface = surface;
	g_object_notify (G_OBJECT (animation), "origin-surface");

	if (priv->origin_surface && priv->dest_surface)
		ev_timeline_start (EV_TIMELINE (animation));
}

void
ev_transition_animation_set_dest_surface (EvTransitionAnimation *animation,
					  cairo_surface_t       *dest_surface)
{
	EvTransitionAnimationPriv *priv;
	cairo_surface_t *surface;

	g_return_if_fail (EV_IS_TRANSITION_ANIMATION (animation));

	priv = EV_TRANSITION_ANIMATION_GET_PRIVATE (animation);

	surface = cairo_surface_reference (dest_surface);

	if (priv->dest_surface)
		cairo_surface_destroy (priv->dest_surface);

	priv->dest_surface = surface;
	g_object_notify (G_OBJECT (animation), "dest-surface");

	if (priv->origin_surface && priv->dest_surface)
		ev_timeline_start (EV_TIMELINE (animation));
}
