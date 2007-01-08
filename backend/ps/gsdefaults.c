/*
 * gsdefaults.c: default settings for the GtkGS widget
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <config.h>

#include <glib/gi18n.h>

#include "gsdefaults.h"

GtkGSPaperSize gtk_gs_paper_sizes[] = {
  {N_("BBox"), 0, 0},
  {N_("Letter"), 612, 792,},
  {N_("Tabloid"), 792, 1224,},
  {N_("Ledger"), 1224, 792,},
  {N_("Legal"), 612, 1008,},
  {N_("Statement"), 396, 612,},
  {N_("Executive"), 540, 720,},
  {N_("A0"), 2380, 3368,},
  {N_("A1"), 1684, 2380,},
  {N_("A2"), 1190, 1684,},
  {N_("A3"), 842, 1190,},
  {N_("A4"), 595, 842,},
  {N_("A5"), 420, 595,},
  {N_("B4"), 729, 1032,},
  {N_("B5"), 516, 729,},
  {N_("Folio"), 612, 936,},
  {N_("Quarto"), 610, 780,},
  {N_("10x14"), 720, 1008,},
  {NULL, 0, 0}
};

GtkGSPaperSize *
gtk_gs_defaults_get_paper_sizes()
{
	return gtk_gs_paper_sizes;
}

const gchar *
gtk_gs_defaults_get_ungzip_cmd (void)
{
	return "gzip -cd";
}

const gchar *
gtk_gs_defaults_get_unbzip2_cmd (void)
{
	return "bzip2 -cd";
}
