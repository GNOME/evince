/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/*
 * Copyright (C) 2009-2010 Juanjo Marín <juanj.marin@juntadeandalucia.es>
 * Copyright (C) 2005, Teemu Tervo <teemu.tervo@gmx.net>
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

#include <config.h>

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#ifdef G_OS_WIN32
# define WIFEXITED(x) ((x) != 3)
# define WEXITSTATUS(x) (x)
#else
# include <sys/wait.h>
#endif

#include "comics-document.h"
#include "ev-document-misc.h"
#include "ev-file-helpers.h"

#ifdef G_OS_WIN32
/* On windows g_spawn_command_line_sync reads stdout in O_BINARY mode, not in O_TEXT mode.
 * As a consequence, newlines are in a platform dependent representation (\r\n). This
 * might be considered a bug in glib.
 */
#define EV_EOL "\r\n"
#else
#define EV_EOL "\n"
#endif

typedef enum
{
	RARLABS,
	GNAUNRAR,
	UNZIP,
	P7ZIP,
	TAR
} ComicBookDecompressType;

typedef struct _ComicsDocumentClass ComicsDocumentClass;

struct _ComicsDocumentClass
{
	EvDocumentClass parent_class;
};

struct _ComicsDocument
{
	EvDocument parent_instance;

	gchar    *archive, *dir;
	GPtrArray *page_names;
	gchar    *selected_command, *alternative_command;
	gchar    *extract_command, *list_command, *decompress_tmp;
	gboolean regex_arg;
	gint     offset;
	ComicBookDecompressType command_usage;
};

#define OFFSET_7Z 53
#define OFFSET_ZIP 2
#define NO_OFFSET 0

/* For perfomance reasons of 7z* we've choosen to decompress on the temporary 
 * directory instead of decompressing on the stdout */

/**
 * @extract: command line arguments to pass to extract a file from the archive
 *   to stdout.
 * @list: command line arguments to list the archive contents
 * @decompress_tmp: command line arguments to pass to extract the archive
 *   into a directory.
 * @regex_arg: whether the command can accept regex expressions
 * @offset: the position offset of the filename on each line in the output of
 *   running the @list command
 */
typedef struct {
        char *extract;
        char *list;
        char *decompress_tmp;
        gboolean regex_arg;
        gint offset;
} ComicBookDecompressCommand;

static const ComicBookDecompressCommand command_usage_def[] = {
        /* RARLABS unrar */
	{"%s p -c- -ierr --", "%s vb -c- -- %s", NULL             , FALSE, NO_OFFSET},

        /* GNA! unrar */
	{NULL               , "%s t %s"        , "%s -xf %s %s"   , FALSE, NO_OFFSET},

        /* unzip */
	{"%s -p -C --"      , "%s %s"          , NULL             , TRUE , OFFSET_ZIP},

        /* 7zip */
	{NULL               , "%s l -- %s"     , "%s x -y %s -o%s", FALSE, OFFSET_7Z},

        /* tar */
	{"%s -xOf"          , "%s -tf %s"      , NULL             , FALSE, NO_OFFSET}
};

static GSList*    get_supported_image_extensions (void);
static void       get_page_size_area_prepared_cb (GdkPixbufLoader *loader,
						  gpointer data);
static void       render_pixbuf_size_prepared_cb (GdkPixbufLoader *loader,
						  gint width,
						  gint height,
						  gpointer data);
static char**     extract_argv                   (EvDocument *document,
						  gint page);


EV_BACKEND_REGISTER (ComicsDocument, comics_document)

/**
 * comics_regex_quote:
 * @unquoted_string: a literal string
 *
 * Quotes a string so unzip will not interpret the regex expressions of
 * @unquoted_string. Basically, this functions uses [] to disable regex 
 * expressions. The return value must be freed with * g_free()
 *
 * Return value: quoted and disabled-regex string
 **/
