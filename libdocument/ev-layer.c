/* this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2008 Carlos Garcia Campos <carlosgc@gnome.org>
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

#include "ev-layer.h"

struct _EvLayerPrivate {
	gboolean is_parent;
	gint     rb_group;
};

typedef struct _EvLayerPrivate EvLayerPrivate;

#define GET_PRIVATE(o) ev_layer_get_instance_private (o);

G_DEFINE_TYPE_WITH_PRIVATE (EvLayer, ev_layer, G_TYPE_OBJECT)

static void
ev_layer_class_init (EvLayerClass *klass)
{
}

static void
ev_layer_init (EvLayer *layer)
{
}

EvLayer *
ev_layer_new (gboolean is_parent,
	      gint     rb_group)
{
	EvLayer *layer;
	EvLayerPrivate *priv;

	layer = EV_LAYER (g_object_new (EV_TYPE_LAYER, NULL));
	priv = GET_PRIVATE (layer);
	priv->is_parent = is_parent;
	priv->rb_group = rb_group;

	return layer;
}

gboolean
ev_layer_is_parent (EvLayer *layer)
{
	g_return_val_if_fail (EV_IS_LAYER (layer), FALSE);
	EvLayerPrivate *priv = GET_PRIVATE (layer);

	return priv->is_parent;
}

gint
ev_layer_get_rb_group (EvLayer *layer)
{
	g_return_val_if_fail (EV_IS_LAYER (layer), 0);
	EvLayerPrivate *priv = GET_PRIVATE (layer);

	return priv->rb_group;
}
