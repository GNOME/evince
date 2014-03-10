/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2007 Johannes Buchner
 *
 *  Author:
 *    Johannes Buchner <buchner.johannes@gmx.at>
 *    Lukas Bezdicka <255993@mail.muni.cz>
 *
 * Evince is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Evince is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "ev-dualscreen.h"
#include "ev-view.h"
#include "ev-utils.h"
#include "ev-sidebar.h"
#include "ev-sidebar-thumbnails.h"
#include "ev-presentation-timer.h"

struct _EvDSCWindowPrivate {
	GtkWidget       *main_box;
	GtkWidget       *menubar;
	GtkWidget       *sidebar;
	GtkWidget       *notesview;
	GtkWidget       *timer;
	GtkWidget       *spinner;
	GtkWidget       *presentation_window;
	GtkWidget       *overview_scrolled_window;
	GtkWidget       *notesview_scrolled_window;

	EvDocumentModel *model;
	EvDocumentModel *notes_model;
	EvDocument      *presentation_document;
	EvDocument      *notes_document;
	EvMetadata      *metadata;
	EvViewPresentation *presentation_view;

	gint		moveback_monitor;
	gint		presentation_monitor;
	guint		page;
};

#define EV_DSCWINDOW_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_DSCWINDOW, EvDSCWindowPrivate))
#define PAGE_CACHE_SIZE 52428800 /* 50MB */
#define SIDEBAR_DEFAULT_SIZE    30 /* This seems like bug in gtk to me */
#define MAX_PRESENTATION_TIME   1440 /* 60*24 ONE DAY */

G_DEFINE_TYPE (EvDSCWindow, ev_dscwindow, GTK_TYPE_WINDOW)

static gint
ev_dscwindow_get_presentation_window_monitor (EvDSCWindow *ev_dscwindow)
{
	GtkWindow *presentation_window = GTK_WINDOW (ev_dscwindow->priv->presentation_window);
	GdkScreen *screen = gtk_window_get_screen (presentation_window);
	gint work_monitor = gdk_screen_get_monitor_at_window (screen, gtk_widget_get_window (GTK_WIDGET (presentation_window)));
	return work_monitor;
}

static void
ev_dscwindow_setup_from_metadata (EvDSCWindow *ev_dscwindow)
{
	if (!ev_dscwindow->priv->metadata)
		return;
	gint	int_value;
	gdouble double_value;
	if (ev_metadata_get_double (ev_dscwindow->priv->metadata, "presentation-time", &double_value))
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (ev_dscwindow->priv->spinner),double_value);
	if (ev_metadata_get_int (ev_dscwindow->priv->metadata, "presentation-monitor", &int_value)) {
		ev_dscwindow->priv->presentation_monitor = int_value;
	} else {
		ev_dscwindow->priv->presentation_monitor = (ev_dscwindow->priv->moveback_monitor +  1) % 2;
	}
}

static gboolean
ev_dscwindow_windows_placement (EvDSCWindow *ev_dscwindow)
{
	if (!EV_IS_DSCWINDOW (ev_dscwindow))
		return FALSE;

	gint num_monitors = get_num_monitors (GTK_WINDOW (ev_dscwindow));
	if (num_monitors == 2) {
		GdkRectangle coords;
		GdkScreen *screen = gtk_window_get_screen (GTK_WINDOW (ev_dscwindow));
		gint presentation_monitor = ev_dscwindow->priv->presentation_monitor;
		gint control_monitor = (presentation_monitor + 1) % 2;

		gdk_screen_get_monitor_geometry (screen, control_monitor, &coords);
		gtk_window_unmaximize (GTK_WINDOW (ev_dscwindow));
		gtk_window_move (GTK_WINDOW (ev_dscwindow), coords.x, coords.y);
		gtk_window_maximize (GTK_WINDOW (ev_dscwindow));

		gdk_screen_get_monitor_geometry (screen, presentation_monitor,&coords);
		gtk_window_move (GTK_WINDOW (ev_dscwindow->priv->presentation_window), coords.x, coords.y);

		return TRUE;
	} else
		return FALSE;
}

