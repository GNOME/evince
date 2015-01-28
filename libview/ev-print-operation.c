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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>

#include "ev-print-operation.h"

#if GTKUNIXPRINT_ENABLED
#include <gtk/gtkunixprint.h>
#endif
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <unistd.h>

#include "ev-jobs.h"
#include "ev-job-scheduler.h"

enum {
	PROP_0,
	PROP_DOCUMENT
};

enum {
	DONE,
	BEGIN_PRINT,
	STATUS_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _EvPrintOperation {
	GObject parent;

	EvDocument *document;

	gboolean    print_preview;

	/* Progress */
	gchar      *status;
	gdouble     progress;
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
	const gchar      *(* get_job_name)           (EvPrintOperation       *op);
	void              (* run)                    (EvPrintOperation       *op,
						      GtkWindow              *parent);
	void              (* cancel)                 (EvPrintOperation       *op);
	void              (* get_error)              (EvPrintOperation       *op,
						      GError                **error);
	void              (* set_embed_page_setup)   (EvPrintOperation       *op,
						      gboolean                embed);
	gboolean          (* get_embed_page_setup)   (EvPrintOperation       *op);

	/* signals */
	void              (* done)                   (EvPrintOperation       *op,
						      GtkPrintOperationResult result);
	void              (* begin_print)            (EvPrintOperation       *op);
	void              (* status_changed)         (EvPrintOperation       *op);
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

	if (op->status) {
		g_free (op->status);
		op->status = NULL;
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
							      G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_STATIC_STRINGS));
	signals[DONE] =
		g_signal_new ("done",
			      G_TYPE_FROM_CLASS (g_object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EvPrintOperationClass, done),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__ENUM,
			      G_TYPE_NONE, 1,
			      GTK_TYPE_PRINT_OPERATION_RESULT);
	signals[BEGIN_PRINT] =
		g_signal_new ("begin_print",
			      G_TYPE_FROM_CLASS (g_object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EvPrintOperationClass, begin_print),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals[STATUS_CHANGED] =
		g_signal_new ("status_changed",
			      G_TYPE_FROM_CLASS (g_object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EvPrintOperationClass, status_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
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

/**
 * ev_print_operation_get_print_settings:
 * @op: an #EvPrintOperation
 *
 * Returns: (transfer none): a #GtkPrintSettings
 */
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

/**
 * ev_print_operation_get_default_page_setup:
 * @op: an #EvPrintOperation
 *
 * Returns: (transfer none): a #GtkPageSetup
 */
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

const gchar *
ev_print_operation_get_job_name (EvPrintOperation *op)
{
	EvPrintOperationClass *class = EV_PRINT_OPERATION_GET_CLASS (op);

	g_return_val_if_fail (EV_IS_PRINT_OPERATION (op), NULL);

	return class->get_job_name (op);
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

void
ev_print_operation_set_embed_page_setup (EvPrintOperation *op,
					 gboolean          embed)
{
	EvPrintOperationClass *class = EV_PRINT_OPERATION_GET_CLASS (op);

	g_return_if_fail (EV_IS_PRINT_OPERATION (op));

	class->set_embed_page_setup (op, embed);
}

gboolean
ev_print_operation_get_embed_page_setup (EvPrintOperation *op)
{
	EvPrintOperationClass *class = EV_PRINT_OPERATION_GET_CLASS (op);

	g_return_val_if_fail (EV_IS_PRINT_OPERATION (op), FALSE);

	return class->get_embed_page_setup (op);
}

const gchar *
ev_print_operation_get_status (EvPrintOperation *op)
{
	g_return_val_if_fail (EV_IS_PRINT_OPERATION (op), NULL);

	return op->status ? op->status : "";
}

gdouble
ev_print_operation_get_progress (EvPrintOperation *op)
{
	g_return_val_if_fail (EV_IS_PRINT_OPERATION (op), 0.0);

	return op->progress;
}

static void
ev_print_operation_update_status (EvPrintOperation *op,
				  gint              page,
				  gint              n_pages,
				  gdouble           progress)
{
	if (op->status && op->progress == progress)
		return;

	g_free (op->status);

	if (op->print_preview) {
		if (page == -1) {
			/* Initial state */
			op->status = g_strdup (_("Preparing preview…"));
		} else if (page > n_pages) {
			op->status = g_strdup (_("Finishing…"));
		} else {
			op->status = g_strdup_printf (_("Generating preview: page %d of %d"),
						      page, n_pages);
		}
 	} else {
		if (page == -1) {
			/* Initial state */
			op->status = g_strdup (_("Preparing to print…"));
		} else if (page > n_pages) {
			op->status = g_strdup (_("Finishing…"));
		} else {
			op->status = g_strdup_printf (_("Printing page %d of %d…"),
						      page, n_pages);
		}
	}

	op->progress = MIN (1.0, progress);

	g_signal_emit (op, signals[STATUS_CHANGED], 0);
}

#if GTKUNIXPRINT_ENABLED

/* Export interface */
#define EV_TYPE_PRINT_OPERATION_EXPORT         (ev_print_operation_export_get_type())
#define EV_PRINT_OPERATION_EXPORT(object)      (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_PRINT_OPERATION_EXPORT, EvPrintOperationExport))
#define EV_PRINT_OPERATION_EXPORT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_PRINT_OPERATION_EXPORT, EvPrintOperationExportClass))
#define EV_IS_PRINT_OPERATION_EXPORT(object)   (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_PRINT_OPERATION_EXPORT))

typedef struct _EvPrintOperationExport      EvPrintOperationExport;
typedef struct _EvPrintOperationExportClass EvPrintOperationExportClass;

static GType    ev_print_operation_export_get_type (void) G_GNUC_CONST;

static void     ev_print_operation_export_begin    (EvPrintOperationExport *export);
static gboolean export_print_page                  (EvPrintOperationExport *export);
static void     export_cancel                      (EvPrintOperationExport *export);

struct _EvPrintOperationExport {
	EvPrintOperation parent;

	GtkWindow *parent_window;
	EvJob *job_export;
	GError *error;

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
	gboolean embed_page_setup;

	guint idle_id;
	
	/* Context */
	EvFileExporterContext fc;
	gint n_pages_to_print;
	gint uncollated_copies;
	gint collated_copies;
	gint uncollated, collated, total;

	gint sheet, page_count;

	gint range, n_ranges;
	GtkPageRange *ranges;
	GtkPageRange one_range;

	gint page, start, end, inc;
};

struct _EvPrintOperationExportClass {
	EvPrintOperationClass parent_class;
};

G_DEFINE_TYPE (EvPrintOperationExport, ev_print_operation_export, EV_TYPE_PRINT_OPERATION)

/* Internal print queue */
static GHashTable *print_queue = NULL;

static void
queue_free (GQueue *queue)
{
	g_queue_foreach (queue, (GFunc)g_object_unref, NULL);
	g_queue_free (queue);
}

static void
ev_print_queue_init (void)
{
	if (G_UNLIKELY (print_queue == NULL)) {
		print_queue = g_hash_table_new_full (g_direct_hash,
						     g_direct_equal,
						     NULL,
						     (GDestroyNotify)queue_free);
	}
}

static void
remove_document_queue (gpointer data,
		       GObject *document)
{
	if (print_queue)
		g_hash_table_remove (print_queue, document);
}

static gboolean
ev_print_queue_is_empty (EvDocument *document)
{
	GQueue *queue;

	queue = g_hash_table_lookup (print_queue, document);
	return (!queue || g_queue_is_empty (queue));
}

static void
ev_print_queue_push (EvPrintOperation *op)
{
	GQueue *queue;

	queue = g_hash_table_lookup (print_queue, op->document);
	if (!queue) {
		queue = g_queue_new ();
		g_hash_table_insert (print_queue,
				     op->document,
				     queue);
		g_object_weak_ref (G_OBJECT (op->document),
				   (GWeakNotify)remove_document_queue,
				   NULL);
	}

	g_queue_push_head (queue, g_object_ref (op));
}

static EvPrintOperation *
ev_print_queue_pop (EvDocument *document)
{
	EvPrintOperation *op;
	GQueue           *queue;

	queue = g_hash_table_lookup (print_queue, document);
	if (!queue || g_queue_is_empty (queue))
		return NULL;
	
	op = g_queue_pop_tail (queue);
	g_object_unref (op);

	return op;
}

static EvPrintOperation *
ev_print_queue_peek (EvDocument *document)
{
	GQueue *queue;

	queue = g_hash_table_lookup (print_queue, document);
	if (!queue || g_queue_is_empty (queue))
		return NULL;

	return g_queue_peek_tail (queue);
}

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

static const gchar *
ev_print_operation_export_get_job_name (EvPrintOperation *op)
{
	EvPrintOperationExport *export = EV_PRINT_OPERATION_EXPORT (op);

	return export->job_name;
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

static gboolean
clamp_ranges (EvPrintOperationExport *export)
{
	gint num_of_correct_ranges = 0;
	gint n_pages_to_print = 0;
	gint i;
	gboolean null_flag = FALSE;

	for (i = 0; i < export->n_ranges; i++) {
		gint n_pages;
		
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
		
		n_pages = export->ranges[i].end - export->ranges[i].start + 1;
		if (export->page_set == GTK_PAGE_SET_ALL) {
			n_pages_to_print += n_pages;
		} else if (n_pages % 2 == 0) {
			n_pages_to_print += n_pages / 2;
		} else if (export->page_set == GTK_PAGE_SET_EVEN) {
			if (n_pages==1 && export->ranges[i].start % 2 == 0)
				null_flag = TRUE;
			else 
				n_pages_to_print += export->ranges[i].start % 2 == 0 ?
				n_pages / 2 : (n_pages / 2) + 1;
		} else if (export->page_set == GTK_PAGE_SET_ODD) {
			if (n_pages==1 && export->ranges[i].start % 2 != 0) 
				null_flag = TRUE;
			else 
				n_pages_to_print += export->ranges[i].start % 2 == 0 ?
				(n_pages / 2) + 1 : n_pages / 2;
		}
	}

	if (null_flag && !n_pages_to_print) {
		return FALSE;
	} else {
		export->n_ranges = num_of_correct_ranges;
		export->n_pages_to_print = n_pages_to_print;
		return TRUE;
	}
}

static void
get_first_and_last_page (EvPrintOperationExport *export,
			 gint                   *first,
			 gint                   *last)
{
	gint i;
	gint first_page = G_MAXINT;
	gint last_page = G_MININT;
	gint max_page = export->n_pages - 1;

	if (export->n_ranges == 0) {
		*first = 0;
		*last = max_page;

		return;
	}

	for (i = 0; i < export->n_ranges; i++) {
		if (export->ranges[i].start < first_page)
			first_page = export->ranges[i].start;
		if (export->ranges[i].end > last_page)
			last_page = export->ranges[i].end;
	}

	*first = MAX (0, first_page);
	*last = MIN (max_page, last_page);
}

static gboolean
export_print_inc_page (EvPrintOperationExport *export)
{
	do {
		export->page += export->inc;

		/* note: when NOT collating, page_count is increased in export_print_page */
		if (export->collate) {
			export->page_count++;
			export->sheet = 1 + (export->page_count - 1) / export->pages_per_sheet;
		}

		if (export->page == export->end) {
			export->range += export->inc;
			if (export->range == -1 || export->range == export->n_ranges) {
				export->uncollated++;

				/* when printing multiple collated copies & multiple pages per sheet we want to
				 * prevent the next copy bleeding into the last sheet of the previous one
				 * we've reached the last range to be printed now, so this is the time to do it */
				if (export->pages_per_sheet > 1 && export->collate == 1 &&
				    (export->page_count - 1) % export->pages_per_sheet != 0) {

					EvPrintOperation *op = EV_PRINT_OPERATION (export);
					ev_document_doc_mutex_lock ();

					/* keep track of all blanks but only actualise those
					 * which are in the current odd / even sheet set */

					export->page_count += export->pages_per_sheet - (export->page_count - 1) % export->pages_per_sheet;
					if (export->page_set == GTK_PAGE_SET_ALL ||
						(export->page_set == GTK_PAGE_SET_EVEN && export->sheet % 2 == 0) ||
						(export->page_set == GTK_PAGE_SET_ODD && export->sheet % 2 == 1) ) {
						ev_file_exporter_end_page (EV_FILE_EXPORTER (op->document));
					}
					ev_document_doc_mutex_unlock ();
					export->sheet = 1 + (export->page_count - 1) / export->pages_per_sheet;
				}

				if (export->uncollated == export->uncollated_copies)
					return FALSE;

				export->range = export->inc < 0 ? export->n_ranges - 1 : 0;
			}
			find_range (export);
			export->page = export->start;
		}

	/* in/decrement the page number until we reach the first page on the next EVEN or ODD sheet
	 * if we're not collating, we have to make sure that this is done only once! */
	} while ( export->collate == 1 &&
		((export->page_set == GTK_PAGE_SET_EVEN && export->sheet % 2 == 1) ||
		(export->page_set == GTK_PAGE_SET_ODD && export->sheet % 2 == 0)));

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
ev_print_operation_export_run_next (EvPrintOperationExport *export)
{
	EvPrintOperation *op = EV_PRINT_OPERATION (export);
	EvPrintOperation *next;
	EvDocument       *document;

	/* First pop the current job */
	document = op->document;
	ev_print_queue_pop (document);
	
	next = ev_print_queue_peek (document);
	if (next)
		ev_print_operation_export_begin (EV_PRINT_OPERATION_EXPORT (next));
}

static void
gtk_print_job_finished (GtkPrintJob            *print_job,
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

	ev_print_operation_export_run_next (export);
}

static void
export_print_done (EvPrintOperationExport *export)
{
	EvPrintOperation *op = EV_PRINT_OPERATION (export);
	GtkPrintSettings *settings;
	EvFileExporterCapabilities capabilities;
	GError *error = NULL;

	g_assert (export->temp_file != NULL);
	
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

	if (op->print_preview) {
		GKeyFile *key_file;
		gchar    *data = NULL;
		gsize     data_len;
		gchar    *print_settings_file = NULL;

		key_file = g_key_file_new ();

		gtk_print_settings_to_key_file (settings, key_file, NULL);
		gtk_page_setup_to_key_file (export->page_setup, key_file, NULL);
		g_key_file_set_string (key_file, "Print Job", "title", export->job_name);

		data = g_key_file_to_data (key_file, &data_len, &error);
		if (data) {
			gint fd;
			
			fd = g_file_open_tmp ("print-settingsXXXXXX", &print_settings_file, &error);
			if (!error)
				g_file_set_contents (print_settings_file, data, data_len, &error);
			close (fd);
			
			g_free (data);
		}

		g_key_file_free (key_file);

		if (!error) {
			gchar  *cmd;
			gchar  *quoted_filename;
			gchar  *quoted_settings_filename;
                        GAppInfo *app;
                        GdkAppLaunchContext *ctx;

			quoted_filename = g_shell_quote (export->temp_file);
			quoted_settings_filename = g_shell_quote (print_settings_file);
			cmd = g_strdup_printf ("evince-previewer --unlink-tempfile --print-settings %s %s",
					       quoted_settings_filename, quoted_filename);

			g_free (quoted_filename);
			g_free (quoted_settings_filename);

			app = g_app_info_create_from_commandline (cmd, NULL, 0, &error);

			if (app != NULL) {
				ctx = gdk_display_get_app_launch_context (gtk_widget_get_display (GTK_WIDGET (export->parent_window)));
				gdk_app_launch_context_set_screen (ctx, gtk_window_get_screen (export->parent_window));

				g_app_info_launch (app, NULL, G_APP_LAUNCH_CONTEXT (ctx), &error);

				g_object_unref (app);
				g_object_unref (ctx);
                        }

			g_free (cmd);
                }

		if (error) {
			if (print_settings_file)
				g_unlink (print_settings_file);
			g_free (print_settings_file);
		} else {
			g_signal_emit (op, signals[DONE], 0, GTK_PRINT_OPERATION_RESULT_APPLY);
			/* temp_file will be deleted by the previewer */

			ev_print_operation_export_run_next (export);
		}
	} else {
		GtkPrintJob *job;
		
		job = gtk_print_job_new (export->job_name,
					 export->printer,
					 settings,
					 export->page_setup);
		gtk_print_job_set_source_file (job, export->temp_file, &error);
		if (!error){
			gtk_print_job_send (job,
					    (GtkPrintJobCompleteFunc)gtk_print_job_finished,
					    g_object_ref (export),
					    (GDestroyNotify)g_object_unref);
		}
	}
	g_object_unref (settings);

	if (error) {
		g_set_error_literal (&export->error,
				     GTK_PRINT_ERROR,
				     GTK_PRINT_ERROR_GENERAL,
				     error->message);
		g_error_free (error);
		ev_print_operation_export_clear_temp_file (export);
		g_signal_emit (op, signals[DONE], 0, GTK_PRINT_OPERATION_RESULT_ERROR);

		ev_print_operation_export_run_next (export);
	}
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

	if (export->pages_per_sheet == 1 ||
	   ( export->page_count % export->pages_per_sheet == 0 &&
	   ( export->page_set == GTK_PAGE_SET_ALL ||
	   ( export->page_set == GTK_PAGE_SET_EVEN && export->sheet % 2 == 0 ) ||
	   ( export->page_set == GTK_PAGE_SET_ODD && export->sheet % 2 == 1 ) ) ) ) {

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
	export_cancel (export);
}

static void
export_cancel (EvPrintOperationExport *export)
{
	EvPrintOperation *op = EV_PRINT_OPERATION (export);

	if (export->idle_id > 0)
		g_source_remove (export->idle_id);
	export->idle_id = 0;

	if (export->job_export) {
		g_signal_handlers_disconnect_by_func (export->job_export,
						      export_job_finished,
						      export);
		g_signal_handlers_disconnect_by_func (export->job_export,
						      export_job_cancelled,
						      export);
		g_object_unref (export->job_export);
		export->job_export = NULL;
	}
	
	if (export->fd != -1) {
		close (export->fd);
		export->fd = -1;
	}

	ev_print_operation_export_clear_temp_file (export);

	g_signal_emit (op, signals[DONE], 0, GTK_PRINT_OPERATION_RESULT_CANCEL);

	ev_print_operation_export_run_next (export);
}

static void
update_progress (EvPrintOperationExport *export)
{
	EvPrintOperation *op = EV_PRINT_OPERATION (export);

	ev_print_operation_update_status (op, export->total,
					  export->n_pages_to_print,
					  export->total / (gdouble)export->n_pages_to_print);
}

static gboolean
export_print_page (EvPrintOperationExport *export)
{
	EvPrintOperation *op = EV_PRINT_OPERATION (export);

	if (!export->temp_file)
		return FALSE; /* cancelled */

	export->total++;
	export->collated++;

	/* note: when collating, page_count is increased in export_print_inc_page */
	if (!export->collate) {
		export->page_count++;
		export->sheet = 1 + (export->page_count - 1) / export->pages_per_sheet;
	}

	if (export->collated == export->collated_copies) {
		export->collated = 0;
		if (!export_print_inc_page (export)) {
			ev_document_doc_mutex_lock ();
			ev_file_exporter_end (EV_FILE_EXPORTER (op->document));
			ev_document_doc_mutex_unlock ();

			close (export->fd);
			export->fd = -1;
			update_progress (export);
			export_print_done (export);

			return FALSE;
		}
	}

	/* we're not collating and we've reached a sheet from the wrong sheet set */
	if (!export->collate &&
	    ((export->page_set == GTK_PAGE_SET_EVEN && export->sheet % 2 != 0) ||
	    (export->page_set == GTK_PAGE_SET_ODD && export->sheet % 2 != 1))) {

		do {
			export->page_count++;
			export->collated++;
			export->sheet = 1 + (export->page_count - 1) / export->pages_per_sheet;

			if (export->collated == export->collated_copies) {
				export->collated = 0;

				if (!export_print_inc_page (export)) {
					ev_document_doc_mutex_lock ();
					ev_file_exporter_end (EV_FILE_EXPORTER (op->document));
					ev_document_doc_mutex_unlock ();

					close (export->fd);
					export->fd = -1;

					update_progress (export);

					export_print_done (export);
					return FALSE;
				}
			}

		} while ((export->page_set == GTK_PAGE_SET_EVEN && export->sheet % 2 != 0) ||
	    		  (export->page_set == GTK_PAGE_SET_ODD && export->sheet % 2 != 1));

	}

	if (export->pages_per_sheet == 1 ||
	    (export->page_count % export->pages_per_sheet == 1 &&
	    (export->page_set == GTK_PAGE_SET_ALL ||
	    (export->page_set == GTK_PAGE_SET_EVEN && export->sheet % 2 == 0) ||
	    (export->page_set == GTK_PAGE_SET_ODD && export->sheet % 2 == 1)))) {
		ev_document_doc_mutex_lock ();
		ev_file_exporter_begin_page (EV_FILE_EXPORTER (op->document));
		ev_document_doc_mutex_unlock ();
	}

	if (!export->job_export) {
		export->job_export = ev_job_export_new (op->document);
		g_signal_connect (export->job_export, "finished",
				  G_CALLBACK (export_job_finished),
				  (gpointer)export);
		g_signal_connect (export->job_export, "cancelled",
				  G_CALLBACK (export_job_cancelled),
				  (gpointer)export);
	}

	ev_job_export_set_page (EV_JOB_EXPORT (export->job_export), export->page);
	ev_job_scheduler_push_job (export->job_export, EV_JOB_PRIORITY_NONE);

	update_progress (export);
	
	return FALSE;
}

static void
ev_print_operation_export_begin (EvPrintOperationExport *export)
{
	EvPrintOperation *op = EV_PRINT_OPERATION (export);

	if (!export->temp_file)
		return; /* cancelled */
	
	ev_document_doc_mutex_lock ();
	ev_file_exporter_begin (EV_FILE_EXPORTER (op->document), &export->fc);
	ev_document_doc_mutex_unlock ();

	export->idle_id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
					   (GSourceFunc)export_print_page,
					   export,
					   (GDestroyNotify)export_print_page_idle_finished);	
}

static EvFileExporterFormat
get_file_exporter_format (EvFileExporter   *exporter,
                          GtkPrintSettings *print_settings)
{
	const gchar *file_format;
	EvFileExporterFormat format = EV_FILE_FORMAT_PS;

	file_format = gtk_print_settings_get (print_settings, GTK_PRINT_SETTINGS_OUTPUT_FILE_FORMAT);
	if (file_format != NULL) {
		format = g_ascii_strcasecmp (file_format, "pdf") == 0 ?
			 EV_FILE_FORMAT_PDF : EV_FILE_FORMAT_PS;
	} else {
		if (ev_file_exporter_get_capabilities (exporter) &
		    EV_FILE_EXPORTER_CAN_GENERATE_PDF)
			format = EV_FILE_FORMAT_PDF;
		else
			format = EV_FILE_FORMAT_PS;
	}

	return format;
}

static void
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
	gchar                *filename;
	GError               *error = NULL;
	EvPrintOperation     *op = EV_PRINT_OPERATION (export);
	EvFileExporterFormat  format;
	
	if (response != GTK_RESPONSE_OK &&
	    response != GTK_RESPONSE_APPLY) {
		gtk_widget_destroy (GTK_WIDGET (dialog));
		g_signal_emit (op, signals[DONE], 0, GTK_PRINT_OPERATION_RESULT_CANCEL);

		return;
	}

	op->print_preview = (response == GTK_RESPONSE_APPLY);
	
	printer = gtk_print_unix_dialog_get_selected_printer (GTK_PRINT_UNIX_DIALOG (dialog));
	ev_print_operation_export_set_printer (export, printer);

	print_settings = gtk_print_unix_dialog_get_settings (GTK_PRINT_UNIX_DIALOG (dialog));
	ev_print_operation_export_set_print_settings (op, print_settings);

	page_setup = gtk_print_unix_dialog_get_page_setup (GTK_PRINT_UNIX_DIALOG (dialog));
	ev_print_operation_export_set_default_page_setup (op, page_setup);

	format = get_file_exporter_format (EV_FILE_EXPORTER (op->document),
					   print_settings);

	if ((format == EV_FILE_FORMAT_PS && !gtk_printer_accepts_ps (export->printer)) ||
	    (format == EV_FILE_FORMAT_PDF && !gtk_printer_accepts_pdf (export->printer))) {
		gtk_widget_destroy (GTK_WIDGET (dialog));
		
		g_set_error_literal (&export->error,
                                     GTK_PRINT_ERROR,
                                     GTK_PRINT_ERROR_GENERAL,
                                     _("Requested format is not supported by this printer."));
		g_signal_emit (op, signals[DONE], 0, GTK_PRINT_OPERATION_RESULT_ERROR);
		
		return;
	}

	filename = g_strdup_printf ("evince_print.%s.XXXXXX", format == EV_FILE_FORMAT_PDF ? "pdf" : "ps");
	export->fd = g_file_open_tmp (filename, &export->temp_file, &error);
	g_free (filename);
	if (export->fd <= -1) {
		gtk_widget_destroy (GTK_WIDGET (dialog));
		
		g_set_error_literal (&export->error,
				     GTK_PRINT_ERROR,
				     GTK_PRINT_ERROR_GENERAL,
				     error->message);
		g_error_free (error);
		g_signal_emit (op, signals[DONE], 0, GTK_PRINT_OPERATION_RESULT_ERROR);

		return;
	}

	export->current_page = gtk_print_unix_dialog_get_current_page (GTK_PRINT_UNIX_DIALOG (dialog));
	export->page_set = gtk_print_settings_get_page_set (print_settings);
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
	default:
		g_warning ("Unsupported print pages setting\n");
	case GTK_PRINT_PAGES_ALL:
		export->ranges = &export->one_range;

		export->ranges[0].start = 0;
		export->ranges[0].end = export->n_pages - 1;
		export->n_ranges = 1;
		
		break;
	}

	if (export->n_ranges < 1 || !clamp_ranges (export)) {
		GtkWidget *message_dialog;

		message_dialog = gtk_message_dialog_new (GTK_WINDOW (dialog),
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_WARNING,
						 GTK_BUTTONS_CLOSE,
						 "%s", _("Invalid page selection"));
		gtk_window_set_title (GTK_WINDOW (message_dialog), _("Warning"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message_dialog),
							  "%s", _("Your print range selection does not include any pages"));
		g_signal_connect (message_dialog, "response",
				  G_CALLBACK (gtk_widget_destroy),
				  NULL);
		gtk_widget_show (message_dialog);

		return;
	} else	ev_print_operation_update_status (op, -1, -1, 0.0);
 
	width = gtk_page_setup_get_paper_width (page_setup, GTK_UNIT_POINTS);
	height = gtk_page_setup_get_paper_height (page_setup, GTK_UNIT_POINTS);
	scale = gtk_print_settings_get_scale (print_settings) * 0.01;
	if (scale != 1.0) {
		width *= scale;
		height *= scale;
	}

	export->pages_per_sheet = MAX (1, gtk_print_settings_get_number_up (print_settings));
	
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

	get_first_and_last_page (export, &first_page, &last_page);

	export->fc.format = format;
	export->fc.filename = export->temp_file;
	export->fc.first_page = MIN (first_page, last_page);
	export->fc.last_page = MAX (first_page, last_page);
	export->fc.paper_width = width;
	export->fc.paper_height = height;
	export->fc.duplex = FALSE;
	export->fc.pages_per_sheet = export->pages_per_sheet;

	if (ev_print_queue_is_empty (op->document))
		ev_print_operation_export_begin (export);

	ev_print_queue_push (op);

	g_signal_emit (op, signals[BEGIN_PRINT], 0);
	
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
ev_print_operation_export_run (EvPrintOperation *op,
			       GtkWindow        *parent)
{
	EvPrintOperationExport *export = EV_PRINT_OPERATION_EXPORT (op);
	GtkWidget              *dialog;
	GtkPrintCapabilities    capabilities;

	ev_print_queue_init ();

	export->parent_window = parent;
	export->error = NULL;
	
	/* translators: Title of the print dialog */
	dialog = gtk_print_unix_dialog_new (_("Print"), parent);
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	
	capabilities = GTK_PRINT_CAPABILITY_PREVIEW |
		ev_file_exporter_get_capabilities (EV_FILE_EXPORTER (op->document));
	gtk_print_unix_dialog_set_manual_capabilities (GTK_PRINT_UNIX_DIALOG (dialog),
						       capabilities);

	gtk_print_unix_dialog_set_embed_page_setup (GTK_PRINT_UNIX_DIALOG (dialog),
						    export->embed_page_setup);

	gtk_print_unix_dialog_set_current_page (GTK_PRINT_UNIX_DIALOG (dialog),
						export->current_page);
	
	gtk_print_unix_dialog_set_settings (GTK_PRINT_UNIX_DIALOG (dialog),
					    export->print_settings);
	
	if (export->page_setup)
		gtk_print_unix_dialog_set_page_setup (GTK_PRINT_UNIX_DIALOG (dialog),
						      export->page_setup);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (ev_print_operation_export_print_dialog_response_cb),
			  export);

	gtk_window_present (GTK_WINDOW (dialog));
}

static void
ev_print_operation_export_cancel (EvPrintOperation *op)
{
	EvPrintOperationExport *export = EV_PRINT_OPERATION_EXPORT (op);

	if (export->job_export &&
	    !ev_job_is_finished (export->job_export)) {
		ev_job_cancel (export->job_export);
	} else {
		export_cancel (export);
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
ev_print_operation_export_set_embed_page_setup (EvPrintOperation *op,
						gboolean          embed)
{
	EvPrintOperationExport *export = EV_PRINT_OPERATION_EXPORT (op);

	export->embed_page_setup = embed;
}

static gboolean
ev_print_operation_export_get_embed_page_setup (EvPrintOperation *op)
{
	EvPrintOperationExport *export = EV_PRINT_OPERATION_EXPORT (op);

	return export->embed_page_setup;
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
	/* sheets are counted from 1 to be physical */
	export->sheet = 1;
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
	export->n_pages = ev_document_get_n_pages (op->document);

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
	ev_print_op_class->get_job_name = ev_print_operation_export_get_job_name;
	ev_print_op_class->run = ev_print_operation_export_run;
	ev_print_op_class->cancel = ev_print_operation_export_cancel;
	ev_print_op_class->get_error = ev_print_operation_export_get_error;
	ev_print_op_class->set_embed_page_setup = ev_print_operation_export_set_embed_page_setup;
	ev_print_op_class->get_embed_page_setup = ev_print_operation_export_get_embed_page_setup;

	g_object_class->constructor = ev_print_operation_export_constructor;
	g_object_class->finalize = ev_print_operation_export_finalize;
}

#endif /* GTKUNIXPRINT_ENABLED */

/* Print to cairo interface */
#define EV_TYPE_PRINT_OPERATION_PRINT         (ev_print_operation_print_get_type())
#define EV_PRINT_OPERATION_PRINT(object)      (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_PRINT_OPERATION_PRINT, EvPrintOperationPrint))
#define EV_PRINT_OPERATION_PRINT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_PRINT_OPERATION_PRINT, EvPrintOperationPrintClass))
#define EV_IS_PRINT_OPERATION_PRINT(object)   (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_PRINT_OPERATION_PRINT))

typedef struct _EvPrintOperationPrint      EvPrintOperationPrint;
typedef struct _EvPrintOperationPrintClass EvPrintOperationPrintClass;

static GType ev_print_operation_print_get_type (void) G_GNUC_CONST;

typedef enum {
	EV_SCALE_NONE,
	EV_SCALE_SHRINK_TO_PRINTABLE_AREA,
	EV_SCALE_FIT_TO_PRINTABLE_AREA
} EvPrintScale;

#define EV_PRINT_SETTING_PAGE_SCALE "evince-print-setting-page-scale"
#define EV_PRINT_SETTING_AUTOROTATE "evince-print-setting-page-autorotate"
#define EV_PRINT_SETTING_PAGE_SIZE  "evince-print-setting-page-size"

struct _EvPrintOperationPrint {
	EvPrintOperation parent;

	GtkPrintOperation *op;
	gint               n_pages_to_print;
	gint               total;
	EvJob             *job_print;
	gchar             *job_name;

        /* Page handling tab */
        GtkWidget   *scale_combo;
        EvPrintScale page_scale;
	GtkWidget   *autorotate_button;
	gboolean     autorotate;
	GtkWidget   *source_button;
	gboolean     use_source_size;
};

struct _EvPrintOperationPrintClass {
	EvPrintOperationClass parent_class;
};

G_DEFINE_TYPE (EvPrintOperationPrint, ev_print_operation_print, EV_TYPE_PRINT_OPERATION)

static void
ev_print_operation_print_set_current_page (EvPrintOperation *op,
					   gint              current_page)
{
	EvPrintOperationPrint *print = EV_PRINT_OPERATION_PRINT (op);

	gtk_print_operation_set_current_page (print->op, current_page);
}

static void
ev_print_operation_print_set_print_settings (EvPrintOperation *op,
					     GtkPrintSettings *print_settings)
{
	EvPrintOperationPrint *print = EV_PRINT_OPERATION_PRINT (op);

	gtk_print_operation_set_print_settings (print->op, print_settings);
}

static GtkPrintSettings *
ev_print_operation_print_get_print_settings (EvPrintOperation *op)
{
	EvPrintOperationPrint *print = EV_PRINT_OPERATION_PRINT (op);

	return gtk_print_operation_get_print_settings (print->op);
}

static void
ev_print_operation_print_set_default_page_setup (EvPrintOperation *op,
						 GtkPageSetup     *page_setup)
{
	EvPrintOperationPrint *print = EV_PRINT_OPERATION_PRINT (op);

	gtk_print_operation_set_default_page_setup (print->op, page_setup);
}

static GtkPageSetup *
ev_print_operation_print_get_default_page_setup (EvPrintOperation *op)
{
	EvPrintOperationPrint *print = EV_PRINT_OPERATION_PRINT (op);

	return gtk_print_operation_get_default_page_setup (print->op);
}

static void
ev_print_operation_print_set_job_name (EvPrintOperation *op,
				       const gchar      *job_name)
{
	EvPrintOperationPrint *print = EV_PRINT_OPERATION_PRINT (op);

	g_free (print->job_name);
	print->job_name = g_strdup (job_name);

	gtk_print_operation_set_job_name (print->op, print->job_name);
}

static const gchar *
ev_print_operation_print_get_job_name (EvPrintOperation *op)
{
	EvPrintOperationPrint *print = EV_PRINT_OPERATION_PRINT (op);

	if (!print->job_name) {
		gchar *name;

		g_object_get (print->op, "job_name", &name, NULL);
		print->job_name = name;
	}

	return print->job_name;
}

static void
ev_print_operation_print_run (EvPrintOperation *op,
			      GtkWindow        *parent)
{
	EvPrintOperationPrint *print = EV_PRINT_OPERATION_PRINT (op);

	gtk_print_operation_run (print->op,
				 GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
				 parent, NULL);
}

static void
ev_print_operation_print_cancel (EvPrintOperation *op)
{
	EvPrintOperationPrint *print = EV_PRINT_OPERATION_PRINT (op);

        if (print->job_print)
                ev_job_cancel (print->job_print);
        else
                gtk_print_operation_cancel (print->op);
}

static void
ev_print_operation_print_get_error (EvPrintOperation *op,
				    GError          **error)
{
	EvPrintOperationPrint *print = EV_PRINT_OPERATION_PRINT (op);

	gtk_print_operation_get_error (print->op, error);
}

static void
ev_print_operation_print_set_embed_page_setup (EvPrintOperation *op,
					       gboolean          embed)
{
	EvPrintOperationPrint *print = EV_PRINT_OPERATION_PRINT (op);

	gtk_print_operation_set_embed_page_setup (print->op, embed);
}

static gboolean
ev_print_operation_print_get_embed_page_setup (EvPrintOperation *op)
{
	EvPrintOperationPrint *print = EV_PRINT_OPERATION_PRINT (op);

	return gtk_print_operation_get_embed_page_setup (print->op);
}

static void
ev_print_operation_print_begin_print (EvPrintOperationPrint *print,
				      GtkPrintContext       *context)
{
	EvPrintOperation *op = EV_PRINT_OPERATION (print);
	gint              n_pages;

	n_pages = ev_document_get_n_pages (op->document);
	gtk_print_operation_set_n_pages (print->op, n_pages);
	ev_print_operation_update_status (op, -1, n_pages, 0);

	g_signal_emit (op, signals[BEGIN_PRINT], 0);
}

static void
ev_print_operation_print_done (EvPrintOperationPrint  *print,
			       GtkPrintOperationResult result)
{
	EvPrintOperation *op = EV_PRINT_OPERATION (print);

	ev_print_operation_update_status (op, 0, print->n_pages_to_print, 1.0);

	g_signal_emit (op, signals[DONE], 0, result);
}

static void
ev_print_operation_print_status_changed (EvPrintOperationPrint *print)
{
	GtkPrintStatus status;

	status = gtk_print_operation_get_status (print->op);
	if (status == GTK_PRINT_STATUS_GENERATING_DATA)
		print->n_pages_to_print = gtk_print_operation_get_n_pages_to_print (print->op);
}

static void
print_job_finished (EvJobPrint            *job,
		    EvPrintOperationPrint *print)
{
	EvPrintOperation *op = EV_PRINT_OPERATION (print);

	gtk_print_operation_draw_page_finish (print->op);

	print->total++;
	ev_print_operation_update_status (op, print->total,
					  print->n_pages_to_print,
					  print->total / (gdouble)print->n_pages_to_print);
	ev_job_print_set_cairo (job, NULL);
}

static gboolean
draw_page_finish_idle (EvPrintOperationPrint *print)
{
        if (ev_job_scheduler_get_running_thread_job () == print->job_print)
                return TRUE;

        gtk_print_operation_draw_page_finish (print->op);

        return FALSE;
}

static void
print_job_cancelled (EvJobPrint            *job,
                     EvPrintOperationPrint *print)
{
        /* Finish the current page, so that draw-page
         * is emitted again and it will cancel the
         * print operation. If the job is still
         * running, wait until it finishes.
         */
        if (ev_job_scheduler_get_running_thread_job () == print->job_print)
                g_idle_add ((GSourceFunc)draw_page_finish_idle, print);
        else
                gtk_print_operation_draw_page_finish (print->op);
}

static void
ev_print_operation_print_request_page_setup (EvPrintOperationPrint *print,
					     GtkPrintContext       *context,
					     gint                   page_nr,
					     GtkPageSetup          *setup)
{
	EvPrintOperation *op = EV_PRINT_OPERATION (print);
	gdouble           width, height;
	GtkPaperSize     *paper_size;

	ev_document_get_page_size (op->document, page_nr,
				   &width, &height);

	if (print->use_source_size) {
		paper_size = gtk_paper_size_new_custom ("custom", "custom",
							width, height, GTK_UNIT_POINTS);
		gtk_page_setup_set_paper_size_and_default_margins (setup, paper_size);
		gtk_paper_size_free (paper_size);
	}

	if (print->autorotate) {
		if (width > height)
			gtk_page_setup_set_orientation (setup, GTK_PAGE_ORIENTATION_LANDSCAPE);
		else
			gtk_page_setup_set_orientation (setup, GTK_PAGE_ORIENTATION_PORTRAIT);
	}
}

static void
_print_context_get_hard_margins (GtkPrintContext *context,
				 gdouble         *top,
				 gdouble         *bottom,
				 gdouble         *left,
				 gdouble         *right)
{
	if (!gtk_print_context_get_hard_margins (context, top, bottom, left, right)) {
		*top = 0;
		*bottom = 0;
		*left = 0;
		*right = 0;
	}
}

static void
ev_print_operation_print_draw_page (EvPrintOperationPrint *print,
				    GtkPrintContext       *context,
				    gint                   page)
{
	EvPrintOperation *op = EV_PRINT_OPERATION (print);
	cairo_t          *cr;
	gdouble           cr_width, cr_height;
	gdouble           width, height, scale;
	gdouble           x_scale, y_scale;
	gdouble           top, bottom, left, right;

	gtk_print_operation_set_defer_drawing (print->op);

	if (!print->job_print) {
		print->job_print = ev_job_print_new (op->document);
		g_signal_connect (G_OBJECT (print->job_print), "finished",
				  G_CALLBACK (print_job_finished),
				  (gpointer)print);
                g_signal_connect (G_OBJECT (print->job_print), "cancelled",
                                  G_CALLBACK (print_job_cancelled),
                                  (gpointer)print);
	} else if (g_cancellable_is_cancelled (print->job_print->cancellable)) {
                gtk_print_operation_cancel (print->op);
                ev_job_print_set_cairo (EV_JOB_PRINT (print->job_print), NULL);
                return;
        }

	ev_job_print_set_page (EV_JOB_PRINT (print->job_print), page);

	cr = gtk_print_context_get_cairo_context (context);
	cr_width = gtk_print_context_get_width (context);
	cr_height = gtk_print_context_get_height (context);
	ev_document_get_page_size (op->document, page, &width, &height);

	if (print->page_scale == EV_SCALE_NONE) {
		/* Center document page on the printed page */
		if (print->autorotate)
			cairo_translate (cr, (cr_width - width) / 2, (cr_height - height) / 2);
	} else {
		_print_context_get_hard_margins (context, &top, &bottom, &left, &right);

		x_scale = (cr_width - left - right) / width;
		y_scale = (cr_height - top - bottom) / height;
                scale = MIN (x_scale, y_scale);

                /* Ignore scale > 1 when shrinking to printable area */
                if (scale > 1.0 && print->page_scale == EV_SCALE_SHRINK_TO_PRINTABLE_AREA)
                        scale = 1.0;

		if (print->autorotate) {
			double left_right_sides, top_bottom_sides;

			cairo_translate (cr, (cr_width - scale * width) / 2,
					 (cr_height - scale * height) / 2);

			/* Ensure document page is within the margins. The
			 * scale guarantees the document will fit in the
			 * margins so we just need to check each side and
			 * if it overhangs the margin, translate it to the
			 * margin. */
			left_right_sides = (cr_width - width*scale)/2;
			top_bottom_sides = (cr_height - height*scale)/2;
			if (left_right_sides < left)
				cairo_translate (cr, left - left_right_sides, 0);

			if (left_right_sides < right)
				cairo_translate (cr, -(right - left_right_sides), 0);

			if (top_bottom_sides < top)
				cairo_translate (cr, 0, top - top_bottom_sides);

			if (top_bottom_sides < bottom)
				cairo_translate (cr, 0, -(bottom - top_bottom_sides));
		} else {
			cairo_translate (cr, left, top);
		}

		if (print->page_scale == EV_SCALE_FIT_TO_PRINTABLE_AREA || scale < 1.0) {
			cairo_scale (cr, scale, scale);
		}
	}

	ev_job_print_set_cairo (EV_JOB_PRINT (print->job_print), cr);
	ev_job_scheduler_push_job (print->job_print, EV_JOB_PRIORITY_NONE);
}

static GObject *
ev_print_operation_print_create_custom_widget (EvPrintOperationPrint *print,
					       GtkPrintContext       *context)
{
	GtkPrintSettings *settings;
	GtkWidget        *label;
	GtkWidget        *grid;
	EvPrintScale      page_scale;
	gboolean          autorotate;
	gboolean          use_source_size;

	settings = gtk_print_operation_get_print_settings (print->op);
	page_scale = gtk_print_settings_get_int_with_default (settings, EV_PRINT_SETTING_PAGE_SCALE, 1);
	autorotate = gtk_print_settings_has_key (settings, EV_PRINT_SETTING_AUTOROTATE) ?
		gtk_print_settings_get_bool (settings, EV_PRINT_SETTING_AUTOROTATE) :
		TRUE;
	use_source_size = gtk_print_settings_get_bool (settings, EV_PRINT_SETTING_PAGE_SIZE);

	grid = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
	gtk_container_set_border_width (GTK_CONTAINER (grid), 12);

	label = gtk_label_new (_("Page Scaling:"));
	gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
	gtk_widget_show (label);

	print->scale_combo = gtk_combo_box_text_new ();
	/* translators: Value for 'Page Scaling:' to not scale the document pages on printing */
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (print->scale_combo), _("None"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (print->scale_combo), _("Shrink to Printable Area"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (print->scale_combo), _("Fit to Printable Area"));
	gtk_combo_box_set_active (GTK_COMBO_BOX (print->scale_combo), page_scale);
	gtk_widget_set_tooltip_text (print->scale_combo,
		_("Scale document pages to fit the selected printer page. Select from one of the following:\n"
		  "\n"
		  "• \"None\": No page scaling is performed.\n"
		  "\n"
		  "• \"Shrink to Printable Area\": Document pages larger than the printable area"
		  " are reduced to fit the printable area of the printer page.\n"
		  "\n"
		  "• \"Fit to Printable Area\": Document pages are enlarged or reduced as"
		  " required to fit the printable area of the printer page.\n"));
	gtk_grid_attach (GTK_GRID (grid), print->scale_combo, 1, 0, 1, 1);
	gtk_widget_show (print->scale_combo);

	print->autorotate_button = gtk_check_button_new_with_label (_("Auto Rotate and Center"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (print->autorotate_button), autorotate);
	gtk_widget_set_tooltip_text (print->autorotate_button,
		_("Rotate printer page orientation of each page to match orientation of each document page. "
		  "Document pages will be centered within the printer page."));
	gtk_grid_attach (GTK_GRID (grid), print->autorotate_button, 0, 1, 2, 1);
	gtk_widget_show (print->autorotate_button);

	print->source_button = gtk_check_button_new_with_label (_("Select page size using document page size"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (print->source_button), use_source_size);
	gtk_widget_set_tooltip_text (print->source_button, _("When enabled, each page will be printed on "
							     "the same size paper as the document page."));
	gtk_grid_attach (GTK_GRID (grid), print->source_button, 0, 2, 2, 1);
	gtk_widget_show (print->source_button);

	return G_OBJECT (grid);
}

static void
ev_print_operation_print_custom_widget_apply (EvPrintOperationPrint *print,
					      GtkPrintContext       *context)
{
	GtkPrintSettings *settings;

	print->page_scale = gtk_combo_box_get_active (GTK_COMBO_BOX (print->scale_combo));
	print->autorotate = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (print->autorotate_button));
	print->use_source_size = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (print->source_button));
	settings = gtk_print_operation_get_print_settings (print->op);
	gtk_print_settings_set_int (settings, EV_PRINT_SETTING_PAGE_SCALE, print->page_scale);
	gtk_print_settings_set_bool (settings, EV_PRINT_SETTING_AUTOROTATE, print->autorotate);
	gtk_print_settings_set_bool (settings, EV_PRINT_SETTING_PAGE_SIZE, print->use_source_size);
}

static gboolean
ev_print_operation_print_preview (EvPrintOperationPrint *print)
{
	EV_PRINT_OPERATION (print)->print_preview = TRUE;

	return FALSE;
}

static void
ev_print_operation_print_finalize (GObject *object)
{
	EvPrintOperationPrint *print = EV_PRINT_OPERATION_PRINT (object);
	GApplication *application;

	if (print->op) {
		g_object_unref (print->op);
		print->op = NULL;
	}

	if (print->job_name) {
		g_free (print->job_name);
		print->job_name = NULL;
	}

	if (print->job_print) {
		if (!ev_job_is_finished (print->job_print))
			ev_job_cancel (print->job_print);
		g_signal_handlers_disconnect_by_func (print->job_print,
						      print_job_finished,
						      print);
		g_signal_handlers_disconnect_by_func (print->job_print,
						      print_job_cancelled,
						      print);
		g_object_unref (print->job_print);
		print->job_print = NULL;
	}

	(* G_OBJECT_CLASS (ev_print_operation_print_parent_class)->finalize) (object);

        application = g_application_get_default ();
        if (application)
        	g_application_release (application);
}

static void
ev_print_operation_print_init (EvPrintOperationPrint *print)
{
	GApplication *application;

	print->op = gtk_print_operation_new ();
	g_signal_connect_swapped (print->op, "begin_print",
				  G_CALLBACK (ev_print_operation_print_begin_print),
				  print);
	g_signal_connect_swapped (print->op, "done",
				  G_CALLBACK (ev_print_operation_print_done),
				  print);
	g_signal_connect_swapped (print->op, "draw_page",
				  G_CALLBACK (ev_print_operation_print_draw_page),
				  print);
	g_signal_connect_swapped (print->op, "status_changed",
				  G_CALLBACK (ev_print_operation_print_status_changed),
				  print);
	g_signal_connect_swapped (print->op, "request_page_setup",
				  G_CALLBACK (ev_print_operation_print_request_page_setup),
				  print);
	g_signal_connect_swapped (print->op, "create_custom_widget",
				  G_CALLBACK (ev_print_operation_print_create_custom_widget),
				  print);
	g_signal_connect_swapped (print->op, "custom_widget_apply",
				  G_CALLBACK (ev_print_operation_print_custom_widget_apply),
				  print);
        g_signal_connect_swapped (print->op, "preview",
				  G_CALLBACK (ev_print_operation_print_preview),
				  print);
	gtk_print_operation_set_allow_async (print->op, TRUE);
	gtk_print_operation_set_use_full_page (print->op, TRUE);
	gtk_print_operation_set_unit (print->op, GTK_UNIT_POINTS);
	gtk_print_operation_set_custom_tab_label (print->op, _("Page Handling"));

	application = g_application_get_default ();
	if (application)
	        g_application_hold (application);
}

static void
ev_print_operation_print_class_init (EvPrintOperationPrintClass *klass)
{
	GObjectClass          *g_object_class = G_OBJECT_CLASS (klass);
	EvPrintOperationClass *ev_print_op_class = EV_PRINT_OPERATION_CLASS (klass);

	ev_print_op_class->set_current_page = ev_print_operation_print_set_current_page;
	ev_print_op_class->set_print_settings = ev_print_operation_print_set_print_settings;
	ev_print_op_class->get_print_settings = ev_print_operation_print_get_print_settings;
	ev_print_op_class->set_default_page_setup = ev_print_operation_print_set_default_page_setup;
	ev_print_op_class->get_default_page_setup = ev_print_operation_print_get_default_page_setup;
	ev_print_op_class->set_job_name = ev_print_operation_print_set_job_name;
	ev_print_op_class->get_job_name = ev_print_operation_print_get_job_name;
	ev_print_op_class->run = ev_print_operation_print_run;
	ev_print_op_class->cancel = ev_print_operation_print_cancel;
	ev_print_op_class->get_error = ev_print_operation_print_get_error;
	ev_print_op_class->set_embed_page_setup = ev_print_operation_print_set_embed_page_setup;
	ev_print_op_class->get_embed_page_setup = ev_print_operation_print_get_embed_page_setup;

	g_object_class->finalize = ev_print_operation_print_finalize;
}

gboolean
ev_print_operation_exists_for_document (EvDocument *document)
{
#if GTKUNIXPRINT_ENABLED
	return (EV_IS_FILE_EXPORTER(document) || EV_IS_DOCUMENT_PRINT(document));
#else
	return EV_IS_DOCUMENT_PRINT(document);
#endif /* GTKUNIXPRINT_ENABLED */
}

/* Factory method */
EvPrintOperation *
ev_print_operation_new (EvDocument *document)
{
	EvPrintOperation *op = NULL;

	g_return_val_if_fail (ev_print_operation_exists_for_document (document), NULL);

	if (EV_IS_DOCUMENT_PRINT (document))
		op = EV_PRINT_OPERATION (g_object_new (EV_TYPE_PRINT_OPERATION_PRINT,
						       "document", document, NULL));
	else
#if GTKUNIXPRINT_ENABLED
		op = EV_PRINT_OPERATION (g_object_new (EV_TYPE_PRINT_OPERATION_EXPORT,
						       "document", document, NULL));
#else
		op = NULL;
#endif
	return op;
}
