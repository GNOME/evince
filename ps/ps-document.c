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
 
/*
Ghostview interface to ghostscript

When the GHOSTVIEW environment variable is set, ghostscript draws on
an existing drawable rather than creating its own window.  Ghostscript
can be directed to draw on either a window or a pixmap.

Drawing on a Window

The GHOSTVIEW environment variable contains the window id of the target
window.  The window id is an integer.  Ghostscript will use the attributes
of the window to obtain the width, height, colormap, screen, and visual of
the window. The remainder of the information is gotten from the GHOSTVIEW
property on that window.


Drawing on a Pixmap

The GHOSTVIEW environment variable contains a window id and a pixmap id.
They are integers separated by white space.  Ghostscript will use the
attributes of the window to obtain the colormap, screen, and visual to use.
The width and height will be obtained from the pixmap. The remainder of the
information, is gotten from the GHOSTVIEW property on the window.  In this
case, the property is deleted when read.

The GHOSTVIEW environment variable

parameters:	window-id [pixmap-id]

scanf format:	"%d %d"

explanation of parameters:

	window-id: tells ghostscript where to
		    - read the GHOSTVIEW property
		    - send events
		    If pixmap-id is not present,
		    ghostscript will draw on this window.

	pixmap-id: If present, tells ghostscript that a pixmap will be used
		    as the final destination for drawing.  The window will
		    not be touched for drawing purposes.

The GHOSTVIEW property

type:	STRING

parameters:

    bpixmap orient llx lly urx ury xdpi ydpi [left bottom top right]

scanf format: "%d %d %d %d %d %d %f %f %d %d %d %d"

explanation of parameters:

	bpixmap: pixmap id of the backing pixmap for the window.  If no
		pixmap is to be used, this parameter should be zero.  This
		parameter must be zero when drawing on a pixmap.

	orient:	orientation of the page.  The number represents clockwise
		rotation of the paper in degrees.  Permitted values are
		0, 90, 180, 270.

	llx, lly, urx, ury: Bounding box of the drawable.  The bounding box
		is specified in PostScript points in default user coordinates.

	xdpi, ydpi: Resolution of window.  (This can be derived from the
		other parameters, but not without roundoff error.  These
		values are included to avoid this error.)

	left, bottom, top, right: (optional)
		Margins around the window.  The margins extend the imageable
		area beyond the boundaries of the window.  This is primarily
		used for popup zoom windows.  I have encountered several
		instances of PostScript programs that position themselves
		with respect to the imageable area.  The margins are specified
		in PostScript points.  If omitted, the margins are assumed to
		be 0.

Events from ghostscript

If the final destination is a pixmap, the client will get a property notify
event when ghostscript reads the GHOSTVIEW property causing it to be deleted.

Ghostscript sends events to the window where it read the GHOSTVIEW property.
These events are of type ClientMessage.  The message_type is set to
either PAGE or DONE.  The first long data value gives the window to be used
to send replies to ghostscript.  The second long data value gives the primary
drawable.  If rendering to a pixmap, it is the primary drawable.  If rendering
to a window, the backing pixmap is the primary drawable.  If no backing pixmap
is employed, then the window is the primary drawable.  This field is necessary
to distinguish multiple ghostscripts rendering to separate pixmaps where the
GHOSTVIEW property was placed on the same window.

The PAGE message indicates that a "page" has completed.  Ghostscript will
wait until it receives a ClientMessage whose message_type is NEXT before
continuing.

The DONE message indicates that ghostscript has finished processing.

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
#ifdef  HAVE_XINERAMA
#   include <gdk/gdkx.h>
#   include <X11/extensions/Xinerama.h>
#endif /* HAVE_XINERAMA */
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

#ifdef HAVE_LOCALE_H
#   include <locale.h>
#endif

/* if POSIX O_NONBLOCK is not available, use O_NDELAY */
#if !defined(O_NONBLOCK) && defined(O_NDELAY)
#   define O_NONBLOCK O_NDELAY
#endif

#define PS_DOCUMENT_WATCH_INTERVAL 1000
#define PS_DOCUMENT_WATCH_TIMEOUT  2

#define MAX_BUFSIZE 1024

#define PS_DOCUMENT_IS_COMPRESSED(gs)       (PS_DOCUMENT(gs)->gs_filename_unc != NULL)
#define PS_DOCUMENT_GET_PS_FILE(gs)         (PS_DOCUMENT_IS_COMPRESSED(gs) ? \
                                        PS_DOCUMENT(gs)->gs_filename_unc : \
                                        PS_DOCUMENT(gs)->gs_filename)

GCond* pixbuf_cond = NULL;
GMutex* pixbuf_mutex = NULL;
GdkPixbuf *current_pixbuf = NULL;

enum { INTERPRETER_MESSAGE, INTERPRETER_ERROR, LAST_SIGNAL };

enum {
	PROP_0,
	PROP_TITLE
};

/* structure to describe section of file to send to ghostscript */
struct record_list {
  FILE *fp;
  long begin;
  guint len;
  gboolean seek_needed;
  gboolean close;
  struct record_list *next;
};

static gboolean broken_pipe = FALSE;

static void
catchPipe(int i)
{
  broken_pipe = True;
}

/* Forward declarations */
static void ps_document_init(PSDocument * gs);
static void ps_document_class_init(PSDocumentClass * klass);
static void ps_document_emit_error_msg(PSDocument * gs, const gchar * msg);
static void ps_document_finalize(GObject * object);
static void send_ps(PSDocument * gs, long begin, unsigned int len, gboolean close);
static void set_up_page(PSDocument * gs);
static void close_pipe(int p[2]);
static void interpreter_failed(PSDocument * gs);
static float compute_xdpi(void);
static float compute_ydpi(void);
static gboolean compute_size(PSDocument * gs);
static void output(gpointer data, gint source, GdkInputCondition condition);
static void input(gpointer data, gint source, GdkInputCondition condition);
static void stop_interpreter(PSDocument * gs);
static gint start_interpreter(PSDocument * gs);
gboolean computeSize(void);
static gboolean ps_document_set_page_size(PSDocument * gs, gint new_pagesize, gint pageid);
static void ps_document_document_iface_init (EvDocumentIface *iface);
static gboolean ps_document_goto_page(PSDocument * gs);
static gboolean ps_document_widget_event (GtkWidget *widget, GdkEvent *event, gpointer data);

static GObjectClass *parent_class = NULL;

static PSDocumentClass *gs_class = NULL;