static void
ev_dscwindow_switch_monitors (EvDSCWindow *ev_dscwindow)
{
	gint num_monitors = get_num_monitors (GTK_WINDOW (ev_dscwindow));
	if (num_monitors == 2) {
		ev_dscwindow->priv->presentation_monitor = (ev_dscwindow->priv->presentation_monitor + 1) % 2;
		if (ev_dscwindow->priv->metadata)
			ev_metadata_set_int (ev_dscwindow->priv->metadata,
			        "presentation-monitor", ev_dscwindow->priv->presentation_monitor);
	}
	ev_dscwindow_windows_placement (ev_dscwindow);
}

static void
ev_dscwindow_sidebar_visibility_cb (GtkWidget *sidebar)
{
	gtk_widget_set_visible (sidebar, !(gtk_widget_get_visible(sidebar)));
}

static void
ev_dscwindow_set_page (EvDSCWindow *ev_dscwindow, gint page)
{
	if((ev_dscwindow->priv->page == 0) && (page == 1))
		ev_presentation_timer_start (EV_PRESENTATION_TIMER (ev_dscwindow->priv->timer));
	if(!(ev_dscwindow->priv->page == page)) {
		ev_dscwindow->priv->page = page;
		if(ev_document_model_get_page (ev_dscwindow->priv->model) != page)
			ev_document_model_set_page(ev_dscwindow->priv->model, page);
		if(ev_document_model_get_page (ev_dscwindow->priv->notes_model) != page)
			ev_document_model_set_page(ev_dscwindow->priv->notes_model, page);
		if(ev_view_presentation_get_current_page (EV_VIEW_PRESENTATION(ev_dscwindow->priv->presentation_view)) != page);
			ev_view_presentation_set_page (EV_VIEW_PRESENTATION(ev_dscwindow->priv->presentation_view), page);
		ev_presentation_timer_set_page (EV_PRESENTATION_TIMER(ev_dscwindow->priv->timer), page);
	}
}

static void
ev_dscwindow_presentation_time_cb (EvDSCWindow *ev_dscwindow)
{
	gint time = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (ev_dscwindow->priv->spinner));
	ev_presentation_timer_set_time (EV_PRESENTATION_TIMER (ev_dscwindow->priv->timer),time);
	if (ev_dscwindow->priv->metadata)
		ev_metadata_set_double (ev_dscwindow->priv->metadata, "presentation-time", time);
}

static void
ev_dscwindow_page_changed_cb (EvDocumentModel *model,
			      GParamSpec      *pspec,
			      EvDSCWindow     *ev_dscwindow)
{
	ev_dscwindow_set_page (ev_dscwindow, ev_document_model_get_page (model));
}

static void
ev_dscwindow_presentation_page_changed_cb (EvViewPresentation   *pview,
					   GParamSpec 		*pspec,
					   EvDSCWindow		*ev_dscwindow)
{
	ev_dscwindow_set_page (ev_dscwindow, ev_view_presentation_get_current_page (pview));
}

