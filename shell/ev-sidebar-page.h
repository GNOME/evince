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

#ifndef EV_SIDEBAR_PAGE_H
#define EV_SIDEBAR_PAGE_H

#include <glib-object.h>
#include <glib.h>

#include "ev-document.h"
#include "ev-document-model.h"

G_BEGIN_DECLS

#define EV_TYPE_SIDEBAR_PAGE	    	(ev_sidebar_page_get_type ())
#define EV_SIDEBAR_PAGE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EV_TYPE_SIDEBAR_PAGE, EvSidebarPage))
#define EV_SIDEBAR_PAGE_IFACE(k)	(G_TYPE_CHECK_CLASS_CAST((k), EV_TYPE_SIDEBAR_PAGE, EvSidebarPageInterface))
#define EV_IS_SIDEBAR_PAGE(o)	    	(G_TYPE_CHECK_INSTANCE_TYPE ((o), EV_TYPE_SIDEBAR_PAGE))
#define EV_IS_SIDEBAR_PAGE_IFACE(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EV_TYPE_SIDEBAR_PAGE))
#define EV_SIDEBAR_PAGE_GET_IFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EV_TYPE_SIDEBAR_PAGE, EvSidebarPageInterface))

typedef struct _EvSidebarPage	         EvSidebarPage;
typedef struct _EvSidebarPageInterface   EvSidebarPageInterface;

struct _EvSidebarPageInterface
{
	GTypeInterface base_iface;

	/* Methods  */
	gboolean    (* support_document)  (EvSidebarPage   *sidebar_page,
				           EvDocument *document);
	void 	    (* set_model)	  (EvSidebarPage   *sidebar_page,
					   EvDocumentModel *model);
	const gchar*(* get_label)         (EvSidebarPage  *sidebar_page);
};

GType         ev_sidebar_page_get_type          (void) G_GNUC_CONST;
gboolean      ev_sidebar_page_support_document  (EvSidebarPage    *sidebar_page,
	 			                 EvDocument *document);
void          ev_sidebar_page_set_model         (EvSidebarPage    *sidebar_page,
				                 EvDocumentModel *model);
const gchar*  ev_sidebar_page_get_label         (EvSidebarPage *page);


G_END_DECLS

#endif /* EV_SIDEBAR_PAGE */
