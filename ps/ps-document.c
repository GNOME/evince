/* Ghostscript widget for GTK/GNOME
 * 
 * Copyright (C) 1998 - 2005 the Free Software Foundation
 * 
 * Authors: Jonathan Blandford, Jaka Mocnik
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
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <gtk/gtk.h>
#include <gtk/gtkobject.h>
#include <gdk/gdkprivate.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>
#include <glib/gi18n.h>
#include <X11/Intrinsic.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <math.h>

#include "ps-document.h"
#include "ev-debug.h"
#include "gsdefaults.h"
#include "ev-ps-exporter.h"
#include "ev-async-renderer.h"

#define MAX_BUFSIZE 1024

#define PS_DOCUMENT_IS_COMPRESSED(gs) (PS_DOCUMENT(gs)->gs_filename_unc != NULL)
#define PS_DOCUMENT_GET_PS_FILE(gs)   (PS_DOCUMENT_IS_COMPRESSED(gs) ? \
                                       PS_DOCUMENT(gs)->gs_filename_unc : \
                                       PS_DOCUMENT(gs)->gs_filename)

/* structure to describe section of file to send to ghostscript */
struct record_list
{
	FILE *fp;
	long begin;
	guint len;
	gboolean seek_needed;
	gboolean close;
	struct record_list *next;
};

static gboolean broken_pipe = FALSE;

/* Forward declarations */
static void	ps_document_init			(PSDocument           *gs);
static void	ps_document_class_init			(PSDocumentClass      *klass);
static void	send_ps					(PSDocument           *gs,
							 long                  begin,
							 unsigned int          len,
							 gboolean	       close);
static void	output					(gpointer              data,
							 gint                  source,
							 GdkInputCondition     condition);
static void	input					(gpointer              data,
							 gint		       source,
							 GdkInputCondition     condition);
static void	stop_interpreter			(PSDocument           *gs);
static gint	start_interpreter			(PSDocument	      *gs);
static void	ps_document_document_iface_init		(EvDocumentIface      *iface);
static void	ps_document_ps_exporter_iface_init	(EvPSExporterIface    *iface);
static void	ps_async_renderer_iface_init		(EvAsyncRendererIface *iface);

G_DEFINE_TYPE_WITH_CODE (PSDocument, ps_document, G_TYPE_OBJECT,
                         {
				 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT,
							ps_document_document_iface_init);
				 G_IMPLEMENT_INTERFACE (EV_TYPE_PS_EXPORTER,
							ps_document_ps_exporter_iface_init);
				 G_IMPLEMENT_INTERFACE (EV_TYPE_ASYNC_RENDERER,
							ps_async_renderer_iface_init);
			 });

static GObjectClass *parent_class = NULL;
static PSDocumentClass *gs_class = NULL;

static void
ps_document_init (PSDocument *gs)
{
	gs->bpixmap = NULL;

	gs->interpreter_pid = -1;

	gs->busy = FALSE;
	gs->gs_filename = 0;
	gs->gs_filename_unc = 0;

	broken_pipe = FALSE;

	gs->structured_doc = FALSE;
	gs->reading_from_pipe = FALSE;
	gs->send_filename_to_gs = FALSE;

	gs->doc = NULL;

	gs->interpreter_input = -1;
	gs->interpreter_output = -1;
	gs->interpreter_err = -1;
	gs->interpreter_input_id = 0;
	gs->interpreter_output_id = 0;
	gs->interpreter_error_id = 0;

	gs->ps_input = NULL;
	gs->input_buffer = NULL;
	gs->input_buffer_ptr = NULL;
	gs->bytes_left = 0;
	gs->buffer_bytes_left = 0;

	gs->gs_status = _("No document loaded.");

	gs->ps_export_pagelist = NULL;
	gs->ps_export_filename = NULL;
}

