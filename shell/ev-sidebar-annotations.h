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

#ifndef __EV_SIDEBAR_ANNOTATIONS_H__
#define __EV_SIDEBAR_ANNOTATIONS_H__

#include <gtk/gtk.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _EvSidebarAnnotations        EvSidebarAnnotations;
typedef struct _EvSidebarAnnotationsClass   EvSidebarAnnotationsClass;
typedef struct _EvSidebarAnnotationsPrivate EvSidebarAnnotationsPrivate;

#define EV_TYPE_SIDEBAR_ANNOTATIONS              (ev_sidebar_annotations_get_type())
#define EV_SIDEBAR_ANNOTATIONS(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_SIDEBAR_ANNOTATIONS, EvSidebarAnnotations))
#define EV_SIDEBAR_ANNOTATIONS_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_SIDEBAR_ANNOTATIONS, EvSidebarAnnotationsClass))
#define EV_IS_SIDEBAR_ANNOTATIONS(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_SIDEBAR_ANNOTATIONS))
#define EV_IS_SIDEBAR_ANNOTATIONS_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_SIDEBAR_ANNOTATIONS))
#define EV_SIDEBAR_ANNOTATIONS_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_SIDEBAR_ANNOTATIONS, EvSidebarAnnotationsClass))

struct _EvSidebarAnnotations {
	GtkVBox base_instance;

	EvSidebarAnnotationsPrivate *priv;
};

struct _EvSidebarAnnotationsClass {
	GtkVBoxClass base_class;

	void    (* annot_activated)     (EvSidebarAnnotations *sidebar_annots,
					 EvMapping            *mapping);
	void    (* begin_annot_add)     (EvSidebarAnnotations *sidebar_annots,
					 EvAnnotationType      annot_type);
	void    (* annot_add_cancelled) (EvSidebarAnnotations *sidebar_annots);
};

GType      ev_sidebar_annotations_get_type      (void) G_GNUC_CONST;
GtkWidget *ev_sidebar_annotations_new           (void);
void       ev_sidebar_annotations_annot_added   (EvSidebarAnnotations *sidebar_annots,
					         EvAnnotation         *annot);
void       ev_sidebar_annotations_annot_removed (EvSidebarAnnotations *sidebar_annots);
G_END_DECLS

#endif /* __EV_SIDEBAR_ANNOTATIONS_H__ */
