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
 *  $Id$
 */

#include "config.h"

#include "ev-navigation-action.h"
#include "ev-window.h"

#include <gtk/gtklabel.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkimagemenuitem.h>
#include <gtk/gtkmenushell.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenutoolbutton.h>

struct _EvNavigationActionPrivate
{
	EvWindow *window;
	EvNavigationDirection direction;
	char *arrow_tooltip;
};

enum
{
	PROP_0,
	PROP_ARROW_TOOLTIP,
	PROP_DIRECTION
};

static void ev_navigation_action_init       (EvNavigationAction *action);
static void ev_navigation_action_class_init (EvNavigationActionClass *class);

static GObjectClass *parent_class = NULL;

G_DEFINE_TYPE (EvNavigationAction, ev_navigation_action, GTK_TYPE_ACTION)

#define EV_NAVIGATION_ACTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_NAVIGATION_ACTION, EvNavigationActionPrivate))

static GtkWidget *
build_menu (EvNavigationAction *action)
{
	GtkMenuShell *menu;

	menu = GTK_MENU_SHELL (gtk_menu_new ());

	return GTK_WIDGET (menu);
}

static void
menu_activated_cb (GtkMenuToolButton *button,
		   EvNavigationAction *action)
{
	GtkWidget *menu;

	menu = build_menu (action);
	gtk_menu_tool_button_set_menu (button, menu);
}

static gboolean
set_tooltip_cb (GtkMenuToolButton *proxy,
		GtkTooltips *tooltips,
		const char *tip,
		const char *tip_private,
		EvNavigationAction *action)
{
	gtk_menu_tool_button_set_arrow_tooltip (proxy, tooltips,
						action->priv->arrow_tooltip,
						NULL);

	/* don't stop emission */
	return FALSE;
}

static void
connect_proxy (GtkAction *action, GtkWidget *proxy)
{
	if (GTK_IS_MENU_TOOL_BUTTON (proxy))
	{
		GtkWidget *menu;

		/* set dummy menu so the arrow gets sensitive */
		menu = gtk_menu_new ();
		gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (proxy), menu);

		g_signal_connect (proxy, "show-menu",
				  G_CALLBACK (menu_activated_cb), action);

		g_signal_connect (proxy, "set-tooltip",
				  G_CALLBACK (set_tooltip_cb), action);
	}

	GTK_ACTION_CLASS (parent_class)->connect_proxy (action, proxy);
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

	g_free (action->priv->arrow_tooltip);

	parent_class->finalize (object);
}

static void
ev_navigation_action_set_property (GObject *object,
				     guint prop_id,
				     const GValue *value,
				     GParamSpec *pspec)
{
	EvNavigationAction *nav = EV_NAVIGATION_ACTION (object);

	switch (prop_id)
	{
		case PROP_ARROW_TOOLTIP:
			nav->priv->arrow_tooltip = g_value_dup_string (value);
			g_object_notify (object, "tooltip");
			break;
		case PROP_DIRECTION:
			nav->priv->direction = g_value_get_int (value);
			break;
	}
}

static void
ev_navigation_action_get_property (GObject *object,
				     guint prop_id,
				     GValue *value,
				     GParamSpec *pspec)
{
	EvNavigationAction *nav = EV_NAVIGATION_ACTION (object);

	switch (prop_id)
	{
		case PROP_ARROW_TOOLTIP:
			g_value_set_string (value, nav->priv->arrow_tooltip);
			break;
		case PROP_DIRECTION:
			g_value_set_int (value, nav->priv->direction);
			break;
	}
}

static void
ev_navigation_action_class_init (EvNavigationActionClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkActionClass *action_class = GTK_ACTION_CLASS (class);

	object_class->finalize = ev_navigation_action_finalize;
	object_class->set_property = ev_navigation_action_set_property;
	object_class->get_property = ev_navigation_action_get_property;

	parent_class = g_type_class_peek_parent (class);

	action_class->toolbar_item_type = GTK_TYPE_MENU_TOOL_BUTTON;
	action_class->connect_proxy = connect_proxy;

	g_object_class_install_property (object_class,
					 PROP_ARROW_TOOLTIP,
					 g_param_spec_string ("arrow-tooltip",
							      "Arrow Tooltip",
							      "Arrow Tooltip",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_DIRECTION,
					 g_param_spec_int ("direction",
							   "Direction",
							   "Direction",
							   0,
							   G_MAXINT,
							   0,
							   G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (EvNavigationActionPrivate));
}
