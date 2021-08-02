/* ev-sidebar-annotations.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2010 Carlos Garcia Campos  <carlosgc@gnome.org>
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

#include <gtk/gtk.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _EvSidebarAnnotationsPrivate EvSidebarAnnotationsPrivate;

#define EV_TYPE_SIDEBAR_ANNOTATIONS              (ev_sidebar_annotations_get_type())
G_DECLARE_DERIVABLE_TYPE (EvSidebarAnnotations, ev_sidebar_annotations, EV, SIDEBAR_ANNOTATIONS, GtkBox);

struct _EvSidebarAnnotationsClass {
	GtkBoxClass base_class;

	void    (* annot_activated)     (EvSidebarAnnotations *sidebar_annots,
					 EvMapping            *mapping);
};

GtkWidget *ev_sidebar_annotations_new           (void);
void       ev_sidebar_annotations_annot_added   (EvSidebarAnnotations *sidebar_annots,
					         EvAnnotation         *annot);
void       ev_sidebar_annotations_annot_changed (EvSidebarAnnotations *sidebar_annots,
					         EvAnnotation         *annot);
void       ev_sidebar_annotations_annot_removed (EvSidebarAnnotations *sidebar_annots);

G_END_DECLS
