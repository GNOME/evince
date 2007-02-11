/* Ghostscript widget for GTK/GNOME
 *
 * Copyright (C) 1998 - 2005 the Free Software Foundation
 *
 * Authors: Jonathan Blandford, Jaka Mocnik, Carlos Garcia Campos
 *
 * Based on code by: Federico Mena (Quartic), Szekeres Istvan (Pista)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <errno.h>

#include "ps-document.h"
#include "ps.h"
#include "gstypes.h"
#include "gsdefaults.h"
#include "ev-file-exporter.h"
#include "ev-async-renderer.h"

#define MAX_BUFSIZE 1024

/* structure to describe section of file to send to ghostscript */
typedef struct {
	FILE *fp;
	glong begin;
	guint len;
	gboolean seek_needed;
	gboolean close;
} PSSection;

struct _PSDocument {
	GObject object;

	GtkWidget *target_window;
	GdkWindow *pstarget;
	GdkPixmap *bpixmap;
	glong message_window;          /* Used by ghostview to receive messages from app */

	GPid interpreter_pid;               /* PID of interpreter, -1 if none  */
	GIOChannel *interpreter_input;      /* stdin of interpreter            */
	GIOChannel *interpreter_output;     /* stdout of interpreter           */
	GIOChannel *interpreter_err;        /* stderr of interpreter           */
	guint interpreter_input_id;
	guint interpreter_output_id;
	guint interpreter_error_id;

	gboolean busy;                /* Is gs busy drawing? */
	gboolean structured_doc;

	GQueue *ps_input;
	gchar *input_buffer_ptr;
	guint bytes_left;
	guint buffer_bytes_left;

	FILE *gs_psfile;              /* the currently loaded FILE */
	gchar *gs_filename;           /* the currently loaded filename */
	gchar *input_buffer;
	gboolean send_filename_to_gs; /* True if gs should read from file directly */
	struct document *doc;

	gint  *ps_export_pagelist;
	gchar *ps_export_filename;
};

struct _PSDocumentClass {
	GObjectClass parent_class;
	
	GdkAtom gs_atom;
	GdkAtom next_atom;
	GdkAtom page_atom;
	GdkAtom string_atom;
}; 

static void     ps_document_document_iface_init      (EvDocumentIface      *iface);
static void     ps_document_file_exporter_iface_init (EvFileExporterIface  *iface);
static void     ps_async_renderer_iface_init         (EvAsyncRendererIface *iface);

static void     ps_interpreter_start                 (PSDocument           *gs);
static void     ps_interpreter_stop                  (PSDocument           *gs);
static void     ps_interpreter_failed                (PSDocument           *gs,
						      const gchar          *msg);
static gboolean ps_interpreter_is_ready              (PSDocument           *gs);

static void     push_pixbuf                          (PSDocument           *gs);


G_DEFINE_TYPE_WITH_CODE (PSDocument, ps_document, G_TYPE_OBJECT,
                         {
				 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT,
							ps_document_document_iface_init);
				 G_IMPLEMENT_INTERFACE (EV_TYPE_FILE_EXPORTER,
							ps_document_file_exporter_iface_init);
				 G_IMPLEMENT_INTERFACE (EV_TYPE_ASYNC_RENDERER,
							ps_async_renderer_iface_init);
			 });

static void
ps_section_free (PSSection *section)
{
	if (!section)
		return;

	if (section->close && section->fp)
		fclose (section->fp);

	g_free (section);
}

/* PSDocument */
static void
ps_document_init (PSDocument *gs)
{
	gs->bpixmap = NULL;
	gs->interpreter_pid = -1;

	gs->busy = FALSE;
	gs->gs_filename = NULL;

	gs->structured_doc = FALSE;
	gs->send_filename_to_gs = FALSE;

	gs->doc = NULL;

	gs->interpreter_input = NULL;
	gs->interpreter_output = NULL;
	gs->interpreter_err = NULL;
	gs->interpreter_input_id = 0;
	gs->interpreter_output_id = 0;
	gs->interpreter_error_id = 0;

	gs->ps_input = g_queue_new ();
	gs->input_buffer = NULL;
	gs->input_buffer_ptr = NULL;
	gs->bytes_left = 0;
	gs->buffer_bytes_left = 0;

	gs->ps_export_pagelist = NULL;
	gs->ps_export_filename = NULL;
}

