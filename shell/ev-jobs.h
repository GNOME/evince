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
#include "ev-window.h"
#include "ev-selection.h"

G_BEGIN_DECLS

typedef struct _EvJob EvJob;
typedef struct _EvJobClass EvJobClass;

typedef struct _EvJobRender EvJobRender;
typedef struct _EvJobRenderClass EvJobRenderClass;

typedef struct _EvJobThumbnail EvJobThumbnail;
typedef struct _EvJobThumbnailClass EvJobThumbnailClass;

typedef struct _EvJobLinks EvJobLinks;
typedef struct _EvJobLinksClass EvJobLinksClass;

typedef struct _EvJobFonts EvJobFonts;
typedef struct _EvJobFontsClass EvJobFontsClass;

typedef struct _EvJobLoad EvJobLoad;
typedef struct _EvJobLoadClass EvJobLoadClass;

typedef struct _EvJobSave EvJobSave;
typedef struct _EvJobSaveClass EvJobSaveClass;

typedef struct _EvJobPrint EvJobPrint;
typedef struct _EvJobPrintClass EvJobPrintClass;

#define EV_TYPE_JOB		     	     (ev_job_get_type())
#define EV_JOB(object)		             (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_JOB, EvJob))
#define EV_JOB_CLASS(klass)	             (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_JOB, EvJobClass))
#define EV_IS_JOB(object)		     (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_JOB))

#define EV_TYPE_JOB_LINKS		     (ev_job_links_get_type())
#define EV_JOB_LINKS(object)		     (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_JOB_LINKS, EvJobLinks))
#define EV_JOB_LINKS_CLASS(klass)	     (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_JOB_LINKS, EvJobLinksClass))
#define EV_IS_JOB_LINKS(object)		     (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_JOB_LINKS))

#define EV_TYPE_JOB_RENDER		     (ev_job_render_get_type())
#define EV_JOB_RENDER(object)		     (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_JOB_RENDER, EvJobRender))
#define EV_JOB_RENDER_CLASS(klass)	     (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_JOB_RENDER, EvJobRenderClass))
#define EV_IS_JOB_RENDER(object)	     (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_JOB_RENDER))

#define EV_TYPE_JOB_THUMBNAIL		     (ev_job_thumbnail_get_type())
#define EV_JOB_THUMBNAIL(object)	     (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_JOB_THUMBNAIL, EvJobThumbnail))
#define EV_JOB_THUMBNAIL_CLASS(klass)	     (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_JOB_THUMBNAIL, EvJobThumbnailClass))
#define EV_IS_JOB_THUMBNAIL(object)	     (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_JOB_THUMBNAIL))

#define EV_TYPE_JOB_FONTS		     (ev_job_fonts_get_type())
#define EV_JOB_FONTS(object)	     	     (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_JOB_FONTS, EvJobFonts))
#define EV_JOB_FONTS_CLASS(klass)	     (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_JOB_FONTS, EvJobFontsClass))
#define EV_IS_JOB_FONTS(object)		     (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_JOB_FONTS))

#define EV_TYPE_JOB_LOAD		     (ev_job_load_get_type())
#define EV_JOB_LOAD(object)	     	     (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_JOB_LOAD, EvJobLoad))
#define EV_JOB_LOAD_CLASS(klass)	     (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_JOB_LOAD, EvJobLoadClass))
#define EV_IS_JOB_LOAD(object)		     (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_JOB_LOAD))

#define EV_TYPE_JOB_SAVE		     (ev_job_save_get_type())
#define EV_JOB_SAVE(object)	     	     (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_JOB_SAVE, EvJobSave))
#define EV_JOB_SAVE_CLASS(klass)	     (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_JOB_SAVE, EvJobSaveClass))
#define EV_IS_JOB_SAVE(object)		     (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_JOB_SAVE))

#define EV_TYPE_JOB_PRINT                    (ev_job_print_get_type())
#define EV_JOB_PRINT(object)                 (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_JOB_PRINT, EvJobPrint))
#define EV_JOB_PRINT_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_JOB_PRINT, EvJobPrintClass))
#define EV_IS_JOB_PRINT(object)              (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_JOB_PRINT))

typedef enum {
	EV_JOB_PRIORITY_LOW,
	EV_JOB_PRIORITY_HIGH,
} EvJobPriority;

struct _EvJob
{
	GObject parent;
	EvDocument *document;
	gboolean finished;
	gboolean async;
};

struct _EvJobClass
{
	GObjectClass parent_class;

	void    (* finished) (EvJob *job);
};

struct _EvJobLinks
{
	EvJob parent;

	GtkTreeModel *model;
};

struct _EvJobLinksClass
{
	EvJobClass parent_class;
};

struct _EvJobRender
{
	EvJob parent;

	EvRenderContext *rc;
	gboolean page_ready;
	gint target_width;
	gint target_height;
	cairo_surface_t *surface;

