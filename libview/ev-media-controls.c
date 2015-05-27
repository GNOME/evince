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

#include "config.h"

#include "ev-media-controls.h"

struct _EvMediaControls {
        GtkBox         parent;

        GtkWidget     *button;
        GtkWidget     *slider;

        EvMediaPlayer *player;
};

struct _EvMediaControlsClass {
        GtkBoxClass parent_class;
};

G_DEFINE_TYPE (EvMediaControls, ev_media_controls, GTK_TYPE_BOX)

static void
ev_media_controls_toggle_state (EvMediaControls *controls)
{
        ev_media_player_toggle_state (controls->player);
}

static void
ev_media_controls_seek (EvMediaControls *controls,
                        GtkScrollType    scroll,
                        gdouble          value)
{
        ev_media_player_seek (controls->player, value);
}

static void
media_player_state_changed (EvMediaPlayer   *player,
                            guint            state,
                            EvMediaControls *controls)
{
        const gchar *icon_name = NULL;
        GtkWidget   *image;

        image = gtk_button_get_image (GTK_BUTTON (controls->button));

        switch (state) {
        case EV_MEDIA_PLAYER_STATE_PLAY:
                icon_name = "media-playback-pause-symbolic";
                break;
        case EV_MEDIA_PLAYER_STATE_PAUSE:
                icon_name = "media-playback-start-symbolic";
                break;
        default:
                g_assert_not_reached ();
        }

        gtk_image_set_from_icon_name (GTK_IMAGE (image), icon_name, GTK_ICON_SIZE_MENU);
}

static void
media_player_duration_changed (EvMediaPlayer   *player,
                               GParamSpec      *spec,
                               EvMediaControls *controls)
{
        gtk_range_set_range (GTK_RANGE (controls->slider), 0,
                             ev_media_player_get_duration (player));
}

static void
media_player_position_changed (EvMediaPlayer   *player,
                               GParamSpec      *spec,
                               EvMediaControls *controls)
{
        gtk_range_set_value (GTK_RANGE (controls->slider),
                             ev_media_player_get_position (player));
}

static void
ev_media_controls_dispose (GObject *object)
{
        EvMediaControls *controls = EV_MEDIA_CONTROLS (object);

        g_clear_object (&controls->player);

        G_OBJECT_CLASS (ev_media_controls_parent_class)->dispose (object);
}

static void
ev_media_controls_init (EvMediaControls *controls)
{
        GtkAdjustment  *adjustment;
        GtkCssProvider *provider;

        gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (controls)),
                                     GTK_STYLE_CLASS_OSD);

        gtk_orientable_set_orientation (GTK_ORIENTABLE (controls), GTK_ORIENTATION_HORIZONTAL);

        controls->button = gtk_button_new ();
        g_signal_connect_swapped (controls->button, "clicked",
                                  G_CALLBACK (ev_media_controls_toggle_state),
                                  controls);
        gtk_widget_set_name (controls->button, "ev-media-controls-play-button");
        gtk_widget_set_valign (controls->button, GTK_ALIGN_CENTER);
        gtk_button_set_relief (GTK_BUTTON (controls->button), GTK_RELIEF_NONE);
        gtk_button_set_image (GTK_BUTTON (controls->button),
                              gtk_image_new_from_icon_name ("media-playback-start-symbolic",
                                                            GTK_ICON_SIZE_MENU));
        gtk_button_set_label(GTK_BUTTON (controls->button), NULL);
        gtk_button_set_focus_on_click (GTK_BUTTON (controls->button), FALSE);

        provider = gtk_css_provider_new ();
        gtk_css_provider_load_from_data (provider, "#ev-media-controls-play-button { padding: 0px 8px 0px 8px; }", -1, NULL);
        gtk_style_context_add_provider (gtk_widget_get_style_context (controls->button),
                                        GTK_STYLE_PROVIDER (provider),
                                        GTK_STYLE_PROVIDER_PRIORITY_USER);
        g_object_unref (provider);

        gtk_box_pack_start (GTK_BOX (controls), controls->button, FALSE, TRUE, 0);
        gtk_widget_show (controls->button);

        adjustment = gtk_adjustment_new (0, 0, 1, 0.1, 0.10, 0);
        controls->slider = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, adjustment);
        g_signal_connect_swapped (controls->slider, "change-value",
                                  G_CALLBACK (ev_media_controls_seek),
                                  controls);
        gtk_widget_set_hexpand (controls->slider, TRUE);
        gtk_scale_set_draw_value (GTK_SCALE (controls->slider), FALSE);
        gtk_box_pack_start (GTK_BOX (controls), controls->slider, FALSE, TRUE, 0);
        gtk_widget_show (controls->slider);
}

static void
ev_media_controls_class_init (EvMediaControlsClass *klass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

        g_object_class->dispose = ev_media_controls_dispose;
}

GtkWidget *
ev_media_controls_new (EvMediaPlayer *player)
{
        EvMediaControls *controls;

        g_return_val_if_fail (EV_IS_MEDIA_PLAYER (player), NULL);

        controls = EV_MEDIA_CONTROLS (g_object_new (EV_TYPE_MEDIA_CONTROLS, NULL));
        controls->player = g_object_ref (player);
        g_signal_connect_object (controls->player, "state-changed",
                                 G_CALLBACK (media_player_state_changed),
                                 controls, 0);
        g_signal_connect_object (controls->player, "notify::duration",
                                 G_CALLBACK (media_player_duration_changed),
                                 controls, 0);
        g_signal_connect_object (controls->player, "notify::position",
                                 G_CALLBACK (media_player_position_changed),
                                 controls, 0);

        return GTK_WIDGET (controls);
}
