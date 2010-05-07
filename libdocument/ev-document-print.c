/* ev-document-print.c
 *  this file is part of evince, a gnome document_links viewer
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

#include "config.h"

#include "ev-document.h"
#include "ev-document-print.h"

G_DEFINE_INTERFACE (EvDocumentPrint, ev_document_print, 0)

static void
ev_document_print_default_init (EvDocumentPrintInterface *klass)
{
}

void
ev_document_print_print_page (EvDocumentPrint *document_print,
			      EvPage          *page,
			      cairo_t         *cr)
{
	EvDocumentPrintInterface *iface = EV_DOCUMENT_PRINT_GET_IFACE (document_print);

	iface->print_page (document_print, page, cr);
}
