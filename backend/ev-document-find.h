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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#ifndef EV_DOCUMENT_FIND_H
#define EV_DOCUMENT_FIND_H

#include <glib-object.h>
#include <glib.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef struct
{
  int page_num;
  GdkRectangle highlight_area;
} EvFindResult;

#define EV_TYPE_DOCUMENT_FIND	    (ev_document_find_get_type ())
#define EV_DOCUMENT_FIND(o)		    (G_TYPE_CHECK_INSTANCE_CAST ((o), EV_TYPE_DOCUMENT_FIND, EvDocumentFind))
#define EV_DOCUMENT_FIND_IFACE(k)	    (G_TYPE_CHECK_CLASS_CAST((k), EV_TYPE_DOCUMENT_FIND, EvDocumentFindIface))
#define EV_IS_DOCUMENT_FIND(o)	    (G_TYPE_CHECK_INSTANCE_TYPE ((o), EV_TYPE_DOCUMENT_FIND))
#define EV_IS_DOCUMENT_FIND_IFACE(k)	    (G_TYPE_CHECK_CLASS_TYPE ((k), EV_TYPE_DOCUMENT_FIND))
#define EV_DOCUMENT_FIND_GET_IFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EV_TYPE_DOCUMENT_FIND, EvDocumentFindIface))

typedef struct _EvDocumentFind	EvDocumentFind;
typedef struct _EvDocumentFindIface	EvDocumentFindIface;

struct _EvDocumentFindIface
{
	GTypeInterface base_iface;

        /* Methods */
        
        void         (* begin)     (EvDocumentFind    *document_find,
                                    const char        *search_string,
                                    gboolean           case_sensitive);
        void         (* cancel)    (EvDocumentFind    *document_find);

        /* Signals */

        void         (* found)      (EvDocumentFind     *document_find,
                                     const EvFindResult *results,
                                     int                 n_results,
                                     double              percent_complete);
};

GType ev_document_find_get_type (void);

void ev_document_find_begin  (EvDocumentFind     *document_find,
                              const char         *search_string,
                              gboolean            case_sensitive);
void ev_document_find_cancel (EvDocumentFind     *document_find);
void ev_document_find_found  (EvDocumentFind     *document_find,
                              const EvFindResult *results,
                              int                 n_results,
                              double              percent_complete);


/* How this interface works:
 *
 * begin() begins a new search, canceling any previous search.
 * 
 * cancel() cancels a search if any, otherwise does nothing.
 * 
 * If begin() has been called and cancel() has not, then the
 * "found" signal can be emitted at any time.
 * The results given in the "found" signal are always all-inclusive,
 * that is, the array will contain all results found so far.
 * There are no guarantees about the ordering of the array,
 * or consistency of ordering between "found" signal emissions.
 *
 * When cancel() is called, "found" will always be emitted with NULL,0
 */

G_END_DECLS

#endif
