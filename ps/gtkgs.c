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

#include "gtkgs.h"
#include "ggvutils.h"
#include "ps.h"
#include "gsdefaults.h"

#ifdef HAVE_LOCALE_H
#   include <locale.h>
#endif

/* if POSIX O_NONBLOCK is not available, use O_NDELAY */
#if !defined(O_NONBLOCK) && defined(O_NDELAY)
#   define O_NONBLOCK O_NDELAY
#endif

#define GTK_GS_WATCH_INTERVAL 1000
#define GTK_GS_WATCH_TIMEOUT  2

#define MAX_BUFSIZE 1024

enum { INTERPRETER_MESSAGE, INTERPRETER_ERROR, LAST_SIGNAL };

static gboolean broken_pipe = FALSE;

static void
catchPipe(int i)
{
  broken_pipe = True;
}

/* Forward declarations */
static void gtk_gs_init(GtkGS * gs);
static void gtk_gs_class_init(GtkGSClass * klass);
static void gtk_gs_destroy(GtkObject * object);
static void gtk_gs_realize(GtkWidget * widget);
static void gtk_gs_size_request(GtkWidget * widget,
                                GtkRequisition * requisition);
static void gtk_gs_size_allocate(GtkWidget * widget,
                                 GtkAllocation * allocation);
static gint gtk_gs_widget_event(GtkWidget * widget, GdkEvent * event,
                                gpointer data);
static void gtk_gs_value_adjustment_changed(GtkAdjustment * adjustment,
                                            gpointer data);
static void gtk_gs_interpreter_message(GtkGS * gs, gchar * msg,
                                       gpointer user_data);
static void gtk_gs_emit_error_msg(GtkGS * gs, const gchar * msg);
static void gtk_gs_set_adjustments(GtkGS * gs, GtkAdjustment * hadj,
                                   GtkAdjustment * vadj);
static void send_ps(GtkGS * gs, long begin, unsigned int len, gboolean close);
static void set_up_page(GtkGS * gs);
static void close_pipe(int p[2]);
static void interpreter_failed(GtkGS * gs);
static float compute_xdpi(void);
static float compute_ydpi(void);
static gboolean compute_size(GtkGS * gs);
static void output(gpointer data, gint source, GdkInputCondition condition);
static void input(gpointer data, gint source, GdkInputCondition condition);
static void stop_interpreter(GtkGS * gs);
static gint start_interpreter(GtkGS * gs);
gboolean computeSize(void);

static GtkWidgetClass *parent_class = NULL;

static GtkGSClass *gs_class = NULL;

static gint gtk_gs_signals[LAST_SIGNAL] = { 0 };

/* Static, private functions */

static void
ggv_marshaller_VOID__POINTER(GClosure * closure,
                             GValue * return_value,
                             guint n_param_values,
                             const GValue * param_values,
                             gpointer invocation_hint, gpointer marshal_data)
{
  typedef void (*GMarshalFunc_VOID__POINTER) (gpointer data1,
                                              gpointer arg_1, gpointer data2);
  register GMarshalFunc_VOID__POINTER callback;
  register GCClosure *cc = (GCClosure *) closure;
  register gpointer data1, data2;

  g_return_if_fail(n_param_values == 2);

  if(G_CCLOSURE_SWAP_DATA(closure)) {
    data1 = closure->data;
    data2 = g_value_peek_pointer(param_values + 0);
  }
  else {
    data1 = g_value_peek_pointer(param_values + 0);
    data2 = closure->data;
  }
  callback =
    (GMarshalFunc_VOID__POINTER) (marshal_data ? marshal_data : cc->callback);

  callback(data1, g_value_get_pointer(param_values + 1), data2);
}

static void
ggv_marshaller_VOID__INT(GClosure * closure,
                         GValue * return_value,
                         guint n_param_values,
                         const GValue * param_values,
                         gpointer invocation_hint, gpointer marshal_data)
{
  typedef void (*GMarshalFunc_VOID__INT) (gpointer data1,
                                          gint arg_1, gpointer data2);
  register GMarshalFunc_VOID__INT callback;
  register GCClosure *cc = (GCClosure *) closure;
  register gpointer data1, data2;

  g_return_if_fail(n_param_values == 2);

  if(G_CCLOSURE_SWAP_DATA(closure)) {
    data1 = closure->data;
    data2 = g_value_peek_pointer(param_values + 0);
  }
  else {
    data1 = g_value_peek_pointer(param_values + 0);
    data2 = closure->data;
  }
  callback =
    (GMarshalFunc_VOID__INT) (marshal_data ? marshal_data : cc->callback);

  callback(data1, g_value_get_int(param_values + 1), data2);
}

static void
ggv_marshaller_VOID__POINTER_POINTER(GClosure * closure,
                                     GValue * return_value,
                                     guint n_param_values,
                                     const GValue * param_values,
                                     gpointer invocation_hint,
                                     gpointer marshal_data)
{
  typedef void (*GMarshalFunc_VOID__POINTER_POINTER) (gpointer data1,
                                                      gpointer arg_1,
                                                      gpointer arg_2,
                                                      gpointer data2);
  register GMarshalFunc_VOID__POINTER_POINTER callback;
  register GCClosure *cc = (GCClosure *) closure;
  register gpointer data1, data2;

  g_return_if_fail(n_param_values == 3);

  if(G_CCLOSURE_SWAP_DATA(closure)) {
    data1 = closure->data;
    data2 = g_value_peek_pointer(param_values + 0);
  }
  else {
    data1 = g_value_peek_pointer(param_values + 0);
    data2 = closure->data;
  }
  callback =
    (GMarshalFunc_VOID__POINTER_POINTER) (marshal_data ? marshal_data : cc->
                                          callback);

  callback(data1,
           g_value_get_pointer(param_values + 1),
           g_value_get_pointer(param_values + 2), data2);
}

static void
gtk_gs_init(GtkGS * gs)
{
  gs->bpixmap = NULL;
  gs->use_bpixmap = TRUE;

  gs->current_page = -2;
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

  /* Set user defined defaults */
  gs->override_orientation = gtk_gs_defaults_get_override_orientation();
  gs->fallback_orientation = gtk_gs_defaults_get_orientation();
  gs->zoom_factor = gtk_gs_defaults_get_zoom_factor();
  gs->default_size = gtk_gs_defaults_get_size();
  gs->antialiased = gtk_gs_defaults_get_antialiased();
  gs->override_size = gtk_gs_defaults_get_override_size();
  gs->respect_eof = gtk_gs_defaults_get_respect_eof();
  gs->show_scroll_rect = gtk_gs_defaults_get_show_scroll_rect();
  gs->scroll_step = gtk_gs_defaults_get_scroll_step();
  gs->zoom_mode = gtk_gs_defaults_get_zoom_mode();

  gs->scroll_start_x = gs->scroll_start_y = -1;

  gs->gs_status = _("No document loaded.");
}

