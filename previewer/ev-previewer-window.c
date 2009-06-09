/* ev-previewer-window.c: 
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2009 Carlos Garcia Campos <carlosgc@gnome.org>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>

#if GTKUNIXPRINT_ENABLED
#include <gtk/gtkunixprint.h>
#endif
#include <glib/gi18n.h>
#include <evince-view.h>
#include "ev-page-action.h"

#include "ev-previewer-window.h"

struct _EvPreviewerWindow {
	GtkWindow         base_instance;

	EvDocument       *document;

	GtkActionGroup   *action_group;
	GtkUIManager     *ui_manager;

	GtkWidget        *swindow;
	EvView           *view;
	gdouble           dpi;

	/* Printing */
	GtkPrintSettings *print_settings;
	GtkPageSetup     *print_page_setup;
#if GTKUNIXPRINT_ENABLED
	GtkPrinter       *printer;
#endif
	gchar            *print_job_title;
	gchar            *source_file;
};

struct _EvPreviewerWindowClass {
	GtkWindowClass base_class;
};

G_DEFINE_TYPE (EvPreviewerWindow, ev_previewer_window, GTK_TYPE_WINDOW)

static gdouble
get_screen_dpi (GtkWindow *window)
{
	GdkScreen *screen;
	gdouble    xdpi, ydpi;

	screen = gtk_window_get_screen (window);

	xdpi = 25.4 * gdk_screen_get_width (screen) / gdk_screen_get_width_mm (screen);
	ydpi = 25.4 * gdk_screen_get_height (screen) / gdk_screen_get_height_mm (screen);

	return (xdpi + ydpi) / 2.0;
}

static void
ev_previewer_window_set_view_size (EvPreviewerWindow *window)
{
	gint width, height;
	GtkRequisition vsb_requisition;
	GtkRequisition hsb_requisition;
	gint scrollbar_spacing;

	if (!window->view)
		return;

	/* Calculate the width available for the content */
	width  = window->swindow->allocation.width;
	height = window->swindow->allocation.height;

	if (gtk_scrolled_window_get_shadow_type (GTK_SCROLLED_WINDOW (window->swindow)) == GTK_SHADOW_IN) {
		width -=  2 * GTK_WIDGET (window->view)->style->xthickness;
		height -= 2 * GTK_WIDGET (window->view)->style->ythickness;
	}

	gtk_widget_size_request (GTK_SCROLLED_WINDOW (window->swindow)->vscrollbar,
				 &vsb_requisition);
	gtk_widget_size_request (GTK_SCROLLED_WINDOW (window->swindow)->hscrollbar,
				 &hsb_requisition);
	gtk_widget_style_get (window->swindow,
			      "scrollbar_spacing",
			      &scrollbar_spacing,
			      NULL);

	ev_view_set_zoom_for_size (window->view,
				   MAX (1, width),
				   MAX (1, height),
				   vsb_requisition.width + scrollbar_spacing,
				   hsb_requisition.height + scrollbar_spacing);
}

#if GTKUNIXPRINT_ENABLED
static void
ev_previewer_window_error_dialog_run (EvPreviewerWindow *window,
				      GError            *error)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (GTK_WINDOW (window),
					 GTK_DIALOG_MODAL |
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_CLOSE,
					 "%s", _("Failed to print document"));
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  "%s", error->message);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}
#endif

static void
ev_previewer_window_previous_page (GtkAction         *action,
				   EvPreviewerWindow *window)
{
	ev_view_previous_page (window->view);
}

static void
ev_previewer_window_next_page (GtkAction         *action,
			       EvPreviewerWindow *window)
{
	ev_view_next_page (window->view);
}

static void
ev_previewer_window_zoom_in (GtkAction         *action,
			     EvPreviewerWindow *window)
{
	ev_view_set_sizing_mode (window->view, EV_SIZING_FREE);
	ev_view_zoom_in (window->view);
}

static void
ev_previewer_window_zoom_out (GtkAction         *action,
			      EvPreviewerWindow *window)
{
	ev_view_set_sizing_mode (window->view, EV_SIZING_FREE);
	ev_view_zoom_out (window->view);
}

static void
ev_previewer_window_zoom_best_fit (GtkToggleAction   *action,
				   EvPreviewerWindow *window)
{
	if (gtk_toggle_action_get_active (action)) {
		ev_view_set_sizing_mode (window->view, EV_SIZING_BEST_FIT);
		ev_previewer_window_set_view_size (window);
	} else {
		ev_view_set_sizing_mode (window->view, EV_SIZING_FREE);
	}
}

static void
ev_previewer_window_zoom_page_width (GtkToggleAction   *action,
				     EvPreviewerWindow *window)
{
	if (gtk_toggle_action_get_active (action)) {
		ev_view_set_sizing_mode (window->view, EV_SIZING_FIT_WIDTH);
		ev_previewer_window_set_view_size (window);
	} else {
		ev_view_set_sizing_mode (window->view, EV_SIZING_FREE);
	}
}

