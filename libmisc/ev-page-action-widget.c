/*
 *  Copyright (C) 2003, 2004 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004 Christian Persch
 *  Copyright (C) 2018       Germán Poo-Caamaño
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

#include "config.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <evince-document.h>
#include "ev-page-action-widget.h"

#define COMPLETION_RESULTS_WIDTH 50

/* Widget we pass back */
static void  ev_page_action_widget_init       (EvPageActionWidget      *action_widget);
static void  ev_page_action_widget_class_init (EvPageActionWidgetClass *action_widget);
static gboolean ev_page_action_widget_completion_search_is_enabled (EvPageActionWidget *proxy);

enum
{
	WIDGET_ACTIVATE_LINK,
	WIDGET_N_SIGNALS
};

struct _EvPageActionWidget
{
	GtkToolItem parent;

	EvDocument *document;
	EvDocumentModel *doc_model;

	GtkWidget *entry;
	GtkWidget *label;
	gulong signal_id;
	gulong notify_document_signal_id;
	GtkTreeModel *filter_model;
	GtkTreeModel *model;
	GtkEntryCompletion *completion;
};

static guint widget_signals[WIDGET_N_SIGNALS] = {0, };

G_DEFINE_TYPE (EvPageActionWidget, ev_page_action_widget, GTK_TYPE_TOOL_ITEM)

static gboolean
show_page_number_in_pages_label (EvPageActionWidget *action_widget,
                                 gint                page)
{
        gchar   *page_label;
        gboolean retval;

        if (!ev_document_has_text_page_labels (action_widget->document))
                return FALSE;

        page_label = g_strdup_printf ("%d", page + 1);
        retval = g_strcmp0 (page_label, gtk_entry_get_text (GTK_ENTRY (action_widget->entry))) != 0;
        g_free (page_label);

        return retval;
}

static void
update_pages_label (EvPageActionWidget *action_widget,
		    gint                page)
{
	char *label_text;
	gint n_pages;

	n_pages = ev_document_get_n_pages (action_widget->document);
        if (show_page_number_in_pages_label (action_widget, page))
                label_text = g_strdup_printf (_("(%d of %d)"), page + 1, n_pages);
        else
                label_text = g_strdup_printf (_("of %d"), n_pages);
	gtk_entry_set_text (GTK_ENTRY (action_widget->label), label_text);
	g_free (label_text);
}

static void
ev_page_action_widget_set_current_page (EvPageActionWidget *action_widget,
					gint                page)
{
	if (page >= 0) {
		gchar *page_label;

		page_label = ev_document_get_page_label (action_widget->document, page);
		gtk_entry_set_text (GTK_ENTRY (action_widget->entry), page_label);
		gtk_editable_set_position (GTK_EDITABLE (action_widget->entry), -1);
		g_free (page_label);
	} else {
		gtk_entry_set_text (GTK_ENTRY (action_widget->entry), "");
	}

	update_pages_label (action_widget, page);
}

static void
ev_page_action_widget_update_max_width (EvPageActionWidget *action_widget)
{
        gchar *max_label;
        gint   n_pages;
        gint   max_label_len;
        gchar *max_page_label;
        gchar *max_page_numeric_label;

        n_pages = ev_document_get_n_pages (action_widget->document);

        max_page_label = ev_document_get_page_label (action_widget->document, n_pages - 1);
        max_page_numeric_label = g_strdup_printf ("%d", n_pages);
        if (ev_document_has_text_page_labels (action_widget->document) != 0) {
                max_label = g_strdup_printf (_("(%d of %d)"), n_pages, n_pages);
                /* Do not take into account the parentheses for the size computation */
                max_label_len = g_utf8_strlen (max_label, -1) - 2;
        } else {
                max_label = g_strdup_printf (_("of %d"), n_pages);
                max_label_len = g_utf8_strlen (max_label, -1);
        }
        g_free (max_page_label);

        gtk_entry_set_width_chars (GTK_ENTRY (action_widget->label), max_label_len);
        g_free (max_label);

        max_label_len = ev_document_get_max_label_len (action_widget->document);
        gtk_entry_set_width_chars (GTK_ENTRY (action_widget->entry),
                                   CLAMP (max_label_len, strlen (max_page_numeric_label) + 1, 12));
        g_free (max_page_numeric_label);
}

