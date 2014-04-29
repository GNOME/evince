/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2009 Carlos Garcia Campos
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

#ifndef __EV_DOCUMENT_MODEL_H__
#define __EV_DOCUMENT_MODEL_H__

#include <glib-object.h>
#include <evince-document.h>

G_BEGIN_DECLS

#define EV_TYPE_DOCUMENT_MODEL            (ev_document_model_get_type ())
#define EV_DOCUMENT_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_DOCUMENT_MODEL, EvDocumentModel))
#define EV_IS_DOCUMENT_MODEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_DOCUMENT_MODEL))
#define EV_DOCUMENT_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_DOCUMENT_MODEL, EvDocumentModelClass))
#define EV_IS_DOCUMENT_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_DOCUMENT_MODEL))
#define EV_DOCUMENT_MODEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_DOCUMENT_MODEL, EvDocumentModelClass))

/**
 * EvSizingMode:
 * @EV_SIZING_FIT_PAGE: Since: 3.8
 * @EV_SIZING_BEST_FIT: Same as %EV_SIZING_FIT_PAGE. Deprecated:
 * @EV_SIZING_FIT_WIDTH:
 * @EV_SIZING_FREE:
 * @EV_SIZING_AUTOMATIC: Since: 3.8
 */
typedef enum {
        EV_SIZING_FIT_PAGE,
	EV_SIZING_BEST_FIT = EV_SIZING_FIT_PAGE, /* Deprecated */
	EV_SIZING_FIT_WIDTH,
	EV_SIZING_FREE,
        EV_SIZING_AUTOMATIC
} EvSizingMode;

typedef enum {
	EV_PAGE_LAYOUT_SINGLE,
	EV_PAGE_LAYOUT_DUAL,
	EV_PAGE_LAYOUT_AUTOMATIC
} EvPageLayout;

typedef struct _EvDocumentModel        EvDocumentModel;
typedef struct _EvDocumentModelClass   EvDocumentModelClass;

GType            ev_document_model_get_type          (void) G_GNUC_CONST;
EvDocumentModel *ev_document_model_new               (void);
EvDocumentModel *ev_document_model_new_with_document (EvDocument      *document);

void             ev_document_model_set_document      (EvDocumentModel *model,
						      EvDocument      *document);
EvDocument      *ev_document_model_get_document      (EvDocumentModel *model);
void             ev_document_model_set_page          (EvDocumentModel *model,
						      gint             page);
void             ev_document_model_set_page_by_label (EvDocumentModel *model,
						      const gchar     *page_label);
gint             ev_document_model_get_page          (EvDocumentModel *model);
void             ev_document_model_set_scale         (EvDocumentModel *model,
						      gdouble          scale);
gdouble          ev_document_model_get_scale         (EvDocumentModel *model);
void             ev_document_model_set_max_scale     (EvDocumentModel *model,
						      gdouble          max_scale);
gdouble          ev_document_model_get_max_scale     (EvDocumentModel *model);
void             ev_document_model_set_min_scale     (EvDocumentModel *model,
						      gdouble          min_scale);
gdouble          ev_document_model_get_min_scale     (EvDocumentModel *model);
void             ev_document_model_set_sizing_mode   (EvDocumentModel *model,
						      EvSizingMode     mode);
EvSizingMode     ev_document_model_get_sizing_mode   (EvDocumentModel *model);
void             ev_document_model_set_page_layout   (EvDocumentModel *model,
						     EvPageLayout     layout);
EvPageLayout	 ev_document_model_get_page_layout   (EvDocumentModel *model);
void             ev_document_model_set_rotation      (EvDocumentModel *model,
						      gint             rotation);
gint             ev_document_model_get_rotation      (EvDocumentModel *model);
void           ev_document_model_set_inverted_colors (EvDocumentModel *model,
						      gboolean         inverted_colors);
gboolean       ev_document_model_get_inverted_colors (EvDocumentModel *model);
void             ev_document_model_set_continuous    (EvDocumentModel *model,
						      gboolean         continuous);
gboolean         ev_document_model_get_continuous    (EvDocumentModel *model);
void             ev_document_model_set_dual_page_odd_pages_left (EvDocumentModel *model,
								 gboolean         odd_left);
gboolean         ev_document_model_get_dual_page_odd_pages_left (EvDocumentModel *model);
void             ev_document_model_set_fullscreen    (EvDocumentModel *model,
						      gboolean         fullscreen);
gboolean         ev_document_model_get_fullscreen    (EvDocumentModel *model);

/* deprecated */

EV_DEPRECATED_FOR(ev_document_model_set_page_layout)
void             ev_document_model_set_dual_page     (EvDocumentModel *model,
						      gboolean         dual_page);
EV_DEPRECATED_FOR(ev_document_model_get_page_layout)
gboolean         ev_document_model_get_dual_page     (EvDocumentModel *model);

G_END_DECLS

#endif /* __EV_DOCUMENT_MODEL_H__ */
