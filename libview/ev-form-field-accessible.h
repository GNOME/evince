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

#pragma once

#if !defined (EVINCE_COMPILATION)
#error "This is a private header."
#endif

#include "ev-page-accessible.h"
#include "ev-form-field.h"

#define EV_TYPE_FORM_FIELD_ACCESSIBLE      (ev_form_field_accessible_get_type ())
#define EV_FORM_FIELD_ACCESSIBLE(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_FORM_FIELD_ACCESSIBLE, EvFormFieldAccessible))
#define EV_IS_FORM_FIELD_ACCESSIBLE(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_FORM_FIELD_ACCESSIBLE))

typedef struct _EvFormFieldAccessible        EvFormFieldAccessible;
typedef struct _EvFormFieldAccessibleClass   EvFormFieldAccessibleClass;
typedef struct _EvFormFieldAccessiblePrivate EvFormFieldAccessiblePrivate;

struct _EvFormFieldAccessible {
        AtkObject parent;
        EvFormFieldAccessiblePrivate *priv;
};

struct _EvFormFieldAccessibleClass {
        AtkObjectClass parent_class;
};

GType ev_form_field_accessible_get_type (void);
EvFormFieldAccessible *ev_form_field_accessible_new (EvPageAccessible *page,
						     EvFormField      *form_field,
						     EvRectangle      *area);
EvFormField           *ev_form_field_accessible_get_field (EvFormFieldAccessible *accessible);
void                   ev_form_field_accessible_update_state (EvFormFieldAccessible *accessible);
