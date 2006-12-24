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

#ifndef __EV_VIEW_PRIVATE_H__
#define __EV_VIEW_PRIVATE_H__

#include "ev-view.h"
#include "ev-pixbuf-cache.h"
#include "ev-page-cache.h"

/* Information for middle clicking and moving around the doc */
typedef struct {
        gboolean in_drag;
	GdkPoint start;
	gdouble hadj;
	gdouble vadj;
} DragInfo;

/* Information for handling selection */
typedef struct {
	gboolean in_drag;
	GdkPoint start;
	gboolean in_selection;
	GList *selections;
} SelectionInfo;

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
	EV_VIEW_CURSOR_DRAG
} EvViewCursor;

typedef enum {
	EV_PRESENTATION_NORMAL,
	EV_PRESENTATION_BLACK,
	EV_PRESENTATION_WHITE,
	EV_PRESENTATION_END
} EvPresentationState;

struct _EvView {
	GtkWidget parent_instance;

	EvDocument *document;

	char *status;
	char *find_status;
	int find_result;
	gboolean jump_to_find_result;
	
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

	int rotation;
	double scale;
	int spacing;

	gboolean loading;
	gboolean continuous;
	gboolean dual_page;
	gboolean fullscreen;
	gboolean presentation;
	EvSizingMode sizing_mode;

	/* Presentation */
	EvPresentationState presentation_state;
	EvSizingMode sizing_mode_saved;
	double scale_saved;

	/* Common for button press handling */
	int pressed_button;

	/* Information for middle clicking and dragging around. */
	DragInfo drag_info;

	/* Selection */
	GdkPoint motion;
	guint selection_update_id;
	guint selection_scroll_id;

	EvViewSelectionMode selection_mode;
	SelectionInfo selection_info;

	/* Links */
	GtkWidget *link_tooltip;
	EvLink *hovered_link;

	/* Goto Popup */
	GtkWidget *goto_window;
	GtkWidget *goto_entry;
};

struct _EvViewClass {
	GtkWidgetClass parent_class;

	void	(*set_scroll_adjustments) (EvView         *view,
					   GtkAdjustment  *hadjustment,
					   GtkAdjustment  *vadjustment);
	void    (*binding_activated)	  (EvView         *view,
					   EvScrollType   scroll,
					   gboolean        horizontal);
	void    (*zoom_invalid)		  (EvView         *view);
	void    (*external_link)	  (EvView         *view,
					   EvLinkAction   *action);
	void    (*popup_menu)		  (EvView         *view,
					   EvLink         *link);
};

#endif  /* __EV_VIEW_PRIVATE_H__ */