static void
page_changed_cb (EvDocumentModel    *model,
		 gint                old_page,
		 gint                new_page,
		 EvPageActionWidget *action_widget)
{
	ev_page_action_widget_set_current_page (action_widget, new_page);
}

static gboolean
page_scroll_cb (EvPageActionWidget *action_widget, GdkEventScroll *event)
{
	EvDocumentModel *model = action_widget->doc_model;
	gint pageno;

	pageno = ev_document_model_get_page (model);
	if ((event->direction == GDK_SCROLL_DOWN) &&
	    (pageno < ev_document_get_n_pages (action_widget->document) - 1))
		pageno++;
	if ((event->direction == GDK_SCROLL_UP) && (pageno > 0))
		pageno--;
	ev_document_model_set_page (model, pageno);

	return TRUE;
}

static void
activate_cb (EvPageActionWidget *action_widget)
{
	EvDocumentModel *model;
	const char *text;
	EvLinkDest *link_dest;
	EvLinkAction *link_action;
	EvLink *link;
	gchar *link_text;
	gchar *new_text;
	gint current_page;

	model = action_widget->doc_model;
	current_page = ev_document_model_get_page (model);

	text = gtk_entry_get_text (GTK_ENTRY (action_widget->entry));

	/* If we are not in search mode, i.e. we are entering a page number */
	if (!ev_page_action_widget_completion_search_is_enabled (action_widget)) {
		/* Convert utf8 fullwidth numbers (eg. japanese) to halfwidth - fixes #1518 */
		new_text = g_utf8_normalize (text, -1, G_NORMALIZE_ALL);
		gtk_entry_set_text (GTK_ENTRY (action_widget->entry), new_text);
		text = gtk_entry_get_text (GTK_ENTRY (action_widget->entry));
		g_free (new_text);
	}

	link_dest = ev_link_dest_new_page_label (text);
	link_action = ev_link_action_new_dest (link_dest);
	link_text = g_strdup_printf (_("Page %s"), text);
	link = ev_link_new (link_text, link_action);

	g_signal_emit (action_widget, widget_signals[WIDGET_ACTIVATE_LINK], 0, link);

	g_object_unref (link_dest);
	g_object_unref (link_action);
	g_object_unref (link);
	g_free (link_text);

	if (current_page == ev_document_model_get_page (model))
		ev_page_action_widget_set_current_page (action_widget, current_page);
}

static gboolean
disable_completion_search (EvPageActionWidget *action_widget)
{
	ev_page_action_widget_enable_completion_search (action_widget, FALSE);
	return G_SOURCE_REMOVE;
}

static gboolean
focus_out_cb (EvPageActionWidget *action_widget)
{
        ev_page_action_widget_set_current_page (action_widget,
                                                ev_document_model_get_page (action_widget->doc_model));
        g_object_set (action_widget->entry, "xalign", 1.0, NULL);
        ev_page_action_widget_update_max_width (action_widget);
        g_idle_add ((GSourceFunc)disable_completion_search, action_widget);

        return FALSE;
}

static void
ev_page_action_widget_init (EvPageActionWidget *action_widget)
{
	GtkWidget *hbox;
	AtkObject *obj;
        GtkStyleContext *style_context;

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

        style_context = gtk_widget_get_style_context (hbox);
        gtk_style_context_add_class (style_context, "raised");
        gtk_style_context_add_class (style_context, "linked");
        gtk_style_context_add_class (style_context, "tnum");

	action_widget->entry = gtk_entry_new ();
	gtk_widget_add_events (action_widget->entry,
			       GDK_BUTTON_MOTION_MASK);
	gtk_entry_set_width_chars (GTK_ENTRY (action_widget->entry), 5);
	gtk_entry_set_text (GTK_ENTRY (action_widget->entry), "");
	g_signal_connect_swapped (action_widget->entry, "scroll-event",
				  G_CALLBACK (page_scroll_cb),
				  action_widget);
	g_signal_connect_swapped (action_widget->entry, "activate",
				  G_CALLBACK (activate_cb),
				  action_widget);
        g_signal_connect_swapped (action_widget->entry, "focus-out-event",
                                  G_CALLBACK (focus_out_cb),
                                  action_widget);
	g_object_set (action_widget->entry, "xalign", 1.0, NULL);

	obj = gtk_widget_get_accessible (action_widget->entry);
	atk_object_set_name (obj, "page-label-entry");

	gtk_box_pack_start (GTK_BOX (hbox), action_widget->entry,
			    FALSE, FALSE, 0);
	gtk_widget_show (action_widget->entry);

	action_widget->label = gtk_entry_new ();
        gtk_widget_set_sensitive (action_widget->label, FALSE);
        gtk_entry_set_width_chars (GTK_ENTRY (action_widget->label), 5);
	gtk_box_pack_start (GTK_BOX (hbox), action_widget->label,
			    FALSE, FALSE, 0);
	gtk_widget_show (action_widget->label);

	gtk_container_add (GTK_CONTAINER (action_widget), hbox);
	gtk_widget_show (hbox);

        gtk_widget_set_sensitive (GTK_WIDGET (action_widget), FALSE);
}

