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

#include <gnome.h>

#include <gtkgs.h>

/*Define StockIds*/
#define GGV_CLEAR_ALL		"ggv-clear-all"
#define GGV_TOGGLE_ALL		"ggv-toggle-all"
#define GGV_TOGGLE_EVEN		"ggv-toggle-even"
#define GGV_TOGGLE_ODD		"ggv-toggle-odd"
#define GGV_FIT_WIDTH		"ggv-fit-width"
#define GGV_ZOOM		    "ggv-zoom"

extern GtkGSPaperSize ggv_paper_sizes[];
extern const gchar *ggv_orientation_labels[];
extern const gint ggv_max_orientation_labels;
extern const gfloat ggv_unit_factors[];
extern const gchar *ggv_unit_labels[];
extern const gint ggv_max_unit_labels;
extern gfloat ggv_zoom_levels[];
extern const gchar *ggv_zoom_level_names[];
extern const gint ggv_max_zoom_levels;
extern const gchar *ggv_auto_fit_modes[];
extern const gint ggv_max_auto_fit_modes;

/* zoom level index <-> zoom factor */
gint ggv_zoom_index_from_float(gfloat zoom_level);
gfloat ggv_zoom_level_from_index(gint index);

/* Split delimited string into a list of strings. */
GSList *ggv_split_string(const gchar * string, const gchar * delimiter);

/* Get index of a string from a list of them. */
gint ggv_get_index_of_string(gchar * string, gchar ** strings);

/* Quote filename for system call */
gchar *ggv_quote_filename(const gchar * str);

/* escape filename to conform to URI specification */
gchar *ggv_filename_to_uri(const gchar * fname);

/* If file exists and is a regular file then return its length, else -1 */
gint ggv_file_length(const gchar * filename);

/* Test if file exists, is a regular file and its length is > 0 */
gboolean ggv_file_readable(const char *filename);

/* Set a tooltip for a widget */
void ggv_set_tooltip(GtkWidget * w, const gchar * tip);

/* zoom <-> magstep (currently not used...) */
gfloat ggv_compute_zoom(gint zoom_spec);
gint ggv_compute_spec(gfloat zoom);

void ggv_raise_and_focus_widget(GtkWidget * widget);

void ggv_get_window_size(GtkWidget * widget, gint * width, gint * height);

void ggv_init_stock_icons(void);

#endif /* __GGV_UTILS_H__ */