static void
ev_previewer_window_action_page_activated (GtkAction         *action,
					   EvLink            *link,
					   EvPreviewerWindow *window)
{
	ev_view_handle_link (window->view, link);
	gtk_widget_grab_focus (GTK_WIDGET (window->view));
}

#if GTKUNIXPRINT_ENABLED
static void
ev_previewer_window_print_finished (GtkPrintJob       *print_job,
				    EvPreviewerWindow *window,
				    GError            *error)
{
	if (error) {
		ev_previewer_window_error_dialog_run (window, error);
	}

	g_object_unref (print_job);
	gtk_widget_destroy (GTK_WIDGET (window));
}

static void
ev_previewer_window_do_print (EvPreviewerWindow *window)
{
	GtkPrintJob *job;
	GError      *error = NULL;

	job = gtk_print_job_new (window->print_job_title ?
				 window->print_job_title :
				 window->source_file,
				 window->printer,
				 window->print_settings,
				 window->print_page_setup);
	if (gtk_print_job_set_source_file (job, window->source_file, &error)) {
		gtk_print_job_send (job,
				    (GtkPrintJobCompleteFunc)ev_previewer_window_print_finished,
				    window, NULL);
	} else {
		ev_previewer_window_error_dialog_run (window, error);
		g_error_free (error);
	}

	gtk_widget_hide (GTK_WIDGET (window));
}

static void
ev_previewer_window_enumerate_finished (EvPreviewerWindow *window)
{
	if (window->printer) {
		ev_previewer_window_do_print (window);
	} else {
		GError *error = NULL;

		g_set_error (&error,
			     GTK_PRINT_ERROR,
			     GTK_PRINT_ERROR_GENERAL,
			     _("The selected printer '%s' could not be found"),
			     gtk_print_settings_get_printer (window->print_settings));
				     
		ev_previewer_window_error_dialog_run (window, error);
		g_error_free (error);
	}
}

static gboolean
ev_previewer_window_enumerate_printers (GtkPrinter        *printer,
					EvPreviewerWindow *window)
{
	const gchar *printer_name;

	printer_name = gtk_print_settings_get_printer (window->print_settings);
	if ((printer_name
	     && strcmp (printer_name, gtk_printer_get_name (printer)) == 0) ||
	    (!printer_name && gtk_printer_is_default (printer))) {
		if (window->printer)
			g_object_unref (window->printer);
		window->printer = g_object_ref (printer);

		return TRUE; /* we're done */
	}

	return FALSE; /* continue the enumeration */
}

static void
ev_previewer_window_print (GtkAction         *action,
			   EvPreviewerWindow *window)
{
	if (!window->print_settings)
		window->print_settings = gtk_print_settings_new ();
	if (!window->print_page_setup)
		window->print_page_setup = gtk_page_setup_new ();
	gtk_enumerate_printers ((GtkPrinterFunc)ev_previewer_window_enumerate_printers,
				window,
				(GDestroyNotify)ev_previewer_window_enumerate_finished,
				FALSE);
}
#endif

static const GtkActionEntry action_entries[] = {
	{ "GoPreviousPage", GTK_STOCK_GO_UP, N_("_Previous Page"), "<control>Page_Up",
          N_("Go to the previous page"),
          G_CALLBACK (ev_previewer_window_previous_page) },
        { "GoNextPage", GTK_STOCK_GO_DOWN, N_("_Next Page"), "<control>Page_Down",
          N_("Go to the next page"),
          G_CALLBACK (ev_previewer_window_next_page) },
        { "ViewZoomIn", GTK_STOCK_ZOOM_IN, NULL, "<control>plus",
          N_("Enlarge the document"),
          G_CALLBACK (ev_previewer_window_zoom_in) },
        { "ViewZoomOut", GTK_STOCK_ZOOM_OUT, NULL, "<control>minus",
          N_("Shrink the document"),
          G_CALLBACK (ev_previewer_window_zoom_out) },
#if GTKUNIXPRINT_ENABLED
	{ "PreviewPrint", GTK_STOCK_PRINT, N_("Print"), NULL,
	  N_("Print this document"),
	  G_CALLBACK (ev_previewer_window_print) }
#endif
};

static const GtkToggleActionEntry toggle_action_entries[] = {
	{ "ViewBestFit", EV_STOCK_ZOOM_PAGE, N_("_Best Fit"), NULL,
	  N_("Make the current document fill the window"),
	  G_CALLBACK (ev_previewer_window_zoom_best_fit) },
	{ "ViewPageWidth", EV_STOCK_ZOOM_WIDTH, N_("Fit Page _Width"), NULL,
	  N_("Make the current document fill the window width"),
	  G_CALLBACK (ev_previewer_window_zoom_page_width) }
};

