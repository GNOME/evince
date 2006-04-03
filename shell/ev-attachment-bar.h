/* ev-attachment-bar.h
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __EV_ATTACHMENT_BAR_H__
#define __EV_ATTACHMENT_BAR_H__

#include <gtk/gtkexpander.h>
#include "ev-attachment.h"
#include "ev-document.h"

G_BEGIN_DECLS

typedef struct _EvAttachmentBar        EvAttachmentBar;
typedef struct _EvAttachmentBarClass   EvAttachmentBarClass;
typedef struct _EvAttachmentBarPrivate EvAttachmentBarPrivate;

#define EV_TYPE_ATTACHMENT_BAR              (ev_attachment_bar_get_type())
#define EV_ATTACHMENT_BAR(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_ATTACHMENT_BAR, EvAttachmentBar))
#define EV_ATTACHMENT_BAR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_ATTACHMENT_BAR, EvAttachmentBarClass))
#define EV_IS_ATTACHMENT_BAR(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_ATTACHMENT_BAR))
#define EV_IS_ATTACHMENT_BAR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_ATTACHMENT_BAR))
#define EV_ATTACHMENT_BAR_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_ATTACHMENT_BAR, EvAttachmentBarClass))

struct _EvAttachmentBar {
	GtkExpander base_instance;

	EvAttachmentBarPrivate *priv;
};

struct _EvAttachmentBarClass {
	GtkExpanderClass base_class;

	/* Signals */
	void (*popup_menu) (EvAttachmentBar *ev_attachbar,
			    EvAttachment    *attachment);
};

GType      ev_attachment_bar_get_type     (void) G_GNUC_CONST;
GtkWidget *ev_attachment_bar_new          (void);

void       ev_attachment_bar_set_document (EvAttachmentBar *ev_attachbar,
					   EvDocument      *document);

G_END_DECLS

#endif /* __EV_ATTACHMENT_BAR_H__ */
