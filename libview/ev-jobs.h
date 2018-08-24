/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2008 Carlos Garcia Campos <carlosgc@gnome.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#if !defined (__EV_EVINCE_VIEW_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-view.h> can be included directly."
#endif

#ifndef __EV_JOBS_H__
#define __EV_JOBS_H__

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <cairo.h>

#include <evince-document.h>

G_BEGIN_DECLS

typedef struct _EvJob EvJob;
typedef struct _EvJobClass EvJobClass;

typedef struct _EvJobRender EvJobRender;
typedef struct _EvJobRenderClass EvJobRenderClass;

typedef struct _EvJobPageData EvJobPageData;
typedef struct _EvJobPageDataClass EvJobPageDataClass;

typedef struct _EvJobThumbnail EvJobThumbnail;
typedef struct _EvJobThumbnailClass EvJobThumbnailClass;

typedef struct _EvJobLinks EvJobLinks;
typedef struct _EvJobLinksClass EvJobLinksClass;

typedef struct _EvJobAttachments EvJobAttachments;
typedef struct _EvJobAttachmentsClass EvJobAttachmentsClass;

typedef struct _EvJobAnnots EvJobAnnots;
typedef struct _EvJobAnnotsClass EvJobAnnotsClass;

typedef struct _EvJobFonts EvJobFonts;
typedef struct _EvJobFontsClass EvJobFontsClass;

typedef struct _EvJobLoad EvJobLoad;
typedef struct _EvJobLoadClass EvJobLoadClass;

typedef struct _EvJobLoadStream EvJobLoadStream;
typedef struct _EvJobLoadStreamClass EvJobLoadStreamClass;

typedef struct _EvJobLoadGFile EvJobLoadGFile;
typedef struct _EvJobLoadGFileClass EvJobLoadGFileClass;

typedef struct _EvJobLoadFd EvJobLoadFd;
typedef struct _EvJobLoadFdClass EvJobLoadFdClass;

typedef struct _EvJobSave EvJobSave;
typedef struct _EvJobSaveClass EvJobSaveClass;

typedef struct _EvJobFind EvJobFind;
typedef struct _EvJobFindClass EvJobFindClass;

typedef struct _EvJobLayers EvJobLayers;
typedef struct _EvJobLayersClass EvJobLayersClass;

typedef struct _EvJobExport EvJobExport;
typedef struct _EvJobExportClass EvJobExportClass;

typedef struct _EvJobPrint EvJobPrint;
typedef struct _EvJobPrintClass EvJobPrintClass;

#define EV_TYPE_JOB            (ev_job_get_type())
#define EV_JOB(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_JOB, EvJob))
#define EV_IS_JOB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_JOB))
#define EV_JOB_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_JOB, EvJobClass))
#define EV_IS_JOB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_JOB))
#define EV_JOB_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_JOB, EvJobClass))

#define EV_TYPE_JOB_LINKS            (ev_job_links_get_type())
#define EV_JOB_LINKS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_JOB_LINKS, EvJobLinks))
#define EV_IS_JOB_LINKS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_JOB_LINKS))
#define EV_JOB_LINKS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_JOB_LINKS, EvJobLinksClass))
#define EV_IS_JOB_LINKS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_JOB_LINKS))
#define EV_JOB_LINKS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_JOB_LINKS, EvJobLinksClass))

#define EV_TYPE_JOB_ATTACHMENTS           (ev_job_attachments_get_type())
#define EV_JOB_ATTACHMENTS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_JOB_ATTACHMENTS, EvJobAttachments))
#define EV_IS_JOB_ATTACHMENTS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_JOB_ATTACHMENTS))
#define EV_JOB_ATTACHMENTS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_JOB_ATTACHMENTS, EvJobAttachmentsClass))
#define EV_IS_JOB_ATTACHMENTS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_JOB_ATTACHMENTS))
#define EV_JOB_ATTACHMENTS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_JOB_ATTACHMENTS, EvJobAttachmentsClass))

