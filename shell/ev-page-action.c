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
#include "ev-document-links.h"
#include "ev-marshal.h"

#include <glib/gi18n.h>
#include <gtk/gtkentry.h>
#include <gtk/gtktoolitem.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkhbox.h>
#include <string.h>

typedef struct _EvPageActionWidget EvPageActionWidget;
typedef struct _EvPageActionWidgetClass EvPageActionWidgetClass;
struct _EvPageActionWidget
{
	GtkToolItem parent;

	GtkWidget *entry;
	GtkWidget *label;
	EvPageCache *page_cache;
	guint signal_id;
	GtkTreeModel *filter_model;
	GtkTreeModel *model;
};

struct _EvPageActionWidgetClass
{
	GtkToolItemClass parent_class;

	void (* activate_link) (EvPageActionWidget *page_action,
			        EvLink             *link);
};

struct _EvPageActionPrivate
{
	EvPageCache *page_cache;
	GtkTreeModel *model;
};


/* Widget we pass back */
static GType ev_page_action_widget_get_type   (void);
static void  ev_page_action_widget_init       (EvPageActionWidget      *action_widget);
static void  ev_page_action_widget_class_init (EvPageActionWidgetClass *action_widget);

#define EV_TYPE_PAGE_ACTION_WIDGET (ev_page_action_widget_get_type ())
#define EV_PAGE_ACTION_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_PAGE_ACTION_WIDGET, EvPageActionWidget))

enum
{
	WIDGET_ACTIVATE_LINK,
	WIDGET_N_SIGNALS
};

static guint widget_signals[WIDGET_N_SIGNALS] = {0, };

G_DEFINE_TYPE (EvPageActionWidget, ev_page_action_widget, GTK_TYPE_TOOL_ITEM)

static void
ev_page_action_widget_init (EvPageActionWidget *action_widget)
{

}

static void
ev_page_action_widget_set_page_cache (EvPageActionWidget *action_widget,
				      EvPageCache        *page_cache)
{
	if (action_widget->page_cache != NULL) {
		g_object_remove_weak_pointer (G_OBJECT (action_widget->page_cache),
					      (gpointer *)&action_widget->page_cache);
		action_widget->page_cache = NULL;
	}

	if (page_cache != NULL) {
		action_widget->page_cache = page_cache;
		g_object_add_weak_pointer (G_OBJECT (page_cache),
					   (gpointer *)&action_widget->page_cache);
	}
}

static void
ev_page_action_widget_finalize (GObject *object)
{
	EvPageActionWidget *action_widget = EV_PAGE_ACTION_WIDGET (object);

	ev_page_action_widget_set_page_cache (action_widget, NULL);
}

static void
ev_page_action_widget_class_init (EvPageActionWidgetClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = ev_page_action_widget_finalize;

	widget_signals[WIDGET_ACTIVATE_LINK] = g_signal_new ("activate_link",
					       G_OBJECT_CLASS_TYPE (object_class),
					       G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
					       G_STRUCT_OFFSET (EvPageActionClass, activate_link),
					       NULL, NULL,
					       g_cclosure_marshal_VOID__OBJECT,
					       G_TYPE_NONE, 1,
					       G_TYPE_OBJECT);

}

static void ev_page_action_init       (EvPageAction *action);
static void ev_page_action_class_init (EvPageActionClass *class);

enum
{
	ACTIVATE_LINK,
	ACTIVATE_LABEL,
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

/* user data to set on the widget. */
#define EPA_FILTER_MODEL_DATA "epa-filter-model"

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
	
		gtk_entry_set_width_chars (GTK_ENTRY (proxy->entry), 
					   CLAMP (ev_page_cache_get_max_label_chars (page_cache), 
					   4, 12));	
		
		gchar *page_label = ev_page_cache_get_page_label (page_cache, page);
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
	gboolean changed;

	text = gtk_entry_get_text (GTK_ENTRY (entry));
	page_cache = page->priv->page_cache;

	g_signal_emit (action, signals[ACTIVATE_LABEL], 0, text, &changed);

