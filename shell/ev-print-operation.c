/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2008 Carlos Garcia Campos <carlosgc@gnome.org>
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

#include "ev-print-operation.h"

#if GTK_CHECK_VERSION (2, 14, 0)
#include <gtk/gtkunixprint.h>
#else
#include <gtk/gtkprintunixdialog.h>
#endif

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <unistd.h>

#include "ev-page-cache.h"
#include "ev-file-exporter.h"
#include "ev-jobs.h"
#include "ev-job-scheduler.h"
#include "ev-application.h"
#include "ev-file-helpers.h"

enum {
	PROP_0,
	PROP_DOCUMENT
};

enum {
	DONE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _EvPrintOperation {
	GObject parent;

	EvDocument *document;
};

struct _EvPrintOperationClass {
	GObjectClass parent_class;

	void              (* set_current_page)       (EvPrintOperation       *op,
						      gint                    current_page);
	void              (* set_print_settings)     (EvPrintOperation       *op,
						      GtkPrintSettings       *print_settings);
	GtkPrintSettings *(* get_print_settings)     (EvPrintOperation       *op);
	void              (* set_default_page_setup) (EvPrintOperation       *op,
						      GtkPageSetup           *page_setup);
	GtkPageSetup     *(* get_default_page_setup) (EvPrintOperation       *op);
	void              (* set_job_name)           (EvPrintOperation       *op,
						      const gchar            *job_name);
	void              (* run)                    (EvPrintOperation       *op,
						      GtkWindow              *parent);
	void              (* cancel)                 (EvPrintOperation       *op);
	void              (* get_error)              (EvPrintOperation       *op,
						      GError                **error);

