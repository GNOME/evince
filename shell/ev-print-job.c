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

#include <config.h>

#include <unistd.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>

/* for gnome_print_job_set_file */
#define GNOME_PRINT_UNSTABLE_API
#include <libgnomeprint/gnome-print-job.h>

#include "ev-ps-exporter.h"
#include "ev-print-job.h"

#define EV_PRINT_JOB_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), EV_PRINT_JOB, EvPrintJobClass))
#define EV_IS_PRINT_JOB_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), EV_PRINT_JOB))
#define EV_PRINT_JOB_GET_CLASS(object)	(G_TYPE_INSTANCE_GET_CLASS((object), EV_PRINT_JOB, EvPrintJobClass))

struct _EvPrintJob {
	GObject parent_instance;

	EvDocument *document;
	GnomePrintJob *gnome_print_job;
	double width; /* FIXME unused */
	double height; /* FIXME unused */
	gboolean duplex; /* FIXME unused */
	int copies; /* FIXME unused */
	int collate; /* FIXME unsued */

	int fd;
	char *temp_file;
	guint idle_id;
	gboolean printing;
	int next_page;
};

struct _EvPrintJobClass {
	GObjectClass parent_class;
};

enum {
	PROP_0,
	PROP_GNOME_PRINT_JOB,
	PROP_DOCUMENT,
	PROP_PRINT_DIALOG
};

G_DEFINE_TYPE (EvPrintJob, ev_print_job, G_TYPE_OBJECT);

static void
ev_print_job_finalize (GObject *object)
{
	EvPrintJob *job = EV_PRINT_JOB (object);

	if (job && job->document) {
		g_object_unref (job->document);
		job->document = NULL;
	}

	if (job && job->gnome_print_job) {
		g_object_unref (job->gnome_print_job);
		job->gnome_print_job = NULL;
	}

	G_OBJECT_CLASS (ev_print_job_parent_class)->finalize (object);
}

