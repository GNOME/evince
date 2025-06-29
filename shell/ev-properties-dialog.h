/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2005 Red Hat, Inc
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

#include "ev-document.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _EvPropertiesDialog EvPropertiesDialog;
typedef struct _EvPropertiesDialogClass EvPropertiesDialogClass;
typedef struct _EvPropertiesDialogPrivate EvPropertiesDialogPrivate;

#define EV_TYPE_PROPERTIES_DIALOG		(ev_properties_dialog_get_type())
#define EV_PROPERTIES_DIALOG(object)	        (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_PROPERTIES_DIALOG, EvPropertiesDialog))
#define EV_PROPERTIES_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_PROPERTIES_DIALOG, EvPropertiesDialogClass))
#define EV_IS_PROPERTIES_DIALOG(object)		(G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_PROPERTIES_DIALOG))
#define EV_IS_PROPERTIES_DIALOG_CLASS(klass)  	(G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_PROPERTIES_DIALOG))
#define EV_PROPERTIES_DIALOG_GET_CLASS(object)	(G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_PROPERTIES_DIALOG, EvPropertiesDialogClass))

GType		 ev_properties_dialog_get_type     (void);
GtkWidget	*ev_properties_dialog_new          (void);
void	         ev_properties_dialog_set_document (EvPropertiesDialog *properties,
						    const gchar        *uri,
					  	    EvDocument         *document);

G_END_DECLS