static void
ps_document_dispose (GObject *object)
{
	PSDocument *gs = PS_DOCUMENT (object);
	
	if (gs->gs_psfile) {
		fclose (gs->gs_psfile);
		gs->gs_psfile = NULL;
	}

	if (gs->gs_filename) {
		g_free (gs->gs_filename);
		gs->gs_filename = NULL;
	}

	if (gs->doc) {
		psfree (gs->doc);
		gs->doc = NULL;
	}

	if (gs->bpixmap) {
		g_object_unref (gs->bpixmap);
		gs->bpixmap = NULL;
	}

	if (gs->input_buffer) {
		g_free (gs->input_buffer);
		gs->input_buffer = NULL;
	}

	if (gs->target_window) {
		gtk_widget_destroy (gs->target_window);
		gs->target_window = NULL;
		gs->pstarget = NULL;
	}

	if (gs->ps_input) {
		g_queue_foreach (gs->ps_input, (GFunc)ps_section_free, NULL);
		g_queue_free (gs->ps_input);
		gs->ps_input = NULL;
	}

	ps_interpreter_stop (gs);

	G_OBJECT_CLASS (ps_document_parent_class)->dispose (object);
}

static void
ps_document_class_init (PSDocumentClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = ps_document_dispose;

	klass->gs_atom = gdk_atom_intern ("GHOSTVIEW", FALSE);
	klass->next_atom = gdk_atom_intern ("NEXT", FALSE);
	klass->page_atom = gdk_atom_intern ("PAGE", FALSE);
	klass->string_atom = gdk_atom_intern ("STRING", FALSE);
}

/* PSInterpreter */
static gboolean
ps_interpreter_input (GIOChannel   *io,
		      GIOCondition condition,
		      PSDocument  *gs)
{
	PSSection *section = NULL;

	do {
		if (gs->buffer_bytes_left == 0) {
			/* Get a new section if required */
			if (gs->bytes_left == 0) {
				ps_section_free (section);
				section = NULL;
				g_queue_pop_tail (gs->ps_input);
			}
			
			if (section == NULL) {
				section = g_queue_peek_tail (gs->ps_input);
			}

			/* Have to seek at the beginning of each section */
			if (section && section->seek_needed) {
				fseek (section->fp, section->begin, SEEK_SET);
				section->seek_needed = FALSE;
				gs->bytes_left = section->len;
			}

			if (gs->bytes_left > MAX_BUFSIZE) {
				gs->buffer_bytes_left = fread (gs->input_buffer, sizeof (char),
							       MAX_BUFSIZE, section->fp);
			} else if (gs->bytes_left > 0) {
				gs->buffer_bytes_left = fread (gs->input_buffer, sizeof (char),
							       gs->bytes_left, section->fp);
			} else {
				gs->buffer_bytes_left = 0;
			}
			
			if (gs->bytes_left > 0 && gs->buffer_bytes_left == 0) {
				ps_interpreter_failed (gs, NULL); /* Error occurred */
			}
			
			gs->input_buffer_ptr = gs->input_buffer;
			gs->bytes_left -= gs->buffer_bytes_left;
		}

		if (gs->buffer_bytes_left > 0) {
			GIOStatus status;
			gsize bytes_written;
			GError *error = NULL;

			status = g_io_channel_write_chars (gs->interpreter_input,
							   gs->input_buffer_ptr,
							   gs->buffer_bytes_left,
							   &bytes_written,
							   &error);
			switch (status) {
			        case G_IO_STATUS_NORMAL:
					gs->buffer_bytes_left -= bytes_written;
					gs->input_buffer_ptr += bytes_written;

					break;
			        case G_IO_STATUS_ERROR:
					ps_interpreter_failed (gs, error->message);
					g_error_free (error);
					
					break;
			        case G_IO_STATUS_AGAIN:
			        default:
					break;
			}
		}
	} while (!g_queue_is_empty (gs->ps_input) && gs->buffer_bytes_left == 0);

	if (g_queue_is_empty (gs->ps_input) && gs->buffer_bytes_left == 0) {
		GIOFlags flags;

		flags = g_io_channel_get_flags (gs->interpreter_input);
		
		g_io_channel_set_flags (gs->interpreter_input,
					flags & ~G_IO_FLAG_NONBLOCK, NULL);
		g_io_channel_flush (gs->interpreter_input, NULL);
		g_io_channel_set_flags (gs->interpreter_input,
					flags | G_IO_FLAG_NONBLOCK, NULL);
		
		gs->interpreter_input_id = 0;
		
		return FALSE;
	}
	
	return TRUE;
}

static gboolean
ps_interpreter_output (GIOChannel   *io,
		       GIOCondition condition,
		       PSDocument  *gs)
{
	gchar buf[MAX_BUFSIZE + 1];
	gsize bytes = 0;
	GIOStatus status;
	GError *error = NULL;

	status = g_io_channel_read_chars (io, buf, MAX_BUFSIZE,
					  &bytes, &error);
	switch (status) {
	        case G_IO_STATUS_NORMAL:
			if (bytes > 0) {
				buf[bytes] = '\0';
				g_print ("%s", buf);
			}
			break;
	        case G_IO_STATUS_EOF:
			g_io_channel_unref (gs->interpreter_output);
			gs->interpreter_output = NULL;
			gs->interpreter_output_id = 0;
			
			return FALSE;
	        case G_IO_STATUS_ERROR:
			ps_interpreter_failed (gs, error->message);
			g_error_free (error);
			gs->interpreter_output_id = 0;
			
			return FALSE;
	        default:
			break;
	}
	
	if (!gs->interpreter_err) {
		ps_interpreter_failed (gs, NULL);
	}

	return TRUE;
}