static void
ps_document_init (PSDocument *gs)
{
	gs->bpixmap = NULL;

	gs->current_page = 0;
	gs->disable_start = FALSE;
	gs->interpreter_pid = -1;

	gs->width = -1;
	gs->height = -1;
	gs->busy = FALSE;
	gs->changed = FALSE;
	gs->gs_scanstyle = 0;
	gs->gs_filename = 0;
	gs->gs_filename_dsc = 0;
	gs->gs_filename_unc = 0;

	broken_pipe = FALSE;

	gs->structured_doc = FALSE;
	gs->reading_from_pipe = FALSE;
	gs->send_filename_to_gs = FALSE;

	gs->doc = NULL;
	gs->loaded = FALSE;

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

	gs->llx = 0;
	gs->lly = 0;
	gs->urx = 0;
	gs->ury = 0;
	gs->xdpi = compute_xdpi();
	gs->ydpi = compute_ydpi();

	gs->left_margin = 0;
	gs->top_margin = 0;
	gs->right_margin = 0;
	gs->bottom_margin = 0;

	gs->page_x_offset = 0;
	gs->page_y_offset = 0;

	/* Set user defined defaults */
	gs->fallback_orientation = GTK_GS_ORIENTATION_PORTRAIT;
	gs->zoom_factor = 1.0;
	gs->default_size = 1;
	gs->antialiased = TRUE;
	gs->respect_eof = TRUE;

	gs->gs_status = _("No document loaded.");

	pixbuf_cond = g_cond_new ();
	pixbuf_mutex = g_mutex_new ();
}

static void
ps_document_set_property (GObject *object,
		          guint prop_id,
		          const GValue *value,
		          GParamSpec *pspec)
{
	switch (prop_id)

	{
		case PROP_TITLE:
			/* read only */
			break;
	}
}

static void
ps_document_get_property (GObject *object,
		          guint prop_id,
		          GValue *value,
		          GParamSpec *pspec)
{
	PSDocument *ps = PS_DOCUMENT (object);

	switch (prop_id)
	{
		case PROP_TITLE:
			if (ps->doc) {
				g_value_set_string (value, ps->doc->title);
			} else {
				g_value_set_string (value, NULL);
			}
			break;
	}
}

static void
ps_document_class_init(PSDocumentClass *klass)
{
  GObjectClass *object_class;

  object_class = (GObjectClass *) klass;
  parent_class = g_type_class_peek_parent (klass);
  gs_class = klass;

  object_class->finalize = ps_document_finalize;
  object_class->get_property = ps_document_get_property;
  object_class->set_property = ps_document_set_property;

  /* Create atoms */
  klass->gs_atom = gdk_atom_intern("GHOSTVIEW", FALSE);
  klass->next_atom = gdk_atom_intern("NEXT", FALSE);
  klass->page_atom = gdk_atom_intern("PAGE", FALSE);
  klass->string_atom = gdk_atom_intern("STRING", FALSE);

  g_object_class_override_property (object_class, PROP_TITLE, "title");
}

/* Clean all memory and temporal files */
static void
ps_document_cleanup(PSDocument * gs)
{
  g_return_if_fail(gs != NULL);
  g_return_if_fail(GTK_IS_GS(gs));

  stop_interpreter(gs);

  if(gs->gs_psfile) {
    fclose(gs->gs_psfile);
    gs->gs_psfile = NULL;
  }
  if(gs->gs_filename) {
    g_free(gs->gs_filename);
    gs->gs_filename = NULL;
  }
  if(gs->doc) {
    psfree(gs->doc);
    gs->doc = NULL;
  }
  if(gs->gs_filename_dsc) {
    unlink(gs->gs_filename_dsc);
    g_free(gs->gs_filename_dsc);
    gs->gs_filename_dsc = NULL;
  }
  if(gs->gs_filename_unc) {
    unlink(gs->gs_filename_unc);
    g_free(gs->gs_filename_unc);
    gs->gs_filename_unc = NULL;
  }
  gs->current_page = 0;
  gs->loaded = FALSE;
  gs->llx = 0;
  gs->lly = 0;
  gs->urx = 0;
  gs->ury = 0;
}

static gboolean
ps_document_widget_event (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	PSDocument *gs = (PSDocument *) data;

	if(event->type != GDK_CLIENT_EVENT)
		return FALSE;

	gs->message_window = event->client.data.l[0];

	if (event->client.message_type == gs_class->page_atom) {
		GdkColormap *cmap;
		GdkPixbuf *pixbuf;

		LOG ("GS rendered the document");
		gs->busy = FALSE;

		cmap = gdk_window_get_colormap (gs->pstarget);
	
		pixbuf =  gdk_pixbuf_get_from_drawable (NULL, gs->bpixmap, cmap,
					      	        0, 0, 0, 0,
						        gs->width, gs->height);
		g_mutex_lock (pixbuf_mutex);
		current_pixbuf = pixbuf;
		g_cond_signal (pixbuf_cond);
		g_mutex_unlock (pixbuf_mutex);
	}

	return TRUE;
}

static void
ps_document_finalize (GObject * object)
{
	PSDocument *gs;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GTK_IS_GS (object));

	LOG ("Finalize");

	gs = PS_DOCUMENT (object);

	ps_document_cleanup (gs);
	stop_interpreter (gs);

	if(gs->input_buffer) {
		g_free(gs->input_buffer);
		gs->input_buffer = NULL;
	}

	(*G_OBJECT_CLASS(parent_class)->finalize) (object);
}

static void
send_ps(PSDocument * gs, long begin, unsigned int len, gboolean close)
{
  struct record_list *ps_new;

  if(gs->interpreter_input < 0) {
    g_critical("No pipe to gs: error in send_ps().");
    return;
  }

  ps_new = (struct record_list *) g_malloc(sizeof(struct record_list));
  ps_new->fp = gs->gs_psfile;
  ps_new->begin = begin;
  ps_new->len = len;
  ps_new->seek_needed = TRUE;
  ps_new->close = close;
  ps_new->next = NULL;

  if(gs->input_buffer == NULL) {
    gs->input_buffer = g_malloc(MAX_BUFSIZE);
  }

  if(gs->ps_input == NULL) {
    gs->input_buffer_ptr = gs->input_buffer;
    gs->bytes_left = len;
    gs->buffer_bytes_left = 0;
    gs->ps_input = ps_new;
    gs->interpreter_input_id =
      gdk_input_add(gs->interpreter_input, GDK_INPUT_WRITE, input, gs);
  }
  else {
    struct record_list *p = gs->ps_input;
    while(p->next != NULL) {
      p = p->next;
    }
    p->next = ps_new;
  }
}

