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

#include "gstypes.h"

G_BEGIN_DECLS

#define GTK_GS_TYPE         (gtk_gs_get_type())
#define GTK_GS(obj)         GTK_CHECK_CAST (obj, gtk_gs_get_type (), GtkGS)
#define GTK_GS_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, gtk_gs_get_type (), GtkGSClass)
#define GTK_IS_GS(obj)      GTK_CHECK_TYPE (obj, gtk_gs_get_type())

typedef struct _GtkGS GtkGS;
typedef struct _GtkGSClass GtkGSClass;

struct _GtkGS {
  GObject object;
  GdkWindow *pstarget;          /* the window passed to gv
                                 * it is a child of widget...
                                 */
  GdkGC *psgc;

  GtkGSZoomMode zoom_mode;

  GdkPixmap *bpixmap;           /* Backing pixmap */

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
  GObjectClass parent_class;

  GdkAtom gs_atom;
  GdkAtom next_atom;
  GdkAtom page_atom;
  GdkAtom string_atom;
};

GType gtk_gs_get_type(void);

G_END_DECLS

#endif /* __GTK_GS_H__ */
