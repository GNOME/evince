/* ev-document-fonts.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2004 Red Hat, Inc.
 *
 * Author:
 *   Marco Pesenti Gritti <mpg@redhat.com>
 *
 * Evince is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Evince is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "ev-macros.h"
#include "ev-document.h"
#include "ev-link.h"

G_BEGIN_DECLS

#define EV_TYPE_DOCUMENT_FONTS		  (ev_document_fonts_get_type ())

EV_PUBLIC
G_DECLARE_INTERFACE (EvDocumentFonts, ev_document_fonts, EV, DOCUMENT_FONTS, GObject)

enum {
	EV_DOCUMENT_FONTS_COLUMN_NAME,
	EV_DOCUMENT_FONTS_COLUMN_DETAILS,
	EV_DOCUMENT_FONTS_COLUMN_NUM_COLUMNS
};

struct _EvDocumentFontsInterface
{
	GTypeInterface base_iface;

        /* Methods */
        void         (* scan)              (EvDocumentFonts *document_fonts);
        void         (* fill_model)        (EvDocumentFonts *document_fonts,
                                            GtkTreeModel    *model);
        const gchar *(* get_fonts_summary) (EvDocumentFonts *document_fonts);
};

EV_PUBLIC
void         ev_document_fonts_scan              (EvDocumentFonts *document_fonts);
EV_PUBLIC
void         ev_document_fonts_fill_model        (EvDocumentFonts *document_fonts,
                                                  GtkTreeModel    *model);
EV_PUBLIC
const gchar *ev_document_fonts_get_fonts_summary (EvDocumentFonts *document_fonts);

G_END_DECLS