static void
ev_print_job_set_property (GObject *object, guint prop_id,
			   const GValue *value, GParamSpec *pspec)
{
	EvPrintJob *job;

	job = EV_PRINT_JOB (object);

	switch (prop_id) {
	case PROP_GNOME_PRINT_JOB:
		ev_print_job_set_gnome_print_job (
			job, GNOME_PRINT_JOB (g_value_get_object (value)));
		break;
	case PROP_DOCUMENT:
		ev_print_job_set_document (job, EV_DOCUMENT (g_value_get_object (value)));
		break;
	case PROP_PRINT_DIALOG:
		ev_print_job_use_print_dialog_settings (
			job, GNOME_PRINT_DIALOG (g_value_get_object (value)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}

}

static void
ev_print_job_get_property (GObject *object, guint prop_id,
			   GValue *value, GParamSpec *pspec)
{
	EvPrintJob *job;

	job = EV_PRINT_JOB (object);

	switch (prop_id) {
	case PROP_GNOME_PRINT_JOB:
		g_value_set_object (value, job->gnome_print_job);
		break;
	case PROP_DOCUMENT:
		g_value_set_object (value, job->document);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}


static void
ev_print_job_class_init (EvPrintJobClass *ev_print_job_class)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS (ev_print_job_class);

	g_object_class->finalize = ev_print_job_finalize;
	g_object_class->set_property = ev_print_job_set_property;
	g_object_class->get_property = ev_print_job_get_property;

	g_object_class_install_property (g_object_class,
					 PROP_GNOME_PRINT_JOB,
					 g_param_spec_object ("gnome_print_job",
							      "GnomePrintJob",
							      "GnomePrintJob",
							      GNOME_TYPE_PRINT_JOB,
							      G_PARAM_READWRITE));
	g_object_class_install_property (g_object_class,
					 PROP_DOCUMENT,
					 g_param_spec_object ("document",
							      "Document object",
							      "Document from which to print",
							      G_TYPE_OBJECT, /* EV_TYPE_DOCUMENT, */
							      G_PARAM_READWRITE));
	g_object_class_install_property (g_object_class,
					 PROP_PRINT_DIALOG,
					 g_param_spec_object ("print_dialog",
							      "GnomePrintDialog",
							      "GnomePrintDialog with user settings",
							      GNOME_TYPE_PRINT_DIALOG,
							      G_PARAM_WRITABLE));

}

static void
ev_print_job_init (EvPrintJob *ev_print_job)
{
}

void
ev_print_job_set_gnome_print_job (EvPrintJob *job, GnomePrintJob *gpj)
{
	g_return_if_fail (EV_IS_PRINT_JOB (job));

	if (job->gnome_print_job == gpj)
		return;

	if (job->gnome_print_job)
		g_object_unref (job->gnome_print_job);
	
	if (gpj)
		g_object_ref (gpj);

	job->gnome_print_job = gpj;
}

void
ev_print_job_set_document (EvPrintJob *job, EvDocument *document)
{
	g_return_if_fail (EV_IS_PRINT_JOB (job));

	if (job->document == document)
		return;

	if (job->document)
		g_object_ref (job->document);

	if (document)
		g_object_ref (document);
	
	job->document = document;
}

void
ev_print_job_use_print_dialog_settings (EvPrintJob *job, GnomePrintDialog *dialog)
{
	GnomePrintConfig *print_config;

	g_return_if_fail (EV_IS_PRINT_JOB (job));
	g_return_if_fail (GNOME_IS_PRINT_DIALOG (dialog));

	print_config = gnome_print_dialog_get_config (dialog);
	gnome_print_dialog_get_copies (dialog, &job->copies, &job->collate);
	gnome_print_config_get_page_size (print_config,
					  &job->width, &job->height);
	gnome_print_config_get_boolean (print_config,
					(guchar *)GNOME_PRINT_KEY_DUPLEX, &job->duplex);
	gnome_print_config_unref (print_config);
}

static gboolean
idle_print_handler (EvPrintJob *job)
{
	EvPageCache *page_cache;

	if (!job->printing) {
		g_mutex_lock (EV_DOC_MUTEX);
		ev_ps_exporter_begin (EV_PS_EXPORTER (job->document),
				      job->temp_file);
		g_mutex_unlock (EV_DOC_MUTEX);
		job->next_page = 1; /* FIXME use 0-based page numbering? */
		job->printing = TRUE;
		return TRUE;
	}

	page_cache = ev_document_get_page_cache (job->document);
	if (job->next_page <= ev_page_cache_get_n_pages (page_cache)) {
#if 0
		g_printerr ("Printing page %d\n", job->next_page);
#endif
		g_mutex_lock (EV_DOC_MUTEX);
		ev_ps_exporter_do_page (EV_PS_EXPORTER (job->document),
					job->next_page);
		g_mutex_unlock (EV_DOC_MUTEX);
		job->next_page++;
		return TRUE;
	} else { /* no more pages */
		g_mutex_lock (EV_DOC_MUTEX);
		ev_ps_exporter_end (EV_PS_EXPORTER (job->document));
		g_mutex_unlock (EV_DOC_MUTEX);

		close (job->fd);
		job->fd = 0;

		gnome_print_job_print (job->gnome_print_job);

		unlink (job->temp_file);
		g_free (job->temp_file);

		g_object_unref (job->gnome_print_job);
		job->gnome_print_job = NULL;

		job->printing = FALSE;
		job->idle_id = 0;
		return FALSE;
	}
}

static void
print_closure_finalize (EvPrintJob *job, GClosure *closure)
{
	g_object_unref (job);
}

void
ev_print_job_print (EvPrintJob *job, GtkWindow *parent)
{
	GClosure *closure;
	GSource *idle_source;

	g_return_if_fail (EV_IS_PRINT_JOB (job));
	g_return_if_fail (job->document != NULL);
	g_return_if_fail (EV_IS_PS_EXPORTER (job->document));
#if 0
	g_printerr ("Printing...\n");
#endif

	job->fd = g_file_open_tmp ("evince_print.ps.XXXXXX", &job->temp_file, NULL);
	if (job->fd <= -1)
		return; /* FIXME use GError */

	gnome_print_job_set_file (job->gnome_print_job, job->temp_file);

	g_object_ref (job);
	closure = g_cclosure_new (G_CALLBACK (idle_print_handler), job, NULL);
	g_closure_add_finalize_notifier (
		closure, job, (GClosureNotify)print_closure_finalize);

	idle_source = g_idle_source_new ();
	g_source_set_closure (idle_source, closure);
	job->idle_id = g_source_attach (idle_source, NULL);
	g_source_unref (idle_source);
}
