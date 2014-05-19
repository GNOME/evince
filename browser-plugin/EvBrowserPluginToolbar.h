/*
 * Copyright (C) 2014 Igalia S.L.
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

#ifndef EvBrowserPluginToolbar_h
#define EvBrowserPluginToolbar_h

#include "EvBrowserPlugin.h"
#include <gtk/gtk.h>
#include <evince-view.h>

G_BEGIN_DECLS

#define EV_TYPE_BROWSER_PLUGIN_TOOLBAR              (ev_browser_plugin_toolbar_get_type())
#define EV_BROWSER_PLUGIN_TOOLBAR(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_BROWSER_PLUGIN_TOOLBAR, EvBrowserPluginToolbar))
#define EV_IS_BROWSER_PLUGIN_TOOLBAR(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_BROWSER_PLUGIN_TOOLBAR))
#define EV_BROWSER_PLUGIN_TOOLBAR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_BROWSER_PLUGIN_TOOLBAR, EvBrowserPluginToolbarClass))
#define EV_IS_BROWSER_PLUGIN_TOOLBAR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_BROWSER_PLUGIN_TOOLBAR))
#define EV_BROWSER_PLUGIN_TOOLBAR_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_BROWSER_PLUGIN_TOOLBAR, EvBrowserPluginToolbarClass))

typedef struct _EvBrowserPluginToolbar        EvBrowserPluginToolbar;
typedef struct _EvBrowserPluginToolbarClass   EvBrowserPluginToolbarClass;
typedef struct _EvBrowserPluginToolbarPrivate EvBrowserPluginToolbarPrivate;

struct _EvBrowserPluginToolbar {
        GtkToolbar base_instance;

        EvBrowserPluginToolbarPrivate *priv;
};

struct _EvBrowserPluginToolbarClass {
        GtkToolbarClass base_class;
};

GType      ev_browser_plugin_toolbar_get_type (void);
GtkWidget *ev_browser_plugin_toolbar_new      (EvBrowserPlugin *plugin);

G_END_DECLS

#endif // EvBrowserPluginToolbar_h
