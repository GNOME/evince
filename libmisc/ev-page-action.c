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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "ev-page-action.h"
#include "ev-page-action-widget.h"

struct _EvPageActionPrivate
{
	EvPageCache *page_cache;
	GtkTreeModel *model;
};


static void ev_page_action_init       (EvPageAction *action);
static void ev_page_action_class_init (EvPageActionClass *class);

enum
{
	ACTIVATE_LINK,
	N_SIGNALS
};

static guint signals[N_SIGNALS] = {0, };

G_DEFINE_TYPE (EvPageAction, ev_page_action, GTK_TYPE_ACTION)

#define EV_PAGE_ACTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_PAGE_ACTION, EvPageActionPrivate))

enum {
	PROP_0,
	PROP_PAGE_CACHE,
	PROP_MODEL,
};

static GtkWidget *
create_tool_item (GtkAction *action)
{
	GtkWidget *proxy;

	proxy = g_object_new (EV_TYPE_PAGE_ACTION_WIDGET, NULL);

	return proxy;
}

static void
update_page_cache (EvPageAction *page, GParamSpec *pspec, EvPageActionWidget *proxy)
{
	ev_page_action_widget_set_page_cache (proxy, page->priv->page_cache);
}

static void
update_model (EvPageAction *page, GParamSpec *pspec, EvPageActionWidget *proxy)
{
	GtkTreeModel *model;

	g_object_get (G_OBJECT (page),
		      "model", &model,
		      NULL);

	ev_page_action_widget_update_model (proxy, model);
}

static void
activate_link_cb (EvPageActionWidget *proxy, EvLink *link, EvPageAction *action)
{
	g_signal_emit (action, signals[ACTIVATE_LINK], 0, link);
}

static void
connect_proxy (GtkAction *action, GtkWidget *proxy)
{
	if (GTK_IS_TOOL_ITEM (proxy)) {
		g_signal_connect_object (action, "notify::page-cache",
					 G_CALLBACK (update_page_cache),
					 proxy, 0);
		g_signal_connect (proxy, "activate_link",
				  G_CALLBACK (activate_link_cb),
				  action);
		update_page_cache (EV_PAGE_ACTION (action), NULL,
				   EV_PAGE_ACTION_WIDGET (proxy));
		g_signal_connect_object (action, "notify::model",
					 G_CALLBACK (update_model),
					 proxy, 0);
	}

	GTK_ACTION_CLASS (ev_page_action_parent_class)->connect_proxy (action, proxy);
}

static void
ev_page_action_dispose (GObject *object)
{
	EvPageAction *page = EV_PAGE_ACTION (object);

	if (page->priv->page_cache) {
		g_object_unref (page->priv->page_cache);
		page->priv->page_cache = NULL;
	}

	G_OBJECT_CLASS (ev_page_action_parent_class)->dispose (object);
}

static void
ev_page_action_set_property (GObject      *object,
			     guint         prop_id,
			     const GValue *value,
			     GParamSpec   *pspec)
{
	EvPageAction *page;
	EvPageCache *page_cache;
	GtkTreeModel *model;
  
	page = EV_PAGE_ACTION (object);

	switch (prop_id)
	{
	case PROP_PAGE_CACHE:
		page_cache = page->priv->page_cache;
		page->priv->page_cache = EV_PAGE_CACHE (g_value_dup_object (value));
		if (page_cache)
			g_object_unref (page_cache);
		break;
	case PROP_MODEL:
		model = page->priv->model;
		page->priv->model = GTK_TREE_MODEL (g_value_dup_object (value));
		if (model)
			g_object_unref (model);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ev_page_action_get_property (GObject    *object,
			     guint       prop_id,
			     GValue     *value,
			     GParamSpec *pspec)
{
	EvPageAction *page;
  
	page = EV_PAGE_ACTION (object);

	switch (prop_id)
	{
	case PROP_PAGE_CACHE:
		g_value_set_object (value, page->priv->page_cache);
		break;
	case PROP_MODEL:
		g_value_set_object (value, page->priv->model);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

void
ev_page_action_set_document (EvPageAction *page, EvDocument *document)
{
	EvPageCache *page_cache = NULL;

	if (document)
		page_cache = ev_page_cache_get (document);
	
	g_object_set (page,
		      "page-cache", page_cache,
		      "model", NULL,
		      NULL);
}

void
ev_page_action_set_model (EvPageAction *page_action,
			  GtkTreeModel *model)
{
	g_object_set (page_action,
		      "model", model,
		      NULL);
}

void
ev_page_action_grab_focus (EvPageAction *page_action)
{
	GSList *proxies;

	proxies = gtk_action_get_proxies (GTK_ACTION (page_action));
	for (; proxies != NULL; proxies = proxies->next) {
		EvPageActionWidget *proxy;

		proxy = EV_PAGE_ACTION_WIDGET (proxies->data);

		if (GTK_WIDGET_MAPPED (GTK_WIDGET (proxy)))
			ev_page_action_widget_grab_focus (proxy);
	}
}

static void
ev_page_action_init (EvPageAction *page)
{
	page->priv = EV_PAGE_ACTION_GET_PRIVATE (page);
}

static void
ev_page_action_class_init (EvPageActionClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkActionClass *action_class = GTK_ACTION_CLASS (class);

	object_class->dispose = ev_page_action_dispose;
	object_class->set_property = ev_page_action_set_property;
	object_class->get_property = ev_page_action_get_property;

	action_class->toolbar_item_type = GTK_TYPE_TOOL_ITEM;
	action_class->create_tool_item = create_tool_item;
	action_class->connect_proxy = connect_proxy;

	signals[ACTIVATE_LINK] = g_signal_new ("activate_link",
					       G_OBJECT_CLASS_TYPE (object_class),
					       G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
					       G_STRUCT_OFFSET (EvPageActionClass, activate_link),
					       NULL, NULL,
					       g_cclosure_marshal_VOID__OBJECT,
					       G_TYPE_NONE, 1,
					       G_TYPE_OBJECT);

	g_object_class_install_property (object_class,
					 PROP_PAGE_CACHE,
					 g_param_spec_object ("page-cache",
							      "Page Cache",
							      "Current page cache",
							      EV_TYPE_PAGE_CACHE,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_MODEL,
					 g_param_spec_object ("model",
							      "Model",
							      "Current Model",
							      GTK_TYPE_TREE_MODEL,
							      G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (EvPageActionPrivate));
}
