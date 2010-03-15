/* ev-sidebar-attachments.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2006 Carlos Garcia Campos
 *
 * Author:
 *   Carlos Garcia Campos <carlosgc@gnome.org>
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

#ifndef __EV_SIDEBAR_ATTACHMENTS_H__
#define __EV_SIDEBAR_ATTACHMENTS_H__

#include "ev-attachment.h"
#include "ev-document.h"

G_BEGIN_DECLS

typedef struct _EvSidebarAttachments        EvSidebarAttachments;
typedef struct _EvSidebarAttachmentsClass   EvSidebarAttachmentsClass;
typedef struct _EvSidebarAttachmentsPrivate EvSidebarAttachmentsPrivate;

#define EV_TYPE_SIDEBAR_ATTACHMENTS              (ev_sidebar_attachments_get_type())
#define EV_SIDEBAR_ATTACHMENTS(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_SIDEBAR_ATTACHMENTS, EvSidebarAttachments))
#define EV_SIDEBAR_ATTACHMENTS_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_SIDEBAR_ATTACHMENTS, EvSidebarAttachmentsClass))
#define EV_IS_SIDEBAR_ATTACHMENTS(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_SIDEBAR_ATTACHMENTS))
#define EV_IS_SIDEBAR_ATTACHMENTS_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_SIDEBAR_ATTACHMENTS))
#define EV_SIDEBAR_ATTACHMENTS_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_SIDEBAR_ATTACHMENTS, EvSidebarAttachmentsClass))

struct _EvSidebarAttachments {
	GtkVBox base_instance;

	EvSidebarAttachmentsPrivate *priv;
};

struct _EvSidebarAttachmentsClass {
	GtkVBoxClass base_class;

	/* Signals */
	void (*popup_menu) (EvSidebarAttachments *ev_attachbar,
			    EvAttachment    *attachment);
};

GType      ev_sidebar_attachments_get_type     (void) G_GNUC_CONST;
GtkWidget *ev_sidebar_attachments_new          (void);

G_END_DECLS

#endif /* __EV_SIDEBAR_ATTACHMENTS_H__ */
