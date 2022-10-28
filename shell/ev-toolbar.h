/* ev-toolbar.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2012 Carlos Garcia Campos <carlosgc@gnome.org>
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
#include "ev-window.h"

G_BEGIN_DECLS

typedef enum {
	EV_TOOLBAR_MODE_NORMAL,
	EV_TOOLBAR_MODE_RECENT_VIEW,
	EV_TOOLBAR_MODE_PASSWORD_VIEW
} EvToolbarMode;

#define EV_TYPE_TOOLBAR              (ev_toolbar_get_type())
G_DECLARE_FINAL_TYPE (EvToolbar, ev_toolbar, EV, TOOLBAR, AdwBin);

struct _EvToolbar {
        AdwBin base_instance;
};

GtkWidget    *ev_toolbar_new                (void);
void          ev_toolbar_action_menu_toggle (EvToolbar *ev_toolbar);
GtkWidget    *ev_toolbar_get_page_selector  (EvToolbar *ev_toolbar);
AdwHeaderBar *ev_toolbar_get_header_bar     (EvToolbar *ev_toolbar);
void          ev_toolbar_set_mode           (EvToolbar     *ev_toolbar,
					     EvToolbarMode  mode);
EvToolbarMode ev_toolbar_get_mode           (EvToolbar     *ev_toolbar);

G_END_DECLS
