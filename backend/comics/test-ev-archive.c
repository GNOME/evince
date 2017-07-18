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

static void
usage (const char *prog)
{
	g_print ("- Lists file in a supported archive format\n");
	g_print ("Usage: %s archive-type filename\n", prog);
	g_print ("Where archive-type is one of rar, zip, 7z or tar\n");
}

static EvArchiveType
str_to_archive_type (const char *str)
{
	g_return_val_if_fail (str != NULL, EV_ARCHIVE_TYPE_NONE);

	if (g_strcmp0 (str, "rar") == 0)
		return EV_ARCHIVE_TYPE_RAR;
	if (g_strcmp0 (str, "zip") == 0)
		return EV_ARCHIVE_TYPE_ZIP;
	if (g_strcmp0 (str, "7z") == 0)
		return EV_ARCHIVE_TYPE_7Z;
	if (g_strcmp0 (str, "tar") == 0)
		return EV_ARCHIVE_TYPE_TAR;

	g_warning ("Archive type '%s' not supported", str);
	return EV_ARCHIVE_TYPE_NONE;
}

int
main (int argc, char **argv)
{
	EvArchive *ar;
	EvArchiveType ar_type;
	GError *error = NULL;
	gboolean printed_header = FALSE;

	if (argc != 3) {
		usage (argv[0]);
		return 1;
	}

	ar_type = str_to_archive_type (argv[1]);
	if (ar_type == EV_ARCHIVE_TYPE_NONE)
		return 1;

	ar = ev_archive_new ();
	if (!ev_archive_set_archive_type (ar, ar_type)) {
		g_warning ("Failed to set archive type");
		goto out;
	}

	if (!ev_archive_open_filename (ar, argv[2], &error)) {
		g_warning ("Failed to open '%s': %s",
			   argv[2], error->message);
		g_error_free (error);
		goto out;
	}

	while (1) {
		const char *name;
		gboolean is_encrypted;
		gint64 size;

		if (!ev_archive_read_next_header (ar, &error)) {
			if (error != NULL) {
				g_warning ("Fatal error handling archive: %s", error->message);
				g_clear_error (&error);
				goto out;
			}
			break;
		}

		name = ev_archive_get_entry_pathname (ar);
		is_encrypted = ev_archive_get_entry_is_encrypted (ar);
		size = ev_archive_get_entry_size (ar);

		if (!printed_header) {
			g_print ("P\tSIZE\tNAME\n");
			printed_header = TRUE;
		}

		g_print ("%c\t%"G_GINT64_FORMAT"\t%s\n",
			 is_encrypted ? 'P' : ' ',
			 size, name);
	}

	ev_archive_reset (ar);
	g_clear_object (&ar);

	return 0;

out:
	g_clear_object (&ar);
	return 1;
}
