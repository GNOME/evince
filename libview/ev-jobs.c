/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2008 Carlos Garcia Campos <carlosgc@gnome.org>
 *  Copyright (C) 2005 Red Hat, Inc
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

#include "cairo.h"
#include "ev-jobs.h"
#include "ev-document-links.h"
#include "ev-document-images.h"
#include "ev-document-forms.h"
#include "ev-file-exporter.h"
#include "ev-document-factory.h"
#include "ev-document-misc.h"
#include "ev-file-helpers.h"
#include "ev-document-fonts.h"
#include "ev-document-security.h"
#include "ev-document-find.h"
#include "ev-document-layers.h"
#include "ev-document-print.h"
#include "ev-document-annotations.h"
#include "ev-document-attachments.h"
#include "ev-document-media.h"
#include "ev-document-text.h"
#include "ev-debug.h"

#include <errno.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <unistd.h>
#include <fcntl.h>

typedef struct _EvJobLoadStreamPrivate EvJobLoadStreamPrivate;
struct _EvJobLoadStreamPrivate
{
        char *mime_type;
};

static void ev_job_init                       (EvJob                    *job);
static void ev_job_class_init                 (EvJobClass               *class);
static void ev_job_links_init                 (EvJobLinks               *job);
static void ev_job_links_class_init           (EvJobLinksClass          *class);
static void ev_job_attachments_init           (EvJobAttachments         *job);
static void ev_job_attachments_class_init     (EvJobAttachmentsClass    *class);
static void ev_job_annots_init                (EvJobAnnots              *job);
static void ev_job_annots_class_init          (EvJobAnnotsClass         *class);
static void ev_job_render_cairo_init          (EvJobRenderCairo         *job);
static void ev_job_render_cairo_class_init    (EvJobRenderCairoClass    *class);
static void ev_job_render_texture_init        (EvJobRenderTexture         *job);
static void ev_job_render_texture_class_init  (EvJobRenderTextureClass    *class);
static void ev_job_page_data_init             (EvJobPageData            *job);
static void ev_job_page_data_class_init       (EvJobPageDataClass       *class);
static void ev_job_thumbnail_cairo_init       (EvJobThumbnailCairo      *job);
static void ev_job_thumbnail_cairo_class_init (EvJobThumbnailCairoClass *class);
static void ev_job_thumbnail_texture_init       (EvJobThumbnailTexture      *job);
static void ev_job_thumbnail_texture_class_init (EvJobThumbnailTextureClass *class);
static void ev_job_load_init                  (EvJobLoad                *job);
static void ev_job_load_class_init            (EvJobLoadClass           *class);
static void ev_job_save_init                  (EvJobSave                *job);
static void ev_job_save_class_init            (EvJobSaveClass           *class);
static void ev_job_find_init                  (EvJobFind                *job);
static void ev_job_find_class_init            (EvJobFindClass           *class);
static void ev_job_layers_init                (EvJobLayers              *job);
static void ev_job_layers_class_init          (EvJobLayersClass         *class);
static void ev_job_export_init                (EvJobExport              *job);
static void ev_job_export_class_init          (EvJobExportClass         *class);
static void ev_job_print_init                 (EvJobPrint               *job);
static void ev_job_print_class_init           (EvJobPrintClass          *class);

enum {
	CANCELLED,
	FINISHED,
	LAST_SIGNAL
};

enum {
	FIND_UPDATED,
	FIND_LAST_SIGNAL
};

static guint job_signals[LAST_SIGNAL] = { 0 };
static guint job_find_signals[FIND_LAST_SIGNAL] = { 0 };

G_DEFINE_ABSTRACT_TYPE (EvJob, ev_job, G_TYPE_OBJECT)
G_DEFINE_TYPE (EvJobLinks, ev_job_links, EV_TYPE_JOB)
G_DEFINE_TYPE (EvJobAttachments, ev_job_attachments, EV_TYPE_JOB)
G_DEFINE_TYPE (EvJobAnnots, ev_job_annots, EV_TYPE_JOB)
G_DEFINE_TYPE (EvJobRenderCairo, ev_job_render_cairo, EV_TYPE_JOB)
G_DEFINE_TYPE (EvJobRenderTexture, ev_job_render_texture, EV_TYPE_JOB)
G_DEFINE_TYPE (EvJobPageData, ev_job_page_data, EV_TYPE_JOB)
G_DEFINE_TYPE (EvJobThumbnailCairo, ev_job_thumbnail_cairo, EV_TYPE_JOB)
G_DEFINE_TYPE (EvJobThumbnailTexture, ev_job_thumbnail_texture, EV_TYPE_JOB)
G_DEFINE_TYPE (EvJobFonts, ev_job_fonts, EV_TYPE_JOB)
G_DEFINE_TYPE (EvJobLoad, ev_job_load, EV_TYPE_JOB)
G_DEFINE_TYPE_WITH_PRIVATE (EvJobLoadStream, ev_job_load_stream, EV_TYPE_JOB)
G_DEFINE_TYPE (EvJobLoadGFile, ev_job_load_gfile, EV_TYPE_JOB)
G_DEFINE_TYPE (EvJobLoadFd, ev_job_load_fd, EV_TYPE_JOB)
G_DEFINE_TYPE (EvJobSave, ev_job_save, EV_TYPE_JOB)
G_DEFINE_TYPE (EvJobFind, ev_job_find, EV_TYPE_JOB)
G_DEFINE_TYPE (EvJobLayers, ev_job_layers, EV_TYPE_JOB)
G_DEFINE_TYPE (EvJobExport, ev_job_export, EV_TYPE_JOB)
G_DEFINE_TYPE (EvJobPrint, ev_job_print, EV_TYPE_JOB)

/* EvJob */
static void
ev_job_init (EvJob *job)
{
	job->cancellable = g_cancellable_new ();
}

static void
ev_job_dispose (GObject *object)
{
	EvJob *job;

	job = EV_JOB (object);

	g_clear_object (&job->document);
	g_clear_object (&job->cancellable);
	g_clear_error (&job->error);

	(* G_OBJECT_CLASS (ev_job_parent_class)->dispose) (object);
}

static void
ev_job_class_init (EvJobClass *class)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (class);

	oclass->dispose = ev_job_dispose;

	job_signals[CANCELLED] =
		g_signal_new ("cancelled",
			      EV_TYPE_JOB,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EvJobClass, cancelled),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	job_signals [FINISHED] =
		g_signal_new ("finished",
			      EV_TYPE_JOB,
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EvJobClass, finished),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

static gboolean
emit_finished (EvJob *job)
{
	ev_debug_message (DEBUG_JOBS, "%s (%p)", EV_GET_TYPE_NAME (job), job);

	job->idle_finished_id = 0;

	if (job->cancelled)
		ev_debug_message (DEBUG_JOBS, "%s (%p) job was cancelled, do not emit finished", EV_GET_TYPE_NAME (job), job);
	else
		g_signal_emit (job, job_signals[FINISHED], 0);

	return G_SOURCE_REMOVE;
}

static void
ev_job_emit_finished (EvJob *job)
{
	ev_debug_message (DEBUG_JOBS, "%s (%p)", EV_GET_TYPE_NAME (job), job);

	if (g_cancellable_is_cancelled (job->cancellable)) {
		ev_debug_message (DEBUG_JOBS, "%s (%p) job was cancelled, returning", EV_GET_TYPE_NAME (job), job);
		return;
	}

	job->finished = TRUE;

	if (job->run_mode == EV_JOB_RUN_THREAD) {
		job->idle_finished_id =
			g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
					 (GSourceFunc)emit_finished,
					 g_object_ref (job),
					 (GDestroyNotify)g_object_unref);
	} else {
		g_signal_emit (job, job_signals[FINISHED], 0);
	}
}

gboolean
ev_job_run (EvJob *job)
{
	EvJobClass *class = EV_JOB_GET_CLASS (job);

	return class->run (job);
}

void
ev_job_cancel (EvJob *job)
{
	if (job->cancelled)
		return;

	ev_debug_message (DEBUG_JOBS, "job %s (%p) cancelled", EV_GET_TYPE_NAME (job), job);

	/* This should never be called from a thread */
	job->cancelled = TRUE;
	g_cancellable_cancel (job->cancellable);

        if (job->finished && job->idle_finished_id == 0)
                return;

	g_signal_emit (job, job_signals[CANCELLED], 0);
}

void
ev_job_failed (EvJob       *job,
	       GQuark       domain,
	       gint         code,
	       const gchar *format,
	       ...)
{
	va_list args;
	gchar  *message;

	if (job->failed || job->finished)
		return;

	ev_debug_message (DEBUG_JOBS, "job %s (%p) failed", EV_GET_TYPE_NAME (job), job);

	job->failed = TRUE;

	va_start (args, format);
	message = g_strdup_vprintf (format, args);
	va_end (args);

	job->error = g_error_new_literal (domain, code, message);
	g_free (message);

	ev_job_emit_finished (job);
}

/**
 * ev_job_failed_from_error: (rename-to ev_job_failed)
 * @job: an #EvJob
 * @error: a #GError
 */
void
ev_job_failed_from_error (EvJob  *job,
			  GError *error)
{
	if (job->failed || job->finished)
		return;

	ev_debug_message (DEBUG_JOBS, "job %s (%p) failed", EV_GET_TYPE_NAME (job), job);

	job->failed = TRUE;
	job->error = g_error_copy (error);

	ev_job_emit_finished (job);
}

void
ev_job_succeeded (EvJob *job)
{
	if (job->finished)
		return;

	ev_debug_message (DEBUG_JOBS, "job %s (%p) succeeded", EV_GET_TYPE_NAME (job), job);

	job->failed = FALSE;
	ev_job_emit_finished (job);
}

gboolean
ev_job_is_finished (EvJob *job)
{
	return job->finished;
}

