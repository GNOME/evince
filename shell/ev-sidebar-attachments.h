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

#pragma once

#include "ev-attachment.h"
#include "ev-document.h"

G_BEGIN_DECLS

typedef struct _EvSidebarAttachmentsPrivate EvSidebarAttachmentsPrivate;

#define EV_TYPE_SIDEBAR_ATTACHMENTS              (ev_sidebar_attachments_get_type())
G_DECLARE_DERIVABLE_TYPE (EvSidebarAttachments, ev_sidebar_attachments, EV, SIDEBAR_ATTACHMENTS, GtkBox);

struct _EvSidebarAttachmentsClass {
	GtkBoxClass base_class;

	/* Signals */
	void (*popup_menu)      (EvSidebarAttachments *ev_attachbar,
			         EvAttachment         *attachment);
	void (*save_attachment) (EvSidebarAttachments *ev_attachbar,
			         EvAttachment         *attachment,
	                         const char          *uri);
};

GtkWidget *ev_sidebar_attachments_new          (void);

G_END_DECLS