/* EvView callbacks */
static void
view_sizing_mode_changed (EvView            *view,
			  GParamSpec        *pspec,
			  EvPreviewerWindow *window)
{
	EvSizingMode sizing_mode;
	GtkAction   *action;

	if (!window->view)
		return;

	g_object_get (window->view,
		      "sizing_mode", &sizing_mode,
		      NULL);

	action = gtk_action_group_get_action (window->action_group, "ViewBestFit");
	g_signal_handlers_block_by_func (action,
					 G_CALLBACK (ev_previewer_window_zoom_best_fit),
					 window);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      sizing_mode == EV_SIZING_BEST_FIT);
	g_signal_handlers_unblock_by_func (action,
					   G_CALLBACK (ev_previewer_window_zoom_best_fit),
					   window);

	action = gtk_action_group_get_action (window->action_group, "ViewPageWidth");
	g_signal_handlers_block_by_func (action,
					 G_CALLBACK (ev_previewer_window_zoom_page_width),
					 window);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      sizing_mode == EV_SIZING_FIT_WIDTH);
	g_signal_handlers_unblock_by_func (action,
					   G_CALLBACK (ev_previewer_window_zoom_page_width),
					   window);
}

static void
ev_previewer_window_dispose (GObject *object)
{
	EvPreviewerWindow *window = EV_PREVIEWER_WINDOW (object);

	if (window->document) {
		g_object_unref (window->document);
		window->document = NULL;
	}

	if (window->action_group) {
		g_object_unref (window->action_group);
		window->action_group = NULL;
	}

	if (window->ui_manager) {
		g_object_unref (window->ui_manager);
		window->ui_manager = NULL;
	}

	if (window->print_settings) {
		g_object_unref (window->print_settings);
		window->print_settings = NULL;
	}

	if (window->print_page_setup) {
		g_object_unref (window->print_page_setup);
		window->print_page_setup = NULL;
	}

#if GTKUNIXPRINT_ENABLED
	if (window->printer) {
		g_object_unref (window->printer);
		window->printer = NULL;
	}
#endif

	if (window->print_job_title) {
		g_free (window->print_job_title);
		window->print_job_title = NULL;
	}

	if (window->source_file) {
		g_free (window->source_file);
		window->source_file = NULL;
	}

	G_OBJECT_CLASS (ev_previewer_window_parent_class)->dispose (object);
}

static gchar*
data_dir (void)
{
	gchar *datadir;
#ifdef G_OS_WIN32
	gchar *dir;

	dir = g_win32_get_package_installation_directory_of_module (NULL);
	datadir = g_build_filename (dir, "share", "evince", NULL);
	g_free (dir);
#else
	datadir = g_strdup (DATADIR);
#endif

       return datadir;
}

static void
ev_previewer_window_init (EvPreviewerWindow *window)
{
	GtkWidget *vbox;
	GtkWidget *toolbar;
	GtkAction *action;
	GError    *error = NULL;
	gchar     *datadir, *ui_path;

	gtk_window_set_default_size (GTK_WINDOW (window), 600, 600);
	
	window->action_group = gtk_action_group_new ("PreviewerActions");
	gtk_action_group_set_translation_domain (window->action_group, NULL);
	gtk_action_group_add_actions (window->action_group, action_entries,
				      G_N_ELEMENTS (action_entries),
				      window);
	gtk_action_group_add_toggle_actions (window->action_group, toggle_action_entries,
					     G_N_ELEMENTS (toggle_action_entries),
					     window);
	gtk_action_group_set_sensitive (window->action_group, FALSE);

	action = g_object_new (EV_TYPE_PAGE_ACTION,
			       "name", "PageSelector",
			       "label", _("Page"),
			       "tooltip", _("Select Page"),
			       "icon_name", "text-x-generic",
			       "visible_overflown", FALSE,
			       NULL);
	g_signal_connect (action, "activate_link",
			  G_CALLBACK (ev_previewer_window_action_page_activated),
			  window);
	gtk_action_group_add_action (window->action_group, action);
	g_object_unref (action);

	window->ui_manager = gtk_ui_manager_new ();
	gtk_ui_manager_insert_action_group (window->ui_manager,
					    window->action_group, 0);
	gtk_window_add_accel_group (GTK_WINDOW (window),
				    gtk_ui_manager_get_accel_group (window->ui_manager));
	datadir = data_dir ();
	ui_path = g_build_filename (datadir, "evince-previewer-ui.xml", NULL);
	if (!gtk_ui_manager_add_ui_from_file (window->ui_manager, ui_path, &error)) {
		g_warning ("Failed to load ui from evince-previewer-ui.xml: %s", error->message);
		g_error_free (error);
	}
	g_free (ui_path);
	g_free (datadir);

	vbox = gtk_vbox_new (FALSE, 0);

	toolbar = gtk_ui_manager_get_widget (window->ui_manager, "/PreviewToolbar");
	gtk_box_pack_start (GTK_BOX (vbox), toolbar, FALSE, FALSE, 0);
	gtk_widget_show (toolbar);
		
	window->swindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (window->swindow),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	
	window->view = EV_VIEW (ev_view_new ());
	g_signal_connect (window->view, "notify::sizing-mode",
			  G_CALLBACK (view_sizing_mode_changed),
			  window);
	g_signal_connect_swapped (window->view, "zoom_invalid",
				  G_CALLBACK (ev_previewer_window_set_view_size),
				  window);
	
	ev_view_set_screen_dpi (window->view, get_screen_dpi (GTK_WINDOW (window)));
	ev_view_set_continuous (window->view, FALSE);
	ev_view_set_sizing_mode (window->view, EV_SIZING_FIT_WIDTH);
	ev_view_set_loading (window->view, TRUE);
	view_sizing_mode_changed (window->view, NULL, window);

	gtk_container_add (GTK_CONTAINER (window->swindow), GTK_WIDGET (window->view));
	gtk_widget_show (GTK_WIDGET (window->view));

	gtk_box_pack_start (GTK_BOX (vbox), window->swindow, TRUE, TRUE, 0);
	gtk_widget_show (window->swindow);
	
	gtk_container_add (GTK_CONTAINER (window), vbox);
	gtk_widget_show (vbox);
}

