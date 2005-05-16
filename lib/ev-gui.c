/*
 *  Copyright (C) 2002 Marco Pesenti Gritti
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
 *  $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>

#include "ev-gui.h"

static void
ev_gui_sanitise_popup_position (GtkMenu *menu,
				GtkWidget *widget,
				gint *x,
				gint *y)
{
	GdkScreen *screen = gtk_widget_get_screen (widget);
	gint monitor_num;
	GdkRectangle monitor;
	GtkRequisition req;

	g_return_if_fail (widget != NULL);

	gtk_widget_size_request (GTK_WIDGET (menu), &req);

	monitor_num = gdk_screen_get_monitor_at_point (screen, *x, *y);
	gtk_menu_set_monitor (menu, monitor_num);
	gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

	*x = CLAMP (*x, monitor.x, monitor.x + MAX (0, monitor.width - req.width));
	*y = CLAMP (*y, monitor.y, monitor.y + MAX (0, monitor.height - req.height));
}

void
ev_gui_menu_position_tree_selection (GtkMenu   *menu,
				     gint      *x,
				     gint      *y,
				     gboolean  *push_in,
				     gpointer  user_data)
{
	GtkTreeSelection *selection;
	GList *selected_rows;
	GtkTreeModel *model;
	GtkTreeView *tree_view = GTK_TREE_VIEW (user_data);
	GtkWidget *widget = GTK_WIDGET (user_data);
	GtkRequisition req;
	GdkRectangle visible;

	gtk_widget_size_request (GTK_WIDGET (menu), &req);
	gdk_window_get_origin (widget->window, x, y);

	*x += (widget->allocation.width - req.width) / 2;

	/* Add on height for the treeview title */
	gtk_tree_view_get_visible_rect (tree_view, &visible);
	*y += widget->allocation.height - visible.height;

	selection = gtk_tree_view_get_selection (tree_view);
	selected_rows = gtk_tree_selection_get_selected_rows (selection, &model);
	if (selected_rows)
	{
		GdkRectangle cell_rect;

		gtk_tree_view_get_cell_area (tree_view, selected_rows->data,
					     NULL, &cell_rect);

		*y += CLAMP (cell_rect.y + cell_rect.height, 0, visible.height);

		g_list_foreach (selected_rows, (GFunc)gtk_tree_path_free, NULL);
		g_list_free (selected_rows);
	}

	ev_gui_sanitise_popup_position (menu, widget, x, y);
}
