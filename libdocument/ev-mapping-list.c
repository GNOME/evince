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

/**
 * SECTION: ev-mapping-list
 * @short_description: a refcounted list of #EvMappings.
 *
 * Since: 3.8
 */
struct _EvMappingList {
	guint          page;
	GList         *list;
	GDestroyNotify data_destroy_func;
	volatile gint  ref_count;
};

G_DEFINE_BOXED_TYPE (EvMappingList, ev_mapping_list, ev_mapping_list_ref, ev_mapping_list_unref)

/**
 * ev_mapping_list_find:
 * @mapping_list: an #EvMappingList
 * @data: mapping data to find
 *
 * Returns: (transfer none): an #EvMapping
 */
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

/**
 * ev_mapping_list_find_custom:
 * @mapping_list: an #EvMappingList
 * @data: mapping data to find
 * @func: (scope call): function to use for equality check
 *
 * Returns: (transfer none): an #EvMapping
 */
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

/**
 * ev_mapping_list_nth:
 * @mapping_list: an #EvMappingList
 * @n: the position to retrieve
 *
 * Returns: (transfer none): the #Evmapping at position @n in @mapping_list
 */
EvMapping *
ev_mapping_list_nth (EvMappingList *mapping_list,
                     guint          n)
{
        g_return_val_if_fail (mapping_list != NULL, NULL);

        return (EvMapping *)g_list_nth_data (mapping_list->list, n);
}

static int
cmp_mapping_area_size (EvMapping *a,
		       EvMapping *b)
{
	gdouble wa, ha, wb, hb;

	wa = a->area.x2 - a->area.x1;
	ha = a->area.y2 - a->area.y1;
	wb = b->area.x2 - b->area.x1;
	hb = b->area.y2 - b->area.y1;

	if (wa == wb) {
		if (ha == hb)
			return 0;
		return (ha < hb) ? -1 : 1;
	}

	if (ha == hb) {
		return (wa < wb) ? -1 : 1;
	}

	return (wa * ha < wb * hb) ? -1 : 1;
}

/**
 * ev_mapping_list_get:
 * @mapping_list: an #EvMappingList
 * @x: X coordinate
 * @y: Y coordinate
 *
 * Returns: (transfer none): the #EvMapping in the list at coordinates (x, y)
 *
 * Since: 3.12
 */
EvMapping *
ev_mapping_list_get (EvMappingList *mapping_list,
		     gdouble        x,
		     gdouble        y)
{
	GList *list;
	EvMapping *found = NULL;

	g_return_val_if_fail (mapping_list != NULL, NULL);

	for (list = mapping_list->list; list; list = list->next) {
		EvMapping *mapping = list->data;

		if ((x >= mapping->area.x1) &&
		    (y >= mapping->area.y1) &&
		    (x <= mapping->area.x2) &&
		    (y <= mapping->area.y2)) {

			/* In case of only one match choose that. Otherwise
			 * compare the area of the bounding boxes and return the
			 * smallest element */
			if(found == NULL || cmp_mapping_area_size (mapping, found) < 0)
				found = mapping;
		}
	}

	return found;
}

/**
 * ev_mapping_list_get_data:
 * @mapping_list: an #EvMappingList
 * @x: X coordinate
 * @y: Y coordinate
 *
 * Returns: (transfer none): the data of a mapping in the list at coordinates (x, y)
 */
gpointer
ev_mapping_list_get_data (EvMappingList *mapping_list,
			  gdouble        x,
			  gdouble        y)
{
	EvMapping *mapping;

	mapping = ev_mapping_list_get (mapping_list, x, y);
	if (mapping)
		return mapping->data;

	return NULL;
}

/**
 * ev_mapping_list_get_list:
 * @mapping_list: an #EvMappingList
 *
 * Returns: (transfer none) (element-type EvMapping): the data for this mapping list
 */
GList *
ev_mapping_list_get_list (EvMappingList *mapping_list)
{
	return mapping_list ? mapping_list->list : NULL;
}

/**
 * ev_mapping_list_remove:
 * @mapping_list: an #EvMappingList
 * @mapping: #EvMapping to remove
 *
 * Since: 3.14
 */
void
ev_mapping_list_remove (EvMappingList *mapping_list,
			EvMapping     *mapping)
{
	mapping_list->list = g_list_remove (mapping_list->list, mapping);
        mapping_list->data_destroy_func (mapping->data);
        g_free (mapping);
}

guint
ev_mapping_list_get_page (EvMappingList *mapping_list)
{
	return mapping_list->page;
}

guint
ev_mapping_list_length (EvMappingList *mapping_list)
{
        g_return_val_if_fail (mapping_list != NULL, 0);

        return g_list_length (mapping_list->list);
}

/**
 * ev_mapping_list_new:
 * @page: page index for this mapping
 * @list: (element-type EvMapping): a #GList of data for the page
 * @data_destroy_func: function to free a list element
 *
 * Returns: an #EvMappingList
 */
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
