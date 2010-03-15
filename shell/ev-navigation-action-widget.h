/*
 *  Copyright (C) 2003, 2004 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004 Christian Persch
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

#include <gtk/gtk.h>

#define EV_TYPE_NAVIGATION_ACTION_WIDGET (ev_navigation_action_widget_get_type ())
#define EV_NAVIGATION_ACTION_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_NAVIGATION_ACTION_WIDGET, EvNavigationActionWidget))

typedef struct _EvNavigationActionWidget EvNavigationActionWidget;
typedef struct _EvNavigationActionWidgetClass EvNavigationActionWidgetClass;

struct _EvNavigationActionWidget
{
	GtkToggleToolButton parent;

	GtkMenu *menu;
};

struct _EvNavigationActionWidgetClass
{
	GtkToggleToolButtonClass parent_class;
	
	void  (*show_menu) (EvNavigationActionWidget *widget);
};

GType ev_navigation_action_widget_get_type   (void);

void
ev_navigation_action_widget_set_menu(EvNavigationActionWidget *widget, GtkWidget *menu);
