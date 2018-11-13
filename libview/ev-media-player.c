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

#if defined (GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#elif defined (GDK_WINDOWING_WIN32)
#include <gdk/gdkwin32.h>
#endif

enum {
        PROP_0,
        PROP_MEDIA,
};

struct _EvMediaPlayer {
        GtkBox           parent;

        EvMedia         *media;
        GtkWidget       *drawing_area;
        GtkWidget       *controls;
        GtkWidget       *play_button;
        GtkWidget       *slider;

        GstElement      *pipeline;
        GstBus          *bus;
        GstVideoOverlay *overlay;
        guint64          window_handle;
        gboolean         is_playing;
        gboolean         is_seeking;
        gdouble          duration;
        gdouble          position;

        guint            position_timeout_id;
};

struct _EvMediaPlayerClass {
        GtkBoxClass parent_class;
};

G_DEFINE_TYPE (EvMediaPlayer, ev_media_player, GTK_TYPE_BOX)

static void
ev_media_player_update_position (EvMediaPlayer *player)
{
        if (!ev_media_get_show_controls (player->media))
                return;

        gtk_range_set_value (GTK_RANGE (player->slider), player->position);
}

static gboolean
query_position_cb (EvMediaPlayer *player)
{
        gint64 position;

        gst_element_query_position (player->pipeline, GST_FORMAT_TIME, &position);
        player->position = (gdouble)position / GST_SECOND;
        ev_media_player_update_position (player);

        return G_SOURCE_CONTINUE;
}

static void
ev_media_player_query_position_start (EvMediaPlayer *player)
{
        if (player->position_timeout_id > 0)
                return;

        if (!ev_media_get_show_controls (player->media))
                return;

        player->position_timeout_id = g_timeout_add (1000 / 15,
                                                     (GSourceFunc)query_position_cb,
                                                     player);
}

static void
ev_media_player_query_position_stop (EvMediaPlayer *player)
{
        if (player->position_timeout_id > 0) {
                g_source_remove (player->position_timeout_id);
                player->position_timeout_id = 0;
        }
}

static void
ev_media_player_query_position (EvMediaPlayer *player)
{
        if (!ev_media_get_show_controls (player->media))
                return;

        if (player->duration <= 0)
                return;

        if (player->is_playing)
                ev_media_player_query_position_start (player);
        else
                ev_media_player_query_position_stop (player);
        query_position_cb (player);
}

static void
ev_media_player_update_play_button (EvMediaPlayer *player)
{
        if (!ev_media_get_show_controls (player->media))
                return;

        gtk_image_set_from_icon_name (GTK_IMAGE (gtk_button_get_image (GTK_BUTTON (player->play_button))),
                                      player->is_playing ? "media-playback-pause-symbolic" : "media-playback-start-symbolic",
                                      GTK_ICON_SIZE_MENU);
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
                ev_media_player_update_play_button (player);
                ev_media_player_query_position (player);
        }

        if (new_state)
                *new_state = state;
}

static void
ev_media_player_toggle_state (EvMediaPlayer *player)
{
        GstState current, pending, new_state;

        if (!player->pipeline)
                return;

        gst_element_get_state (player->pipeline, &current, &pending, 0);
        new_state = current == GST_STATE_PLAYING ? GST_STATE_PAUSED : GST_STATE_PLAYING;
        if (pending != new_state)
                gst_element_set_state (player->pipeline, new_state);
}

static void
ev_media_player_seek (EvMediaPlayer *player,
                      GtkScrollType  scroll,
                      gdouble        position)
{
        if (!player->pipeline)
                return;

        position = CLAMP (position, 0, player->duration);
        if (gst_element_seek_simple (player->pipeline,
                                     GST_FORMAT_TIME,
                                     GST_SEEK_FLAG_FLUSH,
                                     (gint64)(position * GST_SECOND))) {
                player->is_seeking = TRUE;
                ev_media_player_query_position_stop (player);
                player->position = position;
                ev_media_player_update_position (player);
        }
}


