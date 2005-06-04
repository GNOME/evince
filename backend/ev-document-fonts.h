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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef EV_DOCUMENT_FONTS_H
#define EV_DOCUMENT_FONTS_H

#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "ev-document.h"
#include "ev-link.h"

G_BEGIN_DECLS


#define EV_TYPE_DOCUMENT_FONTS		  (ev_document_fonts_get_type ())
#define EV_DOCUMENT_FONTS(o)		  (G_TYPE_CHECK_INSTANCE_CAST ((o), EV_TYPE_DOCUMENT_FONTS, EvDocumentFonts))
#define EV_DOCUMENT_FONTS_IFACE(k)	  (G_TYPE_CHECK_CLASS_CAST((k), EV_TYPE_DOCUMENT_FONTS, EvDocumentFontsIface))
#define EV_IS_DOCUMENT_FONTS(o)		  (G_TYPE_CHECK_INSTANCE_TYPE ((o), EV_TYPE_DOCUMENT_FONTS))
#define EV_IS_DOCUMENT_FONTS_IFACE(k)	  (G_TYPE_CHECK_CLASS_TYPE ((k), EV_TYPE_DOCUMENT_FONTS))
#define EV_DOCUMENT_FONTS_GET_IFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EV_TYPE_DOCUMENT_FONTS, EvDocumentFontsIface))

typedef struct _EvDocumentFonts	     EvDocumentFonts;
typedef struct _EvDocumentFontsIface EvDocumentFontsIface;

enum {
	EV_DOCUMENT_FONTS_COLUMN_NAME,
	EV_DOCUMENT_FONTS_COLUMN_NUM_COLUMNS
};

struct _EvDocumentFontsIface
{
	GTypeInterface base_iface;

	/* Methods  */
	GtkTreeModel *(* get_fonts_model)    (EvDocumentFonts      *document_fonts);
};

GType         ev_document_fonts_get_type           (void);
GtkTreeModel *ev_document_fonts_get_fonts_model    (EvDocumentFonts *document_fonts);

G_END_DECLS

#endif
