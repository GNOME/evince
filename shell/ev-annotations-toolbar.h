/* ev-annotations-toolbar.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2015 Carlos Garcia Campos  <carlosgc@gnome.org>
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

#ifndef __EV_ANNOTATIONS_TOOLBAR_H__
#define __EV_ANNOTATIONS_TOOLBAR_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EV_TYPE_ANNOTATIONS_TOOLBAR              (ev_annotations_toolbar_get_type())
#define EV_ANNOTATIONS_TOOLBAR(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_ANNOTATIONS_TOOLBAR, EvAnnotationsToolbar))
#define EV_IS_ANNOTATIONS_TOOLBAR(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_ANNOTATIONS_TOOLBAR))
#define EV_ANNOTATIONS_TOOLBAR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_ANNOTATIONS_TOOLBAR, EvAnnotationsToolbarClass))
#define EV_IS_ANNOTATIONS_TOOLBAR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_ANNOTATIONS_TOOLBAR))
#define EV_ANNOTATIONS_TOOLBAR_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_ANNOTATIONS_TOOLBAR, EvAnnotationsToolbarClass))

typedef struct _EvAnnotationsToolbar        EvAnnotationsToolbar;
typedef struct _EvAnnotationsToolbarClass   EvAnnotationsToolbarClass;

GType      ev_annotations_toolbar_get_type           (void) G_GNUC_CONST;
GtkWidget *ev_annotations_toolbar_new                (void);
void       ev_annotations_toolbar_add_annot_finished (EvAnnotationsToolbar *toolbar);

G_END_DECLS

#endif /* __EV_ANNOTATIONS_TOOLBAR_H__ */
