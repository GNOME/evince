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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>

#include "ev-sidebar-page.h"

static void ev_sidebar_page_iface_init (gpointer iface);

GType
ev_sidebar_page_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo sidebar_page_info =
		{
			sizeof (EvSidebarPageIface),
			NULL,
			NULL,
			(GClassInitFunc)ev_sidebar_page_iface_init,
		};

		type = g_type_register_static (G_TYPE_INTERFACE,
					       "EvSidebarPage",
					       &sidebar_page_info, (GTypeFlags)0);
	}

	return type;
}


gboolean 
ev_sidebar_page_support_document  (EvSidebarPage    *sidebar_page,
    		            	   EvDocument *document)
{
        EvSidebarPageIface *iface;

	g_return_val_if_fail (EV_IS_SIDEBAR_PAGE (sidebar_page), FALSE);
        g_return_val_if_fail (EV_IS_DOCUMENT (document), FALSE);

	iface = EV_SIDEBAR_PAGE_GET_IFACE (sidebar_page);

        g_return_val_if_fail (iface->set_document, FALSE);
	
        return iface->support_document (sidebar_page, document);    
}

void 
ev_sidebar_page_set_document      (EvSidebarPage    *sidebar_page,
		                   EvDocument *document)
{
	EvSidebarPageIface *iface;

        g_return_if_fail (EV_IS_SIDEBAR_PAGE (sidebar_page));
	g_return_if_fail (EV_IS_DOCUMENT (document));

	iface = EV_SIDEBAR_PAGE_GET_IFACE (sidebar_page);

	g_return_if_fail (iface->set_document);
	
	iface->set_document (sidebar_page, document);
	
	return;
}

const gchar*
ev_sidebar_page_get_label (EvSidebarPage    *sidebar_page)
{
	EvSidebarPageIface *iface;

        g_return_val_if_fail (EV_IS_SIDEBAR_PAGE (sidebar_page), NULL);

	iface = EV_SIDEBAR_PAGE_GET_IFACE (sidebar_page);

	g_return_val_if_fail (iface->get_label, NULL);
	
	return iface->get_label (sidebar_page);
}


static void ev_sidebar_page_iface_init (gpointer         iface)
{
	g_object_interface_install_property (iface,
					     g_param_spec_object ("main-widget",
					  		          "Main Widget",
							          "Main page widget, used to handle focus",
							          GTK_TYPE_WIDGET,
							          G_PARAM_READABLE));
}