static gchar *
comics_regex_quote (const gchar *unquoted_string)
{
	const gchar *p;
	GString *dest;

	dest = g_string_new ("'");

	p = unquoted_string;

	while (*p) {
		switch (*p) {
			/* * matches a sequence of 0 or more characters */
			case ('*'):
			/* ? matches exactly 1 charactere */
			case ('?'):
			/* [...]  matches any single character found inside
			 * the brackets. Disabling the first bracket is enough.
			 */
			case ('['):
				g_string_append (dest, "[");
				g_string_append_c (dest, *p);
				g_string_append (dest, "]");
				break;
			/* Because \ escapes regex expressions that we are
			 * disabling for unzip, we need to disable \ too */
			case ('\\'):
				g_string_append (dest, "[\\\\]");
				break;
			/* Escape single quote inside the string */
			case ('\''):
				g_string_append (dest, "'\\''");
				break;
			default:
				g_string_append_c (dest, *p);
				break;
		}
		++p;
	}
	g_string_append_c (dest, '\'');
	return g_string_free (dest, FALSE);
}


/* This function manages the command for decompressing a comic book */
static gboolean 
comics_decompress_temp_dir (const gchar *command_decompress_tmp,
			    const gchar *command, 
			    GError      **error)
{
	gboolean success;
	gchar *std_out, *basename;
	GError *err = NULL;
	gint retval;
	
	success = g_spawn_command_line_sync (command_decompress_tmp, &std_out, 
					     NULL, &retval, &err);
	basename = g_path_get_basename (command);
	if (!success) {
		g_set_error (error,
			     EV_DOCUMENT_ERROR, 
			     EV_DOCUMENT_ERROR_INVALID,
			     _("Error launching the command “%s” in order to "
			     "decompress the comic book: %s"),
			     basename,
			     err->message);
		g_error_free (err);
	} else if (WIFEXITED (retval)) {
		if (WEXITSTATUS (retval) == EXIT_SUCCESS) {
			g_free (std_out);
			g_free (basename);
			return TRUE;
		} else {
			g_set_error (error,
				     EV_DOCUMENT_ERROR,
				     EV_DOCUMENT_ERROR_INVALID,
				     _("The command “%s” failed at "
				     "decompressing the comic book."),
				     basename);
			g_free (std_out);
		}
	} else {
		g_set_error (error,
			     EV_DOCUMENT_ERROR,
			     EV_DOCUMENT_ERROR_INVALID,
			     _("The command “%s” did not end normally."),
			     basename);
		g_free (std_out);
	}
	g_free (basename);
	return FALSE;
}

/* This function shows how to use the choosen command for decompressing a
 * comic book file. It modifies fields of the ComicsDocument struct with 
 * this information */
static gboolean 
comics_generate_command_lines (ComicsDocument *comics_document, 
			       GError         **error)
{
	gchar *quoted_file, *quoted_file_aux;
	gchar *quoted_command;
	ComicBookDecompressType type;
	
	type = comics_document->command_usage;
	comics_document->regex_arg = command_usage_def[type].regex_arg;
	quoted_command = g_shell_quote (comics_document->selected_command);
	if (comics_document->regex_arg) {
		quoted_file = comics_regex_quote (comics_document->archive);
		quoted_file_aux = g_shell_quote (comics_document->archive);
		comics_document->list_command =
			   g_strdup_printf (command_usage_def[type].list,
			                    comics_document->alternative_command,
			                    quoted_file_aux);
		g_free (quoted_file_aux);
	} else {
		quoted_file = g_shell_quote (comics_document->archive);
		comics_document->list_command =
				g_strdup_printf (command_usage_def[type].list,
				                 quoted_command, quoted_file);
	}
	comics_document->extract_command =
			    g_strdup_printf (command_usage_def[type].extract,
				             quoted_command);
	comics_document->offset = command_usage_def[type].offset;
	if (command_usage_def[type].decompress_tmp) {
		comics_document->dir = ev_mkdtemp ("evince-comics-XXXXXX", error);
                if (comics_document->dir == NULL)
                        return FALSE;

		/* unrar-free can't create directories, but ev_mkdtemp already created the dir */

		comics_document->decompress_tmp =
			g_strdup_printf (command_usage_def[type].decompress_tmp, 
					 quoted_command, quoted_file,
					 comics_document->dir);
		g_free (quoted_file);
		g_free (quoted_command);

		if (!comics_decompress_temp_dir (comics_document->decompress_tmp,
		    comics_document->selected_command, error))
			return FALSE;
		else
			return TRUE;
	} else {
		g_free (quoted_file);
		g_free (quoted_command);
		return TRUE;
	}

}

