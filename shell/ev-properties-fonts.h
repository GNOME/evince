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

typedef struct _EvPropertiesFonts EvPropertiesFonts;
typedef struct _EvPropertiesFontsClass EvPropertiesFontsClass;
typedef struct _EvPropertiesFontsPrivate EvPropertiesFontsPrivate;

#define EV_TYPE_PROPERTIES_FONTS		(ev_properties_fonts_get_type())
#define EV_PROPERTIES_FONTS(object)	        (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_PROPERTIES_FONTS, EvPropertiesFonts))
#define EV_PROPERTIES_FONTS_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_PROPERTIES_FONTS, EvPropertiesFontsClass))
#define EV_IS_PROPERTIES(object)		(G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_PROPERTIES_FONTS))
#define EV_IS_PROPERTIES_CLASS(klass)   	(G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_PROPERTIES_FONTS))
#define EV_PROPERTIES_FONTS_GET_CLASS(object)	(G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_PROPERTIES_FONTS, EvPropertiesFontsClass))

GType	           ev_properties_fonts_get_type     (void);
GtkWidget         *ev_properties_fonts_new          (void);
void		   ev_properties_fonts_set_document (EvPropertiesFonts *properties,
					  	     EvDocument        *document);

G_END_DECLS
