/* this file is part of evince, a gnome document viewer
 *
 * Copyright © 2008 Carlos Garcia Campos <carlosgc@gnome.org>
 * Copyright © 2016, Red Hat, Inc.
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

#include "ev-print-operation.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <unistd.h>

#include "ev-jobs.h"
#include "ev-job-scheduler.h"

#if defined (G_OS_UNIX)
#define PORTAL_ENABLED
#endif

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

	g_clear_object (&op->document);
	g_clear_pointer (&op->status, g_free);

	G_OBJECT_CLASS (ev_print_operation_parent_class)->finalize (object);
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

/* Export interface */
#define EV_TYPE_PRINT_OPERATION_EXPORT            (ev_print_operation_export_get_type())
#define EV_PRINT_OPERATION_EXPORT(object)         (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_PRINT_OPERATION_EXPORT, EvPrintOperationExport))
#define EV_PRINT_OPERATION_EXPORT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_PRINT_OPERATION_EXPORT, EvPrintOperationExportClass))
#define EV_IS_PRINT_OPERATION_EXPORT(object)      (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_PRINT_OPERATION_EXPORT))
#define EV_IS_PRINT_OPERATION_EXPORT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_PRINT_OPERATION_EXPORT))
#define EV_PRINT_OPERATION_EXPORT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_PRINT_OPERATION_EXPORT, EvPrintOperationExportClass))

typedef struct _EvPrintOperationExport      EvPrintOperationExport;
typedef struct _EvPrintOperationExportClass EvPrintOperationExportClass;

static GType    ev_print_operation_export_get_type (void) G_GNUC_CONST;

static void     ev_print_operation_export_begin    (EvPrintOperationExport *export);
static gboolean export_print_page                  (EvPrintOperationExport *export);
static void     export_cancel                      (EvPrintOperationExport *export);

struct _EvPrintOperationExport {
	EvPrintOperation parent;

	EvJob *job_export;
	GError *error;

	gint n_pages;
	gint current_page;
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

        gboolean (*run_previewer) (EvPrintOperationExport *export,
                                   GtkPrintSettings       *settings,
                                   GError                **error);
        gboolean (*send_job)      (EvPrintOperationExport *export,
                                   GtkPrintSettings       *settings,
                                   GError                **error);
};

G_DEFINE_ABSTRACT_TYPE (EvPrintOperationExport, ev_print_operation_export, EV_TYPE_PRINT_OPERATION)

/* Internal print queue */
static GHashTable *print_queue = NULL;

