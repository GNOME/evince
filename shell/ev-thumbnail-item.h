/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2022 Qiu Wenbo
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

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EV_TYPE_THUMBNAIL_ITEM              (ev_thumbnail_item_get_type ())
G_DECLARE_FINAL_TYPE (EvThumbnailItem, ev_thumbnail_item, EV, THUMBNAIL_ITEM, GObject);

struct _EvThumbnailItem
{
        GObject parent;
};

void ev_thumbnail_item_set_primary_text (EvThumbnailItem *ev_thumbnail_item,
					 const gchar *primary_text);
void ev_thumbnail_item_set_secondary_text (EvThumbnailItem *ev_thumbnail_item,
					   const gchar *secondary_text);
void ev_thumbnail_item_set_paintable (EvThumbnailItem *ev_thumbnail_item, GdkPaintable *paintable);

G_END_DECLS