static void
ev_page_action_widget_set_document (EvPageActionWidget *action_widget,
                                    EvDocument         *document)
{
        if (document) {
                g_object_ref (document);
                gtk_widget_set_sensitive (GTK_WIDGET (action_widget), ev_document_get_n_pages (document) > 0);
        }

        if (action_widget->signal_id > 0) {
                if (action_widget->doc_model != NULL) {
                        g_signal_handler_disconnect (action_widget->doc_model,
                                                     action_widget->signal_id);
                }
                action_widget->signal_id = 0;
        }

        if (action_widget->document)
                g_object_unref (action_widget->document);
        action_widget->document = document;
        if (!action_widget->document)
                return;

        action_widget->signal_id =
                g_signal_connect (action_widget->doc_model,
                                  "page-changed",
                                  G_CALLBACK (page_changed_cb),
                                  action_widget);

        ev_page_action_widget_set_current_page (action_widget,
                                                ev_document_model_get_page (action_widget->doc_model));
        ev_page_action_widget_update_max_width (action_widget);
}

static void
ev_page_action_widget_document_changed_cb (EvDocumentModel    *model,
					   GParamSpec         *pspec,
					   EvPageActionWidget *action_widget)
{
        ev_page_action_widget_set_document (action_widget, ev_document_model_get_document (model));
}

void
ev_page_action_widget_set_model (EvPageActionWidget *action_widget,
				 EvDocumentModel    *model)
{
	if (action_widget->doc_model) {
		g_object_remove_weak_pointer (G_OBJECT (action_widget->doc_model),
					      (gpointer)&action_widget->doc_model);
	}
	action_widget->doc_model = model;
	g_object_add_weak_pointer (G_OBJECT (model),
				   (gpointer)&action_widget->doc_model);

        ev_page_action_widget_set_document (action_widget, ev_document_model_get_document (model));

	action_widget->notify_document_signal_id =
		g_signal_connect (model, "notify::document",
				  G_CALLBACK (ev_page_action_widget_document_changed_cb),
				  action_widget);
}

static void
ev_page_action_widget_finalize (GObject *object)
{
	EvPageActionWidget *action_widget = EV_PAGE_ACTION_WIDGET (object);

	if (action_widget->doc_model != NULL) {
		if (action_widget->signal_id > 0) {
			g_signal_handler_disconnect (action_widget->doc_model,
						     action_widget->signal_id);
			action_widget->signal_id = 0;
		}
		if (action_widget->notify_document_signal_id > 0) {
			g_signal_handler_disconnect (action_widget->doc_model,
						     action_widget->notify_document_signal_id);
			action_widget->notify_document_signal_id = 0;
		}
		g_object_remove_weak_pointer (G_OBJECT (action_widget->doc_model),
					      (gpointer)&action_widget->doc_model);
		action_widget->doc_model = NULL;
	}

	g_clear_object (&action_widget->completion);

        ev_page_action_widget_set_document (action_widget, NULL);

	G_OBJECT_CLASS (ev_page_action_widget_parent_class)->finalize (object);
}

static void
ev_page_action_widget_get_preferred_width (GtkWidget *widget,
                                           gint      *minimum_width,
                                           gint      *natural_width)
{
        GtkWidget *child;

        *minimum_width = *natural_width = 0;

        child = gtk_bin_get_child (GTK_BIN (widget));
        if (!child || !gtk_widget_get_visible (child))
                return;

        gtk_widget_get_preferred_width (child, minimum_width, natural_width);
        *natural_width = *minimum_width;
}

