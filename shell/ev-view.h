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

#ifndef __EV_VIEW_H__
#define __EV_VIEW_H__

#include <gtk/gtkwidget.h>

#include "ev-document.h"
#include "ev-link.h"
#include "ev-view-accessible.h"

G_BEGIN_DECLS

#define EV_TYPE_VIEW            (ev_view_get_type ())
#define EV_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_VIEW, EvView))
#define EV_IS_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_VIEW))

#define EV_TYPE_SIZING_MODE     (ev_sizing_mode_get_type())
#define EV_SIZING_MODE_CLASS    (g_type_class_peek (EV_TYPE_SIZING_MODE))

#define EV_TYPE_SCROLL_TYPE     (ev_scroll_type_get_type())
#define EV_SCROLL_TYPE_CLASS    (g_type_class_peek (EV_TYPE_SCROLL_TYPE))

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

typedef enum {
	EV_SCROLL_PAGE_FORWARD,
	EV_SCROLL_PAGE_BACKWARD,
	EV_SCROLL_STEP_BACKWARD,
	EV_SCROLL_STEP_FORWARD,
	EV_SCROLL_STEP_DOWN,
	EV_SCROLL_STEP_UP,
} EvScrollType;

GType		ev_view_get_type	  (void) G_GNUC_CONST;
GType           ev_sizing_mode_get_type   (void) G_GNUC_CONST;
GType           ev_scroll_type_get_type   (void) G_GNUC_CONST;

GtkWidget*	ev_view_new		  (void);
void		ev_view_set_document	  (EvView         *view,
			   		   EvDocument     *document);
void 		ev_view_set_loading       (EvView 	  *view,
				           gboolean        loading);

/* Clipboard */
void		ev_view_copy		  (EvView         *view);
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
double		ev_view_get_zoom	  (EvView         *view);
void		ev_view_set_zoom_for_size (EvView         *view,
					   int             width,
		    			   int             height,
					   int             vsb_width,
					   int             hsb_height);
void            ev_view_set_screen_dpi    (EvView         *view,
					   gdouble         dpi);
void		ev_view_rotate_left       (EvView         *view);
void            ev_view_rotate_right      (EvView         *view);
void            ev_view_set_rotation      (EvView         *view,
					   int             rotation);
int             ev_view_get_rotation      (EvView         *view);

/* Find */
gboolean        ev_view_can_find_next     (EvView         *view);
void            ev_view_find_next         (EvView         *view);
gboolean        ev_view_can_find_previous (EvView         *view);
void            ev_view_find_previous     (EvView         *view);
void            ev_view_search_changed    (EvView         *view);

/* Status */
const char     *ev_view_get_status        (EvView         *view);
const char     *ev_view_get_find_status   (EvView         *view);

/* Cursor */
void           ev_view_hide_cursor        (EvView         *view);
void           ev_view_show_cursor        (EvView         *view);

/* Navigation */
void	       ev_view_scroll             (EvView         *view,
	                                   EvScrollType    scroll,
					   gboolean        horizontal);
void	       ev_view_handle_link        (EvView         *view,
					   EvLink         *link);
gboolean       ev_view_next_page	  (EvView         *view);
gboolean       ev_view_previous_page	  (EvView         *view);

G_END_DECLS

#endif /* __EV_VIEW_H__ */
