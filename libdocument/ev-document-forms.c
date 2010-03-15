/* ev-document-forms.c
 *  this file is part of evince, a gnome document viewer
 * 
 * Copyright (C) 2007 Carlos Garcia Campos <carlosgc@gnome.org>
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

#include <config.h>
#include "ev-document-forms.h"

EV_DEFINE_INTERFACE (EvDocumentForms, ev_document_forms, 0)

static void
ev_document_forms_class_init (EvDocumentFormsIface *klass)
{
}

GList *
ev_document_forms_get_form_fields (EvDocumentForms *document_forms,
				   EvPage          *page)
{
	EvDocumentFormsIface *iface = EV_DOCUMENT_FORMS_GET_IFACE (document_forms);

	return iface->get_form_fields (document_forms, page);
}

gchar *
ev_document_forms_form_field_text_get_text (EvDocumentForms *document_forms, 
					    EvFormField     *field)
{
	EvDocumentFormsIface *iface = EV_DOCUMENT_FORMS_GET_IFACE (document_forms);

	return iface->form_field_text_get_text (document_forms, field);
}

void
ev_document_forms_form_field_text_set_text (EvDocumentForms *document_forms, 
					    EvFormField     *field, 
					    const gchar     *text)
{
	EvDocumentFormsIface *iface = EV_DOCUMENT_FORMS_GET_IFACE (document_forms);

	iface->form_field_text_set_text (document_forms, field, text);
}

gboolean
ev_document_forms_form_field_button_get_state (EvDocumentForms   *document_forms,
					       EvFormField       *field)
{
	EvDocumentFormsIface *iface = EV_DOCUMENT_FORMS_GET_IFACE (document_forms);

	return iface->form_field_button_get_state (document_forms, field);
}

void
ev_document_forms_form_field_button_set_state (EvDocumentForms   *document_forms, 
					       EvFormField       *field, 
					       gboolean           state)
{
	EvDocumentFormsIface *iface = EV_DOCUMENT_FORMS_GET_IFACE (document_forms);

	iface->form_field_button_set_state (document_forms, field, state);
}

gchar *
ev_document_forms_form_field_choice_get_item (EvDocumentForms   *document_forms, 
					      EvFormField       *field, 
					      gint               index)
{
	EvDocumentFormsIface *iface = EV_DOCUMENT_FORMS_GET_IFACE (document_forms);

	return iface->form_field_choice_get_item (document_forms, field, index);
}

gint
ev_document_forms_form_field_choice_get_n_items (EvDocumentForms   *document_forms, 
						 EvFormField       *field)
{
	EvDocumentFormsIface *iface = EV_DOCUMENT_FORMS_GET_IFACE (document_forms);

	return iface->form_field_choice_get_n_items (document_forms, field);
}

gboolean
ev_document_forms_form_field_choice_is_item_selected (EvDocumentForms   *document_forms, 
						      EvFormField       *field, 
						      gint               index)
{
	EvDocumentFormsIface *iface = EV_DOCUMENT_FORMS_GET_IFACE (document_forms);

	return iface->form_field_choice_is_item_selected (document_forms, field, index);
}

void
ev_document_forms_form_field_choice_select_item (EvDocumentForms   *document_forms, 
						 EvFormField       *field, 
						 gint               index)
{
	EvDocumentFormsIface *iface = EV_DOCUMENT_FORMS_GET_IFACE (document_forms);

	iface->form_field_choice_select_item (document_forms, field, index);
}

void
ev_document_forms_form_field_choice_toggle_item (EvDocumentForms   *document_forms, 
						 EvFormField       *field, 
						 gint               index)
{
	EvDocumentFormsIface *iface = EV_DOCUMENT_FORMS_GET_IFACE (document_forms);

	iface->form_field_choice_toggle_item (document_forms, field, index);
}

void
ev_document_forms_form_field_choice_unselect_all (EvDocumentForms   *document_forms, 
						  EvFormField       *field)
{
	EvDocumentFormsIface *iface = EV_DOCUMENT_FORMS_GET_IFACE (document_forms);

	iface->form_field_choice_unselect_all (document_forms, field);
}

void
ev_document_forms_form_field_choice_set_text (EvDocumentForms   *document_forms,
					      EvFormField       *field,
					      const gchar       *text)
{
	EvDocumentFormsIface *iface = EV_DOCUMENT_FORMS_GET_IFACE (document_forms);

	iface->form_field_choice_set_text (document_forms, field, text);
}

gchar *
ev_document_forms_form_field_choice_get_text (EvDocumentForms   *document_forms,
					      EvFormField       *field)
{
	EvDocumentFormsIface *iface = EV_DOCUMENT_FORMS_GET_IFACE (document_forms);

	return iface->form_field_choice_get_text (document_forms, field);
}
