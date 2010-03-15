/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2005 Red Hat, Inc
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

#ifndef __EV_PASSWORD_VIEW_H__
#define __EV_PASSWORD_VIEW_H__

#include <gtk/gtk.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define EV_TYPE_PASSWORD_VIEW            (ev_password_view_get_type ())
#define EV_PASSWORD_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_PASSWORD_VIEW, EvPasswordView))
#define EV_IS_PASSWORD_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_PASSWORD_VIEW))

typedef struct _EvPasswordView        EvPasswordView;
typedef struct _EvPasswordViewClass   EvPasswordViewClass;
typedef struct _EvPasswordViewPrivate EvPasswordViewPrivate;

struct _EvPasswordView {
	GtkViewport parent_instance;
	
	EvPasswordViewPrivate *priv;
};

struct _EvPasswordViewClass {
	GtkViewportClass parent_class;

	/* signals */
	void (*unlock) (EvPasswordView	*self);
};

GType	      ev_password_view_get_type                (void) G_GNUC_CONST;
GtkWidget    *ev_password_view_new                     (GtkWindow      *parent);
void          ev_password_view_set_uri                 (EvPasswordView *password_view,
							const char     *uri);
void          ev_password_view_ask_password            (EvPasswordView *password_view);
const gchar  *ev_password_view_get_password            (EvPasswordView *password_view);
GPasswordSave ev_password_view_get_password_save_flags (EvPasswordView *password_view);

G_END_DECLS

#endif /* __EV_PASSWORD_VIEW_H__ */
