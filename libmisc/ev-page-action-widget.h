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

#pragma once

#include <evince-view.h>
 
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EV_TYPE_PAGE_ACTION_WIDGET (ev_page_action_widget_get_type ())
#define EV_PAGE_ACTION_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_PAGE_ACTION_WIDGET, EvPageActionWidget))

typedef struct _EvPageActionWidget EvPageActionWidget;
typedef struct _EvPageActionWidgetClass EvPageActionWidgetClass;

struct _EvPageActionWidgetClass
{
	GtkToolItemClass parent_class;

	void (* activate_link) (EvPageActionWidget *page_action,
			        EvLink             *link);
};

GType ev_page_action_widget_get_type          (void) G_GNUC_CONST;

void ev_page_action_widget_update_links_model (EvPageActionWidget *proxy,
					       GtkTreeModel       *model);

void ev_page_action_widget_set_model          (EvPageActionWidget *action_widget,
					       EvDocumentModel    *doc_model);
void ev_page_action_widget_grab_focus         (EvPageActionWidget *proxy);
void ev_page_action_widget_clear              (EvPageActionWidget *proxy);
void ev_page_action_widget_set_temporary_entry_width (EvPageActionWidget *proxy, gint width);

G_END_DECLS
