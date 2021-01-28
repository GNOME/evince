/* ev-sidebar.h
 *  this file is part of evince, a gnome document viewer
 * 
 * Copyright (C) 2004 Red Hat, Inc.
 * Copyright (C) 2018 Germán Poo-Caamaño <gpoo@gnome.org>
 *
 * Author:
 *   Jonathan Blandford <jrb@alum.mit.edu>
 *   Germán Poo-Caamaño <gpoo@gnome.org>
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

#ifndef __EV_SIDEBAR_H__
#define __EV_SIDEBAR_H__

#include <gtk/gtk.h>

#include "ev-document-model.h"

G_BEGIN_DECLS

typedef struct _EvSidebar EvSidebar;
typedef struct _EvSidebarClass EvSidebarClass;

#define EV_TYPE_SIDEBAR		     (ev_sidebar_get_type())
#define EV_SIDEBAR(object)	     (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_SIDEBAR, EvSidebar))
#define EV_SIDEBAR_CLASS(klass)	     (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_SIDEBAR, EvSidebarClass))
#define EV_IS_SIDEBAR(object)	     (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_SIDEBAR))
#define EV_IS_SIDEBAR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_SIDEBAR))
#define EV_SIDEBAR_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_SIDEBAR, EvSidebarClass))

struct _EvSidebar {
	GtkBox base_instance;
};

struct _EvSidebarClass {
	GtkBoxClass base_class;
};

GType      ev_sidebar_get_type  (void) G_GNUC_CONST;
GtkWidget *ev_sidebar_new       (void);
void       ev_sidebar_add_page  (EvSidebar       *ev_sidebar,
                                 GtkWidget       *widget,
                                 const gchar     *name,
                                 const gchar     *title,
                                 const gchar     *icon_name);
void       ev_sidebar_set_page  (EvSidebar       *ev_sidebar,
                                 GtkWidget       *main_widget);
void       ev_sidebar_set_model (EvSidebar       *ev_sidebar,
                                 EvDocumentModel *model);
GtkWidget *ev_sidebar_get_current_page (EvSidebar *ev_sidebar);

G_END_DECLS

#endif /* __EV_SIDEBAR_H__ */