static void
ps_document_dispose (GObject *object)
{
	PSDocument *gs = PS_DOCUMENT (object);

	g_return_if_fail (gs != NULL);

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

	if (gs->gs_filename_unc) {
		unlink(gs->gs_filename_unc);
		g_free(gs->gs_filename_unc);
		gs->gs_filename_unc = NULL;
	}

	if (gs->bpixmap) {
		gdk_drawable_unref (gs->bpixmap);
	}

	if(gs->input_buffer) {
		g_free(gs->input_buffer);
		gs->input_buffer = NULL;
	}

	if (gs->target_window) {
		gtk_widget_destroy (gs->target_window);
		gs->target_window = NULL;
		gs->pstarget = NULL;
	}

	stop_interpreter (gs);

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
ps_document_class_init(PSDocumentClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) klass;
	parent_class = g_type_class_peek_parent (klass);
	gs_class = klass;

	object_class->dispose = ps_document_dispose;	

	klass->gs_atom = gdk_atom_intern ("GHOSTVIEW", FALSE);
	klass->next_atom = gdk_atom_intern ("NEXT", FALSE);
	klass->page_atom = gdk_atom_intern ("PAGE", FALSE);
	klass->string_atom = gdk_atom_intern ("STRING", FALSE);
}

static void
push_pixbuf (PSDocument *gs)
{
	GdkColormap *cmap;
	GdkPixbuf *pixbuf;
	int width, height;
	
	if (gs->pstarget == NULL)
		return;

	cmap = gdk_window_get_colormap (gs->pstarget);
	gdk_drawable_get_size (gs->bpixmap, &width, &height);
	LOG ("Get from drawable\n");
	pixbuf =  gdk_pixbuf_get_from_drawable (NULL, gs->bpixmap, cmap,
				      	        0, 0, 0, 0,
					        width, height);
	LOG ("Get from drawable done\n");
	g_signal_emit_by_name (gs, "render_finished", pixbuf);
	g_object_unref (pixbuf);
}

static void
interpreter_failed (PSDocument *gs, char *msg)
{
	LOG ("Interpreter failed %s", msg);

	push_pixbuf (gs);

	stop_interpreter (gs);
}

static gboolean
ps_document_widget_event (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	PSDocument *gs = (PSDocument *) data;

	if(event->type != GDK_CLIENT_EVENT)
		return FALSE;

	gs->message_window = event->client.data.l[0];

	if (event->client.message_type == gs_class->page_atom) {
		LOG ("GS rendered the document");
		gs->busy = FALSE;

		push_pixbuf (gs);
		LOG ("Pixbuf pushed");
	}

	return TRUE;
}

static void
send_ps (PSDocument *gs, long begin, unsigned int len, gboolean close)
{
	struct record_list *ps_new;

	if (gs->interpreter_input < 0) {
		g_critical("No pipe to gs: error in send_ps().");
		return;
	}

	ps_new = g_new0 (struct record_list, 1);
	ps_new->fp = gs->gs_psfile;
	ps_new->begin = begin;
	ps_new->len = len;
	ps_new->seek_needed = TRUE;
	ps_new->close = close;
	ps_new->next = NULL;

	if (gs->input_buffer == NULL) {
		gs->input_buffer = g_malloc(MAX_BUFSIZE);
	}

	if (gs->ps_input == NULL) {
		gs->input_buffer_ptr = gs->input_buffer;
		gs->bytes_left = len;
		gs->buffer_bytes_left = 0;
		gs->ps_input = ps_new;
		gs->interpreter_input_id = gdk_input_add
			(gs->interpreter_input, GDK_INPUT_WRITE, input, gs);
	} else {
		struct record_list *p = gs->ps_input;
		while (p->next != NULL) {
			p = p->next;
		}
		p->next = ps_new;
	}
}

static float
get_xdpi (PSDocument *gs)
{
	return 25.4 * gdk_screen_width() / gdk_screen_width_mm();
}

