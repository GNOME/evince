/*
 * Copyright (C) 2026 Germán Poo-Caamaño <gpoo@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#include <evince-document.h>

G_BEGIN_DECLS

gchar           *ev_test_get_uri                (const gchar     *filename);
EvDocument      *ev_test_load_document          (const gchar     *filename,
						 GError         **error);
EvDocument      *ev_test_load_document_or_fail  (const gchar     *filename);
gchar           *ev_test_get_page_text          (EvDocument      *document,
						 gint             index);
cairo_surface_t *ev_test_render_page            (EvDocument      *document,
						 gint             index,
						 gint             rotation,
						 gdouble          scale);
void             ev_test_assert_pixel           (cairo_surface_t *surface,
						 gint             x,
						 gint             y,
						 guint32          rgb);

G_END_DECLS