	/* signals */
	void              (* done)                   (EvPrintOperation       *op,
						      GtkPrintOperationResult result);
};

G_DEFINE_ABSTRACT_TYPE (EvPrintOperation, ev_print_operation, G_TYPE_OBJECT)

static void
ev_print_operation_finalize (GObject *object)
{
	EvPrintOperation *op = EV_PRINT_OPERATION (object);

	if (op->document) {
		g_object_unref (op->document);
		op->document = NULL;
	}

	(* G_OBJECT_CLASS (ev_print_operation_parent_class)->finalize) (object);
}

static void
ev_print_operation_set_property (GObject      *object,
				 guint         prop_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
	EvPrintOperation *op = EV_PRINT_OPERATION (object);

	switch (prop_id) {
	case PROP_DOCUMENT:
		op->document = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
ev_print_operation_init (EvPrintOperation *op)
{
}

static void
ev_print_operation_class_init (EvPrintOperationClass *klass)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

	g_object_class->set_property = ev_print_operation_set_property;
	g_object_class->finalize = ev_print_operation_finalize;

	g_object_class_install_property (g_object_class,
					 PROP_DOCUMENT,
					 g_param_spec_object ("document",
							      "Document",
							      "The document to print",
							      EV_TYPE_DOCUMENT,
							      G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY));
	signals[DONE] =
		g_signal_new ("done",
			      G_TYPE_FROM_CLASS (g_object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EvPrintOperationClass, done),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__ENUM,
			      G_TYPE_NONE, 1,
			      GTK_TYPE_PRINT_OPERATION_RESULT);
	
}

/* Public methods */
void
ev_print_operation_set_current_page (EvPrintOperation *op,
				     gint              current_page)
{
	EvPrintOperationClass *class = EV_PRINT_OPERATION_GET_CLASS (op);

	g_return_if_fail (EV_IS_PRINT_OPERATION (op));
	g_return_if_fail (current_page >= 0);

	class->set_current_page (op, current_page);
}

void
ev_print_operation_set_print_settings (EvPrintOperation *op,
				       GtkPrintSettings *print_settings)
{
	EvPrintOperationClass *class = EV_PRINT_OPERATION_GET_CLASS (op);

	g_return_if_fail (EV_IS_PRINT_OPERATION (op));
	g_return_if_fail (GTK_IS_PRINT_SETTINGS (print_settings));

	class->set_print_settings (op, print_settings);
}

GtkPrintSettings *
ev_print_operation_get_print_settings (EvPrintOperation *op)
{
	EvPrintOperationClass *class = EV_PRINT_OPERATION_GET_CLASS (op);

	g_return_val_if_fail (EV_IS_PRINT_OPERATION (op), NULL);

	return class->get_print_settings (op);
}

void
ev_print_operation_set_default_page_setup (EvPrintOperation *op,
					   GtkPageSetup     *page_setup)
{
	EvPrintOperationClass *class = EV_PRINT_OPERATION_GET_CLASS (op);

	g_return_if_fail (EV_IS_PRINT_OPERATION (op));
	g_return_if_fail (GTK_IS_PAGE_SETUP (page_setup));

	class->set_default_page_setup (op, page_setup);
}

GtkPageSetup *
ev_print_operation_get_default_page_setup (EvPrintOperation *op)
{
	EvPrintOperationClass *class = EV_PRINT_OPERATION_GET_CLASS (op);

	g_return_val_if_fail (EV_IS_PRINT_OPERATION (op), NULL);

	return class->get_default_page_setup (op);
}

void
ev_print_operation_set_job_name (EvPrintOperation *op,
				 const gchar      *job_name)
{
	EvPrintOperationClass *class = EV_PRINT_OPERATION_GET_CLASS (op);

	g_return_if_fail (EV_IS_PRINT_OPERATION (op));
	g_return_if_fail (job_name != NULL);

	class->set_job_name (op, job_name);
}

void
ev_print_operation_run (EvPrintOperation *op,
			GtkWindow        *parent)
{
	EvPrintOperationClass *class = EV_PRINT_OPERATION_GET_CLASS (op);

	g_return_if_fail (EV_IS_PRINT_OPERATION (op));

	class->run (op, parent);
}

void
ev_print_operation_cancel (EvPrintOperation *op)
{
	EvPrintOperationClass *class = EV_PRINT_OPERATION_GET_CLASS (op);

	g_return_if_fail (EV_IS_PRINT_OPERATION (op));

	class->cancel (op);
}

void
ev_print_operation_get_error (EvPrintOperation *op,
			      GError          **error)
{
	EvPrintOperationClass *class = EV_PRINT_OPERATION_GET_CLASS (op);

	g_return_if_fail (EV_IS_PRINT_OPERATION (op));

	class->get_error (op, error);
}

/* Export interface */
#define EV_TYPE_PRINT_OPERATION_EXPORT         (ev_print_operation_export_get_type())
#define EV_PRINT_OPERATION_EXPORT(object)      (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_PRINT_OPERATION_EXPORT, EvPrintOperationExport))
#define EV_PRINT_OPERATION_EXPORT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_PRINT_OPERATION_EXPORT, EvPrintOperationExportClass))
#define EV_IS_PRINT_OPERATION_EXPORT(object)   (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_PRINT_OPERATION_EXPORT))

typedef struct _EvPrintOperationExport      EvPrintOperationExport;
typedef struct _EvPrintOperationExportClass EvPrintOperationExportClass;

GType           ev_print_operation_export_get_type (void) G_GNUC_CONST;

static gboolean export_print_page                  (EvPrintOperationExport *export);

struct _EvPrintOperationExport {
	EvPrintOperation parent;

	GtkWindow *parent_window;
	EvJob *job_export;
	GError *error;

	gboolean print_preview;
	gint n_pages;
	gint current_page;
	GtkPrinter *printer;
	GtkPageSetup *page_setup;
	GtkPrintSettings *print_settings;
	GtkPageSet page_set;
	gint copies;
	guint collate     : 1;
	guint reverse     : 1;
	gint pages_per_sheet;
	gint fd;
	gchar *temp_file;
	gchar *job_name;
	
	guint idle_id;
	
	/* Context */
	gint uncollated_copies;
	gint collated_copies;
	gint uncollated, collated, total;

	gint range, n_ranges;
	GtkPageRange *ranges;
	GtkPageRange one_range;

	gint page, start, end, inc;
};

struct _EvPrintOperationExportClass {
	EvPrintOperationClass parent_class;
};

G_DEFINE_TYPE (EvPrintOperationExport, ev_print_operation_export, EV_TYPE_PRINT_OPERATION)

static void
ev_print_operation_export_set_current_page (EvPrintOperation *op,
					    gint              current_page)
{
	EvPrintOperationExport *export = EV_PRINT_OPERATION_EXPORT (op);

	g_return_if_fail (current_page < export->n_pages);
	
	export->current_page = current_page;
}

static void
ev_print_operation_export_set_print_settings (EvPrintOperation *op,
					      GtkPrintSettings *print_settings)
{
	EvPrintOperationExport *export = EV_PRINT_OPERATION_EXPORT (op);

	if (print_settings == export->print_settings)
		return;

	g_object_ref (print_settings);
	if (export->print_settings)
		g_object_unref (export->print_settings);
	export->print_settings = print_settings;
}

static GtkPrintSettings *
ev_print_operation_export_get_print_settings (EvPrintOperation *op)
{
	EvPrintOperationExport *export = EV_PRINT_OPERATION_EXPORT (op);

	return export->print_settings;
}

static void
ev_print_operation_export_set_default_page_setup (EvPrintOperation *op,
						  GtkPageSetup     *page_setup)
{
	EvPrintOperationExport *export = EV_PRINT_OPERATION_EXPORT (op);

	if (page_setup == export->page_setup)
		return;

	g_object_ref (page_setup);
	if (export->page_setup)
		g_object_unref (export->page_setup);
	export->page_setup = page_setup;
}

static GtkPageSetup *
ev_print_operation_export_get_default_page_setup (EvPrintOperation *op)
{
	EvPrintOperationExport *export = EV_PRINT_OPERATION_EXPORT (op);

	return export->page_setup;
}

static void
ev_print_operation_export_set_job_name (EvPrintOperation *op,
					const gchar      *job_name)
{
	EvPrintOperationExport *export = EV_PRINT_OPERATION_EXPORT (op);

	g_free (export->job_name);
	export->job_name = g_strdup (job_name);
}

static void
ev_print_operation_export_set_printer (EvPrintOperationExport *export,
				       GtkPrinter             *printer)
{
	if (printer == export->printer)
		return;

	g_object_ref (printer);
	if (export->printer)
		g_object_unref (export->printer);
	export->printer = printer;
}

static void
find_range (EvPrintOperationExport *export)
{
	GtkPageRange *range;

	range = &export->ranges[export->range];

	if (export->inc < 0) {
		export->start = range->end;
		export->end = range->start - 1;
	} else {
		export->start = range->start;
		export->end = range->end + 1;
	}
}

static void
clamp_ranges (EvPrintOperationExport *export)
{
	gint num_of_correct_ranges = 0;
	gint i;

	for (i = 0; i < export->n_ranges; i++) {
		if ((export->ranges[i].start >= 0) &&
		    (export->ranges[i].start < export->n_pages) &&
		    (export->ranges[i].end >= 0) &&
		    (export->ranges[i].end < export->n_pages)) {
			export->ranges[num_of_correct_ranges] = export->ranges[i];
			num_of_correct_ranges++;
		} else if ((export->ranges[i].start >= 0) &&
			   (export->ranges[i].start < export->n_pages) &&
			   (export->ranges[i].end >= export->n_pages)) {
			export->ranges[i].end = export->n_pages - 1;
			export->ranges[num_of_correct_ranges] = export->ranges[i];
			num_of_correct_ranges++;
		} else if ((export->ranges[i].end >= 0) &&
			   (export->ranges[i].end < export->n_pages) &&
			   (export->ranges[i].start < 0)) {
			export->ranges[i].start = 0;
			export->ranges[num_of_correct_ranges] = export->ranges[i];
			num_of_correct_ranges++;
		}
	}

	export->n_ranges = num_of_correct_ranges;
}

static gint
get_first_page (EvPrintOperationExport *export)
{
	gint i;
	gint first_page = G_MAXINT;

	if (export->n_ranges == 0)
		return 0;

	for (i = 0; i < export->n_ranges; i++) {
		if (export->ranges[i].start < first_page)
			first_page = export->ranges[i].start;
	}

	return MAX (0, first_page);
}

static gint
get_last_page (EvPrintOperationExport *export)
{
	gint i;
	gint last_page = G_MININT;
	gint max_page = export->n_pages - 1;

	if (export->n_ranges == 0)
		return max_page;

	for (i = 0; i < export->n_ranges; i++) {
		if (export->ranges[i].end > last_page)
			last_page = export->ranges[i].end;
	}

	return MIN (max_page, last_page);
}

static gboolean
export_print_inc_page (EvPrintOperationExport *export)
{
	do {
		export->page += export->inc;
		if (export->page == export->end) {
			export->range += export->inc;
			if (export->range == -1 || export->range == export->n_ranges) {
				export->uncollated++;
				if (export->uncollated == export->uncollated_copies)
					return FALSE;

				export->range = export->inc < 0 ? export->n_ranges - 1 : 0;
			}
			find_range (export);
			export->page = export->start;
		}
	} while ((export->page_set == GTK_PAGE_SET_EVEN && export->page % 2 == 0) ||
		 (export->page_set == GTK_PAGE_SET_ODD && export->page % 2 == 1));

	return TRUE;
}

static void
ev_print_operation_export_clear_temp_file (EvPrintOperationExport *export)
{
	if (!export->temp_file)
		return;

	g_unlink (export->temp_file);
	g_free (export->temp_file);
	export->temp_file = NULL;
}

static void
print_job_finished (GtkPrintJob            *print_job,
		    EvPrintOperationExport *export,
		    GError                 *error)
{
	EvPrintOperation *op = EV_PRINT_OPERATION (export);
	
	if (error) {
		g_set_error_literal (&export->error,
				     GTK_PRINT_ERROR,
				     GTK_PRINT_ERROR_GENERAL,
				     error->message);
		g_signal_emit (op, signals[DONE], 0, GTK_PRINT_OPERATION_RESULT_ERROR);
	} else {
		g_signal_emit (op, signals[DONE], 0, GTK_PRINT_OPERATION_RESULT_APPLY);
	}

	ev_print_operation_export_clear_temp_file (export);
	g_object_unref (print_job);
}

static void
export_print_done (EvPrintOperationExport *export)
{
	EvPrintOperation *op = EV_PRINT_OPERATION (export);
	GtkPrintSettings *settings;
	EvFileExporterCapabilities capabilities;

	/* Some printers take into account some print settings,
	 * and others don't. However we have exported the document
	 * to a ps or pdf file according to such print settings. So,
	 * we want to send the exported file to printer with those
	 * settings set to default values.
	 */
	settings = gtk_print_settings_copy (export->print_settings);
	capabilities = ev_file_exporter_get_capabilities (EV_FILE_EXPORTER (op->document));

	gtk_print_settings_set_page_ranges (settings, NULL, 0);
	gtk_print_settings_set_print_pages (settings, GTK_PRINT_PAGES_ALL);
	if (capabilities & EV_FILE_EXPORTER_CAN_COPIES)
		gtk_print_settings_set_n_copies (settings, 1);
	if (capabilities & EV_FILE_EXPORTER_CAN_PAGE_SET)
		gtk_print_settings_set_page_set (settings, GTK_PAGE_SET_ALL);
	if (capabilities & EV_FILE_EXPORTER_CAN_SCALE)
		gtk_print_settings_set_scale (settings, 1.0);
	if (capabilities & EV_FILE_EXPORTER_CAN_COLLATE)
		gtk_print_settings_set_collate (settings, FALSE);
	if (capabilities & EV_FILE_EXPORTER_CAN_REVERSE)
		gtk_print_settings_set_reverse (settings, FALSE);
	if (capabilities & EV_FILE_EXPORTER_CAN_NUMBER_UP) {
		gtk_print_settings_set_number_up (settings, 1);
		gtk_print_settings_set_int (settings, "cups-"GTK_PRINT_SETTINGS_NUMBER_UP, 1);
	}

	if (export->print_preview) {
		gchar *uri;
		gchar *print_settings_file = NULL;

		print_settings_file = ev_tmp_filename ("print-settings");
		gtk_print_settings_to_file (settings, print_settings_file, NULL);

		uri = g_filename_to_uri (export->temp_file, NULL, NULL);
		ev_application_open_uri_at_dest (EV_APP,
						 uri,
						 gtk_window_get_screen (export->parent_window),
						 NULL,
						 EV_WINDOW_MODE_PREVIEW,
						 NULL,
						 TRUE,
						 print_settings_file,
						 GDK_CURRENT_TIME);
		g_free (print_settings_file);
		g_free (uri);

		g_signal_emit (op, signals[DONE], 0, GTK_PRINT_OPERATION_RESULT_APPLY);
		/* temp_file will be deleted by the previewer */
	} else {
		GtkPrintJob *job;
		GError      *error = NULL;
		
		job = gtk_print_job_new (export->job_name,
					 export->printer,
					 settings,
					 export->page_setup);
		gtk_print_job_set_source_file (job, export->temp_file, &error);
		if (error) {
			g_set_error_literal (&export->error,
					     GTK_PRINT_ERROR,
					     GTK_PRINT_ERROR_GENERAL,
					     error->message);
			g_error_free (error);
			ev_print_operation_export_clear_temp_file (export);
			g_signal_emit (op, signals[DONE], 0, GTK_PRINT_OPERATION_RESULT_ERROR);
		} else {
			gtk_print_job_send (job,
					    (GtkPrintJobCompleteFunc)print_job_finished,
					    g_object_ref (export),
					    (GDestroyNotify)g_object_unref);
		}
	}
	g_object_unref (settings);
}

static void
export_print_page_idle_finished (EvPrintOperationExport *export)
{
	export->idle_id = 0;
}

static void
export_job_finished (EvJobExport            *job,
		     EvPrintOperationExport *export)
{
	EvPrintOperation *op = EV_PRINT_OPERATION (export);

	if (export->pages_per_sheet == 1 || export->total % export->pages_per_sheet == 0) {
		ev_document_doc_mutex_lock ();
		ev_file_exporter_end_page (EV_FILE_EXPORTER (op->document));
		ev_document_doc_mutex_unlock ();
	}

	/* Reschedule */
	export->idle_id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
					   (GSourceFunc)export_print_page,
					   export,
					   (GDestroyNotify)export_print_page_idle_finished);
}

static void
export_job_cancelled (EvJobExport            *job,
		      EvPrintOperationExport *export)
{
	EvPrintOperation *op = EV_PRINT_OPERATION (export);

	if (export->idle_id > 0)
		g_source_remove (export->idle_id);
	export->idle_id = 0;

	g_signal_handlers_disconnect_by_func (export->job_export,
					      export_job_finished,
					      export);
	g_signal_handlers_disconnect_by_func (export->job_export,
					      export_job_cancelled,
					      export);
	g_object_unref (export->job_export);
	export->job_export = NULL;

	if (export->fd != -1) {
		close (export->fd);
		export->fd = -1;
	}

	ev_print_operation_export_clear_temp_file (export);

	g_signal_emit (op, signals[DONE], 0, GTK_PRINT_OPERATION_RESULT_CANCEL);
}

static gboolean
export_print_page (EvPrintOperationExport *export)
{
	EvPrintOperation *op = EV_PRINT_OPERATION (export);
	
	export->total++;
	export->collated++;

	if (export->collated == export->collated_copies) {
		export->collated = 0;
		if (!export_print_inc_page (export)) {
			ev_document_doc_mutex_lock ();
			if (export->pages_per_sheet > 1 &&
			    export->total - 1 % export->pages_per_sheet == 0)
				ev_file_exporter_end_page (EV_FILE_EXPORTER (op->document));
			ev_file_exporter_end (EV_FILE_EXPORTER (op->document));
			ev_document_doc_mutex_unlock ();

			close (export->fd);
			export->fd = -1;

			export_print_done (export);
			
			return FALSE;
		}
	}

	if (export->pages_per_sheet == 1 || export->total % export->pages_per_sheet == 1) {
		ev_document_doc_mutex_lock ();
		ev_file_exporter_begin_page (EV_FILE_EXPORTER (op->document));
		ev_document_doc_mutex_unlock ();
	}
	
	if (!export->job_export) {
		export->job_export = ev_job_export_new (op->document);
		g_signal_connect (G_OBJECT (export->job_export), "finished",
				  G_CALLBACK (export_job_finished),
				  (gpointer)export);
		g_signal_connect (G_OBJECT (export->job_export), "cancelled",
				  G_CALLBACK (export_job_cancelled),
				  (gpointer)export);
	}

	ev_job_export_set_page (EV_JOB_EXPORT (export->job_export), export->page);
	ev_job_scheduler_push_job (export->job_export, EV_JOB_PRIORITY_NONE);
	
	return FALSE;
}

static gboolean
ev_print_operation_export_print_dialog_response_cb (GtkDialog              *dialog,
						    gint                    response,
						    EvPrintOperationExport *export)
{
	GtkPrintPages         print_pages;
	GtkPrintSettings     *print_settings;
	GtkPageSetup         *page_setup;
	GtkPrinter           *printer;
	gdouble               scale;
	gdouble               width;
	gdouble               height;
	gint                  first_page;
	gint                  last_page;
	const gchar          *file_format;
	gchar                *filename;
	EvFileExporterContext fc;
	GError               *error = NULL;
	EvPrintOperation     *op = EV_PRINT_OPERATION (export);
	
	if (response != GTK_RESPONSE_OK &&
	    response != GTK_RESPONSE_APPLY) {
		gtk_widget_destroy (GTK_WIDGET (dialog));

		return FALSE;
	}

	export->print_preview = (response == GTK_RESPONSE_APPLY);
	
	printer = gtk_print_unix_dialog_get_selected_printer (GTK_PRINT_UNIX_DIALOG (dialog));
	ev_print_operation_export_set_printer (export, printer);

	print_settings = gtk_print_unix_dialog_get_settings (GTK_PRINT_UNIX_DIALOG (dialog));
	ev_print_operation_export_set_print_settings (op, print_settings);

	page_setup = gtk_print_unix_dialog_get_page_setup (GTK_PRINT_UNIX_DIALOG (dialog));
	ev_print_operation_export_set_default_page_setup (op, page_setup);

	if (!gtk_printer_accepts_ps (export->printer)) {
		g_set_error (&export->error,
			     GTK_PRINT_ERROR,
			     GTK_PRINT_ERROR_GENERAL,
			     "%s", _("Printing is not supported on this printer."));
		g_signal_emit (op, signals[DONE], 0, GTK_PRINT_OPERATION_RESULT_ERROR);
		gtk_widget_destroy (GTK_WIDGET (dialog));
		
		return FALSE;
	}

	file_format = gtk_print_settings_get (print_settings, GTK_PRINT_SETTINGS_OUTPUT_FILE_FORMAT);
	
	filename = g_strdup_printf ("evince_print.%s.XXXXXX", file_format);
	export->fd = g_file_open_tmp (filename, &export->temp_file, &error);
	g_free (filename);
	if (export->fd <= -1) {
		g_set_error_literal (&export->error,
				     GTK_PRINT_ERROR,
				     GTK_PRINT_ERROR_GENERAL,
				     error->message);
		g_error_free (error);
		g_signal_emit (op, signals[DONE], 0, GTK_PRINT_OPERATION_RESULT_ERROR);
		gtk_widget_destroy (GTK_WIDGET (dialog));

		return FALSE;
	}

	export->current_page = gtk_print_unix_dialog_get_current_page (GTK_PRINT_UNIX_DIALOG (dialog));
	print_pages = gtk_print_settings_get_print_pages (print_settings);
	
	switch (print_pages) {
	case GTK_PRINT_PAGES_CURRENT:
		export->ranges = &export->one_range;
		
		export->ranges[0].start = export->current_page;
		export->ranges[0].end = export->current_page;
		export->n_ranges = 1;
				
		break;
	case GTK_PRINT_PAGES_RANGES: {
		gint i;
		
		export->ranges = gtk_print_settings_get_page_ranges (print_settings, &export->n_ranges);
		for (i = 0; i < export->n_ranges; i++)
			if (export->ranges[i].end == -1 || export->ranges[i].end >= export->n_pages)
				export->ranges[i].end = export->n_pages - 1;
	}
		break;
	case GTK_PRINT_PAGES_ALL:
		export->ranges = &export->one_range;

		export->ranges[0].start = 0;
		export->ranges[0].end = export->n_pages - 1;
		export->n_ranges = 1;
		
		break;
	}
	clamp_ranges (export);

	export->page_set = gtk_print_settings_get_page_set (print_settings);

	width = gtk_page_setup_get_paper_width (page_setup, GTK_UNIT_POINTS);
	height = gtk_page_setup_get_paper_height (page_setup, GTK_UNIT_POINTS);
	scale = gtk_print_settings_get_scale (print_settings) * 0.01;
	if (scale != 1.0) {
		width *= scale;
		height *= scale;
	}

	export->pages_per_sheet = gtk_print_settings_get_number_up (print_settings);
	
	export->copies = gtk_print_settings_get_n_copies (print_settings);
	export->collate = gtk_print_settings_get_collate (print_settings);
	export->reverse = gtk_print_settings_get_reverse (print_settings);

	if (export->collate) {
		export->uncollated_copies = export->copies;
		export->collated_copies = 1;
	} else {
		export->uncollated_copies = 1;
		export->collated_copies = export->copies;
	}

	if (export->reverse) {
		export->range = export->n_ranges - 1;
		export->inc = -1;
	} else {
		export->range = 0;
		export->inc = 1;
	}
	find_range (export);

	export->page = export->start - export->inc;
	export->collated = export->collated_copies - 1;

	first_page = get_first_page (export);
	last_page = get_last_page (export);

	fc.format = g_ascii_strcasecmp (file_format, "pdf") == 0 ?
		EV_FILE_FORMAT_PDF : EV_FILE_FORMAT_PS;
	fc.filename = export->temp_file;
	fc.first_page = MIN (first_page, last_page);
	fc.last_page = MAX (first_page, last_page);
	fc.paper_width = width;
	fc.paper_height = height;
	fc.duplex = FALSE;
	fc.pages_per_sheet = MAX (1, export->pages_per_sheet);

	ev_document_doc_mutex_lock ();
	ev_file_exporter_begin (EV_FILE_EXPORTER (op->document), &fc);
	ev_document_doc_mutex_unlock ();

	export->idle_id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
					   (GSourceFunc)export_print_page,
					   export,
					   (GDestroyNotify)export_print_page_idle_finished);
	
