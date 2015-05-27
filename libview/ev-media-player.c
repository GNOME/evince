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

#include "config.h"

#include "ev-media-player.h"

#include <gst/video/videooverlay.h>

enum {
        PROP_0,
        PROP_MEDIA,
        PROP_WINDOW_ID,
        PROP_RENDER_AREA,
        PROP_DURATION,
        PROP_POSITION
};

enum {
        STATE_CHANGED,
        N_SIGNALS
};

struct _EvMediaPlayer {
        GObject          parent;

        EvMedia         *media;
        guint64          window_id;
        GdkRectangle     render_area;

        GstElement      *pipeline;
        GstBus          *bus;
        GstVideoOverlay *overlay;
        gboolean         is_playing;
        gboolean         is_seeking;
        gdouble          duration;
        gdouble          position;

        guint            position_timeout_id;
};

struct _EvMediaPlayerClass {
        GObjectClass parent_class;
};

static guint signals[N_SIGNALS];

G_DEFINE_TYPE (EvMediaPlayer, ev_media_player, G_TYPE_OBJECT)

static gboolean
update_position_cb (EvMediaPlayer *player)
{
        gint64 position;

        gst_element_query_position (player->pipeline, GST_FORMAT_TIME, &position);
        player->position = (gdouble)position / GST_SECOND;
        g_object_notify (G_OBJECT (player), "position");

        return G_SOURCE_CONTINUE;
}

static void
ev_media_player_update_position_start (EvMediaPlayer *player)
{
        if (player->position_timeout_id > 0)
                return;

        player->position_timeout_id = g_timeout_add (1000 / 15,
                                                     (GSourceFunc)update_position_cb,
                                                     player);
}

static void
ev_media_player_update_position_stop (EvMediaPlayer *player)
{
        if (player->position_timeout_id > 0) {
                g_source_remove (player->position_timeout_id);
                player->position_timeout_id = 0;
        }
}

static void
ev_media_player_update_position (EvMediaPlayer *player)
{
        if (player->duration <= 0)
                return;

        if (player->is_playing)
                ev_media_player_update_position_start (player);
        else
                ev_media_player_update_position_stop (player);
        update_position_cb (player);
}

static void
ev_media_player_update_state (EvMediaPlayer *player,
                              GstState      *new_state)
{
        GstState state, pending;
        gboolean is_playing;

        gst_element_get_state (player->pipeline, &state, &pending, 250 * GST_NSECOND);

        is_playing = state == GST_STATE_PLAYING;
        if (is_playing != player->is_playing) {
                player->is_playing = is_playing;
                g_signal_emit (player, signals[STATE_CHANGED], 0, player->is_playing ? EV_MEDIA_PLAYER_STATE_PLAY : EV_MEDIA_PLAYER_STATE_PAUSE);
                ev_media_player_update_position (player);
        }

        if (new_state)
                *new_state = state;
}

static void
ev_media_player_notify_eos (EvMediaPlayer *player)
{
        g_signal_emit (player, signals[STATE_CHANGED], 0, EV_MEDIA_PLAYER_STATE_PAUSE);
        ev_media_player_update_position_stop (player);
        player->position = 0;
        g_object_notify (G_OBJECT (player), "position");
        gst_element_set_state (player->pipeline, GST_STATE_READY);
}

static GstBusSyncReply
bus_sync_handle (GstBus        *bus,
                 GstMessage    *message,
                 EvMediaPlayer *player)
{
        GstVideoOverlay *overlay;

        if (!gst_is_video_overlay_prepare_window_handle_message (message))
                return GST_BUS_PASS;

        overlay = GST_VIDEO_OVERLAY (GST_MESSAGE_SRC (message));
        gst_video_overlay_set_window_handle (overlay, (guintptr)player->window_id);
        gst_video_overlay_set_render_rectangle (overlay,
                                                player->render_area.x,
                                                player->render_area.y,
                                                player->render_area.width,
                                                player->render_area.height);
        gst_video_overlay_expose (overlay);

        player->overlay = overlay;

        gst_message_unref (message);

        return GST_BUS_DROP;
}

