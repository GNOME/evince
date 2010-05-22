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

#ifndef EV_MEDIA_PLAYER_KEYS_H
#define EV_MEDIA_PLAYER_KEYS_H

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define EV_TYPE_MEDIA_PLAYER_KEYS		(ev_media_player_keys_get_type ())
#define EV_MEDIA_PLAYER_KEYS(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), EV_TYPE_MEDIA_PLAYER_KEYS, EvMediaPlayerKeys))
#define EV_MEDIA_PLAYER_KEYS_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), EV_TYPE_MEDIA_PLAYER_KEYS, EvMediaPlayerKeysClass))
#define EV_IS_MEDIA_PLAYER_KEYS(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), EV_TYPE_MEDIA_PLAYER_KEYS))
#define EV_IS_MEDIA_PLAYER_KEYS_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), EV_TYPE_MEDIA_PLAYER_KEYS))
#define EV_MEDIA_PLAYER_KEYS_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), EV_TYPE_MEDIA_PLAYER_KEYS, EvMediaPlayerKeysClass))

typedef struct _EvMediaPlayerKeys EvMediaPlayerKeys;
typedef struct _EvMediaPlayerKeysClass EvMediaPlayerKeysClass;


GType	           ev_media_player_keys_get_type  (void) G_GNUC_CONST;

EvMediaPlayerKeys *ev_media_player_keys_new	  (void);

void               ev_media_player_keys_focused	  (EvMediaPlayerKeys *keys);

G_END_DECLS

#endif /* !EV_MEDIA_PLAYER_KEYS_H */