static gint
ps_document_get_orientation(PSDocument * gs)
{
  g_return_val_if_fail(gs != NULL, -1);
  g_return_val_if_fail(GTK_IS_GS(gs), -1);

  if(gs->doc) {
    if(gs->structured_doc) {
      if(gs->doc->pages[MAX(gs->current_page, 0)].orientation !=
         GTK_GS_ORIENTATION_NONE)
        gs->real_orientation =
          gs->doc->pages[MAX(gs->current_page, 0)].orientation;
      else
        gs->real_orientation = gs->doc->default_page_orientation;
    }

    if(gs->real_orientation == GTK_GS_ORIENTATION_NONE)
      gs->real_orientation = gs->doc->orientation;
  }

  if(gs->real_orientation == GTK_GS_ORIENTATION_NONE)
    return gs->fallback_orientation;
  else
    return gs->real_orientation;
}

static void
set_up_page(PSDocument * gs)
{
  guint orientation;
  char buf[1024];
  //GdkColormap *colormap;
  GdkGC *fill;
  GdkColor white = { 0, 0xFFFF, 0xFFFF, 0xFFFF };   /* pixel, r, g, b */
  GdkColormap *colormap;
  gboolean size_changed;

#ifdef HAVE_LOCALE_H
  char *savelocale;
#endif

  LOG ("Setup the page");

  size_changed = compute_size (gs);

  if (gs->pstarget == NULL)
    return;

  /* Do we have to check if the actual geometry changed? */

  stop_interpreter(gs);

  orientation = ps_document_get_orientation(gs);

  if (size_changed || gs->bpixmap == NULL) {
    gdk_flush();
    g_print ("1");
    /* clear new pixmap (set to white) */
    fill = gdk_gc_new(gs->pstarget);
    if(fill) {
      colormap = gdk_drawable_get_colormap(gs->pstarget);
      gdk_color_alloc (colormap, &white);
      gdk_gc_set_foreground(fill, &white);

      if(gs->width > 0 && gs->height > 0) {
        if(gs->bpixmap) {
          gdk_drawable_unref(gs->bpixmap);
          gs->bpixmap = NULL;
        }

        LOG ("Create our internal pixmap");
        gs->bpixmap = gdk_pixmap_new(gs->pstarget, gs->width, gs->height, -1);

        gdk_draw_rectangle(gs->bpixmap, fill, TRUE,
                           0, 0, gs->width, gs->height);
      }
      else {
        gdk_draw_rectangle(gs->pstarget, fill, TRUE,
                           0, 0, gs->width, gs->height);
      }
      gdk_gc_unref(fill);

      gdk_flush();
    }
  }

#ifdef HAVE_LOCALE_H
  /* gs needs floating point parameters with '.' as decimal point
   * while some (european) locales use ',' instead, so we set the 
   * locale for this snprintf to "C".
   */
  savelocale = setlocale(LC_NUMERIC, "C");
#endif

  g_snprintf(buf, 1024, "%ld %d %d %d %d %d %f %f %d %d %d %d",
             0L,
             orientation * 90,
             gs->llx,
             gs->lly,
             gs->urx,
             gs->ury,
             gs->xdpi * gs->zoom_factor,
             gs->ydpi * gs->zoom_factor,
             gs->left_margin,
             gs->bottom_margin, gs->right_margin, gs->top_margin);

  LOG ("GS property %s", buf);

#ifdef HAVE_LOCALE_H
  setlocale(LC_NUMERIC, savelocale);
#endif
  gdk_property_change(gs->pstarget,
                      gs_class->gs_atom,
                      gs_class->string_atom,
                      8, GDK_PROP_MODE_REPLACE, buf, strlen(buf));
  gdk_flush();
}

static void
close_pipe(int p[2])
{
  if(p[0] != -1)
    close(p[0]);
  if(p[1] != -1)
    close(p[1]);
}

static gboolean
is_interpreter_ready(PSDocument * gs)
{
  return (gs->interpreter_pid != -1 && !gs->busy && gs->ps_input == NULL);
}

static void
interpreter_failed(PSDocument * gs)
{
  stop_interpreter(gs);
}

static void
output(gpointer data, gint source, GdkInputCondition condition)
{
  char buf[MAX_BUFSIZE + 1], *msg;
  guint bytes = 0;
  PSDocument *gs = PS_DOCUMENT(data);

  if(source == gs->interpreter_output) {
    bytes = read(gs->interpreter_output, buf, MAX_BUFSIZE);
    if(bytes == 0) {            /* EOF occurred */
      close(gs->interpreter_output);
      gs->interpreter_output = -1;
      gdk_input_remove(gs->interpreter_output_id);
      return;
    }
    else if(bytes == -1) {
      /* trouble... */
      interpreter_failed(gs);
      return;
    }
    if(gs->interpreter_err == -1) {
      stop_interpreter(gs);
    }
  }
  else if(source == gs->interpreter_err) {
    bytes = read(gs->interpreter_err, buf, MAX_BUFSIZE);
    if(bytes == 0) {            /* EOF occurred */
      close(gs->interpreter_err);
      gs->interpreter_err = -1;
      gdk_input_remove(gs->interpreter_error_id);
      return;
    }
    else if(bytes == -1) {
      /* trouble... */
      interpreter_failed(gs);
      return;
    }
    if(gs->interpreter_output == -1) {
      stop_interpreter(gs);
    }
  }
  if(bytes > 0) {
    buf[bytes] = '\0';
    msg = g_strdup(buf);
    ps_document_emit_error_msg (gs, msg);   
  }
}

