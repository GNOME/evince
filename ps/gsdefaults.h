/*
 * gsdefaults.h: default settings of a GtkGS widget.
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
#ifndef __GS_DEFAULTS_H__
#define __GS_DEFAULTS_H__

#include <gnome.h>

G_BEGIN_DECLS

/* defaults accessors */
void gtk_gs_defaults_set_size(gint iNewPageSize);
void gtk_gs_defaults_set_override_size(gboolean bOverrSize);
void gtk_gs_defaults_set_antialiased(gint iNewAntialiased);
void gtk_gs_defaults_set_orientation(gint);
void gtk_gs_defaults_set_override_orientation(gboolean bOverOrien);
void gtk_gs_defaults_set_zoom_factor(gfloat fZoom);
void gtk_gs_defaults_set_respect_eof(gboolean resp);
void gtk_gs_defaults_set_scroll_step(gfloat step);
void gtk_gs_defaults_set_show_scroll_rect(gboolean f);
gint gtk_gs_defaults_get_size(void);
gboolean gtk_gs_defaults_get_override_size(void);
gboolean gtk_gs_defaults_get_antialiased(void);
gint gtk_gs_defaults_get_orientation(void);
gboolean gtk_gs_defaults_get_override_orientation(void);
gfloat gtk_gs_defaults_get_zoom_factor(void);
gboolean gtk_gs_defaults_get_respect_eof(void);
gfloat gtk_gs_defaults_get_scroll_step(void);
gboolean gtk_gs_defaults_get_show_scroll_rect(void);
GtkGSPaperSize *gtk_gs_defaults_get_paper_sizes(void);
const gchar *gtk_gs_defaults_get_interpreter_cmd(void);
const gchar *gtk_gs_defaults_get_convert_pdf_cmd(void);
const gchar *gtk_gs_defaults_get_dsc_cmd(void);
const gchar *gtk_gs_defaults_get_ungzip_cmd(void);
const gchar *gtk_gs_defaults_get_unbzip2_cmd(void);
const gchar *gtk_gs_defaults_get_alpha_parameters(void);
void gtk_gs_defaults_set_interpreter_cmd(gchar *);
void gtk_gs_defaults_set_convert_pdf_cmd(gchar *);
void gtk_gs_defaults_set_dsc_cmd(gchar *);
void gtk_gs_defaults_set_ungzip_cmd(gchar *);
void gtk_gs_defaults_set_unbzip2_cmd(gchar *);
void gtk_gs_defaults_set_alpha_parameters(gchar *);
GtkGSZoomMode gtk_gs_defaults_get_zoom_mode(void);
void gtk_gs_defaults_set_zoom_mode(GtkGSZoomMode zoom_mode);

/* prefs IO */
void gtk_gs_defaults_load(void);

GConfClient *gtk_gs_defaults_gconf_client(void);

G_END_DECLS

#endif /* __GS_DEFAULTS_H__ */
