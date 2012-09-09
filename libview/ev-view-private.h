/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2004 Red Hat, Inc
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

#if !defined (EVINCE_COMPILATION)
#error "This is a private header."
#endif

#ifndef __EV_VIEW_PRIVATE_H__
#define __EV_VIEW_PRIVATE_H__

#include "ev-view.h"
#include "ev-document-model.h"
#include "ev-pixbuf-cache.h"
#include "ev-page-cache.h"
#include "ev-jobs.h"
#include "ev-image.h"
#include "ev-form-field.h"
#include "ev-selection.h"
#include "ev-view-cursor.h"

#define DRAG_HISTORY 10

/* Information for middle clicking and moving around the doc */
typedef struct {
        gboolean in_drag;
	GdkPoint start;
	gdouble hadj;
	gdouble vadj;
	guint drag_timeout_id;
	guint release_timeout_id;
	GdkPoint buffer[DRAG_HISTORY];
	GdkPoint momentum;
} DragInfo;

/* Autoscrolling */
typedef struct {
	gboolean autoscrolling;
	guint last_y;
	guint start_y;
	guint timeout_id;	
} AutoScrollInfo;

/* Information for handling selection */
typedef struct {
	gboolean in_drag;
	GdkPoint start;
	gboolean in_selection;
	GList *selections;
	EvSelectionStyle style;
} SelectionInfo;

/* Information for handling images DND */
typedef struct {
	gboolean in_drag;
	GdkPoint start;
	EvImage *image;
} ImageDNDInfo;

/* Annotation popup windows */
typedef struct {
	GtkWidget *window;
	guint      page;

	/* Current position */
	gint       x;
	gint       y;

	/* EvView root position */
	gint       parent_x;
	gint       parent_y;

	/* Document coords */
	gdouble    orig_x;
	gdouble    orig_y;

	gboolean   visible;
	gboolean   moved;
} EvViewWindowChild;

typedef enum {
	SCROLL_TO_KEEP_POSITION,
	SCROLL_TO_PAGE_POSITION,
	SCROLL_TO_CENTER,
	SCROLL_TO_FIND_LOCATION,
} PendingScroll;

typedef struct _EvHeightToPageCache {
	gint rotation;
	gboolean dual_even_left;
	gdouble *height_to_page;
	gdouble *dual_height_to_page;
} EvHeightToPageCache;

struct _EvView {
	GtkLayout layout;

	/* Container */
	GList *children;

	EvDocument *document;

	/* Find */
	EvJobFind *find_job;
	GList **find_pages; /* Backwards compatibility */
	gint find_result;
	gboolean jump_to_find_result;
	gboolean highlight_find_results;

	EvDocumentModel *model;
	EvPixbufCache *pixbuf_cache;
	gsize pixbuf_cache_size;
	EvPageCache *page_cache;
	EvHeightToPageCache *height_to_page_cache;
	EvViewCursor cursor;
	EvJobRender *current_job;

	GtkRequisition requisition;
	gboolean       internal_size_request;

	/* Scrolling */
	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;
	/* GtkScrollablePolicy needs to be checked when
	 * driving the scrollable adjustment values */
	guint hscroll_policy : 1;
	guint vscroll_policy : 1;

	gint scroll_x;
	gint scroll_y;	

	PendingScroll pending_scroll;
	gboolean      pending_resize;
	EvPoint       pending_point;

	/* Current geometry */
    
	gint start_page;
	gint end_page;
	gint current_page;

	gint rotation;
	gdouble scale;
	gint spacing;

	gboolean loading;
	gboolean continuous;
	gboolean dual_page;
	gboolean dual_even_left;
	gboolean fullscreen;
	EvSizingMode sizing_mode;
	GtkWidget *loading_window;
	guint loading_timeout;

	/* Common for button press handling */
	int pressed_button;

	/* Information for middle clicking and dragging around. */
	DragInfo drag_info;
	
	/* Autoscrolling */
	AutoScrollInfo scroll_info;

	/* Selection */
	GdkPoint motion;
	guint selection_update_id;
	guint selection_scroll_id;

	EvViewSelectionMode selection_mode;
	SelectionInfo selection_info;

	/* Copy link address selection */
	EvLinkAction *link_selected;

	/* Image DND */
	ImageDNDInfo image_dnd_info;

	/* Annotations */
	GList             *window_children;
	EvViewWindowChild *window_child_focus;
	EvMapping         *focus_annotation;
	gboolean           adding_annot;
	EvAnnotationType   adding_annot_type;

	/* Synctex */
	EvMapping *synctex_result;

	/* Accessibility */
	gboolean a11y_enabled;
};

struct _EvViewClass {
	GtkLayoutClass parent_class;

	void    (*binding_activated)	  (EvView         *view,
					   GtkScrollType   scroll,
					   gboolean        horizontal);
	void    (*handle_link)		  (EvView         *view,
					   EvLink         *link);
	void    (*external_link)	  (EvView         *view,
					   EvLinkAction   *action);
	void    (*popup_menu)		  (EvView         *view,
					   GList          *items);
	void    (*selection_changed)      (EvView         *view);
	void    (*sync_source)            (EvView         *view,
					   EvSourceLink   *link);
	void    (*annot_added)            (EvView         *view,
					   EvAnnotation   *annot);
	void    (*layers_changed)         (EvView         *view);
};

void _get_page_size_for_scale_and_rotation (EvDocument *document,
					    gint        page,
					    gdouble     scale,
					    gint        rotation,
					    gint       *page_width,
					    gint       *page_height);

#endif  /* __EV_VIEW_PRIVATE_H__ */