static void
queue_free (GQueue *queue)
{
	g_queue_free_full (queue, g_object_unref);
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

static gboolean
ev_print_operation_export_run_previewer (EvPrintOperationExport *export,
                                         GtkPrintSettings       *settings,
                                         GError                **error)
{
	g_return_val_if_fail (EV_IS_PRINT_OPERATION_EXPORT (export), FALSE);

        return EV_PRINT_OPERATION_EXPORT_GET_CLASS (export)->run_previewer (export,
                                                                            settings,
                                                                            error);
}

static gboolean
ev_print_operation_export_send_job (EvPrintOperationExport *export,
                                    GtkPrintSettings       *settings,
                                    GError                **error)
{
	g_return_val_if_fail (EV_IS_PRINT_OPERATION_EXPORT (export), FALSE);

        return EV_PRINT_OPERATION_EXPORT_GET_CLASS (export)->send_job (export,
                                                                       settings,
                                                                       error);
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
	g_clear_pointer (&export->temp_file, g_free);
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

	if (op->print_preview)
                ev_print_operation_export_run_previewer (export, settings, &error);
        else
                ev_print_operation_export_send_job (export, settings, &error);

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

	g_clear_handle_id (&export->idle_id, g_source_remove);

	if (export->job_export) {
		g_signal_handlers_disconnect_by_func (export->job_export,
						      export_job_finished,
						      export);
		g_signal_handlers_disconnect_by_func (export->job_export,
						      export_job_cancelled,
						      export);
		g_clear_object (&export->job_export);
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
		return G_SOURCE_REMOVE; /* cancelled */

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

			update_progress (export);
			export_print_done (export);

			return G_SOURCE_REMOVE;
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

					update_progress (export);

					export_print_done (export);
					return G_SOURCE_REMOVE;
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

	return G_SOURCE_REMOVE;
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
ev_print_operation_export_run (EvPrintOperation *op,
			       GtkWindow        *parent)
{
	EvPrintOperationExport *export = EV_PRINT_OPERATION_EXPORT (op);

	ev_print_queue_init ();

	export->error = NULL;
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

static gboolean
ev_print_operation_export_mkstemp (EvPrintOperationExport *export,
                                   EvFileExporterFormat    format)
{
        char *filename;
        GError *err = NULL;

        filename = g_strdup_printf ("evince_print.%s.XXXXXX", format == EV_FILE_FORMAT_PDF ? "pdf" : "ps");
        export->fd = g_file_open_tmp (filename, &export->temp_file, &err);
        g_free (filename);
        if (export->fd == -1) {
                g_set_error_literal (&export->error,
                                     GTK_PRINT_ERROR,
                                     GTK_PRINT_ERROR_GENERAL,
                                     err->message);
                g_error_free (err);
                return FALSE;
        }

        return TRUE;
}

static gboolean
ev_print_operation_export_update_ranges (EvPrintOperationExport *export)
{
        GtkPrintPages print_pages;

        export->page_set = gtk_print_settings_get_page_set (export->print_settings);
        print_pages = gtk_print_settings_get_print_pages (export->print_settings);

        switch (print_pages) {
        case GTK_PRINT_PAGES_CURRENT: {
                export->ranges = &export->one_range;

                export->ranges[0].start = export->current_page;
                export->ranges[0].end = export->current_page;
                export->n_ranges = 1;

                break;
        }
        case GTK_PRINT_PAGES_RANGES: {
                gint i;

                export->ranges = gtk_print_settings_get_page_ranges (export->print_settings,
                                                                     &export->n_ranges);
                for (i = 0; i < export->n_ranges; i++)
                        if (export->ranges[i].end == -1 || export->ranges[i].end >= export->n_pages)
                                export->ranges[i].end = export->n_pages - 1;

                break;
        }
        default:
                g_warning ("Unsupported print pages setting\n");
                /* fallthrough */
        case GTK_PRINT_PAGES_ALL: {
                export->ranges = &export->one_range;

                export->ranges[0].start = 0;
                export->ranges[0].end = export->n_pages - 1;
                export->n_ranges = 1;

                break;
        }
        }

        /* Return %TRUE iff there are any pages in the range(s) */
        return (export->n_ranges >= 1 && clamp_ranges (export));
}

static void
ev_print_operation_export_prepare (EvPrintOperationExport *export,
                                   EvFileExporterFormat    format)
{
        EvPrintOperation *op = EV_PRINT_OPERATION (export);
        gdouble           scale;
        gdouble           width;
        gdouble           height;
        gint              first_page;
        gint              last_page;

        ev_print_operation_update_status (op, -1, -1, 0.0);

        width = gtk_page_setup_get_paper_width (export->page_setup, GTK_UNIT_POINTS);
        height = gtk_page_setup_get_paper_height (export->page_setup, GTK_UNIT_POINTS);
        scale = gtk_print_settings_get_scale (export->print_settings) * 0.01;
        if (scale != 1.0) {
                width *= scale;
                height *= scale;
        }

        export->pages_per_sheet = MAX (1, gtk_print_settings_get_number_up (export->print_settings));

        export->copies = gtk_print_settings_get_n_copies (export->print_settings);
        export->collate = gtk_print_settings_get_collate (export->print_settings);
        export->reverse = gtk_print_settings_get_reverse (export->print_settings);

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
}

static void
ev_print_operation_export_finalize (GObject *object)
{
	EvPrintOperationExport *export = EV_PRINT_OPERATION_EXPORT (object);

	g_clear_handle_id (&export->idle_id, g_source_remove);

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

	g_clear_pointer (&export->temp_file, g_free);
	g_clear_pointer (&export->job_name, g_free);

	if (export->job_export) {
		if (!ev_job_is_finished (export->job_export))
			ev_job_cancel (export->job_export);
		g_signal_handlers_disconnect_by_func (export->job_export,
						      export_job_finished,
						      export);
		g_signal_handlers_disconnect_by_func (export->job_export,
						      export_job_cancelled,
						      export);
		g_clear_object (&export->job_export);
	}

	g_clear_error (&export->error);
	g_clear_object (&export->print_settings);
	g_clear_object (&export->page_setup);

	G_OBJECT_CLASS (ev_print_operation_export_parent_class)->finalize (object);
}

static void
ev_print_operation_export_init (EvPrintOperationExport *export)
{
	/* sheets are counted from 1 to be physical */
	export->sheet = 1;
}

static void
ev_print_operation_export_constructed (GObject *object)
{
        EvPrintOperation *op = EV_PRINT_OPERATION (object);
        EvPrintOperationExport *export = EV_PRINT_OPERATION_EXPORT (object);

        G_OBJECT_CLASS (ev_print_operation_export_parent_class)->constructed (object);

	export->n_pages = ev_document_get_n_pages (op->document);
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

	g_object_class->constructed = ev_print_operation_export_constructed;
	g_object_class->finalize = ev_print_operation_export_finalize;
}

#ifdef PORTAL_ENABLED

/* Export with Portal */
/* Some code copied from gtk+/gtk/gtkprintoperation-portal.c */

#include "ev-portal.h"

#include <gio/gunixfdlist.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif

#define EV_TYPE_PRINT_OPERATION_EXPORT_PORTAL            (ev_print_operation_export_portal_get_type())
#define EV_PRINT_OPERATION_EXPORT_PORTAL(object)         (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_PRINT_OPERATION_EXPORT_PORTAL, EvPrintOperationExportPortal))
#define EV_PRINT_OPERATION_EXPORT_PORTAL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_PRINT_OPERATION_EXPORT_PORTAL, EvPrintOperationExportPortalClass))
#define EV_IS_PRINT_OPERATION_EXPORT_PORTAL(object)      (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_PRINT_OPERATION_EXPORT_PORTAL))
#define EV_IS_PRINT_OPERATION_EXPORT_PORTAL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_PRINT_OPERATION_EXPORT_PORTAL))
#define EV_PRINT_OPERATION_EXPORT_PORTAL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_PRINT_OPERATION_EXPORT_PORTAL, EvPrintOperationExportPortalClass))

typedef struct _EvPrintOperationExportPortal      EvPrintOperationExportPortal;
typedef struct _EvPrintOperationExportPortalClass EvPrintOperationExportPortalClass;

static GType ev_print_operation_export_portal_get_type (void) G_GNUC_CONST;

struct _EvPrintOperationExportPortal {
        EvPrintOperationExport parent;

        GDBusProxy *proxy;
        guint response_signal_id;
        guint32 token;
        char *parent_window_handle;
        char *prepare_print_handle;
};

struct _EvPrintOperationExportPortalClass {
        EvPrintOperationExportClass parent_class;
};

G_DEFINE_TYPE (EvPrintOperationExportPortal, ev_print_operation_export_portal, EV_TYPE_PRINT_OPERATION_EXPORT)

/* BEGIN copied from gtkwindow.c */
/* Thanks, gtk+, for not exporting this essential API! */

typedef void (*EvGtkWindowHandleExported)  (GtkWindow  *window,
                                            const char *handle,
                                            gpointer    user_data);

static gboolean ev_gtk_window_export_handle   (GtkWindow                 *window,
                                               EvGtkWindowHandleExported  callback,
                                               gpointer                   user_data);

#ifdef GDK_WINDOWING_WAYLAND

typedef void (*EvGdkWaylandToplevelExported) (GdkToplevel  *toplevel,
                                              const char   *handle,
                                              gpointer      user_data);

typedef struct {
        GtkWindow *window;
        EvGtkWindowHandleExported callback;
        gpointer user_data;
} WaylandSurfaceHandleExportedData;

static void
wayland_window_handle_exported_cb (GdkToplevel  *toplevel,
                                   const char   *wayland_handle_str,
                                   gpointer      user_data)
{
        WaylandSurfaceHandleExportedData *data = user_data;
        char *handle_str;

        handle_str = g_strdup_printf ("wayland:%s", wayland_handle_str);
        data->callback (data->window, handle_str, data->user_data);
        g_free (handle_str);
}
#endif

static gboolean
ev_gtk_window_export_handle (GtkWindow                 *window,
                             EvGtkWindowHandleExported  callback,
                             gpointer                   user_data)
{
	GdkSurface *surface = gtk_native_get_surface (GTK_NATIVE (window));
#ifdef GDK_WINDOWING_X11
        if (GDK_IS_X11_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window)))) {
                char *handle_str;
                guint32 xid = (guint32) gdk_x11_surface_get_xid (surface);

                handle_str = g_strdup_printf ("x11:%x", xid);
                callback (window, handle_str, user_data);
                g_free (handle_str);

                return TRUE;
        }
#endif
#ifdef GDK_WINDOWING_WAYLAND
        if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window)))) {
                WaylandSurfaceHandleExportedData *data;
                data = g_new0 (WaylandSurfaceHandleExportedData, 1);
                data->window = window;
                data->callback = callback;
                data->user_data = user_data;

                if (!gdk_wayland_toplevel_export_handle (GDK_TOPLEVEL (surface),
                                                         wayland_window_handle_exported_cb,
                                                         data,
                                                         g_free)) {
                        g_free (data);
                        return FALSE;
                }

                return TRUE;
        }
