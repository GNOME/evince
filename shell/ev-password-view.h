/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2005 Red Hat, Inc
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
#include <gio/gio.h>

G_BEGIN_DECLS

#define EV_TYPE_PASSWORD_VIEW            (ev_password_view_get_type ())
G_DECLARE_FINAL_TYPE (EvPasswordView, ev_password_view, EV, PASSWORD_VIEW, GtkBox);

struct _EvPasswordView {
	GtkBox parent_instance;
};

EvPasswordView *ev_password_view_new                     (void);
void            ev_password_view_set_filename            (EvPasswordView *password_view,
							  const char     *filename);
void            ev_password_view_ask_password            (EvPasswordView *password_view);
const gchar    *ev_password_view_get_password            (EvPasswordView *password_view);
GPasswordSave   ev_password_view_get_password_save_flags (EvPasswordView *password_view);

G_END_DECLS