#define EV_TYPE_JOB_ANNOTS            (ev_job_annots_get_type())
#define EV_JOB_ANNOTS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_JOB_ANNOTS, EvJobAnnots))
#define EV_IS_JOB_ANNOTS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_JOB_ANNOTS))
#define EV_JOB_ANNOTS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_JOB_ANNOTS, EvJobAnnotsClass))
#define EV_IS_JOB_ANNOTS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_JOB_ANNOTS))
#define EV_JOB_ANNOTS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_JOB_ANNOTS, EvJobAnnotsClass))

#define EV_TYPE_JOB_RENDER            (ev_job_render_get_type())
#define EV_JOB_RENDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_JOB_RENDER, EvJobRender))
#define EV_IS_JOB_RENDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_JOB_RENDER))
#define EV_JOB_RENDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_JOB_RENDER, EvJobRenderClass))
#define EV_IS_JOB_RENDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_JOB_RENDER))
#define EV_JOB_RENDER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_JOB_RENDER, EvJobRenderClass))

#define EV_TYPE_JOB_PAGE_DATA            (ev_job_page_data_get_type())
#define EV_JOB_PAGE_DATA(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_JOB_PAGE_DATA, EvJobPageData))
#define EV_IS_JOB_PAGE_DATA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_JOB_PAGE_DATA))
#define EV_JOB_PAGE_DATA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_JOB_PAGE_DATA, EvJobPageDataClass))
#define EV_IS_JOB_PAGE_DATA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_JOB_PAGE_DATA))
#define EV_JOB_PAGE_DATA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_JOB_PAGE_DATA, EvJobPageDataClass))

#define EV_TYPE_JOB_THUMBNAIL            (ev_job_thumbnail_get_type())
#define EV_JOB_THUMBNAIL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_JOB_THUMBNAIL, EvJobThumbnail))
#define EV_IS_JOB_THUMBNAIL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_JOB_THUMBNAIL))
#define EV_JOB_THUMBNAIL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_JOB_THUMBNAIL, EvJobThumbnailClass))
#define EV_IS_JOB_THUMBNAIL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_JOB_THUMBNAIL))
#define EV_JOB_THUMBNAIL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_JOB_THUMBNAIL, EvJobThumbnailClass))

#define EV_TYPE_JOB_FONTS            (ev_job_fonts_get_type())
#define EV_JOB_FONTS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_JOB_FONTS, EvJobFonts))
#define EV_IS_JOB_FONTS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_JOB_FONTS))
#define EV_JOB_FONTS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_JOB_FONTS, EvJobFontsClass))
#define EV_IS_JOB_FONTS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_JOB_FONTS))
#define EV_JOB_FONTS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_JOB_FONTS, EvJobFontsClass))


#define EV_TYPE_JOB_LOAD            (ev_job_load_get_type())
#define EV_JOB_LOAD(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_JOB_LOAD, EvJobLoad))
#define EV_IS_JOB_LOAD(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_JOB_LOAD))
#define EV_JOB_LOAD_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_JOB_LOAD, EvJobLoadClass))
#define EV_IS_JOB_LOAD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_JOB_LOAD))
#define EV_JOB_LOAD_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_JOB_LOAD, EvJobLoadClass))

#define EV_TYPE_JOB_LOAD_STREAM            (ev_job_load_stream_get_type())
#define EV_JOB_LOAD_STREAM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_JOB_LOAD_STREAM, EvJobLoadStream))
#define EV_IS_JOB_LOAD_STREAM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_JOB_LOAD_STREAM))
#define EV_JOB_LOAD_STREAM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_JOB_LOAD_STREAM, EvJobLoadStreamClass))
#define EV_IS_JOB_LOAD_STREAM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_JOB_LOAD_STREAM))
#define EV_JOB_LOAD_STREAM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_JOB_LOAD_STREAM, EvJobLoadStreamClass))

#define EV_TYPE_JOB_LOAD_GFILE            (ev_job_load_gfile_get_type())
#define EV_JOB_LOAD_GFILE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_JOB_LOAD_GFILE, EvJobLoadGFile))
#define EV_IS_JOB_LOAD_GFILE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_JOB_LOAD_GFILE))
#define EV_JOB_LOAD_GFILE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_JOB_LOAD_GFILE, EvJobLoadGFileClass))
#define EV_IS_JOB_LOAD_GFILE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_JOB_LOAD_GFILE))
#define EV_JOB_LOAD_GFILE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_JOB_LOAD_GFILE, EvJobLoadGFileClass))

