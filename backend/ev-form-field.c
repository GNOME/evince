/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2006 Julien Rebetez
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ev-form-field.h"

EvFormField *
ev_form_field_new ()
{
	EvFormField *field = g_new (EvFormField, 1);
	field->id = -1;
	field->content = NULL;

}

void
ev_form_field_mapping_free (GList *form_field_mapping)
{
	if (form_field_mapping == NULL)
		return;
	
	g_list_foreach (form_field_mapping, (GFunc) (g_free), NULL);
	g_list_free (form_field_mapping);
}


EvFormField *
ev_form_field_mapping_find (GList   *form_field_mapping,
		      gdouble  x,
		      gdouble  y)
{
	GList *list;

	for (list = form_field_mapping; list; list = list->next) {
		EvFormField *field = list->data;

		if ((x >= field->x1) &&
		    (y >= field->y1) &&
		    (x <= field->x2) &&
		    (y <= field->y2)) {
			return field;
		}
	}
	return NULL;
}

EvFormField *
ev_form_field_mapping_find_by_id (GList *form_field_mapping,
			      int id)
{
	GList *list;
	for (list = form_field_mapping; list; list = list->next) {
		EvFormField *field = list->data;

		if (id == field->id)
			return field;
	}
	return NULL;
}


