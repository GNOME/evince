/* ev-toolbar.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2012 Carlos Garcia Campos <carlosgc@gnome.org>
 * Copyright (C) 2018 Germán Poo-Caamaño <gpoo@gnome.org>
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

#ifndef __EV_PREVIEWER_TOOLBAR_H__
#define __EV_PREVIEWER_TOOLBAR_H__

#include <gtk/gtk.h>
#include "ev-previewer-window.h"

G_BEGIN_DECLS

#define EV_TYPE_PREVIEWER_TOOLBAR              (ev_previewer_toolbar_get_type())
#define EV_PREVIEWER_TOOLBAR(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_PREVIEWER_TOOLBAR, EvPreviewerToolbar))
#define EV_IS_PREVIEWER_TOOLBAR(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_PREVIEWER_TOOLBAR))
#define EV_PREVIEWER_TOOLBAR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_PREVIEWER_TOOLBAR, EvPreviewerToolbarClass))
#define EV_IS_PREVIEWER_TOOLBAR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_PREVIEWER_TOOLBAR))
#define EV_PREVIEWER_TOOLBAR_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_PREVIEWER_TOOLBAR, EvPreviewerToolbarClass))

typedef struct _EvPreviewerToolbar        EvPreviewerToolbar;
typedef struct _EvPreviewerToolbarClass   EvPreviewerToolbarClass;

struct _EvPreviewerToolbar {
        GtkHeaderBar base_instance;
};

struct _EvPreviewerToolbarClass {
        GtkHeaderBarClass base_class;
};

GType         ev_previewer_toolbar_get_type           (void);
GtkWidget    *ev_previewer_toolbar_new                (EvPreviewerWindow *window);
GtkWidget    *ev_previewer_toolbar_get_page_selector  (EvPreviewerToolbar *ev_previewer_toolbar);

G_END_DECLS

#endif /* __EV_PREVIEWER_TOOLBAR_H__ */
