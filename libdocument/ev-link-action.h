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

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#ifndef EV_LINK_ACTION_H
#define EV_LINK_ACTION_H

#include <glib-object.h>
#include "ev-link-dest.h"

G_BEGIN_DECLS

typedef struct _EvLinkAction        EvLinkAction;
typedef struct _EvLinkActionClass   EvLinkActionClass;
typedef struct _EvLinkActionPrivate EvLinkActionPrivate;

#define EV_TYPE_LINK_ACTION              (ev_link_action_get_type())
#define EV_LINK_ACTION(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_LINK_ACTION, EvLinkAction))
#define EV_LINK_ACTION_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_LINK_ACTION, EvLinkActionClass))
#define EV_IS_LINK_ACTION(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_LINK_ACTION))
#define EV_IS_LINK_ACTION_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_LINK_ACTION))
#define EV_LINK_ACTION_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_LINK_ACTION, EvLinkActionClass))

typedef enum {
	EV_LINK_ACTION_TYPE_GOTO_DEST,
	EV_LINK_ACTION_TYPE_GOTO_REMOTE,
	EV_LINK_ACTION_TYPE_EXTERNAL_URI,
	EV_LINK_ACTION_TYPE_LAUNCH,
	EV_LINK_ACTION_TYPE_NAMED,
	EV_LINK_ACTION_TYPE_LAYERS_STATE
	/* We'll probably fill this in more as we support the other types of
	 * actions */
} EvLinkActionType;

GType            ev_link_action_get_type         (void) G_GNUC_CONST;

EvLinkActionType ev_link_action_get_action_type  (EvLinkAction *self);
EvLinkDest      *ev_link_action_get_dest         (EvLinkAction *self);
const gchar     *ev_link_action_get_uri          (EvLinkAction *self);
const gchar     *ev_link_action_get_filename     (EvLinkAction *self);
const gchar     *ev_link_action_get_params       (EvLinkAction *self);
const gchar     *ev_link_action_get_name         (EvLinkAction *self);
GList           *ev_link_action_get_show_list    (EvLinkAction *self);
GList           *ev_link_action_get_hide_list    (EvLinkAction *self);
GList           *ev_link_action_get_toggle_list  (EvLinkAction *self);

EvLinkAction    *ev_link_action_new_dest         (EvLinkDest   *dest);
EvLinkAction    *ev_link_action_new_remote       (EvLinkDest   *dest,
						  const gchar  *filename);
EvLinkAction    *ev_link_action_new_external_uri (const gchar  *uri);
EvLinkAction    *ev_link_action_new_launch       (const gchar  *filename,
						  const gchar  *params);
EvLinkAction    *ev_link_action_new_named        (const gchar  *name);
EvLinkAction    *ev_link_action_new_layers_state (GList        *show_list,
						  GList        *hide_list,
						  GList        *toggle_list);

gboolean         ev_link_action_equal            (EvLinkAction *a,
                                                  EvLinkAction *b);

G_END_DECLS

#endif /* EV_LINK_ACTION_H */
