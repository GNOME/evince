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

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#ifndef __EV_LAYER_H__
#define __EV_LAYER_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _EvLayer        EvLayer;
typedef struct _EvLayerClass   EvLayerClass;
typedef struct _EvLayerPrivate EvLayerPrivate;

#define EV_TYPE_LAYER              (ev_layer_get_type())
#define EV_LAYER(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_LAYER, EvLayer))
#define EV_LAYER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_LAYER, EvLayerClass))
#define EV_IS_LAYER(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_LAYER))
#define EV_IS_LAYER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_LAYER))
#define EV_LAYER_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_LAYER, EvLayerClass))

struct _EvLayer {
	GObject base_instance;
	
	EvLayerPrivate *priv;
};

struct _EvLayerClass {
	GObjectClass base_class;
};

GType     ev_layer_get_type     (void) G_GNUC_CONST;
EvLayer  *ev_layer_new          (gboolean is_parent,
				 gint     rb_group);
gboolean  ev_layer_is_parent    (EvLayer *layer);
gint      ev_layer_get_rb_group (EvLayer *layer);

G_END_DECLS

#endif /* __EV_LAYER_H__ */
