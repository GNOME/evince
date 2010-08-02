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

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#ifndef EV_DOCUMENT_FORMS_H
#define EV_DOCUMENT_FORMS_H

#include <glib-object.h>

#include "ev-document.h"
#include "ev-form-field.h"
#include "ev-mapping-list.h"

G_BEGIN_DECLS

#define EV_TYPE_DOCUMENT_FORMS	          (ev_document_forms_get_type ())
#define EV_DOCUMENT_FORMS(o)	          (G_TYPE_CHECK_INSTANCE_CAST ((o), EV_TYPE_DOCUMENT_FORMS, EvDocumentForms))
#define EV_DOCUMENT_FORMS_IFACE(k)	  (G_TYPE_CHECK_CLASS_CAST((k), EV_TYPE_DOCUMENT_FORMS, EvDocumentFormsInterface))
#define EV_IS_DOCUMENT_FORMS(o)	          (G_TYPE_CHECK_INSTANCE_TYPE ((o), EV_TYPE_DOCUMENT_FORMS))
#define EV_IS_DOCUMENT_FORMS_IFACE(k)     (G_TYPE_CHECK_CLASS_TYPE ((k), EV_TYPE_DOCUMENT_FORMS))
#define EV_DOCUMENT_FORMS_GET_IFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EV_TYPE_DOCUMENT_FORMS, EvDocumentFormsInterface))

typedef struct _EvDocumentForms          EvDocumentForms;
typedef struct _EvDocumentFormsInterface EvDocumentFormsInterface;

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
};

GType          ev_document_forms_get_type                           (void) G_GNUC_CONST;
EvMappingList *ev_document_forms_get_form_fields                    (EvDocumentForms   *document_forms,
								     EvPage            *page);
gboolean       ev_document_forms_document_is_modified               (EvDocumentForms   *document_forms);

gchar 	      *ev_document_forms_form_field_text_get_text           (EvDocumentForms   *document_forms,
								     EvFormField       *field);
void 	       ev_document_forms_form_field_text_set_text           (EvDocumentForms   *document_forms,
								     EvFormField       *field,
								     const gchar       *text);

gboolean       ev_document_forms_form_field_button_get_state        (EvDocumentForms   *document_forms,
								     EvFormField       *field);
void 	       ev_document_forms_form_field_button_set_state        (EvDocumentForms   *document_forms,
								     EvFormField       *field,
								     gboolean           state);

gchar         *ev_document_forms_form_field_choice_get_item         (EvDocumentForms   *document_forms,
								     EvFormField       *field,
								     gint               index);
gint 	       ev_document_forms_form_field_choice_get_n_items      (EvDocumentForms   *document_forms,
								     EvFormField       *field);
gboolean       ev_document_forms_form_field_choice_is_item_selected (EvDocumentForms   *document_forms,
								     EvFormField       *field,
								     gint               index);
void 	       ev_document_forms_form_field_choice_select_item      (EvDocumentForms   *document_forms,
								     EvFormField       *field,
								     gint               index);
void 	       ev_document_forms_form_field_choice_toggle_item      (EvDocumentForms   *document_forms,
								     EvFormField       *field,
								     gint               index);
void 	       ev_document_forms_form_field_choice_unselect_all     (EvDocumentForms   *document_forms,
								     EvFormField       *field);
void 	       ev_document_forms_form_field_choice_set_text         (EvDocumentForms   *document_forms,
								     EvFormField       *field,
								     const gchar       *text);
gchar         *ev_document_forms_form_field_choice_get_text         (EvDocumentForms   *document_forms,
								     EvFormField       *field);

G_END_DECLS

#endif /* EV_DOCUMENT_FORMS_H */
