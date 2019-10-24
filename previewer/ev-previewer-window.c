/* ev-previewer-window.c: 
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2009 Carlos Garcia Campos <carlosgc@gnome.org>
 * Copyright (C) 2018 Germán Poo-Caamaño <gpoo@gnome.org>
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

#include <config.h>

#if GTKUNIXPRINT_ENABLED
#include <gtk/gtkunixprint.h>
#endif
#include <glib/gi18n.h>
#include <evince-view.h>
#include "ev-page-action-widget.h"

#include "ev-previewer-window.h"
#include "ev-previewer-toolbar.h"

struct _EvPreviewerWindow {
	GtkApplicationWindow base_instance;

	EvDocumentModel  *model;
	EvDocument       *document;

	EvView           *view;
	GtkWidget        *toolbar;

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
	GtkApplicationWindowClass base_class;
};

enum {
	PROP_0,
	PROP_MODEL
};

#define MIN_SCALE 0.05409
#define MAX_SCALE 4.0

G_DEFINE_TYPE (EvPreviewerWindow, ev_previewer_window, GTK_TYPE_APPLICATION_WINDOW)

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
ev_previewer_window_close (GSimpleAction *action,
			   GVariant      *parameter,
                           gpointer       user_data)
{
        EvPreviewerWindow *window = EV_PREVIEWER_WINDOW (user_data);

	gtk_widget_destroy (GTK_WIDGET (window));
}

static void
ev_previewer_window_previous_page (GSimpleAction *action,
				   GVariant      *parameter,
				   gpointer       user_data)
{
        EvPreviewerWindow *window = EV_PREVIEWER_WINDOW (user_data);

	ev_view_previous_page (window->view);
}

static void
ev_previewer_window_next_page (GSimpleAction *action,
			       GVariant      *parameter,
                               gpointer       user_data)
{
        EvPreviewerWindow *window = EV_PREVIEWER_WINDOW (user_data);

	ev_view_next_page (window->view);
}

static void
ev_previewer_window_zoom_in (GSimpleAction *action,
			     GVariant      *parameter,
                             gpointer       user_data)
{
        EvPreviewerWindow *window = EV_PREVIEWER_WINDOW (user_data);

	ev_document_model_set_sizing_mode (window->model, EV_SIZING_FREE);
	ev_view_zoom_in (window->view);
}

static void
ev_previewer_window_zoom_out (GSimpleAction *action,
			      GVariant      *parameter,
			      gpointer       user_data)
{
        EvPreviewerWindow *window = EV_PREVIEWER_WINDOW (user_data);

	ev_document_model_set_sizing_mode (window->model, EV_SIZING_FREE);
	ev_view_zoom_out (window->view);
}

static void
ev_previewer_window_zoom_default (GSimpleAction *action,
				  GVariant      *parameter,
                                  gpointer       user_data)
{
        EvPreviewerWindow *window = EV_PREVIEWER_WINDOW (user_data);

	ev_document_model_set_sizing_mode (window->model,
					   EV_SIZING_AUTOMATIC);
}

static void
ev_previewer_window_action_page_activated (GtkAction         *action,
                                           EvLink            *link,
                                           EvPreviewerWindow *window)
{
       ev_view_handle_link (window->view, link);
       gtk_widget_grab_focus (GTK_WIDGET (window->view));
}

static void
ev_previewer_window_focus_page_selector (GSimpleAction *action,
					 GVariant      *parameter,
                                         gpointer       user_data)
{
        EvPreviewerWindow *window = EV_PREVIEWER_WINDOW (user_data);
	GtkWidget *page_selector;

	page_selector = ev_previewer_toolbar_get_page_selector (EV_PREVIEWER_TOOLBAR (window->toolbar));
	ev_page_action_widget_grab_focus (EV_PAGE_ACTION_WIDGET (page_selector));
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
			     _("The selected printer “%s” could not be found"),
			     gtk_print_settings_get_printer (window->print_settings));

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
ev_previewer_window_print (GSimpleAction *action,
			   GVariant      *parameter,
			   gpointer       user_data)
{
        EvPreviewerWindow *window = EV_PREVIEWER_WINDOW (user_data);

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

static const GActionEntry actions[] = {
	{ "print", ev_previewer_window_print },
	{ "go-previous-page", ev_previewer_window_previous_page },
	{ "go-next-page", ev_previewer_window_next_page },
	{ "select-page", ev_previewer_window_focus_page_selector },
	{ "zoom-in", ev_previewer_window_zoom_in },
	{ "zoom-out", ev_previewer_window_zoom_out },
	{ "close", ev_previewer_window_close },
	{ "zoom-default", ev_previewer_window_zoom_default },
};

static void
ev_previewer_window_set_action_enabled (EvPreviewerWindow *window,
                                        const char        *name,
                                        gboolean           enabled)
{
	GAction *action;

	action = g_action_map_lookup_action (G_ACTION_MAP (window), name);
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);
}

static void
model_page_changed (EvDocumentModel* model,
		    gint old_page,
		    gint new_page,
		    EvPreviewerWindow *window)
{
	gint n_pages = ev_document_get_n_pages (ev_document_model_get_document (window->model));

        ev_previewer_window_set_action_enabled (window,
                                                "go-previous-page",
                                                new_page > 0);
        ev_previewer_window_set_action_enabled (window,
                                                "go-next-page",
                                                new_page < n_pages - 1);
}

static void
view_sizing_mode_changed (EvDocumentModel   *model,
			  GParamSpec        *pspec,
			  EvPreviewerWindow *window)
{
	EvSizingMode sizing_mode = ev_document_model_get_sizing_mode (model);

        ev_previewer_window_set_action_enabled (window,
                                                "zoom-default",
                                                sizing_mode != EV_SIZING_AUTOMATIC);
}

static void
ev_previewer_window_set_document (EvPreviewerWindow *window,
				  GParamSpec        *pspec,
				  EvDocumentModel   *model)
{
	EvDocument *document = ev_document_model_get_document (model);

	window->document = g_object_ref (document);

	g_signal_connect (model, "notify::sizing-mode",
			  G_CALLBACK (view_sizing_mode_changed),
			  window);
}

static void
ev_previewer_window_dispose (GObject *object)
{
	EvPreviewerWindow *window = EV_PREVIEWER_WINDOW (object);

	if (window->model) {
		g_object_unref (window->model);
		window->model = NULL;
	}

	if (window->document) {
		g_object_unref (window->document);
		window->document = NULL;
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

static void
ev_previewer_window_init (EvPreviewerWindow *window)
{
	gtk_window_set_default_size (GTK_WINDOW (window), 600, 600);

	g_action_map_add_action_entries (G_ACTION_MAP (window),
					 actions, G_N_ELEMENTS (actions),
					 window);
}

static void
ev_previewer_window_set_property (GObject      *object,
				  guint         prop_id,
				  const GValue *value,
				  GParamSpec   *pspec)
{
	EvPreviewerWindow *window = EV_PREVIEWER_WINDOW (object);

	switch (prop_id) {
	case PROP_MODEL:
		window->model = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static gboolean
_gtk_css_provider_load_from_resource (GtkCssProvider *provider,
                                      const char     *resource_path,
                                      GError        **error)
{
        GBytes  *data;
        gboolean retval;

        data = g_resources_lookup_data (resource_path, 0, error);
        if (!data)
                return FALSE;

        retval = gtk_css_provider_load_from_data (provider,
                                                  g_bytes_get_data (data, NULL),
                                                  g_bytes_get_size (data),
                                                  error);
        g_bytes_unref (data);

        return retval;
}

static GObject *
ev_previewer_window_constructor (GType                  type,
				 guint                  n_construct_properties,
				 GObjectConstructParam *construct_params)
{
	GObject           *object;
	EvPreviewerWindow *window;
	GtkWidget         *vbox;
	GtkWidget         *swindow;
	GError            *error = NULL;
	gdouble            dpi;
        GtkCssProvider    *css_provider;

	object = G_OBJECT_CLASS (ev_previewer_window_parent_class)->constructor (type,
										 n_construct_properties,
										 construct_params);
	window = EV_PREVIEWER_WINDOW (object);

	dpi = ev_document_misc_get_widget_dpi (GTK_WIDGET (window));
	ev_document_model_set_min_scale (window->model, MIN_SCALE * dpi / 72.0);
	ev_document_model_set_max_scale (window->model, MAX_SCALE * dpi / 72.0);
	ev_document_model_set_sizing_mode (window->model, EV_SIZING_AUTOMATIC);
	g_signal_connect_swapped (window->model, "notify::document",
				  G_CALLBACK (ev_previewer_window_set_document),
				  window);

        css_provider = gtk_css_provider_new ();
        _gtk_css_provider_load_from_resource (css_provider,
                                              "/org/gnome/evince/previewer/ui/evince-previewer.css",
                                              &error);
        g_assert_no_error (error);
        gtk_style_context_add_provider_for_screen (gtk_widget_get_screen (GTK_WIDGET (window)),
                                                   GTK_STYLE_PROVIDER (css_provider),
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref (css_provider);

	view_sizing_mode_changed (window->model, NULL, window);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	window->toolbar = ev_previewer_toolbar_new (window);
	gtk_widget_set_no_show_all (window->toolbar, TRUE);
	gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (window->toolbar),
                                              TRUE);
	gtk_window_set_titlebar (GTK_WINDOW (window), window->toolbar);
	gtk_widget_show (window->toolbar);

	g_signal_connect (ev_previewer_toolbar_get_page_selector (EV_PREVIEWER_TOOLBAR (window->toolbar)),
			  "activate-link",
			  G_CALLBACK (ev_previewer_window_action_page_activated),
			  window);

	swindow = gtk_scrolled_window_new (NULL, NULL);

	window->view = EV_VIEW (ev_view_new ());
	ev_view_set_model (window->view, window->model);
	ev_document_model_set_continuous (window->model, FALSE);

	g_signal_connect_object (window->model, "page-changed",
				 G_CALLBACK (model_page_changed),
				 window, 0);

	gtk_container_add (GTK_CONTAINER (swindow), GTK_WIDGET (window->view));
	gtk_widget_show (GTK_WIDGET (window->view));

	gtk_box_pack_start (GTK_BOX (vbox), swindow, TRUE, TRUE, 0);
	gtk_widget_show (swindow);

	gtk_container_add (GTK_CONTAINER (window), vbox);
	gtk_widget_show (vbox);

	return object;
}


static void
ev_previewer_window_class_init (EvPreviewerWindowClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->constructor = ev_previewer_window_constructor;
	gobject_class->set_property = ev_previewer_window_set_property;
	gobject_class->dispose = ev_previewer_window_dispose;

	g_object_class_install_property (gobject_class,
					 PROP_MODEL,
					 g_param_spec_object ("model",
							      "Model",
							      "The document model",
							      EV_TYPE_DOCUMENT_MODEL,
							      G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY));
}

/* Public methods */
EvPreviewerWindow *
ev_previewer_window_new (EvDocumentModel *model)
{
	return g_object_new (EV_TYPE_PREVIEWER_WINDOW, 
                             "application", g_application_get_default (),
                             "model", model,
                             NULL);
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

EvDocumentModel *
ev_previewer_window_get_document_model (EvPreviewerWindow *window)
{
	g_return_val_if_fail (EV_PREVIEWER_WINDOW (window), NULL);

	return window->model;
}
