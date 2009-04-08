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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include <evince-view.h>
 
#include <gtk/gtk.h>

#define EV_TYPE_PAGE_ACTION_WIDGET (ev_page_action_widget_get_type ())
#define EV_PAGE_ACTION_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_PAGE_ACTION_WIDGET, EvPageActionWidget))

typedef struct _EvPageActionWidget EvPageActionWidget;
typedef struct _EvPageActionWidgetClass EvPageActionWidgetClass;

struct _EvPageActionWidget
{
	GtkToolItem parent;

	GtkWidget *entry;
	GtkWidget *label;
	EvPageCache *page_cache;
	guint signal_id;
	GtkTreeModel *filter_model;
	GtkTreeModel *model;
};

struct _EvPageActionWidgetClass
{
	GtkToolItemClass parent_class;

	void (* activate_link) (EvPageActionWidget *page_action,
			        EvLink             *link);
};

GType ev_page_action_widget_get_type   (void);

void
ev_page_action_widget_update_model (EvPageActionWidget *proxy, GtkTreeModel *model);

void
ev_page_action_widget_set_page_cache (EvPageActionWidget *action_widget,
				      EvPageCache        *page_cache);
