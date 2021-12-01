/* ev-sidebar-layers.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2008 Carlos Garcia Campos  <carlosgc@gnome.org>
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

#include <gtk/gtk.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _EvSidebarLayers        EvSidebarLayers;
typedef struct _EvSidebarLayersClass   EvSidebarLayersClass;
typedef struct _EvSidebarLayersPrivate EvSidebarLayersPrivate;

#define EV_TYPE_SIDEBAR_LAYERS              (ev_sidebar_layers_get_type())
#define EV_SIDEBAR_LAYERS(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_SIDEBAR_LAYERS, EvSidebarLayers))
#define EV_SIDEBAR_LAYERS_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_SIDEBAR_LAYERS, EvSidebarLayersClass))
#define EV_IS_SIDEBAR_LAYERS(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_SIDEBAR_LAYERS))
#define EV_IS_SIDEBAR_LAYERS_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_SIDEBAR_LAYERS))
#define EV_SIDEBAR_LAYERS_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_SIDEBAR_LAYERS, EvSidebarLayersClass))

struct _EvSidebarLayers {
	GtkBox base_instance;

	EvSidebarLayersPrivate *priv;
};

struct _EvSidebarLayersClass {
	GtkBoxClass base_class;

	/* Signals */
	void (* layers_visibility_changed) (EvSidebarLayers *ev_layers);
};

GType      ev_sidebar_layers_get_type            (void) G_GNUC_CONST;
GtkWidget *ev_sidebar_layers_new                 (void);
void       ev_sidebar_layers_update_layers_state (EvSidebarLayers *sidebar_layers);

G_END_DECLS