static void
bus_message_handle (GstBus        *bus,
                    GstMessage    *message,
                    EvMediaPlayer *player)
{
        switch (GST_MESSAGE_TYPE (message)) {
        case GST_MESSAGE_ERROR: {
                GError *error = NULL;
                gchar  *dbg;

                gst_message_parse_error (message, &error, &dbg);
                g_warning ("Error: %s (%s)\n", error->message, dbg);
                g_error_free (error);
                g_free (dbg);
        }
                break;
        case GST_MESSAGE_STATE_CHANGED:
                if (GST_MESSAGE_SRC (message) != (GstObject *)player->pipeline)
                        return;

                if (!player->is_seeking)
                        ev_media_player_update_state (player, NULL);

                break;
        case GST_MESSAGE_ASYNC_DONE: {
                GstState state;

                if (GST_MESSAGE_SRC (message) != (GstObject *)player->pipeline)
                        return;

                if (player->is_seeking) {
                        player->is_seeking = FALSE;
                        if (player->is_playing)
                                ev_media_player_update_position_start (player);
                } else {
                        ev_media_player_update_state (player, &state);

                        if (player->duration == 0 && (state == GST_STATE_PAUSED || state == GST_STATE_PLAYING)) {
                                gint64 duration;

                                gst_element_query_duration (player->pipeline, GST_FORMAT_TIME, &duration);
                                player->duration = (gdouble)duration / GST_SECOND;
                                g_object_notify (G_OBJECT (player), "duration");
                        }
                }

        }
                break;
        case GST_MESSAGE_EOS:
                player->is_playing = FALSE;
                ev_media_player_notify_eos (player);

                break;
        default:
                break;
        }
}

static void
ev_media_player_dispose (GObject *object)
{
        EvMediaPlayer *player = EV_MEDIA_PLAYER (object);

        ev_media_player_update_position_stop (player);

        if (player->bus) {
                gst_bus_remove_signal_watch (player->bus);
                gst_object_unref (player->bus);
                player->bus = NULL;
        }

        if (player->pipeline) {
                gst_element_set_state (player->pipeline, GST_STATE_NULL);
                gst_object_unref (player->pipeline);
                player->pipeline = NULL;
        }

        g_clear_object (&player->media);

        G_OBJECT_CLASS (ev_media_player_parent_class)->dispose (object);
}

