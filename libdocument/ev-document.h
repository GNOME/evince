/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
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
 *
 *  $Id$
 */

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#ifndef EV_DOCUMENT_H
#define EV_DOCUMENT_H

#include <glib-object.h>
#include <glib.h>
#include <gio/gio.h>
#include <gdk/gdk.h>
#include <cairo.h>

#include "ev-document-info.h"
#include "ev-page.h"
#include "ev-render-context.h"

G_BEGIN_DECLS

#define EV_TYPE_DOCUMENT            (ev_document_get_type ())
#define EV_DOCUMENT(o)              (G_TYPE_CHECK_INSTANCE_CAST ((o), EV_TYPE_DOCUMENT, EvDocument))
#define EV_DOCUMENT_CLASS(k)        (G_TYPE_CHECK_CLASS_CAST((k), EV_TYPE_DOCUMENT, EvDocumentClass))
#define EV_IS_DOCUMENT(o)           (G_TYPE_CHECK_INSTANCE_TYPE ((o), EV_TYPE_DOCUMENT))
#define EV_IS_DOCUMENT_CLASS(k)     (G_TYPE_CHECK_CLASS_TYPE ((k), EV_TYPE_DOCUMENT))
#define EV_DOCUMENT_GET_CLASS(inst) (G_TYPE_INSTANCE_GET_CLASS ((inst), EV_TYPE_DOCUMENT, EvDocumentClass))

typedef struct _EvDocument        EvDocument;
typedef struct _EvDocumentClass   EvDocumentClass;
typedef struct _EvDocumentPrivate EvDocumentPrivate;

#define EV_DOCUMENT_ERROR ev_document_error_quark ()
#define EV_DOC_MUTEX_LOCK (ev_document_doc_mutex_lock ())
#define EV_DOC_MUTEX_UNLOCK (ev_document_doc_mutex_unlock ())

typedef enum /*< flags >*/ {
        EV_DOCUMENT_LOAD_FLAG_NONE = 0
} EvDocumentLoadFlags;

typedef enum
{
        EV_DOCUMENT_ERROR_INVALID,
        EV_DOCUMENT_ERROR_ENCRYPTED
} EvDocumentError;

typedef struct {
        double x;
        double y;
} EvPoint;

typedef struct _EvRectangle EvRectangle;
typedef struct _EvMapping EvMapping;
typedef struct _EvSourceLink EvSourceLink;
typedef struct _EvDocumentBackendInfo EvDocumentBackendInfo;

struct _EvDocumentBackendInfo
{
	const gchar *name;
	const gchar *version;
};

struct _EvDocument
{
	GObject base;

	EvDocumentPrivate *priv;
};

struct _EvDocumentClass
{
        GObjectClass base_class;

        /* Virtual Methods  */
        gboolean          (* load)            (EvDocument      *document,
                                               const char      *uri,
                                               GError         **error);
        gboolean          (* save)            (EvDocument      *document,
                                               const char      *uri,
                                               GError         **error);
        gint              (* get_n_pages)     (EvDocument      *document);
	EvPage          * (* get_page)        (EvDocument      *document,
					       gint             index);
        void              (* get_page_size)   (EvDocument      *document,
                                               EvPage          *page,
                                               double          *width,
                                               double          *height);
        gchar           * (* get_page_label)  (EvDocument      *document,
                                               EvPage          *page);
        cairo_surface_t * (* render)          (EvDocument      *document,
                                               EvRenderContext *rc);
	GdkPixbuf       * (* get_thumbnail)   (EvDocument      *document,
					       EvRenderContext *rc);
        EvDocumentInfo  * (* get_info)        (EvDocument      *document);
        gboolean          (* get_backend_info)(EvDocument      *document,
                                               EvDocumentBackendInfo *info);
        gboolean	  (* support_synctex) (EvDocument      *document);

        /* GIO streams */
        gboolean          (* load_stream)     (EvDocument          *document,
                                               GInputStream        *stream,
                                               EvDocumentLoadFlags  flags,
                                               GCancellable        *cancellable,
                                               GError             **error);
        gboolean          (* load_gfile)      (EvDocument          *document,
                                               GFile               *file,
                                               EvDocumentLoadFlags  flags,
                                               GCancellable        *cancellable,
                                               GError             **error);
};

GType            ev_document_get_type             (void) G_GNUC_CONST;
GQuark           ev_document_error_quark          (void);

/* Document mutex */
GMutex          *ev_document_get_doc_mutex        (void);
void             ev_document_doc_mutex_lock       (void);
void             ev_document_doc_mutex_unlock     (void);
gboolean         ev_document_doc_mutex_trylock    (void);

/* FontConfig mutex */
GMutex          *ev_document_get_fc_mutex         (void);
void             ev_document_fc_mutex_lock        (void);
void             ev_document_fc_mutex_unlock      (void);
gboolean         ev_document_fc_mutex_trylock     (void);

EvDocumentInfo  *ev_document_get_info             (EvDocument      *document);
gboolean         ev_document_get_backend_info     (EvDocument      *document,
						   EvDocumentBackendInfo *info);
gboolean         ev_document_load                 (EvDocument      *document,
						   const char      *uri,
						   GError         **error);
gboolean         ev_document_load_stream          (EvDocument         *document,
                                                   GInputStream       *stream,
                                                   EvDocumentLoadFlags flags,
                                                   GCancellable       *cancellable,
                                                   GError            **error);
gboolean         ev_document_load_gfile           (EvDocument         *document,
                                                   GFile              *file,
                                                   EvDocumentLoadFlags flags,
                                                   GCancellable       *cancellable,
                                                   GError            **error);