static void
ev_previewer_window_class_init (EvPreviewerWindowClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->dispose = ev_previewer_window_dispose;
}

/* Public methods */
GtkWidget *
ev_previewer_window_new (void)
{
	return GTK_WIDGET (g_object_new (EV_TYPE_PREVIEWER_WINDOW, NULL));
}

void
ev_previewer_window_set_document (EvPreviewerWindow *window,
				  EvDocument        *document)
{
	GtkAction *action;
	
	g_return_if_fail (EV_IS_PREVIEWER_WINDOW (window));
	g_return_if_fail (EV_IS_DOCUMENT (document));
	
	if (window->document)
		return;

	action = gtk_action_group_get_action (window->action_group, "PageSelector");
	ev_page_action_set_document (EV_PAGE_ACTION (action), document);
	gtk_action_group_set_sensitive (window->action_group, TRUE);
	
	window->document = g_object_ref (document);
	ev_view_set_document (window->view, document);
	ev_view_set_zoom (window->view,
			  get_screen_dpi (GTK_WINDOW (window)) / 72.0,
			  FALSE);
	ev_view_set_loading (window->view, FALSE);
}

void
ev_previewer_window_set_print_settings (EvPreviewerWindow *window,
					const gchar       *print_settings)
{
	if (window->print_settings)
		g_object_unref (window->print_settings);
	if (window->print_page_setup)
		g_object_unref (window->print_page_setup);
	if (window->print_job_title)
		g_free (window->print_job_title);

	if (print_settings && g_file_test (print_settings, G_FILE_TEST_IS_REGULAR)) {
		GKeyFile *key_file;
		GError   *error = NULL;

		key_file = g_key_file_new ();
		g_key_file_load_from_file (key_file,
					   print_settings,
					   G_KEY_FILE_KEEP_COMMENTS |
					   G_KEY_FILE_KEEP_TRANSLATIONS,
					   &error);
		if (!error) {
			GtkPrintSettings *psettings;
			GtkPageSetup     *psetup;
			gchar            *job_name;

			psettings = gtk_print_settings_new_from_key_file (key_file,
									  "Print Settings",
									  NULL);
			window->print_settings = psettings ? psettings : gtk_print_settings_new ();

			psetup = gtk_page_setup_new_from_key_file (key_file,
								   "Page Setup",
								   NULL);
			window->print_page_setup = psetup ? psetup : gtk_page_setup_new ();

			job_name = g_key_file_get_string (key_file,
							  "Print Job", "title",
							  NULL);
			if (job_name) {
				window->print_job_title = job_name;
				gtk_window_set_title (GTK_WINDOW (window), job_name);
			}
		} else {
			window->print_settings = gtk_print_settings_new ();
			window->print_page_setup = gtk_page_setup_new ();
			g_error_free (error);
		}

		g_key_file_free (key_file);
	} else {
		window->print_settings = gtk_print_settings_new ();
		window->print_page_setup = gtk_page_setup_new ();
	}
}

void
ev_previewer_window_set_source_file (EvPreviewerWindow *window,
				     const gchar       *source_file)
{
	if (window->source_file)
		g_free (window->source_file);
	window->source_file = g_strdup (source_file);
}
