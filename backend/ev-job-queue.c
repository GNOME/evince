#include "ev-job-queue.h"

/* Like glib calling convention, all functions with _locked in their name assume
 * that we've already locked the doc mutex and can freely and safely access
 * data.
 */
GCond *render_cond = NULL;
GMutex *ev_doc_mutex = NULL;

static GQueue *render_queue_high = NULL;
static GQueue *render_queue_low = NULL;
static GQueue *thumbnail_queue_high = NULL;
static GQueue *thumbnail_queue_low = NULL;

static gboolean
notify_finished (gpointer job)
{
	if (EV_IS_JOB_THUMBNAIL (job))
		ev_job_thumbnail_finished (EV_JOB_THUMBNAIL (job));
	else if (EV_IS_JOB_RENDER (job))
		ev_job_render_finished (EV_JOB_RENDER (job));
	return FALSE;
}


static void
handle_job_unlocked (gpointer job)
{
	if (EV_IS_JOB_THUMBNAIL (job))
		ev_job_thumbnail_run (EV_JOB_THUMBNAIL (job));
	else if (EV_IS_JOB_RENDER (job))
		ev_job_render_run (EV_JOB_RENDER (job));

	g_idle_add (notify_finished, job);
}

static void
search_for_jobs_unlocked (void)
{
	EvJobRender *render_job;
	EvJobThumbnail *thumbnail_job;

	render_job = (EvJobRender *) g_queue_pop_head (render_queue_high);
	if (render_job) {
		handle_job_unlocked (render_job);
		return;
	}

	thumbnail_job = (EvJobThumbnail *) g_queue_pop_head (thumbnail_queue_high);
	if (thumbnail_job) {
		handle_job_unlocked (thumbnail_job);
		return;
	}

	render_job = (EvJobRender *) g_queue_pop_head (render_queue_high);
	if (render_job) {
		handle_job_unlocked (render_job);
		return;
	}

	thumbnail_job = (EvJobThumbnail *) g_queue_pop_head (thumbnail_queue_low);
	if (thumbnail_job) {
		handle_job_unlocked (thumbnail_job);
		return;
	}
}

static gboolean
no_jobs_available_unlocked (void)
{
	return g_queue_is_empty (render_queue_high)
		&& g_queue_is_empty (render_queue_low)
		&& g_queue_is_empty (thumbnail_queue_high)
		&& g_queue_is_empty (thumbnail_queue_low);
}

/* the thread mainloop function */
static gpointer
ev_render_thread (gpointer data)
{
	while (TRUE) {
		g_mutex_lock (EV_DOC_MUTEX);

		if (no_jobs_available_unlocked ())
			g_cond_wait (render_cond, ev_doc_mutex);

		search_for_jobs_unlocked ();
		
		g_mutex_unlock (EV_DOC_MUTEX);
	}
	return NULL;

}

/* Public Functions */
void
ev_job_queue_init (void)
{
	if (!g_thread_supported ()) g_thread_init (NULL);

	render_cond = g_cond_new ();
	ev_doc_mutex = g_mutex_new ();

	render_queue_high = g_queue_new ();
	render_queue_low = g_queue_new ();
	thumbnail_queue_high = g_queue_new ();
	thumbnail_queue_low = g_queue_new ();

	g_thread_create (ev_render_thread, NULL, FALSE, NULL);

}

GMutex *
ev_job_queue_get_doc_mutex (void)
{
	g_assert (ev_doc_mutex);
	return ev_doc_mutex;
}

void
ev_job_queue_add_render_job (EvJobRender   *job,
			     EvJobPriority  priority)
{
	g_return_if_fail (job != NULL);

	g_mutex_lock (EV_DOC_MUTEX);

	g_object_ref (job);
	if (priority == EV_JOB_PRIORITY_LOW)
		g_queue_push_tail (render_queue_low, job);
	else if (priority == EV_JOB_PRIORITY_HIGH)
		g_queue_push_tail (render_queue_high, job);
	else g_assert_not_reached ();
	
	g_cond_broadcast (render_cond);
	g_mutex_unlock (EV_DOC_MUTEX);
}

static gboolean
remove_object_from_queue (GQueue *queue, GObject *object)
{
	GList *list;
	list = g_queue_find (queue, object);
	if (list) {
		g_object_unref (object);
		g_queue_delete_link (queue, list);

		return TRUE;
	}
	return FALSE;
}

gboolean
ev_job_queue_remove_render_job (EvJobRender *job)
{
	gboolean retval = FALSE;

	g_mutex_lock (EV_DOC_MUTEX);

	retval = remove_object_from_queue (render_queue_high, G_OBJECT (job));
	retval = retval || remove_object_from_queue (render_queue_low, G_OBJECT (job));

	g_mutex_unlock (EV_DOC_MUTEX);

	return retval;
}

void
ev_job_queue_add_thumbnail_job    (EvJobThumbnail *job,
				   EvJobPriority   priority)
{
	g_return_if_fail (job != NULL);

	g_mutex_lock (EV_DOC_MUTEX);

	g_object_ref (job);
	if (priority == EV_JOB_PRIORITY_LOW)
		g_queue_push_tail (thumbnail_queue_low, job);
	else if (priority == EV_JOB_PRIORITY_HIGH)
		g_queue_push_tail (thumbnail_queue_high, job);
	else g_assert_not_reached ();
	
	g_cond_broadcast (render_cond);
	g_mutex_unlock (EV_DOC_MUTEX);
}

gboolean
ev_job_queue_remove_thumbnail_job (EvJobThumbnail *thumbnail)
{
	gboolean retval = FALSE;

	g_mutex_lock (EV_DOC_MUTEX);

	retval = remove_object_from_queue (thumbnail_queue_high, G_OBJECT (thumbnail));
	retval = retval || remove_object_from_queue (thumbnail_queue_low, G_OBJECT (thumbnail));

	g_mutex_unlock (EV_DOC_MUTEX);

	return retval;
}