	if (!changed) {
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
	EvPageActionWidget *proxy;
	GtkWidget *hbox;

	proxy = g_object_new (ev_page_action_widget_get_type (), NULL);
	gtk_container_set_border_width (GTK_CONTAINER (proxy), 6); 
	gtk_widget_show (GTK_WIDGET (proxy));

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_set_spacing (GTK_BOX (hbox), 6);

	proxy->entry = gtk_entry_new ();
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

static gboolean
build_new_tree_cb (GtkTreeModel *model,
		   GtkTreePath  *path,
		   GtkTreeIter  *iter,
		   gpointer      data)
{
	GtkTreeModel *filter_model = GTK_TREE_MODEL (data);
	EvLink *link;

	gtk_tree_model_get (model, iter,
			    EV_DOCUMENT_LINKS_COLUMN_LINK, &link,
			    -1);

	if (link && ev_link_get_link_type (link) == EV_LINK_TYPE_PAGE) {
		GtkTreeIter filter_iter;

		gtk_list_store_append (GTK_LIST_STORE (filter_model), &filter_iter);
		gtk_list_store_set (GTK_LIST_STORE (filter_model), &filter_iter,
				    0, iter,
				    -1);
	}
	
	return FALSE;
}

static GtkTreeModel *
get_filter_model_from_model (GtkTreeModel *model)
{
	GtkTreeModel *filter_model;

	filter_model =
		(GtkTreeModel *) g_object_get_data (G_OBJECT (model), EPA_FILTER_MODEL_DATA);
	if (filter_model == NULL) {
		filter_model = (GtkTreeModel *) gtk_list_store_new (1, GTK_TYPE_TREE_ITER);

		gtk_tree_model_foreach (model,
					build_new_tree_cb,
					filter_model);
		g_object_set_data_full (G_OBJECT (model), EPA_FILTER_MODEL_DATA, filter_model, g_object_unref);
	}

	return filter_model;
}

static gboolean
match_selected_cb (GtkEntryCompletion *completion,
		   GtkTreeModel       *filter_model,
		   GtkTreeIter        *filter_iter,
		   EvPageActionWidget *proxy)
{
	EvLink *link;
	GtkTreeIter *iter;

	gtk_tree_model_get (filter_model, filter_iter,
			    0, &iter,
			    -1);
	gtk_tree_model_get (proxy->model, iter,
			    EV_DOCUMENT_LINKS_COLUMN_LINK, &link,
			    -1);

	g_signal_emit (proxy, signals[ACTIVATE_LINK], 0, link);
	
	return TRUE;
}
		   

static void
display_completion_text (GtkCellLayout      *cell_layout,
			 GtkCellRenderer    *renderer,
			 GtkTreeModel       *filter_model,
			 GtkTreeIter        *filter_iter,
			 EvPageActionWidget *proxy)
{
	EvLink *link;
	GtkTreeIter *iter;

	gtk_tree_model_get (filter_model, filter_iter,
			    0, &iter,
			    -1);
	gtk_tree_model_get (proxy->model, iter,
			    EV_DOCUMENT_LINKS_COLUMN_LINK, &link,
			    -1);

	g_object_set (renderer, "text", ev_link_get_title (link), NULL);
}

static gboolean
match_completion (GtkEntryCompletion *completion,
		  const gchar        *key,
		  GtkTreeIter        *filter_iter,
		  EvPageActionWidget *proxy)
{
	EvLink *link;
	GtkTreeIter *iter;
	const gchar *text = NULL;

	gtk_tree_model_get (gtk_entry_completion_get_model (completion),
			    filter_iter,
			    0, &iter,
			    -1);
	gtk_tree_model_get (proxy->model, iter,
			    EV_DOCUMENT_LINKS_COLUMN_LINK, &link,
			    -1);


	if (link)
		text = ev_link_get_title (link);

	if (text && key ) {
		gchar *normalized_text;
		gchar *normalized_key;
		gchar *case_normalized_text;
		gchar *case_normalized_key;
		gboolean retval = FALSE;

		normalized_text = g_utf8_normalize (text, -1, G_NORMALIZE_ALL);
		normalized_key = g_utf8_normalize (key, -1, G_NORMALIZE_ALL);
		case_normalized_text = g_utf8_casefold (normalized_text, -1);
		case_normalized_key = g_utf8_casefold (normalized_key, -1);

		if (strstr (case_normalized_text, case_normalized_key))
			retval = TRUE;

		g_free (normalized_text);
		g_free (normalized_key);
		g_free (case_normalized_text);
		g_free (case_normalized_key);

		return retval;
	}

	return FALSE;
}


static void
update_model (EvPageAction *page, GParamSpec *pspec, EvPageActionWidget *proxy)
{
	GtkTreeModel *model;
	GtkTreeModel *filter_model;

	g_object_get (G_OBJECT (page),
		      "model", &model,
		      NULL);
	if (model != NULL) {
		/* Magik */
		GtkEntryCompletion *completion;
		GtkCellRenderer *renderer;

		proxy->model = model;
		filter_model = get_filter_model_from_model (model);

		completion = gtk_entry_completion_new ();

		/* popup-set-width is 2.7.0 only */
		g_object_set (G_OBJECT (completion),
			      "popup-set-width", FALSE,
			      "model", filter_model,
			      NULL);

		g_signal_connect (completion, "match-selected", G_CALLBACK (match_selected_cb), proxy);
		gtk_entry_completion_set_match_func (completion,
						     (GtkEntryCompletionMatchFunc) match_completion,
						     proxy, NULL);

		/* Set up the layout */
		renderer = (GtkCellRenderer *)
			g_object_new (GTK_TYPE_CELL_RENDERER_TEXT,
				      "ellipsize", PANGO_ELLIPSIZE_END,
				      "width_chars", 30,
				      NULL);
		gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (completion), renderer, TRUE);
		gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (completion),
						    renderer,
						    (GtkCellLayoutDataFunc) display_completion_text,
						    proxy, NULL);
		gtk_entry_set_completion (GTK_ENTRY (proxy->entry), completion);
	}
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
		/* We only go through this whole rigmarole if we can set
		 * GtkEntryCompletion::popup-set-width, which appeared in
		 * GTK+-2.7.0 */
		if (gtk_check_version (2, 7, 0) == NULL) {
			g_signal_connect_object (action, "notify::model",
						 G_CALLBACK (update_model),
						 proxy, 0);
		}
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
		page_cache = ev_document_get_page_cache (document);
	
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
	signals[ACTIVATE_LABEL] = g_signal_new ("activate_label",
					        G_OBJECT_CLASS_TYPE (object_class),
					        G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
					        G_STRUCT_OFFSET (EvPageActionClass, activate_link),
					        NULL, NULL,
					        ev_marshal_BOOLEAN__STRING,
					        G_TYPE_BOOLEAN, 1,
					        G_TYPE_STRING);

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
