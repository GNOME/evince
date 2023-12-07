/* ev-document-forms.h
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

#pragma once

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#include <glib-object.h>

#include "ev-macros.h"
#include "ev-document.h"
#include "ev-form-field.h"
#include "ev-mapping-list.h"

G_BEGIN_DECLS

#define EV_TYPE_DOCUMENT_FORMS	          (ev_document_forms_get_type ())

EV_PUBLIC
G_DECLARE_INTERFACE (EvDocumentForms, ev_document_forms, EV, DOCUMENT_FORMS, GObject)

struct _EvDocumentFormsInterface
{
	GTypeInterface base_iface;

	/* Methods  */
	EvMappingList *(* get_form_fields)                    (EvDocumentForms   *document_forms,
							       EvPage            *page);
	gboolean       (* document_is_modified)               (EvDocumentForms   *document_forms);
	gchar         *(* form_field_text_get_text)           (EvDocumentForms   *document_forms,
							       EvFormField       *field);
	void           (* form_field_text_set_text)           (EvDocumentForms   *document_forms,
							       EvFormField       *field,
							       const gchar       *text);
	gboolean       (* form_field_button_get_state)        (EvDocumentForms   *document_forms,
							       EvFormField       *field);
	void           (* form_field_button_set_state)        (EvDocumentForms   *document_forms,
							       EvFormField       *field,
							       gboolean           state);
	gchar         *(* form_field_choice_get_item)         (EvDocumentForms   *document_forms,
							       EvFormField       *field,
							       gint               index);
	gint           (* form_field_choice_get_n_items)      (EvDocumentForms   *document_forms,
							       EvFormField       *field);
	gboolean       (* form_field_choice_is_item_selected) (EvDocumentForms   *document_forms,
							       EvFormField       *field,
							       gint               index);
	void           (* form_field_choice_select_item)      (EvDocumentForms   *document_forms,
							       EvFormField       *field,
							       gint               index);
	void           (* form_field_choice_toggle_item)      (EvDocumentForms   *document_forms,
							       EvFormField       *field,
							       gint               index);
	void           (* form_field_choice_unselect_all)     (EvDocumentForms   *document_forms,
							       EvFormField       *field);
	void           (* form_field_choice_set_text)         (EvDocumentForms   *document_forms,
							       EvFormField       *field,
							       const gchar       *text);
	gchar         *(* form_field_choice_get_text)         (EvDocumentForms   *document_forms,
							       EvFormField       *field);
	void           (* reset_form)                         (EvDocumentForms   *document_forms,
							       EvLinkAction      *action);
};

EV_PUBLIC
EvMappingList *ev_document_forms_get_form_fields                    (EvDocumentForms   *document_forms,
								     EvPage            *page);
EV_PUBLIC
gboolean       ev_document_forms_document_is_modified               (EvDocumentForms   *document_forms);

EV_PUBLIC
gchar 	      *ev_document_forms_form_field_text_get_text           (EvDocumentForms   *document_forms,
								     EvFormField       *field);
EV_PUBLIC
void 	       ev_document_forms_form_field_text_set_text           (EvDocumentForms   *document_forms,
								     EvFormField       *field,
								     const gchar       *text);

EV_PUBLIC
gboolean       ev_document_forms_form_field_button_get_state        (EvDocumentForms   *document_forms,
								     EvFormField       *field);
EV_PUBLIC
void 	       ev_document_forms_form_field_button_set_state        (EvDocumentForms   *document_forms,
								     EvFormField       *field,
								     gboolean           state);

EV_PUBLIC
gchar         *ev_document_forms_form_field_choice_get_item         (EvDocumentForms   *document_forms,
								     EvFormField       *field,
								     gint               index);
EV_PUBLIC
gint 	       ev_document_forms_form_field_choice_get_n_items      (EvDocumentForms   *document_forms,
								     EvFormField       *field);
EV_PUBLIC
gboolean       ev_document_forms_form_field_choice_is_item_selected (EvDocumentForms   *document_forms,
								     EvFormField       *field,
								     gint               index);
EV_PUBLIC
void 	       ev_document_forms_form_field_choice_select_item      (EvDocumentForms   *document_forms,
								     EvFormField       *field,
								     gint               index);
EV_PUBLIC
void 	       ev_document_forms_form_field_choice_toggle_item      (EvDocumentForms   *document_forms,
								     EvFormField       *field,
								     gint               index);
EV_PUBLIC
void 	       ev_document_forms_form_field_choice_unselect_all     (EvDocumentForms   *document_forms,
								     EvFormField       *field);
EV_PUBLIC
void 	       ev_document_forms_form_field_choice_set_text         (EvDocumentForms   *document_forms,
								     EvFormField       *field,
								     const gchar       *text);
EV_PUBLIC
gchar         *ev_document_forms_form_field_choice_get_text         (EvDocumentForms   *document_forms,
								     EvFormField       *field);
EV_PUBLIC
void           ev_document_forms_reset_form                         (EvDocumentForms   *document_forms,
								     EvLinkAction      *action);

G_END_DECLS
