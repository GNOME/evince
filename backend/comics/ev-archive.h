/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/*
 * Copyright (C) 2017, Bastien Nocera <hadess@hadess.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define EV_TYPE_ARCHIVE ev_archive_get_type ()
G_DECLARE_FINAL_TYPE (EvArchive, ev_archive, EV, ARCHIVE, GObject)

typedef enum {
	EV_ARCHIVE_TYPE_NONE = 0,
	EV_ARCHIVE_TYPE_RAR,
	EV_ARCHIVE_TYPE_ZIP,
	EV_ARCHIVE_TYPE_7Z,
	EV_ARCHIVE_TYPE_TAR
} EvArchiveType;

EvArchive     *ev_archive_new                (void);
gboolean       ev_archive_set_archive_type   (EvArchive     *archive,
					      EvArchiveType  archive_type);
EvArchiveType  ev_archive_get_archive_type   (EvArchive     *archive);
gboolean       ev_archive_open_filename      (EvArchive     *archive,
					      const char    *path,
					      GError       **error);
gboolean       ev_archive_read_next_header   (EvArchive     *archive,
					      GError       **error);
gboolean       ev_archive_at_entry           (EvArchive     *archive);
const char    *ev_archive_get_entry_pathname (EvArchive     *archive);
gint64         ev_archive_get_entry_size     (EvArchive     *archive);
gboolean       ev_archive_get_entry_is_encrypted (EvArchive *archive);
gssize         ev_archive_read_data          (EvArchive     *archive,
					      void          *buf,
					      gsize          count,
					      GError       **error);
void           ev_archive_reset              (EvArchive     *archive);

G_END_DECLS
