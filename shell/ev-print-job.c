/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2004 Martin Kretzschmar
 *
 *  Author:
 *    Martin Kretzschmar <martink@gnome.org>
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

#include <glib.h>
#include <glib-object.h>

#include "ev-print-job.h"

#define EV_PRINT_JOB_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), EV_PRINT_JOB, EvPrintJobClass))
#define EV_IS_PRINT_JOB_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), EV_PRINT_JOB))
#define EV_PRINT_JOB_GET_CLASS(object)	(G_TYPE_INSTANCE_GET_CLASS((object), EV_PRINT_JOB, EvPrintJobClass))

struct _EvPrintJob {
	GObject parent_instance;
};

struct _EvPrintJobClass {
	GObjectClass parent_class;
};


G_DEFINE_TYPE (EvPrintJob, ev_print_job, G_TYPE_OBJECT);

static void
ev_print_job_class_init (EvPrintJobClass *ev_print_job_class)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS (ev_print_job_class);
}

static void
ev_print_job_init (EvPrintJob *ev_print_job)
{
}

void
ev_print_job_print (EvPrintJob *job, GtkWindow *parent)
{
	g_return_if_fail (EV_IS_PRINT_JOB (job));
	/* FIXME */
	g_printerr ("Printing...\n");
}
