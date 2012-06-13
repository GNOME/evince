/* ev-bookmark-action.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2010 Carlos Garcia Campos  <carlosgc@gnome.org>
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

#include "config.h"

#include "ev-bookmark-action.h"

enum {
        PROP_0,
        PROP_PAGE
};

struct _EvBookmarkAction {
        GtkAction base;

        guint     page;
};

struct _EvBookmarkActionClass {
        GtkActionClass base_class;
};

G_DEFINE_TYPE (EvBookmarkAction, ev_bookmark_action, GTK_TYPE_ACTION)

static void
ev_bookmark_action_init (EvBookmarkAction *action)
{
}

static void
ev_bookmark_action_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
        EvBookmarkAction *action = EV_BOOKMARK_ACTION (object);

        switch (prop_id) {
        case PROP_PAGE:
                action->page = g_value_get_uint (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
ev_bookmark_action_class_init (EvBookmarkActionClass *klass)
{
        GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

        gobject_class->set_property = ev_bookmark_action_set_property;

        g_object_class_install_property (gobject_class,
                                         PROP_PAGE,
                                         g_param_spec_uint ("page",
                                                            "Page",
                                                            "The bookmark page",
                                                            0, G_MAXUINT, 0,
                                                            G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                                                            G_PARAM_STATIC_STRINGS));
}

GtkAction *
ev_bookmark_action_new (EvBookmark *bookmark)
{
        GtkAction *action;
        gchar *name;

        g_return_val_if_fail (bookmark->title != NULL, NULL);

        name = g_strdup_printf ("EvBookmark%u", bookmark->page);
        action = GTK_ACTION (g_object_new (EV_TYPE_BOOKMARK_ACTION,
                                           "name", name,
                                           "label", bookmark->title,
                                           "page", bookmark->page,
                                           NULL));
        g_free (name);

        return action;
}

guint
ev_bookmark_action_get_page (EvBookmarkAction *action)
{
        g_return_val_if_fail (EV_IS_BOOKMARK_ACTION (action), 0);

        return action->page;
}