static void
input(gpointer data, gint source, GdkInputCondition condition)
{
  PSDocument *gs = PS_DOCUMENT(data);
  int bytes_written;
  void (*oldsig) (int);
  oldsig = signal(SIGPIPE, catchPipe);

  do {
    if(gs->buffer_bytes_left == 0) {
      /* Get a new section if required */
      if(gs->ps_input && gs->bytes_left == 0) {
        struct record_list *ps_old = gs->ps_input;
        gs->ps_input = ps_old->next;
        if(ps_old->close && NULL != ps_old->fp)
          fclose(ps_old->fp);
        g_free((char *) ps_old);
      }
      /* Have to seek at the beginning of each section */
      if(gs->ps_input && gs->ps_input->seek_needed) {
        fseek(gs->ps_input->fp, gs->ps_input->begin, SEEK_SET);
        gs->ps_input->seek_needed = FALSE;
        gs->bytes_left = gs->ps_input->len;
      }

      if(gs->bytes_left > MAX_BUFSIZE) {
        gs->buffer_bytes_left =
          fread(gs->input_buffer, sizeof(char), MAX_BUFSIZE, gs->ps_input->fp);
      }
      else if(gs->bytes_left > 0) {
        gs->buffer_bytes_left =
          fread(gs->input_buffer,
                sizeof(char), gs->bytes_left, gs->ps_input->fp);
      }
      else {
        gs->buffer_bytes_left = 0;
      }
      if(gs->bytes_left > 0 && gs->buffer_bytes_left == 0) {
        interpreter_failed(gs); /* Error occurred */
      }
      gs->input_buffer_ptr = gs->input_buffer;
      gs->bytes_left -= gs->buffer_bytes_left;
    }

    if(gs->buffer_bytes_left > 0) {
      /* g_print (" writing: %s\n",gs->input_buffer_ptr); */

      bytes_written = write(gs->interpreter_input,
                            gs->input_buffer_ptr, gs->buffer_bytes_left);

      if(broken_pipe) {
        ps_document_emit_error_msg(gs, g_strdup(_("Broken pipe.")));
        broken_pipe = FALSE;
        interpreter_failed(gs);
      }
      else if(bytes_written == -1) {
        if((errno != EWOULDBLOCK) && (errno != EAGAIN)) {
          interpreter_failed(gs);   /* Something bad happened */
        }
      }
      else {
        gs->buffer_bytes_left -= bytes_written;
        gs->input_buffer_ptr += bytes_written;
      }
    }
  }
  while(gs->ps_input && gs->buffer_bytes_left == 0);

  signal(SIGPIPE, oldsig);

  if(gs->ps_input == NULL && gs->buffer_bytes_left == 0) {
    if(gs->interpreter_input_id != 0) {
      gdk_input_remove(gs->interpreter_input_id);
      gs->interpreter_input_id = 0;
    }
  }
}

static int
start_interpreter(PSDocument * gs)
{
  int std_in[2] = { -1, -1 };   /* pipe to interp stdin */
  int std_out[2];               /* pipe from interp stdout */
  int std_err[2];               /* pipe from interp stderr */

#define NUM_ARGS    100
#define NUM_GS_ARGS (NUM_ARGS - 20)
#define NUM_ALPHA_ARGS 10

  char *argv[NUM_ARGS], *dir, *gv_env;
  char **gs_args, **alpha_args = NULL;
  int argc = 0, i;

  LOG ("Start the interpreter");

  if(!gs->gs_filename)
    return 0;

  stop_interpreter(gs);

  if(gs->disable_start == TRUE)
    return 0;

  /* set up the args... */
  gs_args = g_strsplit(gtk_gs_defaults_get_interpreter_cmd(), " ", NUM_GS_ARGS);
  for(i = 0; i < NUM_GS_ARGS && gs_args[i]; i++, argc++)
    argv[argc] = gs_args[i];

  if(gs->antialiased) {
    if(strlen(gtk_gs_defaults_get_alpha_parameters()) == 0)
      alpha_args = g_strsplit(ALPHA_PARAMS, " ", NUM_ALPHA_ARGS);
    else
      alpha_args = g_strsplit(gtk_gs_defaults_get_alpha_parameters(),
                              " ", NUM_ALPHA_ARGS);
    for(i = 0; i < NUM_ALPHA_ARGS && alpha_args[i]; i++, argc++)
      argv[argc] = alpha_args[i];
  }
  else
    argv[argc++] = "-sDEVICE=x11";
  argv[argc++] = "-dNOPAUSE";
  argv[argc++] = "-dQUIET";
  /* I assume we do _not_ want to change this... (: */
  argv[argc++] = "-dSAFER";

  /* set up the pipes */
  if(gs->send_filename_to_gs) {
    argv[argc++] = PS_DOCUMENT_GET_PS_FILE(gs);
    argv[argc++] = "-c";
    argv[argc++] = "quit";
  }
  else
    argv[argc++] = "-";

  argv[argc++] = NULL;

  if(!gs->reading_from_pipe && !gs->send_filename_to_gs) {
    if(pipe(std_in) == -1) {
      g_critical("Unable to open pipe to Ghostscript.");
      return -1;
    }
  }
  if(pipe(std_out) == -1) {
    close_pipe(std_in);
    return -1;
  }
  if(pipe(std_err) == -1) {
    close_pipe(std_in);
    close_pipe(std_out);
    return -1;
  }

  gv_env = g_strdup_printf("GHOSTVIEW=%ld %ld",
                           gdk_x11_drawable_get_xid(gs->pstarget),
			   gdk_x11_drawable_get_xid(gs->bpixmap));

  LOG ("Launching ghostview with env %s", gv_env);

  gs->busy = TRUE;
  gs->interpreter_pid = fork();
  switch (gs->interpreter_pid) {
  case -1:                     /* error */
    close_pipe(std_in);
    close_pipe(std_out);
    close_pipe(std_err);
    return -2;
    break;
  case 0:                      /* child */
    close(std_out[0]);
    dup2(std_out[1], 1);
    close(std_out[1]);

    close(std_err[0]);
    dup2(std_err[1], 2);
    close(std_err[1]);

    if(!gs->reading_from_pipe) {
      if(gs->send_filename_to_gs) {
        int stdinfd;
        /* just in case gs tries to read from stdin */
        stdinfd = open("/dev/null", O_RDONLY);
        if(stdinfd != 0) {
          dup2(stdinfd, 0);
          close(stdinfd);
        }
      }
      else {
        close(std_in[1]);
        dup2(std_in[0], 0);
        close(std_in[0]);
      }
    }

    putenv(gv_env);

    /* change to directory where the input file is. This helps
     * with postscript-files which include other files using
     * a relative path */
    dir = g_path_get_dirname(gs->gs_filename);
    chdir(dir);
    g_free(dir);

    execvp(argv[0], argv);

    /* Notify error */
    g_print("Unable to execute [%s]\n", argv[0]);
    g_strfreev(gs_args);
    g_free(gv_env);
    if(alpha_args)
      g_strfreev(alpha_args);
    _exit(1);
    break;
  default:                     /* parent */
    if(!gs->send_filename_to_gs && !gs->reading_from_pipe) {
      int result;
      close(std_in[0]);
      /* use non-blocking IO for pipe to ghostscript */
      result = fcntl(std_in[1], F_GETFL, 0);
      fcntl(std_in[1], F_SETFL, result | O_NONBLOCK);
      gs->interpreter_input = std_in[1];
    }
    else {
      gs->interpreter_input = -1;
    }
    close(std_out[1]);
    gs->interpreter_output = std_out[0];
    close(std_err[1]);
    gs->interpreter_err = std_err[0];
    gs->interpreter_output_id =
      gdk_input_add(std_out[0], GDK_INPUT_READ, output, gs);
    gs->interpreter_error_id =
      gdk_input_add(std_err[0], GDK_INPUT_READ, output, gs);
    break;
  }
  return TRUE;
}

