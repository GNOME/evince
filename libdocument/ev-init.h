/* this file is part of evince, a gnome document viewer
 *
 * Copyright © 2009 Christian Persch
 *
 * Evince is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * Evince is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#include <glib.h>

#include "ev-macros.h"

G_BEGIN_DECLS

EV_PUBLIC
const gchar* ev_get_locale_dir (void);

EV_PUBLIC
gboolean    ev_init           (void);

EV_PUBLIC
void        ev_shutdown       (void);

gboolean   _ev_is_initialized (void);

G_END_DECLS
