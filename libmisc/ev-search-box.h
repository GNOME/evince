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

#ifndef EV_SEARCH_BOX_H
#define EV_SEARCH_BOX_H

#include <gtk/gtk.h>
#include <evince-document.h>
#include <evince-view.h>

G_BEGIN_DECLS

#define EV_TYPE_SEARCH_BOX            (ev_search_box_get_type ())
#define EV_SEARCH_BOX(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_SEARCH_BOX, EvSearchBox))
#define EV_IS_SEARCH_BOX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_SEARCH_BOX))
#define EV_SEARCH_BOX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_SEARCH_BOX, EvSearchBoxClass))
#define EV_IS_SEARCH_BOX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EV_TYPE_SEARCH_BOX))
#define EV_SEARCH_BOX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EV_TYPE_SEARCH_BOX, EvSearchBoxClass))

typedef struct _EvSearchBox        EvSearchBox;
typedef struct _EvSearchBoxClass   EvSearchBoxClass;

struct _EvSearchBox {
        GtkBox parent;
};

struct _EvSearchBoxClass {
        GtkBoxClass parent_class;
};

GType           ev_search_box_get_type    (void);

GtkWidget      *ev_search_box_new         (EvDocumentModel *model);
GtkSearchEntry *ev_search_box_get_entry   (EvSearchBox     *box);
gboolean        ev_search_box_has_results (EvSearchBox     *box);
void            ev_search_box_restart     (EvSearchBox     *box);

G_END_DECLS

#endif
