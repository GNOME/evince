/* ev-search-box.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2015 Igalia S.L.
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
#include <adwaita.h>
#include <evince-document.h>
#include <evince-view.h>

G_BEGIN_DECLS

#define EV_TYPE_SEARCH_BOX            (ev_search_box_get_type ())

G_DECLARE_FINAL_TYPE (EvSearchBox, ev_search_box, EV, SEARCH_BOX, AdwBin);

struct _EvSearchBox {
        AdwBin parent;
};

GtkWidget      *ev_search_box_new         (EvDocumentModel *model);
GtkSearchEntry *ev_search_box_get_entry   (EvSearchBox     *box);
gboolean        ev_search_box_has_results (EvSearchBox     *box);
void            ev_search_box_restart     (EvSearchBox     *box);

G_END_DECLS
