/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2007 Carlos Garcia Campos <carlosgc@gnome.org>
 *  Copyright 1998 - 2005 The Free Software Foundation
 *
 * Evince is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Evince is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
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

#include "ps-interpreter.h"
#include "ps.h"

#define MAX_BUFSIZE 1024

enum {
        PAGE_RENDERED,
        LAST_SIGNAL
};

/* structure to describe section of file to send to ghostscript */
typedef struct {
        FILE *fp;
        glong begin;
        guint len;
        gboolean seek_needed;
        gboolean close;
} PSSection;

struct _PSInterpreter {
        GObject object;

        GtkWidget *target_window;
        GdkWindow *pstarget;
        GdkPixmap *bpixmap;
        glong message_window;          /* Used by ghostview to receive messages from app */

        GPid pid;               /* PID of interpreter, -1 if none  */
        GIOChannel *input;      /* stdin of interpreter            */
        GIOChannel *output;     /* stdout of interpreter           */
        GIOChannel *error;        /* stderr of interpreter           */
        guint input_id;
        guint output_id;
        guint error_id;

        gboolean busy;                /* Is gs busy drawing? */
        gboolean structured_doc;

        GQueue *ps_input;
        gchar *input_buffer_ptr;
        guint bytes_left;
        guint buffer_bytes_left;

        FILE *psfile;              /* the currently loaded FILE */
        gchar *psfilename;           /* the currently loaded filename */
        gchar *input_buffer;
        gboolean send_filename_to_gs; /* True if gs should read from file directly */
        const struct document *doc;
};

struct _PSInterpreterClass {
        GObjectClass parent_class;

        void (* page_rendered) (PSInterpreter *gs,
                                GdkPixbuf     *pixbuf);
        
        GdkAtom gs_atom;
        GdkAtom next_atom;
        GdkAtom page_atom;
        GdkAtom string_atom;
};

static void     ps_interpreter_start    (PSInterpreter *gs);
static void     ps_interpreter_stop     (PSInterpreter *gs);
static void     ps_interpreter_failed   (PSInterpreter *gs,
                                         const gchar   *msg);
static gboolean ps_interpreter_is_ready (PSInterpreter *gs);

static void     push_pixbuf             (PSInterpreter *gs);

G_DEFINE_TYPE (PSInterpreter, ps_interpreter, G_TYPE_OBJECT)

static guint gs_signals[LAST_SIGNAL];

static void
ps_section_free (PSSection *section)
{
        if (!section)
                return;

        if (section->close && section->fp)
                fclose (section->fp);

        g_free (section);
}

static void
ps_interpreter_init (PSInterpreter *gs)
{
        gs->pid = -1;
        gs->ps_input = g_queue_new ();
}

static void
ps_interpreter_dispose (GObject *object)
{
        PSInterpreter *gs = PS_INTERPRETER (object);

        gs->doc = NULL;
        
        if (gs->psfile) {
                fclose (gs->psfile);
                gs->psfile = NULL;
        }

        if (gs->psfilename) {
                g_free (gs->psfilename);
                gs->psfilename = NULL;
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

        G_OBJECT_CLASS (ps_interpreter_parent_class)->dispose (object);
}

static void
ps_interpreter_class_init (PSInterpreterClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        gs_signals[PAGE_RENDERED] =
                g_signal_new ("page_rendered",
                              PS_TYPE_INTERPRETER,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (PSInterpreterClass, page_rendered),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__OBJECT,
                              G_TYPE_NONE,
                              1,
                              GDK_TYPE_PIXBUF);
        
        klass->gs_atom = gdk_atom_intern ("GHOSTVIEW", FALSE);
        klass->next_atom = gdk_atom_intern ("NEXT", FALSE);
        klass->page_atom = gdk_atom_intern ("PAGE", FALSE);
        klass->string_atom = gdk_atom_intern ("STRING", FALSE);

        object_class->dispose = ps_interpreter_dispose;
}

static gboolean
ps_interpreter_input (GIOChannel    *io,
                      GIOCondition   condition,
                      PSInterpreter *gs)
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

                        status = g_io_channel_write_chars (gs->input,
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

                flags = g_io_channel_get_flags (gs->input);
                
                g_io_channel_set_flags (gs->input,
                                        flags & ~G_IO_FLAG_NONBLOCK, NULL);
                g_io_channel_flush (gs->input, NULL);
                g_io_channel_set_flags (gs->input,
                                        flags | G_IO_FLAG_NONBLOCK, NULL);
                
                gs->input_id = 0;
                
                return FALSE;
        }
        
        return TRUE;
}

