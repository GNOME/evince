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
	EvDocumentModel *doc_model;
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
	PROP_MODEL
};

static GtkWidget *
create_tool_item (GtkAction *action)
{
	GtkWidget *proxy;

	proxy = g_object_new (EV_TYPE_PAGE_ACTION_WIDGET, NULL);

	return proxy;
}

static void
update_model (EvPageAction *page, GParamSpec *pspec, EvPageActionWidget *proxy)
{
	ev_page_action_widget_update_links_model (proxy, page->priv->model);
}

static void
activate_link_cb (EvPageActionWidget *proxy, EvLink *link, EvPageAction *action)
{
	g_signal_emit (action, signals[ACTIVATE_LINK], 0, link);
}

static void
connect_proxy (GtkAction *action, GtkWidget *proxy)
{
	EvPageAction *page = EV_PAGE_ACTION (action);

	if (GTK_IS_TOOL_ITEM (proxy)) {
		ev_page_action_widget_set_model (EV_PAGE_ACTION_WIDGET (proxy),
						 page->priv->doc_model);
		g_signal_connect (proxy, "activate_link",
				  G_CALLBACK (activate_link_cb),
				  action);
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

	if (page->priv->model) {
		g_object_unref (page->priv->model);
		page->priv->model = NULL;
	}

	page->priv->doc_model = NULL;

	G_OBJECT_CLASS (ev_page_action_parent_class)->dispose (object);
}

static void
ev_page_action_set_property (GObject      *object,
			     guint         prop_id,
			     const GValue *value,
			     GParamSpec   *pspec)
{
	EvPageAction *page = EV_PAGE_ACTION (object);

	switch (prop_id)
	{
	case PROP_MODEL:
		ev_page_action_set_links_model (page, g_value_get_object (value));
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
	EvPageAction *page = EV_PAGE_ACTION (object);

	switch (prop_id)
	{
	case PROP_MODEL:
		g_value_set_object (value, page->priv->model);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

void
ev_page_action_set_model (EvPageAction    *page,
			  EvDocumentModel *model)
{
	g_return_if_fail (EV_IS_PAGE_ACTION (page));
	g_return_if_fail (EV_IS_DOCUMENT_MODEL (model));

	if (page->priv->doc_model == model)
		return;

	page->priv->doc_model = model;
}

void
ev_page_action_set_links_model (EvPageAction *page,
				GtkTreeModel *links_model)
{
	g_return_if_fail (EV_IS_PAGE_ACTION (page));
	g_return_if_fail (GTK_IS_TREE_MODEL (links_model));

	if (page->priv->model == links_model)
		return;

	if (page->priv->model)
		g_object_unref (page->priv->model);
	page->priv->model = g_object_ref (links_model);

	g_object_notify (G_OBJECT (page), "model");
}

void
ev_page_action_grab_focus (EvPageAction *page_action)
{
	GSList *proxies;

	proxies = gtk_action_get_proxies (GTK_ACTION (page_action));
	for (; proxies != NULL; proxies = proxies->next) {
		EvPageActionWidget *proxy;

		proxy = EV_PAGE_ACTION_WIDGET (proxies->data);

		if (gtk_widget_get_mapped (GTK_WIDGET (proxy)))
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
					 PROP_MODEL,
					 g_param_spec_object ("model",
							      "Model",
							      "Current Links Model",
							      GTK_TYPE_TREE_MODEL,
							      G_PARAM_READWRITE |
                                                              G_PARAM_STATIC_STRINGS));

	g_type_class_add_private (object_class, sizeof (EvPageActionPrivate));
}