	gtk_widget_destroy (GTK_WIDGET (dialog));

	return TRUE;
}

static void
ev_print_operation_export_run (EvPrintOperation *op,
			       GtkWindow        *parent)
{
	EvPrintOperationExport *export = EV_PRINT_OPERATION_EXPORT (op);
	GtkWidget              *dialog;
	GtkPrintCapabilities    capabilities;

	export->parent_window = parent;
	export->error = NULL;
	
	dialog = gtk_print_unix_dialog_new (_("Print"), parent);
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	
	capabilities = GTK_PRINT_CAPABILITY_PREVIEW |
		ev_file_exporter_get_capabilities (EV_FILE_EXPORTER (op->document));
	gtk_print_unix_dialog_set_manual_capabilities (GTK_PRINT_UNIX_DIALOG (dialog),
						       capabilities);

	gtk_print_unix_dialog_set_current_page (GTK_PRINT_UNIX_DIALOG (dialog),
						export->current_page);
	
	gtk_print_unix_dialog_set_settings (GTK_PRINT_UNIX_DIALOG (dialog),
					    export->print_settings);
	
	if (export->page_setup)
		gtk_print_unix_dialog_set_page_setup (GTK_PRINT_UNIX_DIALOG (dialog),
						      export->page_setup);
	
	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (ev_print_operation_export_print_dialog_response_cb),
			  export);

	gtk_window_present (GTK_WINDOW (dialog));
}

