/* ev-job-scheduler.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2008 Carlos Garcia Campos <carlosgc@gnome.org>
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

#include "ev-debug.h"
#include "ev-job-scheduler.h"

typedef struct _EvSchedulerJob {
	EvJob         *job;
	EvJobPriority  priority;
	GSList        *job_link;
} EvSchedulerJob;

G_LOCK_DEFINE_STATIC(job_list);
static GSList *job_list = NULL;

static EvJob *running_job = NULL;

static gpointer ev_job_thread_proxy               (gpointer        data);
static void     ev_scheduler_thread_job_cancelled (EvSchedulerJob *job,
						   GCancellable   *cancellable);

/* EvJobQueue */
static GQueue queue_urgent = G_QUEUE_INIT;
static GQueue queue_high = G_QUEUE_INIT;
static GQueue queue_low = G_QUEUE_INIT;
static GQueue queue_none = G_QUEUE_INIT;
static GCond job_queue_cond;
static GMutex job_queue_mutex;

static GQueue *job_queue[EV_JOB_N_PRIORITIES] = {
	&queue_urgent,
	&queue_high,
	&queue_low,
	&queue_none
};

static void
ev_job_queue_push (EvSchedulerJob *job,
		   EvJobPriority   priority)
{
	ev_debug_message (DEBUG_JOBS, "%s priority %d", EV_GET_TYPE_NAME (job->job), priority);

	g_mutex_lock (&job_queue_mutex);

	g_queue_push_tail (job_queue[priority], job);
	g_cond_broadcast (&job_queue_cond);

	g_mutex_unlock (&job_queue_mutex);
}

static EvSchedulerJob *
ev_job_queue_get_next_unlocked (void)
{
	gint i;
	EvSchedulerJob *job = NULL;

	for (i = EV_JOB_PRIORITY_URGENT; i < EV_JOB_N_PRIORITIES; i++) {
		job = (EvSchedulerJob *) g_queue_pop_head (job_queue[i]);
		if (job)
			break;
	}

	ev_debug_message (DEBUG_JOBS, "%s", job ? EV_GET_TYPE_NAME (job->job) : "No jobs in queue");

	return job;
}

static gpointer
ev_job_scheduler_init (gpointer data)
{
	g_thread_new ("EvJobScheduler", ev_job_thread_proxy, NULL);

	return NULL;
}

static void
ev_scheduler_job_list_add (EvSchedulerJob *job)
{
	ev_debug_message (DEBUG_JOBS, "%s", EV_GET_TYPE_NAME (job->job));

	G_LOCK (job_list);

	job_list = g_slist_prepend (job_list, job);
	job->job_link = job_list;

	G_UNLOCK (job_list);
}

static void
ev_scheduler_job_list_remove (EvSchedulerJob *job)
{
	ev_debug_message (DEBUG_JOBS, "%s", EV_GET_TYPE_NAME (job->job));

	G_LOCK (job_list);

	job_list = g_slist_delete_link (job_list, job->job_link);

	G_UNLOCK (job_list);
}

static void
ev_scheduler_job_free (EvSchedulerJob *job)
{
	if (!job)
		return;

	g_object_unref (job->job);
	g_free (job);
}

static void
ev_scheduler_job_destroy (EvSchedulerJob *job)
{
	ev_debug_message (DEBUG_JOBS, "%s", EV_GET_TYPE_NAME (job->job));

	if (job->job->run_mode == EV_JOB_RUN_MAIN_LOOP) {
		g_signal_handlers_disconnect_by_func (job->job,
						      G_CALLBACK (ev_scheduler_job_destroy),
						      job);
	} else {
		g_signal_handlers_disconnect_by_func (job->job->cancellable,
						      G_CALLBACK (ev_scheduler_thread_job_cancelled),
						      job);
	}

	ev_scheduler_job_list_remove (job);
	ev_scheduler_job_free (job);
}

static void
ev_scheduler_thread_job_cancelled (EvSchedulerJob *job,
				   GCancellable   *cancellable)
{
	GList   *list;

	ev_debug_message (DEBUG_JOBS, "%s", EV_GET_TYPE_NAME (job->job));

	g_mutex_lock (&job_queue_mutex);

	/* If the job is not still running,
	 * remove it from the job queue and job list.
	 * If the job is currently running, it will be
	 * destroyed as soon as it finishes.
	 */
	list = g_queue_find (job_queue[job->priority], job);
	if (list) {
		g_queue_delete_link (job_queue[job->priority], list);
		g_mutex_unlock (&job_queue_mutex);
		ev_scheduler_job_destroy (job);
	} else {
		g_mutex_unlock (&job_queue_mutex);
	}
}

