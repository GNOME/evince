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

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#ifndef __EV_ATTACHMENT_H__
#define __EV_ATTACHMENT_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct _EvAttachment        EvAttachment;
typedef struct _EvAttachmentClass   EvAttachmentClass;
typedef struct _EvAttachmentPrivate EvAttachmentPrivate;

#define EV_TYPE_ATTACHMENT              (ev_attachment_get_type())
#define EV_ATTACHMENT(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_ATTACHMENT, EvAttachment))
#define EV_ATTACHMENT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_ATTACHMENT, EvAttachmentClass))
#define EV_IS_ATTACHMENT(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_ATTACHMENT))
#define EV_IS_ATTACHMENT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_ATTACHMENT))
#define EV_ATTACHMENT_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_ATTACHMENT, EvAttachmentClass))

#define EV_ATTACHMENT_ERROR (ev_attachment_error_quark ())

struct _EvAttachment {
	GObject base_instance;
	
	EvAttachmentPrivate *priv;
};

struct _EvAttachmentClass {
	GObjectClass base_class;
};

GType         ev_attachment_get_type             (void) G_GNUC_CONST;
GQuark        ev_attachment_error_quark          (void) G_GNUC_CONST;
EvAttachment *ev_attachment_new                  (const gchar  *name,
						  const gchar  *description,
						  GTime         mtime,
						  GTime         ctime,
						  gsize         size,
						  gpointer      data);

const gchar *ev_attachment_get_name              (EvAttachment *attachment);
const gchar *ev_attachment_get_description       (EvAttachment *attachment);
GTime        ev_attachment_get_modification_date (EvAttachment *attachment);
GTime        ev_attachment_get_creation_date     (EvAttachment *attachment);
const gchar *ev_attachment_get_mime_type         (EvAttachment *attachment);
gboolean     ev_attachment_save                  (EvAttachment *attachment,
						  GFile        *file,
						  GError      **error);
gboolean     ev_attachment_open                  (EvAttachment *attachment,
						  GdkScreen    *screen,
						  guint32       timestamp,
						  GError      **error);

G_END_DECLS

#endif /* __EV_ATTACHMENT_H__ */
