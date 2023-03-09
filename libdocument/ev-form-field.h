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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#include <glib-object.h>

#include "ev-macros.h"
#include "ev-document.h"
#include "ev-link.h"

G_BEGIN_DECLS

#define EV_TYPE_FORM_FIELD                        (ev_form_field_get_type())
#define EV_FORM_FIELD(object)                     (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_FORM_FIELD, EvFormField))
#define EV_FORM_FIELD_CLASS(klass)                (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_FORM_FIELD, EvFormFieldClass))
#define EV_IS_FORM_FIELD(object)                  (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_FORM_FIELD))
#define EV_IS_FORM_FIELD_CLASS(klass)             (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_FORM_FIELD))
#define EV_FORM_FIELD_GET_CLASS(object)           (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_FORM_FIELD, EvFormFieldClass))

#define EV_TYPE_FORM_FIELD_TEXT                   (ev_form_field_text_get_type())
#define EV_FORM_FIELD_TEXT(object)                (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_FORM_FIELD_TEXT, EvFormFieldText))
#define EV_FORM_FIELD_TEXT_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_FORM_FIELD_TEXT, EvFormFieldTextClass))
#define EV_IS_FORM_FIELD_TEXT(object)             (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_FORM_FIELD_TEXT))
#define EV_IS_FORM_FIELD_TEXT_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_FORM_FIELD_TEXT))
#define EV_FORM_FIELD_TEXT_GET_CLASS(object)      (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_FORM_FIELD_TEXT, EvFormFieldTextClass))

#define EV_TYPE_FORM_FIELD_BUTTON                 (ev_form_field_button_get_type())
#define EV_FORM_FIELD_BUTTON(object)              (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_FORM_FIELD_BUTTON, EvFormFieldButton))
#define EV_FORM_FIELD_BUTTON_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_FORM_FIELD_BUTTON, EvFormFieldButtonClass))
#define EV_IS_FORM_FIELD_BUTTON(object)           (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_FORM_FIELD_BUTTON))
#define EV_IS_FORM_FIELD_BUTTON_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_FORM_FIELD_BUTTON))
#define EV_FORM_FIELD_BUTTON_GET_CLASS(object)    (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_FORM_FIELD_BUTTON, EvFormFieldButtonClass))

#define EV_TYPE_FORM_FIELD_CHOICE                 (ev_form_field_choice_get_type())
#define EV_FORM_FIELD_CHOICE(object)              (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_FORM_FIELD_CHOICE, EvFormFieldChoice))
#define EV_FORM_FIELD_CHOICE_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_FORM_FIELD_CHOICE, EvFormFieldChoiceClass))
#define EV_IS_FORM_FIELD_CHOICE(object)           (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_FORM_FIELD_CHOICE))
#define EV_IS_FORM_FIELD_CHOICE_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_FORM_FIELD_CHOICE))
#define EV_FORM_FIELD_CHOICE_GET_CLASS(object)    (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_FORM_FIELD_CHOICE, EvFormFieldChoiceClass))

#define EV_TYPE_FORM_FIELD_SIGNATURE              (ev_form_field_signature_get_type())
#define EV_FORM_FIELD_SIGNATURE(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_FORM_FIELD_SIGNATURE, EvFormFieldSignature))
#define EV_FORM_FIELD_SIGNATURE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_FORM_FIELD_SIGNATURE, EvFormFieldSignatureClass))
#define EV_IS_FORM_FIELD_SIGNATURE(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_FORM_FIELD_SIGNATURE))
#define EV_IS_FORM_FIELD_SIGNATURE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_FORM_FIELD_SIGNATURE))
#define EV_FORM_FIELD_SIGNATURE_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_FORM_FIELD_SIGNATURE, EvFormFieldSignatureClass))

typedef struct _EvFormField               EvFormField;
typedef struct _EvFormFieldClass          EvFormFieldClass;