static void
gtk_gs_class_init(GtkGSClass * klass)
{
  GtkObjectClass *object_class;
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass *) klass;
  gobject_class = (GObjectClass *) klass;
  widget_class = (GtkWidgetClass *) klass;
  parent_class = gtk_type_class(gtk_widget_get_type());
  gs_class = klass;

  gtk_gs_signals[INTERPRETER_MESSAGE] = g_signal_new("interpreter_message",
                                                     G_TYPE_FROM_CLASS
                                                     (object_class),
                                                     G_SIGNAL_RUN_LAST,
                                                     G_STRUCT_OFFSET
                                                     (GtkGSClass,
                                                      interpreter_message),
                                                     NULL, NULL,
                                                     ggv_marshaller_VOID__POINTER,
                                                     G_TYPE_NONE, 1,
                                                     G_TYPE_POINTER);
  gtk_gs_signals[INTERPRETER_ERROR] =
    g_signal_new("interpreter_error", G_TYPE_FROM_CLASS(object_class),
                 G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET(GtkGSClass,
                                                    interpreter_error),
                 NULL, NULL, ggv_marshaller_VOID__INT, G_TYPE_NONE, 1,
                 G_TYPE_INT);

  object_class->destroy = gtk_gs_destroy;

  widget_class->realize = gtk_gs_realize;
  widget_class->size_request = gtk_gs_size_request;
  widget_class->size_allocate = gtk_gs_size_allocate;
  widget_class->set_scroll_adjustments_signal =
    g_signal_new("set_scroll_adjustments",
                 G_TYPE_FROM_CLASS(object_class),
                 G_SIGNAL_RUN_LAST,
                 G_STRUCT_OFFSET(GtkGSClass, set_scroll_adjustments),
                 NULL,
                 NULL,
                 ggv_marshaller_VOID__POINTER_POINTER,
                 G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);

  /* Create atoms */
  klass->gs_atom = gdk_atom_intern("GHOSTVIEW", FALSE);
  klass->gs_colors_atom = gdk_atom_intern("GHOSTVIEW_COLORS", FALSE);
  klass->next_atom = gdk_atom_intern("NEXT", FALSE);
  klass->page_atom = gdk_atom_intern("PAGE", FALSE);
  klass->done_atom = gdk_atom_intern("DONE", FALSE);
  klass->string_atom = gdk_atom_intern("STRING", FALSE);

  /* a default handler for "interpreter_message" signal */
  klass->interpreter_message = gtk_gs_interpreter_message;
  /* supply a scrollable interface */
  klass->set_scroll_adjustments = gtk_gs_set_adjustments;

  gtk_gs_defaults_load();
}

/* Clean all memory and temporal files */
static void
gtk_gs_cleanup(GtkGS * gs)
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
  if(gs->pstarget && gdk_window_is_visible(gs->pstarget))
    gdk_window_hide(gs->pstarget);
  gs->current_page = -1;
  gs->loaded = FALSE;
  gs->llx = 0;
  gs->lly = 0;
  gs->urx = 0;
  gs->ury = 0;
  set_up_page(gs);
}

/* free message as it was allocated in output() */
static void
gtk_gs_interpreter_message(GtkGS * gs, gchar * msg, gpointer user_data)
{
  gdk_pointer_ungrab(GDK_CURRENT_TIME);
  if(strstr(msg, "Error:")) {
    gs->gs_status = _("File is not a valid PostScript document.");
    gtk_gs_cleanup(gs);
    g_signal_emit_by_name(G_OBJECT(gs), "interpreter_error", 1, NULL);
  }
  g_free(msg);
}

static void
gtk_gs_destroy(GtkObject * object)
{
  GtkGS *gs;

  g_return_if_fail(object != NULL);
  g_return_if_fail(GTK_IS_GS(object));

  gs = GTK_GS(object);

  gtk_gs_cleanup(gs);

  if(gs->input_buffer) {
    g_free(gs->input_buffer);
    gs->input_buffer = NULL;
  }
  if(gs->hadj) {
    g_signal_handlers_disconnect_matched(G_OBJECT(gs->hadj),
                                         G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL, NULL, gs);
    gtk_object_unref(GTK_OBJECT(gs->hadj));
    gs->hadj = NULL;
  }
  if(gs->vadj) {
    g_signal_handlers_disconnect_matched(G_OBJECT(gs->vadj),
                                         G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL, NULL, gs);
    gtk_object_unref(GTK_OBJECT(gs->vadj));
    gs->vadj = NULL;
  }

  if(GTK_OBJECT_CLASS(parent_class)->destroy)
    (*GTK_OBJECT_CLASS(parent_class)->destroy) (object);
}

/* FIXME: I'm not sure if all this is supposed to be here 
 * this is just a quick hack so that this can be called whenever
 * something changes.
 */
static void
gtk_gs_munge_adjustments(GtkGS * gs)
{
  gint x, y;

  gdk_window_get_position(gs->pstarget, &x, &y);

  /* 
   * This is a bit messy:
   * we want to make sure that we do the right thing if dragged.
   */
  if(gs->widget.allocation.width >= gs->width ||
     gs->zoom_mode != GTK_GS_ZOOM_ABSOLUTE) {
    x = (gs->widget.allocation.width - gs->width) / 2;
    gs->hadj->value = 0.0;
    gs->hadj->page_size = 1.0;
    gs->hadj->step_increment = 1.0;
  }
  else {
    if(x > 0)
      x = 0;
    else if(gs->widget.allocation.width > x + gs->width)
      x = gs->widget.allocation.width - gs->width;
    gs->hadj->page_size = ((gfloat) gs->widget.allocation.width) / gs->width;
    gs->hadj->page_increment = gs->hadj->page_size * 0.9;
    gs->hadj->step_increment = gs->scroll_step * gs->hadj->page_size;
    gs->hadj->value = -((gfloat) x) / gs->width;
  }
  if(gs->widget.allocation.height >= gs->height ||
     gs->zoom_mode == GTK_GS_ZOOM_FIT_PAGE) {
    y = (gs->widget.allocation.height - gs->height) / 2;
    gs->vadj->value = 0.0;
    gs->vadj->page_size = 1.0;
    gs->vadj->step_increment = 1.0;
  }
  else {
    if(y > 0)
      y = 0;
    else if(gs->widget.allocation.height > y + gs->height)
      y = gs->widget.allocation.height - gs->height;
    gs->vadj->page_size = ((gfloat) gs->widget.allocation.height) / gs->height;
    gs->vadj->page_increment = gs->vadj->page_size * 0.9;
    gs->vadj->step_increment = gs->scroll_step * gs->vadj->page_size;
    gs->vadj->value = -((gfloat) y) / gs->height;
  }

  gdk_window_move(gs->pstarget, x, y);

  gtk_adjustment_changed(gs->hadj);
  gtk_adjustment_changed(gs->vadj);
}

static void
gtk_gs_realize(GtkWidget * widget)
{
  GtkGS *gs;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail(widget != NULL);
  g_return_if_fail(GTK_IS_GS(widget));

  gs = GTK_GS(widget);

  /* we set up the main widget! */
  GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual(widget);
  attributes.colormap = gtk_widget_get_colormap(widget);
  attributes.event_mask = gtk_widget_get_events(widget) | GDK_EXPOSURE_MASK;
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window =
    gdk_window_new(widget->parent->window, &attributes, attributes_mask);
  gdk_window_set_user_data(widget->window, gs);
  widget->style = gtk_style_attach(widget->style, widget->window);
  gtk_style_set_background(widget->style, widget->window, GTK_STATE_NORMAL);

  /* now we set up the child window.  This is the one that ps actually draws too. */
  attributes.x = 0;
  attributes.y = 0;

  gs->pstarget = gdk_window_new(widget->window, &attributes, attributes_mask);
  gdk_window_set_user_data(gs->pstarget, widget);
  gdk_window_clear(gs->pstarget);
  gtk_style_set_background(widget->style, gs->pstarget, GTK_STATE_ACTIVE);
  gs->psgc = gdk_gc_new(gs->pstarget);
  gdk_gc_set_function(gs->psgc, GDK_INVERT);

  gs->width = 0;
  gs->height = 0;

  gtk_gs_set_page_size(gs, -1, 0);

  if((gs->width > 0) && (gs->height > 0) && GTK_WIDGET_REALIZED(gs)) {
    gtk_gs_munge_adjustments(gs);
  }

  g_signal_connect(G_OBJECT(widget), "event",
                   G_CALLBACK(gtk_gs_widget_event), gs);

  gtk_gs_goto_page(gs, gs->current_page);
}

static void
gtk_gs_size_request(GtkWidget * widget, GtkRequisition * requisition)
{
  GtkGS *gs = GTK_GS(widget);

  compute_size(gs);
  requisition->width = gs->width;
  requisition->height = gs->height;
}