#define EV_TYPE_JOB_LOAD_FD            (ev_job_load_fd_get_type())
#define EV_JOB_LOAD_FD(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_JOB_LOAD_FD, EvJobLoadFd))
#define EV_IS_JOB_LOAD_FD(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_JOB_LOAD_FD))
#define EV_JOB_LOAD_FD_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_JOB_LOAD_FD, EvJobLoadFdClass))
#define EV_IS_JOB_LOAD_FD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_JOB_LOAD_FD))
#define EV_JOB_LOAD_FD_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_JOB_LOAD_FD, EvJobLoadFdClass))

#define EV_TYPE_JOB_SAVE            (ev_job_save_get_type())
#define EV_JOB_SAVE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_JOB_SAVE, EvJobSave))
#define EV_IS_JOB_SAVE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_JOB_SAVE))
#define EV_JOB_SAVE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_JOB_SAVE, EvJobSaveClass))
#define EV_IS_JOB_SAVE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_JOB_SAVE))
#define EV_JOB_SAVE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_JOB_SAVE, EvJobSaveClass))

#define EV_TYPE_JOB_FIND            (ev_job_find_get_type())
#define EV_JOB_FIND(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_JOB_FIND, EvJobFind))
#define EV_IS_JOB_FIND(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_JOB_FIND))
#define EV_JOB_FIND_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_JOB_FIND, EvJobFindClass))
#define EV_IS_JOB_FIND_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_JOB_FIND))
#define EV_JOB_FIND_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_JOB_FIND, EvJobFindClass))

#define EV_TYPE_JOB_LAYERS            (ev_job_layers_get_type())
#define EV_JOB_LAYERS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_JOB_LAYERS, EvJobLayers))
#define EV_IS_JOB_LAYERS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_JOB_LAYERS))
#define EV_JOB_LAYERS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_JOB_LAYERS, EvJobLayersClass))
#define EV_IS_JOB_LAYERS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_JOB_LAYERS))
#define EV_JOB_LAYERS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_JOB_LAYERS, EvJobLayersClass))

#define EV_TYPE_JOB_EXPORT            (ev_job_export_get_type())
#define EV_JOB_EXPORT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_JOB_EXPORT, EvJobExport))
#define EV_IS_JOB_EXPORT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_JOB_EXPORT))
#define EV_JOB_EXPORT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_JOB_EXPORT, EvJobExportClass))
#define EV_IS_JOB_EXPORT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_JOB_EXPORT))
#define EV_JOB_EXPORT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_JOB_EXPORT, EvJobExportClass))

#define EV_TYPE_JOB_PRINT            (ev_job_print_get_type())
#define EV_JOB_PRINT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_JOB_PRINT, EvJobPrint))
#define EV_IS_JOB_PRINT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_JOB_PRINT))
#define EV_JOB_PRINT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_JOB_PRINT, EvJobPrintClass))
#define EV_IS_JOB_PRINT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_JOB_PRINT))
#define EV_JOB_PRINT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_JOB_PRINT, EvJobPrintClass))

typedef enum {
	EV_JOB_RUN_THREAD,
	EV_JOB_RUN_MAIN_LOOP
} EvJobRunMode;

struct _EvJob
{
	GObject parent;
	
	EvDocument *document;

	EvJobRunMode run_mode;

	guint cancelled : 1;
	guint finished : 1;
	guint failed : 1;
	
	GError *error;
	GCancellable *cancellable;

	guint idle_finished_id;
	guint idle_cancelled_id;
};

struct _EvJobClass
{
	GObjectClass parent_class;

	gboolean (*run)         (EvJob *job);
	
	/* Signals */
	void     (* cancelled)  (EvJob *job);
	void     (* finished)   (EvJob *job);
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

struct _EvJobAttachments
{
	EvJob parent;

	GList *attachments;
};

struct _EvJobAttachmentsClass
{
	EvJobClass parent_class;
};

struct _EvJobAnnots
{
	EvJob parent;

	GList *annots;
};

struct _EvJobAnnotsClass
{
	EvJobClass parent_class;
};

struct _EvJobRender
{
	EvJob parent;

