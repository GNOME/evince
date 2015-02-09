/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2015 Lauri Kasanen
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

#ifndef EV_MARGIN_CACHE_H
#define EV_MARGIN_CACHE_H

#include <glib-object.h>
#include <gdk/gdk.h>
#include <evince-document.h>
#include <evince-view.h>


GdkRectangle      *ev_margin_cache_new                  (EvView            *view);
gboolean           ev_margin_cache_is_page_cached       (GdkRectangle      *cache,
                                                         gint               page);
void               ev_margin_cache_free                 (EvView            *view);

#endif /* EV_MARGINS_H */