#endif

        g_printerr ("Unsupported windowing system.\n");
        return FALSE;
}

#if 0
static void
ev_gtk_window_unexport_handle (GtkWindow *window)
{
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window))))
    {
      GdkWindow *gdk_window = gtk_widget_get_window (GTK_WIDGET (window));

      gdk_wayland_window_unexport_handle (gdk_window);
    }
#endif
}
#endif

/* END copied from gtkwindow.c */

static void
export_portal_unsubscribe_response (EvPrintOperationExportPortal *export_portal)
{
        if (export_portal->response_signal_id == 0)
                return;

        g_dbus_connection_signal_unsubscribe (g_dbus_proxy_get_connection (G_DBUS_PROXY (export_portal->proxy)),
                                              export_portal->response_signal_id);
        export_portal->response_signal_id = 0;
}

static void
export_portal_request_response_cb (GDBusConnection *connection,
                                   const char      *sender_name,
                                   const char      *object_path,
                                   const char      *interface_name,
                                   const char      *signal_name,
                                   GVariant        *parameters,
                                   gpointer         data)
{
        EvPrintOperationExportPortal *export_portal = EV_PRINT_OPERATION_EXPORT_PORTAL (data);
        EvPrintOperationExport *export = EV_PRINT_OPERATION_EXPORT (data);
        EvPrintOperation *op = EV_PRINT_OPERATION (data);
        guint32 response;
        GVariant *options;
        GVariant *v;
        GtkPrintSettings *settings;
        GtkPageSetup *page_setup;
        EvFileExporterFormat format;

        export_portal_unsubscribe_response (export_portal);

        g_assert_cmpstr (object_path, ==, export_portal->prepare_print_handle); // FIXMEchpe remove

        if (!g_str_equal (object_path, export_portal->prepare_print_handle))
                return;

        g_variant_get (parameters, "(u@a{sv})", &response, &options);

        /* Cancelled? */
        if (response != 0) {
                g_variant_unref (options);

                g_signal_emit (op, signals[DONE], 0, GTK_PRINT_OPERATION_RESULT_CANCEL);
                return;
        }

        /* FIXME: no "preview" for portal? */
        /* op->print_preview = (response == GTK_RESPONSE_APPLY); */
        op->print_preview = FALSE;

        v = g_variant_lookup_value (options, "settings", G_VARIANT_TYPE_VARDICT);
        settings = gtk_print_settings_new_from_gvariant (v);
        g_variant_unref (v);

        ev_print_operation_set_print_settings (op, settings);
        g_object_unref (settings);

        v = g_variant_lookup_value (options, "page-setup", G_VARIANT_TYPE_VARDICT);
        page_setup = gtk_page_setup_new_from_gvariant (v);
        g_variant_unref (v);

        ev_print_operation_set_default_page_setup (op, page_setup);
        g_object_unref (page_setup);

        g_variant_lookup (options, "token", "u", &export_portal->token);

        g_variant_unref (options);

        format = get_file_exporter_format (EV_FILE_EXPORTER (op->document),
                                           export->print_settings);

        if (!ev_print_operation_export_update_ranges (export)) {
                if (export->error == NULL)
                        g_set_error_literal (&export->error,
                                             GTK_PRINT_ERROR,
                                             GTK_PRINT_ERROR_GENERAL,
                                             _("Your print range selection does not include any pages"));

                g_signal_emit (op, signals[DONE], 0, GTK_PRINT_OPERATION_RESULT_ERROR);
                return;
        }

        if (!ev_print_operation_export_mkstemp (export, format)) {
                g_signal_emit (op, signals[DONE], 0, GTK_PRINT_OPERATION_RESULT_ERROR);
                return;
        }

        ev_print_operation_export_prepare (export, format);
}

static void
export_portal_subscribe_response (EvPrintOperationExportPortal *export_portal)
{
        export_portal->response_signal_id =
                g_dbus_connection_signal_subscribe (g_dbus_proxy_get_connection (G_DBUS_PROXY (export_portal->proxy)),
                                                    "org.freedesktop.portal.Desktop",
                                                    "org.freedesktop.portal.Request",
                                                    "Response",
                                                    export_portal->prepare_print_handle,
                                                    NULL /* cancellable */,
                                                    G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                    export_portal_request_response_cb,
                                                    g_object_ref (export_portal),
                                                    (GDestroyNotify) g_object_unref);
}

static void
export_portal_prepare_print_cb (GObject      *source,
                                GAsyncResult *result,
                                gpointer      data)
{
        EvPrintOperationExportPortal *export_portal = EV_PRINT_OPERATION_EXPORT_PORTAL (data);
        EvPrintOperationExport *export = EV_PRINT_OPERATION_EXPORT (data);
        GError *error = NULL;
        const char *handle = NULL;
        GVariant *rv;

        rv = g_dbus_proxy_call_finish (export_portal->proxy, result, &error);
        if (rv == NULL) {
                if (export->error == NULL)
                        g_propagate_error (&export->error, error);
                else
                        g_error_free (error);

                g_object_unref (export); /* added in g_dbus_proxy_call */
                return;
        }

        g_variant_get (rv, "(&o)", &handle);

        /* This happens on old portal versions only */
        if (!g_str_equal (export_portal->prepare_print_handle, handle)) {
                g_free (export_portal->prepare_print_handle);
                export_portal->prepare_print_handle = g_strdup (handle);

                export_portal_unsubscribe_response (export_portal);
                export_portal_subscribe_response (export_portal);
        }

        g_variant_unref (rv);
        g_object_unref (export); /* added in g_dbus_proxy_call */
}

