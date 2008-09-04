/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * Copyright (C) 2007 Jan Arne Petersen <jap@gnome.org>
 * Copyright (C) 2008 Bastien Nocera <hadess@hadess.net>
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

#include <glib.h>
#include <dbus/dbus-glib.h>
#include <string.h>

#include "ev-media-player-keys.h"

#include "ev-marshal.h"

enum {
	KEY_PRESSED,
	LAST_SIGNAL
};

struct _EvMediaPlayerKeys
{
	GObject        parent;
	
	DBusGProxy    *proxy;
};

struct _EvMediaPlayerKeysClass
{
	GObjectClass parent_class;

	/* Signals */
	void (* key_pressed) (EvMediaPlayerKeys *keys,
			      const gchar       *key);
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (EvMediaPlayerKeys, ev_media_player_keys, G_TYPE_OBJECT)

static void ev_media_player_keys_finalize (GObject *object);

static void
ev_media_player_keys_class_init (EvMediaPlayerKeysClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	signals[KEY_PRESSED] =
		g_signal_new ("key_pressed",
			      EV_TYPE_MEDIA_PLAYER_KEYS,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EvMediaPlayerKeysClass, key_pressed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1, G_TYPE_STRING);
	
	object_class->finalize = ev_media_player_keys_finalize;
}

static void
on_media_player_key_pressed (DBusGProxy        *proxy,
			     const gchar       *application,
			     const gchar       *key,
			     EvMediaPlayerKeys *keys)
{
	if (strcmp ("Evince", application) == 0) {
		g_signal_emit (keys, signals[KEY_PRESSED], 0, key);
	}
}

static void
ev_media_player_keys_grab_keys (EvMediaPlayerKeys *keys)
{
	/*
	 * The uint as second argument is time. We give a very low value so that
	 * if a media player is there it gets higher priority on the keys (0 is
	 * a special value having maximum priority).
	 */
	dbus_g_proxy_call (keys->proxy,
			   "GrabMediaPlayerKeys", NULL,
			   G_TYPE_STRING, "Evince",
			   G_TYPE_UINT, 1,
			   G_TYPE_INVALID, G_TYPE_INVALID);
}

static void
ev_media_player_keys_release_keys (EvMediaPlayerKeys *keys)
{
	dbus_g_proxy_call (keys->proxy,
			   "ReleaseMediaPlayerKeys", NULL,
			   G_TYPE_STRING, "Evince",
			   G_TYPE_INVALID, G_TYPE_INVALID);
}

static void
ev_media_player_keys_init (EvMediaPlayerKeys *keys)
{
	DBusGConnection *connection;
	GError *err = NULL;

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &err);
	if (connection == NULL) {
		g_warning ("Error connecting to D-Bus: %s", err->message);
		return;
	}

	/* Try the gnome-settings-daemon version,
	 * then the gnome-control-center version of things */
	keys->proxy = dbus_g_proxy_new_for_name_owner (connection,
						       "org.gnome.SettingsDaemon",
						       "/org/gnome/SettingsDaemon/MediaKeys",
						       "org.gnome.SettingsDaemon.MediaKeys",
						       NULL);
	if (keys->proxy == NULL) {
		keys->proxy = dbus_g_proxy_new_for_name_owner (connection,
							       "org.gnome.SettingsDaemon",
							       "/org/gnome/SettingsDaemon",
							       "org.gnome.SettingsDaemon",
							       &err);
	}

	dbus_g_connection_unref (connection);
	if (err != NULL) {
		g_warning ("Failed to create dbus proxy for org.gnome.SettingsDaemon: %s",
			   err->message);
		g_error_free (err);
		
		if (keys->proxy) {
			g_object_unref (keys->proxy);
			keys->proxy = NULL;
		}
		
		return;
	}

	g_object_add_weak_pointer (G_OBJECT (keys->proxy),
				   (gpointer) &(keys->proxy));

	ev_media_player_keys_grab_keys (keys);

	dbus_g_object_register_marshaller (ev_marshal_VOID__STRING_STRING,
					   G_TYPE_NONE, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (keys->proxy, "MediaPlayerKeyPressed",
				 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);

	dbus_g_proxy_connect_signal (keys->proxy, "MediaPlayerKeyPressed",
				     G_CALLBACK (on_media_player_key_pressed), keys, NULL);
}

void
ev_media_player_keys_focused (EvMediaPlayerKeys *keys)
{
	if (!keys->proxy)
		return;
	
	ev_media_player_keys_grab_keys (keys);
}

static void
ev_media_player_keys_finalize (GObject *object)
{
	EvMediaPlayerKeys *keys = EV_MEDIA_PLAYER_KEYS (object);

	if (keys->proxy) {
		ev_media_player_keys_release_keys (keys);
		g_object_unref (keys->proxy);
		keys->proxy = NULL;
	}

	G_OBJECT_CLASS (ev_media_player_keys_parent_class)->finalize (object);
}

EvMediaPlayerKeys *
ev_media_player_keys_new (void)
{
	return g_object_new (EV_TYPE_MEDIA_PLAYER_KEYS, NULL);
}