/* This function chooses an external command for decompressing a comic 
 * book based on its mime tipe. */
static gboolean 
comics_check_decompress_command	(gchar          *mime_type, 
				 ComicsDocument *comics_document,
				 GError         **error)
{
	gboolean success;
	gchar *std_out, *std_err;
	gint retval;
	GError *err = NULL;
	
	/* FIXME, use proper cbr/cbz mime types once they're
	 * included in shared-mime-info */
	
	if (!strcmp (mime_type, "application/x-cbr") ||
	    !strcmp (mime_type, "application/x-rar")) {
	        /* The RARLAB provides a no-charge proprietary (freeware) 
	        * decompress-only client for Linux called unrar. Another 
		* option is a GPLv2-licensed command-line tool developed by 
		* the Gna! project. Confusingly enough, the free software RAR 
		* decoder is also named unrar. For this reason we need to add 
		* some lines for disambiguation. Sorry for the added the 
		* complexity but it's life :)
		* Finally, some distributions, like Debian, rename this free 
		* option as unrar-free. 
		* */
		comics_document->selected_command = 
					g_find_program_in_path ("unrar");
		if (comics_document->selected_command) {
			/* We only use std_err to avoid printing useless error 
			 * messages on the terminal */
			success = 
				g_spawn_command_line_sync (
				              comics_document->selected_command, 
							   &std_out, &std_err,
							   &retval, &err);
			if (!success) {
				g_propagate_error (error, err);
				g_error_free (err);
				return FALSE;
			/* I don't check retval status because RARLAB unrar 
			 * doesn't have a way to return 0 without involving an 
			 * operation with a file*/
			} else if (WIFEXITED (retval)) {
				if (g_strrstr (std_out,"freeware") != NULL)
					/* The RARLAB freeware client */
					comics_document->command_usage = RARLABS;
				else
					/* The Gna! free software client */
					comics_document->command_usage = GNAUNRAR;

				g_free (std_out);
				g_free (std_err);
				return TRUE;
			}
		}
		/* The Gna! free software client with Debian naming convention */
		comics_document->selected_command = 
				g_find_program_in_path ("unrar-free");
		if (comics_document->selected_command) {
			comics_document->command_usage = GNAUNRAR;
			return TRUE;
		}

	} else if (!strcmp (mime_type, "application/x-cbz") ||
		   !strcmp (mime_type, "application/zip")) {
		/* InfoZIP's unzip program */
		comics_document->selected_command = 
				g_find_program_in_path ("unzip");
		comics_document->alternative_command =
				g_find_program_in_path ("zipnote");
		if (comics_document->selected_command &&
		    comics_document->alternative_command) {
			comics_document->command_usage = UNZIP;
			return TRUE;
		}
		/* fallback mode using 7za and 7z from p7zip project  */
		comics_document->selected_command =
				g_find_program_in_path ("7za");
		if (comics_document->selected_command) {
			comics_document->command_usage = P7ZIP;
			return TRUE;
		}
		comics_document->selected_command =
				g_find_program_in_path ("7z");
		if (comics_document->selected_command) {
			comics_document->command_usage = P7ZIP;
			return TRUE;
		}

	} else if (!strcmp (mime_type, "application/x-cb7") ||
		   !strcmp (mime_type, "application/x-7z-compressed")) {
		/* 7zr, 7za and 7z are the commands from the p7zip project able 
		 * to decompress .7z files */ 
		comics_document->selected_command =
				g_find_program_in_path ("7zr");
		if (comics_document->selected_command) {
			comics_document->command_usage = P7ZIP;
			return TRUE;
		}
		comics_document->selected_command =
				g_find_program_in_path ("7za");
		if (comics_document->selected_command) {
			comics_document->command_usage = P7ZIP;
			return TRUE;
		}
		comics_document->selected_command =
				g_find_program_in_path ("7z");
		if (comics_document->selected_command) {
			comics_document->command_usage = P7ZIP;
			return TRUE;
		}
	} else if (!strcmp (mime_type, "application/x-cbt") ||
		   !strcmp (mime_type, "application/x-tar")) {
		/* tar utility (Tape ARchive) */
		comics_document->selected_command =
				g_find_program_in_path ("tar");
		if (comics_document->selected_command) {
			comics_document->command_usage = TAR;
			return TRUE;
		}
	} else {
		g_set_error (error,
			     EV_DOCUMENT_ERROR,
			     EV_DOCUMENT_ERROR_INVALID,
			     _("Not a comic book MIME type: %s"),
			     mime_type);
			     return FALSE;
	}
	g_set_error_literal (error,
			     EV_DOCUMENT_ERROR,
			     EV_DOCUMENT_ERROR_INVALID,
			     _("Can't find an appropriate command to "
			     "decompress this type of comic book"));
	return FALSE;
}