static gboolean
ps_interpreter_error (GIOChannel   *io,
		       GIOCondition condition,
		       PSDocument  *gs)
{
	gchar buf[MAX_BUFSIZE + 1];
	gsize bytes = 0;
	GIOStatus status;
	GError *error = NULL;

	status = g_io_channel_read_chars (io, buf, MAX_BUFSIZE,
					  &bytes, &error);
	switch (status) {
	        case G_IO_STATUS_NORMAL:
			if (bytes > 0) {
				buf[bytes] = '\0';
				g_print ("%s", buf);
			}
			
			break;
	        case G_IO_STATUS_EOF:
			g_io_channel_unref (gs->interpreter_err);
			gs->interpreter_err = NULL;
			gs->interpreter_error_id = 0;
			
			return FALSE;
	        case G_IO_STATUS_ERROR:
			ps_interpreter_failed (gs, error->message);
			g_error_free (error);
			gs->interpreter_error_id = 0;
			
			break;
	        default:
			break;
	}
	
	if (!gs->interpreter_output) {
		ps_interpreter_failed (gs, NULL);
	}

	return TRUE;
}

static void
ps_interpreter_finished (GPid        pid,
			 gint        status,
			 PSDocument *gs)
{
	g_spawn_close_pid (gs->interpreter_pid);
	gs->interpreter_pid = -1;
	ps_interpreter_failed (gs, NULL);
}

#define NUM_ARGS    100
#define NUM_GS_ARGS (NUM_ARGS - 20)
#define NUM_ALPHA_ARGS 10

static void
setup_interpreter_env (gchar **envp)
{
	gint i;

	for (i = 0; envp[i]; i++)
		putenv (envp[i]);
}

static void
ps_interpreter_start (PSDocument *gs)
{
	gchar *argv[NUM_ARGS], *dir, *gv_env, *gs_path;
	gchar **gs_args, **alpha_args = NULL;
	gchar **envp;
	gint pin, pout, perr;
	gint argc = 0, i;
	GError *error = NULL;
	
	if (!gs->gs_filename)
		return;

	ps_interpreter_stop (gs);

	dir = g_path_get_dirname (gs->gs_filename);

	/* set up the args... */
	gs_path = g_find_program_in_path ("gs");
	gs_args = g_strsplit (gs_path, " ", NUM_GS_ARGS);
	g_free (gs_path);
	for (i = 0; i < NUM_GS_ARGS && gs_args[i]; i++, argc++) {
		argv[argc] = gs_args[i];
	}

	alpha_args = g_strsplit (ALPHA_PARAMS, " ", NUM_ALPHA_ARGS);
	for (i = 0; i < NUM_ALPHA_ARGS && alpha_args[i]; i++, argc++) {
		argv[argc] = alpha_args[i];
	}

	argv[argc++] = "-dNOPAUSE";
	argv[argc++] = "-dQUIET";
	argv[argc++] = "-dSAFER";

	if (gs->send_filename_to_gs) {
		argv[argc++] = gs->gs_filename;
		argv[argc++] = "-c";
		argv[argc++] = "quit";
	} else {
		argv[argc++] = "-";
	}

	argv[argc++] = NULL;

	gv_env = g_strdup_printf ("GHOSTVIEW=%ld %ld;DISPLAY=%s",
				  gdk_x11_drawable_get_xid (gs->pstarget),
				  gdk_x11_drawable_get_xid (gs->bpixmap),
				  gdk_display_get_name (gdk_drawable_get_display (gs->pstarget)));
	envp = g_strsplit (gv_env, ";", 2);
	g_free (gv_env);

	if (g_spawn_async_with_pipes (dir, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD,
				      (GSpawnChildSetupFunc)setup_interpreter_env, envp,
				      &(gs->interpreter_pid),
				      &pin, &pout, &perr,
				      &error)) {
		GIOFlags flags;

		g_child_watch_add (gs->interpreter_pid,
				   (GChildWatchFunc)ps_interpreter_finished, 
				   gs);

		gs->interpreter_input = g_io_channel_unix_new (pin);
		g_io_channel_set_encoding (gs->interpreter_input, NULL, NULL);
		flags = g_io_channel_get_flags (gs->interpreter_input);
		g_io_channel_set_flags (gs->interpreter_input,
					flags | G_IO_FLAG_NONBLOCK, NULL);

		
		gs->interpreter_output = g_io_channel_unix_new (pout);
		flags = g_io_channel_get_flags (gs->interpreter_output);
		g_io_channel_set_flags (gs->interpreter_output,
					flags | G_IO_FLAG_NONBLOCK, NULL);
		gs->interpreter_output_id =
			g_io_add_watch (gs->interpreter_output, G_IO_IN,
					(GIOFunc)ps_interpreter_output,
					gs);
		
		gs->interpreter_err = g_io_channel_unix_new (perr);
		flags = g_io_channel_get_flags (gs->interpreter_err);
		g_io_channel_set_flags (gs->interpreter_err,
					flags | G_IO_FLAG_NONBLOCK, NULL);
		gs->interpreter_error_id =
			g_io_add_watch (gs->interpreter_err, G_IO_IN,
					(GIOFunc)ps_interpreter_error,
					gs);
	} else {
		g_warning (error->message);
		g_error_free (error);
	}

	g_free (dir);
	g_strfreev (envp);
	g_strfreev (gs_args);
	g_strfreev (alpha_args);
}

