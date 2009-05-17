/*
 *  Copyright (C) 2002 Jorn Baayen
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <errno.h>

#include "ev-file-helpers.h"

static gchar *tmp_dir = NULL;
static gint   count = 0;

gboolean
ev_dir_ensure_exists (const gchar *dir,
                      int          mode)
{
	if (g_mkdir_with_parents (dir, mode) == 0)
		return TRUE;

	if (errno == EEXIST)
		return g_file_test (dir, G_FILE_TEST_IS_DIR);
	
	g_warning ("Failed to create directory %s: %s", dir, g_strerror (errno));
	return FALSE;
}

const gchar *
ev_tmp_dir (void)
{
	if (tmp_dir == NULL) {
		gboolean exists;
		gchar   *dirname, *prgname;

                prgname = g_get_prgname ();
		dirname = g_strdup_printf ("%s-%u", prgname ? prgname : "unknown", getpid ());
		tmp_dir = g_build_filename (g_get_tmp_dir (),
					    dirname,
					    NULL);
		g_free (dirname);

		exists = ev_dir_ensure_exists (tmp_dir, 0700);
		g_assert (exists);
	}

	return tmp_dir;
}

void
_ev_file_helpers_init (void)
{
}

void
_ev_file_helpers_shutdown (void)
{	
	if (tmp_dir != NULL)	
		g_rmdir (tmp_dir);

	g_free (tmp_dir);
	tmp_dir = NULL;
}

GFile *
ev_tmp_file_get (const gchar *prefix)
{
	gchar *path;
	GFile *file;

	path = ev_tmp_filename (prefix);
	file = g_file_new_for_path (path);
	
	g_free (path);
	
	return file;
}

gchar * 
ev_tmp_filename (const gchar *prefix)
{
	gchar *basename;
	gchar *filename = NULL;

	do {
		if (filename != NULL)
			g_free (filename);
			
		basename = g_strdup_printf ("%s-%d",
					    prefix ? prefix : "document",
					    count ++);
		
		filename = g_build_filename (ev_tmp_dir (),
					     basename, NULL);
		
		g_free (basename);
	} while (g_file_test (filename, G_FILE_TEST_EXISTS));
			
	return filename;
}

gchar * 
ev_tmp_directory (const gchar *prefix) 
{
	return ev_tmp_filename (prefix ? prefix : "directory");
}

/* Remove a local temp file created by evince */
void
ev_tmp_filename_unlink (const gchar *filename)
{
	const gchar *tempdir;
	
	if (!filename)
		return;

	tempdir = g_get_tmp_dir ();
	if (g_ascii_strncasecmp (filename, tempdir, strlen (tempdir)) == 0) {
		g_unlink (filename);
	}
}

void
ev_tmp_file_unlink (GFile *file)
{
	gboolean res;
	GError  *error = NULL;

	if (!file)
		return;
	
	res = g_file_delete (file, NULL, &error);
	if (!res) {
		char *uri;
		
		uri = g_file_get_uri (file);
		g_warning ("Unable to delete temp file %s: %s\n", uri, error->message);
		g_free (uri);
		g_error_free (error);
	}
}

void
ev_tmp_uri_unlink (const gchar *uri)
{
	GFile *file;
	
	if (!uri)
		return;
	
	file = g_file_new_for_uri (uri);
	if (!g_file_is_native (file)) {
		g_warning ("Attempting to delete non native uri: %s\n", uri);
		g_object_unref (file);
		return;
	}
	
	ev_tmp_file_unlink (file);
	g_object_unref (file);
}

/**
 * ev_xfer_uri_simple:
 * @from: the source URI
 * @to: the target URI
 * @error: a #GError location to store an error, or %NULL
 *
 * Performs a g_file_copy() from @from to @to.
 *
 * Returns: %TRUE on success, or %FALSE on error with @error filled in
 */
