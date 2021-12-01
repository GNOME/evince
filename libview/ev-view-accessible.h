/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2004 Red Hat, Inc
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

#if !defined (EVINCE_COMPILATION)
#error "This is a private header."
#endif

#include <gtk/gtk-a11y.h>
#include "ev-document-model.h"

#define EV_TYPE_VIEW_ACCESSIBLE      (ev_view_accessible_get_type ())
#define EV_VIEW_ACCESSIBLE(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_VIEW_ACCESSIBLE, EvViewAccessible))
#define EV_IS_VIEW_ACCESSIBLE(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_VIEW_ACCESSIBLE))

typedef struct _EvViewAccessible        EvViewAccessible;
typedef struct _EvViewAccessibleClass   EvViewAccessibleClass;
typedef struct _EvViewAccessiblePrivate EvViewAccessiblePrivate;

struct _EvViewAccessible
{
	GtkContainerAccessible parent;

	EvViewAccessiblePrivate *priv;
};

struct _EvViewAccessibleClass
{
	GtkContainerAccessibleClass parent_class;
};

GType      ev_view_accessible_get_type  (void);
AtkObject *ev_view_accessible_new       (GtkWidget *widget);
void       ev_view_accessible_set_model (EvViewAccessible *accessible,
					 EvDocumentModel  *model);
gint       ev_view_accessible_get_n_pages (EvViewAccessible *accessible);
gint       ev_view_accessible_get_relevant_page (EvViewAccessible *accessible);
void       ev_view_accessible_set_page_range (EvViewAccessible *accessible,
					      gint start,
					      gint end);
void       _transform_doc_rect_to_atk_rect (EvViewAccessible *accessible,
					    gint              page,
					    EvRectangle      *doc_rect,
					    EvRectangle      *atk_rect,
					    AtkCoordType      coord_type);
gboolean  ev_view_accessible_is_doc_rect_showing (EvViewAccessible *accessible,
						  gint              page,
						  EvRectangle      *doc_rect);
void       ev_view_accessible_set_focused_element (EvViewAccessible *accessible,
						   EvMapping        *new_focus,
						   gint              new_focus_page);
void       ev_view_accessible_update_element_state (EvViewAccessible *accessible,
						    EvMapping        *element,
						    gint              element_page);
