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

#ifndef __EV_JOBS_H__
#define __EV_JOBS_H__

#include <gtk/gtk.h>
#include "ev-document.h"

G_BEGIN_DECLS

typedef struct _EvJobRender EvJobRender;
typedef struct _EvJobRenderClass EvJobRenderClass;

typedef struct _EvJobThumbnail EvJobThumbnail;
typedef struct _EvJobThumbnailClass EvJobThumbnailClass;

typedef struct _EvJobLinks EvJobLinks;
typedef struct _EvJobLinksClass EvJobLinksClass;

#define EV_TYPE_JOB_LINKS		     (ev_job_links_get_type())
#define EV_JOB_LINKS(object)		     (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_JOB_LINKS, EvJobLinks))
#define EV_JOB_LINKS_CLASS(klass)	     (G_TYPE_CHACK_CLASS_CAST((klass), EV_TYPE_JOB_LINKS, EvJobLinksClass))
#define EV_IS_JOB_LINKS(object)		     (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_JOB_LINKS))

#define EV_TYPE_JOB_RENDER		     (ev_job_render_get_type())
#define EV_JOB_RENDER(object)		     (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_JOB_RENDER, EvJobRender))
#define EV_JOB_RENDER_CLASS(klass)	     (G_TYPE_CHACK_CLASS_CAST((klass), EV_TYPE_JOB_RENDER, EvJobRenderClass))
#define EV_IS_JOB_RENDER(object)	     (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_JOB_RENDER))

#define EV_TYPE_JOB_THUMBNAIL		     (ev_job_thumbnail_get_type())
#define EV_JOB_THUMBNAIL(object)	     (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_JOB_THUMBNAIL, EvJobThumbnail))
#define EV_JOB_THUMBNAIL_CLASS(klass)	     (G_TYPE_CHACK_CLASS_CAST((klass), EV_TYPE_JOB_THUMBNAIL, EvJobThumbnailClass))
#define EV_IS_JOB_THUMBNAIL(object)	     (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_JOB_THUMBNAIL))

typedef enum {
	EV_JOB_PRIORITY_LOW,
	EV_JOB_PRIORITY_HIGH,
} EvJobPriority;

struct _EvJobLinks
{
	GObject parent;
	EvDocument *document;
	GtkTreeModel *model;
};

struct _EvJobLinksClass
{
	GObjectClass parent_class;

	void    (* finished) (EvJobLinks *job);
};

struct _EvJobRender
{
	GObject parent;
	EvDocument *document;
	gint page;
	double scale;
	gint target_width;
	gint target_height;
	GdkPixbuf *pixbuf;
	gboolean pixbuf_done;
};

struct _EvJobRenderClass
{
	GObjectClass parent_class;

	void    (* finished) (EvJobRender *job);
};

struct _EvJobThumbnail
{
	GObject parent;
	EvDocument *document;
	gint page;
	gint requested_width;
	GdkPixbuf *thumbnail;
	gboolean thumbnail_done;
};

struct _EvJobThumbnailClass
{
	GObjectClass parent_class;

	void    (* finished) (EvJobThumbnail *job);
};


/* EvJobLinks */
GType           ev_job_links_get_type     (void);
EvJobLinks     *ev_job_links_new          (EvDocument     *document);
void            ev_job_links_run          (EvJobLinks     *thumbnail);
void            ev_job_links_finished     (EvJobLinks     *job);

/* EvJobRender */
GType           ev_job_render_get_type    (void);
EvJobRender    *ev_job_render_new         (EvDocument     *document,
					   gint            page,
					   double          scale,
					   gint            width,
					   gint            height);
void            ev_job_render_run         (EvJobRender    *thumbnail);
void            ev_job_render_finished    (EvJobRender    *job);

/* EvJobThumbnail */
GType           ev_job_thumbnail_get_type (void);
EvJobThumbnail *ev_job_thumbnail_new      (EvDocument     *document,
					   gint            page,
					   gint            requested_width);
void            ev_job_thumbnail_run      (EvJobThumbnail *thumbnail);
void            ev_job_thumbnail_finished (EvJobThumbnail *job);


G_END_DECLS

#endif /* __EV_JOBS_H__ */
