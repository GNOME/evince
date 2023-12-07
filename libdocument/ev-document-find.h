/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/*
 *  Copyright (C) 2004 Red Hat, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#include <glib-object.h>
#include <glib.h>

#include "ev-macros.h"
#include "ev-document.h"

G_BEGIN_DECLS

#define EV_TYPE_DOCUMENT_FIND	    (ev_document_find_get_type ())

EV_PUBLIC
G_DECLARE_INTERFACE (EvDocumentFind, ev_document_find, EV, DOCUMENT_FIND, GObject)

typedef struct _EvFindRectangle         EvFindRectangle;

#define EV_TYPE_FIND_RECTANGLE (ev_find_rectangle_get_type ())
struct _EvFindRectangle
{
	gdouble x1;
	gdouble y1;
	gdouble x2;
	gdouble y2;
	gboolean next_line; /* the boolean from poppler_rectangle_find_get_match_continued() */
	gboolean after_hyphen; /* the boolean from poppler_rectangle_find_get_ignored_hyphen() */
	void (*_ev_reserved1) (void);
	void (*_ev_reserved2) (void);
};

EV_PUBLIC
GType            ev_find_rectangle_get_type (void) G_GNUC_CONST;
EV_PUBLIC
EvFindRectangle *ev_find_rectangle_new      (void);
EV_PUBLIC
EvFindRectangle *ev_find_rectangle_copy     (EvFindRectangle *ev_find_rect);
EV_PUBLIC
void             ev_find_rectangle_free     (EvFindRectangle *ev_find_rect);

typedef enum {
	EV_FIND_DEFAULT          = 0,
	EV_FIND_CASE_SENSITIVE   = 1 << 0,
	EV_FIND_WHOLE_WORDS_ONLY = 1 << 1
} EvFindOptions;

struct _EvDocumentFindInterface
{
	GTypeInterface base_iface;

        /* Methods */
	EvFindOptions (*get_supported_options)   (EvDocumentFind *document_find);
	GList        *(* find_text)              (EvDocumentFind *document_find,
						  EvPage         *page,
						  const gchar    *text,
						  EvFindOptions   options);
};

EV_PUBLIC
EvFindOptions ev_document_find_get_supported_options  (EvDocumentFind *document_find);
EV_PUBLIC
GList        *ev_document_find_find_text              (EvDocumentFind *document_find,
						       EvPage         *page,
						       const gchar    *text,
						       EvFindOptions   options);

G_END_DECLS
