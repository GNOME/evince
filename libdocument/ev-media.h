/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2015 Igalia S.L.
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
#include "ev-page.h"

G_BEGIN_DECLS

#define EV_TYPE_MEDIA              (ev_media_get_type())

EV_PUBLIC
G_DECLARE_FINAL_TYPE (EvMedia, ev_media, EV, MEDIA, GObject)

struct _EvMedia {
        GObject base_instance;
};

EV_PUBLIC
EvMedia     *ev_media_new_for_uri       (EvPage      *page,
                                         const gchar *uri);
EV_PUBLIC
const gchar *ev_media_get_uri           (EvMedia     *media);
EV_PUBLIC
guint        ev_media_get_page_index    (EvMedia     *media);
EV_PUBLIC
gboolean     ev_media_get_show_controls (EvMedia     *media);
EV_PUBLIC
void         ev_media_set_show_controls (EvMedia     *media,
                                         gboolean     show_controls);

G_END_DECLS
