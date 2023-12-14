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

#pragma once

#if !defined (__EV_EVINCE_VIEW_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-view.h> can be included directly."
#endif

#include <gtk/gtk.h>

#include <evince-document.h>

#include "ev-document-model.h"
#include "ev-jobs.h"

G_BEGIN_DECLS

#define EV_TYPE_VIEW            (ev_view_get_type ())
EV_PUBLIC
G_DECLARE_DERIVABLE_TYPE (EvView, ev_view, EV, VIEW, GtkWidget)

EV_PUBLIC
EvView *	ev_view_new		    (void);
EV_PUBLIC
void		ev_view_set_model	    (EvView          *view,
					     EvDocumentModel *model);
EV_PUBLIC
gboolean        ev_view_is_loading          (EvView          *view);
EV_PUBLIC
void            ev_view_reload              (EvView          *view);
EV_PUBLIC
void            ev_view_set_page_cache_size (EvView          *view,
					     gsize            cache_size);

EV_PUBLIC
void            ev_view_set_allow_links_change_zoom (EvView  *view,
                                                     gboolean allowed);
EV_PUBLIC
gboolean        ev_view_get_allow_links_change_zoom (EvView  *view);

/* Clipboard */
EV_PUBLIC
void		ev_view_copy		  (EvView         *view);
EV_PUBLIC
void            ev_view_copy_link_address (EvView         *view,
					   EvLinkAction   *action);
EV_PUBLIC
void		ev_view_select_all	  (EvView         *view);
EV_PUBLIC
gboolean        ev_view_has_selection	  (EvView         *view);
EV_PUBLIC
char *   ev_view_get_selected_text (EvView  *view);

/* Page size */
EV_PUBLIC
gboolean	ev_view_can_zoom_in       (EvView         *view);
EV_PUBLIC
void		ev_view_zoom_in		  (EvView         *view);
EV_PUBLIC
gboolean        ev_view_can_zoom_out      (EvView         *view);
EV_PUBLIC
void		ev_view_zoom_out	  (EvView         *view);

/* Find */
EV_PUBLIC
void            ev_view_find_started              (EvView         *view,
						   EvJobFind      *job);
EV_PUBLIC
void            ev_view_find_restart              (EvView         *view,
                                                   gint            page);
EV_PUBLIC
void            ev_view_find_next                 (EvView         *view);
EV_PUBLIC
void            ev_view_find_previous             (EvView         *view);
EV_PUBLIC
void            ev_view_find_set_result           (EvView         *view,
						   gint            page,
						   gint            result);
EV_PUBLIC
void            ev_view_find_search_changed       (EvView         *view);
EV_PUBLIC
void     	ev_view_find_set_highlight_search (EvView         *view,
						   gboolean        value);
EV_PUBLIC
void            ev_view_find_cancel               (EvView         *view);

/* Synctex */
EV_PUBLIC
void            ev_view_highlight_forward_search (EvView       *view,
						  EvSourceLink *link);

/* Cursor */
EV_PUBLIC
void           ev_view_hide_cursor        (EvView         *view);
EV_PUBLIC
void           ev_view_show_cursor        (EvView         *view);

/* Navigation */
EV_PUBLIC
void	       ev_view_handle_link        (EvView         *view,
					   EvLink         *link);
EV_PUBLIC
gboolean       ev_view_next_page	  (EvView         *view);
EV_PUBLIC
gboolean       ev_view_previous_page	  (EvView         *view);

EV_PUBLIC
void	       ev_view_autoscroll_start   (EvView *view);
EV_PUBLIC
void           ev_view_autoscroll_stop    (EvView *view);

EV_PUBLIC
gboolean       ev_view_get_page_extents   (EvView       *view,
                                           gint          page,
                                           GdkRectangle *page_area,
                                           GtkBorder    *border);
EV_PUBLIC
gboolean       ev_view_get_page_extents_for_border (EvView       *view,
                                                    gint          page,
                                                    GtkBorder    *border,
                                                    GdkRectangle *page_area);

/* Annotations */
EV_PUBLIC
void           ev_view_focus_annotation      (EvView          *view,
					      EvMapping       *annot_mapping);
EV_PUBLIC
void           ev_view_begin_add_annotation  (EvView          *view,
					      EvAnnotationType annot_type);
EV_PUBLIC
void           ev_view_cancel_add_annotation (EvView          *view);
EV_PUBLIC
void           ev_view_remove_annotation     (EvView          *view,
					      EvAnnotation    *annot);
EV_PUBLIC
gboolean       ev_view_add_text_markup_annotation_for_selected_text (EvView  *view);
EV_PUBLIC
void           ev_view_set_enable_spellchecking (EvView *view,
                                                 gboolean spellcheck);
EV_PUBLIC
gboolean       ev_view_get_enable_spellchecking (EvView *view);

/* Caret navigation */
EV_PUBLIC
gboolean       ev_view_supports_caret_navigation    (EvView  *view);
EV_PUBLIC
gboolean       ev_view_is_caret_navigation_enabled  (EvView  *view);
EV_PUBLIC
void           ev_view_set_caret_navigation_enabled (EvView  *view,
                                                     gboolean enabled);
EV_PUBLIC
void           ev_view_set_caret_cursor_position    (EvView  *view,
                                                     guint    page,
                                                     guint    offset);
EV_PUBLIC
gboolean       ev_view_current_event_is_type        (EvView *view,
						     GdkEventType type);
G_END_DECLS
