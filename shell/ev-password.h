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

#include <gtk/gtkdialog.h>

G_BEGIN_DECLS

#define EV_TYPE_PASSWORD_DIALOG            (ev_password_dialog_get_type ())
#define EV_PASSWORD_DIALOG(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), EV_TYPE_PASSWORD_DIALOG, EvPasswordDialog))
#define EV_PASSWORD_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_PASSWORD_DIALOG, EvPasswordDialogClass))
#define EV_IS_PASSWORD_DIALOG(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), EV_TYPE_PASSWORD_DIALOG))
#define EV_IS_PASSWORD_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_PASSWORD_DIALOG))
#define EV_PASSWORD_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_PASSWORD_DIALOG, EvPasswordDialogClass))

typedef struct _EvPasswordDialog EvPasswordDialog;
typedef struct _EvPasswordDialogClass EvPasswordDialogClass;
typedef struct _EvPasswordDialogPrivate EvPasswordDialogPrivate;

struct _EvPasswordDialog 
{
  GtkDialog parent_instance;

  EvPasswordDialogPrivate* priv;
};

struct _EvPasswordDialogClass 
{
  GtkDialogClass parent_class;
};

GType     ev_password_dialog_get_type               (void) G_GNUC_CONST;

char      *ev_password_dialog_get_password  (EvPasswordDialog *dialog);
void       ev_password_dialog_set_bad_pass  (EvPasswordDialog *dialog);
void	   ev_password_dialog_save_password (EvPasswordDialog *dialog);

G_END_DECLS

#endif /* __EV_PASSWORD_H__ */