static void
stop_interpreter(PSDocument * gs)
{
  if(gs->interpreter_pid > 0) {
    int status = 0;
    LOG ("Stop the interpreter");
    kill(gs->interpreter_pid, SIGTERM);
    while((wait(&status) == -1) && (errno == EINTR)) ;
    gs->interpreter_pid = -1;
    if(status == 1) {
      ps_document_cleanup(gs);
      gs->gs_status = _("Interpreter failed.");
    }
  }

  if(gs->interpreter_input >= 0) {
    close(gs->interpreter_input);
    gs->interpreter_input = -1;
    if(gs->interpreter_input_id != 0) {
      gdk_input_remove(gs->interpreter_input_id);
      gs->interpreter_input_id = 0;
    }
    while(gs->ps_input) {
      struct record_list *ps_old = gs->ps_input;
      gs->ps_input = gs->ps_input->next;
      if(ps_old->close && NULL != ps_old->fp)
        fclose(ps_old->fp);
      g_free((char *) ps_old);
    }
  }

  if(gs->interpreter_output >= 0) {
    close(gs->interpreter_output);
    gs->interpreter_output = -1;
    if(gs->interpreter_output_id) {
      gdk_input_remove(gs->interpreter_output_id);
      gs->interpreter_output_id = 0;
    }
  }

  if(gs->interpreter_err >= 0) {
    close(gs->interpreter_err);
    gs->interpreter_err = -1;
    if(gs->interpreter_error_id) {
      gdk_input_remove(gs->interpreter_error_id);
      gs->interpreter_error_id = 0;
    }
  }

  gs->busy = FALSE;
}

/* If file exists and is a regular file then return its length, else -1 */
static gint
file_length(const gchar * filename)
{
  struct stat stat_rec;

  if(filename && (stat(filename, &stat_rec) == 0)
     && S_ISREG(stat_rec.st_mode))
    return stat_rec.st_size;
  else
    return -1;
}

/* Test if file exists, is a regular file and its length is > 0 */
static gboolean
file_readable(const char *filename)
{
  return (file_length(filename) > 0);
}

/*
 * Decompress gs->gs_filename if necessary
 * Set gs->filename_unc to the name of the uncompressed file or NULL.
 * Error reporting via signal 'interpreter_message'
 * Return name of input file to use or NULL on error..
 */
static gchar *
check_filecompressed(PSDocument * gs)
{
  FILE *file;
  gchar buf[1024];
  gchar *filename, *filename_unc, *filename_err, *cmdline;
  const gchar *cmd;
  int fd;

  cmd = NULL;

  if((file = fopen(gs->gs_filename, "r"))
     && (fread(buf, sizeof(gchar), 3, file) == 3)) {
    if((buf[0] == '\037') && ((buf[1] == '\235') || (buf[1] == '\213'))) {
      /* file is gzipped or compressed */
      cmd = gtk_gs_defaults_get_ungzip_cmd();
    }
    else if(strncmp(buf, "BZh", 3) == 0) {
      /* file is compressed with bzip2 */
      cmd = gtk_gs_defaults_get_unbzip2_cmd();
    }
  }
  if(NULL != file)
    fclose(file);

  if(!cmd)
    return gs->gs_filename;

  /* do the decompression */
  filename = g_shell_quote(gs->gs_filename);
  filename_unc = g_strconcat(g_get_tmp_dir(), "/ggvXXXXXX", NULL);
  if((fd = mkstemp(filename_unc)) < 0) {
    g_free(filename_unc);
    g_free(filename);
    return NULL;
  }
  close(fd);
  filename_err = g_strconcat(g_get_tmp_dir(), "/ggvXXXXXX", NULL);
  if((fd = mkstemp(filename_err)) < 0) {
    g_free(filename_err);
    g_free(filename_unc);
    g_free(filename);
    return NULL;
  }
  close(fd);
  cmdline = g_strdup_printf("%s %s >%s 2>%s", cmd,
                            filename, filename_unc, filename_err);
  if((system(cmdline) == 0)
     && file_readable(filename_unc)
     && (file_length(filename_err) == 0)) {
    /* sucessfully uncompressed file */
    gs->gs_filename_unc = filename_unc;
  }
  else {
    /* report error */
    g_snprintf(buf, 1024, _("Error while decompressing file %s:\n"),
               gs->gs_filename);
    ps_document_emit_error_msg(gs, buf);
    if(file_length(filename_err) > 0) {
      FILE *err;
      if((err = fopen(filename_err, "r"))) {
        /* print file to message window */
        while(fgets(buf, 1024, err))
          ps_document_emit_error_msg(gs, buf);
        fclose(err);
      }
    }
    unlink(filename_unc);
    g_free(filename_unc);
    filename_unc = NULL;
  }
  unlink(filename_err);
  g_free(filename_err);
  g_free(cmdline);
  g_free(filename);
  return filename_unc;
}

#ifdef BROKEN_XINERAMA_PATCH_THAT_SHOULD_NOT_BE_USED
/* never mind this patch: a properly working X server should take care of
   calculating the proper values. */
static float
compute_xdpi(void)
{
#   ifndef HAVE_XINERAMA
  return 25.4 * gdk_screen_width() / gdk_screen_width_mm();
#   else
  Display *dpy;
  dpy = (Display *) GDK_DISPLAY();
  if(XineramaIsActive(dpy)) {
    int num_heads;
    XineramaScreenInfo *head_info;
    head_info = (XineramaScreenInfo *) XineramaQueryScreens(dpy, &num_heads);
    /* fake it with dimensions of the first head for now */
    return 25.4 * head_info[0].width / gdk_screen_width_mm();
  }
  else {
    return 25.4 * gdk_screen_width() / gdk_screen_width_mm();
  }
#   endif
  /* HAVE_XINERAMA */
}

