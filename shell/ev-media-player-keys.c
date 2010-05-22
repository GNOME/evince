/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2007 Jan Arne Petersen <jap@gnome.org>
 * Copyright (C) 2008 Bastien Nocera <hadess@hadess.net>
 * Copyright © 2010 Christian Persch
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
 *
 */

#include "config.h"

#include "ev-media-player-keys.h"

#include <string.h>
#include <glib.h>
#include <gio/gio.h>

#define SD_NAME        "org.gnome.SettingsDaemon"
#define SD_OBJECT_PATH "/org/gnome/SettingsDaemon/MediaKeys"
#define SD_INTERFACE   "org.gnome.SettingsDaemon.MediaKeys"

enum {
	KEY_PRESSED,
	LAST_SIGNAL
};

struct _EvMediaPlayerKeys
{
	GObject        parent;

        GDBusProxy *proxy;
        guint watch_id;
};

struct _EvMediaPlayerKeysClass
{
	GObjectClass parent_class;

	/* Signals */
	void (* key_pressed) (EvMediaPlayerKeys *keys,
			      const gchar       *key);
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EvMediaPlayerKeys, ev_media_player_keys, G_TYPE_OBJECT)

static void ev_media_player_keys_finalize (GObject *object);

static void
ev_media_player_keys_class_init (EvMediaPlayerKeysClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = ev_media_player_keys_finalize;

	signals[KEY_PRESSED] =
		g_signal_new ("key_pressed",
			      EV_TYPE_MEDIA_PLAYER_KEYS,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EvMediaPlayerKeysClass, key_pressed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1, G_TYPE_STRING);
}

static void
ev_media_player_keys_grab_keys (EvMediaPlayerKeys *keys)
{
	/*
	 * The uint as second argument is time. We give a very low value so that
	 * if a media player is there it gets higher priority on the keys (0 is
	 * a special value having maximum priority).
	 */
        g_dbus_proxy_call (keys->proxy,
			   "GrabMediaPlayerKeys",
			   g_variant_new ("(su)", "Evince", 1),
			   G_DBUS_CALL_FLAGS_NO_AUTO_START,
			   -1,
			   NULL, NULL, NULL);
}

static void
ev_media_player_keys_release_keys (EvMediaPlayerKeys *keys)
{
        g_dbus_proxy_call (keys->proxy,
			   "ReleaseMediaPlayerKeys",
			   g_variant_new ("(s)", "Evince"),
			   G_DBUS_CALL_FLAGS_NO_AUTO_START,
			   -1,
			   NULL, NULL, NULL);
}

static void
media_player_key_pressed_cb (GDBusProxy *proxy,
			     gchar      *sender_name,
			     gchar      *signal_name,
			     GVariant   *parameters,
			     gpointer    user_data)
{
        const char *application, *key;

        if (g_strcmp0 (sender_name, SD_NAME) != 0)
                return;

        if (g_strcmp0 (signal_name, "MediaPlayerKeyPressed") != 0)
                return;

        if (!g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(ss)")))
                return;

        g_variant_get (parameters, "(&s&s)", &application, &key);

        if (strcmp ("Evince", application) == 0) {
                g_signal_emit (user_data, signals[KEY_PRESSED], 0, key);
        }
}

static void
mediakeys_service_appeared_cb (GDBusConnection *connection,
			       const gchar     *name,
			       const gchar     *name_owner,
			       GDBusProxy      *proxy,
			       gpointer         user_data)
{
        EvMediaPlayerKeys *keys = EV_MEDIA_PLAYER_KEYS (user_data);

	keys->proxy = g_object_ref (proxy);
	g_signal_connect (keys->proxy, "g-signal",
			  G_CALLBACK (media_player_key_pressed_cb),
			  keys);

	ev_media_player_keys_grab_keys (keys);
}

static void
mediakeys_service_disappeared_cb (GDBusConnection *connection,
				  const gchar     *name,
				  gpointer         user_data)
{
        EvMediaPlayerKeys *keys = EV_MEDIA_PLAYER_KEYS (user_data);

	if (keys->proxy) {
		g_object_unref (keys->proxy);
		keys->proxy = NULL;
	}
}

static void
ev_media_player_keys_init (EvMediaPlayerKeys *keys)
{
	keys->watch_id = g_bus_watch_proxy (G_BUS_TYPE_SESSION,
					    SD_NAME,
					    G_BUS_NAME_WATCHER_FLAGS_NONE,
					    SD_OBJECT_PATH,
					    SD_INTERFACE,
					    G_TYPE_DBUS_PROXY,
					    G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
					    mediakeys_service_appeared_cb,
					    mediakeys_service_disappeared_cb,
					    keys, NULL);
}

void
ev_media_player_keys_focused (EvMediaPlayerKeys *keys)
{
	if (keys->proxy == NULL)
		return;
	
	ev_media_player_keys_grab_keys (keys);
}

static void
ev_media_player_keys_finalize (GObject *object)
{
	EvMediaPlayerKeys *keys = EV_MEDIA_PLAYER_KEYS (object);

        ev_media_player_keys_release_keys (keys);

	if (keys->watch_id > 0)
		g_bus_unwatch_name (keys->watch_id);

        if (keys->proxy != NULL)
                g_object_unref (keys->proxy);

	G_OBJECT_CLASS (ev_media_player_keys_parent_class)->finalize (object);
}

EvMediaPlayerKeys *
ev_media_player_keys_new (void)
{
	return g_object_new (EV_TYPE_MEDIA_PLAYER_KEYS, NULL);
}