gboolean
ev_job_is_failed (EvJob *job)
{
	return job->failed;
}

EvJobRunMode
ev_job_get_run_mode (EvJob *job)
{
	return job->run_mode;
}

void
ev_job_set_run_mode (EvJob       *job,
		     EvJobRunMode run_mode)
{
	job->run_mode = run_mode;
}


/**
 * ev_job_get_document:
 * @job: an #EvJob
 *
 * Returns: (transfer none): The #EvDocument of this job.
 */
EvDocument *
ev_job_get_document (EvJob	 *job)
{
	g_return_val_if_fail (EV_IS_JOB (job), NULL);

	return job->document;
}

/* EvJobLinks */
static void
ev_job_links_init (EvJobLinks *job)
{
	EV_JOB (job)->run_mode = EV_JOB_RUN_THREAD;
}

static void
ev_job_links_dispose (GObject *object)
{
	EvJobLinks *job;

	ev_debug_message (DEBUG_JOBS, NULL);

	job = EV_JOB_LINKS (object);

	g_clear_object (&job->model);

	(* G_OBJECT_CLASS (ev_job_links_parent_class)->dispose) (object);
}

static gboolean
fill_page_labels (GtkTreeModel   *tree_model,
		  GtkTreePath    *path,
		  GtkTreeIter    *iter,
		  EvJob          *job)
{
	EvDocumentLinks *document_links;
	EvLink          *link;
	gchar           *page_label;

	gtk_tree_model_get (tree_model, iter,
			    EV_DOCUMENT_LINKS_COLUMN_LINK, &link,
			    -1);

	if (!link)
		return FALSE;

	document_links = EV_DOCUMENT_LINKS (job->document);
	page_label = ev_document_links_get_link_page_label (document_links, link);
	if (!page_label)
		return FALSE;

	gtk_tree_store_set (GTK_TREE_STORE (tree_model), iter,
			    EV_DOCUMENT_LINKS_COLUMN_PAGE_LABEL, page_label,
			    -1);

	g_free (page_label);
	g_object_unref (link);

	return FALSE;
}

static gboolean
ev_job_links_run (EvJob *job)
{
	EvJobLinks *job_links = EV_JOB_LINKS (job);

	ev_debug_message (DEBUG_JOBS, NULL);
	EV_PROFILER_START (EV_GET_TYPE_NAME (job));

	ev_document_doc_mutex_lock ();
	job_links->model = ev_document_links_get_links_model (EV_DOCUMENT_LINKS (job->document));
	ev_document_doc_mutex_unlock ();

	gtk_tree_model_foreach (job_links->model, (GtkTreeModelForeachFunc)fill_page_labels, job);

	ev_job_succeeded (job);

	EV_PROFILER_STOP ();
	return FALSE;
}

static void
ev_job_links_class_init (EvJobLinksClass *class)
{
	GObjectClass *oclass = G_OBJECT_CLASS (class);
	EvJobClass   *job_class = EV_JOB_CLASS (class);

	oclass->dispose = ev_job_links_dispose;
	job_class->run = ev_job_links_run;
}

EvJob *
ev_job_links_new (EvDocument *document)
{
	EvJob *job;

	ev_debug_message (DEBUG_JOBS, NULL);

	job = g_object_new (EV_TYPE_JOB_LINKS, NULL);
	job->document = g_object_ref (document);

	return job;
}

/**
 * ev_job_links_get_model:
 * @job: #EvJobLinks
 *
 * Get a #GtkTreeModel loaded with the links
 *
 * Return value: (transfer none): The #GtkTreeModel loaded
 *
 * Since: 3.6
 */
GtkTreeModel *
ev_job_links_get_model (EvJobLinks *job)
{
	return job->model;
}

/* EvJobAttachments */
static void
ev_job_attachments_init (EvJobAttachments *job)
{
	EV_JOB (job)->run_mode = EV_JOB_RUN_THREAD;
}

static void
ev_job_attachments_dispose (GObject *object)
{
	EvJobAttachments *job;

	ev_debug_message (DEBUG_JOBS, NULL);

	job = EV_JOB_ATTACHMENTS (object);

	g_list_free_full (g_steal_pointer (&job->attachments), g_object_unref);

	(* G_OBJECT_CLASS (ev_job_attachments_parent_class)->dispose) (object);
}

static gboolean
ev_job_attachments_run (EvJob *job)
{
	EvJobAttachments *job_attachments = EV_JOB_ATTACHMENTS (job);

	ev_debug_message (DEBUG_JOBS, NULL);
	EV_PROFILER_START (EV_GET_TYPE_NAME (job));

	ev_document_doc_mutex_lock ();
	job_attachments->attachments =
		ev_document_attachments_get_attachments (EV_DOCUMENT_ATTACHMENTS (job->document));
	ev_document_doc_mutex_unlock ();

	ev_job_succeeded (job);

	EV_PROFILER_STOP ();
	return FALSE;
}

static void
ev_job_attachments_class_init (EvJobAttachmentsClass *class)
{
	GObjectClass *oclass = G_OBJECT_CLASS (class);
	EvJobClass   *job_class = EV_JOB_CLASS (class);

	oclass->dispose = ev_job_attachments_dispose;
	job_class->run = ev_job_attachments_run;
}

EvJob *
ev_job_attachments_new (EvDocument *document)
{
	EvJob *job;

	ev_debug_message (DEBUG_JOBS, NULL);

	job = g_object_new (EV_TYPE_JOB_ATTACHMENTS, NULL);
	job->document = g_object_ref (document);

	return job;
}

/* EvJobAnnots */
static void
ev_job_annots_init (EvJobAnnots *job)
{
	EV_JOB (job)->run_mode = EV_JOB_RUN_THREAD;
}

static void
ev_job_annots_dispose (GObject *object)
{
	EvJobAnnots *job;

	ev_debug_message (DEBUG_JOBS, NULL);

	job = EV_JOB_ANNOTS (object);

	g_list_free_full (g_steal_pointer (&job->annots), (GDestroyNotify) ev_mapping_list_unref);

	G_OBJECT_CLASS (ev_job_annots_parent_class)->dispose (object);
}

static gboolean
ev_job_annots_run (EvJob *job)
{
	EvJobAnnots *job_annots = EV_JOB_ANNOTS (job);
	gint         i;

	ev_debug_message (DEBUG_JOBS, NULL);
	EV_PROFILER_START (EV_GET_TYPE_NAME (job));

	ev_document_doc_mutex_lock ();
	for (i = 0; i < ev_document_get_n_pages (job->document); i++) {
		EvMappingList *mapping_list;
		EvPage        *page;

		page = ev_document_get_page (job->document, i);
		mapping_list = ev_document_annotations_get_annotations (EV_DOCUMENT_ANNOTATIONS (job->document),
									page);
		g_object_unref (page);

		if (mapping_list)
			job_annots->annots = g_list_prepend (job_annots->annots, mapping_list);
	}
	ev_document_doc_mutex_unlock ();

	job_annots->annots = g_list_reverse (job_annots->annots);

	ev_job_succeeded (job);

	EV_PROFILER_STOP ();
	return FALSE;
}

static void
ev_job_annots_class_init (EvJobAnnotsClass *class)
{
	GObjectClass *oclass = G_OBJECT_CLASS (class);
	EvJobClass   *job_class = EV_JOB_CLASS (class);

	oclass->dispose = ev_job_annots_dispose;
	job_class->run = ev_job_annots_run;
}

