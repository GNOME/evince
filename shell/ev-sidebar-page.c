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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>

#include "ev-sidebar-page.h"

G_DEFINE_INTERFACE (EvSidebarPage, ev_sidebar_page, 0)

gboolean
ev_sidebar_page_support_document (EvSidebarPage *sidebar_page,
				  EvDocument    *document)
{
        EvSidebarPageInterface *iface;

	g_return_val_if_fail (EV_IS_SIDEBAR_PAGE (sidebar_page), FALSE);
        g_return_val_if_fail (EV_IS_DOCUMENT (document), FALSE);

	iface = EV_SIDEBAR_PAGE_GET_IFACE (sidebar_page);

        g_return_val_if_fail (iface->support_document, FALSE);

        return iface->support_document (sidebar_page, document);
}

void
ev_sidebar_page_set_model (EvSidebarPage   *sidebar_page,
			   EvDocumentModel *model)
{
	EvSidebarPageInterface *iface;

        g_return_if_fail (EV_IS_SIDEBAR_PAGE (sidebar_page));
	g_return_if_fail (EV_IS_DOCUMENT_MODEL (model));

	iface = EV_SIDEBAR_PAGE_GET_IFACE (sidebar_page);

	g_assert (iface->set_model);

	iface->set_model (sidebar_page, model);
}

static void
ev_sidebar_page_default_init (EvSidebarPageInterface *iface)
{
	static gboolean initialized = FALSE;

	if (!initialized) {
		g_object_interface_install_property (iface,
						     g_param_spec_object ("main-widget",
									  "Main Widget",
									  "Main page widget, used to handle focus",
									  GTK_TYPE_WIDGET,
									  G_PARAM_READABLE |
                                                                          G_PARAM_STATIC_STRINGS));
		g_object_interface_install_property (iface,
						     g_param_spec_object ("document-model",
									  "DocumentModel",
									  "The document model",
									  EV_TYPE_DOCUMENT_MODEL,
									  G_PARAM_WRITABLE |
									  G_PARAM_CONSTRUCT_ONLY |
									  G_PARAM_STATIC_STRINGS));
		initialized = TRUE;
	}
}
