/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/*
 *  Copyright (C) 2004 Anders Carlsson <andersca@gnome.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef __EV_UTILS_H__
#define __EV_UTILS_H__

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

GdkPixbuf*		ev_pixbuf_add_shadow (GdkPixbuf *src, int size,
					      int x_offset, int y_offset, double opacity);

void			ev_print_region_contents (cairo_region_t *region);

void 			ev_gui_menu_position_tree_selection (GtkMenu   *menu,
							     gint      *x,
							     gint      *y,
							     gboolean  *push_in,
							     gpointer   user_data);

void           		file_chooser_dialog_add_writable_pixbuf_formats (GtkFileChooser *chooser);
GdkPixbufFormat* 	get_gdk_pixbuf_format_by_extension (const gchar *uri);

G_END_DECLS

#endif /* __EV_VIEW_H__ */