static gboolean
ev_dscwindow_notes_interaction (GtkContainer *container, EvDSCWindow *ev_dscwindow)
{
	GtkWidget *dialog;

	dialog = gtk_file_chooser_dialog_new (
		_("Open Document"),
		GTK_WINDOW (ev_dscwindow),
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
		GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
		NULL);

	ev_document_factory_add_filters (dialog, NULL);
	gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (dialog), FALSE);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), TRUE);
	char *uri;
	if (ev_dscwindow->priv->metadata &&
	    ev_metadata_get_string (ev_dscwindow->priv->metadata, "notes-uri", &uri))
		gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (dialog),uri);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
	{
		uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
		GError * error = NULL;
		ev_view_set_loading (EV_VIEW (ev_dscwindow->priv->notesview), TRUE);

		if (ev_dscwindow->priv->notes_document) {
			ev_document_load (ev_dscwindow->priv->notes_document, uri, &error);
		} else {
			ev_dscwindow->priv->notes_document = ev_document_factory_get_document (uri,
				&error);
		}
		if (error == NULL){
			if (ev_dscwindow->priv->metadata)
				ev_metadata_set_string (ev_dscwindow->priv->metadata,
					"notes-uri", uri);
			ev_dscwindow->priv->notes_model = ev_document_model_new ();
			ev_document_model_set_document (ev_dscwindow->priv->notes_model,
							ev_dscwindow->priv->notes_document);
			ev_document_model_set_continuous (ev_dscwindow->priv->notes_model,
							  FALSE);
			ev_document_model_set_dual_page (ev_dscwindow->priv->notes_model,
							 FALSE);
			ev_document_model_set_sizing_mode (ev_dscwindow->priv->notes_model,
							   EV_SIZING_BEST_FIT);
			ev_document_model_set_page (ev_dscwindow->priv->notes_model,
			        ev_document_model_get_page (ev_dscwindow->priv->model));
			ev_view_set_model(EV_VIEW(ev_dscwindow->priv->notesview),
					  ev_dscwindow->priv->notes_model);
			g_signal_connect (G_OBJECT(ev_dscwindow->priv->notes_model),
					  "notify::page",
					  G_CALLBACK (ev_dscwindow_page_changed_cb),
					  ev_dscwindow);
		}
	}
	g_free (uri);
	gtk_widget_destroy (dialog);

	return TRUE;
}

EvDSCWindow *
ev_dscwindow_get_control (void)
{
	static EvDSCWindow *control = NULL;

	if (!control || !EV_IS_DSCWINDOW (control)) {
		control = EV_DSCWINDOW (g_object_new (EV_TYPE_DSCWINDOW, NULL));
	}

	return control;
}

void
ev_dscwindow_set_presentation   (EvDSCWindow    *ev_dscwindow,
				 EvWindow       *presentation_window,
				 EvDocument     *document,
				 EvViewPresentation *pview,
				 EvMetadata     *metadata)
{
	if (!EV_IS_WINDOW (presentation_window) || !EV_IS_DSCWINDOW (ev_dscwindow) ||
	    !EV_IS_VIEW_PRESENTATION (pview) || !EV_IS_DOCUMENT (document) )
		return;
	ev_dscwindow->priv->presentation_window = GTK_WIDGET(presentation_window);
	ev_dscwindow->priv->presentation_document = document;
	ev_dscwindow->priv->presentation_view = EV_VIEW_PRESENTATION(pview);
	ev_dscwindow->priv->page = ev_view_presentation_get_current_page (pview);
	ev_dscwindow->priv->moveback_monitor = ev_dscwindow_get_presentation_window_monitor (ev_dscwindow);
	ev_dscwindow->priv->metadata = metadata;

	ev_document_model_set_document(ev_dscwindow->priv->model, document);
	ev_document_model_set_page(ev_dscwindow->priv->model, ev_dscwindow->priv->page);
	/*signals*/
	g_signal_connect_swapped (ev_dscwindow->priv->presentation_view,
				  "destroy",
				  G_CALLBACK (gtk_widget_destroy),
				  GTK_WIDGET (ev_dscwindow));
	g_signal_connect (G_OBJECT(ev_dscwindow->priv->model),
			  "notify::page",
			  G_CALLBACK (ev_dscwindow_page_changed_cb),
			  ev_dscwindow);
	g_signal_connect (G_OBJECT(ev_dscwindow->priv->presentation_view),
			  "notify::page",
			  G_CALLBACK (ev_dscwindow_presentation_page_changed_cb),
			  ev_dscwindow);
	ev_presentation_timer_set_pages (EV_PRESENTATION_TIMER(ev_dscwindow->priv->timer),
		ev_document_get_n_pages (document));
	/* Wait for windows to get shown before moving them */
	while(gtk_events_pending())
		gtk_main_iteration();
	/* something like http://mail.gnome.org/archives/gtk-list/2008-June/msg00061.html */
	ev_dscwindow_setup_from_metadata (ev_dscwindow);
	ev_dscwindow_windows_placement (ev_dscwindow);
}

static gboolean
ev_dscwindow_end (GtkWidget *widget, GdkEvent *event)
{
	EvDSCWindow *ev_dscwindow = EV_DSCWINDOW(widget);
	EvWindow *ev_window = EV_WINDOW(ev_dscwindow->priv->presentation_window);
	ev_window_stop_presentation (ev_window, TRUE);
	gtk_widget_destroy (widget);
	return TRUE;
}

