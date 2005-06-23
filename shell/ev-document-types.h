/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/*
 *  Copyright (C) 2005, Red Hat, Inc. 
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef EV_DOCUMENT_TYPES_H
#define EV_DOCUMENT_TYPES_H

#include "ev-document.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

GType ev_document_type_lookup (const char  *uri,
			       gchar       **mime_type,
			       GError      **error);

void ev_document_types_add_filters	    (GtkWidget *chooser);
void ev_document_types_add_filters_for_type (GtkWidget *chooser, GType type);

G_END_DECLS

#endif