static void
ev_page_action_widget_class_init (EvPageActionWidgetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize = ev_page_action_widget_finalize;
        widget_class->get_preferred_width = ev_page_action_widget_get_preferred_width;

	widget_signals[WIDGET_ACTIVATE_LINK] =
		g_signal_new ("activate_link",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (EvPageActionWidgetClass, activate_link),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1,
			      G_TYPE_OBJECT);

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

	g_signal_emit (proxy, widget_signals[WIDGET_ACTIVATE_LINK], 0, link);

	if (link)
		g_object_unref (link);

	gtk_tree_iter_free (iter);
	
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

	if (link)
		g_object_unref (link);
	
	gtk_tree_iter_free (iter);
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


	if (link) {
		text = ev_link_get_title (link);
		g_object_unref (link);
	}

	gtk_tree_iter_free (iter);

	if (text && key) {
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

/* user data to set on the widget. */
#define EPA_FILTER_MODEL_DATA "epa-filter-model"

static gboolean
build_new_tree_cb (GtkTreeModel *model,
		   GtkTreePath  *path,
		   GtkTreeIter  *iter,
		   gpointer      data)
{
	GtkTreeModel *filter_model = GTK_TREE_MODEL (data);
	EvLink *link;
	EvLinkAction *action;
	EvLinkActionType type;

	gtk_tree_model_get (model, iter,
			    EV_DOCUMENT_LINKS_COLUMN_LINK, &link,
			    -1);

	if (!link)
		return FALSE;

	action = ev_link_get_action (link);
	if (!action) {
		g_object_unref (link);
		return FALSE;
	}
	
	type = ev_link_action_get_action_type (action);

	if (type == EV_LINK_ACTION_TYPE_GOTO_DEST) {
		GtkTreeIter filter_iter;

		gtk_list_store_append (GTK_LIST_STORE (filter_model), &filter_iter);
		gtk_list_store_set (GTK_LIST_STORE (filter_model), &filter_iter,
				    0, iter,
				    -1);
	}
	
	g_object_unref (link);
	
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


void
ev_page_action_widget_update_links_model (EvPageActionWidget *proxy, GtkTreeModel *model)
{
	GtkTreeModel *filter_model;
	GtkEntryCompletion *completion;
	GtkCellRenderer *renderer;

	if (!model || model == proxy->model)
		return;

	/* Magik */
	proxy->model = model;
	filter_model = get_filter_model_from_model (model);

	completion = gtk_entry_completion_new ();
	g_clear_object (&proxy->completion);
	proxy->completion = completion;
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
			      "width_chars", COMPLETION_RESULTS_WIDTH,
			      NULL);
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (completion), renderer, TRUE);
	gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (completion),
					    renderer,
					    (GtkCellLayoutDataFunc) display_completion_text,
					    proxy, NULL);
	gtk_entry_set_completion (GTK_ENTRY (proxy->entry), completion);
}

void
ev_page_action_widget_grab_focus (EvPageActionWidget *proxy)
{
	gtk_widget_grab_focus (proxy->entry);
}

void
ev_page_action_widget_clear (EvPageActionWidget *proxy)
{
	gtk_entry_set_text (GTK_ENTRY (proxy->entry), "");
}

/* Sets width of the text entry, width will be restablished
 * to default one on focus_out event. This function is used
 * when searching the Outline, so the user has more space
 * to write the search term. */
void
ev_page_action_widget_set_temporary_entry_width (EvPageActionWidget *proxy, gint width)
{
	gtk_entry_set_width_chars (GTK_ENTRY (proxy->entry), width);
	/* xalign will also be restablished on focus_out */
	g_object_set (proxy->entry, "xalign", 0., NULL);
}

/* Enables or disables the completion search on @proxy according to @enable */
void
ev_page_action_widget_enable_completion_search (EvPageActionWidget *proxy, gboolean enable)
{
	GtkEntryCompletion *completion = enable ? proxy->completion : NULL;
	gtk_entry_set_completion (GTK_ENTRY (proxy->entry), completion);
}

/* Returns whether the completion search is enabled in @proxy */
static gboolean
ev_page_action_widget_completion_search_is_enabled (EvPageActionWidget *proxy)
{
	return gtk_entry_get_completion (GTK_ENTRY (proxy->entry)) != NULL;
}