static void
export_portal_create_proxy_cb (GObject *source,
                               GAsyncResult *result,
                               EvPrintOperationExportPortal *export_portal)
{
        EvPrintOperation *op = EV_PRINT_OPERATION (export_portal);
        EvPrintOperationExport *export = EV_PRINT_OPERATION_EXPORT (export_portal);
        GError *err = NULL;
        GVariantBuilder options_builder;
        GVariant *options_variant;
        GVariant *settings_variant;
        GVariant *setup_variant;
        char *token;
        char *sender;

        export_portal->proxy = g_dbus_proxy_new_for_bus_finish (result, &err);
        if (export_portal->proxy == NULL) {
                g_printerr("Error creating Print portal proxy: %s\n", err->message);
                g_propagate_error (&export->error, err);

                g_signal_emit (op, signals[DONE], 0, GTK_PRINT_OPERATION_RESULT_ERROR);

                g_object_unref (export_portal); /* added in create_proxy */
                return;
        }

        /* Now we can call PreparePrint */
        token = g_strdup_printf ("evince%u", (guint)g_random_int_range (0, G_MAXINT));

        sender = g_strdup (g_dbus_connection_get_unique_name (g_dbus_proxy_get_connection (export_portal->proxy)) + 1);
        sender = g_strdelimit (sender, ".", '_');

        export_portal->prepare_print_handle = g_strdup_printf ("/org/fredesktop/portal/desktop/request/%s/%s", sender, token);
        g_free (sender);

        export_portal_subscribe_response (export_portal);

#if 0
        /* FIXMEchpe: There is no way to specify the GtkPrintCapabilities
         * and the custom options. Needs to be fixed in the Portal!
         */
        capabilities = GTK_PRINT_CAPABILITY_PREVIEW |
                ev_file_exporter_get_capabilities (EV_FILE_EXPORTER (op->document));
        /* ... */
        /* install custom options... */
#endif

        g_variant_builder_init (&options_builder, G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add (&options_builder, "{sv}", "handle_token", g_variant_new_string (token));
        g_variant_builder_add (&options_builder, "{sv}", "modal", g_variant_new_boolean (TRUE));
        g_free (token);

        options_variant = g_variant_builder_end (&options_builder);

        if (export->print_settings) {
                settings_variant = gtk_print_settings_to_gvariant (export->print_settings);
        } else {
                GVariantBuilder builder2;
                g_variant_builder_init (&builder2, G_VARIANT_TYPE_VARDICT);
                settings_variant = g_variant_builder_end (&builder2);
        }

        if (export->page_setup) {
                setup_variant = gtk_page_setup_to_gvariant (export->page_setup);
        } else {
                GtkPageSetup *page_setup = gtk_page_setup_new ();
                setup_variant = gtk_page_setup_to_gvariant (page_setup);
                g_object_unref (page_setup);
        }

        g_dbus_proxy_call (export_portal->proxy,
                           "PreparePrint",
                           g_variant_new ("(ss@a{sv}@a{sv}@a{sv})",
                                          export_portal->parent_window_handle ? export_portal->parent_window_handle : "",
                                          _("Print"), /* title */
                                          settings_variant,
                                          setup_variant,
                                          options_variant),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL /* cancellable */,
                           export_portal_prepare_print_cb,
                           export_portal /* transfer the refcount added in create_proxy */);
}

static void
export_portal_create_proxy (EvPrintOperationExportPortal *export_portal)
{
        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  NULL,
                                  "org.freedesktop.portal.Desktop",
                                  "/org/freedesktop/portal/desktop",
                                  "org.freedesktop.portal.Print",
                                  NULL /* cancellable */,
                                  (GAsyncReadyCallback) export_portal_create_proxy_cb,
                                  g_object_ref (export_portal));
}

static void
export_portal_window_handle_exported_cb (GtkWindow  *window,
                                         const char *handle_str,
                                         gpointer    user_data)
{
        EvPrintOperationExportPortal *export_portal = EV_PRINT_OPERATION_EXPORT_PORTAL (user_data);

        export_portal->parent_window_handle = g_strdup (handle_str);
        export_portal_create_proxy (export_portal);
        g_object_unref (export_portal);
}


static void
export_portal_print_finished_cb (GObject                      *source,
                                 GAsyncResult                 *result,
                                 EvPrintOperationExportPortal *export_portal)
{
        EvPrintOperationExport *export = EV_PRINT_OPERATION_EXPORT (export_portal);
        EvPrintOperation *op = EV_PRINT_OPERATION (export_portal);
        GVariant *rv;
        GError *err = NULL;

        rv = g_dbus_proxy_call_finish (export_portal->proxy, result, &err);
        if (rv == NULL) {
                g_set_error_literal (&export->error,
                                     GTK_PRINT_ERROR,
                                     GTK_PRINT_ERROR_GENERAL,
                                     err->message);
                g_error_free (err);

                g_signal_emit (op, signals[DONE], 0, GTK_PRINT_OPERATION_RESULT_ERROR);
        } else {
                g_variant_unref (rv);
                g_signal_emit (op, signals[DONE], 0, GTK_PRINT_OPERATION_RESULT_APPLY);
        }

        ev_print_operation_export_clear_temp_file (export);

        ev_print_operation_export_run_next (export);

        g_object_unref (export_portal);
}

static gboolean
export_portal_print (EvPrintOperationExportPortal *export_portal,
                     GtkPrintSettings             *settings,
                     GError                      **error)
{
        EvPrintOperationExport *export = EV_PRINT_OPERATION_EXPORT (export_portal);
        GUnixFDList *fd_list;
        int idx;
        GError *err = NULL;
        GVariantBuilder builder;

        fd_list = g_unix_fd_list_new ();
        idx = g_unix_fd_list_append (fd_list, export->fd, &err);
        if (idx == -1) {
                g_propagate_error (error, err);
                return FALSE;
        }

        g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add (&builder, "{sv}",  "token", g_variant_new_uint32 (export_portal->token));

        /* FIXMEchpe: The portal will use the old print settings passed to PreparePrint,
         * not the cleaned-up ones from @settings !!! To fix this, needs to enhance
         * the portal.
         */
        g_dbus_proxy_call_with_unix_fd_list (export_portal->proxy,
                                             "Print",
                                             g_variant_new ("(ssh@a{sv})",
                                                            export_portal->parent_window_handle,
                                                            _("Print"), /* title */
                                                            idx,
                                                            g_variant_builder_end (&builder)),
                                             G_DBUS_CALL_FLAGS_NONE,
                                             -1,
                                             fd_list,
                                             NULL /* cancellable */,
                                             (GAsyncReadyCallback) export_portal_print_finished_cb,
                                             g_object_ref (export_portal));
        g_object_unref (fd_list);
        return TRUE;
}

