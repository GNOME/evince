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

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#ifndef EV_LINK_H
#define EV_LINK_H

#include <glib-object.h>
#include "ev-document.h"
#include "ev-link-action.h"

G_BEGIN_DECLS

typedef struct _EvLink EvLink;
typedef struct _EvLinkClass EvLinkClass;
typedef struct _EvLinkPrivate EvLinkPrivate;

#define EV_TYPE_LINK              (ev_link_get_type())
#define EV_LINK(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_LINK, EvLink))
#define EV_LINK_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_LINK, EvLinkClass))
#define EV_IS_LINK(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_LINK))
#define EV_IS_LINK_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_LINK))
#define EV_LINK_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_LINK, EvLinkClass))

GType         ev_link_get_type	 (void) G_GNUC_CONST;

EvLink	     *ev_link_new	 (const gchar  *title,
				  EvLinkAction *action);

const gchar  *ev_link_get_title  (EvLink       *self);
EvLinkAction *ev_link_get_action (EvLink       *self);

G_END_DECLS

#endif /* !EV_LINK_H */
