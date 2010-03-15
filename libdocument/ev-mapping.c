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

#include "ev-mapping.h"

EvMapping *
ev_mapping_list_find (GList         *mapping_list,
		      gconstpointer  data)
{
	GList *list;

	for (list = mapping_list; list; list = list->next) {
		EvMapping *mapping = list->data;

		if (mapping->data == data)
			return mapping;
	}

	return NULL;
}

EvMapping *
ev_mapping_list_find_custom (GList         *mapping_list,
			     gconstpointer  data,
			     GCompareFunc   func)
{
	GList *list;

	for (list = mapping_list; list; list = list->next) {
		EvMapping *mapping = list->data;

		if (!func (mapping->data, data))
			return mapping;
	}

	return NULL;
}

gpointer
ev_mapping_list_get_data (GList   *mapping_list,
			  gdouble  x,
			  gdouble  y)
{
	GList *list;

	for (list = mapping_list; list; list = list->next) {
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

static void
mapping_list_free_foreach (EvMapping     *mapping,
			   GDestroyNotify destroy_func)
{
	destroy_func (mapping->data);
	g_free (mapping);
}

void
ev_mapping_list_free (GList          *mapping_list,
		      GDestroyNotify  destroy_func)
{
	g_list_foreach (mapping_list,
			(GFunc)mapping_list_free_foreach,
			destroy_func);
	g_list_free (mapping_list);
}
