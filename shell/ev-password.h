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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __EV_PASSWORD_H__
#define __EV_PASSWORD_H__

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

GtkWidget *ev_password_dialog_new          (GtkWidget   *toplevel,
					    const gchar *uri);
char      *ev_password_dialog_get_password (GtkWidget *password);
void       ev_password_dialog_set_bad_pass (GtkWidget *password);

G_END_DECLS

#endif /* __EV_PASSWORD_H__ */
