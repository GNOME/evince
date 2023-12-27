/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/*
 *  Copyright (C) 2005 Marco Pesenti Gritti
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include <glib-object.h>
#include <glib.h>

#include "ev-document.h"
#include "ev-document-model.h"

G_BEGIN_DECLS

#define EV_TYPE_SIDEBAR_PAGE	    	(ev_sidebar_page_get_type ())
G_DECLARE_INTERFACE (EvSidebarPage, ev_sidebar_page, EV, SIDEBAR_PAGE, GObject)

struct _EvSidebarPageInterface
{
	GTypeInterface base_iface;

	/* Methods  */
	gboolean    (* support_document)  (EvSidebarPage   *sidebar_page,
				           EvDocument *document);
	void 	    (* set_model)	  (EvSidebarPage   *sidebar_page,
					   EvDocumentModel *model);
};

gboolean      ev_sidebar_page_support_document  (EvSidebarPage    *sidebar_page,
	 			                 EvDocument *document);
void          ev_sidebar_page_set_model         (EvSidebarPage    *sidebar_page,
				                 EvDocumentModel *model);


G_END_DECLS
