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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include "ev-document-forms.h"

GType
ev_document_forms_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		const GTypeInfo our_info = {
			sizeof (EvDocumentFormsIface),
			NULL,
			NULL,
		};

		type = g_type_register_static (G_TYPE_INTERFACE,
					       "EvDocumentForms",
					       &our_info, (GTypeFlags)0);
	}

	return type;
}

GList *
ev_document_forms_get_form_fields (EvDocumentForms *document_forms,
				   gint             page)
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
