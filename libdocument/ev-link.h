/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2005 Red Hat, Inc.
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
#include "ev-link-action.h"

G_BEGIN_DECLS

#define EV_TYPE_LINK              (ev_link_get_type())

EV_PUBLIC
G_DECLARE_FINAL_TYPE (EvLink, ev_link, EV, LINK, GObject)

EV_PUBLIC
EvLink	     *ev_link_new	 (const gchar  *title,
				  EvLinkAction *action);

EV_PUBLIC
const gchar  *ev_link_get_title  (EvLink       *self);
EV_PUBLIC
EvLinkAction *ev_link_get_action (EvLink       *self);

G_END_DECLS
