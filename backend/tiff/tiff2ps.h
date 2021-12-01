/*
 * Copyright (C) 2005 rpath, Inc.
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

#include <glib.h>
#include <stdio.h>
#include "tiffio.h"

typedef struct _TIFF2PSContext TIFF2PSContext;

TIFF2PSContext *tiff2ps_context_new(const gchar *filename);
void tiff2ps_process_page(TIFF2PSContext* ctx, TIFF* tif,
			  double pagewidth, double pageheight,
			  double leftmargin, double bottommargin,
			  gboolean center);
void tiff2ps_context_finalize(TIFF2PSContext* ctx);
