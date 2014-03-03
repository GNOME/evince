/*
 * Copyright (C) 2007 Carlos Garcia Campos <carlosgc@gnome.org>
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

#ifndef MDVI_CAIRO_DEVICE
#define MDVI_CAIRO_DEVICE

#include <glib.h>
#include <cairo.h>

#include "mdvi.h"

G_BEGIN_DECLS

void             mdvi_cairo_device_init        (DviDevice *device);
void             mdvi_cairo_device_free        (DviDevice *device);
cairo_surface_t *mdvi_cairo_device_get_surface (DviDevice *device);
void             mdvi_cairo_device_render      (DviContext* dvi);
void             mdvi_cairo_device_set_margins (DviDevice *device,
						gint       xmargin,
						gint       ymargin);
void             mdvi_cairo_device_set_scale   (DviDevice *device,
                                                gdouble    xscale,
                                                gdouble    yscale);

G_END_DECLS

#endif /* MDVI_CAIRO_DEVICE */
