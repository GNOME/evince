/*
 * Ghostscript widget for GTK/GNOME
 * 
 * Copyright 1998 - 2005 The Free Software Foundation
 * 
 * Authors: Jaka Mocnik, Federico Mena (Quartic), Szekeres Istvan (Pista)
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

#ifndef __GTK_GS_H__
#define __GTK_GS_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>

#include <gconf/gconf-client.h>

#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>

G_BEGIN_DECLS

#define GTK_GS(obj)         GTK_CHECK_CAST (obj, gtk_gs_get_type (), GtkGS)
#define GTK_GS_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, gtk_gs_get_type (), GtkGSClass)
#define GTK_IS_GS(obj)      GTK_CHECK_TYPE (obj, gtk_gs_get_type())

typedef struct _GtkGS GtkGS;
typedef struct _GtkGSClass GtkGSClass;
typedef struct _GtkGSPaperSize GtkGSPaperSize;

typedef enum {
  GTK_GS_ORIENTATION_NONE = -1,
  GTK_GS_ORIENTATION_PORTRAIT = 0,
  GTK_GS_ORIENTATION_SEASCAPE = 3,
  GTK_GS_ORIENTATION_UPSIDEDOWN = 2,
  GTK_GS_ORIENTATION_LANDSCAPE = 1
} GtkGSOrientation;

typedef enum {
  GTK_GS_ZOOM_ABSOLUTE = 0,
  GTK_GS_ZOOM_FIT_WIDTH = 1,
  GTK_GS_ZOOM_FIT_PAGE = 2
} GtkGSZoomMode;

struct _GtkGS {
  GtkWidget widget;             /* the main widget */
  GdkWindow *pstarget;          /* the window passed to gv
                                 * it is a child of widget...
                                 */
  GtkAdjustment *hadj, *vadj;

  GdkGC *psgc;
  gint scroll_start_x, scroll_start_y;
  gint scroll_width, scroll_height;
  gboolean show_scroll_rect;

  GtkGSZoomMode zoom_mode;

  GdkPixmap *bpixmap;           /* Backing pixmap */
  int use_bpixmap;

  long message_window;          /* Used by ghostview to receive messages from app */

  int disable_start;            /* Can the interpreter be started? */
  pid_t interpreter_pid;        /* PID of interpreter, -1 if none  */
  int interpreter_input;        /* stdin of interpreter            */
  int interpreter_output;       /* stdout of interpreter           */
  int interpreter_err;          /* stderr of interpreter           */
  guint interpreter_input_id;
  guint interpreter_output_id;
  guint interpreter_error_id;

  gint llx;
  gint lly;
  gint urx;
  gint ury;
  gint left_margin;
  gint right_margin;
  gint top_margin;
  gint bottom_margin;
  gint width;                   /* Size of window at last setup()  */
  gint height;
  gboolean busy;                /* Is gs busy drawing? */
  gboolean changed;             /* Anything changed since setup */
  gfloat zoom_factor;
  gfloat scroll_step;
  gint current_page;
  gboolean structured_doc;
  gboolean loaded;

  struct record_list *ps_input;
  gchar *input_buffer_ptr;
  guint bytes_left;
  guint buffer_bytes_left;

  FILE *gs_psfile;              /* the currently loaded FILE */
  gchar *gs_filename;           /* the currently loaded filename */
  gchar *gs_filename_dsc;       /* Used to browse PDF to PS */
  gchar *gs_filename_unc;       /* Uncompressed file */
  gchar *input_buffer;
  gint gs_scanstyle;
  gboolean send_filename_to_gs; /* True if gs should read from file directly */
  gboolean reading_from_pipe;   /* True if ggv is reading input from pipe */
  struct document *doc;

  /* User selected options... */
  gboolean antialiased;         /* Using antialiased display */
  gboolean respect_eof;         /* respect EOF comments? */
  gint default_size;
  gboolean override_size;
  gfloat xdpi, ydpi;
  gboolean override_orientation;
  gint fallback_orientation;    /* Orientation to use if override */
  gint real_orientation;        /* Real orientation from the document */

  const gchar *gs_status;       /* GtkGS status */

  guint avail_w, avail_h;
};

struct _GtkGSClass {
  GtkWidgetClass parent_class;
  GdkAtom gs_atom;
  GdkAtom gs_colors_atom;
  GdkAtom next_atom;
  GdkAtom page_atom;
  GdkAtom done_atom;
  GdkAtom string_atom;

  GConfClient *gconf_client;

