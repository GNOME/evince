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

static void
update_pages_label (EvPageActionWidget *proxy,
		    gint                page,
		    EvPageCache        *page_cache)
{
	char *label_text;
	gint n_pages;

	n_pages = page_cache ? ev_page_cache_get_n_pages (page_cache) : 0;
	if (page_cache && ev_page_cache_has_nonnumeric_page_labels (page_cache)) {
    	        label_text = g_strdup_printf (_("(%d of %d)"), page + 1, n_pages);
	} else {
    	        label_text = g_strdup_printf (_("of %d"), n_pages);
	}
	gtk_label_set_text (GTK_LABEL (proxy->label), label_text);
	g_free (label_text);
}

static void
page_changed_cb (EvPageCache        *page_cache,
		 gint                page,
		 EvPageActionWidget *proxy)
{
	g_assert (proxy);
	
	if (page_cache != NULL && page >= 0) {
		gchar *page_label;

		gtk_entry_set_width_chars (GTK_ENTRY (proxy->entry), 
					   CLAMP (ev_page_cache_get_max_label_chars (page_cache), 
					   6, 12));	
		
		page_label = ev_page_cache_get_page_label (page_cache, page);
		gtk_entry_set_text (GTK_ENTRY (proxy->entry), page_label);
		gtk_editable_set_position (GTK_EDITABLE (proxy->entry), -1);
		g_free (page_label);
		
	} else {
		gtk_entry_set_text (GTK_ENTRY (proxy->entry), "");
	}

	update_pages_label (proxy, page, page_cache);
}

static void
activate_cb (GtkWidget *entry, GtkAction *action)
{
	EvPageAction *page = EV_PAGE_ACTION (action);
	EvPageCache *page_cache;
	const char *text;
	gchar *page_label;
	
	EvLinkDest *link_dest;
	EvLinkAction *link_action;
	EvLink *link;
	gchar *link_text;

	text = gtk_entry_get_text (GTK_ENTRY (entry));
	page_cache = page->priv->page_cache;

	
	link_dest = ev_link_dest_new_page_label (text);
	link_action = ev_link_action_new_dest (link_dest);
	link_text = g_strdup_printf ("Page: %s", text);
	link = ev_link_new (link_text, link_action);

	g_signal_emit (action, signals[ACTIVATE_LINK], 0, link);

	g_object_unref (link);
	g_free (link_text);
	
	/* rest the entry to the current page if we were unable to
	 * change it */
	page_label = ev_page_cache_get_page_label (page_cache,
						   ev_page_cache_get_current_page (page_cache));
	gtk_entry_set_text (GTK_ENTRY (entry), page_label);
	gtk_editable_set_position (GTK_EDITABLE (entry), -1);
	g_free (page_label);
}

static gboolean page_scroll_cb(GtkWidget *widget, GdkEventScroll *event, EvPageAction* action)
{
	gint pageno;

	pageno = ev_page_cache_get_current_page (action->priv->page_cache);
	if ((event->direction == GDK_SCROLL_DOWN) && 
	    (pageno < ev_page_cache_get_n_pages(action->priv->page_cache) - 1))
		pageno++;
	if ((event->direction == GDK_SCROLL_UP) && (pageno > 0))
		pageno--;
	ev_page_cache_set_current_page (action->priv->page_cache, pageno);
	
	return TRUE;
}

static GtkWidget *
create_tool_item (GtkAction *action)
{
	EvPageActionWidget *proxy;
	GtkWidget *hbox;
        AtkObject *obj;

	proxy = g_object_new (ev_page_action_widget_get_type (), NULL);
	gtk_container_set_border_width (GTK_CONTAINER (proxy), 6); 
	gtk_widget_show (GTK_WIDGET (proxy));

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_set_spacing (GTK_BOX (hbox), 6);

	proxy->entry = gtk_entry_new ();
	obj = gtk_widget_get_accessible (proxy->entry);
        atk_object_set_name (obj, "page-label-entry");
	         
	g_signal_connect(proxy->entry, "scroll-event",G_CALLBACK(page_scroll_cb),action);
	gtk_widget_add_events(GTK_WIDGET(proxy->entry),GDK_BUTTON_MOTION_MASK);
	gtk_entry_set_width_chars (GTK_ENTRY (proxy->entry), 5);
	gtk_box_pack_start (GTK_BOX (hbox), proxy->entry, FALSE, FALSE, 0);
	gtk_widget_show (proxy->entry);
	g_signal_connect (proxy->entry, "activate",
			  G_CALLBACK (activate_cb),
			  action);

	proxy->label = gtk_label_new (NULL);
	gtk_box_pack_start (GTK_BOX (hbox), proxy->label, FALSE, FALSE, 0);
	gtk_widget_show (proxy->label);

	gtk_container_add (GTK_CONTAINER (proxy), hbox);
	gtk_widget_show (hbox);

	return GTK_WIDGET (proxy);
}

static void
update_page_cache (EvPageAction *page, GParamSpec *pspec, EvPageActionWidget *proxy)
{
	EvPageCache *page_cache;
	guint signal_id;

	page_cache = page->priv->page_cache;

	/* clear the old signal */
	if (proxy->signal_id > 0 && proxy->page_cache)
		g_signal_handler_disconnect (proxy->page_cache, proxy->signal_id);
	
	if (page_cache != NULL) {
		signal_id = g_signal_connect_object (page_cache,
					             "page-changed",
					             G_CALLBACK (page_changed_cb),
					             proxy, 0);
		/* Set the initial value */
		page_changed_cb (page_cache,
				 ev_page_cache_get_current_page (page_cache),
				 proxy);
	} else {
		/* Or clear the entry */
		signal_id = 0;
		page_changed_cb (NULL, 0, proxy);
	}
	ev_page_action_widget_set_page_cache (proxy, page_cache);
	proxy->signal_id = signal_id;
}

static void
activate_link_cb (EvPageActionWidget *proxy, EvLink *link, EvPageAction *action)
{
	g_signal_emit (action, signals[ACTIVATE_LINK], 0, link);
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
			gtk_widget_grab_focus (proxy->entry);
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