static float
compute_ydpi(void)
{
#   ifndef HAVE_XINERAMA
  return 25.4 * gdk_screen_height() / gdk_screen_height_mm();
#   else
  Display *dpy;
  dpy = (Display *) GDK_DISPLAY();
  if(XineramaIsActive(dpy)) {
    int num_heads;
    XineramaScreenInfo *head_info;
    head_info = (XineramaScreenInfo *) XineramaQueryScreens(dpy, &num_heads);
    /* fake it with dimensions of the first head for now */
    return 25.4 * head_info[0].height / gdk_screen_height_mm();
  }
  else {
    return 25.4 * gdk_screen_height() / gdk_screen_height_mm();
  }
#   endif
  /* HAVE_XINERAMA */
}
#else
static float
compute_xdpi(void)
{
  return 25.4 * gdk_screen_width() / gdk_screen_width_mm();
}

static float
compute_ydpi(void)
{
  return 25.4 * gdk_screen_height() / gdk_screen_height_mm();
}
#endif /* BROKEN_XINERAMA_PATCH_THAT_SHOULD_NOT_BE_USED */

/* Compute new size of window, sets xdpi and ydpi if necessary.
 * returns True if new window size is different */
static gboolean
compute_size(PSDocument * gs)
{
  guint new_width = 1;
  guint new_height = 1;
  gboolean change = FALSE;
  gint orientation;

  /* width and height can be changed, calculate window size according */
  /* to xpdi and ydpi */
  orientation = ps_document_get_orientation(gs);

  switch (orientation) {
  case GTK_GS_ORIENTATION_PORTRAIT:
  case GTK_GS_ORIENTATION_UPSIDEDOWN:
    new_width = (gs->urx - gs->llx) / 72.0 * gs->xdpi + 0.5;
    new_height = (gs->ury - gs->lly) / 72.0 * gs->ydpi + 0.5;
    break;
  case GTK_GS_ORIENTATION_LANDSCAPE:
  case GTK_GS_ORIENTATION_SEASCAPE:
    new_width = (gs->ury - gs->lly) / 72.0 * gs->xdpi + 0.5;
    new_height = (gs->urx - gs->llx) / 72.0 * gs->ydpi + 0.5;
    break;
  }

  change = (new_width != gs->width * gs->zoom_factor)
    || (new_height != gs->height * gs->zoom_factor);
  gs->width = (gint) (new_width * gs->zoom_factor);
  gs->height = (gint) (new_height * gs->zoom_factor);

  return (change);
}

static gint
ps_document_enable_interpreter(PSDocument * gs)
{
  g_return_val_if_fail(gs != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_GS(gs), FALSE);

  if(!gs->gs_filename)
    return 0;

  gs->disable_start = FALSE;
  
  return start_interpreter(gs);
}

/* publicly accessible functions */

GType
ps_document_get_type(void)
{
  static GType gs_type = 0;
  if(!gs_type) {
    GTypeInfo gs_info = {
      sizeof(PSDocumentClass),
      (GBaseInitFunc) NULL,
      (GBaseFinalizeFunc) NULL,
      (GClassInitFunc) ps_document_class_init,
      (GClassFinalizeFunc) NULL,
      NULL,                     /* class_data */
      sizeof(PSDocument),
      0,                        /* n_preallocs */
      (GInstanceInitFunc) ps_document_init
    };

    static const GInterfaceInfo document_info =
    {
        (GInterfaceInitFunc) ps_document_document_iface_init,
        NULL,
        NULL
    };

    gs_type = g_type_register_static(G_TYPE_OBJECT,
                                     "PSDocument", &gs_info, 0);

    g_type_add_interface_static (gs_type,
                                 EV_TYPE_DOCUMENT,
                                 &document_info);
  }
  return gs_type;


}

/*
 * Show error message -> send signal "interpreter_message"
 */
static void
ps_document_emit_error_msg(PSDocument * gs, const gchar * msg)
{
  gdk_pointer_ungrab(GDK_CURRENT_TIME);
  if(strstr(msg, "Error:")) {
    gs->gs_status = _("File is not a valid PostScript document.");
    ps_document_cleanup(gs);
  }
}

static gboolean
document_load(PSDocument * gs, const gchar * fname)
{
  g_return_val_if_fail(gs != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_GS(gs), FALSE);

  LOG ("Load the document");

  /* clean up previous document */
  ps_document_cleanup(gs);

  if(fname == NULL) {
    gs->gs_status = "";
    return FALSE;
  }

  /* prepare this document */

  /* default values: no dsc information available  */
  gs->structured_doc = FALSE;
  gs->send_filename_to_gs = TRUE;
  gs->current_page = 0;
  gs->loaded = FALSE;
  if(*fname == '/') {
    /* an absolute path */
    gs->gs_filename = g_strdup(fname);
  }
  else {
    /* path relative to our cwd: make it absolute */
    gchar *cwd = g_get_current_dir();
    gs->gs_filename = g_strconcat(cwd, "/", fname, NULL);
    g_free(cwd);
  }

  if((gs->reading_from_pipe = (strcmp(fname, "-") == 0))) {
    gs->send_filename_to_gs = FALSE;
  }
  else {
    /*
     * We need to make sure that the file is loadable/exists!
     * otherwise we want to exit without loading new stuff...
     */
    gchar *filename = NULL;

    if(!file_readable(fname)) {
      gchar buf[1024];
      g_snprintf(buf, 1024, _("Cannot open file %s.\n"), fname);
      ps_document_emit_error_msg(gs, buf);
      gs->gs_status = _("File is not readable.");
    }
    else {
      filename = check_filecompressed(gs);
    }

    if(!filename || (gs->gs_psfile = fopen(filename, "r")) == NULL) {
      ps_document_cleanup(gs);
      return FALSE;
    }

    /* we grab the vital statistics!!! */
    gs->doc = psscan(gs->gs_psfile, gs->respect_eof, filename);

    g_object_notify (G_OBJECT (gs), "title");

    if(gs->doc == NULL) {
      /* File does not seem to be a Postscript one */
      gchar buf[1024];
      g_snprintf(buf, 1024, _("Error while scanning file %s\n"), fname);
      ps_document_emit_error_msg(gs, buf);
      ps_document_cleanup(gs);
      gs->gs_status = _("The file is not a PostScript document.");
      return FALSE;
    }

    if((!gs->doc->epsf && gs->doc->numpages > 0) ||
       (gs->doc->epsf && gs->doc->numpages > 1)) {
      gs->structured_doc = TRUE;
      gs->send_filename_to_gs = FALSE;
    }

    /* We have to set up the orientation of the document */
    gs->real_orientation = gs->doc->orientation;
  }
  ps_document_set_page_size(gs, -1, gs->current_page);
  gs->loaded = TRUE;

  gs->gs_status = _("Document loaded.");

  return gs->loaded;
}


