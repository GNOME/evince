/* ev-message-area.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2007 Carlos Garcia Campos
 *
 * Author:
 *   Carlos Garcia Campos <carlosgc@gnome.org>
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

#ifndef EV_MESSAGE_AREA_H
#define EV_MESSAGE_AREA_H

#include <gtk/gtk.h>

#include "gedit-message-area.h"

G_BEGIN_DECLS

#define EV_TYPE_MESSAGE_AREA                  (ev_message_area_get_type ())
#define EV_MESSAGE_AREA(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_MESSAGE_AREA, EvMessageArea))
#define EV_MESSAGE_AREA_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_MESSAGE_AREA, EvMessageAreaClass))
#define EV_IS_MESSAGE_AREA(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_MESSAGE_AREA))
#define EV_IS_MESSAGE_AREA_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_MESSAGE_AREA))
#define EV_MESSAGE_AREA_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_MESSAGE_AREA, EvMessageAreaClass))

typedef struct _EvMessageArea        EvMessageArea;
typedef struct _EvMessageAreaClass   EvMessageAreaClass;
typedef struct _EvMessageAreaPrivate EvMessageAreaPrivate;

struct _EvMessageArea {
	GeditMessageArea parent_instance;

	/*< private >*/
	EvMessageAreaPrivate *priv;
};

struct _EvMessageAreaClass {
	GeditMessageAreaClass parent_class;
};

GType      ev_message_area_get_type             (void) G_GNUC_CONST;
GtkWidget *ev_message_area_new                  (GtkMessageType type,
						 const gchar   *text,
						 const gchar   *first_button_text,
						 ...);
void       ev_message_area_set_image            (EvMessageArea *area,
						 GtkWidget     *image);
void       ev_message_area_set_image_from_stock (EvMessageArea *area,
						 const gchar   *stock_id);
void       ev_message_area_set_text             (EvMessageArea *area,
						 const gchar   *str);
void       ev_message_area_set_secondary_text   (EvMessageArea *area,
						 const gchar   *str);

G_END_DECLS

#endif /* EV_MESSAGE_AREA_H */
