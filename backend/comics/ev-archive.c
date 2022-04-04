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

#include "config.h"
#include "ev-archive.h"

#include <archive.h>
#include <archive_entry.h>
#include <gio/gio.h>

#define BUFFER_SIZE (64 * 1024)

struct _EvArchive {
	GObject parent_instance;
	EvArchiveType type;

	/* libarchive */
	struct archive *libar;
	struct archive_entry *libar_entry;
};

G_DEFINE_TYPE(EvArchive, ev_archive, G_TYPE_OBJECT);

static void
ev_archive_finalize (GObject *object)
{
	EvArchive *archive = EV_ARCHIVE (object);

	switch (archive->type) {
	case EV_ARCHIVE_TYPE_RAR:
	case EV_ARCHIVE_TYPE_ZIP:
	case EV_ARCHIVE_TYPE_7Z:
	case EV_ARCHIVE_TYPE_TAR:
		g_clear_pointer (&archive->libar, archive_free);
		break;
	default:
		break;
	}

	G_OBJECT_CLASS (ev_archive_parent_class)->finalize (object);
}

static void
ev_archive_class_init (EvArchiveClass *klass)
{
        GObjectClass *object_class = (GObjectClass *) klass;

        object_class->finalize = ev_archive_finalize;
}

EvArchive *
ev_archive_new (void)
{
	return g_object_new (EV_TYPE_ARCHIVE, NULL);
}

static void
libarchive_set_archive_type (EvArchive *archive,
			     EvArchiveType archive_type)
{
	archive->type = archive_type;
	archive->libar = archive_read_new ();

	if (archive_type == EV_ARCHIVE_TYPE_ZIP)
		archive_read_support_format_zip (archive->libar);
	else if (archive_type == EV_ARCHIVE_TYPE_7Z)
		archive_read_support_format_7zip (archive->libar);
	else if (archive_type == EV_ARCHIVE_TYPE_TAR)
		archive_read_support_format_tar (archive->libar);
	else if (archive_type == EV_ARCHIVE_TYPE_RAR) {
		archive_read_support_format_rar (archive->libar);
		archive_read_support_format_rar5 (archive->libar);
	} else
		g_assert_not_reached ();
}

EvArchiveType
ev_archive_get_archive_type (EvArchive *archive)
{
	g_return_val_if_fail (EV_IS_ARCHIVE (archive), EV_ARCHIVE_TYPE_NONE);

	return archive->type;
}

gboolean
ev_archive_set_archive_type (EvArchive *archive,
			     EvArchiveType archive_type)
{
	g_return_val_if_fail (EV_IS_ARCHIVE (archive), FALSE);
	g_return_val_if_fail (archive->type == EV_ARCHIVE_TYPE_NONE, FALSE);

	switch (archive_type) {
	case EV_ARCHIVE_TYPE_RAR:
	case EV_ARCHIVE_TYPE_ZIP:
	case EV_ARCHIVE_TYPE_7Z:
	case EV_ARCHIVE_TYPE_TAR:
		libarchive_set_archive_type (archive, archive_type);
		break;
	default:
		g_assert_not_reached ();
	}

	return TRUE;
}

gboolean
ev_archive_open_filename (EvArchive   *archive,
			  const char  *path,
			  GError     **error)
{
	int r;

	g_return_val_if_fail (EV_IS_ARCHIVE (archive), FALSE);
	g_return_val_if_fail (archive->type != EV_ARCHIVE_TYPE_NONE, FALSE);
	g_return_val_if_fail (path != NULL, FALSE);

	switch (archive->type) {
	case EV_ARCHIVE_TYPE_NONE:
		g_assert_not_reached ();
	case EV_ARCHIVE_TYPE_RAR:
	case EV_ARCHIVE_TYPE_ZIP:
	case EV_ARCHIVE_TYPE_7Z:
	case EV_ARCHIVE_TYPE_TAR:
		r = archive_read_open_filename (archive->libar, path, BUFFER_SIZE);
		if (r != ARCHIVE_OK) {
			g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
				     "Error opening archive: %s", archive_error_string (archive->libar));
			return FALSE;
		}
		return TRUE;
	}

	return FALSE;
}

static gboolean
libarchive_read_next_header (EvArchive *archive,
			     GError   **error)
{
	while (1) {
		int r;

		r = archive_read_next_header (archive->libar, &archive->libar_entry);
		if (r != ARCHIVE_OK) {
			if (r != ARCHIVE_EOF)
				g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
					     "Error reading archive: %s", archive_error_string (archive->libar));
			return FALSE;
		}

		if (archive_entry_filetype (archive->libar_entry) != AE_IFREG) {
			g_debug ("Skipping '%s' as it's not a regular file",
				 archive_entry_pathname (archive->libar_entry));
			continue;
		}

		g_debug ("At header for file '%s'", archive_entry_pathname (archive->libar_entry));

		break;
	}

	return TRUE;
}