gboolean         ev_document_save                 (EvDocument      *document,
						   const char      *uri,
						   GError         **error);
gint             ev_document_get_n_pages          (EvDocument      *document);
EvPage          *ev_document_get_page             (EvDocument      *document,
						   gint             index);
void             ev_document_get_page_size        (EvDocument      *document,
						   gint             page_index,
						   double          *width,
						   double          *height);
gchar           *ev_document_get_page_label       (EvDocument      *document,
						   gint             page_index);
cairo_surface_t *ev_document_render               (EvDocument      *document,
						   EvRenderContext *rc);
GdkPixbuf       *ev_document_get_thumbnail        (EvDocument      *document,
						   EvRenderContext *rc);
const gchar     *ev_document_get_uri              (EvDocument      *document);
const gchar     *ev_document_get_title            (EvDocument      *document);
gboolean         ev_document_is_page_size_uniform (EvDocument      *document);
void             ev_document_get_max_page_size    (EvDocument      *document,
						   gdouble         *width,
						   gdouble         *height);
void             ev_document_get_min_page_size    (EvDocument      *document,
						   gdouble         *width,
						   gdouble         *height);
gboolean         ev_document_check_dimensions     (EvDocument      *document);
gint             ev_document_get_max_label_len    (EvDocument      *document);
gboolean         ev_document_has_text_page_labels (EvDocument      *document);
gboolean         ev_document_find_page_by_label   (EvDocument      *document,
						   const gchar     *page_label,
						   gint            *page_index);
gboolean	 ev_document_has_synctex 	  (EvDocument      *document);

EvSourceLink    *ev_document_synctex_backward_search
                                                  (EvDocument      *document,
                                                   gint             page_index,
                                                   gfloat           x,
                                                   gfloat           y);

EvMapping       *ev_document_synctex_forward_search
                                                  (EvDocument      *document,
						   EvSourceLink    *source_link);

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

GType        ev_rectangle_get_type (void) G_GNUC_CONST;
EvRectangle *ev_rectangle_new      (void);
EvRectangle *ev_rectangle_copy     (EvRectangle *ev_rect);
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

GType          ev_source_link_get_type (void) G_GNUC_CONST;
EvSourceLink  *ev_source_link_new      (const gchar *filename,
					gint         line,
					gint         col);
EvSourceLink  *ev_source_link_copy     (EvSourceLink *link);
void           ev_source_link_free     (EvSourceLink *link);

/* convenience macro to ease interface addition in the CODE
 * section of EV_BACKEND_REGISTER_WITH_CODE (this macro relies on
 * the g_define_type_id present within EV_BACKEND_REGISTER_WITH_CODE()).
 * usage example:
 * EV_BACKEND_REGISTER_WITH_CODE (PdfDocument, pdf_document,
 *                          EV_BACKEND_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_THUMBNAILS,
 *                                                 pdf_document_document_thumbnails_iface_init));
 */
#define EV_BACKEND_IMPLEMENT_INTERFACE(TYPE_IFACE, iface_init) {                \
	const GInterfaceInfo g_implement_interface_info = {                     \
		(GInterfaceInitFunc) iface_init, NULL, NULL                     \
	};                                                                      \
	g_type_module_add_interface (module,                                    \
				     g_define_type_id,                          \
				     TYPE_IFACE,                                \
				     &g_implement_interface_info);              \
}

/*
 * Utility macro used to register backends
 *
 * use: EV_BACKEND_REGISTER_WITH_CODE(BackendName, backend_name, CODE)
 */
#define EV_BACKEND_REGISTER_WITH_CODE(BackendName, backend_name, CODE)	        \
										\
static GType g_define_type_id = 0;						\
										\
GType										\
backend_name##_get_type (void)							\
{										\
	return g_define_type_id;						\
}										\
										\
static void     backend_name##_init              (BackendName        *self);	\
static void     backend_name##_class_init        (BackendName##Class *klass);	\
static gpointer backend_name##_parent_class = NULL;				\
static void     backend_name##_class_intern_init (gpointer klass)		\
{										\
	backend_name##_parent_class = g_type_class_peek_parent (klass);		\
	backend_name##_class_init ((BackendName##Class *) klass);		\
}										\
										\
G_MODULE_EXPORT GType								\
register_evince_backend (GTypeModule *module)					\
{										\
	const GTypeInfo our_info = {  				                \
		sizeof (BackendName##Class),					\
		NULL, /* base_init */						\
		NULL, /* base_finalize */					\
		(GClassInitFunc) backend_name##_class_intern_init,		\
		NULL,								\
		NULL, /* class_data */						\
		sizeof (BackendName),						\
		0, /* n_preallocs */						\
		(GInstanceInitFunc) backend_name##_init				\
	};									\
										\
	/* Initialise the i18n stuff */						\
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);			\
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");			\
                                                                                \
	g_define_type_id = g_type_module_register_type (module,		        \
					                EV_TYPE_DOCUMENT,	\
					                #BackendName,		\
					                &our_info,		\
					                (GTypeFlags)0);	        \
							                        \
	CODE									\
										\
	return g_define_type_id;						\
}

/*
 * Utility macro used to register backend
 *
 * use: EV_BACKEND_REGISTER(BackendName, backend_name)
 */
#define EV_BACKEND_REGISTER(BackendName, backend_name)			\
	EV_BACKEND_REGISTER_WITH_CODE(BackendName, backend_name, ;)

G_END_DECLS

#endif /* EV_DOCUMENT_H */