static void
ev_media_player_notify_eos (EvMediaPlayer *player)
{
        if (!ev_media_get_show_controls (player->media)) {
                /* A media without controls can't be played again */
                gtk_widget_destroy (GTK_WIDGET (player));
                return;
        }

        ev_media_player_update_play_button (player);
        ev_media_player_query_position_stop (player);
        player->position = 0;
        ev_media_player_update_position (player);
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
        gst_video_overlay_set_window_handle (overlay, (guintptr)player->window_handle);
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

                if (!ev_media_get_show_controls (player->media))
                        return;

                if (player->is_seeking) {
                        player->is_seeking = FALSE;
                        if (player->is_playing)
                                ev_media_player_query_position_start (player);
                } else {
                        ev_media_player_update_state (player, &state);

                        if (player->duration == 0 && (state == GST_STATE_PAUSED || state == GST_STATE_PLAYING)) {
                                gint64 duration;

                                gst_element_query_duration (player->pipeline, GST_FORMAT_TIME, &duration);
                                player->duration = (gdouble)duration / GST_SECOND;
                                gtk_range_set_range (GTK_RANGE (player->slider), 0, player->duration);
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
drawing_area_realize_cb (GtkWidget     *widget,
                         EvMediaPlayer *player)
{
#if defined (GDK_WINDOWING_X11)
        player->window_handle = (guint64)GDK_WINDOW_XID (gtk_widget_get_window (widget));
#elif defined (GDK_WINDOWING_WIN32)
        player->window_handle = (guint64)GDK_WINDOW_HWND (gtk_widget_get_window (widget));
#else
        g_assert_not_reached ();
#endif
}

static void
ev_media_player_size_allocate (GtkWidget     *widget,
                               GtkAllocation *allocation)
{
        EvMediaPlayer *player = EV_MEDIA_PLAYER (widget);
        GdkRectangle   controls_allocation;

        GTK_WIDGET_CLASS (ev_media_player_parent_class)->size_allocate (widget, allocation);

        if (!ev_media_get_show_controls (player->media))
                return;

        /* Give all the allocated size to the drawing area */
        gtk_widget_size_allocate (player->drawing_area, allocation);

        /* And give space for the controls below */
        controls_allocation.x = allocation->x;
        controls_allocation.y = allocation->y + allocation->height;
        controls_allocation.width = allocation->width;
        controls_allocation.height = gtk_widget_get_allocated_height (player->controls);
        gtk_widget_size_allocate (player->controls, &controls_allocation);

        allocation->height += controls_allocation.height;

        gtk_widget_set_allocation (widget, allocation);
}

static void
ev_media_player_dispose (GObject *object)
{
        EvMediaPlayer *player = EV_MEDIA_PLAYER (object);

        ev_media_player_query_position_stop (player);

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
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
ev_media_player_init (EvMediaPlayer *player)
{
        gtk_orientable_set_orientation (GTK_ORIENTABLE (player), GTK_ORIENTATION_VERTICAL);

        player->pipeline = gst_element_factory_make ("playbin", NULL);
        if (!player->pipeline) {
                g_warning ("Failed to create playbin\n");
                return;
        }

        player->drawing_area = gtk_drawing_area_new ();
        g_signal_connect (player->drawing_area, "realize",
                          G_CALLBACK (drawing_area_realize_cb),
                          player);
        gtk_box_pack_start (GTK_BOX (player), player->drawing_area, TRUE, TRUE, 0);
        gtk_widget_show (player->drawing_area);

        player->bus = gst_pipeline_get_bus (GST_PIPELINE (player->pipeline));
        gst_bus_set_sync_handler (player->bus, (GstBusSyncHandler)bus_sync_handle, player, NULL);
        gst_bus_add_signal_watch (player->bus);
        g_signal_connect_object (player->bus, "message",
                                 G_CALLBACK (bus_message_handle),
                                 player, 0);
}

static void
ev_media_player_setup_media_controls (EvMediaPlayer *player)
{
        GtkAdjustment  *adjustment;
        GtkCssProvider *provider;

        player->controls = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

        gtk_style_context_add_class (gtk_widget_get_style_context (player->controls), GTK_STYLE_CLASS_OSD);

        player->play_button = gtk_button_new ();
        g_signal_connect_swapped (player->play_button, "clicked",
                                  G_CALLBACK (ev_media_player_toggle_state),
                                  player);
        gtk_widget_set_name (player->play_button, "ev-media-player-play-button");
        gtk_widget_set_valign (player->play_button, GTK_ALIGN_CENTER);
        gtk_button_set_relief (GTK_BUTTON (player->play_button), GTK_RELIEF_NONE);
        gtk_button_set_image (GTK_BUTTON (player->play_button),
                              gtk_image_new_from_icon_name ("media-playback-start-symbolic",
                                                            GTK_ICON_SIZE_MENU));
        gtk_button_set_label(GTK_BUTTON (player->play_button), NULL);
        gtk_widget_set_focus_on_click (player->play_button, FALSE);

        provider = gtk_css_provider_new ();
        gtk_css_provider_load_from_data (provider, "#ev-media-player-play-button { padding: 0px 8px 0px 8px; }", -1, NULL);
        gtk_style_context_add_provider (gtk_widget_get_style_context (player->play_button),
                                        GTK_STYLE_PROVIDER (provider),
                                        GTK_STYLE_PROVIDER_PRIORITY_USER);
        g_object_unref (provider);

        gtk_box_pack_start (GTK_BOX (player->controls), player->play_button, FALSE, TRUE, 0);
        gtk_widget_show (player->play_button);

        adjustment = gtk_adjustment_new (0, 0, 1, 0.1, 0.10, 0);
        player->slider = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, adjustment);
        g_signal_connect_swapped (player->slider, "change-value",
                                  G_CALLBACK (ev_media_player_seek),
                                  player);
        gtk_widget_set_hexpand (player->slider, TRUE);
        gtk_scale_set_draw_value (GTK_SCALE (player->slider), FALSE);
        gtk_box_pack_start (GTK_BOX (player->controls), player->slider, FALSE, TRUE, 0);
        gtk_widget_show (player->slider);

        gtk_box_pack_start (GTK_BOX (player), player->controls, FALSE, FALSE, 0);
        gtk_widget_show (player->controls);
}

static void
ev_media_player_constructed (GObject *object)
{
        EvMediaPlayer *player = EV_MEDIA_PLAYER (object);

        G_OBJECT_CLASS (ev_media_player_parent_class)->constructed (object);

        if (ev_media_get_show_controls (player->media))
                ev_media_player_setup_media_controls (player);

        if (!player->pipeline)
                return;

        g_object_set (player->pipeline, "uri", ev_media_get_uri (player->media), NULL);
        gst_element_set_state (player->pipeline, GST_STATE_PLAYING);
}

static void
ev_media_player_class_init (EvMediaPlayerClass *klass)
{
        GObjectClass   *g_object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        if (!gst_is_initialized ()) {
                GError  *error = NULL;

                if (!gst_init_check (NULL, NULL, &error)) {
                        g_warning ("Failed to initialize GStreamer: %s\n", error->message);
                        g_error_free (error);
                }
        }

        g_object_class->constructed = ev_media_player_constructed;
        g_object_class->dispose = ev_media_player_dispose;
        g_object_class->set_property = ev_media_player_set_property;
        widget_class->size_allocate = ev_media_player_size_allocate;

        g_object_class_install_property (g_object_class,
                                         PROP_MEDIA,
                                         g_param_spec_object ("media",
                                                              "Media",
                                                              "The media played by the player",
                                                              EV_TYPE_MEDIA,
                                                              G_PARAM_WRITABLE |
                                                              G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_STATIC_STRINGS));
}

GtkWidget *
ev_media_player_new (EvMedia *media)
{
        g_return_val_if_fail (EV_IS_MEDIA (media), NULL);

        return GTK_WIDGET (g_object_new (EV_TYPE_MEDIA_PLAYER, "media", media, NULL));
}

EvMedia *
ev_media_player_get_media (EvMediaPlayer *player)
{
        g_return_val_if_fail (EV_IS_MEDIA_PLAYER (player), NULL);

        return player->media;
}
