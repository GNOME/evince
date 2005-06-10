/* this file is part of evince, a gnome document viewer
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include "ev-job-xfer.h"
#include "ev-document-types.h"
#include "ev-file-helpers.h"

#include <glib/gi18n.h>
#include <glib.h>

#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-xfer.h>

static void ev_job_xfer_init    	(EvJobXfer	     *job);
static void ev_job_xfer_class_init 	(EvJobXferClass	     *class);

G_DEFINE_TYPE (EvJobXfer, ev_job_xfer, EV_TYPE_JOB)

static void ev_job_xfer_init (EvJobXfer *job) { /* Do Nothing */ }

static void
ev_job_xfer_dispose (GObject *object)
{
	EvJobXfer *job = EV_JOB_XFER (object);

	if (job->uri) {
		g_free (job->uri);
		job->uri = NULL;
	}

	if (job->local_uri) {
		g_free (job->local_uri);
		job->local_uri = NULL;
	}

	if (job->error) {
		g_error_free (job->error);
		job->error = NULL;
	}

	(* G_OBJECT_CLASS (ev_job_xfer_parent_class)->dispose) (object);
}

static void
ev_job_xfer_class_init (EvJobXferClass *class)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (class);

	oclass->dispose = ev_job_xfer_dispose;
}


EvJob *
ev_job_xfer_new (const gchar *uri)
{
	EvJobXfer *job;

	job = g_object_new (EV_TYPE_JOB_XFER, NULL);

	job->uri = g_strdup (uri);

	return EV_JOB (job);
}

void
ev_job_xfer_run (EvJobXfer *job)
{
	GType document_type;
	GError *error = NULL;
	GnomeVFSURI *source_uri;
	GnomeVFSURI *target_uri;

	g_return_if_fail (EV_IS_JOB_XFER (job));
	
	if (job->error) {
	        g_error_free (job->error);
		job->error = NULL;
	}

	document_type = ev_document_type_lookup (job->uri, NULL, &error);

	if (document_type != G_TYPE_INVALID) {
		EV_JOB (job)->document = g_object_new (document_type, NULL);
	} else {
		job->error = error;			
		EV_JOB (job)->finished = TRUE;
		return;	
	}
	
	source_uri = gnome_vfs_uri_new (job->uri);
	if (!gnome_vfs_uri_is_local (source_uri)) {
		char *tmp_name;
		
		tmp_name = ev_tmp_filename ();
		job->local_uri = g_strconcat ("file:", tmp_name, NULL);
		g_free (tmp_name);
		
		target_uri = gnome_vfs_uri_new (job->local_uri);

		gnome_vfs_xfer_uri (source_uri, target_uri, 
				    GNOME_VFS_XFER_DEFAULT | GNOME_VFS_XFER_FOLLOW_LINKS,
				    GNOME_VFS_XFER_ERROR_MODE_ABORT,
				    GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE,
				    NULL,
				    job);
		gnome_vfs_uri_unref (target_uri);
	}
	gnome_vfs_uri_unref (source_uri);

	EV_JOB (job)->finished = TRUE;
	return;
}