static void
ev_print_operation_export_portal_run (EvPrintOperation *op,
                                      GtkWindow        *parent)
{
        EvPrintOperationExportPortal *export_portal = EV_PRINT_OPERATION_EXPORT_PORTAL (op);

        EV_PRINT_OPERATION_CLASS (ev_print_operation_export_portal_parent_class)->run (op, parent);

        if (parent != NULL &&
            gtk_widget_is_visible (GTK_WIDGET (parent)) &&
            ev_gtk_window_export_handle (parent,
                                         export_portal_window_handle_exported_cb,
                                         g_object_ref (export_portal)))
                return;

        export_portal_create_proxy (export_portal);
}

static void
ev_print_operation_export_portal_cancel (EvPrintOperation *op)
{
        //        EvPrintOperationExport *export = EV_PRINT_OPERATION_EXPORT (op);
        //        EvPrintOperationExportPortal *export_portal = EV_PRINT_OPERATION_EXPORT_PORTAL (op);

        EV_PRINT_OPERATION_CLASS (ev_print_operation_export_portal_parent_class)->cancel (op);
}

static gboolean
ev_print_operation_export_portal_run_previewer (EvPrintOperationExport *export,
                                                GtkPrintSettings       *settings,
                                                GError                **error)
{
        /* FIXMEchpe: This needs a new PrintPreview portal, that's just like Print
         * but calls a flatpack'd evince-previewer to do the previewing.
         */

        g_set_error_literal (error,
                             GTK_PRINT_ERROR,
                             GTK_PRINT_ERROR_GENERAL,
                             "Print preview not possible on portal");
        return FALSE;
}

static gboolean
ev_print_operation_export_portal_send_job (EvPrintOperationExport *export,
                                           GtkPrintSettings       *settings,
                                           GError                **error)
{
        EvPrintOperationExportPortal *export_portal = EV_PRINT_OPERATION_EXPORT_PORTAL (export);
        return export_portal_print (export_portal, settings, error);
}

static void
ev_print_operation_export_portal_init (EvPrintOperationExportPortal *export)
{
}

static void
ev_print_operation_export_portal_constructed (GObject *object)
{
        //        EvPrintOperation *op = EV_PRINT_OPERATION (object);
        //        EvPrintOperationExport *export = EV_PRINT_OPERATION_EXPORT (object);
        //        EvPrintOperationExportPortal *export_portal = EV_PRINT_OPERATION_EXPORT_PORTAL (object);

        G_OBJECT_CLASS (ev_print_operation_export_portal_parent_class)->constructed (object);
}

static void
ev_print_operation_export_portal_finalize (GObject *object)
{
        EvPrintOperationExportPortal *export_portal = EV_PRINT_OPERATION_EXPORT_PORTAL (object);

        export_portal_unsubscribe_response (export_portal);

        g_clear_object (&export_portal->proxy);

        g_free (export_portal->parent_window_handle);
        g_free (export_portal->prepare_print_handle);

        G_OBJECT_CLASS (ev_print_operation_export_portal_parent_class)->finalize (object);
}

static void
ev_print_operation_export_portal_class_init (EvPrintOperationExportPortalClass *klass)
{
        GObjectClass          *g_object_class = G_OBJECT_CLASS (klass);
        EvPrintOperationClass *ev_print_op_class = EV_PRINT_OPERATION_CLASS (klass);
        EvPrintOperationExportClass *ev_print_op_ex_class = EV_PRINT_OPERATION_EXPORT_CLASS (klass);

        ev_print_op_class->run = ev_print_operation_export_portal_run;
        ev_print_op_class->cancel = ev_print_operation_export_portal_cancel;

        ev_print_op_ex_class->send_job = ev_print_operation_export_portal_send_job;
        ev_print_op_ex_class->run_previewer = ev_print_operation_export_portal_run_previewer;

        g_object_class->constructed = ev_print_operation_export_portal_constructed;
        g_object_class->finalize = ev_print_operation_export_portal_finalize;
}

#endif /* PORTAL_ENABLED */

/* Export with unix print dialogue */

#if GTKUNIXPRINT_ENABLED

#include <gtk/gtkunixprint.h>

#define EV_TYPE_PRINT_OPERATION_EXPORT_UNIX            (ev_print_operation_export_unix_get_type())
#define EV_PRINT_OPERATION_EXPORT_UNIX(object)         (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_PRINT_OPERATION_EXPORT_UNIX, EvPrintOperationExportUnix))
#define EV_PRINT_OPERATION_EXPORT_UNIX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_PRINT_OPERATION_EXPORT_UNIX, EvPrintOperationExportUnixClass))
#define EV_IS_PRINT_OPERATION_EXPORT_UNIX(object)      (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_PRINT_OPERATION_EXPORT_UNIX))
#define EV_IS_PRINT_OPERATION_EXPORT_UNIX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_PRINT_OPERATION_EXPORT_UNIX))
#define EV_PRINT_OPERATION_EXPORT_UNIX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_PRINT_OPERATION_EXPORT_UNIX, EvPrintOperationExportUnixClass))

typedef struct _EvPrintOperationExportUnix      EvPrintOperationExportUnix;
typedef struct _EvPrintOperationExportUnixClass EvPrintOperationExportUnixClass;

static GType    ev_print_operation_export_unix_get_type (void) G_GNUC_CONST;

struct _EvPrintOperationExportUnix {
	EvPrintOperationExport parent;

	GtkWindow *parent_window;

	GtkPrinter *printer;

	/* Context */
};

struct _EvPrintOperationExportUnixClass {
	EvPrintOperationExportClass parent_class;
};

G_DEFINE_TYPE (EvPrintOperationExportUnix, ev_print_operation_export_unix, EV_TYPE_PRINT_OPERATION_EXPORT)

static void
ev_print_operation_export_unix_set_printer (EvPrintOperationExportUnix *export,
                                            GtkPrinter                 *printer)
{
	if (printer == export->printer)
		return;

	g_object_ref (printer);
	if (export->printer)
		g_object_unref (export->printer);
	export->printer = printer;
}

