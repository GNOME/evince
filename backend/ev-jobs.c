#include "ev-jobs.h"
#include "ev-document-thumbnails.h"

static void ev_job_render_init          (EvJobRender         *job);
static void ev_job_render_class_init    (EvJobRenderClass    *class);
static void ev_job_thumbnail_init       (EvJobThumbnail      *job);
static void ev_job_thumbnail_class_init (EvJobThumbnailClass *class);

enum
{
	FINISHED,
	LAST_SIGNAL
};

static guint render_signals[LAST_SIGNAL] = { 0 };
static guint thumbnail_signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (EvJobRender, ev_job_render, G_TYPE_OBJECT)
G_DEFINE_TYPE (EvJobThumbnail, ev_job_thumbnail, G_TYPE_OBJECT)

static void
ev_job_render_init (EvJobRender *job)
{
	
}

static void
ev_job_render_dispose (GObject *object)
{
	EvJobRender *job;

	job = EV_JOB_RENDER (object);

	if (job->pixbuf) {
		g_object_unref (job->pixbuf);
		job->pixbuf = NULL;
	}
}

static void
ev_job_render_class_init (EvJobRenderClass *class)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (class);

	oclass->dispose = ev_job_render_dispose;

	render_signals [FINISHED] =
		g_signal_new ("finished",
			      EV_TYPE_JOB_RENDER,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EvJobRenderClass, finished),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

static void
ev_job_thumbnail_init (EvJobThumbnail *job)
{
	
}

static void
ev_job_thumbnail_dispose (GObject *object)
{
	EvJobThumbnail *job;

	job = EV_JOB_THUMBNAIL (object);

	if (job->thumbnail) {
		g_object_unref (job->thumbnail);
		job->thumbnail = NULL;
	}
}

static void
ev_job_thumbnail_class_init (EvJobThumbnailClass *class)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (class);

	oclass->dispose = ev_job_thumbnail_dispose;

	thumbnail_signals [FINISHED] =
		g_signal_new ("finished",
			      EV_TYPE_JOB_THUMBNAIL,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EvJobThumbnailClass, finished),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

/* Public functions */

EvJobRender *
ev_job_render_new (EvDocument *document,
		   gint        page,
		   double      scale,
		   gint        width,
		   gint        height)
{
	EvJobRender *job;

	job = g_object_new (EV_TYPE_JOB_RENDER, NULL);

	job->document = document;
	job->page = page;
	job->scale = scale;
	job->target_width = width;
	job->target_height = height;

	return job;
}

void
ev_job_render_run (EvJobRender *job)
{
	g_return_if_fail (EV_IS_JOB_RENDER (job));
	
	ev_document_set_scale (job->document, job->scale);
	job->pixbuf = ev_document_render_pixbuf (job->document);
	job->pixbuf_done = TRUE;
}

void
ev_job_render_finished (EvJobRender *job)
{
	g_return_if_fail (EV_IS_JOB_RENDER (job));

	g_signal_emit (job, render_signals[FINISHED], 0);
}

EvJobThumbnail *
ev_job_thumbnail_new (EvDocument *document,
		      gint        page,
		      gint        requested_width)
{
	EvJobThumbnail *job;

	job = g_object_new (EV_TYPE_JOB_THUMBNAIL, NULL);

	job->document = document;
	job->page = page;
	job->requested_width = requested_width;

	return job;
}

void
ev_job_thumbnail_run (EvJobThumbnail *job)
{
	g_return_if_fail (EV_IS_JOB_THUMBNAIL (job));

	job->thumbnail = ev_document_thumbnails_get_thumbnail (EV_DOCUMENT_THUMBNAILS (job->document),
							       job->page,
							       job->requested_width,
							       TRUE);
	job->thumbnail_done = TRUE;
}

void
ev_job_thumbnail_finished (EvJobThumbnail *job)
{
	g_return_if_fail (EV_IS_JOB_THUMBNAIL (job));

	g_signal_emit (job, thumbnail_signals[FINISHED], 0);
}
