/*
 *  Copyright (C) 2007, Carlos Garcia Campos  <carlosgc@gnome.org>
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

#include <config.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "ev-open-recent-action.h"


enum {
	ITEM_ACTIVATED,
	N_SIGNALS
};

static void ev_open_recent_action_init       (EvOpenRecentAction      *action);
static void ev_open_recent_action_class_init (EvOpenRecentActionClass *class);

static guint action_signals[N_SIGNALS];

G_DEFINE_TYPE (EvOpenRecentAction, ev_open_recent_action, GTK_TYPE_ACTION)

static void
recent_chooser_item_activated (GtkRecentChooser *chooser,
			       GtkAction        *action)
{
	gchar *uri;

	uri = gtk_recent_chooser_get_current_uri (chooser);
	g_signal_emit (action, action_signals[ITEM_ACTIVATED], 0, uri);
	g_free (uri);
}

static GtkWidget *
ev_open_recent_action_create_tool_item (GtkAction *action)
{
	GtkWidget       *tool_item;
	GtkWidget       *toolbar_recent_menu;
	GtkRecentFilter *filter;

	toolbar_recent_menu = gtk_recent_chooser_menu_new_for_manager (gtk_recent_manager_get_default ());
	gtk_recent_chooser_set_local_only (GTK_RECENT_CHOOSER (toolbar_recent_menu), FALSE);
	gtk_recent_chooser_set_sort_type (GTK_RECENT_CHOOSER (toolbar_recent_menu), GTK_RECENT_SORT_MRU);
	gtk_recent_chooser_set_limit (GTK_RECENT_CHOOSER (toolbar_recent_menu), 5);
	g_signal_connect (toolbar_recent_menu, "item_activated",
			  G_CALLBACK (recent_chooser_item_activated),
			  action);

	filter = gtk_recent_filter_new ();
	gtk_recent_filter_add_application (filter, g_get_application_name ());
	gtk_recent_chooser_set_filter (GTK_RECENT_CHOOSER (toolbar_recent_menu), filter);

	tool_item = GTK_WIDGET (gtk_menu_tool_button_new_from_stock (GTK_STOCK_OPEN));
	gtk_menu_tool_button_set_arrow_tooltip_text (GTK_MENU_TOOL_BUTTON (tool_item),
						     _("Open a recently used document"));
	gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (tool_item),
				       GTK_WIDGET (toolbar_recent_menu));
	return tool_item;
}

static void
ev_open_recent_action_init (EvOpenRecentAction *action)
{
}

static void
ev_open_recent_action_class_init (EvOpenRecentActionClass *class)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (class);
	GtkActionClass *action_class = GTK_ACTION_CLASS (class);

	action_class->toolbar_item_type = GTK_TYPE_MENU_TOOL_BUTTON;
	action_class->create_tool_item = ev_open_recent_action_create_tool_item;

	action_signals[ITEM_ACTIVATED] =
		g_signal_new ("item_activated",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (EvOpenRecentActionClass, item_activated),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING, 
			      G_TYPE_NONE, 1,
			      G_TYPE_STRING);
}