static void
export_unix_print_dialog_response_cb (GtkDialog              *dialog,
                                      gint                    response,
                                      EvPrintOperationExport *export)
{
	EvPrintOperationExportUnix *export_unix = EV_PRINT_OPERATION_EXPORT_UNIX (export);
	EvPrintOperation     *op = EV_PRINT_OPERATION (export);
	GtkPrintSettings     *print_settings;
	GtkPageSetup         *page_setup;
	GtkPrinter           *printer;
	EvFileExporterFormat  format;

	if (response != GTK_RESPONSE_OK &&
	    response != GTK_RESPONSE_APPLY) {
		gtk_window_destroy (GTK_WINDOW (dialog));
		g_signal_emit (op, signals[DONE], 0, GTK_PRINT_OPERATION_RESULT_CANCEL);

		return;
	}

	op->print_preview = (response == GTK_RESPONSE_APPLY);

	printer = gtk_print_unix_dialog_get_selected_printer (GTK_PRINT_UNIX_DIALOG (dialog));
	ev_print_operation_export_unix_set_printer (export_unix, printer);

	print_settings = gtk_print_unix_dialog_get_settings (GTK_PRINT_UNIX_DIALOG (dialog));
	ev_print_operation_export_set_print_settings (op, print_settings);

	page_setup = gtk_print_unix_dialog_get_page_setup (GTK_PRINT_UNIX_DIALOG (dialog));
	ev_print_operation_export_set_default_page_setup (op, page_setup);

	format = get_file_exporter_format (EV_FILE_EXPORTER (op->document),
                                           print_settings);

	if ((format == EV_FILE_FORMAT_PS && !gtk_printer_accepts_ps (export_unix->printer)) ||
	    (format == EV_FILE_FORMAT_PDF && !gtk_printer_accepts_pdf (export_unix->printer))) {
		gtk_window_destroy (GTK_WINDOW (dialog));

		g_set_error_literal (&export->error,
                                     GTK_PRINT_ERROR,
                                     GTK_PRINT_ERROR_GENERAL,
                                     _("Requested format is not supported by this printer."));
		g_signal_emit (op, signals[DONE], 0, GTK_PRINT_OPERATION_RESULT_ERROR);

		return;
	}

        if (!ev_print_operation_export_mkstemp (export, format)) {
		gtk_window_destroy (GTK_WINDOW (dialog));

		g_signal_emit (op, signals[DONE], 0, GTK_PRINT_OPERATION_RESULT_ERROR);
		return;
	}

        /* FIXMEchpe (why) is this necessary? */
	export->current_page = gtk_print_unix_dialog_get_current_page (GTK_PRINT_UNIX_DIALOG (dialog));

        if (!ev_print_operation_export_update_ranges (export)) {
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
				  G_CALLBACK (gtk_window_destroy),
				  NULL);
		gtk_widget_set_visible (message_dialog, TRUE);

		return;
	}

	gtk_window_destroy (GTK_WINDOW (dialog));

        ev_print_operation_export_prepare (export, format);
}

static void
export_unix_print_job_finished_cb (GtkPrintJob            *print_job,
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
ev_print_operation_export_unix_run (EvPrintOperation *op,
                                    GtkWindow        *parent)
{
	EvPrintOperationExport *export = EV_PRINT_OPERATION_EXPORT (op);
	EvPrintOperationExportUnix *export_unix = EV_PRINT_OPERATION_EXPORT_UNIX (op);
	GtkWidget              *dialog;
	GtkPrintCapabilities    capabilities;

        EV_PRINT_OPERATION_CLASS (ev_print_operation_export_unix_parent_class)->run (op, parent);

	export_unix->parent_window = parent;

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
			  G_CALLBACK (export_unix_print_dialog_response_cb),
			  export);

	gtk_window_present (GTK_WINDOW (dialog));
}

static gboolean
ev_print_operation_export_unix_run_previewer (EvPrintOperationExport *export,
                                              GtkPrintSettings       *settings,
                                              GError                **error)
{
        EvPrintOperation *op = EV_PRINT_OPERATION (export);
        EvPrintOperationExportUnix *export_unix = EV_PRINT_OPERATION_EXPORT_UNIX (export);
        GKeyFile *key_file;
        gchar    *data = NULL;
        gsize     data_len;
        gchar    *print_settings_file = NULL;
        GError   *err = NULL;

        key_file = g_key_file_new ();

        gtk_print_settings_to_key_file (settings, key_file, NULL);
        gtk_page_setup_to_key_file (export->page_setup, key_file, NULL);
        g_key_file_set_string (key_file, "Print Job", "title", export->job_name);

        data = g_key_file_to_data (key_file, &data_len, &err);
        if (data) {
                gint fd;

                fd = g_file_open_tmp ("print-settingsXXXXXX", &print_settings_file, &err);
                if (!error)
                        g_file_set_contents (print_settings_file, data, data_len, &err);
                close (fd);

                g_free (data);
        }

        g_key_file_free (key_file);

        if (!err) {
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

                app = g_app_info_create_from_commandline (cmd, NULL, 0, &err);

                if (app != NULL) {
                        ctx = gdk_display_get_app_launch_context (gtk_widget_get_display (GTK_WIDGET (export_unix->parent_window)));

                        g_app_info_launch (app, NULL, G_APP_LAUNCH_CONTEXT (ctx), &err);

                        g_object_unref (app);
                        g_object_unref (ctx);
                }

                g_free (cmd);
        }

        if (err) {
                if (print_settings_file)
                        g_unlink (print_settings_file);
                g_free (print_settings_file);

                g_propagate_error (error, err);
        } else {
                g_signal_emit (op, signals[DONE], 0, GTK_PRINT_OPERATION_RESULT_APPLY);
                /* temp_file will be deleted by the previewer */

                ev_print_operation_export_run_next (export);
        }

        return err != NULL;
}

static gboolean
ev_print_operation_export_unix_send_job (EvPrintOperationExport *export,
                                         GtkPrintSettings       *settings,
                                         GError                **error)
{
        EvPrintOperationExportUnix *export_unix = EV_PRINT_OPERATION_EXPORT_UNIX (export);
        GtkPrintJob *job;
        GError *err = NULL;

        job = gtk_print_job_new (export->job_name,
                                 export_unix->printer,
                                 settings,
                                 export->page_setup);
        gtk_print_job_set_source_file (job, export->temp_file, &err);
        if (err) {
                g_propagate_error (error, err);
        } else {
                gtk_print_job_send (job,
                                    (GtkPrintJobCompleteFunc) export_unix_print_job_finished_cb,
                                    g_object_ref (export),
                                    (GDestroyNotify) g_object_unref);
        }

        return err != NULL;
}

