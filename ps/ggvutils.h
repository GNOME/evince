/*
 * ggvutils.h: misc utility functions
 *
 * Copyright 2002 - 2005 the Free Software Foundation
 *
 * Author: Jaka Mocnik  <jaka@gnu.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __GGV_UTILS_H__
#define __GGV_UTILS_H__

#include "gtkgs.h"

extern GtkGSPaperSize ggv_paper_sizes[];
extern gfloat ggv_zoom_levels[];
extern const gchar *ggv_zoom_level_names[];
extern const gint ggv_max_zoom_levels;

/* If file exists and is a regular file then return its length, else -1 */
gint ggv_file_length(const gchar * filename);

/* Test if file exists, is a regular file and its length is > 0 */
gboolean ggv_file_readable(const char *filename);

#endif /* __GGV_UTILS_H__ */
