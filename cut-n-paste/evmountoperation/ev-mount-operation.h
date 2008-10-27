/* GTK - The GIMP Toolkit
 * Copyright (C) Christian Kellner <gicmo@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __EV_MOUNT_OPERATION_H__
#define __EV_MOUNT_OPERATION_H__

#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EV_TYPE_MOUNT_OPERATION         (ev_mount_operation_get_type ())
#define EV_MOUNT_OPERATION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), EV_TYPE_MOUNT_OPERATION, EvMountOperation))
#define EV_MOUNT_OPERATION_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EV_TYPE_MOUNT_OPERATION, EvMountOperationClass))
#define EV_IS_MOUNT_OPERATION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), EV_TYPE_MOUNT_OPERATION))
#define EV_IS_MOUNT_OPERATION_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), EV_TYPE_MOUNT_OPERATION))
#define EV_MOUNT_OPERATION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), EV_TYPE_MOUNT_OPERATION, EvMountOperationClass))

typedef struct EvMountOperation         EvMountOperation;
typedef struct EvMountOperationClass    EvMountOperationClass;
typedef struct EvMountOperationPrivate  EvMountOperationPrivate;

struct EvMountOperation
{
  GMountOperation parent_instance;

  EvMountOperationPrivate *priv;
};

struct EvMountOperationClass
{
  GMountOperationClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};


GType            ev_mount_operation_get_type   (void);
GMountOperation *ev_mount_operation_new        (GtkWindow         *parent);
gboolean         ev_mount_operation_is_showing (EvMountOperation *op);
void             ev_mount_operation_set_parent (EvMountOperation *op,
						GtkWindow         *parent);
GtkWindow *      ev_mount_operation_get_parent (EvMountOperation *op);
void             ev_mount_operation_set_screen (EvMountOperation *op,
						GdkScreen         *screen);
GdkScreen       *ev_mount_operation_get_screen (EvMountOperation *op);

G_END_DECLS

#endif /* __EV_MOUNT_OPERATION_H__ */

