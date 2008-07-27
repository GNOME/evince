#include <config.h>
#include "ev-jobs.h"
#include "ev-job-queue.h"
#include "ev-document-thumbnails.h"
#include "ev-document-links.h"
#include "ev-document-images.h"
#include "ev-document-forms.h"
#include "ev-file-exporter.h"
#include "ev-document-factory.h"
#include "ev-document-misc.h"
#include "ev-file-helpers.h"
#include "ev-document-fonts.h"
#include "ev-async-renderer.h"
#include "ev-debug.h"

#include <errno.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <unistd.h>

static void ev_job_init                 (EvJob               *job);
static void ev_job_class_init           (EvJobClass          *class);
static void ev_job_links_init           (EvJobLinks          *job);
static void ev_job_links_class_init     (EvJobLinksClass     *class);
static void ev_job_render_init          (EvJobRender         *job);
static void ev_job_render_class_init    (EvJobRenderClass    *class);
static void ev_job_thumbnail_init       (EvJobThumbnail      *job);
static void ev_job_thumbnail_class_init (EvJobThumbnailClass *class);
static void ev_job_load_init    	(EvJobLoad	     *job);
static void ev_job_load_class_init 	(EvJobLoadClass	     *class);
static void ev_job_save_init            (EvJobSave           *job);
static void ev_job_save_class_init      (EvJobSaveClass      *class);
static void ev_job_print_init           (EvJobPrint          *job);
static void ev_job_print_class_init     (EvJobPrintClass     *class);

enum {
	FINISHED,
	LAST_SIGNAL
};

enum {
	PAGE_READY,
	RENDER_LAST_SIGNAL
};

static guint job_signals[LAST_SIGNAL] = { 0 };
static guint job_render_signals[RENDER_LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (EvJob, ev_job, G_TYPE_OBJECT)
G_DEFINE_TYPE (EvJobLinks, ev_job_links, EV_TYPE_JOB)
G_DEFINE_TYPE (EvJobRender, ev_job_render, EV_TYPE_JOB)
G_DEFINE_TYPE (EvJobThumbnail, ev_job_thumbnail, EV_TYPE_JOB)
G_DEFINE_TYPE (EvJobFonts, ev_job_fonts, EV_TYPE_JOB)
G_DEFINE_TYPE (EvJobLoad, ev_job_load, EV_TYPE_JOB)
G_DEFINE_TYPE (EvJobSave, ev_job_save, EV_TYPE_JOB)
G_DEFINE_TYPE (EvJobPrint, ev_job_print, EV_TYPE_JOB)

static void ev_job_init (EvJob *job) { /* Do Nothing */ }

static void
ev_job_dispose (GObject *object)
{
	EvJob *job;

	job = EV_JOB (object);

	if (job->document) {
		g_object_unref (job->document);
		job->document = NULL;
	}

	(* G_OBJECT_CLASS (ev_job_parent_class)->dispose) (object);
}

static void
ev_job_class_init (EvJobClass *class)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (class);

	oclass->dispose = ev_job_dispose;

	job_signals [FINISHED] =
		g_signal_new ("finished",
			      EV_TYPE_JOB,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EvJobClass, finished),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}


static void ev_job_links_init (EvJobLinks *job) { /* Do Nothing */ }

static void
ev_job_links_dispose (GObject *object)
{
	EvJobLinks *job;

	ev_debug_message (DEBUG_JOBS, NULL);
	
	job = EV_JOB_LINKS (object);

	if (job->model) {
		g_object_unref (job->model);
		job->model = NULL;
	}

	(* G_OBJECT_CLASS (ev_job_links_parent_class)->dispose) (object);
}

static void
ev_job_links_class_init (EvJobLinksClass *class)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (class);

	oclass->dispose = ev_job_links_dispose;
}


static void ev_job_render_init (EvJobRender *job) { /* Do Nothing */ }

static void
ev_job_render_dispose (GObject *object)
{
	EvJobRender *job;

	job = EV_JOB_RENDER (object);

	if (job->ev_page) {
		ev_debug_message (DEBUG_JOBS, "page: %d", job->ev_page->index);
		g_object_unref (job->ev_page);
		job->ev_page = NULL;
	}
	
	if (job->surface) {
		cairo_surface_destroy (job->surface);
		job->surface = NULL;
	}

	if (job->selection) {
		cairo_surface_destroy (job->selection);
		job->selection = NULL;
	}

	if (job->selection_region) {
		gdk_region_destroy (job->selection_region);
		job->selection_region = NULL;
	}

	(* G_OBJECT_CLASS (ev_job_render_parent_class)->dispose) (object);
}