static void
ev_print_operation_export_cancel (EvPrintOperation *op)
{
	EvPrintOperationExport *export = EV_PRINT_OPERATION_EXPORT (op);

	if (export->job_export) {
		ev_job_cancel (export->job_export);
	}
}

static void
ev_print_operation_export_get_error (EvPrintOperation *op,
				     GError          **error)
{
	EvPrintOperationExport *export = EV_PRINT_OPERATION_EXPORT (op);

	g_propagate_error (error, export->error);
	export->error = NULL;
}

static void
ev_print_operation_export_finalize (GObject *object)
{
	EvPrintOperationExport *export = EV_PRINT_OPERATION_EXPORT (object);

	if (export->idle_id > 0) {
		g_source_remove (export->idle_id);
		export->idle_id = 0;
	}

	if (export->fd != -1) {
		close (export->fd);
		export->fd = -1;
	}
	
	if (export->ranges) {
		if (export->ranges != &export->one_range)
			g_free (export->ranges);
		export->ranges = NULL;
		export->n_ranges = 0;
	}

	if (export->temp_file) {
		g_free (export->temp_file);
		export->temp_file = NULL;
	}

	if (export->job_name) {
		g_free (export->job_name);
		export->job_name = NULL;
	}

	if (export->job_export) {
		if (!ev_job_is_finished (export->job_export))
			ev_job_cancel (export->job_export);
		g_signal_handlers_disconnect_by_func (export->job_export,
						      export_job_finished,
						      export);
		g_signal_handlers_disconnect_by_func (export->job_export,
						      export_job_cancelled,
						      export);
		g_object_unref (export->job_export);
		export->job_export = NULL;
	}

	if (export->error) {
		g_error_free (export->error);
		export->error = NULL;
	}

	if (export->print_settings) {
		g_object_unref (export->print_settings);
		export->print_settings = NULL;
	}

	if (export->page_setup) {
		g_object_unref (export->page_setup);
		export->page_setup = NULL;
	}

	if (export->printer) {
		g_object_unref (export->printer);
		export->printer = NULL;
	}

	(* G_OBJECT_CLASS (ev_print_operation_export_parent_class)->finalize) (object);
}

