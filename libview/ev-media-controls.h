/* ev-media-controls.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2015 Igalia S.L.
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

#ifndef EV_MEDIA_CONTROLS_H
#define EV_MEDIA_CONTROLS_H

#include <gtk/gtk.h>
#include "ev-media-player.h"

G_BEGIN_DECLS

#define EV_TYPE_MEDIA_CONTROLS              (ev_media_controls_get_type())
#define EV_MEDIA_CONTROLS(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_MEDIA_CONTROLS, EvMediaControls))
#define EV_IS_MEDIA_CONTROLS(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_MEDIA_CONTROLS))
#define EV_MEDIA_CONTROLS_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_MEDIA_CONTROLS, EvMediaControlsClass))
#define EV_IS_MEDIA_CONTROLS_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_MEDIA_CONTROLS))
#define EV_MEDIA_CONTROLS_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_MEDIA_CONTROLS, EvMediaControlsClass))

typedef struct _EvMediaControls      EvMediaControls;
typedef struct _EvMediaControlsClass EvMediaControlsClass;

GType      ev_media_controls_get_type (void) G_GNUC_CONST;

GtkWidget *ev_media_controls_new      (EvMediaPlayer *player);

G_END_DECLS

#endif /* EV_MEDIA_CONTROLS_H */