static void
ev_job_render_class_init (EvJobRenderClass *class)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (class);

	job_render_signals [PAGE_READY] =
		g_signal_new ("page-ready",
			      EV_TYPE_JOB_RENDER,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EvJobRenderClass, page_ready),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	oclass->dispose = ev_job_render_dispose;
}

static void ev_job_thumbnail_init (EvJobThumbnail *job) { /* Do Nothing */ }

static void
ev_job_thumbnail_dispose (GObject *object)
{
	EvJobThumbnail *job;

	job = EV_JOB_THUMBNAIL (object);

	ev_debug_message (DEBUG_JOBS, "%d", job->page);
	
	if (job->thumbnail) {
		g_object_unref (job->thumbnail);
		job->thumbnail = NULL;
	}

	(* G_OBJECT_CLASS (ev_job_thumbnail_parent_class)->dispose) (object);
}

static void
ev_job_thumbnail_class_init (EvJobThumbnailClass *class)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (class);

	oclass->dispose = ev_job_thumbnail_dispose;
}

static void ev_job_print_init (EvJobPrint *job) { /* Do Nothing */ }

static void
ev_job_print_dispose (GObject *object)
{
	EvJobPrint *job;

	job = EV_JOB_PRINT (object);

	ev_debug_message (DEBUG_JOBS, NULL);
	
	if (job->temp_file) {
		g_unlink (job->temp_file);
		g_free (job->temp_file);
		job->temp_file = NULL;
	}

	if (job->error) {
		g_error_free (job->error);
		job->error = NULL;
	}

	if (job->ranges) {
		g_free (job->ranges);
		job->ranges = NULL;
		job->n_ranges = 0;
	}

	(* G_OBJECT_CLASS (ev_job_print_parent_class)->dispose) (object);
}

static void
ev_job_print_class_init (EvJobPrintClass *class)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (class);

	oclass->dispose = ev_job_print_dispose;
}