static void
ev_job_thread (EvJob *job)
{
	gboolean result;

	ev_debug_message (DEBUG_JOBS, "%s", EV_GET_TYPE_NAME (job));

	do {
		if (g_cancellable_is_cancelled (job->cancellable))
			result = FALSE;
		else {
                        g_atomic_pointer_set (&running_job, job);
			result = ev_job_run (job);
                }
	} while (result);

        g_atomic_pointer_set (&running_job, NULL);
}

static gboolean
ev_job_idle (EvJob *job)
{
	ev_debug_message (DEBUG_JOBS, "%s", EV_GET_TYPE_NAME (job));

	if (g_cancellable_is_cancelled (job->cancellable))
		return G_SOURCE_REMOVE;

	return ev_job_run (job);
}

static gpointer
ev_job_thread_proxy (gpointer data)
{
	while (TRUE) {
		EvSchedulerJob *job;

		g_mutex_lock (&job_queue_mutex);
		job = ev_job_queue_get_next_unlocked ();
		if (!job) {
			g_cond_wait (&job_queue_cond, &job_queue_mutex);
			g_mutex_unlock (&job_queue_mutex);
			continue;
		}
		g_mutex_unlock (&job_queue_mutex);

		ev_job_thread (job->job);
		ev_scheduler_job_destroy (job);
	}

	return NULL;
}

void
ev_job_scheduler_push_job (EvJob         *job,
			   EvJobPriority  priority)
{
	static GOnce once_init = G_ONCE_INIT;
	EvSchedulerJob *s_job;

	g_once (&once_init, ev_job_scheduler_init, NULL);

	ev_debug_message (DEBUG_JOBS, "%s priority %d", EV_GET_TYPE_NAME (job), priority);

	s_job = g_new0 (EvSchedulerJob, 1);
	s_job->job = g_object_ref (job);
	s_job->priority = priority;

	ev_scheduler_job_list_add (s_job);

	switch (ev_job_get_run_mode (job)) {
	case EV_JOB_RUN_THREAD:
		g_signal_connect_swapped (job->cancellable, "cancelled",
					  G_CALLBACK (ev_scheduler_thread_job_cancelled),
					  s_job);
		ev_job_queue_push (s_job, priority);
		break;
	case EV_JOB_RUN_MAIN_LOOP:
		g_signal_connect_swapped (job, "finished",
					  G_CALLBACK (ev_scheduler_job_destroy),
					  s_job);
		g_signal_connect_swapped (job, "cancelled",
					  G_CALLBACK (ev_scheduler_job_destroy),
					  s_job);
		g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
				 (GSourceFunc)ev_job_idle,
				 g_object_ref (job),
				 (GDestroyNotify)g_object_unref);
		break;
	default:
		g_assert_not_reached ();
	}
}

void
ev_job_scheduler_update_job (EvJob         *job,
			     EvJobPriority  priority)
{
	GSList         *l;
	EvSchedulerJob *s_job = NULL;
	gboolean        need_resort = FALSE;

	/* Main loop jobs are scheduled immediately */
	if (ev_job_get_run_mode (job) == EV_JOB_RUN_MAIN_LOOP)
		return;

	ev_debug_message (DEBUG_JOBS, "%s priority %d", EV_GET_TYPE_NAME (job), priority);

	G_LOCK (job_list);

	for (l = job_list; l; l = l->next) {
		s_job = (EvSchedulerJob *)l->data;

		if (s_job->job == job) {
			need_resort = (s_job->priority != priority);
			break;
		}
	}

	G_UNLOCK (job_list);

	if (need_resort) {
		GList *list;

		g_mutex_lock (&job_queue_mutex);

		list = g_queue_find (job_queue[s_job->priority], s_job);
		if (list) {
			ev_debug_message (DEBUG_JOBS, "Moving job %s from priority %d to %d",
					  EV_GET_TYPE_NAME (job), s_job->priority, priority);
			g_queue_delete_link (job_queue[s_job->priority], list);
			g_queue_push_tail (job_queue[priority], s_job);
			g_cond_broadcast (&job_queue_cond);
		}

		g_mutex_unlock (&job_queue_mutex);
	}
}

/**
 * ev_job_scheduler_get_running_thread_job:
 *
 * Returns: (transfer none): an #EvJob
 */
EvJob *
ev_job_scheduler_get_running_thread_job (void)
{
        return g_atomic_pointer_get (&running_job);
}

/**
 * ev_job_scheduler_wait:
 *
 * Synchronously waits until all jobs are done.
 * Remember that main loop is not running already probably.
 */
void
ev_job_scheduler_wait (void)
{
	ev_debug_message (DEBUG_JOBS, "Waiting for empty job list");

	while (job_list != NULL)
		g_usleep (100);

	ev_debug_message (DEBUG_JOBS, "Job list is empty");
}
