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

#define EV_LAYER_GET_PRIVATE(object) \
                (G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_LAYER, EvLayerPrivate))

G_DEFINE_TYPE (EvLayer, ev_layer, G_TYPE_OBJECT)

static void
ev_layer_class_init (EvLayerClass *klass)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (g_object_class, sizeof (EvLayerPrivate));
}

static void
ev_layer_init (EvLayer *layer)
{
	layer->priv = EV_LAYER_GET_PRIVATE (layer);
}

EvLayer *
ev_layer_new (gboolean is_parent,
	      gint     rb_group)
{
	EvLayer *layer;

	layer = EV_LAYER (g_object_new (EV_TYPE_LAYER, NULL));
	layer->priv->is_parent = is_parent;
	layer->priv->rb_group = rb_group;

	return layer;
}

gboolean
ev_layer_is_parent (EvLayer *layer)
{
	g_return_val_if_fail (EV_IS_LAYER (layer), FALSE);

	return layer->priv->is_parent;
}

gint
ev_layer_get_rb_group (EvLayer *layer)
{
	g_return_val_if_fail (EV_IS_LAYER (layer), 0);

	return layer->priv->rb_group;
}