/* Public functions */
void
ev_job_finished (EvJob *job)
{
	ev_debug_message (DEBUG_JOBS, EV_GET_TYPE_NAME (job));
	ev_profiler_stop (EV_PROFILE_JOBS, "%s (%p)", EV_GET_TYPE_NAME (job), job);
	
	g_return_if_fail (EV_IS_JOB (job));
	
	g_signal_emit (job, job_signals[FINISHED], 0);
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

void
ev_job_links_run (EvJobLinks *job)
{
	ev_debug_message (DEBUG_JOBS, NULL);
	ev_profiler_start (EV_PROFILE_JOBS, "%s (%p)", EV_GET_TYPE_NAME (job), job);
	
	g_return_if_fail (EV_IS_JOB_LINKS (job));

	ev_document_doc_mutex_lock ();
	job->model = ev_document_links_get_links_model (EV_DOCUMENT_LINKS (EV_JOB (job)->document));
	EV_JOB (job)->finished = TRUE;
	ev_document_doc_mutex_unlock ();
}

EvJob *
ev_job_render_new (EvDocument   *document,
		   gint          page,
		   gint          rotation,
		   gdouble       scale, 
		   gint          width,
		   gint          height,
		   EvRenderFlags flags)
{
	EvJobRender *job;

	ev_debug_message (DEBUG_JOBS, "page: %d", page);
	
	job = g_object_new (EV_TYPE_JOB_RENDER, NULL);

	EV_JOB (job)->document = g_object_ref (document);
	job->page = page;
	job->rotation = rotation;
	job->scale = scale;
	job->target_width = width;
	job->target_height = height;
	job->flags = flags;

	if (EV_IS_ASYNC_RENDERER (document)) {	
		EV_JOB (job)->async = TRUE;
	}

	return EV_JOB (job);
}

void
ev_job_render_set_selection_info (EvJobRender     *job,
				  EvRectangle     *selection_points,
				  EvSelectionStyle selection_style,
				  GdkColor        *text,
				  GdkColor        *base)
{
	job->flags |= EV_RENDER_INCLUDE_SELECTION;
	
	job->selection_points = *selection_points;
	job->selection_style = selection_style;
	job->text = *text;
	job->base = *base;
}

static void
render_finished_cb (EvDocument      *document,
		    GdkPixbuf       *pixbuf,
		    EvJobRender     *job)
{
	g_signal_handlers_disconnect_by_func (EV_JOB (job)->document,
					      render_finished_cb, job);

	job->surface = ev_document_misc_surface_from_pixbuf (pixbuf);
	job->page_ready = TRUE;
	g_signal_emit (job, job_render_signals[PAGE_READY], 0);
	EV_JOB (job)->finished = TRUE;
	ev_job_finished (EV_JOB (job));
}

static gboolean
notify_page_ready (EvJobRender *job)
{
	ev_debug_message (DEBUG_JOBS, "%d", job->ev_page->index);
	ev_profiler_stop (EV_PROFILE_JOBS, "Rendering page %d", job->ev_page->index);
	
	g_signal_emit (job, job_render_signals[PAGE_READY], 0);

	return FALSE;
}

static void
ev_job_render_page_ready (EvJobRender *job)
{
	ev_debug_message (DEBUG_JOBS, "%d", job->ev_page->index);
	
	job->page_ready = TRUE;
	g_idle_add_full (G_PRIORITY_HIGH_IDLE,
			 (GSourceFunc)notify_page_ready,
			 g_object_ref (job),
			 (GDestroyNotify)g_object_unref);
}

void
ev_job_render_run (EvJobRender *job)
{
	g_return_if_fail (EV_IS_JOB_RENDER (job));

	ev_debug_message (DEBUG_JOBS, "page: %d", job->page);
	ev_profiler_start (EV_PROFILE_JOBS, "%s (%p)", EV_GET_TYPE_NAME (job), job);
	
	ev_document_doc_mutex_lock ();

	if (EV_JOB (job)->async) {
		EvAsyncRenderer *renderer = EV_ASYNC_RENDERER (EV_JOB (job)->document);
		ev_async_renderer_render_pixbuf (renderer, job->page, job->scale,
						 job->rotation);
		g_signal_connect (EV_JOB (job)->document, "render_finished",
				  G_CALLBACK (render_finished_cb), job);
	} else {
		EvRenderContext *rc;

		ev_profiler_start (EV_PROFILE_JOBS, "Rendering page %d", job->page);
		
		ev_document_fc_mutex_lock ();

		job->ev_page = ev_document_get_page (EV_JOB (job)->document, job->page);

		rc = ev_render_context_new (job->ev_page, job->rotation, job->scale);
		
		job->surface = ev_document_render (EV_JOB (job)->document, rc);
		if ((job->flags & EV_RENDER_INCLUDE_SELECTION) && EV_IS_SELECTION (EV_JOB (job)->document)) {
			ev_selection_render_selection (EV_SELECTION (EV_JOB (job)->document),
						       rc,
						       &(job->selection),
						       &(job->selection_points),
						       NULL,
						       job->selection_style,
						       &(job->text), &(job->base));
			job->selection_region =
				ev_selection_get_selection_region (EV_SELECTION (EV_JOB (job)->document),
								   rc,
								   job->selection_style,
								   &(job->selection_points));
		}

		ev_job_render_page_ready (job);
		
		ev_document_fc_mutex_unlock ();
		
		if ((job->flags & EV_RENDER_INCLUDE_TEXT) && EV_IS_SELECTION (EV_JOB (job)->document))
			job->text_mapping =
				ev_selection_get_selection_map (EV_SELECTION (EV_JOB (job)->document), rc);
		if ((job->flags & EV_RENDER_INCLUDE_LINKS) && EV_IS_DOCUMENT_LINKS (EV_JOB (job)->document))
			job->link_mapping =
				ev_document_links_get_links (EV_DOCUMENT_LINKS (EV_JOB (job)->document), job->page);
		if ((job->flags & EV_RENDER_INCLUDE_FORMS) && EV_IS_DOCUMENT_FORMS (EV_JOB (job)->document))
			job->form_field_mapping =
				ev_document_forms_get_form_fields (EV_DOCUMENT_FORMS (EV_JOB (job)->document),
								   job->ev_page);
		if ((job->flags & EV_RENDER_INCLUDE_IMAGES) && EV_IS_DOCUMENT_IMAGES (EV_JOB (job)->document))
			job->image_mapping =
				ev_document_images_get_image_mapping (EV_DOCUMENT_IMAGES (EV_JOB (job)->document),
								      job->page);
		g_object_unref (rc);
		
		EV_JOB (job)->finished = TRUE;
	}

	ev_document_doc_mutex_unlock ();
}

EvJob *
ev_job_thumbnail_new (EvDocument *document,
		      gint        page,
		      gint        rotation,
		      gdouble     scale)
{
	EvJobThumbnail *job;

	ev_debug_message (DEBUG_JOBS, "%d", page);
	
	job = g_object_new (EV_TYPE_JOB_THUMBNAIL, NULL);

	EV_JOB (job)->document = g_object_ref (document);
	job->page = page;
	job->rotation = rotation;
	job->scale = scale;

	return EV_JOB (job);
}

void
ev_job_thumbnail_run (EvJobThumbnail *job)
{
	EvRenderContext *rc;
	EvPage          *page;

	ev_debug_message (DEBUG_JOBS, "%d", job->page);
	ev_profiler_start (EV_PROFILE_JOBS, "%s (%p)", EV_GET_TYPE_NAME (job), job);
	
	g_return_if_fail (EV_IS_JOB_THUMBNAIL (job));

	ev_document_doc_mutex_lock ();

	page = ev_document_get_page (EV_JOB (job)->document, job->page);
	rc = ev_render_context_new (page, job->rotation, job->scale);
	g_object_unref (page);

	job->thumbnail =
		ev_document_thumbnails_get_thumbnail (EV_DOCUMENT_THUMBNAILS (EV_JOB (job)->document),
						      rc, TRUE);
	g_object_unref (rc);
	ev_document_doc_mutex_unlock ();
	
	EV_JOB (job)->finished = TRUE;
}

static void ev_job_fonts_init (EvJobFonts *job) { /* Do Nothing */ }

static void ev_job_fonts_class_init (EvJobFontsClass *class) { /* Do Nothing */ }

EvJob *
ev_job_fonts_new (EvDocument *document)
{
	EvJobFonts *job;

	ev_debug_message (DEBUG_JOBS, NULL);
	
	job = g_object_new (EV_TYPE_JOB_FONTS, NULL);

	EV_JOB (job)->document = g_object_ref (document);

	return EV_JOB (job);
}

void
ev_job_fonts_run (EvJobFonts *job)
{
	EvDocumentFonts *fonts;

	ev_debug_message (DEBUG_JOBS, NULL);
	ev_profiler_start (EV_PROFILE_JOBS, "%s (%p)", EV_GET_TYPE_NAME (job), job);
	
	g_return_if_fail (EV_IS_JOB_FONTS (job));

	ev_document_doc_mutex_lock ();
	
	fonts = EV_DOCUMENT_FONTS (EV_JOB (job)->document);
	ev_document_fc_mutex_lock ();
	job->scan_completed = !ev_document_fonts_scan (fonts, 20);
	ev_document_fc_mutex_unlock ();
	
	EV_JOB (job)->finished = TRUE;

	ev_document_doc_mutex_unlock ();
}

static void ev_job_load_init (EvJobLoad *job) { /* Do Nothing */ }

static void
ev_job_load_dispose (GObject *object)
{
	EvJobLoad *job = EV_JOB_LOAD (object);

	ev_debug_message (DEBUG_JOBS, "%s", job->uri);
	
	if (job->uri) {
		g_free (job->uri);
		job->uri = NULL;
	}

	if (job->error) {
		g_error_free (job->error);
		job->error = NULL;
	}

	if (job->dest) {
		g_object_unref (job->dest);
		job->dest = NULL;
	}

	if (job->search_string) {
		g_free (job->search_string);
		job->search_string = NULL;
	}

	(* G_OBJECT_CLASS (ev_job_load_parent_class)->dispose) (object);
}

static void
ev_job_load_class_init (EvJobLoadClass *class)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (class);

	oclass->dispose = ev_job_load_dispose;
}


