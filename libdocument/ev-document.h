/*
 *  Copyright (C) 2009 Carlos Garcia Campos
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#include <glib-object.h>
#include <glib.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo.h>

#include "ev-macros.h"
#include "ev-document-info.h"
#include "ev-page.h"
#include "ev-render-context.h"

G_BEGIN_DECLS

#define EV_TYPE_DOCUMENT            (ev_document_get_type ())

EV_PUBLIC
G_DECLARE_DERIVABLE_TYPE (EvDocument, ev_document, EV, DOCUMENT, GObject)

#define EV_DOCUMENT_ERROR ev_document_error_quark ()
#define EV_DOC_MUTEX_LOCK (ev_document_doc_mutex_lock ())
#define EV_DOC_MUTEX_UNLOCK (ev_document_doc_mutex_unlock ())

typedef enum /*< flags >*/ {
        EV_DOCUMENT_LOAD_FLAG_NONE = 0,
        EV_DOCUMENT_LOAD_FLAG_NO_CACHE
} EvDocumentLoadFlags;

typedef enum
{
        EV_DOCUMENT_ERROR_INVALID,
        EV_DOCUMENT_ERROR_UNSUPPORTED_CONTENT,
        EV_DOCUMENT_ERROR_ENCRYPTED
} EvDocumentError;

typedef struct _EvPoint EvPoint;
typedef struct _EvRectangle EvRectangle;
typedef struct _EvMapping EvMapping;
typedef struct _EvSourceLink EvSourceLink;
typedef struct _EvDocumentBackendInfo EvDocumentBackendInfo;

struct _EvPoint {
        double x;
        double y;
};

struct _EvDocumentBackendInfo
{
	const gchar *name;
	const gchar *version;
};

struct _EvDocumentClass
{
        GObjectClass base_class;

        /* Virtual Methods  */
        gboolean          (* load)                  (EvDocument          *document,
						     const char          *uri,
						     GError             **error);
        gboolean          (* save)                  (EvDocument          *document,
						     const char          *uri,
						     GError             **error);
        gint              (* get_n_pages)           (EvDocument          *document);
	EvPage          * (* get_page)              (EvDocument          *document,
						     gint                 index);
        void              (* get_page_size)         (EvDocument          *document,
						     EvPage              *page_index,
						     double              *width,
						     double              *height);
        gchar           * (* get_page_label)        (EvDocument          *document,
						     EvPage              *page);
        cairo_surface_t * (* render)                (EvDocument          *document,
						     EvRenderContext     *rc);
	GdkPixbuf       * (* get_thumbnail)         (EvDocument          *document,
						     EvRenderContext     *rc);
        EvDocumentInfo  * (* get_info)              (EvDocument          *document);
        gboolean          (* get_backend_info)      (EvDocument          *document,
						     EvDocumentBackendInfo *info);
        gboolean	  (* support_synctex)       (EvDocument          *document);

        /* GIO streams */
        gboolean          (* load_stream)           (EvDocument          *document,
						     GInputStream        *stream,
						     EvDocumentLoadFlags  flags,
						     GCancellable        *cancellable,
						     GError             **error);
        gboolean          (* load_gfile)            (EvDocument          *document,
						     GFile               *file,
						     EvDocumentLoadFlags  flags,
						     GCancellable        *cancellable,
						     GError             **error);
	cairo_surface_t * (* get_thumbnail_surface) (EvDocument          *document,
						     EvRenderContext     *rc);
        gboolean          (* load_fd)               (EvDocument          *document,
						     int                  fd,
						     EvDocumentLoadFlags  flags,
						     GCancellable        *cancellable,
						     GError             **error);
};

EV_PUBLIC
GQuark           ev_document_error_quark          (void);

/* Document mutex */
EV_PUBLIC
void             ev_document_doc_mutex_lock       (void);
EV_PUBLIC
void             ev_document_doc_mutex_unlock     (void);
EV_PUBLIC
gboolean         ev_document_doc_mutex_trylock    (void);

/* FontConfig mutex */
EV_PUBLIC
void             ev_document_fc_mutex_lock        (void);
EV_PUBLIC
void             ev_document_fc_mutex_unlock      (void);
EV_PUBLIC
gboolean         ev_document_fc_mutex_trylock     (void);

EV_PUBLIC
EvDocumentInfo  *ev_document_get_info             (EvDocument      *document);
EV_PUBLIC
gboolean         ev_document_get_backend_info     (EvDocument      *document,
						   EvDocumentBackendInfo *info);
EV_PUBLIC
gboolean         ev_document_get_modified         (EvDocument      *document);
EV_PUBLIC
void             ev_document_set_modified         (EvDocument      *document,
						   gboolean         modified);
EV_PUBLIC
gboolean         ev_document_load                 (EvDocument      *document,
						   const char      *uri,
						   GError         **error);
EV_PUBLIC
gboolean         ev_document_load_full            (EvDocument           *document,
						   const char           *uri,
						   EvDocumentLoadFlags   flags,
						   GError              **error);
