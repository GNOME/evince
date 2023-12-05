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

#pragma once

#if !defined (__EV_EVINCE_VIEW_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-view.h> can be included directly."
#endif

#include <glib-object.h>
#include <evince-document.h>

G_BEGIN_DECLS

#define EV_TYPE_DOCUMENT_MODEL (ev_document_model_get_type ())

EV_PUBLIC
G_DECLARE_FINAL_TYPE(EvDocumentModel, ev_document_model, EV, DOCUMENT_MODEL, GObject)

/**
 * EvSizingMode:
 * @EV_SIZING_FIT_PAGE: Since: 3.8
 * @EV_SIZING_FIT_WIDTH:
 * @EV_SIZING_FREE:
 * @EV_SIZING_AUTOMATIC: Since: 3.8
 */
typedef enum {
        EV_SIZING_FIT_PAGE,
	EV_SIZING_FIT_WIDTH,
	EV_SIZING_FREE,
        EV_SIZING_AUTOMATIC
} EvSizingMode;

typedef enum {
	EV_PAGE_LAYOUT_SINGLE,
	EV_PAGE_LAYOUT_DUAL,
	EV_PAGE_LAYOUT_AUTOMATIC
} EvPageLayout;

EV_PUBLIC
EvDocumentModel *ev_document_model_new               (void);
EV_PUBLIC
EvDocumentModel *ev_document_model_new_with_document (EvDocument      *document);

EV_PUBLIC
void             ev_document_model_set_document      (EvDocumentModel *model,
						      EvDocument      *document);
EV_PUBLIC
EvDocument      *ev_document_model_get_document      (EvDocumentModel *model);
EV_PUBLIC
void             ev_document_model_set_page          (EvDocumentModel *model,
						      gint             page);
EV_PUBLIC
void             ev_document_model_set_page_by_label (EvDocumentModel *model,
						      const gchar     *page_label);
EV_PUBLIC
gint             ev_document_model_get_page          (EvDocumentModel *model);
EV_PUBLIC
void             ev_document_model_set_scale         (EvDocumentModel *model,
						      gdouble          scale);
EV_PUBLIC
gdouble          ev_document_model_get_scale         (EvDocumentModel *model);
EV_PUBLIC
void             ev_document_model_set_max_scale     (EvDocumentModel *model,
						      gdouble          max_scale);
EV_PUBLIC
gdouble          ev_document_model_get_max_scale     (EvDocumentModel *model);
EV_PUBLIC
void             ev_document_model_set_min_scale     (EvDocumentModel *model,
						      gdouble          min_scale);
EV_PUBLIC
gdouble          ev_document_model_get_min_scale     (EvDocumentModel *model);
EV_PUBLIC
void             ev_document_model_set_sizing_mode   (EvDocumentModel *model,
						      EvSizingMode     mode);
EV_PUBLIC
EvSizingMode     ev_document_model_get_sizing_mode   (EvDocumentModel *model);
EV_PUBLIC
void             ev_document_model_set_page_layout   (EvDocumentModel *model,
						     EvPageLayout     layout);
EV_PUBLIC
EvPageLayout	 ev_document_model_get_page_layout   (EvDocumentModel *model);
EV_PUBLIC
void             ev_document_model_set_rotation      (EvDocumentModel *model,
						      gint             rotation);
EV_PUBLIC
gint             ev_document_model_get_rotation      (EvDocumentModel *model);
EV_PUBLIC
void           ev_document_model_set_inverted_colors (EvDocumentModel *model,
						      gboolean         inverted_colors);
EV_PUBLIC
gboolean       ev_document_model_get_inverted_colors (EvDocumentModel *model);
EV_PUBLIC
void             ev_document_model_set_continuous    (EvDocumentModel *model,
						      gboolean         continuous);
EV_PUBLIC
gboolean         ev_document_model_get_continuous    (EvDocumentModel *model);
EV_PUBLIC
void             ev_document_model_set_dual_page_odd_pages_left (EvDocumentModel *model,
								 gboolean         odd_left);
EV_PUBLIC
gboolean         ev_document_model_get_dual_page_odd_pages_left (EvDocumentModel *model);
EV_PUBLIC
void             ev_document_model_set_rtl (EvDocumentModel *model,
                                            gboolean         rtl);
EV_PUBLIC
gboolean         ev_document_model_get_rtl (EvDocumentModel *model);

G_END_DECLS
