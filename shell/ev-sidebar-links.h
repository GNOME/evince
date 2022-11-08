/* ev-sidebar-links.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2004 Red Hat, Inc.
 *
 * Author:
 *   Jonathan Blandford <jrb@alum.mit.edu>
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

#include <gtk/gtk.h>

#include "ev-document.h"
#include "ev-link.h"
#include "ev-utils.h"

G_BEGIN_DECLS

typedef struct _EvSidebarLinks EvSidebarLinks;
typedef struct _EvSidebarLinksClass EvSidebarLinksClass;
typedef struct _EvSidebarLinksPrivate EvSidebarLinksPrivate;

#define EV_TYPE_SIDEBAR_LINKS		   (ev_sidebar_links_get_type())
#define EV_SIDEBAR_LINKS(object)	   (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_SIDEBAR_LINKS, EvSidebarLinks))
#define EV_SIDEBAR_LINKS_CLASS(klass)	   (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_SIDEBAR_LINKS, EvSidebarLinksClass))
#define EV_IS_SIDEBAR_LINKS(object)	   (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_SIDEBAR_LINKS))
#define EV_IS_SIDEBAR_LINKS_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_SIDEBAR_LINKS))
#define EV_SIDEBAR_LINKS_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_SIDEBAR_LINKS, EvSidebarLinksClass))

struct _EvSidebarLinks {
	GtkBox base_instance;

	EvSidebarLinksPrivate *priv;
};

struct _EvSidebarLinksClass {
	GtkBoxClass base_class;

	void    (* link_activated) (EvSidebarLinks *sidebar_links,
				    EvLink         *link);
};

GType      ev_sidebar_links_get_type       (void);
GtkWidget *ev_sidebar_links_new            (void);

G_END_DECLS
