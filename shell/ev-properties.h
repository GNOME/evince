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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __EV_PROPERTIES_H__
#define __EV_PROPERTIES_H__

#include "ev-document.h"

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

typedef struct _EvProperties EvProperties;
typedef struct _EvPropertiesClass EvPropertiesClass;
typedef struct _EvPropertiesPrivate EvPropertiesPrivate;

#define EV_TYPE_PROPERTIES		(ev_properties_get_type())
#define EV_PROPERTIES(object)	        (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_PROPERTIES, EvProperties))
#define EV_PROPERTIES_CLASS(klass)	(G_TYPE_CHACK_CLASS_CAST((klass), EV_TYPE_PROPERTIES, EvPropertiesClass))
#define EV_IS_PROPERTIES(object)	(G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_PROPERTIES))
#define EV_IS_PROPERTIES_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_PROPERTIES))
#define EV_PROPERTIES_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_PROPERTIES, EvPropertiesClass))

GType	      ev_properties_get_type     (void);
EvProperties *ev_properties_new          (void);
void	      ev_properties_set_document (EvProperties *properties,
					  EvDocument   *document);
void	      ev_properties_show	 (EvProperties *properties,
				          GtkWidget    *parent);

G_END_DECLS

#endif /* __EV_PROPERTIES_H__ */