static void
ps_interpreter_stop (PSDocument *gs)
{
	if (gs->interpreter_pid > 0) {
		gint status = 0;
		
		kill (gs->interpreter_pid, SIGTERM);
		while ((wait (&status) == -1) && (errno == EINTR));
		g_spawn_close_pid (gs->interpreter_pid);
		gs->interpreter_pid = -1;
	}

	if (gs->interpreter_input) {
		g_io_channel_unref (gs->interpreter_input);
		gs->interpreter_input = NULL;

		if (gs->interpreter_input_id > 0) {
			g_source_remove (gs->interpreter_input_id);
			gs->interpreter_input_id = 0;
		}
		
		if (gs->ps_input) {
			g_queue_foreach (gs->ps_input, (GFunc)ps_section_free, NULL);
			g_queue_free (gs->ps_input);
			gs->ps_input = g_queue_new ();
		}
	}

	if (gs->interpreter_output) {
		g_io_channel_unref (gs->interpreter_output);
		gs->interpreter_output = NULL;

		if (gs->interpreter_output_id > 0) {
			g_source_remove (gs->interpreter_output_id);
			gs->interpreter_output_id = 0;
		}
	}

	if (gs->interpreter_err) {
		g_io_channel_unref (gs->interpreter_err);
		gs->interpreter_err = NULL;
		
		if (gs->interpreter_error_id > 0) {
			g_source_remove (gs->interpreter_error_id);
			gs->interpreter_error_id = 0;
		}
	}

	gs->busy = FALSE;
}

static void
ps_interpreter_failed (PSDocument *gs, const char *msg)
{
	g_warning (msg ? msg : _("Interpreter failed."));
	
	push_pixbuf (gs);
	ps_interpreter_stop (gs);
}

static gboolean
ps_interpreter_is_ready (PSDocument *gs)
{
	return (gs->interpreter_pid != -1 &&
		!gs->busy &&
		(g_queue_is_empty (gs->ps_input)));
}

/* EvDocumentIface */
static gboolean
document_load (PSDocument *gs, const gchar *fname, GError **error)
{
	/* prepare this document */
	gs->structured_doc = FALSE;
	gs->send_filename_to_gs = TRUE;
	gs->gs_filename = g_strdup (fname);

	/*
	 * We need to make sure that the file is loadable/exists!
	 * otherwise we want to exit without loading new stuff...
	 */
	if (!g_file_test (fname, G_FILE_TEST_IS_REGULAR)) {
		gchar *filename_dsp;

		filename_dsp = g_filename_display_name (fname);
		g_set_error (error,
			     G_FILE_ERROR,
			     G_FILE_ERROR_NOENT,
			     _("Cannot open file “%s”.\n"), /* FIXME: remove \n after freeze */
			     filename_dsp);
		g_free (filename_dsp);
		
		ps_interpreter_failed (gs, NULL);
		return FALSE;
	}

	if (!gs->gs_filename || (gs->gs_psfile = fopen (gs->gs_filename, "r")) == NULL) {
		gchar *filename_dsp;

		filename_dsp = g_filename_display_name (fname);
		g_set_error (error,
			     G_FILE_ERROR,
			     G_FILE_ERROR_NOENT,
			     _("Cannot open file “%s”.\n"), /* FIXME: remove \n after freeze */
			     filename_dsp);
		g_free (filename_dsp);
		
		ps_interpreter_failed (gs, NULL);
		return FALSE;
	}
	
	/* we grab the vital statistics!!! */
	gs->doc = psscan (gs->gs_psfile, TRUE, gs->gs_filename);
	if (!gs->doc)
		return FALSE;
	
	
	if ((!gs->doc->epsf && gs->doc->numpages > 0) ||
	    (gs->doc->epsf && gs->doc->numpages > 1)) {
		gs->structured_doc = TRUE;
		gs->send_filename_to_gs = FALSE;
	}

	return TRUE;
}