static gboolean
ps_interpreter_output (GIOChannel    *io,
                       GIOCondition   condition,
                       PSInterpreter *gs)
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
                g_io_channel_unref (gs->output);
                gs->output = NULL;
                gs->output_id = 0;
                        
                return FALSE;
        case G_IO_STATUS_ERROR:
                ps_interpreter_failed (gs, error->message);
                g_error_free (error);
                gs->output_id = 0;
                        
                return FALSE;
        default:
                break;
        }
        
        if (!gs->error) {
                ps_interpreter_failed (gs, NULL);
        }

        return TRUE;
}

static gboolean
ps_interpreter_error (GIOChannel    *io,
                      GIOCondition   condition,
                      PSInterpreter *gs)
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
                g_io_channel_unref (gs->error);
                gs->error = NULL;
                gs->error_id = 0;
                        
                return FALSE;
        case G_IO_STATUS_ERROR:
                ps_interpreter_failed (gs, error->message);
                g_error_free (error);
                gs->error_id = 0;
                        
                break;
        default:
                break;
        }
        
        if (!gs->output) {
                ps_interpreter_failed (gs, NULL);
        }

        return TRUE;
}

static void
ps_interpreter_finished (GPid           pid,
                         gint           status,
                         PSInterpreter *gs)
{
        g_spawn_close_pid (gs->pid);
        gs->pid = -1;
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
ps_interpreter_start (PSInterpreter *gs)
{
        gchar *argv[NUM_ARGS], *dir, *gv_env, *gs_path;
        gchar **gs_args, **alpha_args = NULL;
        gchar **envp;
        gint pin, pout, perr;
        gint argc = 0, i;
        GError *error = NULL;

        g_assert (gs->psfilename != NULL);

        ps_interpreter_stop (gs);

        dir = g_path_get_dirname (gs->psfilename);

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
                argv[argc++] = gs->psfilename;
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
                                      &(gs->pid), &pin, &pout, &perr,
                                      &error)) {
                GIOFlags flags;

                g_child_watch_add (gs->pid,
                                   (GChildWatchFunc)ps_interpreter_finished, 
                                   gs);

                gs->input = g_io_channel_unix_new (pin);
                g_io_channel_set_encoding (gs->input, NULL, NULL);
                flags = g_io_channel_get_flags (gs->input);
                g_io_channel_set_flags (gs->input, flags | G_IO_FLAG_NONBLOCK, NULL);

                
                gs->output = g_io_channel_unix_new (pout);
                flags = g_io_channel_get_flags (gs->output);
                g_io_channel_set_flags (gs->output, flags | G_IO_FLAG_NONBLOCK, NULL);
                gs->output_id = g_io_add_watch (gs->output, G_IO_IN,
                                                (GIOFunc)ps_interpreter_output,
                                                gs);
                
                gs->error = g_io_channel_unix_new (perr);
                flags = g_io_channel_get_flags (gs->error);
                g_io_channel_set_flags (gs->error, flags | G_IO_FLAG_NONBLOCK, NULL);
                gs->error_id = g_io_add_watch (gs->error, G_IO_IN,
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
ps_interpreter_stop (PSInterpreter *gs)
{
        if (gs->pid > 0) {
                gint status = 0;
                
                kill (gs->pid, SIGTERM);
                while ((wait (&status) == -1) && (errno == EINTR));
                g_spawn_close_pid (gs->pid);
                gs->pid = -1;
        }

        if (gs->input) {
                g_io_channel_unref (gs->input);
                gs->input = NULL;

                if (gs->input_id > 0) {
                        g_source_remove (gs->input_id);
                        gs->input_id = 0;
                }
                
                if (gs->ps_input) {
                        g_queue_foreach (gs->ps_input, (GFunc)ps_section_free, NULL);
                        g_queue_free (gs->ps_input);
                        gs->ps_input = g_queue_new ();
                }
        }

        if (gs->output) {
                g_io_channel_unref (gs->output);
                gs->output = NULL;

                if (gs->output_id > 0) {
                        g_source_remove (gs->output_id);
                        gs->output_id = 0;
                }
        }

        if (gs->error) {
                g_io_channel_unref (gs->error);
                gs->error = NULL;
                
                if (gs->error_id > 0) {
                        g_source_remove (gs->error_id);
                        gs->error_id = 0;
                }
        }

        gs->busy = FALSE;
}

static void
ps_interpreter_failed (PSInterpreter *gs, const char *msg)
{
        g_warning (msg ? msg : _("Interpreter failed."));

        push_pixbuf (gs);
        ps_interpreter_stop (gs);
}

static gboolean
ps_interpreter_is_ready (PSInterpreter *gs)
{
        return (gs->pid != -1 && !gs->busy &&
                (g_queue_is_empty (gs->ps_input)));
}

static void
setup_page (PSInterpreter *gs, int page, double scale, int rotation)
{
        gchar *buf;
        char scaled_dpi[G_ASCII_DTOSTR_BUF_SIZE];       
        int urx, ury, llx, lly;
        PSInterpreterClass *gs_class = PS_INTERPRETER_GET_CLASS (gs);

        psgetpagebox (gs->doc, page, &urx, &ury, &llx, &lly);
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
setup_pixmap (PSInterpreter *gs, int page, double scale, int rotation)
{
        GdkGC *fill;
        GdkColor white = { 0, 0xFFFF, 0xFFFF, 0xFFFF };   /* pixel, r, g, b */
        GdkColormap *colormap;
        double width, height;
        int pixmap_width, pixmap_height;
        int urx, ury, llx, lly;

        psgetpagebox (gs->doc, page, &urx, &ury, &llx, &lly);
        width = (urx - llx) + 0.5;
        height = (ury - lly) + 0.5;

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
push_pixbuf (PSInterpreter *gs)
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
        g_signal_emit (gs, gs_signals[PAGE_RENDERED], 0, pixbuf);
        g_object_unref (pixbuf);
}

static gboolean
ps_interpreter_widget_event (GtkWidget     *widget,
                             GdkEvent      *event,
                             PSInterpreter *gs)
{
        PSInterpreterClass *gs_class = PS_INTERPRETER_GET_CLASS (gs);

        if (event->type != GDK_CLIENT_EVENT)
                return FALSE;

        gs->message_window = event->client.data.l[0];

        if (event->client.message_type == gs_class->page_atom) {
                gs->busy = FALSE;

                push_pixbuf (gs);
        }

        return TRUE;
}

static void
send_ps (PSInterpreter *gs, glong begin, guint len, gboolean close)
{
        PSSection *ps_new;

        g_assert (gs->psfile != NULL);
        
        if (!gs->input) {
                g_critical ("No pipe to gs: error in send_ps().");
                return;
        }

        ps_new = g_new0 (PSSection, 1);
        ps_new->fp = gs->psfile;
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
                gs->input_id = g_io_add_watch (gs->input, G_IO_OUT,
                                               (GIOFunc)ps_interpreter_input,
                                               gs);
        } else {
                g_queue_push_head (gs->ps_input, ps_new);
        }
}

static void
ps_interpreter_next_page (PSInterpreter *gs)
{
        XEvent              event;
        GdkScreen          *screen;
        GdkDisplay         *display;
        Display            *dpy;
        PSInterpreterClass *gs_class = PS_INTERPRETER_GET_CLASS (gs);

        g_assert (gs->pid != 0);
        g_assert (gs->busy != TRUE);

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

static void
render_page (PSInterpreter *gs, gint page)
{
        if (gs->structured_doc && gs->doc) {
                if (ps_interpreter_is_ready (gs)) {
                        ps_interpreter_next_page (gs);
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
                ps_interpreter_next_page (gs);
        }
}

/* Public methods */
PSInterpreter *
ps_interpreter_new (const gchar           *filename,
                    const struct document *doc)
{
        PSInterpreter *gs;

        g_return_val_if_fail (filename != NULL, NULL);
        g_return_val_if_fail (doc != NULL, NULL);

        gs = PS_INTERPRETER (g_object_new (PS_TYPE_INTERPRETER, NULL));

        gs->psfilename = g_strdup (filename);
        gs->doc = doc;
        gs->structured_doc = FALSE;
        gs->send_filename_to_gs = TRUE;
        gs->psfile = fopen (gs->psfilename, "r");

        if ((!gs->doc->epsf && gs->doc->numpages > 0) ||
            (gs->doc->epsf && gs->doc->numpages > 1)) {
                gs->structured_doc = TRUE;
                gs->send_filename_to_gs = FALSE;
        }

        return gs;
}

void
ps_interpreter_render_page (PSInterpreter *gs,
                            gint           page,
                            gdouble        scale,
                            gint           rotation)
{
        g_return_if_fail (PS_IS_INTERPRETER (gs));

        if (gs->pstarget == NULL) {
                gs->target_window = gtk_window_new (GTK_WINDOW_POPUP);
                gtk_widget_realize (gs->target_window);
                gs->pstarget = gs->target_window->window;

                g_assert (gs->pstarget != NULL);

                g_signal_connect (gs->target_window, "event",
                                  G_CALLBACK (ps_interpreter_widget_event),
                                  gs);
        }

        setup_pixmap (gs, page, scale, rotation);
        setup_page (gs, page, scale, rotation);

        render_page (gs, page);
}
