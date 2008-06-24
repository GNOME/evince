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
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <gmodule.h>
#include <dbus/dbus-glib.h>
#include <string.h>

#include "ev-media-player-keys.h"

#include "ev-marshal.h"

struct _EvMediaPlayerKeys
{
	GObject        parent;
	DBusGProxy    *media_player_keys_proxy;
	EvWindow      *window;
};

struct _EvMediaPlayerKeysClass
{
	GObjectClass parent_class;
};

G_DEFINE_TYPE (EvMediaPlayerKeys, ev_media_player_keys, G_TYPE_OBJECT)

static void ev_media_player_keys_init		(EvMediaPlayerKeys *keys);
static void ev_media_player_keys_finalize	(GObject *object);

static void
ev_media_player_keys_class_init (EvMediaPlayerKeysClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = ev_media_player_keys_finalize;
}

static void
proxy_destroy (DBusGProxy *proxy,
	       EvMediaPlayerKeys* keys)
{
	keys->media_player_keys_proxy = NULL;
}

static void
on_media_player_key_pressed (DBusGProxy *proxy, const gchar *application, const gchar *key, EvMediaPlayerKeys *keys)
{
	if (strcmp ("Evince", application) == 0 && keys->window != NULL) {
		/* Note how Previous/Next only go to the
		 * next/previous page despite their icon telling you
		 * they should go to the beginning/end.
		 *
		 * There's very few keyboards with FFW/RWD though,
		 * so we stick the most useful keybinding on the most
		 * often seen keys
		 */
		if (strcmp ("Play", key) == 0) {
			ev_window_start_presentation (keys->window);
		} else if (strcmp ("Previous", key) == 0) {
			ev_window_go_previous_page (keys->window);
		} else if (strcmp ("Next", key) == 0) {
			ev_window_go_next_page (keys->window);
		} else if (strcmp ("FastForward", key) == 0) {
			ev_window_go_last_page (keys->window);
		} else if (strcmp ("Rewind", key) == 0) {
			ev_window_go_first_page (keys->window);
		}
	}
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
	keys->media_player_keys_proxy = dbus_g_proxy_new_for_name_owner (connection,
								       "org.gnome.SettingsDaemon",
								       "/org/gnome/SettingsDaemon/MediaKeys",
								       "org.gnome.SettingsDaemon.MediaKeys",
								       NULL);
	if (keys->media_player_keys_proxy == NULL) {
		keys->media_player_keys_proxy = dbus_g_proxy_new_for_name_owner (connection,
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
		return;
	} else {
		g_signal_connect_object (keys->media_player_keys_proxy,
					 "destroy",
					 G_CALLBACK (proxy_destroy),
					 keys, 0);
	}

	dbus_g_proxy_call (keys->media_player_keys_proxy,
			   "GrabMediaPlayerKeys", NULL,
			   G_TYPE_STRING, "Evince", G_TYPE_UINT, 0, G_TYPE_INVALID,
			   G_TYPE_INVALID);

	dbus_g_object_register_marshaller (ev_marshal_VOID__STRING_STRING,
					   G_TYPE_NONE, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_add_signal (keys->media_player_keys_proxy, "MediaPlayerKeyPressed",
				 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);

	dbus_g_proxy_connect_signal (keys->media_player_keys_proxy, "MediaPlayerKeyPressed",
				     G_CALLBACK (on_media_player_key_pressed), keys, NULL);
}

void
ev_media_player_keys_focused (EvMediaPlayerKeys *keys, EvWindow *window)
{
	if (keys->media_player_keys_proxy != NULL) {
		if (keys->window != NULL) {
			g_object_unref (keys->window);
			keys->window = NULL;
		}
		if (window != NULL) {
			dbus_g_proxy_call (keys->media_player_keys_proxy,
					   "GrabMediaPlayerKeys", NULL,
					   G_TYPE_STRING, "Evince", G_TYPE_UINT, 0, G_TYPE_INVALID,
					   G_TYPE_INVALID);
			keys->window = g_object_ref (window);
		}
	}
}

static void
ev_media_player_keys_finalize (GObject *object)
{
	EvMediaPlayerKeys *keys = EV_MEDIA_PLAYER_KEYS (object);

	if (keys->media_player_keys_proxy != NULL) {
		dbus_g_proxy_call (keys->media_player_keys_proxy,
				   "ReleaseMediaPlayerKeys", NULL,
				   G_TYPE_STRING, "Ev", G_TYPE_INVALID, G_TYPE_INVALID);
		g_object_unref (keys->media_player_keys_proxy);
		keys->media_player_keys_proxy = NULL;
	}

	if (keys->window != NULL) {
		g_object_unref (keys->window);
		keys->window = NULL;
	}

	G_OBJECT_CLASS (ev_media_player_keys_parent_class)->finalize (object);
}

EvMediaPlayerKeys *
ev_media_player_keys_new (void)
{
	return g_object_new (EV_TYPE_MEDIA_PLAYER_KEYS, NULL);
}