gboolean
ev_xfer_uri_simple (const char *from,
		    const char *to,
		    GError     **error)
{
	GFile *source_file;
	GFile *target_file;
	gboolean result;
	
	if (!from)
		return TRUE;

        g_return_val_if_fail (to != NULL, TRUE);

	source_file = g_file_new_for_uri (from);
	target_file = g_file_new_for_uri (to);
	
	result = g_file_copy (source_file, target_file,
#if GLIB_CHECK_VERSION(2,19,0)
			      G_FILE_COPY_TARGET_DEFAULT_PERMS |
#endif
			      G_FILE_COPY_OVERWRITE,
			      NULL, NULL, NULL, error);

	g_object_unref (target_file);
	g_object_unref (source_file);
    
	return result;
}

static gchar *
get_mime_type_from_uri (const gchar *uri, GError **error)
{
	GFile       *file;
	GFileInfo   *file_info;
	const gchar *content_type;
        gchar       *mime_type = NULL;

	file = g_file_new_for_uri (uri);
	file_info = g_file_query_info (file,
				       G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				       0, NULL, error);
	g_object_unref (file);

	if (file_info == NULL)
		return NULL;

	content_type = g_file_info_get_content_type (file_info);
	if (content_type) {
                mime_type = g_content_type_get_mime_type (content_type);
        }

	g_object_unref (file_info);
	return mime_type;
}

static gchar *
get_mime_type_from_data (const gchar *uri, GError **error)
{
	GFile            *file;
	GFileInputStream *input_stream;
	gssize            size_read;
	guchar            buffer[1024];
	gboolean          retval;
	gchar            *content_type, *mime_type;

	file = g_file_new_for_uri (uri);
	
	input_stream = g_file_read (file, NULL, error);
	if (!input_stream) {
		g_object_unref (file);
		return NULL;
	}

	size_read = g_input_stream_read (G_INPUT_STREAM (input_stream),
					 buffer, sizeof (buffer), NULL, error);
	if (size_read == -1) {
		g_object_unref (input_stream);
		g_object_unref (file);
		return NULL;
	}

	retval = g_input_stream_close (G_INPUT_STREAM (input_stream), NULL, error);

	g_object_unref (input_stream);
	g_object_unref (file);
	if (!retval)
		return NULL;

	content_type = g_content_type_guess (NULL, /* no filename */
					     buffer, size_read,
					     NULL);
	if (!content_type)
		return NULL;

	mime_type = g_content_type_get_mime_type (content_type);
	g_free (content_type);
	return mime_type;
}

/**
 * ev_file_get_mime_type:
 * @uri: the URI
 * @fast: whether to use fast MIME type detection
 * @error: a #GError location to store an error, or %NULL
 *
 * Note: on unknown MIME types, this may return NULL without @error
 * being filled in.
 * 
 * Returns: a newly allocated string with the MIME type of the file at
 *   @uri, or %NULL on error or if the MIME type could not be determined
 */
gchar *
ev_file_get_mime_type (const gchar *uri,
		       gboolean     fast,
		       GError     **error)
{
	return fast ? get_mime_type_from_uri (uri, error) : get_mime_type_from_data (uri, error);
}

/* Compressed files support */
#define BZIPCOMMAND "bzip2"
#define GZIPCOMMAND "gzip"
#define N_ARGS      4
#define BUFFER_SIZE 1024

