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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifndef EV_DOCUMENT_MISC_H
#define EV_DOCUMENT_MISC_H


#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkstyle.h>

G_BEGIN_DECLS


GdkPixbuf *ev_document_misc_get_thumbnail_frame  (int           width,
						  int           height,
						  int           rotation,
						  GdkPixbuf    *source_pixbuf);
void       ev_document_misc_get_page_border_size (gint          page_width,
						  gint          page_height,
						  GtkBorder    *border);
void       ev_document_misc_paint_one_page       (GdkDrawable  *drawable,
						  GtkWidget    *widget,
						  GdkRectangle *area,
						  GtkBorder    *border);

G_END_DECLS

#endif /* EV_DOCUMENT_MISC_H */
