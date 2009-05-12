/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/*
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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
#include <gdk/gdk.h>
#include <cairo.h>

#include "ev-document-info.h"
#include "ev-page.h"
#include "ev-render-context.h"

G_BEGIN_DECLS

#define EV_TYPE_DOCUMENT            (ev_document_get_type ())
#define EV_DOCUMENT(o)              (G_TYPE_CHECK_INSTANCE_CAST ((o), EV_TYPE_DOCUMENT, EvDocument))
#define EV_DOCUMENT_IFACE(k)        (G_TYPE_CHECK_CLASS_CAST((k), EV_TYPE_DOCUMENT, EvDocumentIface))
#define EV_IS_DOCUMENT(o)           (G_TYPE_CHECK_INSTANCE_TYPE ((o), EV_TYPE_DOCUMENT))
#define EV_IS_DOCUMENT_IFACE(k)     (G_TYPE_CHECK_CLASS_TYPE ((k), EV_TYPE_DOCUMENT))
#define EV_DOCUMENT_GET_IFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EV_TYPE_DOCUMENT, EvDocumentIface))

typedef struct _EvDocument        EvDocument;
typedef struct _EvDocumentIface   EvDocumentIface;
typedef struct _EvPageCache       EvPageCache;
typedef struct _EvPageCacheClass  EvPageCacheClass;

#define EV_DOCUMENT_ERROR ev_document_error_quark ()
#define EV_DOC_MUTEX_LOCK (ev_document_doc_mutex_lock ())
#define EV_DOC_MUTEX_UNLOCK (ev_document_doc_mutex_unlock ())

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

struct _EvDocumentIface
{
        GTypeInterface base_iface;

        /* Methods  */
        gboolean          (* load)            (EvDocument      *document,
                                               const char      *uri,
                                               GError         **error);
        gboolean          (* save)            (EvDocument      *document,
                                               const char      *uri,
                                               GError         **error);
        int               (* get_n_pages)     (EvDocument      *document);
	EvPage          * (* get_page)        (EvDocument      *document,
					       gint             index);
        void              (* get_page_size)   (EvDocument      *document,
                                               EvPage          *page,
                                               double          *width,
                                               double          *height);
        char            * (* get_page_label)  (EvDocument      *document,
                                               EvPage          *page);
        gboolean          (* has_attachments) (EvDocument      *document);
        GList           * (* get_attachments) (EvDocument      *document);
        cairo_surface_t * (* render)          (EvDocument      *document,
                                               EvRenderContext *rc);
        EvDocumentInfo *  (* get_info)        (EvDocument      *document);
};

GType            ev_document_get_type         (void) G_GNUC_CONST;
GQuark           ev_document_error_quark      (void);

/* Document mutex */
GMutex          *ev_document_get_doc_mutex    (void);
void             ev_document_doc_mutex_lock   (void);
void             ev_document_doc_mutex_unlock (void);
gboolean         ev_document_doc_mutex_trylock(void);

/* FontConfig mutex */
GMutex          *ev_document_get_fc_mutex     (void);
void             ev_document_fc_mutex_lock    (void);
void             ev_document_fc_mutex_unlock  (void);
gboolean         ev_document_fc_mutex_trylock (void);

EvDocumentInfo  *ev_document_get_info         (EvDocument      *document);
gboolean         ev_document_load             (EvDocument      *document,
                                               const char      *uri,
                                               GError         **error);
gboolean         ev_document_save             (EvDocument      *document,
                                               const char      *uri,
                                               GError         **error);
int              ev_document_get_n_pages      (EvDocument      *document);
EvPage          *ev_document_get_page         (EvDocument      *document,
					       gint             index);
void             ev_document_get_page_size    (EvDocument      *document,
                                               EvPage          *page,
                                               double          *width,
                                               double          *height);
char            *ev_document_get_page_label   (EvDocument      *document,
                                               EvPage          *page);
gboolean         ev_document_has_attachments  (EvDocument      *document);
GList           *ev_document_get_attachments  (EvDocument      *document);
cairo_surface_t *ev_document_render           (EvDocument      *document,
                                               EvRenderContext *rc);

gint            ev_rect_cmp                   (EvRectangle     *a,
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
	const GTypeInfo our_info = {  				        \
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
					    G_TYPE_OBJECT,			\
					    #BackendName,			\
					    &our_info,				\
					   (GTypeFlags)0);	                \
							                        \
	EV_BACKEND_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT,                       \
                               backend_name##_document_iface_init);             \
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

/*
 * A convenience macro for boxed type implementations, which defines a
 * type_name_get_type() function registering the boxed type.
 */
#define EV_DEFINE_BOXED_TYPE(TypeName, type_name, copy_func, free_func)               \
GType                                                                                 \
type_name##_get_type (void)                                                           \
{                                                                                     \
        static volatile gsize g_define_type_id__volatile = 0;                         \
	if (g_once_init_enter (&g_define_type_id__volatile)) {                        \
	        GType g_define_type_id =                                              \
		    g_boxed_type_register_static (g_intern_static_string (#TypeName), \
		                                  (GBoxedCopyFunc) copy_func,         \
						  (GBoxedFreeFunc) free_func);        \
		g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);    \
	}                                                                             \
	return g_define_type_id__volatile;                                            \
}

/* A convenience macro for GTypeInterface definitions, which declares
 * a default vtable initialization function and defines a *_get_type()
 * function.
 *
 * The macro expects the interface initialization function to have the
 * name <literal>t_n ## _default_init</literal>, and the interface
 * structure to have the name <literal>TN ## Interface</literal>.
 */
#define EV_DEFINE_INTERFACE(TypeName, type_name, TYPE_PREREQ)	                             \
static void     type_name##_class_init        (TypeName##Iface *klass);                      \
                                                                                             \
GType                                                                                        \
type_name##_get_type (void)                                                                  \
{                                                                                            \
        static volatile gsize g_define_type_id__volatile = 0;                                \
	if (g_once_init_enter (&g_define_type_id__volatile)) {                               \
		GType g_define_type_id =                                                     \
		    g_type_register_static_simple (G_TYPE_INTERFACE,                         \
		                                   g_intern_static_string (#TypeName),       \
		                                   sizeof (TypeName##Iface),                 \
		                                   (GClassInitFunc)type_name##_class_init,   \
		                                   0,                                        \
		                                   (GInstanceInitFunc)NULL,                  \
						   (GTypeFlags) 0);                          \
		if (TYPE_PREREQ)                                                             \
			g_type_interface_add_prerequisite (g_define_type_id, TYPE_PREREQ);   \
		g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);           \
	}                                                                                    \
	return g_define_type_id__volatile;                                                   \
}

/*
 * A convenience macro for boxed type implementations, which defines a
 * type_name_get_type() function registering the boxed type.
 */
#define EV_DEFINE_BOXED_TYPE(TypeName, type_name, copy_func, free_func)               \
GType                                                                                 \
type_name##_get_type (void)                                                           \
{                                                                                     \
        static volatile gsize g_define_type_id__volatile = 0;                         \
	if (g_once_init_enter (&g_define_type_id__volatile)) {                        \
	        GType g_define_type_id =                                              \
		    g_boxed_type_register_static (g_intern_static_string (#TypeName), \
		                                  (GBoxedCopyFunc) copy_func,         \
						  (GBoxedFreeFunc) free_func);        \
		g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);    \
	}                                                                             \
	return g_define_type_id__volatile;                                            \
}
		
G_END_DECLS

#endif /* EV_DOCUMENT_H */