EvJob *
ev_job_load_new (const gchar    *uri,
		 EvLinkDest     *dest,
		 EvWindowRunMode mode,
		 const gchar    *search_string)
{
	EvJobLoad *job;

	ev_debug_message (DEBUG_JOBS, "%s", uri);
	
	job = g_object_new (EV_TYPE_JOB_LOAD, NULL);

	job->uri = g_strdup (uri);
	if (dest)
		job->dest = g_object_ref (dest);

	job->mode = mode;
	if (search_string)
		job->search_string = g_strdup (search_string);

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
ev_job_load_run (EvJobLoad *job)
{
	g_return_if_fail (EV_IS_JOB_LOAD (job));

	ev_debug_message (DEBUG_JOBS, "%s", job->uri);
	ev_profiler_start (EV_PROFILE_JOBS, "%s (%p)", EV_GET_TYPE_NAME (job), job);
	
	if (job->error) {
	        g_error_free (job->error);
		job->error = NULL;
	}

	ev_document_fc_mutex_lock ();
	
	/* This job may already have a document even if the job didn't complete
	   because, e.g., a password is required - if so, just reload rather than
	   creating a new instance */
	if (EV_JOB (job)->document) {
		ev_document_load (EV_JOB (job)->document,
				  job->uri,
				  &job->error);
	} else {
		EV_JOB(job)->document =
			ev_document_factory_get_document (job->uri,
							  &job->error);
	}

	ev_document_fc_mutex_unlock ();
	EV_JOB (job)->finished = TRUE;
}

static void ev_job_save_init (EvJobSave *job) { /* Do Nothing */ }

static void
ev_job_save_dispose (GObject *object)
{
	EvJobSave *job = EV_JOB_SAVE (object);

	ev_debug_message (DEBUG_JOBS, "%s", job->uri);
	
	if (job->uri) {
		g_free (job->uri);
		job->uri = NULL;
	}

	if (job->document_uri) {
		g_free (job->document_uri);
		job->document_uri = NULL;
	}

	if (job->error) {
		g_error_free (job->error);
		job->error = NULL;
	}

	(* G_OBJECT_CLASS (ev_job_save_parent_class)->dispose) (object);
}

static void
ev_job_save_class_init (EvJobSaveClass *class)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (class);

	oclass->dispose = ev_job_save_dispose;
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
	job->error = NULL;

	return EV_JOB (job);
}

