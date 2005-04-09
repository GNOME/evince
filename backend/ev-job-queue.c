#include "ev-job-queue.h"

/* Like glib calling convention, all functions with _locked in their name assume
 * that we've already locked the doc mutex and can freely and safely access
 * data.
 */
GCond *render_cond = NULL;
GMutex *ev_queue_mutex = NULL;

static GQueue *links_queue = NULL;
static GQueue *render_queue_high = NULL;
static GQueue *render_queue_low = NULL;
static GQueue *thumbnail_queue_high = NULL;
static GQueue *thumbnail_queue_low = NULL;

static gboolean
remove_job_from_queue_locked (GQueue *queue, EvJob *job)
{
	GList *list;

	list = g_queue_find (queue, job);
	if (list) {
		g_object_unref (G_OBJECT (job));
		g_queue_delete_link (queue, list);

		return TRUE;
	}
	return FALSE;
}

static void
add_job_to_queue_locked (GQueue *queue,
			 EvJob  *job)
{
	g_object_ref (job);
	g_queue_push_tail (queue, job);
	g_cond_broadcast (render_cond);
}


static gboolean
notify_finished (GObject *job)
{
	ev_job_finished (EV_JOB (job));

	return FALSE;
}


static void
handle_job (EvJob *job)
{
	g_object_ref (G_OBJECT (job));

	if (EV_IS_JOB_THUMBNAIL (job))
		ev_job_thumbnail_run (EV_JOB_THUMBNAIL (job));
	else if (EV_IS_JOB_LINKS (job))
		ev_job_links_run (EV_JOB_LINKS (job));
	else if (EV_IS_JOB_RENDER (job))
		ev_job_render_run (EV_JOB_RENDER (job));

	/* We let the idle own a ref, as we (the queue) are done with the job. */
	g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
			 (GSourceFunc) notify_finished,
			 job,
			 g_object_unref);
}

static EvJob *
search_for_jobs_unlocked (void)
{
	EvJob *job;

	job = (EvJob *) g_queue_pop_head (render_queue_high);
	if (job)
		return job;

	job = (EvJob *) g_queue_pop_head (thumbnail_queue_high);
	if (job)
		return job;

	job = (EvJob *) g_queue_pop_head (render_queue_low);
	if (job)
		return job;

	job = (EvJob *) g_queue_pop_head (links_queue);
	if (job)
		return job;

	job = (EvJob *) g_queue_pop_head (thumbnail_queue_low);
	if (job)
		return job;

	return NULL;
}

static gboolean
no_jobs_available_unlocked (void)
{
	return g_queue_is_empty (render_queue_high)
		&& g_queue_is_empty (render_queue_low)
		&& g_queue_is_empty (links_queue)
		&& g_queue_is_empty (thumbnail_queue_high)
		&& g_queue_is_empty (thumbnail_queue_low);
}

/* the thread mainloop function */
static gpointer
ev_render_thread (gpointer data)
{
	while (TRUE) {
		EvJob *job;

		g_mutex_lock (ev_queue_mutex);
		if (no_jobs_available_unlocked ()) {
			g_cond_wait (render_cond, ev_queue_mutex);
		}

		job = search_for_jobs_unlocked ();
		g_mutex_unlock (ev_queue_mutex);

		/* Now that we have our job, we handle it */
		if (job) {
			handle_job (job);
			g_object_unref (G_OBJECT (job));
		}
	}
	return NULL;

}

/* Public Functions */
void
ev_job_queue_init (void)
{
	if (!g_thread_supported ()) g_thread_init (NULL);

	render_cond = g_cond_new ();
	ev_queue_mutex = g_mutex_new ();

	links_queue = g_queue_new ();
	render_queue_high = g_queue_new ();
	render_queue_low = g_queue_new ();
	thumbnail_queue_high = g_queue_new ();
	thumbnail_queue_low = g_queue_new ();

	g_thread_create (ev_render_thread, NULL, FALSE, NULL);

}

static GQueue *
find_queue (EvJob         *job,
	    EvJobPriority  priority)
{
	if (EV_IS_JOB_RENDER (job)) {
		if (priority == EV_JOB_PRIORITY_HIGH)
			return render_queue_high;
		else
			return render_queue_low;
	} else if (EV_IS_JOB_THUMBNAIL (job)) {
		if (priority == EV_JOB_PRIORITY_HIGH)
			return thumbnail_queue_high;
		else
			return thumbnail_queue_low;
	} else if (EV_IS_JOB_LINKS (job)) {
		/* the priority doesn't effect links */
		return links_queue;
	}

	g_assert_not_reached ();
	return NULL;
}

void
ev_job_queue_add_job (EvJob         *job,
		      EvJobPriority  priority)
{
	GQueue *queue;

	g_return_if_fail (EV_IS_JOB (job));

	queue = find_queue (job, priority);

	g_mutex_lock (ev_queue_mutex);
	add_job_to_queue_locked (queue, job);
	g_mutex_unlock (ev_queue_mutex);
}

gboolean
ev_job_queue_update_job (EvJob         *job,
			 EvJobPriority  new_priority)
{
	gboolean retval = FALSE;
	
	g_return_val_if_fail (EV_IS_JOB (job), FALSE);

	g_mutex_lock (ev_queue_mutex);
	g_object_ref (job);
	
	if (EV_IS_JOB_THUMBNAIL (job)) {
		if (new_priority == EV_JOB_PRIORITY_LOW) {
			if (remove_job_from_queue_locked (thumbnail_queue_high, job)) {
				add_job_to_queue_locked (thumbnail_queue_low, job);
				retval = TRUE;
			}
		} else if (new_priority == EV_JOB_PRIORITY_HIGH) {
			if (remove_job_from_queue_locked (thumbnail_queue_low, job)) {
				add_job_to_queue_locked (thumbnail_queue_high, job);
				retval = TRUE;
			}
		}
	} else if (EV_IS_JOB_RENDER (job)) {
		if (new_priority == EV_JOB_PRIORITY_LOW) {
			if (remove_job_from_queue_locked (render_queue_high, job)) {
				add_job_to_queue_locked (render_queue_low, job);
				retval = TRUE;
			}
		} else if (new_priority == EV_JOB_PRIORITY_HIGH) {
			if (remove_job_from_queue_locked (render_queue_low, job)) {
				add_job_to_queue_locked (render_queue_high, job);
				retval = TRUE;
			}
		}
	} else {
		/* We don't have a priority queue for any of the other jobs */
	}
	g_object_unref (job);
	g_mutex_unlock (ev_queue_mutex);

	return retval;
}

gboolean
ev_job_queue_remove_job (EvJob *job)
{
	gboolean retval = FALSE;

	g_return_val_if_fail (EV_IS_JOB (job), FALSE);

	g_mutex_lock (ev_queue_mutex);

	if (EV_IS_JOB_THUMBNAIL (job)) {
		retval = remove_job_from_queue_locked (thumbnail_queue_high, job);
		retval = retval || remove_job_from_queue_locked (thumbnail_queue_low, job);
	} else if (EV_IS_JOB_RENDER (job)) {
		retval = remove_job_from_queue_locked (render_queue_high, job);
		retval = retval || remove_job_from_queue_locked (render_queue_low, job);
	} else if (EV_IS_JOB_LINKS (job)) {
		retval = remove_job_from_queue_locked (links_queue, job);
	} else {
		g_assert_not_reached ();
	}
	
	g_mutex_unlock (ev_queue_mutex);

	return retval;
}