static void
ev_print_operation_export_init (EvPrintOperationExport *export)
{
}

static GObject *
ev_print_operation_export_constructor (GType                  type,
				       guint                  n_construct_properties,
				       GObjectConstructParam *construct_params)
{
	GObject                *object;
	EvPrintOperationExport *export;
	EvPrintOperation       *op;
	
	object = G_OBJECT_CLASS (ev_print_operation_export_parent_class)->constructor (type,
										       n_construct_properties,
										       construct_params);
	export = EV_PRINT_OPERATION_EXPORT (object);
	op = EV_PRINT_OPERATION (object);
	export->n_pages = ev_page_cache_get_n_pages (ev_page_cache_get (op->document));

	return object;
}

static void
ev_print_operation_export_class_init (EvPrintOperationExportClass *klass)
{
	GObjectClass          *g_object_class = G_OBJECT_CLASS (klass);
	EvPrintOperationClass *ev_print_op_class = EV_PRINT_OPERATION_CLASS (klass);

	ev_print_op_class->set_current_page = ev_print_operation_export_set_current_page;
	ev_print_op_class->set_print_settings = ev_print_operation_export_set_print_settings;
	ev_print_op_class->get_print_settings = ev_print_operation_export_get_print_settings;
	ev_print_op_class->set_default_page_setup = ev_print_operation_export_set_default_page_setup;
	ev_print_op_class->get_default_page_setup = ev_print_operation_export_get_default_page_setup;
	ev_print_op_class->set_job_name = ev_print_operation_export_set_job_name;
	ev_print_op_class->run = ev_print_operation_export_run;
	ev_print_op_class->cancel = ev_print_operation_export_cancel;
	ev_print_op_class->get_error = ev_print_operation_export_get_error;

	g_object_class->constructor = ev_print_operation_export_constructor;
	g_object_class->finalize = ev_print_operation_export_finalize;
}

/* Factory method */
EvPrintOperation *
ev_print_operation_new (EvDocument *document)
{
	/* TODO: EvPrintOperationPrint */

	return EV_PRINT_OPERATION (g_object_new (EV_TYPE_PRINT_OPERATION_EXPORT,
						 "document", document, NULL));
}