void
ev_job_save_run (EvJobSave *job)
{
	gint   fd;
	gchar *filename;
	gchar *tmp_filename;
	gchar *local_uri;

	ev_debug_message (DEBUG_JOBS, "uri: %s, document_uri: %s", job->uri, job->document_uri);
	ev_profiler_start (EV_PROFILE_JOBS, "%s (%p)", EV_GET_TYPE_NAME (job), job);
	
	filename = ev_tmp_filename ("saveacopy");
	tmp_filename = g_strdup_printf ("%s.XXXXXX", filename);
	g_free (filename);

	fd = g_mkstemp (tmp_filename);
	if (fd == -1) {
		gchar *display_name;
		gint   save_errno = errno;

		display_name = g_filename_display_name (tmp_filename);
		g_set_error (&(job->error),
			     G_FILE_ERROR,
			     g_file_error_from_errno (save_errno),
			     _("Failed to create file “%s”: %s"),
			     display_name, g_strerror (save_errno));
		g_free (display_name);
		g_free (tmp_filename);

		return;
	}

	ev_document_doc_mutex_lock ();

	/* Save document to temp filename */
	local_uri = g_filename_to_uri (tmp_filename, NULL, NULL);
	ev_document_save (EV_JOB (job)->document, local_uri, &(job->error));
	close (fd);

	ev_document_doc_mutex_unlock ();

	if (job->error) {
		g_free (local_uri);
		return;
	}

	/* If original document was compressed,
	 * compress it again before saving
	 */
	if (g_object_get_data (G_OBJECT (EV_JOB (job)->document),
			       "uri-uncompressed")) {
		EvCompressionType ctype = EV_COMPRESSION_NONE;
		const gchar      *ext;
		gchar            *uri_comp;
		
		ext = g_strrstr (job->document_uri, ".gz");
		if (ext && g_ascii_strcasecmp (ext, ".gz") == 0)
			ctype = EV_COMPRESSION_GZIP;
		
		ext = g_strrstr (job->document_uri, ".bz2");
		if (ext && g_ascii_strcasecmp (ext, ".bz2") == 0)
			ctype = EV_COMPRESSION_BZIP2;

		uri_comp = ev_file_compress (local_uri, ctype, &(job->error));
		g_free (local_uri);
		ev_tmp_filename_unlink (tmp_filename);

		if (!uri_comp || job->error) {
			local_uri = NULL;
		} else {
			local_uri = uri_comp;
		}
	}

	g_free (tmp_filename);
	
	if (job->error) {
		g_free (local_uri);
		return;
	}

	if (!local_uri)
		return;

	ev_xfer_uri_simple (local_uri, job->uri, &(job->error));
	ev_tmp_uri_unlink (local_uri);
}

