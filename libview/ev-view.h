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

#if !defined (__EV_EVINCE_VIEW_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-view.h> can be included directly."
#endif

#ifndef __EV_VIEW_H__
#define __EV_VIEW_H__

#include <gtk/gtk.h>

#include <evince-document.h>

#include "ev-document-model.h"
#include "ev-jobs.h"

G_BEGIN_DECLS

#define EV_TYPE_VIEW            (ev_view_get_type ())
#define EV_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_VIEW, EvView))
#define EV_IS_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_VIEW))
#define EV_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_VIEW, EvViewClass))
#define EV_IS_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_VIEW))
#define EV_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_VIEW, EvViewClass))

typedef struct _EvView       EvView;
typedef struct _EvViewClass  EvViewClass;

GType		ev_view_get_type	    (void) G_GNUC_CONST;

GtkWidget*	ev_view_new		    (void);
void		ev_view_set_model	    (EvView          *view,
					     EvDocumentModel *model);
EV_DEPRECATED
void 		ev_view_set_loading         (EvView 	     *view,
					     gboolean         loading);
gboolean        ev_view_is_loading          (EvView          *view);
void            ev_view_reload              (EvView          *view);
void            ev_view_set_page_cache_size (EvView          *view,
					     gsize            cache_size);

void            ev_view_set_allow_links_change_zoom (EvView  *view,
                                                     gboolean allowed);
gboolean        ev_view_get_allow_links_change_zoom (EvView  *view);

/* Clipboard */
void		ev_view_copy		  (EvView         *view);
void            ev_view_copy_link_address (EvView         *view,
					   EvLinkAction   *action);
void		ev_view_select_all	  (EvView         *view);
gboolean        ev_view_get_has_selection (EvView         *view);

/* Page size */
gboolean	ev_view_can_zoom_in       (EvView         *view);
void		ev_view_zoom_in		  (EvView         *view);
gboolean        ev_view_can_zoom_out      (EvView         *view);
void		ev_view_zoom_out	  (EvView         *view);

/* Find */
void            ev_view_find_started              (EvView         *view,
						   EvJobFind      *job);
void            ev_view_find_restart              (EvView         *view,
                                                   gint            page);
void            ev_view_find_next                 (EvView         *view);
void            ev_view_find_previous             (EvView         *view);
void            ev_view_find_set_result           (EvView         *view,
						   gint            page,
						   gint            result);
void            ev_view_find_search_changed       (EvView         *view);
void     	ev_view_find_set_highlight_search (EvView         *view,
						   gboolean        value);
EV_DEPRECATED_FOR(ev_view_find_started)
void            ev_view_find_changed              (EvView         *view,
						   GList         **results,
						   gint            page);
void            ev_view_find_cancel               (EvView         *view);

/* Synctex */
void            ev_view_highlight_forward_search (EvView       *view,
						  EvSourceLink *link);

/* Cursor */
void           ev_view_hide_cursor        (EvView         *view);
void           ev_view_show_cursor        (EvView         *view);

/* Navigation */
EV_DEPRECATED_FOR(g_signal_emit_by_name)
void	       ev_view_scroll             (EvView         *view,
	                                   GtkScrollType   scroll,
					   gboolean        horizontal);
void	       ev_view_handle_link        (EvView         *view,
					   EvLink         *link);
gboolean       ev_view_next_page	  (EvView         *view);
gboolean       ev_view_previous_page	  (EvView         *view);

void	       ev_view_autoscroll_start   (EvView *view);
void           ev_view_autoscroll_stop    (EvView *view);

gboolean       ev_view_get_page_extents   (EvView       *view,
                                           gint          page,
                                           GdkRectangle *page_area,
                                           GtkBorder    *border);
/* Annotations */
void           ev_view_focus_annotation      (EvView          *view,
					      EvMapping       *annot_mapping);
void           ev_view_begin_add_annotation  (EvView          *view,
					      EvAnnotationType annot_type);
void           ev_view_cancel_add_annotation (EvView          *view);
void           ev_view_remove_annotation     (EvView          *view,
					      EvAnnotation    *annot);

/* Caret navigation */
gboolean       ev_view_supports_caret_navigation    (EvView  *view);
gboolean       ev_view_is_caret_navigation_enabled  (EvView  *view);
void           ev_view_set_caret_navigation_enabled (EvView  *view,
                                                     gboolean enabled);
void           ev_view_set_caret_cursor_position    (EvView  *view,
                                                     guint    page,
                                                     guint    offset);
G_END_DECLS

#endif /* __EV_VIEW_H__ */