static float
get_ydpi (PSDocument *gs)
{
	return 25.4 * gdk_screen_height() / gdk_screen_height_mm();
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

	if(gs->bpixmap) {
		int w, h;

		gdk_drawable_get_size (gs->bpixmap, &w, &h);

		if (pixmap_width != w || h != pixmap_height) {
			gdk_drawable_unref (gs->bpixmap);
			gs->bpixmap = NULL;
			stop_interpreter (gs);
		}
	}

	if (!gs->bpixmap) {
		LOG ("Create pixmap");

		fill = gdk_gc_new (gs->pstarget);
		colormap = gdk_drawable_get_colormap (gs->pstarget);
		gdk_color_alloc (colormap, &white);
		gdk_gc_set_foreground (fill, &white);
		gs->bpixmap = gdk_pixmap_new (gs->pstarget, pixmap_width,
					      pixmap_height, -1);
		gdk_draw_rectangle (gs->bpixmap, fill, TRUE,
	                            0, 0, pixmap_width, pixmap_height);
	}
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

	g_return_if_fail (PS_IS_DOCUMENT (gs));

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
setup_page (PSDocument *gs, int page, double scale, int rotation)
{
	gchar *buf;
	char scaled_xdpi[G_ASCII_DTOSTR_BUF_SIZE];	
	char scaled_ydpi[G_ASCII_DTOSTR_BUF_SIZE];
	int urx, ury, llx, lly;

	LOG ("Setup the page");

	get_page_box (gs, page, &urx, &ury, &llx, &lly);
	g_ascii_dtostr (scaled_xdpi, G_ASCII_DTOSTR_BUF_SIZE, get_xdpi (gs) * scale);
	g_ascii_dtostr (scaled_ydpi, G_ASCII_DTOSTR_BUF_SIZE, get_ydpi (gs) * scale);

	buf = g_strdup_printf ("%ld %d %d %d %d %d %s %s %d %d %d %d",
			       0L, rotation, llx, lly, urx, ury,
			       scaled_xdpi, scaled_ydpi,
			       0, 0, 0, 0);
	LOG ("GS property %s", buf);

	gdk_property_change (gs->pstarget, gs_class->gs_atom, gs_class->string_atom,
			     8, GDK_PROP_MODE_REPLACE, (guchar *)buf, strlen(buf));
	g_free (buf);
	
	gdk_flush ();
}

static void
close_pipe (int p[2])
{
	if (p[0] != -1) {
		close (p[0]);
	}
	if (p[1] != -1) {
		close (p[1]);
	}
}

static gboolean
is_interpreter_ready (PSDocument *gs)
{
	return (gs->interpreter_pid != -1 && !gs->busy && gs->ps_input == NULL);
}

static void
output (gpointer data, gint source, GdkInputCondition condition)
{
	char buf[MAX_BUFSIZE + 1];
	guint bytes = 0;
	PSDocument *gs = PS_DOCUMENT(data);

	if (source == gs->interpreter_output) {
		bytes = read(gs->interpreter_output, buf, MAX_BUFSIZE);
		if (bytes == 0) {            /* EOF occurred */
			close (gs->interpreter_output);
			gs->interpreter_output = -1;
			gdk_input_remove (gs->interpreter_output_id);
			return;
		} else if (bytes == -1) {
			/* trouble... */
			interpreter_failed (gs, NULL);
			return;
		}
		if (gs->interpreter_err == -1) {
			interpreter_failed (gs, NULL);
		}
	} else if (source == gs->interpreter_err) {
		bytes = read (gs->interpreter_err, buf, MAX_BUFSIZE);
		if (bytes == 0) {            /* EOF occurred */
			close (gs->interpreter_err);
			gs->interpreter_err = -1;
			gdk_input_remove (gs->interpreter_error_id);
			return;
		} else if (bytes == -1) {
			/* trouble... */
			interpreter_failed (gs, NULL);
			return;
		}
		if (gs->interpreter_output == -1) {
			interpreter_failed(gs, NULL);
		}
	}

	if (bytes > 0) {
		buf[bytes] = '\0';
		printf ("%s", buf);
	}
}

static void
catchPipe (int i)
{
	broken_pipe = True;
}

static void
input(gpointer data, gint source, GdkInputCondition condition)
{
	PSDocument *gs = PS_DOCUMENT(data);
	int bytes_written;
	void (*oldsig) (int);
	oldsig = signal(SIGPIPE, catchPipe);

	LOG ("Input");

	do {
		if (gs->buffer_bytes_left == 0) {
			/* Get a new section if required */
			if (gs->ps_input && gs->bytes_left == 0) {
				struct record_list *ps_old = gs->ps_input;
				gs->ps_input = ps_old->next;
				if (ps_old->close && NULL != ps_old->fp)
					fclose (ps_old->fp);
				g_free (ps_old);
			}

			/* Have to seek at the beginning of each section */
			if (gs->ps_input && gs->ps_input->seek_needed) {
				fseek (gs->ps_input->fp, gs->ps_input->begin, SEEK_SET);
				gs->ps_input->seek_needed = FALSE;
				gs->bytes_left = gs->ps_input->len;
			}

			if (gs->bytes_left > MAX_BUFSIZE) {
				gs->buffer_bytes_left = fread (gs->input_buffer, sizeof(char),
							       MAX_BUFSIZE, gs->ps_input->fp);
			} else if (gs->bytes_left > 0) {
				gs->buffer_bytes_left = fread (gs->input_buffer, sizeof(char),
							       gs->bytes_left, gs->ps_input->fp);
			} else {
				gs->buffer_bytes_left = 0;
			}
			if (gs->bytes_left > 0 && gs->buffer_bytes_left == 0) {
				interpreter_failed (gs, NULL); /* Error occurred */
			}
			gs->input_buffer_ptr = gs->input_buffer;
			gs->bytes_left -= gs->buffer_bytes_left;
		}

		if (gs->buffer_bytes_left > 0) {
			bytes_written = write (gs->interpreter_input,
                	                       gs->input_buffer_ptr, gs->buffer_bytes_left);

			if (broken_pipe) {
				interpreter_failed (gs, g_strdup(_("Broken pipe.")));
				broken_pipe = FALSE;
				interpreter_failed (gs, NULL);
			} else if (bytes_written == -1) {
				if ((errno != EWOULDBLOCK) && (errno != EAGAIN)) {
					interpreter_failed (gs, NULL);   /* Something bad happened */
				}
				} else {
				gs->buffer_bytes_left -= bytes_written;
				gs->input_buffer_ptr += bytes_written;
			}
		}
	} while (gs->ps_input && gs->buffer_bytes_left == 0);

	signal (SIGPIPE, oldsig);

	if (gs->ps_input == NULL && gs->buffer_bytes_left == 0) {
		if (gs->interpreter_input_id != 0) {
			gdk_input_remove (gs->interpreter_input_id);
			gs->interpreter_input_id = 0;
		}
	}
}

static int
start_interpreter (PSDocument *gs)
{
	int std_in[2] = { -1, -1 };   /* pipe to interp stdin */
	int std_out[2];               /* pipe from interp stdout */
	int std_err[2];               /* pipe from interp stderr */

#define NUM_ARGS    100
#define NUM_GS_ARGS (NUM_ARGS - 20)
#define NUM_ALPHA_ARGS 10

	char *argv[NUM_ARGS], *dir, *gv_env, *gs_path;
	char **gs_args, **alpha_args = NULL;
	int argc = 0, i;

	LOG ("Start the interpreter");

	if(!gs->gs_filename)
		return 0;

	stop_interpreter(gs);

	/* set up the args... */
	gs_path = g_find_program_in_path ("gs");
	gs_args = g_strsplit (gs_path, " ", NUM_GS_ARGS);
	g_free (gs_path);
	for(i = 0; i < NUM_GS_ARGS && gs_args[i]; i++, argc++) {
		argv[argc] = gs_args[i];
	}

	alpha_args = g_strsplit (ALPHA_PARAMS, " ", NUM_ALPHA_ARGS);
	for(i = 0; i < NUM_ALPHA_ARGS && alpha_args[i]; i++, argc++) {
		argv[argc] = alpha_args[i];
	}

	argv[argc++] = "-dNOPAUSE";
	argv[argc++] = "-dQUIET";
	argv[argc++] = "-dSAFER";

	/* set up the pipes */
	if (gs->send_filename_to_gs) {
		argv[argc++] = PS_DOCUMENT_GET_PS_FILE (gs);
		argv[argc++] = "-c";
		argv[argc++] = "quit";
	} else {
		argv[argc++] = "-";
	}

	argv[argc++] = NULL;

	if (!gs->reading_from_pipe && !gs->send_filename_to_gs) {
		if (pipe (std_in) == -1) {
			g_critical ("Unable to open pipe to Ghostscript.");
			return -1;
		}
	}

	if (pipe (std_out) == -1) {
		close_pipe (std_in);
		return -1;
	}

	if (pipe(std_err) == -1) {
		close_pipe (std_in);
		close_pipe (std_out);
		return -1;
	}

	gv_env = g_strdup_printf ("GHOSTVIEW=%ld %ld",
                           	  gdk_x11_drawable_get_xid (gs->pstarget),
				  gdk_x11_drawable_get_xid (gs->bpixmap));
	LOG ("Launching ghostview with env %s", gv_env);

	gs->interpreter_pid = fork ();
	switch (gs->interpreter_pid) {
		case -1:                     /* error */
			close_pipe (std_in);
			close_pipe (std_out);
 			close_pipe (std_err);
			return -2;
			break;
		case 0:                      /* child */
			close (std_out[0]);
			dup2 (std_out[1], 1);
			close (std_out[1]);

			close (std_err[0]);
			dup2 (std_err[1], 2);
			close (std_err[1]);

			if (!gs->reading_from_pipe) {
				if (gs->send_filename_to_gs) {
					int stdinfd;
					/* just in case gs tries to read from stdin */
					stdinfd = open("/dev/null", O_RDONLY);
					if (stdinfd != 0) {
						dup2(stdinfd, 0);
						close(stdinfd);
					}
				} else {
					close (std_in[1]);
					dup2 (std_in[0], 0);
					close (std_in[0]);
				}
			}

			putenv(gv_env);

			/* change to directory where the input file is. This helps
			 * with postscript-files which include other files using
			 * a relative path */
			dir = g_path_get_dirname (gs->gs_filename);
			chdir (dir);
			g_free (dir);

			execvp (argv[0], argv);

			/* Notify error */
			g_critical ("Unable to execute [%s]\n", argv[0]);
			g_strfreev (gs_args);
			g_free (gv_env);
			g_strfreev (alpha_args);
			_exit (1);
			break;
		default:                     /* parent */
			if (!gs->send_filename_to_gs && !gs->reading_from_pipe) {
				int result;
				close (std_in[0]);
				/* use non-blocking IO for pipe to ghostscript */
				result = fcntl (std_in[1], F_GETFL, 0);
				fcntl (std_in[1], F_SETFL, result | O_NONBLOCK);
				gs->interpreter_input = std_in[1];
			} else {
				gs->interpreter_input = -1;
			}
			close (std_out[1]);

 			gs->interpreter_output = std_out[0];
			close (std_err[1]);
			gs->interpreter_err = std_err[0];
			gs->interpreter_output_id =
				gdk_input_add (std_out[0], GDK_INPUT_READ, output, gs);
			gs->interpreter_error_id =
				gdk_input_add (std_err[0], GDK_INPUT_READ, output, gs);
			break;
	}

	return TRUE;
}

static void
stop_interpreter(PSDocument * gs)
{
	if (gs->interpreter_pid > 0) {
		int status = 0;
		LOG ("Stop the interpreter");
		kill (gs->interpreter_pid, SIGTERM);
		while ((wait(&status) == -1) && (errno == EINTR));
		gs->interpreter_pid = -1;
		if (status == 1) {
			gs->gs_status = _("Interpreter failed.");
		}
	}

	if (gs->interpreter_input >= 0) {
		close (gs->interpreter_input);
		gs->interpreter_input = -1;
		if (gs->interpreter_input_id != 0) {
			gdk_input_remove(gs->interpreter_input_id);
			gs->interpreter_input_id = 0;
		}
		while (gs->ps_input) {
			struct record_list *ps_old = gs->ps_input;
			gs->ps_input = gs->ps_input->next;
			if (ps_old->close && NULL != ps_old->fp)
				fclose (ps_old->fp);
			g_free (ps_old);
		}
	}

	if (gs->interpreter_output >= 0) {
		close (gs->interpreter_output);
		gs->interpreter_output = -1;
		if (gs->interpreter_output_id) {
			gdk_input_remove (gs->interpreter_output_id);
			gs->interpreter_output_id = 0;
		}
	}

	if (gs->interpreter_err >= 0) {
		close (gs->interpreter_err);
		gs->interpreter_err = -1;
		if (gs->interpreter_error_id) {
			gdk_input_remove (gs->interpreter_error_id);
			gs->interpreter_error_id = 0;
		}
	}

	gs->busy = FALSE;
}

/* If file exists and is a regular file then return its length, else -1 */
static gint
file_length (const gchar * filename)
{
	struct stat stat_rec;

	if (filename && (stat (filename, &stat_rec) == 0) && S_ISREG (stat_rec.st_mode))
		return stat_rec.st_size;
	else
		return -1;
}

/* Test if file exists, is a regular file and its length is > 0 */
static gboolean
file_readable(const char *filename)
{
	return (file_length (filename) > 0);
}

/*
 * Decompress gs->gs_filename if necessary
 * Set gs->filename_unc to the name of the uncompressed file or NULL.
 * Error reporting via signal 'interpreter_message'
 * Return name of input file to use or NULL on error..
 */
static gchar *
check_filecompressed (PSDocument * gs)
{
	FILE *file;
	gchar buf[1024];
	gchar *filename, *filename_unc, *filename_err, *cmdline;
	const gchar *cmd;
	int fd;

	cmd = NULL;

	if ((file = fopen(gs->gs_filename, "r")) &&
	    (fread (buf, sizeof(gchar), 3, file) == 3)) {
		if ((buf[0] == '\037') && ((buf[1] == '\235') || (buf[1] == '\213'))) {
			/* file is gzipped or compressed */
			cmd = gtk_gs_defaults_get_ungzip_cmd ();
		} else if (strncmp (buf, "BZh", 3) == 0) {
			/* file is compressed with bzip2 */
			cmd = gtk_gs_defaults_get_unbzip2_cmd ();
		}
	}

	if (NULL != file)
		fclose(file);

	if (!cmd)
		return gs->gs_filename;

	/* do the decompression */
	filename = g_shell_quote (gs->gs_filename);
	filename_unc = g_strconcat (g_get_tmp_dir (), "/evinceXXXXXX", NULL);
	if ((fd = mkstemp (filename_unc)) < 0) {
		g_free (filename_unc);
		g_free (filename);
		return NULL;
	}
	close (fd);

	filename_err = g_strconcat (g_get_tmp_dir (), "/evinceXXXXXX", NULL);
	if ((fd = mkstemp(filename_err)) < 0) {
		g_free (filename_err);
		g_free (filename_unc);
		g_free (filename);
		return NULL;
	}
	close (fd);

	cmdline = g_strdup_printf ("%s %s >%s 2>%s", cmd,
                                   filename, filename_unc, filename_err);
	if (system (cmdline) == 0 &&
	    file_readable (filename_unc) &&
	    file_length (filename_err) == 0) {
		/* sucessfully uncompressed file */
		gs->gs_filename_unc = filename_unc;
	} else {
		gchar *filename_dsp;
		gchar *msg;

		/* report error */
		filename_dsp = g_filename_display_name (gs->gs_filename);
		msg = g_strdup_printf (_("Error while decompressing file %s:\n"), filename_dsp);
		g_free (filename_dsp);
		
		interpreter_failed (gs, msg);
		g_free (msg);
		unlink (filename_unc);
		g_free (filename_unc);
		filename_unc = NULL;
	}

	unlink (filename_err);
	g_free (filename_err);
	g_free (cmdline);
	g_free (filename);

	return filename_unc;
}

static gint
ps_document_enable_interpreter(PSDocument *gs)
{
	g_return_val_if_fail (PS_IS_DOCUMENT (gs), FALSE);

	if (!gs->gs_filename)
		return 0;

	return start_interpreter (gs);
}

static gboolean
document_load (PSDocument *gs, const gchar *fname)
{
	g_return_val_if_fail (PS_IS_DOCUMENT(gs), FALSE);

	LOG ("Load the document");

	if (fname == NULL) {
		gs->gs_status = "";
		return FALSE;
	}

	/* prepare this document */
	gs->structured_doc = FALSE;
	gs->send_filename_to_gs = TRUE;
	gs->gs_filename = g_strdup (fname);

	if ((gs->reading_from_pipe = (strcmp (fname, "-") == 0))) {
		gs->send_filename_to_gs = FALSE;
	} else {
		/*
		 * We need to make sure that the file is loadable/exists!
		 * otherwise we want to exit without loading new stuff...
		 */
		gchar *filename = NULL;

		if (!file_readable(fname)) {
			gchar *filename_dsp;
			gchar *msg;

			filename_dsp = g_filename_display_name (fname);
			msg = g_strdup_printf (_("Cannot open file %s.\n"), filename_dsp);
			g_free (filename_dsp);
			
			interpreter_failed (gs, msg);
			g_free (msg);
			gs->gs_status = _("File is not readable.");
		} else {
			filename = check_filecompressed(gs);
		}

		if (!filename || (gs->gs_psfile = fopen(filename, "r")) == NULL) {
			interpreter_failed (gs, NULL);
			return FALSE;
		}

		/* we grab the vital statistics!!! */
		gs->doc = psscan(gs->gs_psfile, TRUE, filename);

		if ((!gs->doc->epsf && gs->doc->numpages > 0) ||
		    (gs->doc->epsf && gs->doc->numpages > 1)) {
			gs->structured_doc = TRUE;
			gs->send_filename_to_gs = FALSE;
		}
	}

	gs->gs_status = _("Document loaded.");

	return TRUE;
}

static gboolean
ps_document_next_page (PSDocument *gs)
{
	XEvent event;

	LOG ("Make ghostscript render next page");

	g_return_val_if_fail (PS_IS_DOCUMENT(gs), FALSE);
	g_return_val_if_fail (gs->interpreter_pid != 0, FALSE);
	g_return_val_if_fail (gs->busy != TRUE, FALSE);

	gs->busy = TRUE;

	event.xclient.type = ClientMessage;
	event.xclient.display = gdk_display;
	event.xclient.window = gs->message_window;
	event.xclient.message_type = gdk_x11_atom_to_xatom(gs_class->next_atom);
	event.xclient.format = 32;

	gdk_error_trap_push ();
	XSendEvent (gdk_display, gs->message_window, FALSE, 0, &event);
	gdk_flush ();
	gdk_error_trap_pop ();

	return TRUE;
}

static gboolean
render_page (PSDocument *gs, int page)
{
	g_return_val_if_fail(gs != NULL, FALSE);
	g_return_val_if_fail(PS_IS_DOCUMENT(gs), FALSE);

	if(!gs->gs_filename) {
		return FALSE;
	}

	if (gs->structured_doc && gs->doc) {
		LOG ("It's a structured document, let's send one page to gs");

		if (is_interpreter_ready (gs)) {
			ps_document_next_page (gs);
		} else {
			ps_document_enable_interpreter (gs);
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

		LOG ("It's an unstructured document, gs will just read the file");

		if (!is_interpreter_ready (gs)) {
			ps_document_enable_interpreter(gs);
		}
		ps_document_next_page(gs);
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
		    g_set_error(error,
				G_FILE_ERROR,
				G_FILE_ERROR_NOENT,
				_("Failed to load document '%s'. Ghostscript interpreter was not found in path"),
				filename);
		    g_free (filename_dsp);
		    result = FALSE;	
	} else {
		result = document_load (PS_DOCUMENT (document), filename);
		if (!result) {
	    		gchar *filename_dsp;
	    		filename_dsp = g_filename_display_name (filename);
			
			g_set_error (error, G_FILE_ERROR,
				     G_FILE_ERROR_FAILED,
				     _("Failed to load document '%s'"),
				     filename_dsp);
			g_free (filename_dsp);
		}
		g_free (gs_path);
	}
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

	src_file = fopen (PS_DOCUMENT_GET_PS_FILE(document), "r");
	if (src_file) {
		struct stat stat_rec;

		if (stat (PS_DOCUMENT_GET_PS_FILE(document), &stat_rec) == 0) {
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

	pscopydoc (sink, PS_DOCUMENT_GET_PS_FILE(document), 
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
ps_document_get_n_pages (EvDocument  *document)
{
	PSDocument *ps = PS_DOCUMENT (document);

	g_return_val_if_fail (ps != NULL, -1);

	if (!ps->gs_filename || !ps->doc) {
		return -1;
	}

	return ps->structured_doc ? ps->doc->numpages : 1;
}

static void
ps_document_get_page_size (EvDocument   *document,
			   int           page,
			   double       *width,
			   double       *height)
{
	PSDocument *gs = PS_DOCUMENT (document);
	int urx, ury, llx, lly;

	get_page_box (PS_DOCUMENT (document), page, &urx, &ury, &llx, &lly);

	if (width) {
		*width = (urx - llx) / 72.0 * get_xdpi (gs) + 0.5;
	}

	if (height) {
		*height = (ury - lly) / 72.0 * get_ydpi (gs) + 0.5;
	}
}

static gboolean
ps_document_can_get_text (EvDocument *document)
{
	return FALSE;
}

static void
ps_async_renderer_render_pixbuf (EvAsyncRenderer *renderer, int page, double scale, int rotation)
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

static EvDocumentInfo *
ps_document_get_info (EvDocument *document)
{
	EvDocumentInfo *info;
	PSDocument *ps = PS_DOCUMENT (document);

	info = g_new0 (EvDocumentInfo, 1);
	info->fields_mask = EV_DOCUMENT_INFO_TITLE |
	                    EV_DOCUMENT_INFO_FORMAT |
			    EV_DOCUMENT_INFO_CREATOR |
			    EV_DOCUMENT_INFO_N_PAGES;
	info->title = g_strdup (ps->doc->title);
	info->format = ps->doc->epsf ? g_strdup (_("Encapsulated PostScript"))
		                     : g_strdup (_("PostScript"));
	info->creator = g_strdup (ps->doc->creator);
	info->n_pages = ev_document_get_n_pages (document);

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

static void
ps_async_renderer_iface_init (EvAsyncRendererIface *iface)
{
	iface->render_pixbuf = ps_async_renderer_render_pixbuf;
}

static void
ps_document_ps_export_begin (EvPSExporter *exporter, const char *filename,
			     int first_page, int last_page,
                             double width, double height, gboolean duplex)
{
	PSDocument *document = PS_DOCUMENT (exporter);

	if (document->structured_doc) {
		g_free (document->ps_export_pagelist);
	
		document->ps_export_pagelist = g_new0 (int, document->doc->numpages);
	}

	document->ps_export_filename = g_strdup (filename);
}

static void
ps_document_ps_export_do_page (EvPSExporter *exporter, EvRenderContext *rc)
{
	PSDocument *document = PS_DOCUMENT (exporter);
	
	if (document->structured_doc) {
		document->ps_export_pagelist[rc->page] = 1;
	}
}

static void
ps_document_ps_export_end (EvPSExporter *exporter)
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
ps_document_ps_exporter_iface_init (EvPSExporterIface *iface)
{
	iface->begin = ps_document_ps_export_begin;
	iface->do_page = ps_document_ps_export_do_page;
	iface->end = ps_document_ps_export_end;
}
