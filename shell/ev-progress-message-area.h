/* ev-progress-message-area.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2008 Carlos Garcia Campos <carlosgc@gnome.org>
 * Copyright (C) 2018 Germán Poo-Caamaño <gpoo@gnome.org>
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

#pragma once

#include <gtk/gtk.h>

#include "ev-message-area.h"

G_BEGIN_DECLS

#define EV_TYPE_PROGRESS_MESSAGE_AREA                  (ev_progress_message_area_get_type ())
#define EV_PROGRESS_MESSAGE_AREA(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_PROGRESS_MESSAGE_AREA, EvProgressMessageArea))
#define EV_PROGRESS_MESSAGE_AREA_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_PROGRESS_MESSAGE_AREA, EvProgressMessageAreaClass))
#define EV_IS_PROGRESS_MESSAGE_AREA(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_PROGRESS_MESSAGE_AREA))
#define EV_IS_PROGRESS_MESSAGE_AREA_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_PROGRESS_MESSAGE_AREA))
#define EV_PROGRESS_MESSAGE_AREA_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_PROGRESS_MESSAGE_AREA, EvProgressMessageAreaClass))

typedef struct _EvProgressMessageArea        EvProgressMessageArea;
typedef struct _EvProgressMessageAreaClass   EvProgressMessageAreaClass;

struct _EvProgressMessageArea {
	EvMessageArea parent_instance;
};

struct _EvProgressMessageAreaClass {
	EvMessageAreaClass parent_class;
};

GType      ev_progress_message_area_get_type        (void) G_GNUC_CONST;
GtkWidget *ev_progress_message_area_new             (const gchar           *icon_name,
						     const gchar           *text,
						     const gchar           *first_button_text,
						     ...);
void       ev_progress_message_area_set_status      (EvProgressMessageArea *area,
						     const gchar           *str);
void       ev_progress_message_area_set_fraction    (EvProgressMessageArea *area,
						     gdouble                fraction);

G_END_DECLS
