/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2006 Carlos Garcia Campos <carlosgc@gnome.org>
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
#include <gio/gio.h>

#include "ev-macros.h"

G_BEGIN_DECLS

#define EV_TYPE_ATTACHMENT              (ev_attachment_get_type())

#define EV_ATTACHMENT_ERROR (ev_attachment_error_quark ())

EV_PUBLIC
G_DECLARE_DERIVABLE_TYPE (EvAttachment, ev_attachment, EV, ATTACHMENT, GObject);

struct _EvAttachmentClass {
	GObjectClass base_class;
};

EV_PUBLIC
GQuark        ev_attachment_error_quark          (void) G_GNUC_CONST;

EV_PUBLIC
EvAttachment *ev_attachment_new (const gchar  *name,
				 const gchar  *description,
				 GDateTime    *mtime,
				 GDateTime    *ctime,
				 gsize         size,
				 gpointer      data);

EV_PUBLIC
const gchar *ev_attachment_get_name              (EvAttachment *attachment);
EV_PUBLIC
const gchar *ev_attachment_get_description       (EvAttachment *attachment);

EV_PUBLIC
GDateTime   *ev_attachment_get_modification_datetime (EvAttachment *attachment);
EV_PUBLIC
GDateTime   *ev_attachment_get_creation_datetime     (EvAttachment *attachment);

EV_PUBLIC
const gchar *ev_attachment_get_mime_type         (EvAttachment *attachment);
EV_PUBLIC
gboolean     ev_attachment_save                  (EvAttachment *attachment,
						  GFile        *file,
						  GError      **error);
EV_PUBLIC
gboolean     ev_attachment_open                  (EvAttachment       *attachment,
						  GAppLaunchContext  *context,
						  GError            **error);

G_END_DECLS