EV_PUBLIC
gboolean         ev_document_load_stream          (EvDocument         *document,
                                                   GInputStream       *stream,
                                                   EvDocumentLoadFlags flags,
                                                   GCancellable       *cancellable,
                                                   GError            **error);
EV_PUBLIC
gboolean         ev_document_load_gfile           (EvDocument         *document,
                                                   GFile              *file,
                                                   EvDocumentLoadFlags flags,
                                                   GCancellable       *cancellable,
                                                   GError            **error);
EV_PUBLIC
gboolean         ev_document_load_fd              (EvDocument         *document,
                                                   int                 fd,
                                                   EvDocumentLoadFlags flags,
                                                   GCancellable       *cancellable,
                                                   GError            **error);
EV_PUBLIC
gboolean         ev_document_save                 (EvDocument      *document,
						   const char      *uri,
						   GError         **error);
EV_PUBLIC
gint             ev_document_get_n_pages          (EvDocument      *document);
EV_PUBLIC
EvPage          *ev_document_get_page             (EvDocument      *document,
						   gint             index);
EV_PUBLIC
void             ev_document_get_page_size        (EvDocument      *document,
						   gint             page_index,
						   double          *width,
						   double          *height);
EV_PUBLIC
gchar           *ev_document_get_page_label       (EvDocument      *document,
						   gint             page_index);
EV_PUBLIC
cairo_surface_t *ev_document_render               (EvDocument      *document,
						   EvRenderContext *rc);
EV_PUBLIC
GdkPixbuf       *ev_document_get_thumbnail        (EvDocument      *document,
						   EvRenderContext *rc);
EV_PUBLIC
cairo_surface_t *ev_document_get_thumbnail_surface (EvDocument      *document,
						    EvRenderContext *rc);
EV_PUBLIC
guint64          ev_document_get_size             (EvDocument      *document);
EV_PUBLIC
const gchar     *ev_document_get_uri              (EvDocument      *document);
EV_PUBLIC
const gchar     *ev_document_get_title            (EvDocument      *document);
EV_PUBLIC
gboolean         ev_document_is_page_size_uniform (EvDocument      *document);
EV_PUBLIC
void             ev_document_get_max_page_size    (EvDocument      *document,
						   gdouble         *width,
						   gdouble         *height);
EV_PUBLIC
void             ev_document_get_min_page_size    (EvDocument      *document,
						   gdouble         *width,
						   gdouble         *height);
EV_PUBLIC
gboolean         ev_document_check_dimensions     (EvDocument      *document);
EV_PUBLIC
gint             ev_document_get_max_label_len    (EvDocument      *document);
EV_PUBLIC
gboolean         ev_document_has_text_page_labels (EvDocument      *document);
EV_PUBLIC
gboolean         ev_document_find_page_by_label   (EvDocument      *document,
						   const gchar     *page_label,
						   gint            *page_index);
EV_PUBLIC
gboolean	 ev_document_has_synctex 	  (EvDocument      *document);

EV_PUBLIC
EvSourceLink    *ev_document_synctex_backward_search
                                                  (EvDocument      *document,
                                                   gint             page_index,
                                                   gfloat           x,
                                                   gfloat           y);

EV_PUBLIC
EvMapping       *ev_document_synctex_forward_search
                                                  (EvDocument      *document,
						   EvSourceLink    *source_link);

EV_PUBLIC
gint             ev_rect_cmp                      (EvRectangle     *a,
					           EvRectangle     *b);

#define EV_TYPE_RECTANGLE (ev_rectangle_get_type ())
struct _EvRectangle
{
	gdouble x1;
	gdouble y1;
	gdouble x2;
	gdouble y2;
};

EV_PUBLIC
GType        ev_rectangle_get_type (void) G_GNUC_CONST;
EV_PUBLIC
EvRectangle *ev_rectangle_new      (void);
EV_PUBLIC
EvRectangle *ev_rectangle_copy     (EvRectangle *ev_rect);
EV_PUBLIC
void         ev_rectangle_free     (EvRectangle *ev_rect);

struct _EvMapping {
	EvRectangle area;
	gpointer    data;
};

#define EV_TYPE_SOURCE_LINK (ev_source_link_get_type ())
struct _EvSourceLink
{
        gchar *filename;
        gint   line;
        gint   col;
};

EV_PUBLIC
GType          ev_source_link_get_type (void) G_GNUC_CONST;
EV_PUBLIC
EvSourceLink  *ev_source_link_new      (const gchar *filename,
					gint         line,
					gint         col);
EV_PUBLIC
EvSourceLink  *ev_source_link_copy     (EvSourceLink *link);
EV_PUBLIC
void           ev_source_link_free     (EvSourceLink *link);

/* backends shall implement this function to be able to be opened by Evince
 */
EV_PUBLIC
GType ev_backend_query_type (void);

G_END_DECLS