static gboolean
ps_document_load (EvDocument  *document,
		  const char  *uri,
		  GError     **error)
{
	char *filename;
	char *gs_path;
	gboolean result;

	filename = g_filename_from_uri (uri, NULL, error);
	if (!filename)
		return FALSE;

	gs_path = g_find_program_in_path ("gs");
	if (!gs_path) {
		gchar *filename_dsp;
		
		filename_dsp = g_filename_display_name (filename);
		g_set_error (error,
			     G_FILE_ERROR,
			     G_FILE_ERROR_NOENT,
			     _("Failed to load document “%s”. Ghostscript interpreter was not found in path"),
			     filename);
		g_free (filename_dsp);
		g_free (filename);
		
		return FALSE;
	}

	result = document_load (PS_DOCUMENT (document), filename, error);
	if (!result && !(*error)) {
		gchar *filename_dsp;
		
		filename_dsp = g_filename_display_name (filename);
		g_set_error (error,
			     G_FILE_ERROR,
			     G_FILE_ERROR_FAILED,
			     _("Failed to load document “%s”"),
			     filename_dsp);
		g_free (filename_dsp);
	}
	
	g_free (gs_path);
	g_free (filename);

	return result;
}

static gboolean
save_document (PSDocument *document, const char *filename)
{
	gboolean result = TRUE;
	GtkGSDocSink *sink = gtk_gs_doc_sink_new ();
	FILE *f, *src_file;
	gchar *buf;

	src_file = fopen (document->gs_filename, "r");
	if (src_file) {
		struct stat stat_rec;

		if (stat (document->gs_filename, &stat_rec) == 0) {
			pscopy (src_file, sink, 0, stat_rec.st_size - 1);
		}

		fclose (src_file);
	}
	
	buf = gtk_gs_doc_sink_get_buffer (sink);
	if (buf == NULL) {
		return FALSE;
	}
	
	f = fopen (filename, "w");
	if (f) {
		fputs (buf, f);
		fclose (f);
	} else {
		result = FALSE;
	}

	g_free (buf);
	gtk_gs_doc_sink_free (sink);
	g_free (sink);

	return result;
}

static gboolean
save_page_list (PSDocument *document, int *page_list, const char *filename)
{
	gboolean result = TRUE;
	GtkGSDocSink *sink = gtk_gs_doc_sink_new ();
	FILE *f;
	gchar *buf;

	pscopydoc (sink, document->gs_filename, 
		   document->doc, page_list);
	
	buf = gtk_gs_doc_sink_get_buffer (sink);
	
	f = fopen (filename, "w");
	if (f) {
		fputs (buf, f);
		fclose (f);
	} else {
		result = FALSE;
	}

	g_free (buf);
	gtk_gs_doc_sink_free (sink);
	g_free (sink);

	return result;
}

static gboolean
ps_document_save (EvDocument  *document,
		  const char  *uri,
		  GError     **error)
{
	PSDocument *ps = PS_DOCUMENT (document);
	gboolean result;
	char *filename;

	filename = g_filename_from_uri (uri, NULL, error);
	if (!filename)
		return FALSE;

	result = save_document (ps, filename);

	g_free (filename);

	return result;
}

static int
ps_document_get_n_pages (EvDocument *document)
{
	PSDocument *ps = PS_DOCUMENT (document);

	if (!ps->gs_filename || !ps->doc) {
		return -1;
	}

	return ps->structured_doc ? ps->doc->numpages : 1;
}

#define DEFAULT_PAGE_SIZE 1

