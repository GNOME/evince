/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2014 Igalia
 * Author: Joanmarie Diggs <jdiggs@igalia.com>
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

#include "ev-form-field-accessible.h"
#include "ev-view-private.h"

struct _EvFormFieldAccessiblePrivate {
	EvPageAccessible *page;
	EvFormField      *form_field;
	EvRectangle       area;
};

static void ev_form_field_accessible_component_iface_init (AtkComponentIface *iface);

G_DEFINE_TYPE_WITH_CODE (EvFormFieldAccessible, ev_form_field_accessible, ATK_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (ATK_TYPE_COMPONENT, ev_form_field_accessible_component_iface_init))

static void
ev_form_field_accessible_get_extents (AtkComponent *atk_component,
				      gint         *x,
				      gint         *y,
				      gint         *width,
				      gint         *height,
				      AtkCoordType coord_type)
{
	EvFormFieldAccessible *self;
	EvViewAccessible *view_accessible;
	gint page;
	EvRectangle atk_rect;

	self = EV_FORM_FIELD_ACCESSIBLE (atk_component);
	view_accessible = ev_page_accessible_get_view_accessible (self->priv->page);
	page = ev_page_accessible_get_page (self->priv->page);
	_transform_doc_rect_to_atk_rect (view_accessible, page, &self->priv->area, &atk_rect, coord_type);
	*x = atk_rect.x1;
	*y = atk_rect.y1;
	*width = atk_rect.x2 - atk_rect.x1;
	*height = atk_rect.y2 - atk_rect.y1;
}

static gboolean
ev_form_field_accessible_grab_focus (AtkComponent *atk_component)
{
	EvFormFieldAccessible *self;
	EvView *view;

	self = EV_FORM_FIELD_ACCESSIBLE (atk_component);
	view = ev_page_accessible_get_view (self->priv->page);
	_ev_view_focus_form_field (view, self->priv->form_field);

	return TRUE;
}

static void
ev_form_field_accessible_component_iface_init (AtkComponentIface *iface)
{
	iface->get_extents = ev_form_field_accessible_get_extents;
	iface->grab_focus = ev_form_field_accessible_grab_focus;
}

static AtkObject *
ev_form_field_accessible_get_parent (AtkObject *atk_object)
{
	EvFormFieldAccessiblePrivate *priv = EV_FORM_FIELD_ACCESSIBLE (atk_object)->priv;

	return ATK_OBJECT (priv->page);
}

static AtkRole
ev_form_field_accessible_get_role (AtkObject *atk_object)
{
	EvFormField *ev_form_field;

	ev_form_field = EV_FORM_FIELD_ACCESSIBLE (atk_object)->priv->form_field;
	if (EV_IS_FORM_FIELD_BUTTON (ev_form_field)) {
		EvFormFieldButton *field_button = EV_FORM_FIELD_BUTTON (ev_form_field);

		switch (field_button->type) {
		case EV_FORM_FIELD_BUTTON_CHECK:
			return ATK_ROLE_CHECK_BOX;
		case EV_FORM_FIELD_BUTTON_RADIO:
			return ATK_ROLE_RADIO_BUTTON;
		case EV_FORM_FIELD_BUTTON_PUSH:
			return ATK_ROLE_PUSH_BUTTON;
		default:
			return ATK_ROLE_UNKNOWN;
		}
	}

	if (EV_IS_FORM_FIELD_CHOICE (ev_form_field)) {
		EvFormFieldChoice *field_choice = EV_FORM_FIELD_CHOICE (ev_form_field);

		switch (field_choice->type) {
		case EV_FORM_FIELD_CHOICE_COMBO:
			return ATK_ROLE_COMBO_BOX;
		case EV_FORM_FIELD_CHOICE_LIST:
			return ATK_ROLE_LIST_BOX;
		default:
			return ATK_ROLE_UNKNOWN;
		}
	}

	if (EV_IS_FORM_FIELD_TEXT (ev_form_field)) {
		EvFormFieldText *field_text = EV_FORM_FIELD_TEXT (ev_form_field);

		if (field_text->is_password)
			return ATK_ROLE_PASSWORD_TEXT;
		else if (field_text->type == EV_FORM_FIELD_TEXT_MULTILINE)
			return ATK_ROLE_TEXT;
		else
			return ATK_ROLE_ENTRY;
	}

	return ATK_ROLE_UNKNOWN;
}