static void
ev_dscwindow_init (EvDSCWindow *ev_dscwindow)
{

	ev_dscwindow->priv = EV_DSCWINDOW_GET_PRIVATE (ev_dscwindow);
	ev_dscwindow->priv->page = 0;
	ev_dscwindow->priv->moveback_monitor = -1;
	ev_dscwindow->priv->notes_document = NULL;

	gtk_window_set_title (GTK_WINDOW (ev_dscwindow), _("Presentation Control"));

	GtkWidget *hpaned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	ev_dscwindow->priv->model = ev_document_model_new ();
	ev_document_model_set_continuous (ev_dscwindow->priv->model, FALSE);
	ev_document_model_set_dual_page (ev_dscwindow->priv->model, FALSE);
	ev_document_model_set_sizing_mode (ev_dscwindow->priv->model,
		EV_SIZING_BEST_FIT);

	ev_dscwindow->priv->sidebar = ev_sidebar_new ();
	ev_sidebar_set_model (EV_SIDEBAR (ev_dscwindow->priv->sidebar),
			      ev_dscwindow->priv->model);

	GtkWidget *sidebar_widget;
	sidebar_widget = ev_sidebar_thumbnails_new ();
	ev_sidebar_add_page (EV_SIDEBAR (ev_dscwindow->priv->sidebar),
			     sidebar_widget);

	ev_dscwindow->priv->notesview_scrolled_window =
		GTK_WIDGET (g_object_new(GTK_TYPE_SCROLLED_WINDOW,
			    "shadow-type",
			    GTK_SHADOW_IN,
			    NULL));
	ev_dscwindow->priv->notesview = ev_view_new ();
	gtk_container_add (GTK_CONTAINER (ev_dscwindow->priv->notesview_scrolled_window),
			   ev_dscwindow->priv->notesview);
	ev_dscwindow->priv->notes_model = ev_dscwindow->priv->model;
	ev_view_set_model (EV_VIEW (ev_dscwindow->priv->notesview),
			   ev_dscwindow->priv->notes_model);
	gtk_paned_pack1 (GTK_PANED (hpaned),
			 ev_dscwindow->priv->sidebar,
			 FALSE,
			 TRUE);
	gtk_paned_pack2 (GTK_PANED (hpaned),
			 ev_dscwindow->priv->notesview_scrolled_window,
			 FALSE,
			 FALSE);

	gtk_box_pack_start(GTK_BOX(vbox),hpaned,TRUE,TRUE,0);

	GtkWidget *expander = gtk_expander_new (_("Dualscreen configuration"));
	gtk_expander_set_expanded (GTK_EXPANDER (expander), FALSE);
	GtkWidget *toolbar = gtk_toolbar_new ();

	GtkToolItem *b_switch = gtk_tool_button_new (NULL, _("Switch monitors"));
	gtk_tool_item_set_tooltip_text (b_switch,
	        _("Switch monitors, In case of more than two monitor window placing has to be manual."));
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (b_switch), "object-flip-horizontal");
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), b_switch, -1);
	g_signal_connect_swapped (b_switch, "clicked",
		G_CALLBACK (ev_dscwindow_switch_monitors), ev_dscwindow);

	GtkToolItem *b_notes = gtk_tool_button_new_from_stock (GTK_STOCK_OPEN);
	gtk_tool_button_set_label (GTK_TOOL_BUTTON(b_notes), _("Load notes..."));
	gtk_tool_item_set_tooltip_text (b_notes, _("Load your notes document"));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), b_notes, -1);
	g_signal_connect (b_notes, "clicked",
		G_CALLBACK (ev_dscwindow_notes_interaction), ev_dscwindow);

	GtkToolItem *b_close = gtk_tool_button_new_from_stock (GTK_STOCK_CLOSE);
	gtk_tool_button_set_label (GTK_TOOL_BUTTON(b_close), _("End presentation"));
	gtk_tool_item_set_tooltip_text (b_close, _("End presentation"));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), b_close, -1);
	g_signal_connect_swapped (b_close, "clicked",
		G_CALLBACK (ev_dscwindow_end), ev_dscwindow);

	GtkToolItem *b_sidebar = gtk_tool_button_new_from_stock (GTK_STOCK_PAGE_SETUP);
	gtk_tool_button_set_label (GTK_TOOL_BUTTON(b_sidebar), _("Show sidebar"));
	gtk_tool_item_set_tooltip_text (b_sidebar, _("Show/hide sidebar"));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), b_sidebar, -1);
	g_signal_connect_swapped (b_sidebar, "clicked",
		G_CALLBACK (ev_dscwindow_sidebar_visibility_cb), ev_dscwindow->priv->sidebar);

	GtkToolItem *b_spinner = gtk_tool_item_new ();
	GtkWidget* alignment = gtk_alignment_new (0.0f, 0.5f, 1.0f, 0.1f);
	GtkAdjustment *timer_adjust = gtk_adjustment_new (-1.0, -1.0,
		MAX_PRESENTATION_TIME, 1.0, 10.0, 10.0);
	ev_dscwindow->priv->spinner = gtk_spin_button_new (timer_adjust, 1.0, 0);
	g_signal_connect_swapped (ev_dscwindow->priv->spinner, "value-changed",
	        G_CALLBACK (ev_dscwindow_presentation_time_cb), ev_dscwindow);
	gtk_container_add (GTK_CONTAINER (b_spinner), alignment);
	gtk_container_add (GTK_CONTAINER (alignment), ev_dscwindow->priv->spinner);
	gtk_tool_item_set_tooltip_text (b_spinner,
	        _("To enable timer, set presentation timer to expected time in minutes. Timer starts by changing from first slide to second one. Value -1 means disabled."));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), b_spinner, -1);

	gtk_container_add (GTK_CONTAINER (expander), toolbar);

	GtkWidget *hpan = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_paned_pack1 (GTK_PANED(hpan), expander, FALSE, TRUE);
	ev_dscwindow->priv->timer = ev_presentation_timer_new ();
	gtk_paned_pack2 (GTK_PANED(hpan),ev_dscwindow->priv->timer, FALSE, FALSE);

	gtk_box_pack_end (GTK_BOX (vbox), hpan, FALSE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (ev_dscwindow), vbox);
	gtk_widget_show_all(vbox);
	gtk_paned_set_position (GTK_PANED (hpan), SIDEBAR_DEFAULT_SIZE);
	gtk_paned_set_position (GTK_PANED (hpaned), SIDEBAR_DEFAULT_SIZE);
}

