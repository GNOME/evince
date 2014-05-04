/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2013 Aakash Goenka
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

#ifndef __EV_RECENT_VIEW_H__
#define __EV_RECENT_VIEW_H__

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _EvRecentView        EvRecentView;
typedef struct _EvRecentViewClass   EvRecentViewClass;
typedef struct _EvRecentViewPrivate EvRecentViewPrivate;

#define EV_TYPE_RECENT_VIEW              (ev_recent_view_get_type ())
#define EV_RECENT_VIEW(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_RECENT_VIEW, EvRecentView))
#define EV_IS_RECENT_VIEW(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_RECENT_VIEW))
#define EV_RECENT_VIEW_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_RECENT_VIEW, EvRecentViewClass))
#define EV_IS_RECENT_VIEW_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_RECENT_VIEW))
#define EV_RECENT_VIEW_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), EV_TYPE_RECENT_VIEW, EvRecentViewClass))

struct _EvRecentView
{
        GtkScrolledWindow parent;

        EvRecentViewPrivate *priv;
};

struct _EvRecentViewClass
{
        GtkScrolledWindowClass parent_class;
};

GType      ev_recent_view_get_type (void) G_GNUC_CONST;
GtkWidget *ev_recent_view_new      (void);

G_END_DECLS

#endif /* __EV_RECENT_VIEW_H__ */
