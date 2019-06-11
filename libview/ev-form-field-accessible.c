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

	gchar            *name;
	gint              start_index;
	gint              end_index;

	AtkStateSet      *saved_states;
};

static void ev_form_field_accessible_component_iface_init (AtkComponentIface *iface);

G_DEFINE_TYPE_WITH_CODE (EvFormFieldAccessible, ev_form_field_accessible, ATK_TYPE_OBJECT,
			 G_ADD_PRIVATE (EvFormFieldAccessible)
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

static gboolean
get_indices_in_parent (AtkObject *atk_object,
		       gint *start,
		       gint *end)
{
	EvFormFieldAccessiblePrivate *priv;
	EvView *view;
	EvRectangle *areas = NULL;
	guint n_areas = 0;
	gint last_zero_sized_index = -1;
	gint i;

	priv = EV_FORM_FIELD_ACCESSIBLE (atk_object)->priv;
	if (priv->start_index != -1 && priv->end_index != -1) {
		*start = priv->start_index;
		*end = priv->end_index;
		return TRUE;
	}

	view = ev_page_accessible_get_view (priv->page);
	if (!view->page_cache)
		return FALSE;

	ev_page_cache_get_text_layout (view->page_cache,
				       ev_page_accessible_get_page (priv->page),
				       &areas, &n_areas);
	if (!areas)
		return FALSE;

	for (i = 0; i < n_areas; i++) {
		EvRectangle *rect = areas + i;
		gdouble      c_x, c_y;

		c_x = rect->x1 + (rect->x2 - rect->x1) / 2.;
		c_y = rect->y1 + (rect->y2 - rect->y1) / 2.;

		if (c_x >= priv->area.x1 && c_x <= priv->area.x2 &&
		    c_y >= priv->area.y1 && c_y <= priv->area.y2) {
			priv->start_index = i;
			break;
		}
	}

	if (priv->start_index == -1)
		return FALSE;

	for (i = priv->start_index + 1; i < n_areas; i++) {
		EvRectangle *rect = areas + i;
		gdouble      c_x, c_y;

		/* A zero-sized text rect suggests a line break. If it is within the text of the
		 * field, we want to preserve it; if it is the character immediately after, we
		 * do not. We won't know which it is until we find the first text rect that is
		 * outside of the area occupied by the field.
		 */
		if (rect->y1 == rect->y2) {
			last_zero_sized_index = i;
			continue;
		}

		c_x = rect->x1 + (rect->x2 - rect->x1) / 2.;
		c_y = rect->y1 + (rect->y2 - rect->y1) / 2.;

		if (c_x < priv->area.x1 || c_x > priv->area.x2 ||
		    c_y < priv->area.y1 || c_y > priv->area.y2) {
			priv->end_index = last_zero_sized_index + 1 == i ? i - 1 : i;
			break;
		}
	}

	if (priv->end_index == -1)
		return FALSE;

	*start = priv->start_index;
	*end = priv->end_index;
	return TRUE;
}

static gchar *
get_text_under_element (AtkObject *atk_object)
{
	gint start = -1;
	gint end = -1;

	if (get_indices_in_parent (atk_object, &start, &end) && start != end)
		return atk_text_get_text (ATK_TEXT (atk_object_get_parent (atk_object)), start, end);

	return NULL;
}

static const gchar *
ev_form_field_accessible_get_name (AtkObject *atk_object)
{
	EvFormFieldAccessiblePrivate *priv;

	priv = EV_FORM_FIELD_ACCESSIBLE (atk_object)->priv;
	if (priv->name)
		return priv->name;

	if (EV_IS_FORM_FIELD_BUTTON (priv->form_field)) {
		EvFormFieldButton *button = EV_FORM_FIELD_BUTTON (priv->form_field);

		if (button->type == EV_FORM_FIELD_BUTTON_PUSH)
			priv->name = get_text_under_element (atk_object);
	}

	return priv->name;
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

void
ev_form_field_accessible_update_state (EvFormFieldAccessible *accessible)
{
	AtkObject *atk_object;
	AtkStateSet *states;
	AtkStateSet *changed_states;
	gint i;

	atk_object = ATK_OBJECT (accessible);
	states = ev_form_field_accessible_ref_state_set (atk_object);
	changed_states = atk_state_set_xor_sets (accessible->priv->saved_states, states);
	if (changed_states && !atk_state_set_is_empty (accessible->priv->saved_states)) {
		for (i = 0; i < ATK_STATE_LAST_DEFINED; i++) {
			if (atk_state_set_contains_state (changed_states, i))
				atk_object_notify_state_change (atk_object, i, atk_state_set_contains_state (states, i));
		}
	}

	g_object_unref (accessible->priv->saved_states);

	atk_state_set_clear_states (changed_states);
	accessible->priv->saved_states = atk_state_set_or_sets (changed_states, states);

	g_object_unref (changed_states);
	g_object_unref (states);
}

static void
ev_form_field_accessible_finalize (GObject *object)
{
	EvFormFieldAccessiblePrivate *priv = EV_FORM_FIELD_ACCESSIBLE (object)->priv;

	g_object_unref (priv->form_field);
	g_free (priv->name);
	g_object_unref (priv->saved_states);

	G_OBJECT_CLASS (ev_form_field_accessible_parent_class)->finalize (object);
}

static void
ev_form_field_accessible_class_init (EvFormFieldAccessibleClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	AtkObjectClass *atk_class = ATK_OBJECT_CLASS (klass);

	object_class->finalize = ev_form_field_accessible_finalize;
	atk_class->get_name = ev_form_field_accessible_get_name;
	atk_class->get_parent = ev_form_field_accessible_get_parent;
	atk_class->get_role = ev_form_field_accessible_get_role;
	atk_class->ref_state_set = ev_form_field_accessible_ref_state_set;
}

static void
ev_form_field_accessible_init (EvFormFieldAccessible *accessible)
{
	accessible->priv = ev_form_field_accessible_get_instance_private (accessible);
	accessible->priv->start_index = -1;
	accessible->priv->end_index = -1;
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
	atk_form_field->priv->saved_states = atk_state_set_new ();
	ev_form_field_accessible_update_state (atk_form_field);

	return EV_FORM_FIELD_ACCESSIBLE (atk_form_field);
}

EvFormField *
ev_form_field_accessible_get_field (EvFormFieldAccessible *accessible)
{
	return accessible->priv->form_field;
}
