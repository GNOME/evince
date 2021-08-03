/* ev-find-sidebar.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2013 Carlos Garcia Campos  <carlosgc@gnome.org>
 * Copyright (C) 2008 Sergey Pushkin  <pushkinsv@gmail.com >
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

#include "ev-jobs.h"

G_BEGIN_DECLS

#define EV_TYPE_FIND_SIDEBAR              (ev_find_sidebar_get_type ())
#define EV_FIND_SIDEBAR(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), EV_TYPE_FIND_SIDEBAR, EvFindSidebar))
#define EV_IS_FIND_SIDEBAR(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), EV_TYPE_FIND_SIDEBAR))
#define EV_FIND_SIDEBAR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_FIND_SIDEBAR, EvFindSidebarClass))
#define EV_IS_FIND_SIDEBAR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_FIND_SIDEBAR))
#define EV_FIND_SIDEBAR_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS ((object), EV_TYPE_FIND_SIDEBAR, EvFindSidebarClass))

typedef struct _EvFindSidebar EvFindSidebar;
typedef struct _EvFindSidebarClass EvFindSidebarClass;

struct _EvFindSidebar {
        GtkBox base_instance;
};

struct _EvFindSidebarClass {
        GtkBoxClass base_class;
};

GType      ev_find_sidebar_get_type (void);
GtkWidget *ev_find_sidebar_new      (void);
void       ev_find_sidebar_start    (EvFindSidebar *find_sidebar,
                                     EvJobFind     *job);
void       ev_find_sidebar_restart  (EvFindSidebar *find_sidebar,
                                     gint           page);
void       ev_find_sidebar_update   (EvFindSidebar *find_sidebar);
void       ev_find_sidebar_clear    (EvFindSidebar *find_sidebar);
void       ev_find_sidebar_previous (EvFindSidebar *find_sidebar);
void       ev_find_sidebar_next     (EvFindSidebar *find_sidebar);

G_END_DECLS
