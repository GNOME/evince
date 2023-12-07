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

#pragma once

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#include <glib-object.h>

#include "ev-macros.h"

G_BEGIN_DECLS

#define EV_TYPE_LAYER              (ev_layer_get_type())

EV_PUBLIC
G_DECLARE_FINAL_TYPE (EvLayer, ev_layer, EV, LAYER, GObject)

struct _EvLayer {
	GObject base_instance;
};

EV_PUBLIC
EvLayer  *ev_layer_new          (gboolean is_parent,
				 gint     rb_group);
EV_PUBLIC
gboolean  ev_layer_is_parent    (EvLayer *layer);
EV_PUBLIC
gint      ev_layer_get_rb_group (EvLayer *layer);

G_END_DECLS
