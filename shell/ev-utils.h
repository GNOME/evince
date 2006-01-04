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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __EV_UTILS_H__
#define __EV_UTILS_H__

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <libgnomeprintui/gnome-print-dialog.h>

G_BEGIN_DECLS

GdkPixbuf*		ev_pixbuf_add_shadow (GdkPixbuf *src, int size,
					      int x_offset, int y_offset, double opacity);

void			ev_print_region_contents (GdkRegion *region);


GnomePrintConfig* 	load_print_config_from_file (void);
void       		save_print_config_to_file (GnomePrintConfig *config);
gboolean		using_postscript_printer (GnomePrintConfig *config);
gboolean		using_pdf_printer (GnomePrintConfig *config);

G_END_DECLS

#endif /* __EV_VIEW_H__ */
