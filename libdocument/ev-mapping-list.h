/* ev-mapping.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2009 Carlos Garcia Campos <carlosgc@gnome.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#ifndef EV_MAPPING_LIST_H
#define EV_MAPPING_LIST_H

#include "ev-document.h"

G_BEGIN_DECLS

typedef struct _EvMappingList EvMappingList;

EvMappingList *ev_mapping_list_new         (guint          page,
					    GList         *list,
					    GDestroyNotify data_destroy_func);
EvMappingList *ev_mapping_list_ref         (EvMappingList *mapping_list);
void           ev_mapping_list_unref       (EvMappingList *mapping_list);

guint          ev_mapping_list_get_page    (EvMappingList *mapping_list);
GList         *ev_mapping_list_get_list    (EvMappingList *mapping_list);
EvMapping     *ev_mapping_list_find        (EvMappingList *mapping_list,
					    gconstpointer  data);
EvMapping     *ev_mapping_list_find_custom (EvMappingList *mapping_list,
					    gconstpointer  data,
					    GCompareFunc   func);
gpointer       ev_mapping_list_get_data    (EvMappingList *mapping_list,
					    gdouble        x,
					    gdouble        y);

G_END_DECLS

#endif /* EV_MAPPING_LIST_H */
