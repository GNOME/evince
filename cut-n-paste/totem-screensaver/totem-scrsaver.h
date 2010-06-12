/* 
   Copyright (C) 2004, Bastien Nocera <hadess@hadess.net>

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301  USA.

   Author: Bastien Nocera <hadess@hadess.net>
 */

#ifndef TOTEM_SCRSAVER_H
#define TOTEM_SCRSAVER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define TOTEM_TYPE_SCRSAVER		(totem_scrsaver_get_type ())
#define TOTEM_SCRSAVER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TOTEM_TYPE_SCRSAVER, TotemScrsaver))
#define TOTEM_SCRSAVER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TOTEM_TYPE_SCRSAVER, TotemScrsaverClass))
#define TOTEM_IS_SCRSAVER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TOTEM_TYPE_SCRSAVER))
#define TOTEM_IS_SCRSAVER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TOTEM_TYPE_SCRSAVER))

typedef struct TotemScrsaver TotemScrsaver;
typedef struct TotemScrsaverClass TotemScrsaverClass;
typedef struct TotemScrsaverPrivate TotemScrsaverPrivate;

struct TotemScrsaver {
	GObject parent;
	TotemScrsaverPrivate *priv;
};

struct TotemScrsaverClass {
	GObjectClass parent_class;
};

GType totem_scrsaver_get_type		(void) G_GNUC_CONST;
TotemScrsaver *totem_scrsaver_new       (void);
void totem_scrsaver_enable		(TotemScrsaver *scr);
void totem_scrsaver_disable		(TotemScrsaver *scr);
void totem_scrsaver_set_state		(TotemScrsaver *scr,
					 gboolean enable);

G_END_DECLS

#endif /* !TOTEM_SCRSAVER_H */