static int
sort_page_names (gconstpointer a,
                 gconstpointer b)
{
  return strcmp (* (const char **) a, * (const char **) b);
}

static gboolean
comics_document_load (EvDocument *document,
		      const char *uri,
		      GError    **error)
{
	ComicsDocument *comics_document = COMICS_DOCUMENT (document);
	GSList *supported_extensions;
	gchar *std_out;
	gchar *mime_type;
	gchar **cb_files, *cb_file;
	gboolean success;
	int i, retval;
	GError *err = NULL;

	comics_document->archive = g_filename_from_uri (uri, NULL, error);
	if (!comics_document->archive)
		return FALSE;

	mime_type = ev_file_get_mime_type (uri, FALSE, &err);
	if (mime_type == NULL)
		return FALSE;
	
	if (!comics_check_decompress_command (mime_type, comics_document, 
	error)) {	
		g_free (mime_type);
		return FALSE;
	} else if (!comics_generate_command_lines (comics_document, error)) {
		   g_free (mime_type);
		return FALSE;
	}

	g_free (mime_type);

	/* Get list of files in archive */
	success = g_spawn_command_line_sync (comics_document->list_command,
					     &std_out, NULL, &retval, error);

	if (!success) {
		return FALSE;
	} else if (!WIFEXITED(retval) || WEXITSTATUS(retval) != EXIT_SUCCESS) {
		g_set_error_literal (error,
                                     EV_DOCUMENT_ERROR,
                                     EV_DOCUMENT_ERROR_INVALID,
                                     _("File corrupted"));
		return FALSE;
	}

	/* FIXME: is this safe against filenames containing \n in the archive ? */
	cb_files = g_strsplit (std_out, EV_EOL, 0);

	g_free (std_out);

	if (!cb_files) {
		g_set_error_literal (error,
				     EV_DOCUMENT_ERROR,
				     EV_DOCUMENT_ERROR_INVALID,
				     _("No files in archive"));
		return FALSE;
	}

        comics_document->page_names = g_ptr_array_sized_new (64);

	supported_extensions = get_supported_image_extensions ();
	for (i = 0; cb_files[i] != NULL; i++) {
		if (comics_document->offset != NO_OFFSET) {
			if (g_utf8_strlen (cb_files[i],-1) > 
			    comics_document->offset) {
				cb_file = 
					g_utf8_offset_to_pointer (cb_files[i], 
						       comics_document->offset);
			} else {
				continue;
			}
		} else {
			cb_file = cb_files[i];
		}
		gchar *suffix = g_strrstr (cb_file, ".");
		if (!suffix)
			continue;
		suffix = g_ascii_strdown (suffix + 1, -1);
		if (g_slist_find_custom (supported_extensions, suffix,
					 (GCompareFunc) strcmp) != NULL) {
                        g_ptr_array_add (comics_document->page_names,
                                         g_strstrip (g_strdup (cb_file)));
		}
		g_free (suffix);
	}
	g_strfreev (cb_files);
	g_slist_foreach (supported_extensions, (GFunc) g_free, NULL);
	g_slist_free (supported_extensions);

	if (comics_document->page_names->len == 0) {
		g_set_error (error,
			     EV_DOCUMENT_ERROR,
			     EV_DOCUMENT_ERROR_INVALID,
			     _("No images found in archive %s"),
			     uri);
		return FALSE;
	}

        /* Now sort the pages */
        g_ptr_array_sort (comics_document->page_names, sort_page_names);

	return TRUE;
}


