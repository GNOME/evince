/* ev-toolbar.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2012 Carlos Garcia Campos <carlosgc@gnome.org>
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

#ifndef __EV_TOOLBAR_H__
#define __EV_TOOLBAR_H__

#include <gtk/gtk.h>
#include "ev-window.h"

G_BEGIN_DECLS

typedef enum {
	EV_TOOLBAR_MODE_NORMAL,
	EV_TOOLBAR_MODE_FULLSCREEN,
	EV_TOOLBAR_MODE_RECENT_VIEW
} EvToolbarMode;

#define EV_TYPE_TOOLBAR              (ev_toolbar_get_type())
#define EV_TOOLBAR(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_TOOLBAR, EvToolbar))
#define EV_IS_TOOLBAR(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_TOOLBAR))
#define EV_TOOLBAR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_TOOLBAR, EvToolbarClass))
#define EV_IS_TOOLBAR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_TOOLBAR))
#define EV_TOOLBAR_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_TOOLBAR, EvToolbarClass))

typedef struct _EvToolbar        EvToolbar;
typedef struct _EvToolbarClass   EvToolbarClass;

struct _EvToolbar {
        HdyHeaderBar base_instance;
};

struct _EvToolbarClass {
        HdyHeaderBarClass base_class;
};

GType         ev_toolbar_get_type           (void);
GtkWidget    *ev_toolbar_new                (EvWindow *window);
gboolean      ev_toolbar_has_visible_popups (EvToolbar *ev_toolbar);
void          ev_toolbar_action_menu_toggle (EvToolbar *ev_toolbar);
GtkWidget    *ev_toolbar_get_page_selector  (EvToolbar *ev_toolbar);
void          ev_toolbar_set_mode           (EvToolbar     *ev_toolbar,
					     EvToolbarMode  mode);
EvToolbarMode ev_toolbar_get_mode           (EvToolbar     *ev_toolbar);

G_END_DECLS

#endif /* __EV_TOOLBAR_H__ */