	gint page;
	gint rotation;
	gdouble scale;

	gboolean page_ready;
	gint target_width;
	gint target_height;
	cairo_surface_t *surface;

	gboolean include_selection;
	cairo_surface_t *selection;
	cairo_region_t *selection_region;
	EvRectangle selection_points;
	EvSelectionStyle selection_style;
	GdkColor base;
	GdkColor text;
};

struct _EvJobRenderClass
{
	EvJobClass parent_class;
};

typedef enum {
        EV_PAGE_DATA_INCLUDE_NONE           = 0,
        EV_PAGE_DATA_INCLUDE_LINKS          = 1 << 0,
        EV_PAGE_DATA_INCLUDE_TEXT           = 1 << 1,
        EV_PAGE_DATA_INCLUDE_TEXT_MAPPING   = 1 << 2,
        EV_PAGE_DATA_INCLUDE_TEXT_LAYOUT    = 1 << 3,
        EV_PAGE_DATA_INCLUDE_TEXT_ATTRS     = 1 << 4,
        EV_PAGE_DATA_INCLUDE_TEXT_LOG_ATTRS = 1 << 5,
        EV_PAGE_DATA_INCLUDE_IMAGES         = 1 << 6,
        EV_PAGE_DATA_INCLUDE_FORMS          = 1 << 7,
        EV_PAGE_DATA_INCLUDE_ANNOTS         = 1 << 8,
        EV_PAGE_DATA_INCLUDE_MEDIA          = 1 << 9,
        EV_PAGE_DATA_INCLUDE_ALL            = (1 << 10) - 1
} EvJobPageDataFlags;

struct _EvJobPageData
{
	EvJob parent;

	gint page;
	EvJobPageDataFlags flags;

	EvMappingList  *link_mapping;
	EvMappingList  *image_mapping;
	EvMappingList  *form_field_mapping;
	EvMappingList  *annot_mapping;
        EvMappingList  *media_mapping;
	cairo_region_t *text_mapping;
	gchar *text;
	EvRectangle *text_layout;
	guint text_layout_length;
        PangoAttrList *text_attrs;
        PangoLogAttr *text_log_attrs;
        gulong text_log_attrs_length;
};

struct _EvJobPageDataClass
{
	EvJobClass parent_class;
};

typedef enum {
        EV_JOB_THUMBNAIL_PIXBUF,
        EV_JOB_THUMBNAIL_SURFACE
} EvJobThumbnailFormat;

struct _EvJobThumbnail
{
	EvJob parent;

	gint page;
	gint rotation;
	gdouble scale;
	gint target_width;
	gint target_height;

	GdkPixbuf *thumbnail;
        gboolean has_frame;

        EvJobThumbnailFormat format;
        cairo_surface_t *thumbnail_surface;
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

	/* Signals */
	void (* updated)  (EvJobFonts *job,
			   gdouble     progress);
};

struct _EvJobLoad
{
	EvJob parent;

	gchar *uri;
	gchar *password;
};

struct _EvJobLoadClass
{
	EvJobClass parent_class;
};

struct _EvJobLoadStream
{
        EvJob parent;

        char *mime_type;
        char *password;
        GInputStream *stream;
        EvDocumentLoadFlags flags;
};

struct _EvJobLoadStreamClass
{
        EvJobClass parent_class;
};

struct _EvJobLoadGFile
{
        EvJob parent;

        char *password;
        GFile *gfile;
        EvDocumentLoadFlags flags;
};

struct _EvJobLoadGFileClass
{
        EvJobClass parent_class;
};

struct _EvJobLoadFd
{
        EvJob parent;

        char *mime_type;
        char *password;
        int fd;
        EvDocumentLoadFlags flags;
};

struct _EvJobLoadFdClass
{
        EvJobClass parent_class;
};

struct _EvJobSave
{
	EvJob parent;

	gchar *uri;
	gchar *document_uri;
};

struct _EvJobSaveClass
{
	EvJobClass parent_class;
};

struct _EvJobFind
{
	EvJob parent;

	gint start_page;
	gint current_page;
	gint n_pages;
	GList **pages;
	gchar *text;
	gboolean case_sensitive;
	gboolean has_results;
        EvFindOptions options;
};

struct _EvJobFindClass
{
	EvJobClass parent_class;

