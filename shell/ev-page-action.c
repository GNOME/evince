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

#include "ev-page-action.h"
#include "ev-window.h"

#include <glib/gi18n.h>
#include <gtk/gtkspinbutton.h>
#include <gtk/gtktoolitem.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkhbox.h>

struct _EvPageActionPrivate
{
	int current_page;
	int total_pages;
};

enum
{
	PROP_0,
	PROP_CURRENT_PAGE,
	PROP_TOTAL_PAGES
};

static void ev_page_action_init       (EvPageAction *action);
static void ev_page_action_class_init (EvPageActionClass *class);

enum
{
	GOTO_PAGE_SIGNAL,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

G_DEFINE_TYPE (EvPageAction, ev_page_action, GTK_TYPE_ACTION)

#define EV_PAGE_ACTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_PAGE_ACTION, EvPageActionPrivate))

static void
update_label (GtkAction *action, gpointer dummy, GtkWidget *proxy)
{
	EvPageAction *page = EV_PAGE_ACTION (action);
	char *text;
	GtkWidget *label;

	label = GTK_WIDGET (g_object_get_data (G_OBJECT (proxy), "label"));

	text = g_strdup_printf (_("of %d"), page->priv->total_pages);
	gtk_label_set_text (GTK_LABEL (label), text);
}

static void
update_spin (GtkAction *action, gpointer dummy, GtkWidget *proxy)
{
	EvPageAction *page = EV_PAGE_ACTION (action);
	GtkWidget *spin;
	int value;

	spin = GTK_WIDGET (g_object_get_data (G_OBJECT (proxy), "spin"));

	value = gtk_spin_button_get_value (GTK_SPIN_BUTTON (spin));

	if (value != page->priv->current_page )
	{
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin),
					   page->priv->current_page);
	}
}

static void
value_changed_cb (GtkWidget *spin, GtkAction *action)
{
	int value;

	value = gtk_spin_button_get_value (GTK_SPIN_BUTTON (spin));

	g_signal_emit (action, signals[GOTO_PAGE_SIGNAL], 0, value);
}

static void
total_pages_changed_cb (EvPageAction *action, GParamSpec *pspec,
			GtkSpinButton *spin)
{
	gtk_spin_button_set_range (GTK_SPIN_BUTTON (spin), 1, 
				   action->priv->total_pages);
}

static GtkWidget *
create_tool_item (GtkAction *action)
{
	GtkWidget *hbox, *spin, *item, *label;

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 6); 
	gtk_widget_show (hbox);

	item = GTK_WIDGET (gtk_tool_item_new ());
	gtk_widget_show (item);

	spin = gtk_spin_button_new_with_range (1, 9999, 1);
	gtk_spin_button_set_digits (GTK_SPIN_BUTTON (spin), 0);
	g_object_set_data (G_OBJECT (item), "spin", spin);
	gtk_widget_show (spin);

	g_signal_connect (action, "notify::total-pages",
			  G_CALLBACK (total_pages_changed_cb),
			  spin);
	g_signal_connect (spin, "value_changed",
			  G_CALLBACK (value_changed_cb),
			  action);

	label = gtk_label_new ("");
	g_object_set_data (G_OBJECT (item), "label", label);
	update_label (action, NULL, item);
	gtk_widget_show (label);

	gtk_box_pack_start (GTK_BOX (hbox), spin, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (item), hbox);

	return item;
}

static void
connect_proxy (GtkAction *action, GtkWidget *proxy)
{
	if (GTK_IS_TOOL_ITEM (proxy))
	{
		g_signal_connect_object (action, "notify::total-pages",
					 G_CALLBACK (update_label),
					 proxy, 0);
		g_signal_connect_object (action, "notify::current-page",
					 G_CALLBACK (update_spin),
					 proxy, 0);
	}

	GTK_ACTION_CLASS (parent_class)->connect_proxy (action, proxy);
}

static void
ev_page_action_init (EvPageAction *action)
{
	action->priv = EV_PAGE_ACTION_GET_PRIVATE (action);
}

static void
ev_page_action_finalize (GObject *object)
{
	parent_class->finalize (object);
}

static void
ev_page_action_set_property (GObject *object,
			     guint prop_id,
			     const GValue *value,
			     GParamSpec *pspec)
{
	EvPageAction *page = EV_PAGE_ACTION (object);

	switch (prop_id)
	{
		case PROP_CURRENT_PAGE:
			page->priv->current_page = g_value_get_int (value);
			break;
		case PROP_TOTAL_PAGES:
			page->priv->total_pages = g_value_get_int (value);
			break;
	}
}

static void
ev_page_action_get_property (GObject *object,
			     guint prop_id,
			     GValue *value,
			     GParamSpec *pspec)
{
	EvPageAction *page = EV_PAGE_ACTION (object);

	switch (prop_id)
	{
		case PROP_CURRENT_PAGE:
			g_value_set_int (value, page->priv->current_page);
			break;
		case PROP_TOTAL_PAGES:
			g_value_set_int (value, page->priv->total_pages);
			break;
	}
}

void
ev_page_action_set_current_page (EvPageAction *page, int current_page)
{
	g_object_set (page, "current-page", current_page, NULL);
}

void
ev_page_action_set_total_pages (EvPageAction *page, int total_pages)
{
	g_object_set (page, "total-pages", total_pages, NULL);
}

static void
ev_page_action_class_init (EvPageActionClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkActionClass *action_class = GTK_ACTION_CLASS (class);

	object_class->finalize = ev_page_action_finalize;
	object_class->set_property = ev_page_action_set_property;
	object_class->get_property = ev_page_action_get_property;

	parent_class = g_type_class_peek_parent (class);

	action_class->toolbar_item_type = GTK_TYPE_TOOL_ITEM;
	action_class->create_tool_item = create_tool_item;
	action_class->connect_proxy = connect_proxy;

	signals[GOTO_PAGE_SIGNAL] =
		g_signal_new ("goto_page",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EvPageActionClass, goto_page),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_INT);

	g_object_class_install_property (object_class,
					 PROP_CURRENT_PAGE,
					 g_param_spec_int ("current-page",
							   "Current Page",
							   "The number of current page",
							   0,
							   G_MAXINT,
							   0,
							   G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_TOTAL_PAGES,
					 g_param_spec_int ("total-pages",
							   "Total Pages",
							   "The total number of pages",
							   0,
							   G_MAXINT,
							   0,
							   G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (EvPageActionPrivate));
}
