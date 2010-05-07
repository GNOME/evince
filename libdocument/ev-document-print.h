/* ev-document-print.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2009 Carlos Garcia Campos  <carlosgc@gnome.org>
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

#ifndef EV_DOCUMENT_PRINT_H
#define EV_DOCUMENT_PRINT_H

#include <glib-object.h>
#include <cairo.h>

#include "ev-page.h"

G_BEGIN_DECLS

#define EV_TYPE_DOCUMENT_PRINT		   (ev_document_print_get_type ())
#define EV_DOCUMENT_PRINT(o)		   (G_TYPE_CHECK_INSTANCE_CAST ((o), EV_TYPE_DOCUMENT_PRINT, EvDocumentPrint))
#define EV_DOCUMENT_PRINT_IFACE(k)	   (G_TYPE_CHECK_CLASS_CAST((k), EV_TYPE_DOCUMENT_PRINT, EvDocumentPrintInterface))
#define EV_IS_DOCUMENT_PRINT(o)	           (G_TYPE_CHECK_INSTANCE_TYPE ((o), EV_TYPE_DOCUMENT_PRINT))
#define EV_IS_DOCUMENT_PRINT_IFACE(k)	   (G_TYPE_CHECK_CLASS_TYPE ((k), EV_TYPE_DOCUMENT_PRINT))
#define EV_DOCUMENT_PRINT_GET_IFACE(inst)  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EV_TYPE_DOCUMENT_PRINT, EvDocumentPrintInterface))

typedef struct _EvDocumentPrint          EvDocumentPrint;
typedef struct _EvDocumentPrintInterface EvDocumentPrintInterface;

struct _EvDocumentPrintInterface
{
	GTypeInterface base_iface;

	/* Methods  */
	void (* print_page) (EvDocumentPrint *document_print,
			     EvPage          *page,
			     cairo_t         *cr);
};

GType ev_document_print_get_type   (void) G_GNUC_CONST;

void  ev_document_print_print_page (EvDocumentPrint *document_print,
				    EvPage          *page,
				    cairo_t         *cr);

G_END_DECLS

#endif /* EV_DOCUMENT_PRINT_H */