static gboolean
ps_document_next_page(PSDocument * gs)
{
  XEvent event;

  LOG ("Make ghostscript render next page");

  g_return_val_if_fail(gs != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_GS(gs), FALSE);

  if(gs->interpreter_pid == 0) {    /* no interpreter active */
    return FALSE;
  }

  if(gs->busy) {                /* interpreter is busy */
    return FALSE;
  }

  gs->busy = TRUE;

  event.xclient.type = ClientMessage;
  event.xclient.display = gdk_display;
  event.xclient.window = gs->message_window;
  event.xclient.message_type = gdk_x11_atom_to_xatom(gs_class->next_atom);
  event.xclient.format = 32;

  gdk_error_trap_push();
  XSendEvent(gdk_display, gs->message_window, FALSE, 0, &event);
  gdk_flush();
  gdk_error_trap_pop();

  return TRUE;
}

static gboolean
ps_document_goto_page(PSDocument * gs)
{
  int page = gs->current_page;

  g_return_val_if_fail(gs != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_GS(gs), FALSE);

  LOG ("Go to page %d", page);

  if(!gs->gs_filename) {
    return FALSE;
  }

  /* range checking... */
  if(page < 0)
    page = 0;

  if(gs->structured_doc && gs->doc) {

    LOG ("It's a structured document, let's send one page to gs");

    if(page >= gs->doc->numpages)
      page = gs->doc->numpages - 1;

    gs->current_page = page;

    if(gs->doc->pages[page].orientation != NONE &&
       gs->doc->pages[page].orientation != gs->real_orientation) {
      gs->real_orientation = gs->doc->pages[page].orientation;
      gs->changed = TRUE;
    }

    ps_document_set_page_size(gs, -1, page);

    gs->changed = FALSE;

    if(is_interpreter_ready(gs)) {
      ps_document_next_page(gs);
    }
    else {
      ps_document_enable_interpreter(gs);
      send_ps(gs, gs->doc->beginprolog, gs->doc->lenprolog, FALSE);
      send_ps(gs, gs->doc->beginsetup, gs->doc->lensetup, FALSE);
    }

    send_ps(gs, gs->doc->pages[gs->current_page].begin,
            gs->doc->pages[gs->current_page].len, FALSE);
  }
  else {
    /* Unstructured document */
    /* In the case of non structured documents,
       GS read the PS from the  actual file (via command
       line. Hence, ggv only send a signal next page.
       If ghostview is not running it is usually because
       the last page of the file was displayed. In that
       case, ggv restarts GS again and the first page is displayed.
     */

    LOG ("It's an unstructured document, gs will just read the file");

    if(page == gs->current_page && !gs->changed)
      return TRUE;

    ps_document_set_page_size(gs, -1, page);

    if(!is_interpreter_ready(gs))
      ps_document_enable_interpreter(gs);

    gs->current_page = page;

    ps_document_next_page(gs);
  }
  return TRUE;
}

/*
 * set pagesize sets the size from
 * if new_pagesize is -1, then it is set to either
 *  a) the default settings of pageid, if they exist, or if pageid != -1.
 *  b) the default setting of the document, if it exists.
 *  c) the default setting of the widget.
 * otherwise, the new_pagesize is used as the pagesize
 */
static gboolean
ps_document_set_page_size(PSDocument * gs, gint new_pagesize, gint pageid)
{
  gint new_llx = 0;
  gint new_lly = 0;
  gint new_urx = 0;
  gint new_ury = 0;
  GtkGSPaperSize *papersizes = gtk_gs_defaults_get_paper_sizes();

  LOG ("Set the page size");

  g_return_val_if_fail(gs != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_GS(gs), FALSE);

  if(new_pagesize == -1) {
    if(gs->default_size > 0)
      new_pagesize = gs->default_size;
    if(gs->doc) {
      /* If we have a document:
         We use -- the page size (if specified)
         or the doc. size (if specified)
         or the page bbox (if specified)
         or the bounding box
       */
      if((pageid >= 0) && (gs->doc->numpages > pageid) &&
         (gs->doc->pages) && (gs->doc->pages[pageid].size)) {
        new_pagesize = gs->doc->pages[pageid].size - gs->doc->size;
      }
      else if(gs->doc->default_page_size != NULL) {
        new_pagesize = gs->doc->default_page_size - gs->doc->size;
      }
      else if((pageid >= 0) &&
              (gs->doc->numpages > pageid) &&
              (gs->doc->pages) &&
              (gs->doc->pages[pageid].boundingbox[URX] >
               gs->doc->pages[pageid].boundingbox[LLX]) &&
              (gs->doc->pages[pageid].boundingbox[URY] >
               gs->doc->pages[pageid].boundingbox[LLY])) {
        new_pagesize = -1;
      }
      else if((gs->doc->boundingbox[URX] > gs->doc->boundingbox[LLX]) &&
              (gs->doc->boundingbox[URY] > gs->doc->boundingbox[LLY])) {
        new_pagesize = -1;
      }
    }
  }

  /* Compute bounding box */
  if(gs->doc && (gs->doc->epsf || new_pagesize == -1)) {    /* epsf or bbox */
    if((pageid >= 0) &&
       (gs->doc->pages) &&
       (gs->doc->pages[pageid].boundingbox[URX] >
        gs->doc->pages[pageid].boundingbox[LLX])
       && (gs->doc->pages[pageid].boundingbox[URY] >
           gs->doc->pages[pageid].boundingbox[LLY])) {
      /* use page bbox */
      new_llx = gs->doc->pages[pageid].boundingbox[LLX];
      new_lly = gs->doc->pages[pageid].boundingbox[LLY];
      new_urx = gs->doc->pages[pageid].boundingbox[URX];
      new_ury = gs->doc->pages[pageid].boundingbox[URY];
    }
    else if((gs->doc->boundingbox[URX] > gs->doc->boundingbox[LLX]) &&
            (gs->doc->boundingbox[URY] > gs->doc->boundingbox[LLY])) {
      /* use doc bbox */
      new_llx = gs->doc->boundingbox[LLX];
      new_lly = gs->doc->boundingbox[LLY];
      new_urx = gs->doc->boundingbox[URX];
      new_ury = gs->doc->boundingbox[URY];
    }
  }
  else {
    if(new_pagesize < 0)
      new_pagesize = gs->default_size;
    new_llx = new_lly = 0;
    if(gs->doc && gs->doc->size &&
       (new_pagesize < gs->doc->numsizes)) {
      new_urx = gs->doc->size[new_pagesize].width;
      new_ury = gs->doc->size[new_pagesize].height;
    }
    else {
      new_urx = papersizes[new_pagesize].width;
      new_ury = papersizes[new_pagesize].height;
    }
  }

  if(new_urx <= new_llx)
    new_urx = papersizes[12].width;
  if(new_ury <= new_lly)
    new_ury = papersizes[12].height;

  /* If bounding box changed, setup for new size. */
  /* ps_document_disable_interpreter (gs); */
  if((new_llx != gs->llx) || (new_lly != gs->lly) ||
     (new_urx != gs->urx) || (new_ury != gs->ury)) {
    gs->llx = new_llx;
    gs->lly = new_lly;
    gs->urx = new_urx;
    gs->ury = new_ury;
    gs->changed = TRUE;
  }

  return FALSE;
}