static void
get_page_box (PSDocument *gs, int page, int *urx, int *ury, int *llx, int *lly)
{
	gint new_llx = 0;
	gint new_lly = 0;
	gint new_urx = 0;
	gint new_ury = 0;
	GtkGSPaperSize *papersizes = gtk_gs_defaults_get_paper_sizes ();
	int new_pagesize = -1;

	if (new_pagesize == -1) {
		new_pagesize = DEFAULT_PAGE_SIZE;
		if (gs->doc) {
			/* If we have a document:
			 * We use -- the page size (if specified)	
			 * or the doc. size (if specified)	
			 * or the page bbox (if specified)	
			 * or the bounding box	
			 */
			if ((page >= 0) && (gs->doc->numpages > page) &&
			    (gs->doc->pages) && (gs->doc->pages[page].size)) {
				new_pagesize = gs->doc->pages[page].size - gs->doc->size;
			} else if (gs->doc->default_page_size != NULL) {
				new_pagesize = gs->doc->default_page_size - gs->doc->size;
			} else if ((page >= 0) &&
				   (gs->doc->numpages > page) &&
				   (gs->doc->pages) &&
				   (gs->doc->pages[page].boundingbox[URX] >
				    gs->doc->pages[page].boundingbox[LLX]) &&
              			   (gs->doc->pages[page].boundingbox[URY] >
               			    gs->doc->pages[page].boundingbox[LLY])) {
        			new_pagesize = -1;
      			} else if ((gs->doc->boundingbox[URX] > gs->doc->boundingbox[LLX]) &&
              			   (gs->doc->boundingbox[URY] > gs->doc->boundingbox[LLY])) {
				new_pagesize = -1;
			}
		}
	}

	/* Compute bounding box */
	if (gs->doc && (gs->doc->epsf || new_pagesize == -1)) {    /* epsf or bbox */
		if ((page >= 0) &&
		    (gs->doc->pages) &&
		    (gs->doc->pages[page].boundingbox[URX] >
		     gs->doc->pages[page].boundingbox[LLX]) &&
		    (gs->doc->pages[page].boundingbox[URY] >
                     gs->doc->pages[page].boundingbox[LLY])) {
			/* use page bbox */
			new_llx = gs->doc->pages[page].boundingbox[LLX];
			new_lly = gs->doc->pages[page].boundingbox[LLY];
			new_urx = gs->doc->pages[page].boundingbox[URX];
			new_ury = gs->doc->pages[page].boundingbox[URY];
		} else if ((gs->doc->boundingbox[URX] > gs->doc->boundingbox[LLX]) &&
        	           (gs->doc->boundingbox[URY] > gs->doc->boundingbox[LLY])) {
			/* use doc bbox */
			new_llx = gs->doc->boundingbox[LLX];
			new_lly = gs->doc->boundingbox[LLY];
			new_urx = gs->doc->boundingbox[URX];
			new_ury = gs->doc->boundingbox[URY];
		}
	} else {
		if (new_pagesize < 0)
			new_pagesize = DEFAULT_PAGE_SIZE;
		new_llx = new_lly = 0;
		if (gs->doc && gs->doc->size &&
		    (new_pagesize < gs->doc->numsizes)) {
			new_urx = gs->doc->size[new_pagesize].width;
			new_ury = gs->doc->size[new_pagesize].height;
		} else {
			new_urx = papersizes[new_pagesize].width;
			new_ury = papersizes[new_pagesize].height;
		}
	}

	if (new_urx <= new_llx)
		new_urx = papersizes[12].width;
	if (new_ury <= new_lly)
		new_ury = papersizes[12].height;

	*urx = new_urx;
	*ury = new_ury;
	*llx = new_llx;
	*lly = new_lly;
}

static void
ps_document_get_page_size (EvDocument *document,
			   int         page,
			   double     *width,
			   double     *height)
{
	PSDocument *gs = PS_DOCUMENT (document);
	int urx, ury, llx, lly;

	get_page_box (gs, page, &urx, &ury, &llx, &lly);

	if (width) {
		*width = (urx - llx) + 0.5;
	}

	if (height) {
		*height = (ury - lly) + 0.5;
	}
}

static gboolean
ps_document_can_get_text (EvDocument *document)
{
	return FALSE;
}

static EvDocumentInfo *
ps_document_get_info (EvDocument *document)
{
	EvDocumentInfo *info;
	PSDocument *ps = PS_DOCUMENT (document);
	int urx, ury, llx, lly;

	info = g_new0 (EvDocumentInfo, 1);
	info->fields_mask = EV_DOCUMENT_INFO_TITLE |
	                    EV_DOCUMENT_INFO_FORMAT |
			    EV_DOCUMENT_INFO_CREATOR |
			    EV_DOCUMENT_INFO_N_PAGES |
	                    EV_DOCUMENT_INFO_PAPER_SIZE;

	info->title = g_strdup (ps->doc->title);
	info->format = ps->doc->epsf ? g_strdup (_("Encapsulated PostScript"))
		                     : g_strdup (_("PostScript"));
	info->creator = g_strdup (ps->doc->creator);
	info->n_pages = ev_document_get_n_pages (document);
	
	get_page_box (PS_DOCUMENT (document), 0, &urx, &ury, &llx, &lly);

	info->paper_width  = (urx - llx) / 72.0f * 25.4f;
	info->paper_height = (ury - lly) / 72.0f * 25.4f;

	return info;
}

static void
ps_document_document_iface_init (EvDocumentIface *iface)
{
	iface->load = ps_document_load;
	iface->save = ps_document_save;
	iface->can_get_text = ps_document_can_get_text;
	iface->get_n_pages = ps_document_get_n_pages;
	iface->get_page_size = ps_document_get_page_size;
	iface->get_info = ps_document_get_info;
}