static gboolean
comics_document_save (EvDocument *document,
		      const char *uri,
		      GError    **error)
{
	ComicsDocument *comics_document = COMICS_DOCUMENT (document);

	return ev_xfer_uri_simple (comics_document->archive, uri, error);
}

static int
comics_document_get_n_pages (EvDocument *document)
{
	ComicsDocument *comics_document = COMICS_DOCUMENT (document);

        if (comics_document->page_names == NULL)
                return 0;

	return comics_document->page_names->len;
}

static void
comics_document_get_page_size (EvDocument *document,
			       EvPage     *page,
			       double     *width,
			       double     *height)
{
	GdkPixbufLoader *loader;
	char **argv;
	guchar buf[1024];
	gboolean success, got_size = FALSE;
	gint outpipe = -1;
	GPid child_pid;
	gssize bytes;
	GdkPixbuf *pixbuf;
	gchar *filename;
	ComicsDocument *comics_document = COMICS_DOCUMENT (document);
	
	if (!comics_document->decompress_tmp) {
		argv = extract_argv (document, page->index);
		success = g_spawn_async_with_pipes (NULL, argv, NULL,
						    G_SPAWN_SEARCH_PATH | 
						    G_SPAWN_STDERR_TO_DEV_NULL,
						    NULL, NULL,
						    &child_pid,
						    NULL, &outpipe, NULL, NULL);
		g_strfreev (argv);
		g_return_if_fail (success == TRUE);

		loader = gdk_pixbuf_loader_new ();
		g_signal_connect (loader, "area-prepared",
				  G_CALLBACK (get_page_size_area_prepared_cb),
				  &got_size);

		while (outpipe >= 0) {
			bytes = read (outpipe, buf, 1024);
		
			if (bytes > 0)
			gdk_pixbuf_loader_write (loader, buf, bytes, NULL);
			if (bytes <= 0 || got_size) {
				close (outpipe);
				outpipe = -1;
				gdk_pixbuf_loader_close (loader, NULL);
			}
		}
		pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
		if (pixbuf) {
			if (width)
				*width = gdk_pixbuf_get_width (pixbuf);
			if (height)
				*height = gdk_pixbuf_get_height (pixbuf);
		}
		g_spawn_close_pid (child_pid);
		g_object_unref (loader);
	} else {
		filename = g_build_filename (comics_document->dir,
                                             (char *) comics_document->page_names->pdata[page->index],
					     NULL);
		pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
		if (pixbuf) {
			if (width)
				*width = gdk_pixbuf_get_width (pixbuf);
			if (height)
				*height = gdk_pixbuf_get_height (pixbuf);
			g_object_unref (pixbuf);
		}
		g_free (filename);
	}
}

