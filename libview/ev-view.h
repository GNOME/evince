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

#if !defined (__EV_EVINCE_VIEW_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-view.h> can be included directly."
#endif

#ifndef __EV_VIEW_H__
#define __EV_VIEW_H__

#include <gtk/gtk.h>

#include <evince-document.h>

G_BEGIN_DECLS

#define EV_TYPE_VIEW            (ev_view_get_type ())
#define EV_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_VIEW, EvView))
#define EV_IS_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_VIEW))

typedef struct _EvView       EvView;
typedef struct _EvViewClass  EvViewClass;


typedef enum {
	EV_SIZING_BEST_FIT,
	EV_SIZING_FIT_WIDTH,
	EV_SIZING_FREE,
} EvSizingMode;

typedef enum {
	EV_VIEW_SELECTION_TEXT,
	EV_VIEW_SELECTION_RECTANGLE,
} EvViewSelectionMode;

GType		ev_view_get_type	  (void) G_GNUC_CONST;

GtkWidget*	ev_view_new		  (void);
void		ev_view_set_document	  (EvView         *view,
			   		   EvDocument     *document);
void 		ev_view_set_loading       (EvView 	  *view,
				           gboolean        loading);
void            ev_view_reload            (EvView         *view);

/* Clipboard */
void		ev_view_copy		  (EvView         *view);
void            ev_view_copy_link_address (EvView         *view,
					   EvLinkAction   *action);
void		ev_view_select_all	  (EvView         *view);
gboolean        ev_view_get_has_selection (EvView         *view);

/* sizing and behavior */
/* These are all orthoganal to each other, except 'presentation' trumps all
 * other behaviors
 */
gboolean	ev_view_get_continuous	  (EvView         *view);
void     	ev_view_set_continuous    (EvView         *view,
					   gboolean        continuous);
gboolean	ev_view_get_dual_page	  (EvView         *view);
void     	ev_view_set_dual_page	  (EvView         *view,
					   gboolean        dual_page);
void     	ev_view_set_fullscreen	  (EvView         *view,
					   gboolean        fullscreen);
gboolean 	ev_view_get_fullscreen	  (EvView         *view);
void     	ev_view_set_presentation  (EvView         *view,
					   gboolean        presentation);
gboolean 	ev_view_get_presentation  (EvView         *view);
void     	ev_view_set_sizing_mode	  (EvView         *view,
					   EvSizingMode    mode);
EvSizingMode	ev_view_get_sizing_mode	  (EvView         *view);


/* Page size */
gboolean	ev_view_can_zoom_in       (EvView         *view);
void		ev_view_zoom_in		  (EvView         *view);
gboolean        ev_view_can_zoom_out      (EvView         *view);
void		ev_view_zoom_out	  (EvView         *view);
void		ev_view_set_zoom	  (EvView         *view,
					   double          factor,
					   gboolean        relative);
void		ev_view_set_zoom_for_size (EvView         *view,
					   int             width,
					   int             height,
					   int             vsb_width,
					   int             hsb_height);
double		ev_view_get_zoom	  (EvView         *view);
void            ev_view_set_screen_dpi    (EvView         *view,
					   gdouble         dpi);
void		ev_view_rotate_left       (EvView         *view);
void            ev_view_rotate_right      (EvView         *view);
void            ev_view_set_rotation      (EvView         *view,
					   int             rotation);
int             ev_view_get_rotation      (EvView         *view);

/* Find */
void            ev_view_find_next                 (EvView         *view);
void            ev_view_find_previous             (EvView         *view);
void            ev_view_find_search_changed       (EvView         *view);
void     	ev_view_find_set_highlight_search (EvView         *view,
						   gboolean        value);
void            ev_view_find_changed              (EvView         *view,
						   GList         **results,
						   gint            page);
void            ev_view_find_cancel               (EvView         *view);

/* Cursor */
void           ev_view_hide_cursor        (EvView         *view);
void           ev_view_show_cursor        (EvView         *view);

/* Navigation */
void	       ev_view_scroll             (EvView         *view,
	                                   GtkScrollType   scroll,
					   gboolean        horizontal);
void	       ev_view_handle_link        (EvView         *view,
					   EvLink         *link);
gboolean       ev_view_next_page	  (EvView         *view);
gboolean       ev_view_previous_page	  (EvView         *view);
gchar*         ev_view_page_label_from_dest (EvView *view, EvLinkDest *dest);

void	       ev_view_autoscroll_start   (EvView *view);
void           ev_view_autoscroll_stop    (EvView *view);

G_END_DECLS

#endif /* __EV_VIEW_H__ */
