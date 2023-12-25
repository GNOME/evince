/* ev-previewer-window.c:
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2009 Carlos Garcia Campos <carlosgc@gnome.org>
 * Copyright (C) 2018 Germán Poo-Caamaño <gpoo@gnome.org>
 * Copyright © 2018, 2021 Christian Persch
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

#include <fcntl.h>

#if GTKUNIXPRINT_ENABLED
#include <gtk/gtkunixprint.h>
#endif
#include <glib/gi18n.h>
#include <adwaita.h>
#include <evince-view.h>
#include "ev-page-action-widget.h"

#include "ev-previewer-window.h"

struct _EvPreviewerWindow {
	AdwApplicationWindow base_instance;

	EvJob            *job;
	EvDocumentModel  *model;
	EvDocument       *document;

	EvView           *view;
	GtkWidget        *page_selector;

	/* Printing */
	GtkPrintSettings *print_settings;
	GtkPageSetup     *print_page_setup;
#if GTKUNIXPRINT_ENABLED
	GtkPrinter       *printer;
#endif
	gchar            *print_job_title;
	gchar            *source_file;
	int               source_fd;
};

struct _EvPreviewerWindowClass {
	AdwApplicationWindowClass base_class;
};

#define MIN_SCALE 0.05409
#define MAX_SCALE 4.0

G_DEFINE_TYPE (EvPreviewerWindow, ev_previewer_window, ADW_TYPE_APPLICATION_WINDOW)

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
}

