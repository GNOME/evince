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

G_BEGIN_DECLS

#define EV_TYPE_VIEW            (ev_view_get_type ())
#define EV_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_VIEW, EvView))
#define EV_IS_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_VIEW))


typedef struct _EvView       EvView;
typedef struct _EvViewClass  EvViewClass;

GType	   ev_view_get_type (void) G_GNUC_CONST;
GtkWidget* ev_view_new      (void);

void ev_view_set_document (EvView     *view,
			   EvDocument *document);
void ev_view_set_page     (EvView     *view,
			   int         page);
int  ev_view_get_page     (EvView     *view);

void ev_view_zoom_in     (EvView *view);
void ev_view_zoom_out    (EvView *view);
void ev_view_normal_size (EvView *view);
void ev_view_best_fit    (EvView *view);
void ev_view_fit_width   (EvView *view);

G_END_DECLS

#endif /* __EV_VIEW_H__ */
