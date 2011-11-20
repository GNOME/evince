/* ev-mapping.c
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

#include "ev-mapping-list.h"

struct _EvMappingList {
	guint          page;
	GList         *list;
	GDestroyNotify data_destroy_func;
	volatile gint  ref_count;
};

EvMapping *
ev_mapping_list_find (EvMappingList *mapping_list,
		      gconstpointer  data)
{
	GList *list;

	for (list = mapping_list->list; list; list = list->next) {
		EvMapping *mapping = list->data;

		if (mapping->data == data)
			return mapping;
	}

	return NULL;
}

EvMapping *
ev_mapping_list_find_custom (EvMappingList *mapping_list,
			     gconstpointer  data,
			     GCompareFunc   func)
{
	GList *list;

	for (list = mapping_list->list; list; list = list->next) {
		EvMapping *mapping = list->data;

		if (!func (mapping->data, data))
			return mapping;
	}

	return NULL;
}

gpointer
ev_mapping_list_get_data (EvMappingList *mapping_list,
			  gdouble        x,
			  gdouble        y)
{
	GList *list;

	for (list = mapping_list->list; list; list = list->next) {
		EvMapping *mapping = list->data;

		if ((x >= mapping->area.x1) &&
		    (y >= mapping->area.y1) &&
		    (x <= mapping->area.x2) &&
		    (y <= mapping->area.y2)) {
			return mapping->data;
		}
	}

	return NULL;
}

GList *
ev_mapping_list_get_list (EvMappingList *mapping_list)
{
	return mapping_list ? mapping_list->list : NULL;
}

guint
ev_mapping_list_get_page (EvMappingList *mapping_list)
{
	return mapping_list->page;
}

EvMappingList *
ev_mapping_list_new (guint          page,
		     GList         *list,
		     GDestroyNotify data_destroy_func)
{
	EvMappingList *mapping_list;

	g_return_val_if_fail (data_destroy_func != NULL, NULL);

	mapping_list = g_slice_new (EvMappingList);
	mapping_list->page = page;
	mapping_list->list = list;
	mapping_list->data_destroy_func = data_destroy_func;
	mapping_list->ref_count = 1;

	return mapping_list;
}

EvMappingList *
ev_mapping_list_ref (EvMappingList *mapping_list)
{
	g_return_val_if_fail (mapping_list != NULL, NULL);
	g_return_val_if_fail (mapping_list->ref_count > 0, mapping_list);

	g_atomic_int_add (&mapping_list->ref_count, 1);

	return mapping_list;
}

static void
mapping_list_free_foreach (EvMapping     *mapping,
			   GDestroyNotify destroy_func)
{
	destroy_func (mapping->data);
	g_free (mapping);
}

void
ev_mapping_list_unref (EvMappingList *mapping_list)
{
	g_return_if_fail (mapping_list != NULL);
	g_return_if_fail (mapping_list->ref_count > 0);

	if (g_atomic_int_add (&mapping_list->ref_count, -1) - 1 == 0) {
		g_list_foreach (mapping_list->list,
				(GFunc)mapping_list_free_foreach,
				mapping_list->data_destroy_func);
		g_list_free (mapping_list->list);
		g_slice_free (EvMappingList, mapping_list);
	}
}