/* EvAsyncRendererIface */
static void
setup_page (PSDocument *gs, int page, double scale, int rotation)
{
	gchar *buf;
	char scaled_dpi[G_ASCII_DTOSTR_BUF_SIZE];	
	int urx, ury, llx, lly;
	PSDocumentClass *gs_class;

	gs_class = PS_DOCUMENT_GET_CLASS (gs);

	get_page_box (gs, page, &urx, &ury, &llx, &lly);
	g_ascii_dtostr (scaled_dpi, G_ASCII_DTOSTR_BUF_SIZE, 72.0 * scale);

	buf = g_strdup_printf ("%ld %d %d %d %d %d %s %s %d %d %d %d",
			       0L, rotation, llx, lly, urx, ury,
			       scaled_dpi, scaled_dpi,
			       0, 0, 0, 0);
	
	gdk_property_change (gs->pstarget, gs_class->gs_atom, gs_class->string_atom,
			     8, GDK_PROP_MODE_REPLACE, (guchar *)buf, strlen (buf));
	g_free (buf);
	
	gdk_flush ();
}

static void
setup_pixmap (PSDocument *gs, int page, double scale, int rotation)
{
	GdkGC *fill;
	GdkColor white = { 0, 0xFFFF, 0xFFFF, 0xFFFF };   /* pixel, r, g, b */
	GdkColormap *colormap;
	double width, height;
	int pixmap_width, pixmap_height;
	
	if (gs->pstarget == NULL)
		return;

	ev_document_get_page_size (EV_DOCUMENT (gs), page, &width, &height);

	if (rotation == 90 || rotation == 270) {
		pixmap_height = width * scale + 0.5;
		pixmap_width = height * scale + 0.5;
	} else {
		pixmap_width = width * scale + 0.5;
		pixmap_height = height * scale + 0.5;
	}

	if (gs->bpixmap) {
		gint w, h;

		gdk_drawable_get_size (gs->bpixmap, &w, &h);

		if (pixmap_width != w || h != pixmap_height) {
			g_object_unref (gs->bpixmap);
			gs->bpixmap = NULL;
			ps_interpreter_stop (gs);
		}
	}

	if (!gs->bpixmap) {
		fill = gdk_gc_new (gs->pstarget);
		colormap = gdk_drawable_get_colormap (gs->pstarget);
		gdk_colormap_alloc_color (colormap, &white, FALSE, TRUE);
		gdk_gc_set_foreground (fill, &white);
		gs->bpixmap = gdk_pixmap_new (gs->pstarget, pixmap_width,
					      pixmap_height, -1);
		gdk_draw_rectangle (gs->bpixmap, fill, TRUE,
	                            0, 0, pixmap_width, pixmap_height);
	}
}

static void
push_pixbuf (PSDocument *gs)
{
	GdkColormap *cmap;
	GdkPixbuf *pixbuf;
	gint width, height;

	if (gs->pstarget == NULL)
		return;

	cmap = gdk_drawable_get_colormap (gs->pstarget);
	gdk_drawable_get_size (gs->bpixmap, &width, &height);
	pixbuf = gdk_pixbuf_get_from_drawable (NULL, gs->bpixmap, cmap,
					       0, 0, 0, 0,
					       width, height);
	g_signal_emit_by_name (gs, "render_finished", pixbuf);
	g_object_unref (pixbuf);
}

static gboolean
ps_document_widget_event (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	PSDocument *gs = (PSDocument *) data;
	PSDocumentClass *gs_class;

	if (event->type != GDK_CLIENT_EVENT)
		return FALSE;

	gs_class = PS_DOCUMENT_GET_CLASS (gs);
	
	gs->message_window = event->client.data.l[0];

	if (event->client.message_type == gs_class->page_atom) {
		gs->busy = FALSE;

		push_pixbuf (gs);
	}

	return TRUE;
}

static void
send_ps (PSDocument *gs, long begin, unsigned int len, gboolean close)
{
	PSSection *ps_new;

	if (!gs->interpreter_input) {
		g_critical ("No pipe to gs: error in send_ps().");
		return;
	}

	ps_new = g_new0 (PSSection, 1);
	ps_new->fp = gs->gs_psfile;
	ps_new->begin = begin;
	ps_new->len = len;
	ps_new->seek_needed = TRUE;
	ps_new->close = close;

	if (gs->input_buffer == NULL) {
		gs->input_buffer = g_malloc (MAX_BUFSIZE);
	}

	if (g_queue_is_empty (gs->ps_input)) {
		gs->input_buffer_ptr = gs->input_buffer;
		gs->bytes_left = len;
		gs->buffer_bytes_left = 0;
		g_queue_push_head (gs->ps_input, ps_new);
		gs->interpreter_input_id =
			g_io_add_watch (gs->interpreter_input,
					G_IO_OUT/* | G_IO_HUP | G_IO_ERR | G_IO_NVAL*/,
					(GIOFunc)ps_interpreter_input,
					gs);
	} else {
		g_queue_push_head (gs->ps_input, ps_new);
	}
}

