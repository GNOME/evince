/* ev-zoom-action-widget.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2012 Carlos Garcia Campos  <carlosgc@gnome.org>
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

#ifndef EV_ZOOM_ACTION_WIDGET_H
#define EV_ZOOM_ACTION_WIDGET_H

#include <gtk/gtk.h>
#include <evince-document.h>
#include <evince-view.h>

#include "ev-window.h"

G_BEGIN_DECLS

#define EV_TYPE_ZOOM_ACTION_WIDGET            (ev_zoom_action_widget_get_type())
#define EV_ZOOM_ACTION_WIDGET(object)         (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_ZOOM_ACTION_WIDGET, EvZoomActionWidget))
#define EV_IS_ZOOM_ACTION_WIDGET(object)      (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_ZOOM_ACTION_WIDGET))
#define EV_ZOOM_ACTION_WIDGET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_ZOOM_ACTION_WIDGET, EvZoomActionWidgetClass))
#define EV_IS_ZOOM_ACTION_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_ZOOM_ACTION_WIDGET))
#define EV_ZOOM_ACTION_WIDGET_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EV_TYPE_ZOOM_ACTION_WIDGET, EvZoomActionWidgetClass))

typedef struct _EvZoomActionWidget        EvZoomActionWidget;
typedef struct _EvZoomActionWidgetClass   EvZoomActionWidgetClass;
typedef struct _EvZoomActionWidgetPrivate EvZoomActionWidgetPrivate;

struct _EvZoomActionWidget {
        GtkToolItem parent_object;

        EvZoomActionWidgetPrivate *priv;
};

struct _EvZoomActionWidgetClass {
        GtkToolItemClass parent_class;
};

GType      ev_zoom_action_widget_get_type   (void);

void       ev_zoom_action_widget_set_model  (EvZoomActionWidget *control,
                                             EvDocumentModel    *model);
GtkWidget *ev_zoom_action_widget_get_entry  (EvZoomActionWidget *control);
void       ev_zoom_action_widget_set_window (EvZoomActionWidget *control,
                                             EvWindow           *window);

G_END_DECLS

#endif