static void
gtk_gs_size_allocate(GtkWidget * widget, GtkAllocation * allocation)
{
  GtkGS *gs = GTK_GS(widget);
  GdkEventConfigure event;

  g_return_if_fail(widget != NULL);
  g_return_if_fail(GTK_IS_GS(widget));
  g_return_if_fail(allocation != NULL);

  widget->allocation = *allocation;
  if(GTK_WIDGET_REALIZED(widget)) {
    gdk_window_move_resize(widget->window,
                           allocation->x, allocation->y,
                           allocation->width, allocation->height);

    event.type = GDK_CONFIGURE;
    event.window = widget->window;
    event.x = allocation->x;
    event.y = allocation->y;
    event.width = allocation->width;
    event.height = allocation->height;
    gtk_widget_event(widget, (GdkEvent *) & event);
  }

  /* 
   * update the adjustment if necessary (ie. a resize);
   */
#if 0
  if(gs->zoom_mode != GTK_GS_ZOOM_ABSOLUTE) {
    gtk_gs_set_zoom(gs, 0.0);
  }
#endif

  if(GTK_WIDGET_REALIZED(gs)) {
    gtk_gs_munge_adjustments(gs);
  }

  gtk_gs_goto_page(gs, gs->current_page);
}

static gboolean
gtk_gs_widget_event(GtkWidget * widget, GdkEvent * event, gpointer data)
{
  GtkGS *gs = (GtkGS *) data;
  if(event->type != GDK_CLIENT_EVENT)
    return FALSE;

  /* the first long is the window to communicate with gs,
     only if event if client_event */
  gs->message_window = event->client.data.l[0];

  if(event->client.message_type == gs_class->page_atom) {
    gs->busy = FALSE;
  }
  return TRUE;
}


static void
gtk_gs_value_adjustment_changed(GtkAdjustment * adjustment, gpointer data)
{
  GtkGS *gs;
  gint x, y, width, height, depth;
  gint newx, newy;

  g_return_if_fail(adjustment != NULL);
  g_return_if_fail(data != NULL);
  gs = GTK_GS(data);
  if(gs->bpixmap == NULL)
    return;

#if 0
  g_print("Adjustment %c: val = %f, page = %f, upper = %f, lower = %f\n",
          (adjustment == gs->hadj) ? 'H' : 'V',
          adjustment->value, adjustment->page_size,
          adjustment->upper, adjustment->lower);
#endif

  gdk_window_get_geometry(gs->pstarget, &x, &y, &width, &height, &depth);
  if(gs->width <= gs->widget.allocation.width)
    newx = (gs->widget.allocation.width - gs->width) / 2;
  else
    newx = -gs->hadj->value * gs->width;
  if(gs->height <= gs->widget.allocation.height)
    newy = (gs->widget.allocation.height - gs->height) / 2;
  else
    newy = -gs->vadj->value * gs->height;

  gdk_window_move(gs->pstarget, newx, newy);
}

void
gtk_gs_set_center(GtkGS * gs, gfloat hval, gfloat vval)
{
  if(hval <= gs->hadj->upper - gs->hadj->page_size / 2 &&
     hval >= gs->hadj->lower + gs->hadj->page_size / 2)
    gtk_adjustment_set_value(gs->hadj, hval);
  if(vval <= gs->vadj->upper - gs->vadj->page_size / 2 &&
     vval >= gs->vadj->lower + gs->vadj->page_size / 2)
    gtk_adjustment_set_value(gs->vadj, vval);
}

static void
send_ps(GtkGS * gs, long begin, unsigned int len, gboolean close)
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