EvJob *
ev_job_annots_new (EvDocument *document)
{
	EvJob *job;

	ev_debug_message (DEBUG_JOBS, NULL);

	job = g_object_new (EV_TYPE_JOB_ANNOTS, NULL);
	job->document = g_object_ref (document);

	return job;
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
/* EvJobRenderCairo */
static void
ev_job_render_cairo_init (EvJobRenderCairo *job)
{
	EV_JOB (job)->run_mode = EV_JOB_RUN_THREAD;
}

static void
ev_job_render_cairo_dispose (GObject *object)
{
	EvJobRenderCairo *job;

	job = EV_JOB_RENDER_CAIRO (object);

	ev_debug_message (DEBUG_JOBS, "page: %d (%p)", job->page, job);

	g_clear_pointer (&job->surface, cairo_surface_destroy);
	g_clear_pointer (&job->selection, cairo_surface_destroy);
	g_clear_pointer (&job->selection_region, cairo_region_destroy);

	G_OBJECT_CLASS (ev_job_render_cairo_parent_class)->dispose (object);
}

static gboolean
ev_job_render_cairo_run (EvJob *job)
{
	EvJobRenderCairo     *job_render = EV_JOB_RENDER_CAIRO (job);
	EvPage          *ev_page;
	EvRenderContext *rc;

	ev_debug_message (DEBUG_JOBS, "page: %d (%p)", job_render->page, job);
	EV_PROFILER_START (EV_GET_TYPE_NAME (job));

	ev_document_doc_mutex_lock ();
	ev_document_fc_mutex_lock ();

	ev_page = ev_document_get_page (job->document, job_render->page);
	rc = ev_render_context_new (ev_page, job_render->rotation, job_render->scale);
	ev_render_context_set_target_size (rc,
					   job_render->target_width, job_render->target_height);
	g_object_unref (ev_page);

	job_render->surface = ev_document_render (job->document, rc);

	if (job_render->surface == NULL ||
	    cairo_surface_status (job_render->surface) != CAIRO_STATUS_SUCCESS) {
		ev_document_fc_mutex_unlock ();
		ev_document_doc_mutex_unlock ();
		g_object_unref (rc);

                if (job_render->surface != NULL) {
                        cairo_status_t status = cairo_surface_status (job_render->surface);
                        ev_job_failed (job,
                                       EV_DOCUMENT_ERROR,
                                       EV_DOCUMENT_ERROR_INVALID,
                                       _("Failed to render page %d: %s"),
                                       job_render->page,
                                       cairo_status_to_string (status));
                } else {
                        ev_job_failed (job,
                                       EV_DOCUMENT_ERROR,
                                       EV_DOCUMENT_ERROR_INVALID,
                                       _("Failed to render page %d"),
                                       job_render->page);
                }

		EV_PROFILER_STOP ();
		return FALSE;
	}

	/* If job was cancelled during the page rendering,
	 * we return now, so that the thread is finished ASAP
	 */
	if (g_cancellable_is_cancelled (job->cancellable)) {
		ev_document_fc_mutex_unlock ();
		ev_document_doc_mutex_unlock ();
		g_object_unref (rc);

		EV_PROFILER_STOP ();
		return FALSE;
	}

	if (job_render->include_selection && EV_IS_SELECTION (job->document)) {
		ev_selection_render_selection (EV_SELECTION (job->document),
					       rc,
					       &(job_render->selection),
					       &(job_render->selection_points),
					       NULL,
					       job_render->selection_style,
					       &(job_render->text), &(job_render->base));
		job_render->selection_region =
			ev_selection_get_selection_region (EV_SELECTION (job->document),
							   rc,
							   job_render->selection_style,
							   &(job_render->selection_points));
	}

	g_object_unref (rc);

	ev_document_fc_mutex_unlock ();
	ev_document_doc_mutex_unlock ();

	ev_job_succeeded (job);

	EV_PROFILER_STOP ();
	return FALSE;
}

static void
ev_job_render_cairo_class_init (EvJobRenderCairoClass *class)
{
	GObjectClass *oclass = G_OBJECT_CLASS (class);
	EvJobClass   *job_class = EV_JOB_CLASS (class);

	oclass->dispose = ev_job_render_cairo_dispose;
	job_class->run = ev_job_render_cairo_run;
}

EvJob *
ev_job_render_cairo_new (EvDocument   *document,
			 gint          page,
			 gint          rotation,
			 gdouble       scale,
			 gint          width,
			 gint          height)
{
	EvJobRenderCairo *job;

	ev_debug_message (DEBUG_JOBS, "page: %d", page);

	job = g_object_new (EV_TYPE_JOB_RENDER_CAIRO, NULL);

	EV_JOB (job)->document = g_object_ref (document);
	job->page = page;
	job->rotation = rotation;
	job->scale = scale;
	job->target_width = width;
	job->target_height = height;

	return EV_JOB (job);
}

void
ev_job_render_cairo_set_selection_info (EvJobRenderCairo *job,
					EvRectangle      *selection_points,
					EvSelectionStyle  selection_style,
					GdkRGBA          *text,
					GdkRGBA          *base)
{
	job->include_selection = TRUE;

	job->selection_points = *selection_points;
	job->selection_style = selection_style;
	job->text = *text;
	job->base = *base;
}
G_GNUC_END_IGNORE_DEPRECATIONS

/* EvJobRenderTexture */
static void
ev_job_render_texture_init (EvJobRenderTexture *job)
{
	EV_JOB (job)->run_mode = EV_JOB_RUN_THREAD;
}

static void
ev_job_render_texture_dispose (GObject *object)
{
	EvJobRenderTexture *job;

	job = EV_JOB_RENDER_TEXTURE (object);

	ev_debug_message (DEBUG_JOBS, "page: %d (%p)", job->page, job);

	g_clear_object (&job->texture);
	g_clear_object (&job->selection);
	g_clear_pointer (&job->selection_region, cairo_region_destroy);

	(* G_OBJECT_CLASS (ev_job_render_texture_parent_class)->dispose) (object);
}

static GdkTexture *
gdk_texture_new_for_surface (cairo_surface_t *surface)
{
	GdkTexture *texture;
	GBytes *bytes;

	g_return_val_if_fail (surface != NULL, NULL);
	g_return_val_if_fail (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_IMAGE, NULL);
	g_return_val_if_fail (cairo_image_surface_get_width (surface) > 0, NULL);
	g_return_val_if_fail (cairo_image_surface_get_height (surface) > 0, NULL);

	bytes = g_bytes_new_with_free_func (cairo_image_surface_get_data (surface),
					    cairo_image_surface_get_height (surface) * cairo_image_surface_get_stride (surface),
					    (GDestroyNotify)cairo_surface_destroy,
					    cairo_surface_reference (surface));

	texture = gdk_memory_texture_new (cairo_image_surface_get_width (surface),
					  cairo_image_surface_get_height (surface),
					  GDK_MEMORY_DEFAULT,
					  bytes,
					  cairo_image_surface_get_stride (surface));

	g_bytes_unref (bytes);

	return texture;
}

EvJob *
ev_job_render_texture_new (EvDocument   *document,
			 gint          page,
			 gint          rotation,
			 gdouble       scale,
			 gint          width,
			 gint          height)
{
	EvJobRenderTexture *job;

	ev_debug_message (DEBUG_JOBS, "page: %d", page);

	job = g_object_new (EV_TYPE_JOB_RENDER_TEXTURE, NULL);

	EV_JOB (job)->document = g_object_ref (document);
	job->page = page;
	job->rotation = rotation;
	job->scale = scale;
	job->target_width = width;
	job->target_height = height;

	return EV_JOB (job);
}

static gboolean
ev_job_render_texture_run (EvJob *job)
{
	EvJobRenderTexture     *job_render = EV_JOB_RENDER_TEXTURE (job);
	EvPage          *ev_page;
	EvRenderContext *rc;
	cairo_surface_t *surface, *selection = NULL;

	ev_debug_message (DEBUG_JOBS, "page: %d (%p)", job_render->page, job);
	EV_PROFILER_START (EV_GET_TYPE_NAME (job));

	ev_document_doc_mutex_lock ();

	ev_document_fc_mutex_lock ();

	ev_page = ev_document_get_page (job->document, job_render->page);
	rc = ev_render_context_new (ev_page, job_render->rotation, job_render->scale);
	ev_render_context_set_target_size (rc,
					   job_render->target_width, job_render->target_height);
	g_object_unref (ev_page);

	surface = ev_document_render (job->document, rc);

	if (surface == NULL ||
	    cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS) {
		ev_document_fc_mutex_unlock ();
		ev_document_doc_mutex_unlock ();
		g_object_unref (rc);

                if (surface != NULL) {
                        cairo_status_t status = cairo_surface_status (surface);
                        ev_job_failed (job,
                                       EV_DOCUMENT_ERROR,
                                       EV_DOCUMENT_ERROR_INVALID,
                                       _("Failed to render page %d: %s"),
                                       job_render->page,
                                       cairo_status_to_string (status));
                } else {
                        ev_job_failed (job,
                                       EV_DOCUMENT_ERROR,
                                       EV_DOCUMENT_ERROR_INVALID,
                                       _("Failed to render page %d"),
                                       job_render->page);
                }

		job_render->texture = NULL;
		EV_PROFILER_STOP ();
		return FALSE;
	}

	job_render->texture = gdk_texture_new_for_surface (surface);
	cairo_surface_destroy (surface);

	/* If job was cancelled during the page rendering,
	 * we return now, so that the thread is finished ASAP
	 */
	if (g_cancellable_is_cancelled (job->cancellable)) {
		ev_document_fc_mutex_unlock ();
		ev_document_doc_mutex_unlock ();
		g_object_unref (rc);
		EV_PROFILER_STOP ();

		return FALSE;
	}

	if (job_render->include_selection && EV_IS_SELECTION (job->document)) {
		ev_selection_render_selection (EV_SELECTION (job->document),
					       rc,
					       &selection,
					       &(job_render->selection_points),
					       NULL,
					       job_render->selection_style,
					       &(job_render->text), &(job_render->base));
		job_render->selection_region =
			ev_selection_get_selection_region (EV_SELECTION (job->document),
							   rc,
							   job_render->selection_style,
							   &(job_render->selection_points));

		if (selection != NULL) {
			job_render->selection = gdk_texture_new_for_surface (selection);
			cairo_surface_destroy (selection);
		}
	}

	g_object_unref (rc);

	ev_document_fc_mutex_unlock ();
	ev_document_doc_mutex_unlock ();

	ev_job_succeeded (job);

	EV_PROFILER_STOP ();
	return FALSE;
}

static void
ev_job_render_texture_class_init (EvJobRenderTextureClass *class)
{
	GObjectClass *oclass = G_OBJECT_CLASS (class);
	EvJobClass   *job_class = EV_JOB_CLASS (class);

	oclass->dispose = ev_job_render_texture_dispose;
	job_class->run = ev_job_render_texture_run;
}

void
ev_job_render_texture_set_selection_info (EvJobRenderTexture *job,
					EvRectangle      *selection_points,
					EvSelectionStyle  selection_style,
					GdkRGBA          *text,
					GdkRGBA          *base)
{
	job->include_selection = TRUE;

	job->selection_points = *selection_points;
	job->selection_style = selection_style;
	job->text = *text;
	job->base = *base;
}

/* EvJobPageData */
static void
ev_job_page_data_init (EvJobPageData *job)
{
	EV_JOB (job)->run_mode = EV_JOB_RUN_THREAD;
}

static gboolean
ev_job_page_data_run (EvJob *job)
{
	EvJobPageData *job_pd = EV_JOB_PAGE_DATA (job);
	EvPage        *ev_page;

	ev_debug_message (DEBUG_JOBS, "page: %d (%p)", job_pd->page, job);
	EV_PROFILER_START (EV_GET_TYPE_NAME (job));

	ev_document_doc_mutex_lock ();
	ev_page = ev_document_get_page (job->document, job_pd->page);

	if ((job_pd->flags & EV_PAGE_DATA_INCLUDE_TEXT_MAPPING) && EV_IS_DOCUMENT_TEXT (job->document))
		job_pd->text_mapping =
			ev_document_text_get_text_mapping (EV_DOCUMENT_TEXT (job->document), ev_page);
	if ((job_pd->flags & EV_PAGE_DATA_INCLUDE_TEXT) && EV_IS_DOCUMENT_TEXT (job->document))
		job_pd->text =
			ev_document_text_get_text (EV_DOCUMENT_TEXT (job->document), ev_page);
	if ((job_pd->flags & EV_PAGE_DATA_INCLUDE_TEXT_LAYOUT) && EV_IS_DOCUMENT_TEXT (job->document))
		ev_document_text_get_text_layout (EV_DOCUMENT_TEXT (job->document),
						  ev_page,
						  &(job_pd->text_layout),
						  &(job_pd->text_layout_length));
	if ((job_pd->flags & EV_PAGE_DATA_INCLUDE_TEXT_ATTRS) && EV_IS_DOCUMENT_TEXT (job->document))
		job_pd ->text_attrs =
			ev_document_text_get_text_attrs (EV_DOCUMENT_TEXT (job->document),
							 ev_page);
        if ((job_pd->flags & EV_PAGE_DATA_INCLUDE_TEXT_LOG_ATTRS) && job_pd->text) {
                job_pd->text_log_attrs_length = g_utf8_strlen (job_pd->text, -1);
                job_pd->text_log_attrs = g_new0 (PangoLogAttr, job_pd->text_log_attrs_length + 1);

                /* FIXME: We need API to get the language of the document */
                pango_get_log_attrs (job_pd->text, -1, -1, NULL, job_pd->text_log_attrs, job_pd->text_log_attrs_length + 1);
        }
	if ((job_pd->flags & EV_PAGE_DATA_INCLUDE_LINKS) && EV_IS_DOCUMENT_LINKS (job->document))
		job_pd->link_mapping =
			ev_document_links_get_links (EV_DOCUMENT_LINKS (job->document), ev_page);
	if ((job_pd->flags & EV_PAGE_DATA_INCLUDE_FORMS) && EV_IS_DOCUMENT_FORMS (job->document))
		job_pd->form_field_mapping =
			ev_document_forms_get_form_fields (EV_DOCUMENT_FORMS (job->document),
							   ev_page);
	if ((job_pd->flags & EV_PAGE_DATA_INCLUDE_IMAGES) && EV_IS_DOCUMENT_IMAGES (job->document))
		job_pd->image_mapping =
			ev_document_images_get_image_mapping (EV_DOCUMENT_IMAGES (job->document),
							      ev_page);
	if ((job_pd->flags & EV_PAGE_DATA_INCLUDE_ANNOTS) && EV_IS_DOCUMENT_ANNOTATIONS (job->document))
		job_pd->annot_mapping =
			ev_document_annotations_get_annotations (EV_DOCUMENT_ANNOTATIONS (job->document),
								 ev_page);
        if ((job_pd->flags & EV_PAGE_DATA_INCLUDE_MEDIA) && EV_IS_DOCUMENT_MEDIA (job->document))
                job_pd->media_mapping =
                        ev_document_media_get_media_mapping (EV_DOCUMENT_MEDIA (job->document),
                                                             ev_page);
	g_object_unref (ev_page);
	ev_document_doc_mutex_unlock ();

	ev_job_succeeded (job);

	EV_PROFILER_STOP ();
	return FALSE;
}

static void
ev_job_page_data_class_init (EvJobPageDataClass *class)
{
	EvJobClass *job_class = EV_JOB_CLASS (class);

	job_class->run = ev_job_page_data_run;
}

EvJob *
ev_job_page_data_new (EvDocument        *document,
		      gint               page,
		      EvJobPageDataFlags flags)
{
	EvJobPageData *job;

	ev_debug_message (DEBUG_JOBS, "%d", page);

	job = g_object_new (EV_TYPE_JOB_PAGE_DATA, NULL);

	EV_JOB (job)->document = g_object_ref (document);
	job->page = page;
	job->flags = flags;

	return EV_JOB (job);
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
/* EvJobThumbnailCairo */
static void
ev_job_thumbnail_cairo_init (EvJobThumbnailCairo *job)
{
	EV_JOB (job)->run_mode = EV_JOB_RUN_THREAD;
}

static void
ev_job_thumbnail_cairo_dispose (GObject *object)
{
	EvJobThumbnailCairo *job;

	job = EV_JOB_THUMBNAIL_CAIRO (object);

	ev_debug_message (DEBUG_JOBS, "%d (%p)", job->page, job);

	g_clear_pointer (&job->thumbnail_surface, cairo_surface_destroy);

	G_OBJECT_CLASS (ev_job_thumbnail_cairo_parent_class)->dispose (object);
}

static gboolean
ev_job_thumbnail_cairo_run (EvJob *job)
{
	EvJobThumbnailCairo  *job_thumb = EV_JOB_THUMBNAIL_CAIRO (job);
	EvRenderContext *rc;
	EvPage          *page;

	ev_debug_message (DEBUG_JOBS, "%d (%p)", job_thumb->page, job);
	EV_PROFILER_START (EV_GET_TYPE_NAME (job));

	ev_document_doc_mutex_lock ();

	page = ev_document_get_page (job->document, job_thumb->page);
	rc = ev_render_context_new (page, job_thumb->rotation, job_thumb->scale);
	ev_render_context_set_target_size (rc,
					   job_thumb->target_width, job_thumb->target_height);
	g_object_unref (page);

	job_thumb->thumbnail_surface = ev_document_get_thumbnail_surface (job->document, rc);
	g_object_unref (rc);
	ev_document_doc_mutex_unlock ();

	if (job_thumb->thumbnail_surface == NULL) {
		ev_job_failed (job,
			       EV_DOCUMENT_ERROR,
			       EV_DOCUMENT_ERROR_INVALID,
			       _("Failed to create thumbnail for page %d"),
			       job_thumb->page);
	} else {
		ev_job_succeeded (job);
	}

	EV_PROFILER_STOP ();
	return FALSE;
}

static void
ev_job_thumbnail_cairo_class_init (EvJobThumbnailCairoClass *class)
{
	GObjectClass *oclass = G_OBJECT_CLASS (class);
	EvJobClass   *job_class = EV_JOB_CLASS (class);

	oclass->dispose = ev_job_thumbnail_cairo_dispose;
	job_class->run = ev_job_thumbnail_cairo_run;
}

EvJob *
ev_job_thumbnail_cairo_new (EvDocument *document,
			    gint        page,
			    gint        rotation,
			    gdouble     scale)
{
	EvJobThumbnailCairo *job;

	ev_debug_message (DEBUG_JOBS, "%d", page);

	job = g_object_new (EV_TYPE_JOB_THUMBNAIL_CAIRO, NULL);

	EV_JOB (job)->document = g_object_ref (document);
	job->page = page;
	job->rotation = rotation;
	job->scale = scale;
        job->target_width = -1;
        job->target_height = -1;

	return EV_JOB (job);
}

EvJob *
ev_job_thumbnail_cairo_new_with_target_size (EvDocument *document,
					     gint        page,
					     gint        rotation,
					     gint        target_width,
					     gint        target_height)
{
	EvJob *job = ev_job_thumbnail_cairo_new (document, page, rotation, 1.);
	EvJobThumbnailCairo  *job_thumb = EV_JOB_THUMBNAIL_CAIRO (job);

        job_thumb->target_width = target_width;
        job_thumb->target_height = target_height;

        return job;
}
G_GNUC_END_IGNORE_DEPRECATIONS

/* EvJobThumbnailTexture */
static void
ev_job_thumbnail_texture_init (EvJobThumbnailTexture *job)
{
	EV_JOB (job)->run_mode = EV_JOB_RUN_THREAD;
}

static void
ev_job_thumbnail_texture_dispose (GObject *object)
{
	EvJobThumbnailTexture *job;

	job = EV_JOB_THUMBNAIL_TEXTURE (object);

	ev_debug_message (DEBUG_JOBS, "%d (%p)", job->page, job);

	g_clear_object (&job->thumbnail_texture);

	G_OBJECT_CLASS (ev_job_thumbnail_texture_parent_class)->dispose (object);
}

static gboolean
ev_job_thumbnail_texture_run (EvJob *job)
{
	EvJobThumbnailTexture  *job_thumb = EV_JOB_THUMBNAIL_TEXTURE(job);
	EvRenderContext *rc;
	EvPage          *page;
	cairo_surface_t *surface;

	ev_debug_message (DEBUG_JOBS, "%d (%p)", job_thumb->page, job);
	EV_PROFILER_START (EV_GET_TYPE_NAME (job));

	ev_document_doc_mutex_lock ();

	page = ev_document_get_page (job->document, job_thumb->page);
	rc = ev_render_context_new (page, job_thumb->rotation, job_thumb->scale);
	ev_render_context_set_target_size (rc,
					   job_thumb->target_width, job_thumb->target_height);
	g_object_unref (page);

	surface = ev_document_get_thumbnail_surface (job->document, rc);

	job_thumb->thumbnail_texture = gdk_texture_new_for_surface (surface);
	cairo_surface_destroy(surface);
	g_object_unref (rc);
	ev_document_doc_mutex_unlock ();

	if (job_thumb->thumbnail_texture == NULL) {
		ev_job_failed (job,
			       EV_DOCUMENT_ERROR,
			       EV_DOCUMENT_ERROR_INVALID,
			       _("Failed to create thumbnail for page %d"),
			       job_thumb->page);
	} else {
		ev_job_succeeded (job);
	}

	EV_PROFILER_STOP ();
	return FALSE;
}

static void
ev_job_thumbnail_texture_class_init (EvJobThumbnailTextureClass *class)
{
	GObjectClass *oclass = G_OBJECT_CLASS (class);
	EvJobClass   *job_class = EV_JOB_CLASS (class);

	oclass->dispose = ev_job_thumbnail_texture_dispose;
	job_class->run = ev_job_thumbnail_texture_run;
}

EvJob *
ev_job_thumbnail_texture_new (EvDocument *document,
		      gint        page,
		      gint        rotation,
		      gdouble     scale)
{
	EvJobThumbnailTexture *job;

	ev_debug_message (DEBUG_JOBS, "%d", page);

	job = g_object_new (EV_TYPE_JOB_THUMBNAIL_TEXTURE, NULL);

	EV_JOB (job)->document = g_object_ref (document);
	job->page = page;
	job->rotation = rotation;
	job->scale = scale;
        job->target_width = -1;
        job->target_height = -1;

	return EV_JOB (job);
}

EvJob *
ev_job_thumbnail_texture_new_with_target_size (EvDocument *document,
                                       gint        page,
                                       gint        rotation,
                                       gint        target_width,
                                       gint        target_height)
{
        EvJob *job = ev_job_thumbnail_texture_new (document, page, rotation, 1.);
        EvJobThumbnailTexture  *job_thumb = EV_JOB_THUMBNAIL_TEXTURE(job);

        job_thumb->target_width = target_width;
        job_thumb->target_height = target_height;

        return job;
}

/**
 * ev_job_thumbnail_texture_get_texture:
 * @job: an #EvJobThumbnailTexture job
 *
 * This is similar to ev_job_find_get_n_results() but it takes
 * care to treat any multi-line matches as being only one result.
 *
 * Returns: (nullable) (transfer none): total number of match results in @page
 */
GdkTexture *
ev_job_thumbnail_texture_get_texture (EvJobThumbnailTexture *job)
{
	g_return_val_if_fail (EV_IS_JOB_THUMBNAIL_TEXTURE (job), NULL);

	return job->thumbnail_texture;
}

/* EvJobFonts */
static void
ev_job_fonts_init (EvJobFonts *job)
{
	EV_JOB (job)->run_mode = EV_JOB_RUN_THREAD;
}

static gboolean
ev_job_fonts_run (EvJob *job)
{
	EvDocument      *document = ev_job_get_document (job);

	ev_debug_message (DEBUG_JOBS, NULL);
	EV_PROFILER_START (EV_GET_TYPE_NAME (job));

	ev_document_doc_mutex_lock ();
	ev_document_fc_mutex_lock ();

	ev_document_fonts_scan (EV_DOCUMENT_FONTS (document));

	ev_document_fc_mutex_unlock ();
	ev_document_doc_mutex_unlock ();

	ev_job_succeeded (job);

	EV_PROFILER_STOP ();
	return FALSE;
}

static void
ev_job_fonts_class_init (EvJobFontsClass *class)
{
	EvJobClass *job_class = EV_JOB_CLASS (class);

	job_class->run = ev_job_fonts_run;
}

EvJob *
ev_job_fonts_new (EvDocument *document)
{
	EvJobFonts *job;

	ev_debug_message (DEBUG_JOBS, NULL);

	job = g_object_new (EV_TYPE_JOB_FONTS, NULL);

	EV_JOB (job)->document = g_object_ref (document);

	return EV_JOB (job);
}

/* EvJobLoad */
static void
ev_job_load_init (EvJobLoad *job)
{
	EV_JOB (job)->run_mode = EV_JOB_RUN_THREAD;
}

static void
ev_job_load_dispose (GObject *object)
{
	EvJobLoad *job = EV_JOB_LOAD (object);

	ev_debug_message (DEBUG_JOBS, "%s", job->uri);

	g_clear_pointer (&job->uri, g_free);
	g_clear_pointer (&job->password, g_free);

	(* G_OBJECT_CLASS (ev_job_load_parent_class)->dispose) (object);
}

static gboolean
ev_job_load_run (EvJob *job)
{
	EvJobLoad *job_load = EV_JOB_LOAD (job);
	GError    *error = NULL;

	ev_debug_message (DEBUG_JOBS, "%s", job_load->uri);
	EV_PROFILER_START (EV_GET_TYPE_NAME (job));

	ev_document_fc_mutex_lock ();

	/* This job may already have a document even if the job didn't complete
	   because, e.g., a password is required - if so, just reload rather than
	   creating a new instance */
	if (job->document) {
		const gchar *uncompressed_uri;

		if (job_load->password) {
			ev_document_security_set_password (EV_DOCUMENT_SECURITY (job->document),
							   job_load->password);
		}

		job->failed = FALSE;
		job->finished = FALSE;
		g_clear_error (&job->error);

		uncompressed_uri = g_object_get_data (G_OBJECT (job->document),
						      "uri-uncompressed");
		ev_document_load (job->document,
				  uncompressed_uri ? uncompressed_uri : job_load->uri,
				  &error);
	} else {
		job->document = ev_document_factory_get_document (job_load->uri,
								  &error);
	}

	ev_document_fc_mutex_unlock ();

	if (error) {
		ev_job_failed_from_error (job, error);
		g_error_free (error);
	} else {
		ev_job_succeeded (job);
	}

	EV_PROFILER_STOP ();
	return FALSE;
}

static void
ev_job_load_class_init (EvJobLoadClass *class)
{
	GObjectClass *oclass = G_OBJECT_CLASS (class);
	EvJobClass   *job_class = EV_JOB_CLASS (class);

	oclass->dispose = ev_job_load_dispose;
	job_class->run = ev_job_load_run;
}

EvJob *
ev_job_load_new (const gchar *uri)
{
	EvJobLoad *job;

	ev_debug_message (DEBUG_JOBS, "%s", uri);

	job = g_object_new (EV_TYPE_JOB_LOAD, NULL);
	job->uri = g_strdup (uri);

	return EV_JOB (job);
}

void
ev_job_load_set_uri (EvJobLoad *job, const gchar *uri)
{
	ev_debug_message (DEBUG_JOBS, "%s", uri);

	if (job->uri)
		g_free (job->uri);
	job->uri = g_strdup (uri);
}

void
ev_job_load_set_password (EvJobLoad *job, const gchar *password)
{
	ev_debug_message (DEBUG_JOBS, NULL);

	if (job->password)
		g_free (job->password);
	job->password = password ? g_strdup (password) : NULL;
}

/* EvJobLoadStream */

/**
 * EvJobLoadStream:
 *
 * A job class to load a #EvDocument from a #GInputStream.
 *
 * Since: 3.6
 */

static void
ev_job_load_stream_init (EvJobLoadStream *job)
{
        job->flags = EV_DOCUMENT_LOAD_FLAG_NONE;

        EV_JOB (job)->run_mode = EV_JOB_RUN_THREAD;
}

static void
ev_job_load_stream_dispose (GObject *object)
{
        EvJobLoadStream *job = EV_JOB_LOAD_STREAM (object);
        EvJobLoadStreamPrivate *priv = ev_job_load_stream_get_instance_private (job);

	g_clear_object (&job->stream);

	g_clear_pointer (&priv->mime_type, g_free);
	g_clear_pointer (&job->password, g_free);

        G_OBJECT_CLASS (ev_job_load_stream_parent_class)->dispose (object);
}

static gboolean
ev_job_load_stream_run (EvJob *job)
{
        EvJobLoadStream *job_load_stream = EV_JOB_LOAD_STREAM (job);
        EvJobLoadStreamPrivate *priv = ev_job_load_stream_get_instance_private (job_load_stream);
        GError *error = NULL;

	EV_PROFILER_START (EV_GET_TYPE_NAME (job));

        ev_document_fc_mutex_lock ();

        /* This job may already have a document even if the job didn't complete
           because, e.g., a password is required - if so, just reload_stream rather than
           creating a new instance */
        /* FIXME: do we need to rewind the stream here?? */
        if (job->document) {

                if (job_load_stream->password) {
                        ev_document_security_set_password (EV_DOCUMENT_SECURITY (job->document),
                                                           job_load_stream->password);
                }

                job->failed = FALSE;
                job->finished = FALSE;
                g_clear_error (&job->error);

                ev_document_load_stream (job->document,
                                         job_load_stream->stream,
                                         job_load_stream->flags,
                                         job->cancellable,
                                         &error);
        } else {
                job->document = ev_document_factory_get_document_for_stream (job_load_stream->stream,
                                                                             priv->mime_type,
                                                                             job_load_stream->flags,
                                                                             job->cancellable,
                                                                             &error);
        }

        ev_document_fc_mutex_unlock ();

        if (error) {
                ev_job_failed_from_error (job, error);
                g_error_free (error);
        } else {
                ev_job_succeeded (job);
        }

	EV_PROFILER_STOP ();
        return FALSE;
}

static void
ev_job_load_stream_class_init (EvJobLoadStreamClass *class)
{
        GObjectClass *oclass = G_OBJECT_CLASS (class);
        EvJobClass   *job_class = EV_JOB_CLASS (class);

        oclass->dispose = ev_job_load_stream_dispose;
        job_class->run = ev_job_load_stream_run;
}

EvJob *
ev_job_load_stream_new (GInputStream       *stream,
                        EvDocumentLoadFlags flags)
{
        EvJobLoadStream *job;

        job = g_object_new (EV_TYPE_JOB_LOAD_STREAM, NULL);
        ev_job_load_stream_set_stream (job, stream);
        ev_job_load_stream_set_load_flags (job, flags);

        return EV_JOB (job);
}

void
ev_job_load_stream_set_stream (EvJobLoadStream *job,
                               GInputStream    *stream)
{
        g_return_if_fail (EV_IS_JOB_LOAD_STREAM (job));
        g_return_if_fail (G_IS_INPUT_STREAM (stream));

        g_object_ref (stream);
        if (job->stream)
              g_object_unref (job->stream);
        job->stream = stream;
}

void
ev_job_load_stream_set_mime_type (EvJobLoadStream    *job,
                                  const char         *mime_type)
{
        EvJobLoadStreamPrivate *priv;

        g_return_if_fail (EV_IS_JOB_LOAD_STREAM (job));

        priv = ev_job_load_stream_get_instance_private (job);
        g_free (priv->mime_type);
        priv->mime_type = g_strdup (mime_type);
}

void
ev_job_load_stream_set_load_flags (EvJobLoadStream    *job,
                                   EvDocumentLoadFlags flags)
{
        g_return_if_fail (EV_IS_JOB_LOAD_STREAM (job));

        job->flags = flags;
}

void
ev_job_load_stream_set_password (EvJobLoadStream *job,
                                 const char *password)
{
        char *old_password;

        ev_debug_message (DEBUG_JOBS, NULL);

        g_return_if_fail (EV_IS_JOB_LOAD_STREAM (job));

        old_password = job->password;
        job->password = g_strdup (password);
        g_free (old_password);
}

/* EvJobLoadGFile */

/**
 * EvJobLoadGFile:
 *
 * A job class to load a #EvDocument from a #GFile.
 *
 * Since: 3.6
 */

static void
ev_job_load_gfile_init (EvJobLoadGFile *job)
{
        job->flags = EV_DOCUMENT_LOAD_FLAG_NONE;

        EV_JOB (job)->run_mode = EV_JOB_RUN_THREAD;
}

static void
ev_job_load_gfile_dispose (GObject *object)
{
        EvJobLoadGFile *job = EV_JOB_LOAD_GFILE (object);

	g_clear_object (&job->gfile);
	g_clear_pointer (&job->password, g_free);

        G_OBJECT_CLASS (ev_job_load_gfile_parent_class)->dispose (object);
}

static gboolean
ev_job_load_gfile_run (EvJob *job)
{
        EvJobLoadGFile *job_load_gfile = EV_JOB_LOAD_GFILE (job);
        GError    *error = NULL;

	EV_PROFILER_START (EV_GET_TYPE_NAME (job));

        ev_document_fc_mutex_lock ();

        /* This job may already have a document even if the job didn't complete
           because, e.g., a password is required - if so, just reload_gfile rather than
           creating a new instance */
        if (job->document) {

                if (job_load_gfile->password) {
                        ev_document_security_set_password (EV_DOCUMENT_SECURITY (job->document),
                                                           job_load_gfile->password);
                }

                job->failed = FALSE;
                job->finished = FALSE;
                g_clear_error (&job->error);

                ev_document_load_gfile (job->document,
                                        job_load_gfile->gfile,
                                        job_load_gfile->flags,
                                        job->cancellable,
                                        &error);
        } else {
                job->document = ev_document_factory_get_document_for_gfile (job_load_gfile->gfile,
                                                                            job_load_gfile->flags,
                                                                            job->cancellable,
                                                                            &error);
        }

        ev_document_fc_mutex_unlock ();

        if (error) {
                ev_job_failed_from_error (job, error);
                g_error_free (error);
        } else {
                ev_job_succeeded (job);
        }

	EV_PROFILER_STOP ();
        return FALSE;
}

static void
ev_job_load_gfile_class_init (EvJobLoadGFileClass *class)
{
        GObjectClass *oclass = G_OBJECT_CLASS (class);
        EvJobClass   *job_class = EV_JOB_CLASS (class);

        oclass->dispose = ev_job_load_gfile_dispose;
        job_class->run = ev_job_load_gfile_run;
}

EvJob *
ev_job_load_gfile_new (GFile              *gfile,
                       EvDocumentLoadFlags flags)
{
        EvJobLoadGFile *job;

        job = g_object_new (EV_TYPE_JOB_LOAD_GFILE, NULL);
        ev_job_load_gfile_set_gfile (job, gfile);
        ev_job_load_gfile_set_load_flags (job, flags);

        return EV_JOB (job);
}

void
ev_job_load_gfile_set_gfile (EvJobLoadGFile *job,
                             GFile          *gfile)
{
        g_return_if_fail (EV_IS_JOB_LOAD_GFILE (job));
        g_return_if_fail (G_IS_FILE (gfile));

        g_object_ref (gfile);
        if (job->gfile)
              g_object_unref (job->gfile);
        job->gfile = gfile;
}

void
ev_job_load_gfile_set_load_flags (EvJobLoadGFile     *job,
                                  EvDocumentLoadFlags flags)
{
        g_return_if_fail (EV_IS_JOB_LOAD_GFILE (job));

        job->flags = flags;
}

void
ev_job_load_gfile_set_password (EvJobLoadGFile *job,
                                const char *password)
{
        char *old_password;

        ev_debug_message (DEBUG_JOBS, NULL);

        g_return_if_fail (EV_IS_JOB_LOAD_GFILE (job));

        old_password = job->password;
        job->password = g_strdup (password);
        g_free (old_password);
}

/* EvJobLoadFd */

/**
 * EvJobLoadFd:
 *
 * A job class to load a #EvDocument from a file descriptor
 * referring to a regular file.
 *
 * Since: 42.0
 */

static int
ev_dupfd (int fd,
          GError **error)
{
        int new_fd;

        new_fd = fcntl (fd, F_DUPFD_CLOEXEC, 3);
        if (new_fd == -1) {
                int errsv = errno;
                g_set_error_literal (error, G_FILE_ERROR, g_file_error_from_errno (errsv),
                                     g_strerror (errsv));
        }

        return new_fd;
}

static void
ev_job_load_fd_init (EvJobLoadFd *job)
{
        job->flags = EV_DOCUMENT_LOAD_FLAG_NONE;
        job->fd = -1;

        EV_JOB (job)->run_mode = EV_JOB_RUN_THREAD;
}

static void
ev_job_load_fd_dispose (GObject *object)
{
        EvJobLoadFd *job = EV_JOB_LOAD_FD (object);

        if (job->fd != -1) {
                close (job->fd);
                job->fd = -1;
        }

	g_clear_pointer (&job->mime_type, g_free);
	g_clear_pointer (&job->password, g_free);

        G_OBJECT_CLASS (ev_job_load_fd_parent_class)->dispose (object);
}

static gboolean
ev_job_load_fd_run (EvJob *job)
{
        EvJobLoadFd *job_load_fd = EV_JOB_LOAD_FD (job);
        GError *error = NULL;
        int fd;

	EV_PROFILER_START (EV_GET_TYPE_NAME (job));

        if (job_load_fd->fd == -1) {
                g_set_error_literal (&error, G_FILE_ERROR, G_FILE_ERROR_BADF,
                                     "Invalid file descriptor");
                goto out;
        }

        /* We need to dup the FD here since we may need to pass it again
         * to ev_document_load_fd() if the document is encrypted,
         * and the previous call to it consumes the FD.
         */
        fd = ev_dupfd (job_load_fd->fd, &error);
        if (fd == -1)
                goto out;


        ev_document_fc_mutex_lock ();

        /* This job may already have a document even if the job didn't complete
           because, e.g., a password is required - if so, just reload_fd rather than
           creating a new instance */

        if (job->document) {

                if (job_load_fd->password) {
                        ev_document_security_set_password (EV_DOCUMENT_SECURITY (job->document),
                                                           job_load_fd->password);
                }

                job->failed = FALSE;
                job->finished = FALSE;
                g_clear_error (&job->error);

                ev_document_load_fd (job->document,
                                     fd,
                                     job_load_fd->flags,
                                     job->cancellable,
                                     &error);
                fd = -1; /* consumed */
        } else {
                job->document = ev_document_factory_get_document_for_fd (fd,
                                                                         job_load_fd->mime_type,
                                                                         job_load_fd->flags,
                                                                         job->cancellable,
                                                                         &error);
                fd = -1; /* consumed */
        }

        ev_document_fc_mutex_unlock ();

 out:
        if (error) {
                ev_job_failed_from_error (job, error);
                g_error_free (error);
        } else {
                ev_job_succeeded (job);
        }

	EV_PROFILER_STOP ();
        return FALSE;
}

static void
ev_job_load_fd_class_init (EvJobLoadFdClass *class)
{
        GObjectClass *oclass = G_OBJECT_CLASS (class);
        EvJobClass   *job_class = EV_JOB_CLASS (class);

        oclass->dispose = ev_job_load_fd_dispose;
        job_class->run = ev_job_load_fd_run;
}

/**
 * ev_job_load_fd_new:
 * @fd: a file descriptor
 * @mime_type: the mime type
 * @flags:flags from #EvDocumentLoadFlags
 * @error: (nullable): a location to store a #GError, or %NULL
 *
 * Creates a new #EvJobLoadFd for @fd. If duplicating @fd fails,
 * returns %NULL with @error filled in.
 *
 * Returns: (transfer full): the new #EvJobLoadFd, or %NULL
 *
 * Since: 42.0
 */
EvJob *
ev_job_load_fd_new (int                 fd,
                    const char         *mime_type,
                    EvDocumentLoadFlags flags,
                    GError            **error)
{
        EvJobLoadFd *job;

        job = g_object_new (EV_TYPE_JOB_LOAD_FD, NULL);
        if (!ev_job_load_fd_set_fd (job, fd, error)) {
                g_object_unref (job);
                return NULL;
        }

        ev_job_load_fd_set_mime_type (job, mime_type);
        ev_job_load_fd_set_load_flags (job, flags);

        return EV_JOB (job);
}

/**
 * ev_job_load_new_take:
 * @fd: a file descriptor
 * @mime_type: the mime type
 * @flags:flags from #EvDocumentLoadFlags
 *
 * Creates a new #EvJobLoadFd for @fd.
 * Note that the job takes ownership of @fd; you must not do anything
 * with it afterwards.
 *
 * Returns: (transfer full): the new #EvJobLoadFd
 *
 * Since: 42.0
 */
EvJob *
ev_job_load_fd_new_take (int                 fd,
                         const char         *mime_type,
                         EvDocumentLoadFlags flags)
{
        EvJobLoadFd *job;

        job = g_object_new (EV_TYPE_JOB_LOAD_FD, NULL);
        ev_job_load_fd_take_fd (job, fd);
        ev_job_load_fd_set_mime_type (job, mime_type);
        ev_job_load_fd_set_load_flags (job, flags);

        return EV_JOB (job);
}

/**
 * ev_job_load_fd_set_fd:
 * @job: an #EvJob
 * @fd: a file descriptor
 * @error: (nullable): a location to store a #GError, or %NULL
 *
 * Sets @fd as the file descriptor in @job. If duplicating @fd fails,
 * returns %FALSE with @error filled in.
 *
 * Returns: %TRUE if the file descriptor could be set
 *
 * Since: 42.0
 */
gboolean
ev_job_load_fd_set_fd (EvJobLoadFd *job,
                       int          fd,
                       GError     **error)
{
        g_return_val_if_fail (EV_IS_JOB_LOAD_FD (job), FALSE);
        g_return_val_if_fail (fd != -1, FALSE);

        job->fd = ev_dupfd (fd, error);
        return job->fd != -1;
}

/**
 * ev_job_load_fd_take_fd:
 * @job: an #EvJob
 * @fd: a file descriptor
 *
 * Sets @fd as the file descriptor in @job.
 * Note that @job takes ownership of @fd; you must not do anything
 * with it afterwards.
 *
 * Since: 42.0
 */
void
ev_job_load_fd_take_fd (EvJobLoadFd *job,
                        int          fd)
{
        g_return_if_fail (EV_IS_JOB_LOAD_FD (job));
        g_return_if_fail (fd != -1);

        job->fd = fd;
}

void
ev_job_load_fd_set_mime_type (EvJobLoadFd *job,
                              const char  *mime_type)
{
        g_return_if_fail (EV_IS_JOB_LOAD_FD (job));
        g_return_if_fail (mime_type != NULL);

        g_free (job->mime_type);
        job->mime_type = g_strdup (mime_type);
}

void
ev_job_load_fd_set_load_flags (EvJobLoadFd        *job,
                               EvDocumentLoadFlags flags)
{
        g_return_if_fail (EV_IS_JOB_LOAD_FD (job));

        job->flags = flags;
}

void
ev_job_load_fd_set_password (EvJobLoadFd *job,
                             const char  *password)
{
        char *old_password;

        ev_debug_message (DEBUG_JOBS, NULL);

        g_return_if_fail (EV_IS_JOB_LOAD_FD (job));

        old_password = job->password;
        job->password = g_strdup (password);
        g_free (old_password);
}

/* EvJobSave */
static void
ev_job_save_init (EvJobSave *job)
{
	EV_JOB (job)->run_mode = EV_JOB_RUN_THREAD;
}

static void
ev_job_save_dispose (GObject *object)
{
	EvJobSave *job = EV_JOB_SAVE (object);

	ev_debug_message (DEBUG_JOBS, "%s", job->uri);

	g_clear_pointer (&job->uri, g_free);
	g_clear_pointer (&job->document_uri, g_free);

	(* G_OBJECT_CLASS (ev_job_save_parent_class)->dispose) (object);
}

static gboolean
ev_job_save_run (EvJob *job)
{
	EvJobSave *job_save = EV_JOB_SAVE (job);
	gint       fd;
	gchar     *tmp_filename = NULL;
	gchar     *local_uri;
	GError    *error = NULL;

	ev_debug_message (DEBUG_JOBS, "uri: %s, document_uri: %s", job_save->uri, job_save->document_uri);
	EV_PROFILER_START (EV_GET_TYPE_NAME (job));

        fd = ev_mkstemp ("saveacopy.XXXXXX", &tmp_filename, &error);
        if (fd == -1) {
                ev_job_failed_from_error (job, error);
                g_error_free (error);

		EV_PROFILER_STOP ();
		return FALSE;
	}
	close (fd);

	ev_document_doc_mutex_lock ();

	/* Save document to temp filename */
	local_uri = g_filename_to_uri (tmp_filename, NULL, &error);
        if (local_uri != NULL) {
                ev_document_save (job->document, local_uri, &error);
        }

	ev_document_doc_mutex_unlock ();

	if (error) {
		g_free (local_uri);
		ev_job_failed_from_error (job, error);
		g_error_free (error);

		EV_PROFILER_STOP ();
		return FALSE;
	}

	/* If original document was compressed,
	 * compress it again before saving
	 */
	if (g_object_get_data (G_OBJECT (job->document), "uri-uncompressed")) {
		EvCompressionType ctype = EV_COMPRESSION_NONE;
		const gchar      *ext;
		gchar            *uri_comp;

		ext = g_strrstr (job_save->document_uri, ".gz");
		if (ext && g_ascii_strcasecmp (ext, ".gz") == 0)
			ctype = EV_COMPRESSION_GZIP;

		ext = g_strrstr (job_save->document_uri, ".bz2");
		if (ext && g_ascii_strcasecmp (ext, ".bz2") == 0)
			ctype = EV_COMPRESSION_BZIP2;

		uri_comp = ev_file_compress (local_uri, ctype, &error);
		g_free (local_uri);
		g_unlink (tmp_filename);

		if (!uri_comp || error) {
			local_uri = NULL;
		} else {
			local_uri = uri_comp;
		}
	}

	g_free (tmp_filename);

	if (error) {
		g_free (local_uri);
		ev_job_failed_from_error (job, error);
		g_error_free (error);

		EV_PROFILER_STOP ();
		return FALSE;
	}

	if (!local_uri) {
		EV_PROFILER_STOP ();
		return FALSE;
	}

	ev_xfer_uri_simple (local_uri, job_save->uri, &error);
	ev_tmp_uri_unlink (local_uri);

        /* Copy the metadata from the original file */
        if (!error) {
                /* Ignore errors here. Failure to copy metadata is not a hard error */
                ev_file_copy_metadata (job_save->document_uri, job_save->uri, NULL);
        }

	if (error) {
		ev_job_failed_from_error (job, error);
		g_error_free (error);
	} else {
		ev_job_succeeded (job);
	}

	EV_PROFILER_STOP ();
	return FALSE;
}

static void
ev_job_save_class_init (EvJobSaveClass *class)
{
	GObjectClass *oclass = G_OBJECT_CLASS (class);
	EvJobClass   *job_class = EV_JOB_CLASS (class);

	oclass->dispose = ev_job_save_dispose;
	job_class->run = ev_job_save_run;
}

EvJob *
ev_job_save_new (EvDocument  *document,
		 const gchar *uri,
		 const gchar *document_uri)
{
	EvJobSave *job;

	ev_debug_message (DEBUG_JOBS, "uri: %s, document_uri: %s", uri, document_uri);

	job = g_object_new (EV_TYPE_JOB_SAVE, NULL);

	EV_JOB (job)->document = g_object_ref (document);
	job->uri = g_strdup (uri);
	job->document_uri = g_strdup (document_uri);

	return EV_JOB (job);
}

/* EvJobFind */
static void
ev_job_find_init (EvJobFind *job)
{
	EV_JOB (job)->run_mode = EV_JOB_RUN_MAIN_LOOP;
}

static void
ev_job_find_dispose (GObject *object)
{
	EvJobFind *job = EV_JOB_FIND (object);

	ev_debug_message (DEBUG_JOBS, NULL);

	g_clear_pointer (&job->text, g_free);

	if (job->pages) {
		gint i;

		for (i = 0; i < job->n_pages; i++) {
			g_list_free_full (job->pages[i], (GDestroyNotify)ev_find_rectangle_free);
		}

		g_clear_pointer (&job->pages, g_free);
	}

	(* G_OBJECT_CLASS (ev_job_find_parent_class)->dispose) (object);
}

static gboolean
ev_job_find_run (EvJob *job)
{
	EvJobFind      *job_find = EV_JOB_FIND (job);
	EvDocumentFind *find = EV_DOCUMENT_FIND (job->document);
	EvPage         *ev_page;
	GList          *matches;
	int64_t         sysprof_begin;

	ev_debug_message (DEBUG_JOBS, NULL);

	/* Do not block the main loop */
	if (!ev_document_doc_mutex_trylock ())
		return TRUE;

#ifdef EV_ENABLE_DEBUG
	/* We use the #ifdef in this case because of the if */
	if (job_find->current_page == job_find->start_page)
		/* Skipping EV_PROFILER_START () that would declare the
		   variables within the `if` scope.
		 */
		sysprof_begin = SYSPROF_CAPTURE_CURRENT_TIME;
#endif

	ev_page = ev_document_get_page (job->document, job_find->current_page);
	matches = ev_document_find_find_text (find, ev_page, job_find->text,
					      job_find->options);
	g_object_unref (ev_page);

	ev_document_doc_mutex_unlock ();

	if (!job_find->has_results)
		job_find->has_results = (matches != NULL);

	job_find->pages[job_find->current_page] = matches;
	g_signal_emit (job_find, job_find_signals[FIND_UPDATED], 0, job_find->current_page);

	job_find->current_page = (job_find->current_page + 1) % job_find->n_pages;
	if (job_find->current_page == job_find->start_page) {
		ev_job_succeeded (job);
#ifdef EV_ENABLE_DEBUG
		/* Because we skipped EV_PROFILER_START () to escape the
		   scope, we must declare sysprof_name, which otherwise would
		   be in the macro
		 */
		const char* sysprof_name  = EV_GET_TYPE_NAME (job);
		EV_PROFILER_STOP ();
#endif
		return FALSE;
	}

	return TRUE;
}

static void
ev_job_find_class_init (EvJobFindClass *class)
{
	EvJobClass   *job_class = EV_JOB_CLASS (class);
	GObjectClass *gobject_class = G_OBJECT_CLASS (class);

	job_class->run = ev_job_find_run;
	gobject_class->dispose = ev_job_find_dispose;

	job_find_signals[FIND_UPDATED] =
		g_signal_new ("updated",
			      EV_TYPE_JOB_FIND,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EvJobFindClass, updated),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE,
			      1, G_TYPE_INT);
}

EvJob *
ev_job_find_new (EvDocument    *document,
		 gint           start_page,
		 gint           n_pages,
		 const gchar   *text,
		 EvFindOptions  options)
{
	EvJobFind *job;

	ev_debug_message (DEBUG_JOBS, NULL);

	job = g_object_new (EV_TYPE_JOB_FIND, NULL);

	EV_JOB (job)->document = g_object_ref (document);
	job->start_page = start_page;
	job->current_page = start_page;
	job->n_pages = n_pages;
	job->pages = g_new0 (GList *, n_pages);
	job->text = g_strdup (text);
	job->has_results = FALSE;
	job->options = options;

	return EV_JOB (job);
}

/**
 * ev_job_find_get_options:
 * @job:
 *
 * Returns: the job's find options
 *
 * Since: 3.6
 */
EvFindOptions
ev_job_find_get_options (EvJobFind *job)
{
        return job->options;
}

/**
 * ev_job_find_get_n_main_results:
 * @job: an #EvJobFind job
 * @page: number of the page we want to count its match results.
 *
 * This is similar to ev_job_find_get_n_results() but it takes
 * care to treat any multi-line matches as being only one result.
 *
 * Returns: total number of match results in @page
 */
gint
ev_job_find_get_n_main_results (EvJobFind *job,
				gint       page)
{
	GList *l;
	int n = 0;

	for (l = job->pages[page]; l; l = l->next) {
		if ( !((EvFindRectangle *) l->data)->next_line )
			n++;
	}

	return n;
}

gdouble
ev_job_find_get_progress (EvJobFind *job)
{
	gint pages_done;

	if (ev_job_is_finished (EV_JOB (job)))
		return 1.0;

	if (job->current_page > job->start_page) {
		pages_done = job->current_page - job->start_page + 1;
	} else if (job->current_page == job->start_page) {
		pages_done = job->n_pages;
	} else {
		pages_done = job->n_pages - job->start_page + job->current_page;
	}

	return pages_done / (gdouble) job->n_pages;
}

gboolean
ev_job_find_has_results (EvJobFind *job)
{
	return job->has_results;
}

/**
 * ev_job_find_get_results: (skip)
 * @job: an #EvJobFind
 *
 * Returns: a #GList of #GList<!-- -->s containing #EvFindRectangle<!-- -->s
 */
GList **
ev_job_find_get_results (EvJobFind *job)
{
	return job->pages;
}

/* EvJobLayers */
static void
ev_job_layers_init (EvJobLayers *job)
{
	EV_JOB (job)->run_mode = EV_JOB_RUN_THREAD;
}

static void
ev_job_layers_dispose (GObject *object)
{
	EvJobLayers *job;

	ev_debug_message (DEBUG_JOBS, NULL);

	job = EV_JOB_LAYERS (object);

	g_clear_object (&job->model);

	(* G_OBJECT_CLASS (ev_job_layers_parent_class)->dispose) (object);
}

static gboolean
ev_job_layers_run (EvJob *job)
{
	EvJobLayers *job_layers = EV_JOB_LAYERS (job);

	ev_debug_message (DEBUG_JOBS, NULL);
	EV_PROFILER_START (EV_GET_TYPE_NAME (job));

	ev_document_doc_mutex_lock ();
	job_layers->model = ev_document_layers_get_layers (EV_DOCUMENT_LAYERS (job->document));
	ev_document_doc_mutex_unlock ();

	ev_job_succeeded (job);

	EV_PROFILER_STOP ();
	return FALSE;
}

static void
ev_job_layers_class_init (EvJobLayersClass *class)
{
	GObjectClass *oclass = G_OBJECT_CLASS (class);
	EvJobClass   *job_class = EV_JOB_CLASS (class);

	oclass->dispose = ev_job_layers_dispose;
	job_class->run = ev_job_layers_run;
}

EvJob *
ev_job_layers_new (EvDocument *document)
{
	EvJob *job;

	ev_debug_message (DEBUG_JOBS, NULL);

	job = g_object_new (EV_TYPE_JOB_LAYERS, NULL);
	job->document = g_object_ref (document);

	return job;
}

/* EvJobExport */
static void
ev_job_export_init (EvJobExport *job)
{
	EV_JOB (job)->run_mode = EV_JOB_RUN_THREAD;
	job->page = -1;
}

static void
ev_job_export_dispose (GObject *object)
{
	EvJobExport *job;

	ev_debug_message (DEBUG_JOBS, NULL);

	job = EV_JOB_EXPORT (object);

	g_clear_object (&job->rc);

	(* G_OBJECT_CLASS (ev_job_export_parent_class)->dispose) (object);
}

static gboolean
ev_job_export_run (EvJob *job)
{
	EvJobExport *job_export = EV_JOB_EXPORT (job);
	EvPage      *ev_page;

	g_assert (job_export->page != -1);

	ev_debug_message (DEBUG_JOBS, NULL);
	EV_PROFILER_START (EV_GET_TYPE_NAME (job));

	ev_document_doc_mutex_lock ();

	ev_page = ev_document_get_page (job->document, job_export->page);
	if (job_export->rc) {
		job->failed = FALSE;
		job->finished = FALSE;
		g_clear_error (&job->error);

		ev_render_context_set_page (job_export->rc, ev_page);
	} else {
		job_export->rc = ev_render_context_new (ev_page, 0, 1.0);
	}
	g_object_unref (ev_page);

	ev_file_exporter_do_page (EV_FILE_EXPORTER (job->document), job_export->rc);

	ev_document_doc_mutex_unlock ();

	ev_job_succeeded (job);

	EV_PROFILER_STOP ();
	return FALSE;
}

static void
ev_job_export_class_init (EvJobExportClass *class)
{
	GObjectClass *oclass = G_OBJECT_CLASS (class);
	EvJobClass   *job_class = EV_JOB_CLASS (class);

	oclass->dispose = ev_job_export_dispose;
	job_class->run = ev_job_export_run;
}

EvJob *
ev_job_export_new (EvDocument *document)
{
	EvJob *job;

	ev_debug_message (DEBUG_JOBS, NULL);

	job = g_object_new (EV_TYPE_JOB_EXPORT, NULL);
	job->document = g_object_ref (document);

	return job;
}

void
ev_job_export_set_page (EvJobExport *job,
			gint         page)
{
	job->page = page;
}

/* EvJobPrint */
static void
ev_job_print_init (EvJobPrint *job)
{
	EV_JOB (job)->run_mode = EV_JOB_RUN_THREAD;
	job->page = -1;
}

static void
ev_job_print_dispose (GObject *object)
{
	EvJobPrint *job;

	ev_debug_message (DEBUG_JOBS, NULL);

	job = EV_JOB_PRINT (object);

	g_clear_pointer (&job->cr, cairo_destroy);

	(* G_OBJECT_CLASS (ev_job_print_parent_class)->dispose) (object);
}

static gboolean
ev_job_print_run (EvJob *job)
{
	EvJobPrint     *job_print = EV_JOB_PRINT (job);
	EvPage         *ev_page;
	cairo_status_t  cr_status;

	g_assert (job_print->page != -1);
	g_assert (job_print->cr != NULL);

	ev_debug_message (DEBUG_JOBS, NULL);
	EV_PROFILER_START (EV_GET_TYPE_NAME (job));

	job->failed = FALSE;
	job->finished = FALSE;
	g_clear_error (&job->error);

	ev_document_doc_mutex_lock ();

	ev_page = ev_document_get_page (job->document, job_print->page);
	ev_document_print_print_page (EV_DOCUMENT_PRINT (job->document),
				      ev_page, job_print->cr);
	g_object_unref (ev_page);

	ev_document_doc_mutex_unlock ();

	if (g_cancellable_is_cancelled (job->cancellable)) {
		EV_PROFILER_STOP ();
		return FALSE;
	}

	cr_status = cairo_status (job_print->cr);
	if (cr_status == CAIRO_STATUS_SUCCESS) {
		ev_job_succeeded (job);
	} else {
		ev_job_failed (job,
			       GTK_PRINT_ERROR,
			       GTK_PRINT_ERROR_GENERAL,
			       _("Failed to print page %d: %s"),
			       job_print->page,
			       cairo_status_to_string (cr_status));
	}

	EV_PROFILER_STOP ();
	return FALSE;
}

static void
ev_job_print_class_init (EvJobPrintClass *class)
{
	GObjectClass *oclass = G_OBJECT_CLASS (class);
	EvJobClass   *job_class = EV_JOB_CLASS (class);

	oclass->dispose = ev_job_print_dispose;
	job_class->run = ev_job_print_run;
}

EvJob *
ev_job_print_new (EvDocument *document)
{
	EvJob *job;

	ev_debug_message (DEBUG_JOBS, NULL);

	job = g_object_new (EV_TYPE_JOB_PRINT, NULL);
	job->document = g_object_ref (document);

	return job;
}

void
ev_job_print_set_page (EvJobPrint *job,
		       gint        page)
{
	job->page = page;
}

void
ev_job_print_set_cairo (EvJobPrint *job,
			cairo_t    *cr)
{
	if (job->cr == cr)
		return;

	if (job->cr)
		cairo_destroy (job->cr);
	job->cr = cr ? cairo_reference (cr) : NULL;
}
