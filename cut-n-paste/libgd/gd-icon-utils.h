/*
 * Copyright (c) 2011, 2012 Red Hat, Inc.
 *
 * Gnome Documents is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * Gnome Documents is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with Gnome Documents; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#ifndef __GD_CREATE_SYMBOLIC_ICON_H__
#define __GD_CREATE_SYMBOLIC_ICON_H__

#include <gtk/gtk.h>

GIcon *gd_create_symbolic_icon (const gchar *name,
                                gint base_size);

cairo_surface_t *gd_embed_image_in_frame (cairo_surface_t *source_image,
                                          const gchar *frame_image_url,
                                          GtkBorder *slice_width,
                                          GtkBorder *border_width);

#endif /* __GD_CREATE_SYMBOLIC_ICON_H__ */
