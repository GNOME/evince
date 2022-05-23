/* ev-job-scheduler.h
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

#pragma once

#if !defined (__EV_EVINCE_VIEW_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-view.h> can be included directly."
#endif

#include <glib.h>

#include <evince-document.h>
#include "ev-jobs.h"

G_BEGIN_DECLS

typedef enum {
	EV_JOB_PRIORITY_URGENT, /* Rendering current page range */
	EV_JOB_PRIORITY_HIGH,   /* Rendering current thumbnail range */
	EV_JOB_PRIORITY_LOW,    /* Rendering pages not in current range */
	EV_JOB_PRIORITY_NONE,   /* Any other job: load, save, print, ... */
	EV_JOB_N_PRIORITIES
} EvJobPriority;

EV_PUBLIC
void   ev_job_scheduler_push_job               (EvJob        *job,
                                                EvJobPriority priority);
EV_PUBLIC
void   ev_job_scheduler_update_job             (EvJob        *job,
                                                EvJobPriority priority);
EV_PUBLIC
EvJob *ev_job_scheduler_get_running_thread_job (void);

EV_PUBLIC
void   ev_job_scheduler_wait                   (void);

G_END_DECLS
