/*
 *  Copyright (C) 2004 Jonathan Blandforde
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef EV_PAGE_VIEW_H
#define EV_PAGE_VIEW_H

#include <gtk/gtk.h>
#include "ev-document.h"

G_BEGIN_DECLS

#define EV_TYPE_PAGE_VIEW            (ev_page_view_get_type ())
#define EV_PAGE_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_PAGE_VIEW, EvPageView))
#define EV_PAGE_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_PAGE_VIEW, EvPageViewClass))
#define EV_IS_PAGE_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_PAGE_VIEW))
#define EV_IS_PAGE_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EV_TYPE_PAGE_VIEW))
#define EV_PAGE_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EV_TYPE_PAGE_VIEW, EvPageViewClass))

typedef struct _EvPageView		EvPageView;
typedef struct _EvPageViewPrivate	EvPageViewPrivate;
typedef struct _EvPageViewClass		EvPageViewClass;

struct _EvPageView
{
	GtkWidget parent;
	
	/*< private >*/
	EvPageViewPrivate *priv;
};

struct _EvPageViewClass
{
	GtkWidgetClass parent_class;

	void (* set_scroll_adjustments) (EvPageView    *page_view,
					 GtkAdjustment *hadjustment,
					 GtkAdjustment *vadjustment);
};

GType      ev_page_view_get_type     (void);
GtkWidget *ev_page_view_new          (void);
void       ev_page_view_set_document (EvPageView *page_view,
				      EvDocument *document);


G_END_DECLS

#endif
