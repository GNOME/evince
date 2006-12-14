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

#ifndef EV_FORM_FIELD_H
#define EV_FORM_FIELD_H

#include <glib-object.h>

/*MUST be in the same order as poppler/glib/poppler.h PopplerFormFieldType !*/
typedef enum
{
	EV_FORM_FIELD_TYPE_BUTTON,
	EV_FORM_FIELD_TYPE_TEXT,
	EV_FORM_FIELD_TYPE_CHOICE,
	EV_FORM_FIELD_TYPE_SIGNATURE,
	
	EV_FORM_FIELD_TYPE_BUTTON_CHECK,
	EV_FORM_FIELD_TYPE_BUTTON_PUSH,
	EV_FORM_FIELD_TYPE_BUTTON_RADIO
} EvFormFieldType;


typedef struct _EvFormField	  EvFormField;
struct _EvFormField
{
	EvFormFieldType type;
	int id;
	gdouble x1;
	gdouble y1;
	gdouble x2;
	gdouble y2;
	//text field specific
	gchar *content;
	int length;
	//button specific
	gboolean state;
};

EvFormField *ev_form_field_new ();
void    ev_form_field_mapping_free (GList   *form_field_mapping);
EvFormField *ev_form_field_mapping_find (GList   *form_field_mapping,
			      gdouble  x,
			      gdouble  y);
EvFormField *ev_form_field_mapping_find_by_id (GList *form_field_mapping,
			      int id);

#endif /* !EV_FORM_H */

