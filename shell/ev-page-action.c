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
#include <gtk/gtkentry.h>
#include <gtk/gtktoolitem.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkhbox.h>

struct _EvPageActionPrivate
{
	EvPageCache *page_cache;
};


static void ev_page_action_init       (EvPageAction *action);
static void ev_page_action_class_init (EvPageActionClass *class);

G_DEFINE_TYPE (EvPageAction, ev_page_action, GTK_TYPE_ACTION)

#define EV_PAGE_ACTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_PAGE_ACTION, EvPageActionPrivate))

enum {
	PROP_0,
	PROP_PAGE_CACHE,
};

#define ENTRY_DATA      "epa-entry"
#define PAGE_CACHE_DATA "epa-page-cache"
#define SIGNAL_ID_DATA  "epa-signal-id"

static void
page_changed_cb (EvPageCache *page_cache,
		 gint         page,
		 GtkWidget   *proxy)
{
	GtkWidget *entry;

	entry = GTK_WIDGET (g_object_get_data (G_OBJECT (proxy), ENTRY_DATA));
	if (page_cache != NULL) {
		gchar *page_label = ev_page_cache_get_page_label (page_cache, page);
		gtk_entry_set_text (GTK_ENTRY (entry), page_label);
		gtk_editable_set_position (GTK_EDITABLE (entry), -1);
		g_free (page_label);
	} else {
		gtk_entry_set_text (GTK_ENTRY (entry), "");
	}
}

static void
activate_cb (GtkWidget *entry, GtkAction *action)
{
	EvPageAction *page = EV_PAGE_ACTION (action);
	EvPageCache *page_cache;
	const char *text;

	text = gtk_entry_get_text (GTK_ENTRY (entry));
	page_cache = page->priv->page_cache;

	if (! ev_page_cache_set_page_label (page_cache, text)) {
		/* rest the entry to the current page if we were unable to
		 * change it */
		gchar *page_label =
			ev_page_cache_get_page_label (page_cache,
						      ev_page_cache_get_current_page (page_cache));
		gtk_entry_set_text (GTK_ENTRY (entry), page_label);
		gtk_editable_set_position (GTK_EDITABLE (entry), -1);
		g_free (page_label);
	}
}

static GtkWidget *
create_tool_item (GtkAction *action)
{
	GtkWidget *hbox, *entry, *item;

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 6); 
	gtk_widget_show (hbox);

	item = GTK_WIDGET (gtk_tool_item_new ());
	gtk_widget_show (item);

	entry = gtk_entry_new ();
	gtk_entry_set_width_chars (GTK_ENTRY (entry), 5);
	g_object_set_data (G_OBJECT (item), ENTRY_DATA, entry);
	gtk_widget_show (entry);

	g_signal_connect (entry, "activate",
			  G_CALLBACK (activate_cb),
			  action);

	gtk_box_pack_start (GTK_BOX (hbox), entry, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (item), hbox);

	return item;
}

static void
update_page_cache (EvPageAction *page, gpointer dummy, GtkWidget *proxy)
{
	EvPageCache *page_cache;
	EvPageCache *old_page_cache;
	guint signal_id;

	page_cache = page->priv->page_cache;
	old_page_cache = (EvPageCache *) g_object_get_data (G_OBJECT (proxy), PAGE_CACHE_DATA);
	signal_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (proxy), SIGNAL_ID_DATA));

	/* clear the old signal */
	if (signal_id > 0 && old_page_cache)
		g_signal_handler_disconnect (old_page_cache, signal_id);
	
	if (page_cache != NULL) {
		signal_id = g_signal_connect (page_cache,
					      "page-changed",
					      G_CALLBACK (page_changed_cb),
					      proxy);
		/* Set the initial value */
		page_changed_cb (page_cache,
				 ev_page_cache_get_current_page (page_cache),
				 proxy);
	} else {
		/* Or clear the entry */
		signal_id = 0;
		page_changed_cb (NULL, 0, proxy);
	}
	g_object_set_data (G_OBJECT (proxy), PAGE_CACHE_DATA, page_cache);
	g_object_set_data (G_OBJECT (proxy), SIGNAL_ID_DATA, GINT_TO_POINTER (signal_id));
}

static void
connect_proxy (GtkAction *action, GtkWidget *proxy)
{
	if (GTK_IS_TOOL_ITEM (proxy)) {
		g_signal_connect_object (action, "notify::page-cache",
					 G_CALLBACK (update_page_cache),
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
  
	page = EV_PAGE_ACTION (object);

	switch (prop_id)
	{
	case PROP_PAGE_CACHE:
		page_cache = page->priv->page_cache;
		page->priv->page_cache = EV_PAGE_CACHE (g_value_dup_object (value));
		if (page_cache)
			g_object_unref (page_cache);
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
		page_cache = ev_document_get_page_cache (document);
	
	g_object_set (page, "page-cache", page_cache, NULL);
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

	g_object_class_install_property (object_class,
					 PROP_PAGE_CACHE,
					 g_param_spec_object ("page-cache",
							      "Page Cache",
							      "Current page cache",
							      EV_TYPE_PAGE_CACHE,
							      G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (EvPageActionPrivate));
}