EvJob *
ev_job_print_new (EvDocument    *document,
		  const gchar   *format,
		  gdouble        width,
		  gdouble        height,
		  EvPrintRange  *ranges,
		  gint           n_ranges,
		  EvPrintPageSet page_set,
		  gint           pages_per_sheet,
		  gint           copies,
		  gdouble        collate,
		  gdouble        reverse)
{
	EvJobPrint *job;

	ev_debug_message (DEBUG_JOBS, "format: %s, width: %f, height:%f,"
			  "n_ranges: %d, pages_per_sheet: %d, copies: %d,"
			  "collate: %s, reverse: %s",
			  format, width, height, n_ranges, pages_per_sheet, copies,
			  collate ? "True" : "False", reverse  ? "True" : "False");

	job = g_object_new (EV_TYPE_JOB_PRINT, NULL);

	EV_JOB (job)->document = g_object_ref (document);

	job->format = format;
	
	job->temp_file = NULL;
	job->error = NULL;

	job->width = width;
	job->height = height;

	job->ranges = ranges;
	job->n_ranges = n_ranges;

	job->page_set = page_set;

	job->pages_per_sheet = CLAMP (pages_per_sheet, 1, 16);
	
	job->copies = copies;
	job->collate = collate;
	job->reverse = reverse;
	
	return EV_JOB (job);
}

static gint
ev_print_job_get_first_page (EvJobPrint *job)
{
	gint i;
	gint first_page = G_MAXINT;
	
	if (job->n_ranges == 0)
		return 0;

	for (i = 0; i < job->n_ranges; i++) {
		if (job->ranges[i].start < first_page)
			first_page = job->ranges[i].start;
	}

	return MAX (0, first_page);
}

static gint
ev_print_job_get_last_page (EvJobPrint *job)
{
	gint i;
	gint last_page = G_MININT;
	gint max_page;

	max_page = ev_document_get_n_pages (EV_JOB (job)->document) - 1;

	if (job->n_ranges == 0)
		return max_page;

	for (i = 0; i < job->n_ranges; i++) {
		if (job->ranges[i].end > last_page)
			last_page = job->ranges[i].end;
	}

	return MIN (max_page, last_page);
}

static gboolean
ev_print_job_print_page_in_set (EvJobPrint *job,
				gint        page)
{
	switch (job->page_set) {
	        case EV_PRINT_PAGE_SET_EVEN:
			return page % 2 == 0;
	        case EV_PRINT_PAGE_SET_ODD:
			return page % 2 != 0;
	        case EV_PRINT_PAGE_SET_ALL:
			return TRUE;
	}

	return FALSE;
}

static gint *
ev_job_print_get_page_list (EvJobPrint *job,
			    gint       *n_pages)
{
	gint  i, j, page, max_page;
	gint  pages = 0;
	gint *page_list;

	max_page = ev_document_get_n_pages (EV_JOB (job)->document) - 1;

	for (i = 0; i < job->n_ranges; i++) {
		gint rsize;
		gint start, end;

		if (job->ranges[i].start == -1)
			job->ranges[i].start = 0;
		if (job->ranges[i].end == -1)
			job->ranges[i].end = max_page;

		if (job->ranges[i].start > max_page)
			continue;
		
		start = job->ranges[i].start + 1;
		end = job->ranges[i].end <= max_page ? job->ranges[i].end + 1 : max_page + 1;
		rsize = end - start + 1;

		switch (job->page_set) {
		        case EV_PRINT_PAGE_SET_EVEN:
				pages += start % 2 == 0 ? (rsize / 2) + (rsize % 2) : (rsize / 2);
				break;
		        case EV_PRINT_PAGE_SET_ODD:
				pages += start % 2 != 0 ? (rsize / 2) + (rsize % 2) : (rsize / 2);
				break;
		        default:
				pages += rsize;
				break;
		}
	}

	*n_pages = pages;

	if (pages == 0)
		return NULL;

	page_list = g_new (gint, pages);

	page = 0;
	for (i = 0; i < job->n_ranges; i++) {
		for (j = job->ranges[i].start; j <= job->ranges[i].end; j++) {
			if (j > max_page)
				break;
		
			if (ev_print_job_print_page_in_set (job, j + 1))
				page_list[page++] = j;
		}
	}

	return page_list;
}