static void
ev_dscwindow_dispose (GObject *obj)
{
	EvDSCWindow *ev_dscwindow = EV_DSCWINDOW (obj);
	EvDSCWindowPrivate *priv = EV_DSCWINDOW (ev_dscwindow)->priv;
	EvWindow *ev_window = EV_WINDOW(priv->presentation_window);
	if(EV_IS_WINDOW (ev_window)) {
		if (priv->moveback_monitor >= 0) {
			GtkWindow *presentation_window = GTK_WINDOW (priv->presentation_window);
			GdkRectangle coords;

			gdk_screen_get_monitor_geometry (gtk_window_get_screen (presentation_window),
						 priv->moveback_monitor, &coords);
			gtk_window_move (presentation_window, coords.x, coords.y);
		}
	}
	G_OBJECT_CLASS (ev_dscwindow_parent_class)->dispose (obj);
}

static void
ev_dscwindow_class_init (EvDSCWindowClass *ev_dscwindow_class)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (ev_dscwindow_class);
	g_type_class_add_private (g_object_class, sizeof (EvDSCWindowPrivate));
	g_object_class->dispose  = ev_dscwindow_dispose;
}

GtkWidget *
ev_dscwindow_new (void)
{
	EvDSCWindow *ev_dscwindow;

	ev_dscwindow = g_object_new (EV_TYPE_DSCWINDOW, "type", GTK_WINDOW_TOPLEVEL, NULL);
	return GTK_WIDGET (ev_dscwindow);
}