static void
ev_previewer_window_close (GSimpleAction *action,
			   GVariant      *parameter,
                           gpointer       user_data)
{
	gtk_window_destroy (GTK_WINDOW (user_data));
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
ev_previewer_window_action_page_activated (GObject           *object,
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

	gtk_widget_grab_focus (window->page_selector);
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
	gtk_window_destroy (GTK_WINDOW (window));
}

static void
ev_previewer_window_do_print (EvPreviewerWindow *window)
{
	GtkPrintJob *job;
	gboolean     rv = FALSE;
	GError      *error = NULL;

	job = gtk_print_job_new (window->print_job_title ?
				 window->print_job_title :
				 (window->source_file ? window->source_file : _("Evince Document Viewer")),
				 window->printer,
				 window->print_settings,
				 window->print_page_setup);

        if (window->source_fd != -1)
                rv = gtk_print_job_set_source_fd (job, window->source_fd, &error);
        else if (window->source_file != NULL)
                rv = gtk_print_job_set_source_file (job, window->source_file, &error);
        else
                g_set_error_literal (&error, GTK_PRINT_ERROR, GTK_PRINT_ERROR_GENERAL,
                                     "Neither file nor FD to print.");

        if (rv) {
		gtk_print_job_send (job,
				    (GtkPrintJobCompleteFunc)ev_previewer_window_print_finished,
				    window, NULL);
	} else {
		ev_previewer_window_error_dialog_run (window, error);
		g_error_free (error);
	}

	gtk_widget_set_visible (GTK_WIDGET (window), FALSE);
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

#endif /* GTKUNIXPRINT_ENABLED */

static const GActionEntry actions[] = {
#if GTKUNIXPRINT_ENABLED
	{ "print", ev_previewer_window_print },
#endif
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
load_job_finished_cb (EvJob             *job,
                      EvPreviewerWindow *window)
{
        g_assert (job == window->job);

        if (ev_job_is_failed (job)) {
                ev_previewer_window_error_dialog_run (window, job->error);

		g_clear_object (&window->job);

                return;
        }

        window->document = g_object_ref (job->document);

	g_clear_object (&window->job);

        ev_document_model_set_document (window->model, window->document);
        g_signal_connect (window->model, "notify::sizing-mode",
                          G_CALLBACK (view_sizing_mode_changed),
                          window);
}

static void
ev_previewer_window_dispose (GObject *object)
{
	EvPreviewerWindow *window = EV_PREVIEWER_WINDOW (object);

	g_clear_object (&window->job);
	g_clear_object (&window->model);
	g_clear_object (&window->document);
	g_clear_object (&window->print_settings);
	g_clear_object (&window->print_page_setup);
#if GTKUNIXPRINT_ENABLED
	g_clear_object (&window->printer);
#endif
	g_clear_pointer (&window->print_job_title, g_free);
	g_clear_pointer (&window->source_file, g_free);

        if (window->source_fd != -1) {
                close (window->source_fd);
                window->source_fd = -1;
        }

	G_OBJECT_CLASS (ev_previewer_window_parent_class)->dispose (object);
}

static void
ev_previewer_window_init (EvPreviewerWindow *window)
{
	window->source_fd = -1;

	gtk_widget_init_template (GTK_WIDGET (window));

	g_action_map_add_action_entries (G_ACTION_MAP (window),
					 actions, G_N_ELEMENTS (actions),
					 window);
}

static void
ev_previewer_window_constructed (GObject *object)
{
	EvPreviewerWindow *window = EV_PREVIEWER_WINDOW (object);
	gdouble            dpi;

	G_OBJECT_CLASS (ev_previewer_window_parent_class)->constructed (object);

	window->model = ev_document_model_new ();
	ev_document_model_set_continuous (window->model, FALSE);

	ev_view_set_model (window->view, window->model);
	ev_page_action_widget_set_model (EV_PAGE_ACTION_WIDGET (window->page_selector),
			window->model);

	dpi = ev_document_misc_get_widget_dpi (GTK_WIDGET (window));
	ev_document_model_set_min_scale (window->model, MIN_SCALE * dpi / 72.0);
	ev_document_model_set_max_scale (window->model, MAX_SCALE * dpi / 72.0);
	ev_document_model_set_sizing_mode (window->model, EV_SIZING_AUTOMATIC);

	view_sizing_mode_changed (window->model, NULL, window);

	g_signal_connect (window->page_selector,
			  "activate-link",
			  G_CALLBACK (ev_previewer_window_action_page_activated),
			  window);

	g_signal_connect_object (window->model, "page-changed",
				 G_CALLBACK (model_page_changed),
				 window, 0);
}

static void
ev_previewer_window_class_init (EvPreviewerWindowClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	gobject_class->constructed = ev_previewer_window_constructed;
	gobject_class->dispose = ev_previewer_window_dispose;

	g_type_ensure (EV_TYPE_PAGE_ACTION_WIDGET);
	gtk_widget_class_set_template_from_resource (widget_class,
			"/org/gnome/evince/previewer/ui/previewer-window.ui");

	gtk_widget_class_bind_template_child (widget_class, EvPreviewerWindow, view);
	gtk_widget_class_bind_template_child (widget_class, EvPreviewerWindow, page_selector);
}

/* Public methods */
EvPreviewerWindow *
ev_previewer_window_new (void)
{
	return g_object_new (EV_TYPE_PREVIEWER_WINDOW,
                             "application", g_application_get_default (),
                             NULL);
}

void
ev_previewer_window_set_job (EvPreviewerWindow *window,
                             EvJob             *job)
{
        g_return_if_fail (EV_IS_PREVIEWER_WINDOW (window));
        g_return_if_fail (EV_IS_JOB (job));

        g_clear_object (&window->job);
        window->job = g_object_ref (job);

        g_signal_connect_object (window->job, "finished",
                                 G_CALLBACK (load_job_finished_cb),
                                 window, 0);
        ev_job_scheduler_push_job (window->job, EV_JOB_PRIORITY_NONE);
}

static gboolean
ev_previewer_window_set_print_settings_take_file (EvPreviewerWindow *window,
                                                  GMappedFile       *file,
                                                  GError           **error)
{
        GBytes           *bytes;
        GKeyFile         *key_file;
        GtkPrintSettings *psettings;
        GtkPageSetup     *psetup;
        char             *job_name;
        gboolean          rv;

        g_clear_object (&window->print_settings);
        g_clear_object (&window->print_page_setup);
        g_clear_pointer (&window->print_job_title, g_free);

        bytes = g_mapped_file_get_bytes (file);
        key_file = g_key_file_new ();
        rv = g_key_file_load_from_bytes (key_file, bytes, G_KEY_FILE_NONE, error);
        g_bytes_unref (bytes);
        g_mapped_file_unref (file);

        if (!rv) {
                window->print_settings = gtk_print_settings_new ();
                window->print_page_setup = gtk_page_setup_new ();
                window->print_job_title = g_strdup (_("Evince"));
                return FALSE;
        }

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
        if (job_name && job_name[0]) {
                window->print_job_title = job_name;
                gtk_window_set_title (GTK_WINDOW (window), job_name);
        } else {
                g_free (job_name);
                window->print_job_title = g_strdup (_("Evince Document Viewer"));
        }

        g_key_file_free (key_file);

        return TRUE;
}

gboolean
ev_previewer_window_set_print_settings (EvPreviewerWindow *window,
					const gchar       *print_settings,
                                        GError           **error)
{
        GMappedFile *file;

        g_return_val_if_fail (EV_IS_PREVIEWER_WINDOW (window), FALSE);
        g_return_val_if_fail (print_settings != NULL, FALSE);
        g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

        file = g_mapped_file_new (print_settings, FALSE, error);
        if (file == NULL)
                return FALSE;

        return ev_previewer_window_set_print_settings_take_file (window, file, error);
}

/**
 * ev_previewer_window_set_print_settings_fd:
 * @window:
 * @fd:
 * @error:
 *
 * Sets the print settings from FD.
 *
 * Note that this function takes ownership of @fd.
 */
gboolean
ev_previewer_window_set_print_settings_fd (EvPreviewerWindow *window,
                                           int                fd,
                                           GError           **error)
{
        GMappedFile *file;

        g_return_val_if_fail (EV_IS_PREVIEWER_WINDOW (window), FALSE);
        g_return_val_if_fail (fd != -1, FALSE);
        g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

        file = g_mapped_file_new_from_fd (fd, FALSE, error);
        if (file == NULL)
                return FALSE;

        return ev_previewer_window_set_print_settings_take_file (window, file, error);
}

void
ev_previewer_window_set_source_file (EvPreviewerWindow *window,
				     const gchar       *source_file)
{
        g_return_if_fail (EV_IS_PREVIEWER_WINDOW (window));

        g_free (window->source_file);
	window->source_file = g_strdup (source_file);
}

EvDocumentModel *
ev_previewer_window_get_document_model (EvPreviewerWindow *window)
{
        g_return_val_if_fail (EV_IS_PREVIEWER_WINDOW (window), NULL);

        return window->model;
}

/**
 * ev_previewer_window_set_source_fd:
 * @window:
 * @fd:
 *
 * Sets the source document FD.
 *
 * Note that this function takes ownership of @fd.
 *
 * Returns: %TRUE on success
 */
gboolean
ev_previewer_window_set_source_fd (EvPreviewerWindow *window,
                                   int                fd,
                                   GError           **error)
{
        int nfd;

        g_return_val_if_fail (EV_IS_PREVIEWER_WINDOW (window), FALSE);
        g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

        nfd = fcntl (fd, F_DUPFD_CLOEXEC, 3);
        if (nfd == -1) {
                int errsv = errno;
                g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errsv),
                             "Failed to duplicate file descriptor: %s",
                             g_strerror (errsv));
                return FALSE;
        }

        if (window->source_fd != -1)
                close (window->source_fd);

        window->source_fd = fd;

        return TRUE;
}
