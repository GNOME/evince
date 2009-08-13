/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/*
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <config.h>
#include <unistd.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>

#include "comics-document.h"
#include "ev-document-misc.h"
#include "ev-document-thumbnails.h"
#include "ev-file-helpers.h"

struct _ComicsDocumentClass
{
	GObjectClass parent_class;
};
 
typedef enum 
{
	RARLABS,
	GNAUNRAR,
	UNZIP,
	P7ZIP
} ComicBookDecompressType;

struct _ComicsDocument
{
	GObject parent_instance;
	gchar    *archive, *dir;
	GSList   *page_names;
	gint     n_pages;
	gchar    *selected_command;
	gchar    *extract_command, *list_command, *decompress_tmp;
	gboolean regex_arg;
	gint     offset;
	ComicBookDecompressType command_usage;
};

#define OFFSET_7Z 53
#define NO_OFFSET 0

/* For perfomance reasons of 7z* we've choosen to decompress on the temporary 
 * directory instead of decompressing on the stdout */

struct {
	char *extract, *list, *decompress_tmp; 
	gboolean regex_arg;
	gint offset;
} command_usage_def[] = {
	{"%s p -c- -ierr", "%s vb -c- -- %s", NULL	       , FALSE, NO_OFFSET},
	{NULL		 , "%s t %s"	    , "%s -xf %s %s"   , TRUE , NO_OFFSET},
	{"%s -p -C"	 , "%s -Z -1 -- %s" , NULL	       , TRUE , NO_OFFSET},
	{NULL		 , "%s l -- %s"	    , "%s x -y %s -o%s", FALSE, OFFSET_7Z}
};


typedef struct    _ComicsDocumentClass ComicsDocumentClass;

static void       comics_document_document_iface_init (EvDocumentIface *iface);
static void       comics_document_document_thumbnails_iface_init (EvDocumentThumbnailsIface *iface);

static GSList*    get_supported_image_extensions (void);
static void       get_page_size_area_prepared_cb (GdkPixbufLoader *loader,
						  gpointer data);
static void       render_pixbuf_size_prepared_cb (GdkPixbufLoader *loader,
						  gint width,
						  gint height,
						  gpointer data);
static char**     extract_argv                   (EvDocument *document,
						  gint page);


EV_BACKEND_REGISTER_WITH_CODE (ComicsDocument, comics_document,
	{
		EV_BACKEND_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_THUMBNAILS,
						comics_document_document_thumbnails_iface_init);
	} );

static char *
comics_regex_quote (const char *s)
{
    char *ret, *d;

    d = ret = g_malloc (strlen (s) * 4 + 3);
    
    *d++ = '\'';

    for (; *s; s++, d++) {
	switch (*s) {
	case '?':
	case '|':
	case '[':
	case ']':
	case '*':
	case '\\':
	    *d++ = '\\';
	    break;
	case '\'':
	    *d++ = '\'';
	    *d++ = '\\';
	    *d++ = '\'';
	    break;
	}
	*d = *s;
    }
    
    *d++ = '\'';
    *d = '\0';

    return ret;
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
	gchar *quoted_file;
	ComicBookDecompressType type;
	
	type = comics_document->command_usage;
	quoted_file = g_shell_quote (comics_document->archive);
	
	comics_document->extract_command = 
			    g_strdup_printf (command_usage_def[type].extract, 
				             comics_document->selected_command);
	comics_document->list_command =
			    g_strdup_printf (command_usage_def[type].list, 
				             comics_document->selected_command, 
					     quoted_file);
	comics_document->regex_arg = command_usage_def[type].regex_arg;
	comics_document->offset = command_usage_def[type].offset;
	if (command_usage_def[type].decompress_tmp) {
		comics_document->dir = ev_tmp_directory (NULL); 
		comics_document->decompress_tmp = 
			g_strdup_printf (command_usage_def[type].decompress_tmp, 
					 comics_document->selected_command, 
					 quoted_file, 
					 comics_document->dir);
		g_free (quoted_file);
		/* unrar-free can't create directories so we do it on its 
		 * behalf */
		if (type == GNAUNRAR) {
			if (g_mkdir_with_parents (comics_document->dir, 0700) != 
			    0) {
				int errsv = errno;
				g_set_error (error,
					     EV_DOCUMENT_ERROR,
					     EV_DOCUMENT_ERROR_INVALID,
					     _("Failed to create a temporary "
					     "directory."));
				g_warning ("Failed to create directory %s: %s", 
					   comics_document->dir, 
					   g_strerror (errsv));
				
				return FALSE;
			}
		}
		if (!comics_decompress_temp_dir (comics_document->decompress_tmp, 
		    comics_document->selected_command, error))
			return FALSE;
		else
			return TRUE;
	} else {
		g_free (quoted_file);
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
		if (comics_document->selected_command) {
			comics_document->command_usage = UNZIP;
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
	if (!mime_type) {
		if (err) {
			g_propagate_error (error, err);
		} else {
			g_set_error_literal (error,
					     EV_DOCUMENT_ERROR,
					     EV_DOCUMENT_ERROR_INVALID,
					     _("Unknown MIME Type"));
		}

		return FALSE;
	}
	
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
	cb_files = g_strsplit (std_out, "\n", 0);
	g_free (std_out);

	if (!cb_files) {
		g_set_error_literal (error,
				     EV_DOCUMENT_ERROR,
				     EV_DOCUMENT_ERROR_INVALID,
				     _("No files in archive"));
		return FALSE;
	}

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
			comics_document->page_names =
				g_slist_insert_sorted (
					comics_document->page_names,
					g_strdup (g_strstrip (cb_file)),
					(GCompareFunc) strcmp);
			comics_document->n_pages++;
		}
		g_free (suffix);
	}
	g_strfreev (cb_files);
	g_slist_foreach (supported_extensions, (GFunc) g_free, NULL);
	g_slist_free (supported_extensions);

	if (comics_document->n_pages == 0) {
		g_set_error (error,
			     EV_DOCUMENT_ERROR,
			     EV_DOCUMENT_ERROR_INVALID,
			     _("No images found in archive %s"),
			     uri);
		return FALSE;
	}
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
	return COMICS_DOCUMENT (document)->n_pages;
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
	GPid child_pid = -1;
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

		if (gdk_pixbuf_loader_get_pixbuf (loader)) {
			pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
			if (width)
				*width = gdk_pixbuf_get_width (pixbuf);
			if (height)
				*height = gdk_pixbuf_get_height (pixbuf);
		}

		g_spawn_close_pid (child_pid);
		g_object_unref (loader);
	} else {
		filename = g_build_filename (comics_document->dir, 	
					     (char*) g_slist_nth_data (
					     comics_document->page_names, 
					     page->index),
					     NULL);
		pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
		g_free (filename);
		if (width)
			*width = gdk_pixbuf_get_width (pixbuf);
		if (height)
			*height = gdk_pixbuf_get_height (pixbuf);
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
	GdkPixbuf *rotated_pixbuf;
	char **argv;
	guchar buf[4096];
	gboolean success;
	gint outpipe = -1;
	GPid child_pid = -1;
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

		rotated_pixbuf = gdk_pixbuf_rotate_simple (
					gdk_pixbuf_loader_get_pixbuf (loader),
					360 - rc->rotation);
		g_spawn_close_pid (child_pid);
		g_object_unref (loader);
	} else {
		filename = 
			g_build_filename (comics_document->dir,
					  (char*) g_slist_nth_data (
						comics_document->page_names, 
						rc->page->index),
					  NULL);
	   
		gdk_pixbuf_get_file_info (filename, &width, &height);
		
		rotated_pixbuf = 
		  gdk_pixbuf_rotate_simple (gdk_pixbuf_new_from_file_at_size (
					    filename, width * (rc->scale) + 0.5,
					    height * (rc->scale) + 0.5, NULL),
					    360 - rc->rotation);
		g_free (filename);
	
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
		g_remove (ev_tmp_dir ());
	}
	
	if (comics_document->page_names) {
		g_slist_foreach (comics_document->page_names,
				 (GFunc) g_free, NULL);
		g_slist_free (comics_document->page_names);
	}

	g_free (comics_document->archive);
	g_free (comics_document->selected_command);
	g_free (comics_document->extract_command);
	g_free (comics_document->list_command);

	G_OBJECT_CLASS (comics_document_parent_class)->finalize (object);
}