static AtkStateSet *
ev_form_field_accessible_ref_state_set (AtkObject *atk_object)
{
	AtkStateSet *state_set;
	AtkStateSet *copy_set;
	AtkStateSet *page_accessible_state_set;
	EvFormFieldAccessible *self;
	EvFormField *ev_form_field;
	EvViewAccessible *view_accessible;
	gint page;

	self = EV_FORM_FIELD_ACCESSIBLE (atk_object);
	state_set = ATK_OBJECT_CLASS (ev_form_field_accessible_parent_class)->ref_state_set (atk_object);
	atk_state_set_clear_states (state_set);

	page_accessible_state_set = atk_object_ref_state_set (ATK_OBJECT (self->priv->page));
	copy_set = atk_state_set_or_sets (state_set, page_accessible_state_set);

	view_accessible = ev_page_accessible_get_view_accessible (self->priv->page);
	page = ev_page_accessible_get_page (self->priv->page);
	if (!ev_view_accessible_is_doc_rect_showing (view_accessible, page, &self->priv->area))
		atk_state_set_remove_state (copy_set, ATK_STATE_SHOWING);

	ev_form_field = EV_FORM_FIELD_ACCESSIBLE (atk_object)->priv->form_field;
	if (EV_IS_FORM_FIELD_BUTTON (ev_form_field)) {
		EvFormFieldButton *field_button = EV_FORM_FIELD_BUTTON (ev_form_field);

		if (field_button->state) {
			if (field_button->type == EV_FORM_FIELD_BUTTON_PUSH)
				atk_state_set_add_state (copy_set, ATK_STATE_PRESSED);
			else
				atk_state_set_add_state (copy_set, ATK_STATE_CHECKED);
		}
	}

	else if (EV_IS_FORM_FIELD_CHOICE (ev_form_field)) {
		EvFormFieldChoice *field_choice = EV_FORM_FIELD_CHOICE (ev_form_field);

		if (field_choice->is_editable && !ev_form_field->is_read_only)
			atk_state_set_add_state (copy_set, ATK_STATE_EDITABLE);
		if (field_choice->multi_select)
			atk_state_set_add_state (copy_set, ATK_STATE_MULTISELECTABLE);
	}

	else if (EV_IS_FORM_FIELD_TEXT (ev_form_field)) {
		EvFormFieldText *field_text = EV_FORM_FIELD_TEXT (ev_form_field);

		if (!ev_form_field->is_read_only)
			atk_state_set_add_state (copy_set, ATK_STATE_EDITABLE);
		if (field_text->type == EV_FORM_FIELD_TEXT_NORMAL)
			atk_state_set_add_state (copy_set, ATK_STATE_SINGLE_LINE);
		else if (field_text->type == EV_FORM_FIELD_TEXT_MULTILINE)
			atk_state_set_add_state (copy_set, ATK_STATE_MULTI_LINE);
	}

	g_object_unref (state_set);
	g_object_unref (page_accessible_state_set);

	return copy_set;
}

static void
ev_form_field_accessible_finalize (GObject *object)
{
	EvFormFieldAccessiblePrivate *priv = EV_FORM_FIELD_ACCESSIBLE (object)->priv;

	g_object_unref (priv->form_field);

	G_OBJECT_CLASS (ev_form_field_accessible_parent_class)->finalize (object);
}

static void
ev_form_field_accessible_class_init (EvFormFieldAccessibleClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	AtkObjectClass *atk_class = ATK_OBJECT_CLASS (klass);

	object_class->finalize = ev_form_field_accessible_finalize;
	atk_class->get_parent = ev_form_field_accessible_get_parent;
	atk_class->get_role = ev_form_field_accessible_get_role;
	atk_class->ref_state_set = ev_form_field_accessible_ref_state_set;

	g_type_class_add_private (klass, sizeof (EvFormFieldAccessiblePrivate));
}

static void
ev_form_field_accessible_init (EvFormFieldAccessible *accessible)
{
	accessible->priv = G_TYPE_INSTANCE_GET_PRIVATE (accessible, EV_TYPE_FORM_FIELD_ACCESSIBLE, EvFormFieldAccessiblePrivate);
}

EvFormFieldAccessible*
ev_form_field_accessible_new (EvPageAccessible *page,
			      EvFormField      *form_field,
			      EvRectangle      *area)
{
	EvFormFieldAccessible *atk_form_field;

	atk_form_field = g_object_new (EV_TYPE_FORM_FIELD_ACCESSIBLE, NULL);
	atk_form_field->priv->page = page;
	atk_form_field->priv->form_field = g_object_ref (form_field);
	atk_form_field->priv->area = *area;

	return EV_FORM_FIELD_ACCESSIBLE (atk_form_field);
}