static void
ev_media_player_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
        EvMediaPlayer *player = EV_MEDIA_PLAYER (object);

        switch (prop_id) {
        case PROP_DURATION:
                g_value_set_double (value, player->duration);
                break;
        case PROP_POSITION:
                g_value_set_double (value, player->position);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
ev_media_player_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
        EvMediaPlayer *player = EV_MEDIA_PLAYER (object);

        switch (prop_id) {
        case PROP_MEDIA:
                player->media = EV_MEDIA (g_value_dup_object (value));
                break;
        case PROP_WINDOW_ID:
                player->window_id = g_value_get_uint64 (value);
                break;
        case PROP_RENDER_AREA:
                ev_media_player_set_render_area (player, (GdkRectangle *)g_value_get_boxed (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
ev_media_player_init (EvMediaPlayer *player)
{
        player->pipeline = gst_element_factory_make ("playbin", NULL);
        if (!player->pipeline) {
                g_warning ("Failed to create playbin\n");
                return;
        }

        player->bus = gst_pipeline_get_bus (GST_PIPELINE (player->pipeline));
        gst_bus_set_sync_handler (player->bus, (GstBusSyncHandler)bus_sync_handle, player, NULL);
        gst_bus_add_signal_watch (player->bus);
        g_signal_connect_object (player->bus, "message",
                                 G_CALLBACK (bus_message_handle),
                                 player, 0);
}

static void
ev_media_player_constructed (GObject *object)
{
        EvMediaPlayer *player = EV_MEDIA_PLAYER (object);

        G_OBJECT_CLASS (ev_media_player_parent_class)->constructed (object);

        if (!player->pipeline)
                return;

        g_object_set (player->pipeline, "uri", ev_media_get_uri (player->media), NULL);
        gst_element_set_state (player->pipeline, GST_STATE_PLAYING);
}

static void
ev_media_player_class_init (EvMediaPlayerClass *klass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

        if (!gst_is_initialized ()) {
                GError  *error = NULL;

                if (!gst_init_check (NULL, NULL, &error)) {
                        g_warning ("Failed to initialize GStreamer: %s\n", error->message);
                        g_error_free (error);
                }
        }

        g_object_class->constructed = ev_media_player_constructed;
        g_object_class->dispose = ev_media_player_dispose;
        g_object_class->get_property = ev_media_player_get_property;
        g_object_class->set_property = ev_media_player_set_property;

        g_object_class_install_property (g_object_class,
                                         PROP_MEDIA,
                                         g_param_spec_object ("media",
                                                              "Media",
                                                              "The media played by the player",
                                                              EV_TYPE_MEDIA,
                                                              G_PARAM_WRITABLE |
                                                              G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_STATIC_STRINGS));
        g_object_class_install_property (g_object_class,
                                         PROP_WINDOW_ID,
                                         g_param_spec_uint64 ("window-id",
                                                              "Window ID",
                                                              "The identifier of the window where the media will be rendered",
                                                              0, G_MAXUINT64, 0,
                                                              G_PARAM_WRITABLE |
                                                              G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_STATIC_STRINGS));
        g_object_class_install_property (g_object_class,
                                         PROP_RENDER_AREA,
                                         g_param_spec_boxed ("render-area",
                                                             "Render area",
                                                             "The area of the window where the media will be rendered",
                                                             GDK_TYPE_RECTANGLE,
                                                             G_PARAM_WRITABLE |
                                                             G_PARAM_CONSTRUCT |
                                                             G_PARAM_STATIC_STRINGS));
        g_object_class_install_property (g_object_class,
                                         PROP_DURATION,
                                         g_param_spec_double ("duration",
                                                              "Duration",
                                                              "Duration of the media",
                                                              0, G_MAXDOUBLE, 0,
                                                              G_PARAM_READABLE |
                                                              G_PARAM_STATIC_STRINGS));
        g_object_class_install_property (g_object_class,
                                         PROP_POSITION,
                                         g_param_spec_double ("position",
                                                              "Position",
                                                              "Current position of the media",
                                                              0, G_MAXDOUBLE, 0,
                                                              G_PARAM_READABLE |
                                                              G_PARAM_STATIC_STRINGS));

        signals[STATE_CHANGED] =
                g_signal_new ("state-changed",
                              G_TYPE_FROM_CLASS (g_object_class),
                              G_SIGNAL_RUN_LAST,
                              0, NULL, NULL,
                              g_cclosure_marshal_VOID__UINT,
                              G_TYPE_NONE, 1, G_TYPE_UINT);
}

EvMediaPlayer *
ev_media_player_new (EvMedia      *media,
                     guint64       window_id,
                     GdkRectangle *area)
{
        g_return_val_if_fail (EV_IS_MEDIA (media), NULL);
        g_return_val_if_fail (window_id > 0, NULL);
        g_return_val_if_fail (area != NULL, NULL);

        return EV_MEDIA_PLAYER (g_object_new (EV_TYPE_MEDIA_PLAYER,
                                              "media", media,
                                              "window-id", window_id,
                                              "render-area", area,
                                              NULL));
}

void
ev_media_player_set_render_area (EvMediaPlayer *player,
                                 GdkRectangle  *area)
{
        g_return_if_fail (EV_IS_MEDIA_PLAYER (player));
        g_return_if_fail (area != NULL);

        if (player->render_area.x == area->x &&
            player->render_area.y == area->y &&
            player->render_area.width == area->width &&
            player->render_area.height == area->height)
                return;

        player->render_area = *area;
        if (!player->overlay)
                return;

        gst_video_overlay_set_render_rectangle (player->overlay, area->x, area->y, area->width, area->height);
        gst_video_overlay_expose (player->overlay);
}

void
ev_media_player_expose (EvMediaPlayer *player)
{
        g_return_if_fail (EV_IS_MEDIA_PLAYER (player));

        if (!player->overlay)
                return;

        gst_video_overlay_expose (player->overlay);
}

gdouble
ev_media_player_get_duration (EvMediaPlayer *player)
{
        g_return_val_if_fail (EV_IS_MEDIA_PLAYER (player), 0);

        return player->duration;
}

gdouble
ev_media_player_get_position (EvMediaPlayer *player)
{
        g_return_val_if_fail (EV_IS_MEDIA_PLAYER (player), 0);

        return player->position;
}

void
ev_media_player_toggle_state (EvMediaPlayer *player)
{
        GstState current, pending, new_state;

        g_return_if_fail (EV_IS_MEDIA_PLAYER (player));

        if (!player->pipeline)
                return;

        gst_element_get_state (player->pipeline, &current, &pending, 0);
        new_state = current == GST_STATE_PLAYING ? GST_STATE_PAUSED : GST_STATE_PLAYING;
        if (pending != new_state)
                gst_element_set_state (player->pipeline, new_state);
}

void
ev_media_player_seek (EvMediaPlayer *player,
                      gdouble        position)
{
        g_return_if_fail (EV_IS_MEDIA_PLAYER (player));

        if (!player->pipeline)
                return;

        position = CLAMP (position, 0, player->duration);
        if (gst_element_seek_simple (player->pipeline,
                                     GST_FORMAT_TIME,
                                     GST_SEEK_FLAG_FLUSH,
                                     (gint64)(position * GST_SECOND))) {
                player->is_seeking = TRUE;
                ev_media_player_update_position_stop (player);
                player->position = position;
                g_object_notify (G_OBJECT (player), "position");
        }
}
