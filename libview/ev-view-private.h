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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#if !defined (EVINCE_COMPILATION)
#error "This is a private header."
#endif

#ifndef __EV_VIEW_PRIVATE_H__
#define __EV_VIEW_PRIVATE_H__

#include "ev-view.h"
#include "ev-pixbuf-cache.h"
#include "ev-page-cache.h"
#include "ev-jobs.h"
#include "ev-image.h"
#include "ev-form-field.h"
#include "ev-selection.h"
#include "ev-transition-animation.h"

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

typedef enum {
	EV_VIEW_CURSOR_NORMAL,
	EV_VIEW_CURSOR_IBEAM,
	EV_VIEW_CURSOR_LINK,
	EV_VIEW_CURSOR_WAIT,
	EV_VIEW_CURSOR_HIDDEN,
	EV_VIEW_CURSOR_DRAG,
	EV_VIEW_CURSOR_AUTOSCROLL,
} EvViewCursor;

typedef enum {
	EV_PRESENTATION_NORMAL,
	EV_PRESENTATION_BLACK,
	EV_PRESENTATION_WHITE,
	EV_PRESENTATION_END
} EvPresentationState;

struct _EvView {
	GtkLayout layout;

	EvDocument *document;

	/* Find */
	GList **find_pages;
	gint find_result;
	gboolean jump_to_find_result;
	gboolean highlight_find_results;
	
	EvPageCache *page_cache;
	EvPixbufCache *pixbuf_cache;
	EvViewCursor cursor;
	EvJobRender *current_job;

	/* Scrolling */
	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;

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
	gdouble dpi;
	gdouble max_scale;
	gdouble min_scale;

	gboolean loading;
	gboolean continuous;
	gboolean dual_page;
	gboolean fullscreen;
	gboolean presentation;
	EvSizingMode sizing_mode;
	cairo_surface_t *loading_text;

	/* Presentation */
	EvPresentationState presentation_state;
	EvSizingMode sizing_mode_saved;
	double scale_saved;
	guint  trans_timeout_id;

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

	/* Goto Popup */
	GtkWidget *goto_window;
	GtkWidget *goto_entry;

	EvTransitionAnimation *animation;

	/* Annotations */
	GList             *window_children;
	EvViewWindowChild *window_child_focus;
};

struct _EvViewClass {
	GtkLayoutClass parent_class;

	void    (*binding_activated)	  (EvView         *view,
					   GtkScrollType   scroll,
					   gboolean        horizontal);
	void    (*zoom_invalid)		  (EvView         *view);
	void    (*handle_link)		  (EvView         *view,
					   EvLink         *link);
	void    (*external_link)	  (EvView         *view,
					   EvLinkAction   *action);
	void    (*popup_menu)		  (EvView         *view,
					   EvLink         *link);
};

#endif  /* __EV_VIEW_PRIVATE_H__ */

