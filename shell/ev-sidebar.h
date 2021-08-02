/* ev-sidebar.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2004 Red Hat, Inc.
 * Copyright (C) 2018 Germán Poo-Caamaño <gpoo@gnome.org>
 *
 * Author:
 *   Jonathan Blandford <jrb@alum.mit.edu>
 *   Germán Poo-Caamaño <gpoo@gnome.org>
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

#include "ev-document-model.h"

G_BEGIN_DECLS

#define EV_TYPE_SIDEBAR		     (ev_sidebar_get_type())
G_DECLARE_FINAL_TYPE (EvSidebar, ev_sidebar, EV, SIDEBAR, GtkBox);

struct _EvSidebar {
	GtkBox base_instance;
};

GtkWidget *ev_sidebar_new       (void);
void       ev_sidebar_set_page  (EvSidebar       *ev_sidebar,
                                 GtkWidget       *main_widget);
void       ev_sidebar_set_model (EvSidebar       *ev_sidebar,
                                 EvDocumentModel *model);
GtkWidget *ev_sidebar_get_current_page (EvSidebar *ev_sidebar);

G_END_DECLS