static void
get_page_size_area_prepared_cb (GdkPixbufLoader *loader,
				gpointer         data)
{
	gboolean *got_size = data;
	*got_size = TRUE;
}

static GdkPixbuf *
comics_document_render_pixbuf (EvDocument      *document,
			       EvRenderContext *rc)
{
	GdkPixbufLoader *loader;
	GdkPixbuf *rotated_pixbuf, *tmp_pixbuf;
	char **argv;
	guchar buf[4096];
	gboolean success;
	gint outpipe = -1;
	GPid child_pid;
	gssize bytes;
	gint width, height;
	gchar *filename;
	ComicsDocument *comics_document = COMICS_DOCUMENT (document);
	
	if (!comics_document->decompress_tmp) {
		argv = extract_argv (document, rc->page->index);
		success = g_spawn_async_with_pipes (NULL, argv, NULL,
						    G_SPAWN_SEARCH_PATH | 
						    G_SPAWN_STDERR_TO_DEV_NULL,
						    NULL, NULL,
						    &child_pid,
						    NULL, &outpipe, NULL, NULL);
		g_strfreev (argv);
		g_return_val_if_fail (success == TRUE, NULL);

		loader = gdk_pixbuf_loader_new ();
		g_signal_connect (loader, "size-prepared",
				  G_CALLBACK (render_pixbuf_size_prepared_cb), 
				  &rc->scale);

		while (outpipe >= 0) {
			bytes = read (outpipe, buf, 4096);

			if (bytes > 0) {
				gdk_pixbuf_loader_write (loader, buf, bytes, 
				NULL);
			} else if (bytes <= 0) {
				close (outpipe);
				gdk_pixbuf_loader_close (loader, NULL);
				outpipe = -1;
			}
		}
		tmp_pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
		rotated_pixbuf =
			gdk_pixbuf_rotate_simple (tmp_pixbuf,
						  360 - rc->rotation);
		g_spawn_close_pid (child_pid);
		g_object_unref (loader);
	} else {
		filename = 
			g_build_filename (comics_document->dir,
                                          (char *) comics_document->page_names->pdata[rc->page->index],
					  NULL);
	   
		gdk_pixbuf_get_file_info (filename, &width, &height);
		
		tmp_pixbuf =
			gdk_pixbuf_new_from_file_at_size (
				    filename, width * (rc->scale) + 0.5,
				    height * (rc->scale) + 0.5, NULL);
		rotated_pixbuf =
			gdk_pixbuf_rotate_simple (tmp_pixbuf,
						  360 - rc->rotation);
		g_free (filename);
		g_object_unref (tmp_pixbuf);
	}
	return rotated_pixbuf;
}

static cairo_surface_t *
comics_document_render (EvDocument      *document,
			EvRenderContext *rc)
{
	GdkPixbuf       *pixbuf;
	cairo_surface_t *surface;

	pixbuf = comics_document_render_pixbuf (document, rc);
	surface = ev_document_misc_surface_from_pixbuf (pixbuf);
	g_object_unref (pixbuf);
	
	return surface;
}

static void
render_pixbuf_size_prepared_cb (GdkPixbufLoader *loader,
				gint             width,
				gint             height,
				gpointer         data)
{
	double *scale = data;
	int w = (width  * (*scale) + 0.5);
	int h = (height * (*scale) + 0.5);

	gdk_pixbuf_loader_set_size (loader, w, h);
}

/**
 * comics_remove_dir: Removes a directory recursively. 
 * Returns:
 *   	0 if it was successfully deleted,
 * 	-1 if an error occurred 		
 */
