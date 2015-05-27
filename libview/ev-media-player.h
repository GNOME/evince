/* ev-media-player.h
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

#ifndef EV_MEDIA_PLAYER_H
#define EV_MEDIA_PLAYER_H

#include <glib-object.h>
#include <gdk/gdk.h>
#include "ev-media.h"

G_BEGIN_DECLS

#define EV_TYPE_MEDIA_PLAYER              (ev_media_player_get_type())
#define EV_MEDIA_PLAYER(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_MEDIA_PLAYER, EvMediaPlayer))
#define EV_IS_MEDIA_PLAYER(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_MEDIA_PLAYER))
#define EV_MEDIA_PLAYER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_MEDIA_PLAYER, EvMediaPlayerClass))
#define EV_IS_MEDIA_PLAYER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_MEDIA_PLAYER))
#define EV_MEDIA_PLAYER_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_MEDIA_PLAYER, EvMediaPlayerClass))

typedef enum {
        EV_MEDIA_PLAYER_STATE_PAUSE,
        EV_MEDIA_PLAYER_STATE_PLAY
} EvMediaPlayerState;

typedef struct _EvMediaPlayer      EvMediaPlayer;
typedef struct _EvMediaPlayerClass EvMediaPlayerClass;

GType          ev_media_player_get_type        (void) G_GNUC_CONST;
EvMediaPlayer *ev_media_player_new             (EvMedia       *media,
                                                guint64        window_id,
                                                GdkRectangle  *area);
void           ev_media_player_set_render_area (EvMediaPlayer *player,
                                                GdkRectangle  *area);
void           ev_media_player_expose          (EvMediaPlayer *player);
gdouble        ev_media_player_get_duration    (EvMediaPlayer *player);
gdouble        ev_media_player_get_position    (EvMediaPlayer *player);
void           ev_media_player_toggle_state    (EvMediaPlayer *player);
void           ev_media_player_seek            (EvMediaPlayer *player,
                                                gdouble        position);

G_END_DECLS

#endif /* EV_MEDIA_PLAYER_H */