  void (*interpreter_message) (GtkGS *, gchar *, gpointer);
  void (*interpreter_error) (GtkGS *, gint, gpointer);
  void (*set_scroll_adjustments) (GtkGS *, GtkAdjustment *, GtkAdjustment *);
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

struct _GtkGSPaperSize {
  gchar *name;
  gint width, height;
};

GType gtk_gs_get_type(void);

GtkWidget *gtk_gs_new_from_file(GtkAdjustment * hadj, GtkAdjustment * vadj,
                                gchar * fname);
GtkWidget *gtk_gs_new(GtkAdjustment * hadj, GtkAdjustment * vadj);
gboolean gtk_gs_load(GtkGS * gs, const gchar * fname);
void gtk_gs_reload(GtkGS * gs);

/* control functions */
void gtk_gs_center_page(GtkGS * gs);
void gtk_gs_scroll(GtkGS * gs, gint, gint);
gboolean gtk_gs_scroll_step(GtkGS * gs, GtkScrollType direction,
                            gboolean dowrap);
gboolean gtk_gs_scroll_to_edge(GtkGS * gs, GtkPositionType vertical,
                               GtkPositionType horizontal);
gboolean gtk_gs_next_page(GtkGS * gs);
gboolean gtk_gs_prev_page(GtkGS * gs);
gboolean gtk_gs_goto_page(GtkGS * gs, gint);
gint gtk_gs_enable_interpreter(GtkGS * gs);
void gtk_gs_disable_interpreter(GtkGS * gs);

gint gtk_gs_get_current_page(GtkGS * gs);
gint gtk_gs_get_page_count(GtkGS * gs);
gboolean gtk_gs_set_page_size(GtkGS * gs, gint new_pagesize, gint pageid);
gboolean gtk_gs_set_default_orientation(GtkGS * gs, gint orientation);
gint gtk_gs_get_default_orientation(GtkGS * gs);
void gtk_gs_set_default_size(GtkGS * gs, gint size);
gint gtk_gs_get_default_size(GtkGS * gs);
void gtk_gs_set_zoom(GtkGS * gs, gfloat zoom);
gfloat gtk_gs_get_zoom(GtkGS * gs);
void gtk_gs_set_scroll_step(GtkGS * gs, gfloat scroll_step);
gfloat gtk_gs_get_scroll_step(GtkGS * gs);
gfloat gtk_gs_zoom_to_fit(GtkGS * gs, gboolean fit_width);
void gtk_gs_set_center(GtkGS * gs, gfloat hval, gfloat vval);
gint gtk_gs_get_orientation(GtkGS * gs);
void gtk_gs_set_override_orientation(GtkGS * gs, gboolean f);
gboolean gtk_gs_get_override_orientation(GtkGS * gs);
void gtk_gs_set_respect_eof(GtkGS * gs, gboolean f);
gboolean gtk_gs_get_respect_eof(GtkGS * gs);
void gtk_gs_set_antialiasing(GtkGS * gs, gboolean f);
gboolean gtk_gs_get_antialiasing(GtkGS * gs);
void gtk_gs_set_override_size(GtkGS * gs, gboolean f);
gboolean gtk_gs_get_override_size(GtkGS * gs);
const gchar *gtk_gs_get_document_title(GtkGS * widget);
guint gtk_gs_get_document_numpages(GtkGS * widget);
const gchar *gtk_gs_get_document_page_label(GtkGS * widget, int page);
void gtk_gs_set_show_scroll_rect(GtkGS * gs, gboolean f);
gboolean gtk_gs_get_show_scroll_rect(GtkGS * gs);

void gtk_gs_start_scroll(GtkGS * gs);
void gtk_gs_end_scroll(GtkGS * gs);

void gtk_gs_set_zoom_mode(GtkGS * gs, GtkGSZoomMode zoom_mode);
GtkGSZoomMode gtk_gs_get_zoom_mode(GtkGS * gs);

void gtk_gs_set_available_size(GtkGS * gs, guint avail_w, guint avail_h);

/* utility functions */
gint gtk_gs_get_size_index(const gchar * string, GtkGSPaperSize * size);

gchar *gtk_gs_get_postscript(GtkGS * gs, gint * pages);

#define GTK_GS_IS_COMPRESSED(gs)       (GTK_GS(gs)->gs_filename_unc != NULL)
#define GTK_GS_GET_PS_FILE(gs)         (GTK_GS_IS_COMPRESSED(gs) ? \
                                        GTK_GS(gs)->gs_filename_unc : \
                                        GTK_GS(gs)->gs_filename)
#define GTK_GS_IS_PDF(gs)              (GTK_GS(gs)->gs_filename_dsc != NULL)
#define GTK_GS_IS_STRUCTURED_DOC(gs)   (GTK_GS(gs)->structured_doc)

G_END_DECLS

#endif /* __GTK_GS_H__ */
