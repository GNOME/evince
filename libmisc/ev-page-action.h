/*
 *  Copyright (C) 2003, 2004 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004 Christian Persch
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef EV_PAGE_ACTION_H
#define EV_PAGE_ACTION_H

#include <gtk/gtk.h>

#include <evince-document.h>
#include <evince-view.h>

G_BEGIN_DECLS

#define EV_TYPE_PAGE_ACTION            (ev_page_action_get_type ())
#define EV_PAGE_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_PAGE_ACTION, EvPageAction))
#define EV_PAGE_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_PAGE_ACTION, EvPageActionClass))
#define EV_IS_PAGE_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_PAGE_ACTION))
#define EV_IS_PAGE_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EV_TYPE_PAGE_ACTION))
#define EV_PAGE_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EV_TYPE_PAGE_ACTION, EvPageActionClass))

typedef struct _EvPageAction		EvPageAction;
typedef struct _EvPageActionPrivate	EvPageActionPrivate;
typedef struct _EvPageActionClass		EvPageActionClass;

struct _EvPageAction
{
	GtkAction parent;
	
	/*< private >*/
	EvPageActionPrivate *priv;
};

struct _EvPageActionClass
{
	GtkActionClass parent_class;

	void     (* activate_link) (EvPageAction *page_action,
			            EvLink       *link);
};

GType ev_page_action_get_type        (void) G_GNUC_CONST;

void  ev_page_action_set_model       (EvPageAction    *page_action,
				      EvDocumentModel *model);
void  ev_page_action_set_links_model (EvPageAction    *page_action,
				      GtkTreeModel    *links_model);
void  ev_page_action_grab_focus      (EvPageAction    *page_action);

G_END_DECLS

#endif