gboolean
ev_archive_read_next_header (EvArchive *archive,
			     GError   **error)
{
	g_return_val_if_fail (EV_IS_ARCHIVE (archive), FALSE);
	g_return_val_if_fail (archive->type != EV_ARCHIVE_TYPE_NONE, FALSE);

	switch (archive->type) {
	case EV_ARCHIVE_TYPE_NONE:
		g_assert_not_reached ();
	case EV_ARCHIVE_TYPE_RAR:
	case EV_ARCHIVE_TYPE_ZIP:
	case EV_ARCHIVE_TYPE_7Z:
	case EV_ARCHIVE_TYPE_TAR:
		return libarchive_read_next_header (archive, error);
	}

	return FALSE;
}

gboolean
ev_archive_at_entry (EvArchive *archive)
{
	g_return_val_if_fail (EV_IS_ARCHIVE (archive), FALSE);
	g_return_val_if_fail (archive->type != EV_ARCHIVE_TYPE_NONE, FALSE);

	return (archive->libar_entry != NULL);
}

const char *
ev_archive_get_entry_pathname (EvArchive *archive)
{
	g_return_val_if_fail (EV_IS_ARCHIVE (archive), NULL);
	g_return_val_if_fail (archive->type != EV_ARCHIVE_TYPE_NONE, NULL);

	switch (archive->type) {
	case EV_ARCHIVE_TYPE_NONE:
		g_assert_not_reached ();
	case EV_ARCHIVE_TYPE_RAR:
	case EV_ARCHIVE_TYPE_ZIP:
	case EV_ARCHIVE_TYPE_7Z:
	case EV_ARCHIVE_TYPE_TAR:
		g_return_val_if_fail (archive->libar_entry != NULL, NULL);
		return archive_entry_pathname (archive->libar_entry);
	}

	return NULL;
}

gint64
ev_archive_get_entry_size (EvArchive *archive)
{
	g_return_val_if_fail (EV_IS_ARCHIVE (archive), -1);
	g_return_val_if_fail (archive->type != EV_ARCHIVE_TYPE_NONE, -1);

	switch (archive->type) {
	case EV_ARCHIVE_TYPE_NONE:
		g_assert_not_reached ();
	case EV_ARCHIVE_TYPE_RAR:
	case EV_ARCHIVE_TYPE_ZIP:
	case EV_ARCHIVE_TYPE_7Z:
	case EV_ARCHIVE_TYPE_TAR:
		g_return_val_if_fail (archive->libar_entry != NULL, -1);
		return archive_entry_size (archive->libar_entry);
	}

	return -1;
}

gboolean
ev_archive_get_entry_is_encrypted (EvArchive *archive)
{
	g_return_val_if_fail (EV_IS_ARCHIVE (archive), FALSE);
	g_return_val_if_fail (archive->type != EV_ARCHIVE_TYPE_NONE, FALSE);

	switch (archive->type) {
	case EV_ARCHIVE_TYPE_NONE:
		g_assert_not_reached ();
	case EV_ARCHIVE_TYPE_RAR:
	case EV_ARCHIVE_TYPE_ZIP:
	case EV_ARCHIVE_TYPE_7Z:
	case EV_ARCHIVE_TYPE_TAR:
		g_return_val_if_fail (archive->libar_entry != NULL, -1);
		return archive_entry_is_encrypted (archive->libar_entry);
	}

	return FALSE;
}

gssize
ev_archive_read_data (EvArchive *archive,
		      void      *buf,
		      gsize      count,
		      GError   **error)
{
	gssize r = -1;

	g_return_val_if_fail (EV_IS_ARCHIVE (archive), -1);
	g_return_val_if_fail (archive->type != EV_ARCHIVE_TYPE_NONE, -1);

	switch (archive->type) {
	case EV_ARCHIVE_TYPE_NONE:
		g_assert_not_reached ();
	case EV_ARCHIVE_TYPE_RAR:
	case EV_ARCHIVE_TYPE_ZIP:
	case EV_ARCHIVE_TYPE_7Z:
	case EV_ARCHIVE_TYPE_TAR:
		g_return_val_if_fail (archive->libar_entry != NULL, -1);
		r = archive_read_data (archive->libar, buf, count);
		if (r < 0) {
			g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
				     "Failed to decompress data: %s", archive_error_string (archive->libar));
		}
		break;
	}

	return r;
}

void
ev_archive_reset (EvArchive *archive)
{
	g_return_if_fail (EV_IS_ARCHIVE (archive));
	g_return_if_fail (archive->type != EV_ARCHIVE_TYPE_NONE);

	switch (archive->type) {
	case EV_ARCHIVE_TYPE_RAR:
	case EV_ARCHIVE_TYPE_ZIP:
	case EV_ARCHIVE_TYPE_7Z:
	case EV_ARCHIVE_TYPE_TAR:
		g_clear_pointer (&archive->libar, archive_free);
		libarchive_set_archive_type (archive, archive->type);
		archive->libar_entry = NULL;
		break;
	default:
		g_assert_not_reached ();
	}
}

static void
ev_archive_init (EvArchive *archive)
{
}
