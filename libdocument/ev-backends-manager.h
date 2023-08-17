/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2007 Carlos Garcia Campos <carlosgc@gnome.org>
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

#include <glib.h>

#include "ev-document.h"
#include "ev-macros.h"

G_BEGIN_DECLS

typedef struct _EvTypeInfo {
	const gchar  *desc;
	const gchar **mime_types;
} EvTypeInfo;

EV_PUBLIC
EvDocument  *ev_backends_manager_get_document             (const gchar *mime_type);

EV_PUBLIC
EvTypeInfo  *ev_backends_manager_get_document_type_info   (EvDocument  *document);

EV_PUBLIC
GList       *ev_backends_manager_get_all_types_info       (void);

G_END_DECLS