static void
ev_print_operation_export_unix_finalize (GObject *object)
{
	EvPrintOperationExportUnix *export_unix = EV_PRINT_OPERATION_EXPORT_UNIX (object);

	g_clear_object (&export_unix->printer);

	G_OBJECT_CLASS (ev_print_operation_export_unix_parent_class)->finalize (object);
}

static void
ev_print_operation_export_unix_init (EvPrintOperationExportUnix *export)
{
}

static void
ev_print_operation_export_unix_class_init (EvPrintOperationExportUnixClass *klass)
{
	GObjectClass          *g_object_class = G_OBJECT_CLASS (klass);
	EvPrintOperationClass *ev_print_op_class = EV_PRINT_OPERATION_CLASS (klass);
	EvPrintOperationExportClass *ev_print_op_ex_class = EV_PRINT_OPERATION_EXPORT_CLASS (klass);

	ev_print_op_class->run = ev_print_operation_export_unix_run;
        //	ev_print_op_class->cancel = ev_print_operation_export_unix_cancel;

        ev_print_op_ex_class->send_job = ev_print_operation_export_unix_send_job;
        ev_print_op_ex_class->run_previewer = ev_print_operation_export_unix_run_previewer;

	g_object_class->finalize = ev_print_operation_export_unix_finalize;
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

#define EV_PRINT_SETTING_PAGE_SCALE   "evince-print-setting-page-scale"
#define EV_PRINT_SETTING_AUTOROTATE   "evince-print-setting-page-autorotate"
#define EV_PRINT_SETTING_PAGE_SIZE    "evince-print-setting-page-size"
#define EV_PRINT_SETTING_DRAW_BORDERS "evince-print-setting-page-draw-borders"

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
	GtkWidget   *borders_button;
	gboolean     draw_borders;
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
		return G_SOURCE_CONTINUE;

        gtk_print_operation_draw_page_finish (print->op);

	return G_SOURCE_REMOVE;
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
		gdouble paper_width, paper_height;
		gboolean page_is_landscape, paper_is_landscape;

		GtkPaperSize *psize = gtk_page_setup_get_paper_size (setup);
		paper_width = gtk_paper_size_get_width (psize, GTK_UNIT_POINTS);
		paper_height = gtk_paper_size_get_height (psize, GTK_UNIT_POINTS);

		paper_is_landscape = paper_width > paper_height;
		page_is_landscape = width > height;

		if (page_is_landscape != paper_is_landscape)
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
ev_print_operation_print_get_scaled_page_size (EvPrintOperationPrint *print,
                                               gint                   page,
                                               gdouble               *width,
                                               gdouble               *height,
					       gdouble               *manual_scale)
{
        GtkPrintSettings *settings;

        ev_document_get_page_size (EV_PRINT_OPERATION (print)->document,
                                   page, width, height);

        settings = gtk_print_operation_get_print_settings (print->op);
        *manual_scale = gtk_print_settings_get_scale (settings) / 100.0;
        if (*manual_scale == 1.0)
                return;

        *width *= *manual_scale;
        *height *= *manual_scale;
}

static void
ev_print_operation_print_draw_page (EvPrintOperationPrint *print,
				    GtkPrintContext       *context,
				    gint                   page)
{
	EvPrintOperation *op = EV_PRINT_OPERATION (print);
	cairo_t          *cr;
	gdouble           cr_width, cr_height;
	gdouble           width, height, manual_scale, scale;
	gdouble           x_scale, y_scale;
        gdouble           x_offset, y_offset;
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
        ev_print_operation_print_get_scaled_page_size (print, page, &width, &height, &manual_scale);

	if (print->page_scale == EV_SCALE_NONE) {
		/* Center document page on the printed page */
		if (print->autorotate) {
                        x_offset = (cr_width - width) / (2 * manual_scale);
                        y_offset = (cr_height - height) / (2 * manual_scale);
                        cairo_translate (cr, x_offset, y_offset);
                }
	} else {
		_print_context_get_hard_margins (context, &top, &bottom, &left, &right);

		x_scale = (cr_width - left - right) / width;
		y_scale = (cr_height - top - bottom) / height;
                scale = MIN (x_scale, y_scale);

                /* Ignore scale > 1 when shrinking to printable area */
                if (scale > 1.0 && print->page_scale == EV_SCALE_SHRINK_TO_PRINTABLE_AREA)
                        scale = 1.0;

		if (print->autorotate) {
                        x_offset = (cr_width - scale * width) / (2 * manual_scale);
                        y_offset = (cr_height - scale * height) / (2 * manual_scale);
                        cairo_translate (cr, x_offset, y_offset);

			/* Ensure document page is within the margins. The
			 * scale guarantees the document will fit in the
			 * margins so we just need to check each side and
			 * if it overhangs the margin, translate it to the
                         * margin. */
			if (x_offset < left)
				cairo_translate (cr, left - x_offset, 0);

			if (x_offset < right)
				cairo_translate (cr, -(right - x_offset), 0);

			if (y_offset < top)
				cairo_translate (cr, 0, top - y_offset);

			if (y_offset < bottom)
				cairo_translate (cr, 0, -(bottom - y_offset));
		} else {
			cairo_translate (cr, left, top);
		}

		if (print->page_scale == EV_SCALE_FIT_TO_PRINTABLE_AREA || scale < 1.0) {
			cairo_scale (cr, scale, scale);
		}
	}

	if (print->draw_borders) {
		cairo_set_line_width (cr, 1);
		cairo_set_source_rgb (cr, 0., 0., 0.);
		cairo_rectangle (cr, 0, 0,
				 gtk_print_context_get_width (context),
				 gtk_print_context_get_height (context));
		cairo_stroke (cr);
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
	gboolean          draw_borders;

	settings = gtk_print_operation_get_print_settings (print->op);
	page_scale = gtk_print_settings_get_int_with_default (settings, EV_PRINT_SETTING_PAGE_SCALE, 1);
	autorotate = gtk_print_settings_has_key (settings, EV_PRINT_SETTING_AUTOROTATE) ?
		gtk_print_settings_get_bool (settings, EV_PRINT_SETTING_AUTOROTATE) :
		TRUE;
	use_source_size = gtk_print_settings_get_bool (settings, EV_PRINT_SETTING_PAGE_SIZE);
	draw_borders = gtk_print_settings_has_key (settings, EV_PRINT_SETTING_DRAW_BORDERS) ?
		gtk_print_settings_get_bool (settings, EV_PRINT_SETTING_DRAW_BORDERS) :
		FALSE;

	grid = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
	gtk_widget_set_margin_top (grid, 12);
	gtk_widget_set_margin_bottom (grid, 12);
	gtk_widget_set_margin_start (grid, 12);
	gtk_widget_set_margin_end (grid, 12);

	label = gtk_label_new (_("Page Scaling:"));
	gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
	gtk_widget_set_visible (label, TRUE);

	print->scale_combo = gtk_combo_box_text_new ();
	/* translators: Value for 'Page Scaling:' to not scale the document pages on printing */
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (print->scale_combo), _("None"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (print->scale_combo), _("Shrink to Printable Area"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (print->scale_combo), _("Fit to Printable Area"));
	gtk_combo_box_set_active (GTK_COMBO_BOX (print->scale_combo), page_scale);
	gtk_widget_set_tooltip_text (print->scale_combo,
		_("Scale document pages to fit the selected printer page. Select from one of the following:\n"
		  "\n"
		  "• “None”: No page scaling is performed.\n"
		  "\n"
		  "• “Shrink to Printable Area”: Document pages larger than the printable area"
		  " are reduced to fit the printable area of the printer page.\n"
		  "\n"
		  "• “Fit to Printable Area”: Document pages are enlarged or reduced as"
		  " required to fit the printable area of the printer page.\n"));
	gtk_grid_attach (GTK_GRID (grid), print->scale_combo, 1, 0, 1, 1);
	gtk_widget_set_visible (print->scale_combo, TRUE);

	print->autorotate_button = gtk_check_button_new_with_label (_("Auto Rotate and Center"));
	gtk_check_button_set_active (GTK_CHECK_BUTTON (print->autorotate_button), autorotate);
	gtk_widget_set_tooltip_text (print->autorotate_button,
		_("Rotate printer page orientation of each page to match orientation of each document page. "
		  "Document pages will be centered within the printer page."));
	gtk_grid_attach (GTK_GRID (grid), print->autorotate_button, 0, 1, 2, 1);
	gtk_widget_set_visible (print->autorotate_button, TRUE);

	print->source_button = gtk_check_button_new_with_label (_("Select page size using document page size"));
	gtk_check_button_set_active (GTK_CHECK_BUTTON (print->source_button), use_source_size);
	gtk_widget_set_tooltip_text (print->source_button, _("When enabled, each page will be printed on "
							     "the same size paper as the document page."));
	gtk_grid_attach (GTK_GRID (grid), print->source_button, 0, 2, 2, 1);
	gtk_widget_set_visible (print->source_button, TRUE);

	print->borders_button = gtk_check_button_new_with_label (_("Draw border around pages"));
	gtk_check_button_set_active (GTK_CHECK_BUTTON (print->borders_button), draw_borders);
	gtk_widget_set_tooltip_text (print->borders_button, _("When enabled, a border will be drawn "
							     "around each page."));
	gtk_grid_attach (GTK_GRID (grid), print->borders_button, 0, 3, 2, 1);
	gtk_widget_set_visible (print->borders_button, TRUE);

	return G_OBJECT (grid);
}

static void
ev_print_operation_print_custom_widget_apply (EvPrintOperationPrint *print,
					      GtkPrintContext       *context)
{
	GtkPrintSettings *settings;

	print->page_scale = gtk_combo_box_get_active (GTK_COMBO_BOX (print->scale_combo));
	print->autorotate = gtk_check_button_get_active (GTK_CHECK_BUTTON (print->autorotate_button));
	print->use_source_size = gtk_check_button_get_active (GTK_CHECK_BUTTON (print->source_button));
	print->draw_borders = gtk_check_button_get_active (GTK_CHECK_BUTTON (print->borders_button));
	settings = gtk_print_operation_get_print_settings (print->op);
	gtk_print_settings_set_int (settings, EV_PRINT_SETTING_PAGE_SCALE, print->page_scale);
	gtk_print_settings_set_bool (settings, EV_PRINT_SETTING_AUTOROTATE, print->autorotate);
	gtk_print_settings_set_bool (settings, EV_PRINT_SETTING_PAGE_SIZE, print->use_source_size);
	gtk_print_settings_set_bool (settings, EV_PRINT_SETTING_PAGE_SIZE, print->draw_borders);
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

	g_clear_object (&print->op);
	g_clear_pointer (&print->job_name, g_free);

	if (print->job_print) {
		if (!ev_job_is_finished (print->job_print))
			ev_job_cancel (print->job_print);
		g_signal_handlers_disconnect_by_func (print->job_print,
						      print_job_finished,
						      print);
		g_signal_handlers_disconnect_by_func (print->job_print,
						      print_job_cancelled,
						      print);
		g_clear_object (&print->job_print);
	}

	G_OBJECT_CLASS (ev_print_operation_print_parent_class)->finalize (object);

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

/* Factory functions */

static GType
ev_print_operation_get_gtype_for_document (EvDocument *document)
{
        GType type = G_TYPE_INVALID;
        const char *env;

        /* Allow to override the selection by an env var */
        env = g_getenv ("EV_PRINT");

        if (EV_IS_DOCUMENT_PRINT (document) && g_strcmp0 (env, "export") != 0) {
                type = EV_TYPE_PRINT_OPERATION_PRINT;
        } else if (EV_IS_FILE_EXPORTER (document)) {
                if (ev_should_use_portal ()) {
#ifdef PORTAL_ENABLED
                        type = EV_TYPE_PRINT_OPERATION_EXPORT_PORTAL;
#endif
                } else {
#if GTKUNIXPRINT_ENABLED
                        type = EV_TYPE_PRINT_OPERATION_EXPORT_UNIX;
#endif
                }
        }

        return type;
}

gboolean
ev_print_operation_exists_for_document (EvDocument *document)
{
        return ev_print_operation_get_gtype_for_document (document) != G_TYPE_INVALID;
}

/* Factory method */
EvPrintOperation *
ev_print_operation_new (EvDocument *document)
{
        GType type;

        type = ev_print_operation_get_gtype_for_document (document);
        if (type == G_TYPE_INVALID)
                return NULL;

        return EV_PRINT_OPERATION (g_object_new (type, "document", document, NULL));
}
