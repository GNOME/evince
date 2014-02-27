/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/*
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
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
 *  $Id$
 */

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#ifndef EV_DOCUMENT_MISC_H
#define EV_DOCUMENT_MISC_H

#include <cairo.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include "ev-macros.h"

G_BEGIN_DECLS

EV_DEPRECATED
GdkPixbuf *ev_document_misc_get_thumbnail_frame  (int           width,
						  int           height,
						  GdkPixbuf    *source_pixbuf);
EV_DEPRECATED
GdkPixbuf *ev_document_misc_get_loading_thumbnail (int      width,
						   int      height,
						   gboolean inverted_colors);

GdkPixbuf *ev_document_misc_render_loading_thumbnail    (GtkWidget *widget,
							 int        width,
							 int        height,
							 gboolean   inverted_colors);
GdkPixbuf *ev_document_misc_render_thumbnail_with_frame (GtkWidget *widget,
							 GdkPixbuf *source_pixbuf);

cairo_surface_t *ev_document_misc_render_loading_thumbnail_surface (GtkWidget *widget,
								    int        width,
								    int        height,
								    gboolean   inverted_colors);
cairo_surface_t *ev_document_misc_render_thumbnail_surface_with_frame (GtkWidget       *widget,
								       cairo_surface_t *source_surface,
								       int              width,
								       int              height);

EV_DEPRECATED
void       ev_document_misc_get_page_border_size (gint          page_width,
						  gint          page_height,
						  GtkBorder    *border);
EV_DEPRECATED
void       ev_document_misc_paint_one_page       (cairo_t      *cr,
						  GtkWidget    *widget,
						  GdkRectangle *area,
						  GtkBorder    *border,
						  gboolean      highlight,
						  gboolean      inverted_colors);

cairo_surface_t *ev_document_misc_surface_from_pixbuf (GdkPixbuf *pixbuf);
GdkPixbuf       *ev_document_misc_pixbuf_from_surface (cairo_surface_t *surface);
cairo_surface_t *ev_document_misc_surface_rotate_and_scale (cairo_surface_t *surface,
							    gint             dest_width,
							    gint             dest_height,
							    gint             dest_rotation);
void             ev_document_misc_invert_surface (cairo_surface_t *surface);
void		 ev_document_misc_invert_pixbuf  (GdkPixbuf       *pixbuf);

gdouble          ev_document_misc_get_screen_dpi (GdkScreen *screen);

gchar           *ev_document_misc_format_date (GTime utime);

void             ev_document_misc_get_pointer_position (GtkWidget *widget,
							gint      *x,
							gint      *y);

G_END_DECLS

#endif /* EV_DOCUMENT_MISC_H */