void
ev_job_print_run (EvJobPrint *job)
{
	EvDocument            *document = EV_JOB (job)->document;
	EvFileExporterContext  fc;
	EvRenderContext       *rc;
	gint                   fd;
	gint                  *page_list;
	gint                   n_pages;
	gint                   last_page;
	gint                   first_page;
	gint                   i, j;
	gchar                 *filename;
	
	g_return_if_fail (EV_IS_JOB_PRINT (job));

	ev_debug_message (DEBUG_JOBS, NULL);
	ev_profiler_start (EV_PROFILE_JOBS, "%s (%p)", EV_GET_TYPE_NAME (job), job);
	
	if (job->temp_file)
		g_free (job->temp_file);
	job->temp_file = NULL;
	
	if (job->error)
		g_error_free (job->error);
	job->error = NULL;

	filename = g_strdup_printf ("evince_print.%s.XXXXXX", job->format);
	fd = g_file_open_tmp (filename, &job->temp_file, &job->error);
	g_free (filename);
	if (fd <= -1) {
		EV_JOB (job)->finished = TRUE;
		return;
	}

	page_list = ev_job_print_get_page_list (job, &n_pages);
	if (n_pages == 0) {
		close (fd);
		EV_JOB (job)->finished = TRUE;
		return;
	}

	first_page = ev_print_job_get_first_page (job);
	last_page = ev_print_job_get_last_page (job);

	fc.format = g_ascii_strcasecmp (job->format, "pdf") == 0 ?
		EV_FILE_FORMAT_PDF : EV_FILE_FORMAT_PS;
	fc.filename = job->temp_file;
	fc.first_page = MIN (first_page, last_page);
	fc.last_page = MAX (first_page, last_page);
	fc.paper_width = job->width;
	fc.paper_height = job->height;
	fc.duplex = FALSE;
	fc.pages_per_sheet = MAX (1, job->pages_per_sheet);

	rc = ev_render_context_new (NULL, 0, 1.0);

	ev_document_doc_mutex_lock ();
	ev_file_exporter_begin (EV_FILE_EXPORTER (document), &fc);

	for (i = 0; i < job->copies; i++) {
		gint page, step;
		gint n_copies;
		
		step = job->reverse ? -1 * job->pages_per_sheet : job->pages_per_sheet;
		page = job->reverse ? ((n_pages - 1) / job->pages_per_sheet) * job->pages_per_sheet : 0;
		n_copies = job->collate ? 1 : job->copies;

		while ((job->reverse && (page >= 0)) || (!job->reverse && (page < n_pages))) {
			gint k;

			for (k = 0; k < n_copies; k++) {
				ev_file_exporter_begin_page (EV_FILE_EXPORTER (document));
				
				for (j = 0; j < job->pages_per_sheet; j++) {
					EvPage *ev_page;
					
					gint p = page + j;

					if (p < 0 || p >= n_pages)
						break;

					ev_page = ev_document_get_page (document, page_list[p]);
					ev_render_context_set_page (rc, ev_page);
					g_object_unref (ev_page);
					
					ev_file_exporter_do_page (EV_FILE_EXPORTER (document), rc);
				}

				ev_file_exporter_end_page (EV_FILE_EXPORTER (document));
			}

			page += step;
		}

		if (!job->collate)
			break;
	}

	ev_file_exporter_end (EV_FILE_EXPORTER (document));
	ev_document_doc_mutex_unlock ();
	
	g_free (page_list);
	close (fd);
	g_object_unref (rc);
	
	EV_JOB (job)->finished = TRUE;
}
