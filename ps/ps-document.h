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

#ifndef __PS_DOCUMENT_H__
#define __PS_DOCUMENT_H__

#include <sys/types.h>

#include "ev-document.h"
#include "ps.h"
#include "gstypes.h"

G_BEGIN_DECLS

#define PS_TYPE_DOCUMENT         (ps_document_get_type())
#define PS_DOCUMENT(obj)         GTK_CHECK_CAST (obj, ps_document_get_type (), PSDocument)
#define PS_DOCUMENT_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, ps_document_get_type (), PSDocumentClass)
#define PS_IS_DOCUMENT(obj)      GTK_CHECK_TYPE (obj, ps_document_get_type())

typedef struct _PSDocument PSDocument;
typedef struct _PSDocumentClass PSDocumentClass;

struct _PSDocument {
  GObject object;

  GdkWindow *pstarget;
  GdkPixmap *bpixmap;
  long message_window;          /* Used by ghostview to receive messages from app */

  pid_t interpreter_pid;        /* PID of interpreter, -1 if none  */
  int interpreter_input;        /* stdin of interpreter            */
  int interpreter_output;       /* stdout of interpreter           */
  int interpreter_err;          /* stderr of interpreter           */
  guint interpreter_input_id;
  guint interpreter_output_id;
  guint interpreter_error_id;

  gboolean busy;                /* Is gs busy drawing? */
  gboolean structured_doc;

  struct record_list *ps_input;
  gchar *input_buffer_ptr;
  guint bytes_left;
  guint buffer_bytes_left;

  FILE *gs_psfile;              /* the currently loaded FILE */
  gchar *gs_filename;           /* the currently loaded filename */
  gchar *gs_filename_unc;       /* Uncompressed file */
  gchar *input_buffer;
  gboolean send_filename_to_gs; /* True if gs should read from file directly */
  gboolean reading_from_pipe;   /* True if ggv is reading input from pipe */
  struct document *doc;
  
  int *ps_export_pagelist;
  char *ps_export_filename;

  const gchar *gs_status;       /* PSDocument status */
};

struct _PSDocumentClass {
  GObjectClass parent_class;

  GdkAtom gs_atom;
  GdkAtom next_atom;
  GdkAtom page_atom;
  GdkAtom string_atom;
};

GType ps_document_get_type(void);

G_END_DECLS

#endif /* __PS_DOCUMENT_H__ */
