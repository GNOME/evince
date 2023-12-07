/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2006 Carlos Garcia Campos <carlosgc@gnome.org>
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
#include "ev-link-dest.h"

G_BEGIN_DECLS

#define EV_TYPE_LINK_ACTION              (ev_link_action_get_type())

EV_PUBLIC
G_DECLARE_FINAL_TYPE (EvLinkAction, ev_link_action, EV, LINK_ACTION, GObject)

typedef enum {
	EV_LINK_ACTION_TYPE_GOTO_DEST,
	EV_LINK_ACTION_TYPE_GOTO_REMOTE,
	EV_LINK_ACTION_TYPE_EXTERNAL_URI,
	EV_LINK_ACTION_TYPE_LAUNCH,
	EV_LINK_ACTION_TYPE_NAMED,
	EV_LINK_ACTION_TYPE_LAYERS_STATE,
	EV_LINK_ACTION_TYPE_RESET_FORM
	/* We'll probably fill this in more as we support the other types of
	 * actions */
} EvLinkActionType;

EV_PUBLIC
EvLinkActionType ev_link_action_get_action_type          (EvLinkAction *self);
EV_PUBLIC
EvLinkDest      *ev_link_action_get_dest                 (EvLinkAction *self);
EV_PUBLIC
const gchar     *ev_link_action_get_uri                  (EvLinkAction *self);
EV_PUBLIC
const gchar     *ev_link_action_get_filename             (EvLinkAction *self);
EV_PUBLIC
const gchar     *ev_link_action_get_params               (EvLinkAction *self);
EV_PUBLIC
const gchar     *ev_link_action_get_name                 (EvLinkAction *self);
EV_PUBLIC
GList           *ev_link_action_get_show_list            (EvLinkAction *self);
EV_PUBLIC
GList           *ev_link_action_get_hide_list            (EvLinkAction *self);
EV_PUBLIC
GList           *ev_link_action_get_toggle_list          (EvLinkAction *self);
EV_PUBLIC
GList           *ev_link_action_get_reset_fields         (EvLinkAction *self);
EV_PUBLIC
gboolean         ev_link_action_get_exclude_reset_fields (EvLinkAction *self);

EV_PUBLIC
EvLinkAction    *ev_link_action_new_dest                 (EvLinkDest   *dest);
EV_PUBLIC
EvLinkAction    *ev_link_action_new_remote               (EvLinkDest   *dest,
						          const gchar  *filename);
EV_PUBLIC
EvLinkAction    *ev_link_action_new_external_uri         (const gchar  *uri);
EV_PUBLIC
EvLinkAction    *ev_link_action_new_launch               (const gchar  *filename,
						          const gchar  *params);
EV_PUBLIC
EvLinkAction    *ev_link_action_new_named                (const gchar  *name);
EV_PUBLIC
EvLinkAction    *ev_link_action_new_layers_state         (GList        *show_list,
						          GList        *hide_list,
						          GList        *toggle_list);
EV_PUBLIC
EvLinkAction    *ev_link_action_new_reset_form           (GList        *fields,
						          gboolean      exclude_fields);

EV_PUBLIC
gboolean         ev_link_action_equal                    (EvLinkAction *a,
                                                          EvLinkAction *b);

G_END_DECLS