static void
set_up_page(GtkGS * gs)
     /* 
      * This is used to prepare the widget internally for
      * a new document. It sets gs->pstarget to the
      * correct size and position, and updates the 
      * adjustments appropriately.
      *
      * It is not meant to be used every time a specific page
      * is selected.
      *
      * NOTE: It expects the widget is realized.
      */
{
  guint orientation;
  char buf[1024];
  GdkPixmap *pprivate;
  GdkColormap *colormap;
  GdkGC *fill;
  GdkColor white = { 0, 0xFFFF, 0xFFFF, 0xFFFF };   /* pixel, r, g, b */

#ifdef HAVE_LOCALE_H
  char *savelocale;
#endif

  if(!GTK_WIDGET_REALIZED(gs))
    return;

  /* Do we have to check if the actual geometry changed? */

  stop_interpreter(gs);

  orientation = gtk_gs_get_orientation(gs);

  if(compute_size(gs)) {
    gdk_flush();

    /* clear new pixmap (set to white) */
    fill = gdk_gc_new(gs->pstarget);
    if(fill) {
      colormap = gtk_widget_get_colormap(GTK_WIDGET(gs));
      gdk_color_alloc(colormap, &white);
      gdk_gc_set_foreground(fill, &white);

      if(gs->use_bpixmap && gs->width > 0 && gs->height > 0) {
        if(gs->bpixmap) {
          gdk_drawable_unref(gs->bpixmap);
          gs->bpixmap = NULL;
        }

        gs->bpixmap = gdk_pixmap_new(gs->pstarget, gs->width, gs->height, -1);

        gdk_draw_rectangle(gs->bpixmap, fill, TRUE,
                           0, 0, gs->width, gs->height);

        gdk_window_set_back_pixmap(gs->pstarget, gs->bpixmap, FALSE);
      }
      else {
        gdk_draw_rectangle(gs->pstarget, fill, TRUE,
                           0, 0, gs->width, gs->height);
      }
      gdk_gc_unref(fill);

      gdk_window_resize(gs->pstarget, gs->width, gs->height);

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
  pprivate = (GdkPixmap *) gs->bpixmap;

  g_snprintf(buf, 1024, "%ld %d %d %d %d %d %f %f %d %d %d %d",
             pprivate ? gdk_x11_drawable_get_xid(pprivate) : 0L,
             orientation * 90,
             gs->llx,
             gs->lly,
             gs->urx,
             gs->ury,
             gs->xdpi * gs->zoom_factor,
             gs->ydpi * gs->zoom_factor,
             gs->left_margin,
             gs->bottom_margin, gs->right_margin, gs->top_margin);

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
is_interpreter_ready(GtkGS * gs)
{
  return (gs->interpreter_pid != -1 && !gs->busy && gs->ps_input == NULL);
}

static void
interpreter_failed(GtkGS * gs)
{
  stop_interpreter(gs);
}

static void
output(gpointer data, gint source, GdkInputCondition condition)
{
  char buf[MAX_BUFSIZE + 1], *msg;
  guint bytes = 0;
  GtkGS *gs = GTK_GS(data);

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
    gtk_signal_emit(GTK_OBJECT(gs), gtk_gs_signals[INTERPRETER_MESSAGE], msg);
  }
}

static void
input(gpointer data, gint source, GdkInputCondition condition)
{
  GtkGS *gs = GTK_GS(data);
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
        gtk_gs_emit_error_msg(gs, g_strdup(_("Broken pipe.")));
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
start_interpreter(GtkGS * gs)
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
    argv[argc++] = GTK_GS_GET_PS_FILE(gs);
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

    gv_env = g_strdup_printf("GHOSTVIEW=%ld",
                             gdk_x11_drawable_get_xid(gs->pstarget));
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
stop_interpreter(GtkGS * gs)
{
  if(gs->interpreter_pid > 0) {
    int status = 0;
    kill(gs->interpreter_pid, SIGTERM);
    while((wait(&status) == -1) && (errno == EINTR)) ;
    gs->interpreter_pid = -1;
    if(status == 1) {
      gtk_gs_cleanup(gs);
      gs->gs_status = _("Interpreter failed.");
      g_signal_emit_by_name(G_OBJECT(gs), "interpreter_error", status);
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


/*
 * Decompress gs->gs_filename if necessary
 * Set gs->filename_unc to the name of the uncompressed file or NULL.
 * Error reporting via signal 'interpreter_message'
 * Return name of input file to use or NULL on error..
 */
static gchar *
check_filecompressed(GtkGS * gs)
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
  filename = ggv_quote_filename(gs->gs_filename);
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
     && ggv_file_readable(filename_unc)
     && (ggv_file_length(filename_err) == 0)) {
    /* sucessfully uncompressed file */
    gs->gs_filename_unc = filename_unc;
  }
  else {
    /* report error */
    g_snprintf(buf, 1024, _("Error while decompressing file %s:\n"),
               gs->gs_filename);
    gtk_gs_emit_error_msg(gs, buf);
    if(ggv_file_length(filename_err) > 0) {
      FILE *err;
      if((err = fopen(filename_err, "r"))) {
        /* print file to message window */
        while(fgets(buf, 1024, err))
          gtk_gs_emit_error_msg(gs, buf);
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

/*
 * Check if gs->gs_filename or gs->gs_filename_unc is a pdf file and scan
 * pdf file if necessary.
 * Set gs->filename_dsc to the name of the dsc file or NULL.
 * Error reporting via signal 'interpreter_message'.
 */
static gchar *
check_pdf(GtkGS * gs)
{
  FILE *file;
  gchar buf[1024], *filename;
  int fd;

  /* use uncompressed file as input if necessary */
  filename = (gs->gs_filename_unc ? gs->gs_filename_unc : gs->gs_filename);

  if((file = fopen(filename, "r"))
     && (fread(buf, sizeof(char), 5, file) == 5)
     && (strncmp(buf, "%PDF-", 5) == 0)) {
    /* we found a PDF file */
    gchar *fname, *filename_dsc, *filename_err, *cmd, *cmdline;
    filename_dsc = g_strconcat(g_get_tmp_dir(), "/ggvXXXXXX", NULL);
    if((fd = mkstemp(filename_dsc)) < 0) {
      return NULL;
    }
    close(fd);
    filename_err = g_strconcat(g_get_tmp_dir(), "/ggvXXXXXX", NULL);
    if((fd = mkstemp(filename_err)) < 0) {
      g_free(filename_dsc);
      return NULL;
    }
    close(fd);
    fname = ggv_quote_filename(filename);
    cmd = g_strdup_printf(gtk_gs_defaults_get_dsc_cmd(), filename_dsc, fname);
    g_free(fname);
    /* this command (sometimes?) prints error messages to stdout! */
    cmdline = g_strdup_printf("%s >%s 2>&1", cmd, filename_err);
    g_free(cmd);

    if((system(cmdline) == 0) && ggv_file_readable(filename_dsc)) {

      /* success */
      filename = gs->gs_filename_dsc = filename_dsc;

      if(ggv_file_length(filename_err) > 0) {
        gchar *err_msg = " ";
        GtkWidget *dialog;
        FILE *err;
        GdkColor color;

        if((err = fopen(filename_err, "r"))) {

          /* print the content of the file to a message box */
          while(fgets(buf, 1024, err))
            err_msg = g_strconcat(err_msg, buf, NULL);

          /* FIXME The dialog is not yet set to modal, difficult to 
           * get the parent of the dialog box here 
           */

          dialog = gtk_message_dialog_new(NULL,
                                          GTK_DIALOG_MODAL,
                                          GTK_MESSAGE_WARNING,
                                          GTK_BUTTONS_OK,
                                          ("There was an error while scaning the file: %s \n%s"),
                                          gs->gs_filename, err_msg);

          gdk_color_parse("white", &color);
          gtk_widget_modify_bg(GTK_WIDGET(dialog), GTK_STATE_NORMAL, &color);

          g_signal_connect(G_OBJECT(dialog), "response",
                           G_CALLBACK(gtk_widget_destroy), NULL);

          gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
          gtk_widget_show(dialog);
          g_free(err_msg);
        }
      }

    }
    else {
      /* report error */
      g_snprintf(buf, 1024,
                 _("Error while converting pdf file %s:\n"), filename);
      gtk_gs_emit_error_msg(gs, buf);

      if(ggv_file_length(filename_err) > 0) {
        FILE *err;
        if((err = fopen(filename_err, "r"))) {
          /* print file to message window */
          while(fgets(buf, 1024, err))
            gtk_gs_emit_error_msg(gs, buf);
        }
      }
      unlink(filename_dsc);
      g_free(filename_dsc);
      filename = NULL;
    }
    unlink(filename_err);
    g_free(filename_err);
    g_free(cmdline);
  }
  if(NULL != file)
    fclose(file);
  return filename;
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
compute_size(GtkGS * gs)
{
  guint new_width = 1;
  guint new_height = 1;
  gboolean change = FALSE;
  gint orientation;

  /* width and height can be changed, calculate window size according */
  /* to xpdi and ydpi */
  orientation = gtk_gs_get_orientation(gs);

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
  if(GTK_WIDGET_REALIZED(gs)) {
    if(!gs->loaded) {
      if(gdk_window_is_visible(gs->pstarget))
        gdk_window_hide(gs->pstarget);
    }
    else {
      if(!gdk_window_is_visible(gs->pstarget) && gs->width > 0
         && gs->height > 0)
        gdk_window_show(gs->pstarget);
    }
    gtk_gs_munge_adjustments(gs);
  }

  return (change);
}

gint
gtk_gs_enable_interpreter(GtkGS * gs)
{
  g_return_val_if_fail(gs != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_GS(gs), FALSE);

  if(!gs->gs_filename)
    return 0;

  gs->disable_start = FALSE;
  if(GTK_WIDGET_REALIZED(gs)) {
    return start_interpreter(gs);
  }
  else {
    return 0;
  }
}

/* publicly accessible functions */

GType
gtk_gs_get_type(void)
{
  static GType gs_type = 0;
  if(!gs_type) {
    GTypeInfo gs_info = {
      sizeof(GtkGSClass),
      (GBaseInitFunc) NULL,
      (GBaseFinalizeFunc) NULL,
      (GClassInitFunc) gtk_gs_class_init,
      (GClassFinalizeFunc) NULL,
      NULL,                     /* class_data */
      sizeof(GtkGS),
      0,                        /* n_preallocs */
      (GInstanceInitFunc) gtk_gs_init
    };

    gs_type = g_type_register_static(gtk_widget_get_type(),
                                     "GtkGS", &gs_info, 0);
  }
  return gs_type;


}

GtkWidget *
gtk_gs_new(GtkAdjustment * hadj, GtkAdjustment * vadj)
{
  GtkGS *gs;

  if(NULL == hadj)
    hadj = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 1.0, 0.01, 0.1, 0.09));
  if(NULL == vadj)
    vadj = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 1.0, 0.01, 0.1, 0.09));

  gs = (GtkGS *) gtk_type_new(gtk_gs_get_type());

  gtk_gs_set_adjustments(gs, hadj, vadj);

  return GTK_WIDGET(gs);
}


GtkWidget *
gtk_gs_new_from_file(GtkAdjustment * hadj, GtkAdjustment * vadj, char *fname)
{
  GtkWidget *gs = gtk_gs_new(hadj, vadj);
  gtk_gs_load(GTK_GS(gs), fname);
  return gs;
}

void
gtk_gs_reload(GtkGS * gs)
{
  gchar *fname;
  gfloat hval = gs->hadj->value;
  gfloat vval = gs->vadj->value;
  gint page;

  if(!gs->gs_filename)
    return;

  page = gtk_gs_get_current_page(gs);
  fname = g_strdup(gs->gs_filename);
  gtk_gs_load(gs, fname);
  gtk_gs_goto_page(gs, page);
  gtk_adjustment_set_value(gs->hadj, hval);
  gtk_adjustment_set_value(gs->vadj, vval);
  g_free(fname);
}


/*
 * Show error message -> send signal "interpreter_message"
 */
static void
gtk_gs_emit_error_msg(GtkGS * gs, const gchar * msg)
{
  gtk_signal_emit(GTK_OBJECT(gs),
                  gtk_gs_signals[INTERPRETER_MESSAGE], g_strdup(msg));
}


void
gtk_gs_center_page(GtkGS * gs)
{
  g_return_if_fail(gs != NULL);
  g_return_if_fail(GTK_IS_GS(gs));

  gdk_window_move(gs->pstarget,
                  (gs->widget.allocation.width - gs->width) / 2,
                  (gs->widget.allocation.height - gs->height) / 2);
  gs->hadj->page_size = ((gfloat) gs->widget.allocation.width) / gs->width;
  gs->hadj->page_size = MIN(gs->hadj->page_size, 1.0);
  gs->vadj->page_size = ((gfloat) gs->widget.allocation.height) / gs->height;
  gs->vadj->page_size = MIN(gs->vadj->page_size, 1.0);
  gs->hadj->value = 0.5 - gs->hadj->page_size / 2;
  gs->vadj->value = 0.5 - gs->vadj->page_size / 2;
  gtk_adjustment_changed(gs->hadj);
  gtk_adjustment_changed(gs->vadj);
}

void
gtk_gs_scroll(GtkGS * gs, gint x_delta, gint y_delta)
{
  gfloat hval, vval;

  g_return_if_fail(gs != NULL);
  g_return_if_fail(GTK_IS_GS(gs));

  hval = gs->hadj->value + ((gfloat) x_delta) / gs->width;
  vval = gs->vadj->value + ((gfloat) y_delta) / gs->height;
  if(hval <= gs->hadj->upper - gs->hadj->page_size && hval >= gs->hadj->lower)
    gtk_adjustment_set_value(gs->hadj, hval);
  if(vval <= gs->vadj->upper - gs->vadj->page_size && vval >= gs->vadj->lower)
    gtk_adjustment_set_value(gs->vadj, vval);
}

void
gtk_gs_disable_interpreter(GtkGS * gs)
{
  g_return_if_fail(gs != NULL);
  g_return_if_fail(GTK_IS_GS(gs));

  gs->disable_start = TRUE;
  if(GTK_WIDGET_REALIZED(GTK_WIDGET(gs)))
    stop_interpreter(gs);
}

gboolean
gtk_gs_load(GtkGS * gs, const gchar * fname)
{
  g_return_val_if_fail(gs != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_GS(gs), FALSE);

  /* clean up previous document */
  gtk_gs_cleanup(gs);

  if(fname == NULL) {
    if(gs->pstarget != NULL && gdk_window_is_visible(gs->pstarget))
      gdk_window_hide(gs->pstarget);
    gs->gs_status = "";
    return FALSE;
  }

  /* prepare this document */

  /* default values: no dsc information available  */
  gs->structured_doc = FALSE;
  gs->send_filename_to_gs = TRUE;
  gs->current_page = -2;
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

    if(!ggv_file_readable(fname)) {
      gchar buf[1024];
      g_snprintf(buf, 1024, _("Cannot open file %s.\n"), fname);
      gtk_gs_emit_error_msg(gs, buf);
      gs->gs_status = _("File is not readable.");
    }
    else {
      filename = check_filecompressed(gs);
      if(filename)
        filename = check_pdf(gs);
    }

    if(!filename || (gs->gs_psfile = fopen(filename, "r")) == NULL) {
      gtk_gs_cleanup(gs);
      return FALSE;
    }

    /* we grab the vital statistics!!! */
    gs->doc = psscan(gs->gs_psfile, gs->respect_eof, filename);

    if(gs->doc == NULL) {
      /* File does not seem to be a Postscript one */
      gchar buf[1024];
      g_snprintf(buf, 1024, _("Error while scanning file %s\n"), fname);
      gtk_gs_emit_error_msg(gs, buf);
      gtk_gs_cleanup(gs);
      gs->gs_status = _("The file is not a PostScript document.");
      return FALSE;
    }

    if((!gs->doc->epsf && gs->doc->numpages > 0) ||
       (gs->doc->epsf && gs->doc->numpages > 1)) {
      gs->structured_doc = TRUE;
      gs->send_filename_to_gs = FALSE;
    }

    /* We have to set up the orientation of the document */


    /* orientation can only be portrait, and landscape or none.
       This is the document default. A document can have
       pages in landscape and some in portrait */
    if(gs->override_orientation) {
      /* If the orientation should be override... 
         then gs->orientation has already the correct
         value (it was set when the widget was created */
      /* So do nothing */

    }
    else {
      /* Otherwise, set the proper orientation for the doc */
      gs->real_orientation = gs->doc->orientation;
    }
  }
  gtk_gs_set_page_size(gs, -1, gs->current_page);
  gtk_widget_queue_resize(&(gs->widget));
  gs->loaded = TRUE;

  gs->gs_status = _("Document loaded.");

  return gs->loaded;
}


gboolean
gtk_gs_next_page(GtkGS * gs)
{
  XEvent event;

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

gint
gtk_gs_get_current_page(GtkGS * gs)
{
  g_return_val_if_fail(gs != NULL, -1);
  g_return_val_if_fail(GTK_IS_GS(gs), -1);

  return gs->current_page;
}

gint
gtk_gs_get_page_count(GtkGS * gs)
{
  if(!gs->gs_filename)
    return 0;

  if(gs->doc) {
    if(gs->structured_doc)
      return gs->doc->numpages;
    else
      return G_MAXINT;
  }
  else
    return 0;
}

gboolean
gtk_gs_goto_page(GtkGS * gs, gint page)
{
  g_return_val_if_fail(gs != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_GS(gs), FALSE);

  if(!gs->gs_filename) {
    return FALSE;
  }

  /* range checking... */
  if(page < 0)
    page = 0;

  if(gs->structured_doc && gs->doc) {
    if(page >= gs->doc->numpages)
      page = gs->doc->numpages - 1;

    if(page == gs->current_page && !gs->changed)
      return TRUE;

    gs->current_page = page;

    if(!GTK_WIDGET_REALIZED(gs))
      return FALSE;

    if(gs->doc->pages[page].orientation != NONE &&
       !gs->override_orientation &&
       gs->doc->pages[page].orientation != gs->real_orientation) {
      gs->real_orientation = gs->doc->pages[page].orientation;
      gs->changed = TRUE;
    }

    gtk_gs_set_page_size(gs, -1, page);

    gs->changed = FALSE;

    if(is_interpreter_ready(gs)) {
      gtk_gs_next_page(gs);
    }
    else {
      gtk_gs_enable_interpreter(gs);
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
    if(page == gs->current_page && !gs->changed)
      return TRUE;

    if(!GTK_WIDGET_REALIZED(gs))
      return FALSE;

    if(!is_interpreter_ready(gs))
      gtk_gs_enable_interpreter(gs);

    gs->current_page = page;

    gtk_gs_next_page(gs);
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
gboolean
gtk_gs_set_page_size(GtkGS * gs, gint new_pagesize, gint pageid)
{
  gint new_llx = 0;
  gint new_lly = 0;
  gint new_urx = 0;
  gint new_ury = 0;
  GtkGSPaperSize *papersizes = gtk_gs_defaults_get_paper_sizes();

  g_return_val_if_fail(gs != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_GS(gs), FALSE);

  if(new_pagesize == -1) {
    if(gs->default_size > 0)
      new_pagesize = gs->default_size;
    if(!gs->override_size && gs->doc) {
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
  if(gs->doc && ((gs->doc->epsf && !gs->override_size) || new_pagesize == -1)) {    /* epsf or bbox */
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
    if(gs->doc && !gs->override_size && gs->doc->size &&
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
  /* gtk_gs_disable_interpreter (gs); */
  if((new_llx != gs->llx) || (new_lly != gs->lly) ||
     (new_urx != gs->urx) || (new_ury != gs->ury)) {
    gs->llx = new_llx;
    gs->lly = new_lly;
    gs->urx = new_urx;
    gs->ury = new_ury;
    gs->changed = TRUE;
  }

  if(gs->changed) {
    if(GTK_WIDGET_REALIZED(gs)) {
      set_up_page(gs);
      gtk_widget_queue_resize(&(gs->widget));
    }
    return TRUE;
  }

  return FALSE;
}

void
gtk_gs_set_override_orientation(GtkGS * gs, gboolean bNewOverride)
{
  gint iOldOrientation;

  g_return_if_fail(gs != NULL);
  g_return_if_fail(GTK_IS_GS(gs));

  iOldOrientation = gtk_gs_get_orientation(gs);

  gs->override_orientation = bNewOverride;

  /* If the current orientation is different from the 
     new orientation  then redisplay */
  if(iOldOrientation != gtk_gs_get_orientation(gs)) {
    gs->changed = TRUE;
    if(GTK_WIDGET_REALIZED(gs))
      set_up_page(gs);
  }
  gtk_widget_queue_resize(&(gs->widget));
}

gboolean
gtk_gs_get_override_orientation(GtkGS * gs)
{
  g_return_val_if_fail(gs != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_GS(gs), FALSE);

  return gs->override_orientation;
}

void
gtk_gs_set_override_size(GtkGS * gs, gboolean f)
{
  g_return_if_fail(gs != NULL);
  g_return_if_fail(GTK_IS_GS(gs));

  if(f != gs->override_size) {
    gs->override_size = f;
    gs->changed = TRUE;
    gtk_gs_set_page_size(gs, -1, gs->current_page);
    if(GTK_WIDGET_REALIZED(gs))
      set_up_page(gs);
  }
  gtk_widget_queue_resize(&(gs->widget));
}

gboolean
gtk_gs_get_override_size(GtkGS * gs)
{
  g_return_val_if_fail(gs != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_GS(gs), FALSE);

  return gs->override_size;
}

void
gtk_gs_set_zoom(GtkGS * gs, gfloat zoom)
{
  g_return_if_fail(gs != NULL);
  g_return_if_fail(GTK_IS_GS(gs));

  switch (gs->zoom_mode) {
  case GTK_GS_ZOOM_FIT_WIDTH:
    zoom = gtk_gs_zoom_to_fit(gs, TRUE);
    break;
  case GTK_GS_ZOOM_FIT_PAGE:
    zoom = gtk_gs_zoom_to_fit(gs, FALSE);
    break;
  case GTK_GS_ZOOM_ABSOLUTE:
  default:
    break;
  }
  if(zoom < ggv_zoom_levels[0])
    zoom = ggv_zoom_levels[0];
  else if(zoom > ggv_zoom_levels[ggv_max_zoom_levels])
    zoom = ggv_zoom_levels[ggv_max_zoom_levels];
  if(fabs(gs->zoom_factor - zoom) > 0.001) {
    gs->zoom_factor = zoom;
    if(GTK_WIDGET_REALIZED(gs))
      set_up_page(gs);
    gs->changed = TRUE;
    gtk_widget_queue_resize(&(gs->widget));
  }
}

gfloat
gtk_gs_get_zoom(GtkGS * gs)
{
  g_return_val_if_fail(gs != NULL, 0.0);
  g_return_val_if_fail(GTK_IS_GS(gs), 0.0);

  return gs->zoom_factor;
}

gfloat
gtk_gs_zoom_to_fit(GtkGS * gs, gboolean fit_width)
{
  gint new_y;
  gfloat new_zoom;
  guint avail_w, avail_h;

  g_return_val_if_fail(gs != NULL, 0.0);
  g_return_val_if_fail(GTK_IS_GS(gs), 0.0);

  avail_w = (gs->avail_w > 0) ? gs->avail_w : gs->width;
  avail_h = (gs->avail_h > 0) ? gs->avail_h : gs->height;

  new_zoom = ((gfloat) avail_w) / ((gfloat) gs->width) * gs->zoom_factor;
  if(!fit_width) {
    new_y = new_zoom * ((gfloat) gs->height) / gs->zoom_factor;
    if(new_y > avail_h)
      new_zoom = ((gfloat) avail_h) / ((gfloat) gs->height) * gs->zoom_factor;
  }

  return new_zoom;
}

gboolean
gtk_gs_set_default_orientation(GtkGS * gs, gint orientation)
{
  gint iOldOrientation;

  g_return_val_if_fail(gs != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_GS(gs), FALSE);
  g_return_val_if_fail((orientation == GTK_GS_ORIENTATION_PORTRAIT) ||
                       (orientation == GTK_GS_ORIENTATION_LANDSCAPE) ||
                       (orientation == GTK_GS_ORIENTATION_UPSIDEDOWN) ||
                       (orientation == GTK_GS_ORIENTATION_SEASCAPE), FALSE);

  iOldOrientation = gtk_gs_get_orientation(gs);
  gs->fallback_orientation = orientation;

  /* We are setting the fallback orientation */
  if(iOldOrientation != gtk_gs_get_orientation(gs)) {
    gs->changed = TRUE;
    if(GTK_WIDGET_REALIZED(gs))
      set_up_page(gs);
    gtk_widget_queue_resize(&(gs->widget));
    return TRUE;
  }

  return FALSE;
}

gint
gtk_gs_get_default_orientation(GtkGS * gs)
{
  g_return_val_if_fail(gs != NULL, -1);
  g_return_val_if_fail(GTK_IS_GS(gs), -1);

  return gs->fallback_orientation;
}

gint
gtk_gs_get_orientation(GtkGS * gs)
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

  if(gs->override_orientation ||
     gs->real_orientation == GTK_GS_ORIENTATION_NONE)
    return gs->fallback_orientation;
  else
    return gs->real_orientation;
}

void
gtk_gs_set_default_size(GtkGS * gs, gint size)
{
  g_return_if_fail(gs != NULL);
  g_return_if_fail(GTK_IS_GS(gs));

  gs->default_size = size;
  gtk_gs_set_page_size(gs, -1, gs->current_page);
}

gint
gtk_gs_get_default_size(GtkGS * gs)
{
  g_return_val_if_fail(gs != NULL, -1);
  g_return_val_if_fail(GTK_IS_GS(gs), -1);

  return gs->default_size;
}

void
gtk_gs_set_respect_eof(GtkGS * gs, gboolean f)
{
  g_return_if_fail(gs != NULL);
  g_return_if_fail(GTK_IS_GS(gs));

  if(gs->respect_eof == f)
    return;

  gs->respect_eof = f;
  gtk_gs_set_page_size(gs, -1, gs->current_page);
}

gint
gtk_gs_get_respect_eof(GtkGS * gs)
{
  g_return_val_if_fail(gs != NULL, -1);
  g_return_val_if_fail(GTK_IS_GS(gs), -1);

  return gs->respect_eof;
}

void
gtk_gs_set_antialiasing(GtkGS * gs, gboolean f)
{
  g_return_if_fail(gs != NULL);
  g_return_if_fail(GTK_IS_GS(gs));

  if(gs->antialiased == f)
    return;

  gs->antialiased = f;
  gs->changed = TRUE;
  if(GTK_WIDGET_REALIZED(gs))
    start_interpreter(gs);
  gtk_gs_goto_page(gs, gs->current_page);
}

gint
gtk_gs_get_antialiasing(GtkGS * gs)
{
  g_return_val_if_fail(gs != NULL, -1);
  g_return_val_if_fail(GTK_IS_GS(gs), -1);

  return gs->antialiased;
}

const gchar *
gtk_gs_get_document_title(GtkGS * gs)
{
  g_return_val_if_fail(gs != NULL, NULL);
  g_return_val_if_fail(GTK_IS_GS(gs), NULL);

  if(gs->doc && gs->doc->title)
    return gs->doc->title;

  return NULL;
}

guint
gtk_gs_get_document_numpages(GtkGS * widget)
{
  g_return_val_if_fail(widget != NULL, 0);
  g_return_val_if_fail(GTK_IS_GS(widget), 0);

  if(widget->doc)
    return widget->doc->numpages;

  return 0;
}

const gchar *
gtk_gs_get_document_page_label(GtkGS * widget, int page)
{
  g_return_val_if_fail(widget != NULL, NULL);
  g_return_val_if_fail(GTK_IS_GS(widget), NULL);

  if(widget->doc && widget->doc->pages && (widget->doc->numpages >= page))
    return widget->doc->pages[page - 1].label;

  return NULL;
}

gint
gtk_gs_get_size_index(const gchar * string, GtkGSPaperSize * size)
{
  guint idx = 0;

  while(size[idx].name != NULL) {
    if(strcmp(size[idx].name, string) == 0)
      return idx;
    idx++;
  }

  return -1;
}

void
gtk_gs_start_scroll(GtkGS * gs)
{
  gint x, y, w, h;

  if(!GTK_WIDGET_REALIZED(gs) || !gs->show_scroll_rect)
    return;

  gdk_window_get_geometry(gs->pstarget, &x, &y, &w, &h, NULL);
  gs->scroll_start_x = MAX(-x, 0);
  gs->scroll_start_y = MAX(-y, 0);
  gs->scroll_width = MIN(gs->widget.allocation.width - 1, w - 1);
  gs->scroll_height = MIN(gs->widget.allocation.height - 1, h - 1);

  if(gs->bpixmap) {
    GdkRectangle rect;
    rect.x = gs->scroll_start_x;
    rect.y = gs->scroll_start_y;
    rect.width = gs->scroll_width + 1;
    rect.height = gs->scroll_height + 1;
    gdk_draw_rectangle(gs->bpixmap, gs->psgc, FALSE,
                       gs->scroll_start_x, gs->scroll_start_y,
                       gs->scroll_width, gs->scroll_height);
    rect.width = 1;
    gdk_window_invalidate_rect(gs->pstarget, &rect, TRUE);
    rect.x = gs->scroll_start_x + gs->scroll_width;
    gdk_window_invalidate_rect(gs->pstarget, &rect, TRUE);
    rect.x = gs->scroll_start_x + 1;
    rect.width = gs->scroll_start_x + gs->scroll_width - 1;
    rect.height = 1;
    gdk_window_invalidate_rect(gs->pstarget, &rect, TRUE);
    rect.y = gs->scroll_start_y + gs->scroll_height;
    gdk_window_invalidate_rect(gs->pstarget, &rect, TRUE);
  }
}

void
gtk_gs_end_scroll(GtkGS * gs)
{
  if(!GTK_WIDGET_REALIZED(gs) || !gs->show_scroll_rect)
    return;

  if(gs->scroll_start_x == -1 || gs->scroll_start_y == -1)
    return;

  if(gs->bpixmap) {
    GdkRectangle rect;
    rect.x = gs->scroll_start_x;
    rect.y = gs->scroll_start_y;
    rect.width = gs->scroll_width + 1;
    rect.height = gs->scroll_height + 1;
    gdk_draw_rectangle(gs->bpixmap, gs->psgc, FALSE,
                       gs->scroll_start_x, gs->scroll_start_y,
                       gs->scroll_width, gs->scroll_height);
    rect.width = 1;
    gdk_window_invalidate_rect(gs->pstarget, &rect, TRUE);
    rect.x = gs->scroll_start_x + gs->scroll_width;
    gdk_window_invalidate_rect(gs->pstarget, &rect, TRUE);
    rect.x = gs->scroll_start_x + 1;
    rect.width = gs->scroll_start_x + gs->scroll_width - 1;
    rect.height = 1;
    gdk_window_invalidate_rect(gs->pstarget, &rect, TRUE);
    rect.y = gs->scroll_start_y + gs->scroll_height;
    gdk_window_invalidate_rect(gs->pstarget, &rect, TRUE);
  }
  gs->scroll_start_x = -1;
  gs->scroll_start_y = -1;
}

void
gtk_gs_set_show_scroll_rect(GtkGS * gs, gboolean f)
{
  gs->show_scroll_rect = f;
}

gboolean
gtk_gs_get_show_scroll_rect(GtkGS * gs)
{
  return gs->show_scroll_rect;
}

gboolean
gtk_gs_scroll_to_edge(GtkGS * gs, GtkPositionType vertical,
                      GtkPositionType horizontal)
{
  g_return_val_if_fail(gs != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_GS(gs), FALSE);

  switch (vertical) {
  case GTK_POS_TOP:
    gs->vadj->value = gs->vadj->lower;
    gtk_adjustment_value_changed(gs->vadj);
    break;
  case GTK_POS_BOTTOM:
    gs->vadj->value = gs->vadj->upper - gs->vadj->page_size;
    gtk_adjustment_value_changed(gs->vadj);
    break;
  default:
    g_assert(0);                /* Illegal parameter error */
  }


  switch (horizontal) {
  case GTK_POS_TOP:
    gs->hadj->value = gs->hadj->lower;
    gtk_adjustment_value_changed(gs->hadj);
    break;
  case GTK_POS_BOTTOM:
    gs->hadj->value = gs->hadj->upper - gs->hadj->page_size;
    gtk_adjustment_value_changed(gs->hadj);
    break;
  default:
    g_assert(0);                /* Illegal parameter error */
  }

  return TRUE;
}

gboolean
gtk_gs_scroll_step(GtkGS * gs, GtkScrollType direction, gboolean dowrap)
{
  GtkAdjustment *MainAdj;       /* We will move this adjustment */
  GtkAdjustment *SecoAdj;       /* And this _only_ if we can't move MainAdj (ie. we're edge)
                                   and there is wrapping */

  gboolean MoveHorizontal = TRUE;   /* Positive if we move horizontal */
  gboolean DirectionFlag = TRUE;    /* Positive if we move towards upper */
  g_return_val_if_fail(gs != NULL, FALSE);
  g_return_val_if_fail(GTK_IS_GS(gs), FALSE);

#define EPSILON 0.00005

#define CHECK_THERE_IS_NO_LOWER_SPACE(adj) \
        ((adj)->value - (EPSILON) <= (adj)->lower)
#define CHECK_THERE_IS_NO_UPPER_SPACE(adj) \
        ((adj)->value + (EPSILON) >= (adj)->upper - (adj)->page_size)

#define CHECK_THERE_IS_NO_SPACE_FOR_STEP(adj,dir) \
        (dir?CHECK_THERE_IS_NO_UPPER_SPACE(adj):CHECK_THERE_IS_NO_LOWER_SPACE(adj))

  /* To make code more readable, we make a macro */
#define ADVANCE_TOWARDS_LOWER(adj) \
        (adj->value -= gs->scroll_step * (adj->page_size))
#define ADVANCE_TOWARDS_UPPER(adj) \
        (adj->value += gs->scroll_step * (adj->page_size))

#define ADVANCE_STEP(adj,dir) \
        (dir?ADVANCE_TOWARDS_UPPER(adj):ADVANCE_TOWARDS_LOWER(adj))

#define MOVE_TO_LOWER_EDGE(adj) \
        (adj->value = adj->lower)
#define MOVE_TO_UPPER_EDGE(adj) \
        (adj->value = adj->upper - adj->page_size)

  /* if upper is 1 goto upper, otherwise to lower */
#define MOVE_TO_EDGE(adj,upper) (upper?MOVE_TO_UPPER_EDGE(adj):MOVE_TO_LOWER_EDGE(adj))

  /* These variables make our life easier */
  switch (direction) {
  case GTK_SCROLL_STEP_RIGHT:
    MoveHorizontal = TRUE;
    DirectionFlag = TRUE;
    break;
  case GTK_SCROLL_STEP_LEFT:
    MoveHorizontal = TRUE;
    DirectionFlag = FALSE;
    break;
  case GTK_SCROLL_STEP_DOWN:
    MoveHorizontal = FALSE;
    DirectionFlag = TRUE;
    break;
  case GTK_SCROLL_STEP_UP:
    MoveHorizontal = FALSE;
    DirectionFlag = FALSE;
    break;
  default:
    g_warning("Illegal scroll step direction.");
  }

  if(MoveHorizontal) {
    MainAdj = gs->hadj;
    SecoAdj = gs->vadj;
  }
  else {
    MainAdj = gs->vadj;
    SecoAdj = gs->hadj;
  }

  if(CHECK_THERE_IS_NO_SPACE_FOR_STEP(MainAdj, DirectionFlag)) {
    if(!dowrap)
      return FALSE;
    /* Move in the oposite axis */
    if(CHECK_THERE_IS_NO_SPACE_FOR_STEP(SecoAdj, DirectionFlag)) {
      /* there is no place to move, we need a new page */
      return FALSE;
    }
    ADVANCE_STEP(SecoAdj, DirectionFlag);

    if(CHECK_THERE_IS_NO_SPACE_FOR_STEP(SecoAdj, DirectionFlag)) {
      /* We move it too far, lets move it to the edge */
      MOVE_TO_EDGE(SecoAdj, DirectionFlag);
    }
    /* now move to edge (other axis) in oposite direction */
    MOVE_TO_EDGE(MainAdj, !DirectionFlag);
    gtk_adjustment_value_changed(SecoAdj);
    return TRUE;
  }

  /* Now we know we can move in the direction sought */
  ADVANCE_STEP(MainAdj, DirectionFlag);

  if(CHECK_THERE_IS_NO_SPACE_FOR_STEP(MainAdj, DirectionFlag)) {
    /* We move it too far, lets move it to the edge */
    MOVE_TO_EDGE(MainAdj, DirectionFlag);
  }
  gtk_adjustment_value_changed(MainAdj);

  return TRUE;
}

gchar *
gtk_gs_get_postscript(GtkGS * gs, gint * pages)
{
  GtkGSDocSink *sink;
  gchar *doc;
  gboolean free_pages = FALSE;

  if(pages == NULL) {
    if(!gs->structured_doc) {
      FILE *f;
      struct stat sb;

      if(stat(GTK_GS_GET_PS_FILE(gs), &sb))
        return NULL;
      doc = g_new(gchar, sb.st_size);
      if(!doc)
        return NULL;
      f = fopen(GTK_GS_GET_PS_FILE(gs), "r");
      if(NULL != f && fread(doc, sb.st_size, 1, f) != 1) {
        g_free(doc);
        doc = NULL;
      }
      if(NULL != f)
        fclose(f);
      return doc;
    }
    else {
      int i, n = gtk_gs_get_page_count(gs);
      pages = g_new0(gint, n);
      for(i = 0; i < n; i++)
        pages[i] = TRUE;
      free_pages = TRUE;
    }
  }

  sink = gtk_gs_doc_sink_new();

  if(GTK_GS_IS_PDF(gs)) {
    gchar *tmpn = g_strconcat(g_get_tmp_dir(), "/ggvXXXXXX", NULL);
    gchar *cmd, *fname;
    int tmpfd;

    if((tmpfd = mkstemp(tmpn)) < 0) {
      g_free(tmpn);
      return NULL;
    }
    close(tmpfd);
    fname = ggv_quote_filename(gs->gs_filename_unc ?
                               gs->gs_filename_unc : gs->gs_filename);
    cmd = g_strdup_printf(gtk_gs_defaults_get_convert_pdf_cmd(), tmpn, fname);
    g_free(fname);
    if((system(cmd) == 0) && ggv_file_readable(tmpn)) {
      GtkWidget *tmp_gs;
      tmp_gs = gtk_gs_new_from_file(NULL, NULL, tmpn);
      if(NULL != tmp_gs) {
        if(GTK_GS(tmp_gs)->loaded)
          pscopydoc(sink, tmpn, GTK_GS(tmp_gs)->doc, pages);
        gtk_widget_destroy(tmp_gs);
      }
    }
    g_free(cmd);
    g_free(tmpn);
  }
  else {
    /* Use uncompressed file if necessary */
    pscopydoc(sink, GTK_GS_GET_PS_FILE(gs), gs->doc, pages);
  }
  if(free_pages)
    g_free(pages);
  doc = gtk_gs_doc_sink_get_buffer(sink);
  gtk_gs_doc_sink_free(sink);
  return doc;
}

void
gtk_gs_set_adjustments(GtkGS * gs, GtkAdjustment * hadj, GtkAdjustment * vadj)
{
  g_return_if_fail(gs != NULL);
  g_return_if_fail(GTK_IS_GS(gs));
  if(hadj)
    g_return_if_fail(GTK_IS_ADJUSTMENT(hadj));
  else
    hadj = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 1.0, 0.0, 0.0, 1.0));
  if(vadj)
    g_return_if_fail(GTK_IS_ADJUSTMENT(vadj));
  else
    vadj = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 1.0, 0.0, 0.0, 1.0));

  if(gs->hadj && (gs->hadj != hadj)) {
    g_signal_handlers_disconnect_matched(G_OBJECT(gs->hadj),
                                         G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL, NULL, gs);
    gtk_object_unref(GTK_OBJECT(gs->hadj));
  }
  if(gs->vadj && (gs->vadj != vadj)) {
    g_signal_handlers_disconnect_matched(G_OBJECT(gs->vadj),
                                         G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL, NULL, gs);
    gtk_object_unref(GTK_OBJECT(gs->vadj));
  }
  if(gs->hadj != hadj) {
    hadj->lower = 0.0;
    hadj->upper = 1.0;
    hadj->value = 0.0;
    hadj->page_size = 1.0;
    hadj->page_increment = 1.0;
    gs->hadj = hadj;
    gtk_object_ref(GTK_OBJECT(gs->hadj));
    gtk_object_sink(GTK_OBJECT(gs->hadj));

    g_signal_connect(G_OBJECT(hadj), "value_changed",
                     G_CALLBACK(gtk_gs_value_adjustment_changed),
                     (gpointer) gs);
  }
  if(gs->vadj != vadj) {
    vadj->lower = 0.0;
    vadj->upper = 1.0;
    vadj->value = 0.0;
    vadj->page_size = 1.0;
    vadj->page_increment = 1.0;
    gs->vadj = vadj;
    gtk_object_ref(GTK_OBJECT(gs->vadj));
    gtk_object_sink(GTK_OBJECT(gs->vadj));

    g_signal_connect(G_OBJECT(vadj), "value_changed",
                     G_CALLBACK(gtk_gs_value_adjustment_changed),
                     (gpointer) gs);
  }
  if(GTK_WIDGET_REALIZED(gs))
    gtk_gs_munge_adjustments(gs);
}


void
gtk_gs_set_scroll_step(GtkGS * gs, gfloat scroll_step)
{
  gs->scroll_step = scroll_step;
}

gfloat
gtk_gs_get_scroll_step(GtkGS * gs)
{
  return gs->scroll_step;
}

void
gtk_gs_set_zoom_mode(GtkGS * gs, GtkGSZoomMode zoom_mode)
{
  if(zoom_mode != gs->zoom_mode) {
    gs->zoom_mode = zoom_mode;
    gtk_gs_set_zoom(gs, 1.0);
  }
}

GtkGSZoomMode
gtk_gs_get_zoom_mode(GtkGS * gs)
{
  return gs->zoom_mode;
}

void
gtk_gs_set_available_size(GtkGS * gs, guint avail_w, guint avail_h)
{
  gs->avail_w = avail_w;
  gs->avail_h = avail_h;
  if(gs->zoom_mode != GTK_GS_ZOOM_ABSOLUTE) {
    gtk_gs_set_zoom(gs, 0.0);
  }
}
