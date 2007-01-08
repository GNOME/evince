/*
 * gsio.h: an IO abstraction
 *
 * Copyright 2002 - 2005 The Free Software Foundation
 *
 * Author: jaKa Mocnik <jaka@gnu.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __GS_IO_H__
#define __GS_IO_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _GtkGSDocSink GtkGSDocSink;

GtkGSDocSink *gtk_gs_doc_sink_new(void);
void gtk_gs_doc_sink_free(GtkGSDocSink * sink);
void gtk_gs_doc_sink_write(GtkGSDocSink * sink, const gchar * buf, int len);
void gtk_gs_doc_sink_printf_v(GtkGSDocSink * sink, const gchar * fmt,
                              va_list ap);
void gtk_gs_doc_sink_printf(GtkGSDocSink * sink, const gchar * fmt, ...);
gchar *gtk_gs_doc_sink_get_buffer(GtkGSDocSink * sink);

G_END_DECLS

#endif /* __GS_IO_H__ */