	/* Signals */
	void (* updated)  (EvJobFind *job,
			   gint       page);
};

struct _EvJobLayers
{
	EvJob parent;

	GtkTreeModel *model;
};

struct _EvJobLayersClass
{
	EvJobClass parent_class;
};

struct _EvJobExport
{
	EvJob parent;

	gint page;
	EvRenderContext *rc;
};

struct _EvJobExportClass
{
	EvJobClass parent_class;
};

struct _EvJobPrint
{
	EvJob parent;

	gint page;
	cairo_t *cr;
};

struct _EvJobPrintClass
{
	EvJobClass parent_class;
};

/* Base job class */
GType           ev_job_get_type           (void) G_GNUC_CONST;
gboolean        ev_job_run                (EvJob          *job);
void            ev_job_cancel             (EvJob          *job);
void            ev_job_failed             (EvJob          *job,
					   GQuark          domain,
					   gint            code,
					   const gchar    *format,
					   ...) G_GNUC_PRINTF (4, 5);
void            ev_job_failed_from_error  (EvJob          *job,
					   GError         *error);
void            ev_job_succeeded          (EvJob          *job);
gboolean        ev_job_is_finished        (EvJob          *job);
gboolean        ev_job_is_failed          (EvJob          *job);
EvJobRunMode    ev_job_get_run_mode       (EvJob          *job);
void            ev_job_set_run_mode       (EvJob          *job,
					   EvJobRunMode    run_mode);

/* EvJobLinks */
GType           ev_job_links_get_type     (void) G_GNUC_CONST;
EvJob          *ev_job_links_new          (EvDocument     *document);
GtkTreeModel   *ev_job_links_get_model    (EvJobLinks     *job);

/* EvJobAttachments */
GType           ev_job_attachments_get_type (void) G_GNUC_CONST;
EvJob          *ev_job_attachments_new      (EvDocument     *document);

/* EvJobAnnots */
GType           ev_job_annots_get_type      (void) G_GNUC_CONST;
EvJob          *ev_job_annots_new           (EvDocument     *document);

/* EvJobRender */
GType           ev_job_render_get_type    (void) G_GNUC_CONST;
EvJob          *ev_job_render_new         (EvDocument      *document,
					   gint             page,
					   gint             rotation,
					   gdouble          scale,
					   gint             width,
					   gint             height);
void     ev_job_render_set_selection_info (EvJobRender     *job,
					   EvRectangle     *selection_points,
					   EvSelectionStyle selection_style,
					   GdkColor        *text,
					   GdkColor        *base);
/* EvJobPageData */
GType           ev_job_page_data_get_type (void) G_GNUC_CONST;
EvJob          *ev_job_page_data_new      (EvDocument      *document,
					   gint             page,
					   EvJobPageDataFlags flags);

/* EvJobThumbnail */
GType           ev_job_thumbnail_get_type      (void) G_GNUC_CONST;
EvJob          *ev_job_thumbnail_new           (EvDocument      *document,
                                                gint             page,
                                                gint             rotation,
                                                gdouble          scale);
EvJob	       *ev_job_thumbnail_new_with_target_size (EvDocument      *document,
						       gint		page,
						       gint		rotation,
						       gint		target_width,
						       gint		target_height);
void            ev_job_thumbnail_set_has_frame (EvJobThumbnail  *job,
                                                gboolean         has_frame);
void            ev_job_thumbnail_set_output_format (EvJobThumbnail      *job,
                                                    EvJobThumbnailFormat format);
/* EvJobFonts */
GType 		ev_job_fonts_get_type 	  (void) G_GNUC_CONST;
EvJob 	       *ev_job_fonts_new 	  (EvDocument      *document);

/* EvJobLoad */
GType 		ev_job_load_get_type 	  (void) G_GNUC_CONST;
EvJob 	       *ev_job_load_new 	  (const gchar 	   *uri);
void            ev_job_load_set_uri       (EvJobLoad       *load,
					   const gchar     *uri);
void            ev_job_load_set_password  (EvJobLoad       *job,
					   const gchar     *password);

/* EvJobLoadStream */
GType           ev_job_load_stream_get_type       (void) G_GNUC_CONST;
EvJob          *ev_job_load_stream_new            (GInputStream       *stream,
                                                   EvDocumentLoadFlags flags);
void            ev_job_load_stream_set_stream     (EvJobLoadStream    *job,
                                                   GInputStream       *stream);
void            ev_job_load_stream_set_mime_type  (EvJobLoadStream    *job,
                                                   const char         *mime_type);
void            ev_job_load_stream_set_load_flags (EvJobLoadStream    *job,
                                                   EvDocumentLoadFlags flags);
void            ev_job_load_stream_set_password   (EvJobLoadStream    *job,
                                                   const gchar        *password);

/* EvJobLoadGFile */
GType           ev_job_load_gfile_get_type        (void) G_GNUC_CONST;
EvJob          *ev_job_load_gfile_new             (GFile              *gfile,
                                                   EvDocumentLoadFlags flags);
void            ev_job_load_gfile_set_gfile       (EvJobLoadGFile     *job,
                                                   GFile              *gfile);
void            ev_job_load_gfile_set_load_flags  (EvJobLoadGFile     *job,
                                                   EvDocumentLoadFlags flags);
void            ev_job_load_gfile_set_password    (EvJobLoadGFile     *job,
                                                   const gchar        *password);


/* EvJobLoadFd */
GType           ev_job_load_fd_get_type       (void) G_GNUC_CONST;
EvJob          *ev_job_load_fd_new            (int                 fd,
                                               const char         *mime_type,
                                               EvDocumentLoadFlags flags,
                                               GError            **error);
EvJob          *ev_job_load_fd_new_take       (int                 fd,
                                               const char         *mime_type,
                                               EvDocumentLoadFlags flags);
gboolean        ev_job_load_fd_set_fd         (EvJobLoadFd        *job,
                                               int                 fd,
                                               GError            **error);
void            ev_job_load_fd_take_fd        (EvJobLoadFd        *job,
                                               int                 fd);
void            ev_job_load_fd_set_mime_type  (EvJobLoadFd        *job,
                                               const char         *mime_type);
void            ev_job_load_fd_set_load_flags (EvJobLoadFd        *job,
                                               EvDocumentLoadFlags flags);
void            ev_job_load_fd_set_password   (EvJobLoadFd        *job,
                                               const gchar        *password);

/* EvJobSave */
GType           ev_job_save_get_type      (void) G_GNUC_CONST;
EvJob          *ev_job_save_new           (EvDocument      *document,
					   const gchar     *uri,
					   const gchar     *document_uri);
/* EvJobFind */
GType           ev_job_find_get_type      (void) G_GNUC_CONST;
EvJob          *ev_job_find_new           (EvDocument      *document,
					   gint             start_page,
					   gint             n_pages,
					   const gchar     *text,
					   gboolean         case_sensitive);
void            ev_job_find_set_options   (EvJobFind       *job,
                                           EvFindOptions    options);
EvFindOptions   ev_job_find_get_options   (EvJobFind       *job);
gint            ev_job_find_get_n_results (EvJobFind       *job,
					   gint             pages);
gdouble         ev_job_find_get_progress  (EvJobFind       *job);
gboolean        ev_job_find_has_results   (EvJobFind       *job);
GList         **ev_job_find_get_results   (EvJobFind       *job);

/* EvJobLayers */
GType           ev_job_layers_get_type    (void) G_GNUC_CONST;
EvJob          *ev_job_layers_new         (EvDocument     *document);

/* EvJobExport */
GType           ev_job_export_get_type    (void) G_GNUC_CONST;
EvJob          *ev_job_export_new         (EvDocument     *document);
void            ev_job_export_set_page    (EvJobExport    *job,
					   gint            page);
/* EvJobPrint */
GType           ev_job_print_get_type    (void) G_GNUC_CONST;
EvJob          *ev_job_print_new         (EvDocument     *document);
void            ev_job_print_set_page    (EvJobPrint     *job,
					  gint            page);
void            ev_job_print_set_cairo   (EvJobPrint     *job,
					  cairo_t        *cr);

G_END_DECLS

#endif /* __EV_JOBS_H__ */
