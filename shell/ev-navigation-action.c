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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include "ev-navigation-action.h"
#include "ev-navigation-action-widget.h"
#include "ev-window.h"

#include <gtk/gtklabel.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkimagemenuitem.h>
#include <gtk/gtkmenushell.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenutoolbutton.h>
#include <glib/gi18n.h>

struct _EvNavigationActionPrivate
{
	EvWindow *window;
	EvHistory *history;
};

static void ev_navigation_action_init       (EvNavigationAction *action);
static void ev_navigation_action_class_init (EvNavigationActionClass *class);

G_DEFINE_TYPE (EvNavigationAction, ev_navigation_action, GTK_TYPE_ACTION)

#define MAX_LABEL_LENGTH 48

#define EV_NAVIGATION_ACTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_NAVIGATION_ACTION, EvNavigationActionPrivate))

void
ev_navigation_action_set_history (EvNavigationAction *action,
				  EvHistory	     *history)
{
	action->priv->history = history;

	g_object_add_weak_pointer (G_OBJECT (action->priv->history),
				   (gpointer *) &action->priv->history);
}

void
ev_navigation_action_set_window (EvNavigationAction *action,
				 EvWindow	    *window)
{
	action->priv->window = window;
}

static void
activate_menu_item_cb (GtkWidget *widget, EvNavigationAction *action)
{
	int index;

	g_return_if_fail (EV_IS_HISTORY (action->priv->history));

	index = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "index"));
	ev_history_set_current_index (action->priv->history, index);
	
	if (action->priv->window) {
		EvLink *link;
		EvLinkAction *link_action;
		EvLinkDest *dest;

		link = ev_history_get_link_nth (action->priv->history, index);
		link_action = ev_link_get_action (link);
		dest = ev_link_action_get_dest (link_action);
		
		ev_window_goto_dest (action->priv->window, dest);
	}
}

static GtkWidget *
new_history_menu_item (EvNavigationAction *action,
		       EvLink             *link,
		       int                 index)
{
	GtkLabel *label;
	GtkWidget *item;
	const char *title;

	title = ev_link_get_title (link);
	item = gtk_image_menu_item_new_with_label (title);
	g_object_set_data (G_OBJECT (item), "index",
			   GINT_TO_POINTER (index));

	label = GTK_LABEL (GTK_BIN (item)->child);
	gtk_label_set_ellipsize (label, PANGO_ELLIPSIZE_END);
	gtk_label_set_max_width_chars (label, MAX_LABEL_LENGTH);

	g_signal_connect (item, "activate",
			  G_CALLBACK (activate_menu_item_cb),
			  action);

	gtk_widget_show (item);

	return item;
}

static GtkWidget *
new_empty_history_menu_item (EvNavigationAction *action)
{
	GtkWidget *item;
	
	item = gtk_image_menu_item_new_with_label (_("Empty"));
	gtk_widget_set_sensitive (item, FALSE);
	gtk_widget_show (item);
	
	return item;
}

static GtkWidget *
build_menu (EvNavigationAction *action)
{
	GtkMenuShell *menu;
	GtkWidget *item;
	EvLink *link;
	EvHistory *history = action->priv->history;
	int start, end, i;

	menu = GTK_MENU_SHELL (gtk_menu_new ());

	if (history == NULL || ev_history_get_n_links (history) <= 0) {
		item = new_empty_history_menu_item (action);
		gtk_menu_shell_append (menu, item);		
		return GTK_WIDGET (menu);
	}

	start = MAX (ev_history_get_current_index (action->priv->history) - 5, 0);
	end = MIN (ev_history_get_n_links (history), start + 7);

	for (i = start; i < end; i++) {
		link = ev_history_get_link_nth (history, i);
		item = new_history_menu_item (action, link, i);
		gtk_menu_shell_prepend (menu, item);
	}

	return GTK_WIDGET (menu);
}

static void
menu_activated_cb (EvNavigationActionWidget *button,
		   EvNavigationAction *action)
{
	GtkWidget *menu;

	menu = build_menu (action);
	ev_navigation_action_widget_set_menu (button, menu);
}

static void
connect_proxy (GtkAction *action, GtkWidget *proxy)
{
	GtkWidget *menu;

	/* set dummy menu so the arrow gets sensitive */
	menu = gtk_menu_new ();
	ev_navigation_action_widget_set_menu (EV_NAVIGATION_ACTION_WIDGET (proxy), menu);

	g_signal_connect (proxy, "show-menu",
			  G_CALLBACK (menu_activated_cb), action);

	GTK_ACTION_CLASS (ev_navigation_action_parent_class)->connect_proxy (action, proxy);
}

static GtkWidget *
create_tool_item (GtkAction *action)
{
	EvNavigationActionWidget *proxy;

	proxy = g_object_new (EV_TYPE_NAVIGATION_ACTION_WIDGET, NULL);
	gtk_widget_show (GTK_WIDGET (proxy));

	return GTK_WIDGET (proxy);
}

static void
ev_navigation_action_init (EvNavigationAction *action)
{
	action->priv = EV_NAVIGATION_ACTION_GET_PRIVATE (action);
}

static void
ev_navigation_action_finalize (GObject *object)
{
	EvNavigationAction *action = EV_NAVIGATION_ACTION (object);

	if (action->priv->history) {
		g_object_add_weak_pointer (G_OBJECT (action->priv->history),
					   (gpointer *) &action->priv->history);
	}

	G_OBJECT_CLASS (ev_navigation_action_parent_class)->finalize (object);
}

static void
ev_navigation_action_class_init (EvNavigationActionClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkActionClass *action_class = GTK_ACTION_CLASS (class);

	object_class->finalize = ev_navigation_action_finalize;

	action_class->toolbar_item_type = GTK_TYPE_TOOL_ITEM;
	action_class->create_tool_item = create_tool_item;
	action_class->connect_proxy = connect_proxy;

	g_type_class_add_private (object_class, sizeof (EvNavigationActionPrivate));
}