static gchar *
compression_run (const gchar       *uri,
		 EvCompressionType  type,
		 gboolean           compress, 
		 GError           **error)
{
	gchar *argv[N_ARGS];
	gchar *uri_dst = NULL;
	gchar *filename, *filename_dst;
	gchar *cmd;
	gint   fd, pout;
	GError *err = NULL;

	if (type == EV_COMPRESSION_NONE)
		return NULL;

	cmd = g_find_program_in_path ((type == EV_COMPRESSION_BZIP2) ? BZIPCOMMAND : GZIPCOMMAND);
	if (!cmd) {
		/* FIXME: better error codes! */
		/* FIXME: i18n later */
		g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
			     "Failed to find the \"%s\" command in the search path.",
			     type == EV_COMPRESSION_BZIP2 ? BZIPCOMMAND : GZIPCOMMAND);
		return NULL;
	}

	filename = g_filename_from_uri (uri, NULL, error);
	if (!filename) {
		g_free (cmd);
		return NULL;
	}
	
	filename_dst = g_build_filename (ev_tmp_dir (), "evinceXXXXXX", NULL);
	fd = g_mkstemp (filename_dst);
	if (fd < 0) {
		int errsv = errno;

		g_free (cmd);
		g_free (filename);
		g_free (filename_dst);

		g_set_error (error, G_IO_ERROR,
			     g_io_error_from_errno (errsv),
			     "Error creating a temporary file: %s",
			     g_strerror (errsv));
		return NULL;
	}

	argv[0] = cmd;
	argv[1] = compress ? "-c" : "-cd";
	argv[2] = filename;
	argv[3] = NULL;

	if (g_spawn_async_with_pipes (NULL, argv, NULL,
				      G_SPAWN_STDERR_TO_DEV_NULL,
				      NULL, NULL, NULL,
				      NULL, &pout, NULL, &err)) {
		GIOChannel *in, *out;
		gchar buf[BUFFER_SIZE];
		GIOStatus read_st, write_st;
		gsize bytes_read, bytes_written;

		in = g_io_channel_unix_new (pout);
		g_io_channel_set_encoding (in, NULL, NULL);
		out = g_io_channel_unix_new (fd);
		g_io_channel_set_encoding (out, NULL, NULL);

		do {
			read_st = g_io_channel_read_chars (in, buf,
							   BUFFER_SIZE,
							   &bytes_read,
							   error);
			if (read_st == G_IO_STATUS_NORMAL) {
				write_st = g_io_channel_write_chars (out, buf,
								     bytes_read,
								     &bytes_written,
								     error);
				if (write_st == G_IO_STATUS_ERROR)
					break;
			} else if (read_st == G_IO_STATUS_ERROR) {
				break;
			}
		} while (bytes_read > 0);

		g_io_channel_unref (in);
		g_io_channel_unref (out);
	}

	close (fd);

	if (err) {
		g_propagate_error (error, err);
	} else {
		uri_dst = g_filename_to_uri (filename_dst, NULL, error);
	}

	g_free (cmd);
	g_free (filename);
	g_free (filename_dst);

	return uri_dst;
}

/**
 * ev_file_uncompress:
 * @uri: a file URI
 * @type: the compression type
 * @error: a #GError location to store an error, or %NULL
 *
 * Uncompresses the file at @uri.
 *
 * If @type is %EV_COMPRESSION_NONE, it does nothing and returns %NULL.
 *
 * Otherwise, it returns the filename of a
 * temporary file containing the decompressed data from the file at @uri.
 * On error it returns %NULL and fills in @error.
 *
 * It is the caller's responsibility to unlink the temp file after use.
 *
 * Returns: a newly allocated string URI, or %NULL on error
 */
gchar *
ev_file_uncompress (const gchar       *uri,
		    EvCompressionType  type,
		    GError           **error)
{
	g_return_val_if_fail (uri != NULL, NULL);

	return compression_run (uri, type, FALSE, error);
}

/**
 * ev_file_compress:
 * @uri: a file URI
 * @type: the compression type
 * @error: a #GError location to store an error, or %NULL
 *
 * Compresses the file at @uri.
 
 * If @type is %EV_COMPRESSION_NONE, it does nothing and returns %NULL.
 *
 * Otherwise, it returns the filename of a
 * temporary file containing the compressed data from the file at @uri.
 *
 * On error it returns %NULL and fills in @error.
 *
 * It is the caller's responsibility to unlink the temp file after use.
 *
 * Returns: a newly allocated string URI, or %NULL on error
 */
gchar *
ev_file_compress (const gchar       *uri,
		  EvCompressionType  type,
		  GError           **error)
{
	g_return_val_if_fail (uri != NULL, NULL);

	return compression_run (uri, type, TRUE, error);
}
