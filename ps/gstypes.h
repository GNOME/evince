/*
 * Ghostscript widget for GTK/GNOME
 * 
 * Copyright 1998 - 2005 The Free Software Foundation
 * 
 * Authors: Jaka Mocnik, Federico Mena (Quartic), Szekeres Istvan (Pista)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GSTYPES_H__
#define __GSTYPES_H__

G_BEGIN_DECLS

typedef struct _GtkGSPaperSize GtkGSPaperSize;

typedef enum {
  GTK_GS_ORIENTATION_NONE = -1,
  GTK_GS_ORIENTATION_PORTRAIT = 0,
  GTK_GS_ORIENTATION_SEASCAPE = 3,
  GTK_GS_ORIENTATION_UPSIDEDOWN = 2,
  GTK_GS_ORIENTATION_LANDSCAPE = 1
} GtkGSOrientation;

typedef enum {
  GTK_GS_ZOOM_ABSOLUTE = 0,
  GTK_GS_ZOOM_FIT_WIDTH = 1,
  GTK_GS_ZOOM_FIT_PAGE = 2
} GtkGSZoomMode;

struct _GtkGSPaperSize {
  gchar *name;
  gint width, height;
};

G_END_DECLS

#endif /* __GSTYPES_H__ */
