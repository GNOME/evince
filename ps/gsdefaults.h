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

#include <glib.h>

#include "gstypes.h"

G_BEGIN_DECLS

/* defaults accessors */

GtkGSPaperSize *gtk_gs_defaults_get_paper_sizes(void);
const gchar *gtk_gs_defaults_get_interpreter_cmd(void);
const gchar *gtk_gs_defaults_get_alpha_parameters(void);
const gchar *gtk_gs_defaults_get_ungzip_cmd(void);
const gchar *gtk_gs_defaults_get_unbzip2_cmd(void);

G_END_DECLS

#endif /* __GS_DEFAULTS_H__ */