static void
comics_document_class_init (ComicsDocumentClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->finalize = comics_document_finalize;
}

static EvDocumentInfo *
comics_document_get_info (EvDocument *document)
{
	EvDocumentInfo *info;
	info = g_new0 (EvDocumentInfo, 1);
	return info;
}

static void
comics_document_document_iface_init (EvDocumentIface *iface)
{
	iface->load = comics_document_load;
	iface->save = comics_document_save;
	iface->get_n_pages = comics_document_get_n_pages;
	iface->get_page_size = comics_document_get_page_size;
	iface->render = comics_document_render;
	iface->get_info = comics_document_get_info;
}

static void
comics_document_init (ComicsDocument *comics_document)
{
	comics_document->archive = NULL;
	comics_document->page_names = NULL;
	comics_document->extract_command = NULL;
	comics_document->n_pages = 0;
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

static GdkPixbuf *
comics_document_thumbnails_get_thumbnail (EvDocumentThumbnails *document,
					  EvRenderContext      *rc,
					  gboolean              border)
{
	GdkPixbuf *thumbnail;

	thumbnail = comics_document_render_pixbuf (EV_DOCUMENT (document), rc);

	if (border) {
	      GdkPixbuf *tmp_pixbuf = thumbnail;
	      
	      thumbnail = ev_document_misc_get_thumbnail_frame (-1, -1, tmp_pixbuf);
	      g_object_unref (tmp_pixbuf);
	}

	return thumbnail;
}

static void
comics_document_thumbnails_get_dimensions (EvDocumentThumbnails *document,
					   EvRenderContext      *rc,
					   gint                 *width,
					   gint                 *height)
{
	gdouble page_width, page_height;
	
	comics_document_get_page_size (EV_DOCUMENT (document), rc->page,
				       &page_width, &page_height);

	if (rc->rotation == 90 || rc->rotation == 270) {
		*width = (gint) (page_height * rc->scale);
		*height = (gint) (page_width * rc->scale);
	} else {
		*width = (gint) (page_width * rc->scale);
		*height = (gint) (page_height * rc->scale);
	}
}

static void
comics_document_document_thumbnails_iface_init (EvDocumentThumbnailsIface *iface)
{
	iface->get_thumbnail = comics_document_thumbnails_get_thumbnail;
	iface->get_dimensions = comics_document_thumbnails_get_dimensions;
}

static char**
extract_argv (EvDocument *document, gint page)
{
	ComicsDocument *comics_document = COMICS_DOCUMENT (document);
	char **argv;
	char *command_line, *quoted_archive, *quoted_filename;
	GError *err = NULL;

	quoted_archive = g_shell_quote (comics_document->archive);
	if (comics_document->regex_arg) {
		quoted_filename = comics_regex_quote (
			g_slist_nth_data (comics_document->page_names, page));
	} else {
		quoted_filename = g_shell_quote (
			g_slist_nth_data (comics_document->page_names, page));
	}

	command_line = g_strdup_printf ("%s -- %s %s",
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
