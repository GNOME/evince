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
#include <stdlib.h>

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
update_entry (EvPageAction *page_action, GtkWidget *entry)
{
	char *text;

	text = g_strdup_printf ("%d", page_action->priv->current_page);
	gtk_entry_set_text (GTK_ENTRY (entry), text);
	g_free (text);
}

static void
sync_entry (GtkAction *action, gpointer dummy, GtkWidget *proxy)
{
	EvPageAction *page_action = EV_PAGE_ACTION (action);
	GtkWidget *entry;

	entry = GTK_WIDGET (g_object_get_data (G_OBJECT (proxy), "entry"));
	update_entry (page_action, entry);
}

static void
activate_cb (GtkWidget *entry, GtkAction *action)
{
	EvPageAction *page_action = EV_PAGE_ACTION (action);
	const char *text;
	char *endptr;
	int page = -1;

	text = gtk_entry_get_text (GTK_ENTRY (entry));
	if (text) {
		long value;

		value = strtol (text, &endptr, 10);
		if (endptr[0] == '\0') {
			/* Page number is an integer */
			page = MIN (G_MAXINT, value);
		}
	}

	if (page > 0 && page <= page_action->priv->total_pages) {
		g_signal_emit (action, signals[GOTO_PAGE_SIGNAL], 0, page);
	} else {
		update_entry (page_action, entry);
	}
}

static void
entry_size_request_cb (GtkWidget      *entry,
		       GtkRequisition *requisition,
		       GtkAction      *action)
{
	PangoContext *context;
	PangoFontMetrics *metrics;
	int digit_width;

	context = gtk_widget_get_pango_context (entry);
	metrics = pango_context_get_metrics
			(context, entry->style->font_desc,
			 pango_context_get_language (context));

	digit_width = pango_font_metrics_get_approximate_digit_width (metrics);
	digit_width = PANGO_SCALE * ((digit_width + PANGO_SCALE - 1) / PANGO_SCALE);

	pango_font_metrics_unref (metrics);

	/* Space for 4 digits. Probably 3 would be enough but it doesnt
	   seem to possible to calculate entry borders without using
	   gtk private info */
	requisition->width = PANGO_PIXELS (digit_width * 4);
}

static GtkWidget *
create_tool_item (GtkAction *action)
{
	GtkWidget *hbox, *entry, *item, *label;

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 6); 
	gtk_widget_show (hbox);

	item = GTK_WIDGET (gtk_tool_item_new ());
	gtk_widget_show (item);

	entry = gtk_entry_new ();
	g_signal_connect (entry, "size_request",
			  G_CALLBACK (entry_size_request_cb),
			  action);
	g_object_set_data (G_OBJECT (item), "entry", entry);
	gtk_widget_show (entry);

	g_signal_connect (entry, "activate",
			  G_CALLBACK (activate_cb),
			  action);

	label = gtk_label_new ("");
	g_object_set_data (G_OBJECT (item), "label", label);
	update_label (action, NULL, item);
	gtk_widget_show (label);

	gtk_box_pack_start (GTK_BOX (hbox), entry, FALSE, FALSE, 0);
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
					 G_CALLBACK (sync_entry),
					 proxy, 0);
	}

	GTK_ACTION_CLASS (ev_page_action_parent_class)->connect_proxy (action, proxy);
}

static void
ev_page_action_init (EvPageAction *action)
{
	action->priv = EV_PAGE_ACTION_GET_PRIVATE (action);
}

static void
ev_page_action_finalize (GObject *object)
{
	G_OBJECT_CLASS (ev_page_action_parent_class)->finalize (object);
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