static int 
comics_remove_dir (gchar *path_name) 
{
	GDir  *content_dir;
	const gchar *filename;
	gchar *filename_with_path;
	
	if (g_file_test (path_name, G_FILE_TEST_IS_DIR)) {
		content_dir = g_dir_open  (path_name, 0, NULL);
		filename  = g_dir_read_name (content_dir);
		while (filename) {
			filename_with_path = 
				g_build_filename (path_name, 
						  filename, NULL);
			comics_remove_dir (filename_with_path);
			g_free (filename_with_path);
			filename = g_dir_read_name (content_dir);
		}
		g_dir_close (content_dir);
	}
	/* Note from g_remove() documentation: on Windows, it is in general not 
	 * possible to remove a file that is open to some process, or mapped 
	 * into memory.*/
	return (g_remove (path_name));
}

static void
comics_document_finalize (GObject *object)
{
	ComicsDocument *comics_document = COMICS_DOCUMENT (object);
	
	if (comics_document->decompress_tmp) {
		if (comics_remove_dir (comics_document->dir) == -1)
			g_warning (_("There was an error deleting “%s”."),
				   comics_document->dir);
		g_free (comics_document->dir);
	}
	
	if (comics_document->page_names) {
                g_ptr_array_foreach (comics_document->page_names, (GFunc) g_free, NULL);
                g_ptr_array_free (comics_document->page_names, TRUE);
	}

	g_free (comics_document->archive);
	g_free (comics_document->selected_command);
	g_free (comics_document->alternative_command);
	g_free (comics_document->extract_command);
	g_free (comics_document->list_command);

	G_OBJECT_CLASS (comics_document_parent_class)->finalize (object);
}

static void
comics_document_class_init (ComicsDocumentClass *klass)
{
	GObjectClass    *gobject_class = G_OBJECT_CLASS (klass);
	EvDocumentClass *ev_document_class = EV_DOCUMENT_CLASS (klass);

	gobject_class->finalize = comics_document_finalize;

	ev_document_class->load = comics_document_load;
	ev_document_class->save = comics_document_save;
	ev_document_class->get_n_pages = comics_document_get_n_pages;
	ev_document_class->get_page_size = comics_document_get_page_size;
	ev_document_class->render = comics_document_render;
}

static void
comics_document_init (ComicsDocument *comics_document)
{
	comics_document->archive = NULL;
	comics_document->page_names = NULL;
	comics_document->extract_command = NULL;
}

/* Returns a list of file extensions supported by gdk-pixbuf */
static GSList*
get_supported_image_extensions()
{
	GSList *extensions = NULL;
	GSList *formats = gdk_pixbuf_get_formats ();
	GSList *l;

	for (l = formats; l != NULL; l = l->next) {
		int i;
		gchar **ext = gdk_pixbuf_format_get_extensions (l->data);

		for (i = 0; ext[i] != NULL; i++) {
			extensions = g_slist_append (extensions,
						     g_strdup (ext[i]));
		}

		g_strfreev (ext);
	}

	g_slist_free (formats);
	return extensions;
}

static char**
extract_argv (EvDocument *document, gint page)
{
	ComicsDocument *comics_document = COMICS_DOCUMENT (document);
	char **argv;
	char *command_line, *quoted_archive, *quoted_filename;
	GError *err = NULL;

        if (page >= comics_document->page_names->len)
                return NULL;

	if (comics_document->regex_arg) {
		quoted_archive = comics_regex_quote (comics_document->archive);
		quoted_filename =
			comics_regex_quote (comics_document->page_names->pdata[page]);
	} else {
		quoted_archive = g_shell_quote (comics_document->archive);
		quoted_filename = g_shell_quote (comics_document->page_names->pdata[page]);
	}

	command_line = g_strdup_printf ("%s %s %s",
					comics_document->extract_command,
					quoted_archive,
					quoted_filename);
	g_shell_parse_argv (command_line, NULL, &argv, &err);

	if (err) {
		g_warning (_("Error %s"), err->message);
		g_error_free (err);
		return NULL;
	}

	g_free (command_line);
	g_free (quoted_archive);
	g_free (quoted_filename);
	return argv;
}