	GList *link_mapping;
	GdkRegion *text_mapping;
	GList *image_mapping;
	GList *form_field_mapping;

	cairo_surface_t *selection;
	GdkRegion *selection_region;
	EvRectangle selection_points;
	EvSelectionStyle selection_style;
	GdkColor base;
	GdkColor text; 

	gint include_forms : 1;
	gint include_links : 1;
	gint include_text : 1;
	gint include_selection : 1;
	gint include_images : 1;
};

struct _EvJobRenderClass
{
	EvJobClass parent_class;

	void    (* page_ready) (EvJobRender *job);
};

struct _EvJobThumbnail
{
	EvJob parent;

	gint page;
	gint rotation;
	gdouble scale;
	
	GdkPixbuf *thumbnail;
};

struct _EvJobThumbnailClass
{
	EvJobClass parent_class;
};

struct _EvJobFonts
{
	EvJob parent;
	gboolean scan_completed;
};

struct _EvJobFontsClass
{
        EvJobClass parent_class;
};

struct _EvJobLoad
{
	EvJob parent;
	
	EvLinkDest *dest;
	EvWindowRunMode mode;
	gchar *search_string;
	GError *error;
	gchar *uri;
};

struct _EvJobLoadClass
{
	EvJobClass parent_class;
};

struct _EvJobSave
{
	EvJob parent;

	GError *error;
	gchar *uri;
	gchar *document_uri;
};

struct _EvJobSaveClass
{
	EvJobClass parent_class;
};

struct _EvJobPrint
{
	EvJob parent;

	GError *error;
	const gchar *format;
	gchar  *temp_file;
	EvPrintRange *ranges;
	gint n_ranges;
	EvPrintPageSet page_set;
	gint copies;
	gint pages_per_sheet;
	gboolean collate;
	gboolean reverse;
	gdouble width;
	gdouble height;
};

struct _EvJobPrintClass
{
	EvJobClass parent_class;
};

/* Base job class */
GType           ev_job_get_type           (void) G_GNUC_CONST;
void            ev_job_finished           (EvJob          *job);

/* EvJobLinks */
GType           ev_job_links_get_type     (void) G_GNUC_CONST;
EvJob          *ev_job_links_new          (EvDocument     *document);
void            ev_job_links_run          (EvJobLinks     *thumbnail);

/* EvJobRender */
GType           ev_job_render_get_type    (void) G_GNUC_CONST;
EvJob          *ev_job_render_new         (EvDocument      *document,
					   EvRenderContext *rc,
					   gint             width,
					   gint             height,
					   EvRectangle     *selection_points,
					   EvSelectionStyle selection_style,
					   GdkColor        *text,
					   GdkColor        *base,
					   gboolean         include_forms,
					   gboolean         include_links,
					   gboolean         include_images,
					   gboolean         include_text,
					   gboolean         include_selection);
void            ev_job_render_run         (EvJobRender     *thumbnail);

/* EvJobThumbnail */
GType           ev_job_thumbnail_get_type (void) G_GNUC_CONST;
EvJob          *ev_job_thumbnail_new      (EvDocument      *document,
					   gint             page,
					   gint             rotation,
					   gdouble          scale);
void            ev_job_thumbnail_run      (EvJobThumbnail  *thumbnail);

/* EvJobFonts */
GType 		ev_job_fonts_get_type 	  (void) G_GNUC_CONST;
EvJob 	       *ev_job_fonts_new 	  (EvDocument      *document);
void		ev_job_fonts_run 	  (EvJobFonts 	   *fonts);

/* EvJobLoad */
GType 		ev_job_load_get_type 	  (void) G_GNUC_CONST;
EvJob 	       *ev_job_load_new 	  (const gchar 	   *uri,
					   EvLinkDest      *dest,
					   EvWindowRunMode  mode,
					   const gchar     *search_string);
void            ev_job_load_set_uri       (EvJobLoad       *load,
					   const gchar     *uri);
void		ev_job_load_run 	  (EvJobLoad 	   *load);

/* EvJobSave */
GType           ev_job_save_get_type      (void) G_GNUC_CONST;
EvJob          *ev_job_save_new           (EvDocument      *document,
					   const gchar     *uri,
					   const gchar     *document_uri);
void            ev_job_save_run           (EvJobSave       *save);

/* EvJobPrint */
GType           ev_job_print_get_type     (void) G_GNUC_CONST;
EvJob          *ev_job_print_new          (EvDocument      *document,
					   const gchar     *format,
					   gdouble          width,
					   gdouble          height,
					   EvPrintRange    *ranges,
					   gint             n_ranges,
					   EvPrintPageSet   page_set,
					   gint             pages_per_sheet,
					   gint             copies,
					   gdouble          collate,
					   gdouble          reverse);
void            ev_job_print_run          (EvJobPrint      *print);

G_END_DECLS

#endif /* __EV_JOBS_H__ */
