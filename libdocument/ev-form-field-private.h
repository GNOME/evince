/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2020 Germán Poo-Caamaño <gpoo@gnome.org>
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

#if !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#ifndef EV_FORM_FIELD_PRIVATE_H
#define EV_FORM_FIELD_PRIVATE_H

//#include <glib-object.h>

#include "ev-form-field.h"

G_BEGIN_DECLS

/* EvFormField base class */
gchar *ev_form_field_get_alternate_name (EvFormField *field);
void   ev_form_field_set_alternate_name (EvFormField *field,
					 gchar       *alternative_text);

G_END_DECLS

#endif /* !EV_FORM_FIELD_PRIVATE_H */

