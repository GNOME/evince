/*
 * gsio.c: an IO abstraction
 *
 * Copyright 2002 - 2005 the Free Software Foundation
 *
 * Author: Jaka Mocnik  <jaka@gnu.org>
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <config.h>
#include <string.h>

#include <gsio.h>

#define CHUNK_SIZE 32768

typedef struct _GtkGSDocChunk GtkGSDocChunk;
struct _GtkGSDocChunk {
  gchar *buf, *ptr;
  guint len, max_len;
};

struct _GtkGSDocSink {
  GSList *chunks;
  GtkGSDocChunk *tail;
};

static GtkGSDocChunk *
gtk_gs_doc_chunk_new(guint size)
{
  GtkGSDocChunk *c;

  c = g_new0(GtkGSDocChunk, 1);
  if((c->buf = g_malloc(sizeof(gchar) * size)) == NULL) {
    g_free(c);
    return NULL;
  }
  c->ptr = c->buf;
  *c->ptr = '\0';
  c->max_len = size;
  c->len = 0;
  return c;
}

static void
gtk_gs_doc_chunk_free(GtkGSDocChunk * c)
{
  if(c->buf)
    g_free(c->buf);
  g_free(c);
}

GtkGSDocSink *
gtk_gs_doc_sink_new()
{
  GtkGSDocSink *sink;

  sink = g_new0(GtkGSDocSink, 1);
  return sink;
}

void
gtk_gs_doc_sink_free(GtkGSDocSink * sink)
{
  GSList *node;

  node = sink->chunks;
  while(node) {
    gtk_gs_doc_chunk_free((GtkGSDocChunk *) node->data);
    node = node->next;
  }
  g_slist_free(sink->chunks);
}

void
gtk_gs_doc_sink_write(GtkGSDocSink * sink, const gchar * buf, int len)
{
  gint real_len;

  if(sink->tail == NULL) {
    sink->tail = gtk_gs_doc_chunk_new(CHUNK_SIZE);
    sink->chunks = g_slist_append(sink->chunks, sink->tail);
  }

  real_len = MIN(sink->tail->max_len - sink->tail->len, len);
  if(real_len > 0) {
    strncpy(sink->tail->ptr, buf, real_len);
    sink->tail->ptr += real_len;
    sink->tail->len += real_len;
  }
  len -= real_len;
  if(len > 0) {
    sink->tail = NULL;
    gtk_gs_doc_sink_write(sink, buf + real_len, len);
  }
}

void
gtk_gs_doc_sink_printf_v(GtkGSDocSink * sink, const gchar * fmt, va_list ap)
{
  gint max_len, len;

  if(sink->tail == NULL) {
    sink->tail = gtk_gs_doc_chunk_new(CHUNK_SIZE);
    sink->chunks = g_slist_append(sink->chunks, sink->tail);
  }

  max_len = sink->tail->max_len - sink->tail->len;
  if(max_len > 0) {
    len = g_vsnprintf(sink->tail->ptr, max_len, fmt, ap);
    if(len >= max_len - 1) {
      /* force printf in the next chunk later on */
      max_len = 0;
      sink->tail = NULL;
    }
    else {
      sink->tail->ptr += len;
      sink->tail->len += len;
    }
  }
  if(max_len <= 0) {
    gtk_gs_doc_sink_printf(sink, fmt, ap);
  }
}

void
gtk_gs_doc_sink_printf(GtkGSDocSink * sink, const gchar * fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  gtk_gs_doc_sink_printf_v(sink, fmt, ap);
  va_end(ap);
}

gchar *
gtk_gs_doc_sink_get_buffer(GtkGSDocSink * sink)
{
  guint total;
  GSList *node;

  for(total = 0, node = sink->chunks; node; node = node->next) {
    total += ((GtkGSDocChunk *) node->data)->len;
  }
  if(total) {
    gchar *buf = g_malloc(sizeof(gchar) * (total + 1)), *ptr;
    if(!buf)
      return NULL;
    for(ptr = buf, node = sink->chunks; node; node = node->next) {
      memcpy(ptr,
             ((GtkGSDocChunk *) node->data)->buf,
             ((GtkGSDocChunk *) node->data)->len);
      ptr += ((GtkGSDocChunk *) node->data)->len;
    }
    buf[total] = '\0';
    return buf;
  }
  else
    return NULL;
}