static void
ps_document_next_page (PSDocument *gs)
{
	XEvent      event;
	GdkScreen  *screen;
	GdkDisplay *display;
	Display    *dpy;
	PSDocumentClass *gs_class;

	g_assert (gs->interpreter_pid != 0);
	g_assert (gs->busy != TRUE);

	gs_class = PS_DOCUMENT_GET_CLASS (gs);
	
	gs->busy = TRUE;

	screen = gtk_window_get_screen (GTK_WINDOW (gs->target_window));
	display = gdk_screen_get_display (screen);
	dpy = gdk_x11_display_get_xdisplay (display);

	event.xclient.type = ClientMessage;
	event.xclient.display = dpy;
	event.xclient.window = gs->message_window;
	event.xclient.message_type =
		gdk_x11_atom_to_xatom_for_display (display,
						   gs_class->next_atom);
	event.xclient.format = 32;

	gdk_error_trap_push ();
	XSendEvent (dpy, gs->message_window, FALSE, 0, &event);
	gdk_flush ();
	gdk_error_trap_pop ();
}

static gboolean
render_page (PSDocument *gs, int page)
{
	g_assert (gs != NULL);

	if (!gs->gs_filename) {
		return FALSE;
	}

	if (gs->structured_doc && gs->doc) {
		if (ps_interpreter_is_ready (gs)) {
			ps_document_next_page (gs);
		} else {
			ps_interpreter_start (gs);
			send_ps (gs, gs->doc->beginprolog, gs->doc->lenprolog, FALSE);
			send_ps (gs, gs->doc->beginsetup, gs->doc->lensetup, FALSE);
		}

		send_ps (gs, gs->doc->pages[page].begin,
			 gs->doc->pages[page].len, FALSE);
	} else {
		/* Unstructured document
		 *
		 * In the case of non structured documents,
		 * GS read the PS from the  actual file (via command
		 * line. Hence, ggv only send a signal next page.
		 * If ghostview is not running it is usually because
		 * the last page of the file was displayed. In that
		 * case, ggv restarts GS again and the first page is displayed.
		 */

		if (!ps_interpreter_is_ready (gs)) {
			ps_interpreter_start (gs);
		}
		ps_document_next_page (gs);
	}

	return TRUE;
}

static void
ps_async_renderer_render_pixbuf (EvAsyncRenderer *renderer,
				 gint             page,
				 gdouble          scale,
				 gint             rotation)
{
	PSDocument *gs = PS_DOCUMENT (renderer);

	if (gs->pstarget == NULL) {
		gs->target_window = gtk_window_new (GTK_WINDOW_POPUP);
	        gtk_widget_realize (gs->target_window);
		gs->pstarget = gs->target_window->window;

	        g_assert (gs->pstarget != NULL);

		g_signal_connect (gs->target_window, "event",
			    	  G_CALLBACK (ps_document_widget_event),
			          gs);
	}

	setup_pixmap (gs, page, scale, rotation);
	setup_page (gs, page, scale, rotation);

	render_page (gs, page);
}

static void
ps_async_renderer_iface_init (EvAsyncRendererIface *iface)
{
	iface->render_pixbuf = ps_async_renderer_render_pixbuf;
}

/* EvFileExporterIface */
static gboolean
ps_document_file_exporter_format_supported (EvFileExporter      *exporter,
					    EvFileExporterFormat format)
{
	return (format == EV_FILE_FORMAT_PS);
}

static void
ps_document_file_exporter_begin (EvFileExporter      *exporter,
				 EvFileExporterFormat format,
				 const char          *filename,
				 int                  first_page,
				 int                  last_page,
				 double               width,
				 double               height,
				 gboolean             duplex)
{
	PSDocument *document = PS_DOCUMENT (exporter);

	if (document->structured_doc) {
		g_free (document->ps_export_pagelist);
	
		document->ps_export_pagelist = g_new0 (int, document->doc->numpages);
	}

	document->ps_export_filename = g_strdup (filename);
}

static void
ps_document_file_exporter_do_page (EvFileExporter *exporter, EvRenderContext *rc)
{
	PSDocument *document = PS_DOCUMENT (exporter);
	
	if (document->structured_doc) {
		document->ps_export_pagelist[rc->page] = 1;
	}
}

static void
ps_document_file_exporter_end (EvFileExporter *exporter)
{
	PSDocument *document = PS_DOCUMENT (exporter);

	if (!document->structured_doc) {
		save_document (document, document->ps_export_filename);
	} else {
		save_page_list (document, document->ps_export_pagelist,
				document->ps_export_filename);
		g_free (document->ps_export_pagelist);
		g_free (document->ps_export_filename);	
		document->ps_export_pagelist = NULL;
		document->ps_export_filename = NULL;
	}
}

static void
ps_document_file_exporter_iface_init (EvFileExporterIface *iface)
{
	iface->format_supported = ps_document_file_exporter_format_supported;
	iface->begin = ps_document_file_exporter_begin;
	iface->do_page = ps_document_file_exporter_do_page;
	iface->end = ps_document_file_exporter_end;
}