static void
ps_document_set_zoom (PSDocument * gs, gfloat zoom)
{
	g_return_if_fail (gs != NULL);

	gs->zoom_factor = zoom;
	compute_size (gs);
}

static gboolean
ps_document_load (EvDocument  *document,
		  const char  *uri,
		  GError     **error)
{
	gboolean result;
	char *filename;

	filename = g_filename_from_uri (uri, NULL, error);
	if (!filename)
		return FALSE;

	result = document_load (PS_DOCUMENT (document), filename);
	if (!result) {
		g_set_error (error, G_FILE_ERROR,
			     G_FILE_ERROR_FAILED,
			     "Failed to load document '%s'\n",
			     uri);
	}

	g_free (filename);

	return result;
}

static gboolean
ps_document_save (EvDocument  *document,
		  const char  *uri,
		  GError     **error)
{
	g_warning ("ps_document_save not implemented"); /* FIXME */
	return TRUE;
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
ps_document_set_page (EvDocument  *document,
		       int          page)
{
	LOG ("Set document page %d\n", page);

	PS_DOCUMENT (document)->current_page = page - 1;
}

static int
ps_document_get_page (EvDocument  *document)
{
	PSDocument *ps = PS_DOCUMENT (document);

	g_return_val_if_fail (ps != NULL, -1);

	return ps->current_page + 1;
}

static void
ps_document_set_scale (EvDocument  *document,
			double       scale)
{
	ps_document_set_zoom (PS_DOCUMENT (document), scale);
}

static void
ps_document_set_page_offset (EvDocument  *document,
			      int          x,
			      int          y)
{
	PSDocument *gs = PS_DOCUMENT (document);

	gs->page_x_offset = x;
	gs->page_y_offset = y;
}

static void
ps_document_get_page_size (EvDocument   *document,
			   int           page,
			   int          *width,
			   int          *height)
{
	/* Post script documents never vary in size */

	PSDocument *gs = PS_DOCUMENT (document);

	if (width) {
		*width = gs->width;
	}

	if (height) {
		*height = gs->height;
	}
}

static void
ps_document_render (EvDocument  *document,
		    int          clip_x,
		    int          clip_y,
		    int          clip_width,
		    int          clip_height)
{
	PSDocument *gs = PS_DOCUMENT (document);
	GdkRectangle page;
	GdkRectangle draw;
	GdkGC *gc;

	if (gs->pstarget == NULL ||
	    gs->bpixmap == NULL) {
		return;
	}

	page.x = gs->page_x_offset;
	page.y = gs->page_y_offset;
	page.width = gs->width;
	page.height = gs->height;

	draw.x = clip_x;
	draw.y = clip_y;
	draw.width = clip_width;
	draw.height = clip_height;

	gc = gdk_gc_new (gs->pstarget);

	gdk_draw_drawable (gs->pstarget, gc,
			   gs->bpixmap,
			   draw.x - page.x, draw.y - page.y,
			   draw.x, draw.y,
			   draw.width, draw.height);

	LOG ("Copy the internal pixmap: %d %d %d %d %d %d",
             draw.x - page.x, draw.y - page.y,
             draw.x, draw.y, draw.width, draw.height);

	g_object_unref (gc);
}

static char *
ps_document_get_text (EvDocument *document, GdkRectangle *rect)
{
	g_warning ("ps_document_get_text not implemented"); /* FIXME ? */
	return NULL;
}

static EvLink *
ps_document_get_link (EvDocument *document,
		      int         x,
		      int	  y)
{
	return NULL;
}

static gboolean
render_pixbuf_idle (EvDocument *document)
{
	PSDocument *gs = PS_DOCUMENT (document);

	if (gs->pstarget == NULL) {
		GtkWidget *widget;

		widget = gtk_window_new (GTK_WINDOW_POPUP);
	        gtk_widget_realize (widget);
		gs->pstarget = widget->window;

	        g_assert (gs->pstarget != NULL);

		g_signal_connect (widget, "event",
			    	  G_CALLBACK (ps_document_widget_event),
			          gs);
	}

	set_up_page (gs);

	ps_document_goto_page (PS_DOCUMENT (document));

	return FALSE;
}

static GdkPixbuf *
ps_document_render_pixbuf (EvDocument *document)
{
	GdkPixbuf *pixbuf;

	g_idle_add ((GSourceFunc)render_pixbuf_idle, document);

	g_mutex_lock (pixbuf_mutex);
	while (!current_pixbuf)
		g_cond_wait (pixbuf_cond, pixbuf_mutex);
	pixbuf = current_pixbuf;
	current_pixbuf = NULL;
	g_mutex_unlock (pixbuf_mutex);

	LOG ("Pixbuf rendered %p\n", pixbuf);

	return pixbuf;
}

static void
ps_document_document_iface_init (EvDocumentIface *iface)
{
	iface->load = ps_document_load;
	iface->save = ps_document_save;
	iface->get_text = ps_document_get_text;
	iface->get_link = ps_document_get_link;
	iface->get_n_pages = ps_document_get_n_pages;
	iface->set_page = ps_document_set_page;
	iface->get_page = ps_document_get_page;
	iface->set_scale = ps_document_set_scale;
	iface->set_page_offset = ps_document_set_page_offset;
	iface->get_page_size = ps_document_get_page_size;
	iface->render = ps_document_render;
	iface->render_pixbuf = ps_document_render_pixbuf;
}
