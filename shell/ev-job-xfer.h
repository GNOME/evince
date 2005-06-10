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

#ifndef __EV_JOB_XFER_H__
#define __EV_JOB_XFER_H__

#include <gtk/gtk.h>
#include "ev-document.h"
#include "ev-jobs.h"

G_BEGIN_DECLS

typedef struct _EvJobXfer EvJobXfer;
typedef struct _EvJobXferClass EvJobXferClass;

#define EV_TYPE_JOB_XFER		     (ev_job_xfer_get_type())
#define EV_JOB_XFER(object)	     	     (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_JOB_XFER, EvJobXfer))
#define EV_JOB_XFER_CLASS(klass)	     (G_TYPE_CHACK_CLASS_CAST((klass), EV_TYPE_JOB_XFER, EvJobXferClass))
#define EV_IS_JOB_XFER(object)		     (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_JOB_XFER))

struct _EvJobXfer
{
	EvJob parent;
	GError *error;
	char *uri;
	char *local_uri;
};

struct _EvJobXferClass
{
	EvJobClass parent_class;
};

/* EvJobXfer */
GType 		ev_job_xfer_get_type 	  (void);
EvJob 	       *ev_job_xfer_new 	  (const gchar 	   *uri);
void		ev_job_xfer_run 	  (EvJobXfer 	   *xfer);					   

G_END_DECLS

#endif /* __EV_JOB_XFER_H__ */