typedef struct _EvFormFieldText           EvFormFieldText;
typedef struct _EvFormFieldTextClass      EvFormFieldTextClass;

typedef struct _EvFormFieldButton         EvFormFieldButton;
typedef struct _EvFormFieldButtonClass    EvFormFieldButtonClass;

typedef struct _EvFormFieldChoice         EvFormFieldChoice;
typedef struct _EvFormFieldChoiceClass    EvFormFieldChoiceClass;

typedef struct _EvFormFieldSignature      EvFormFieldSignature;
typedef struct _EvFormFieldSignatureClass EvFormFieldSignatureClass;

typedef enum
{
	EV_FORM_FIELD_TEXT_NORMAL,
	EV_FORM_FIELD_TEXT_MULTILINE,
	EV_FORM_FIELD_TEXT_FILE_SELECT
} EvFormFieldTextType;

typedef enum
{
	EV_FORM_FIELD_BUTTON_PUSH,
	EV_FORM_FIELD_BUTTON_CHECK,
	EV_FORM_FIELD_BUTTON_RADIO
} EvFormFieldButtonType;

typedef enum
{
	EV_FORM_FIELD_CHOICE_COMBO,
	EV_FORM_FIELD_CHOICE_LIST
} EvFormFieldChoiceType;

struct _EvFormField
{
	GObject parent;

	gint     id;
	gboolean is_read_only;
	gdouble  font_size;
	EvLink  *activation_link;

	EvPage  *page;
	gboolean changed;
};

struct _EvFormFieldClass
{
	GObjectClass parent_class;
};

struct _EvFormFieldText
{
	EvFormField parent;

	EvFormFieldTextType type;

	gboolean do_spell_check : 1;
	gboolean do_scroll : 1;
	gboolean comb : 1;
	gboolean is_rich_text : 1;
	gboolean is_password;

	gint   max_len;
	gchar *text;
};

struct _EvFormFieldTextClass
{
	EvFormFieldClass parent_class;
};

struct _EvFormFieldButton
{
	EvFormField parent;

	EvFormFieldButtonType type;

	gboolean state;
};

struct _EvFormFieldButtonClass
{
	EvFormFieldClass parent_class;
};

struct _EvFormFieldChoice
{
	EvFormField parent;

	EvFormFieldChoiceType type;

	gboolean multi_select : 1;
	gboolean is_editable : 1;
	gboolean do_spell_check : 1;
	gboolean commit_on_sel_change : 1;

	GList *selected_items;
	gchar *text;
};

struct _EvFormFieldChoiceClass
{
	EvFormFieldClass parent_class;
};

struct _EvFormFieldSignature
{
	EvFormField parent;

	/* TODO */
};

struct _EvFormFieldSignatureClass
{
	EvFormFieldClass parent_class;
};

/* EvFormField base class */
EV_PUBLIC
GType        ev_form_field_get_type           (void) G_GNUC_CONST;

/* EvFormFieldText */
EV_PUBLIC
GType        ev_form_field_text_get_type      (void) G_GNUC_CONST;
EV_PUBLIC
EvFormField *ev_form_field_text_new           (gint                  id,
					       EvFormFieldTextType   type);

/* EvFormFieldButton */
EV_PUBLIC
GType        ev_form_field_button_get_type    (void) G_GNUC_CONST;
EV_PUBLIC
EvFormField *ev_form_field_button_new         (gint                  id,
					       EvFormFieldButtonType type);

/* EvFormFieldChoice */
EV_PUBLIC
GType        ev_form_field_choice_get_type    (void) G_GNUC_CONST;
EV_PUBLIC
EvFormField *ev_form_field_choice_new         (gint                  id,
					       EvFormFieldChoiceType type);

/* EvFormFieldSignature */
EV_PUBLIC
GType        ev_form_field_signature_get_type (void) G_GNUC_CONST;
EV_PUBLIC
EvFormField *ev_form_field_signature_new      (gint                  id);


G_END_DECLS
