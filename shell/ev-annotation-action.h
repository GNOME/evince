/* ev-annotation-action.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2020 Germán Poo-Caamaño <gpooo@gnome.org>
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

#ifndef EV_ANNOTATION_ACTION_H
#define EV_ANNOTATION_ACTION_H

#include <gtk/gtk.h>
#include <evince-document.h>
#include <evince-view.h>

G_BEGIN_DECLS

#define EV_TYPE_ANNOTATION_ACTION            (ev_annotation_action_get_type ())
#define EV_ANNOTATION_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_ANNOTATION_ACTION, EvAnnotationAction))
#define EV_IS_ANNOTATION_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_ANNOTATION_ACTION))
#define EV_ANNOTATION_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_ANNOTATION_ACTION, EvAnnotationActionClass))
#define EV_IS_ANNOTATION_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EV_TYPE_ANNOTATION_ACTION))
#define EV_ANNOTATION_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EV_TYPE_ANNOTATION_ACTION, EvAnnotationActionClass))

typedef struct _EvAnnotationAction        EvAnnotationAction;
typedef struct _EvAnnotationActionClass   EvAnnotationActionClass;

struct _EvAnnotationAction {
        GtkBox parent;
};

struct _EvAnnotationActionClass {
        GtkBoxClass parent_class;
};

typedef enum {
        EV_ANNOTATION_ACTION_TYPE_NOTE,
        EV_ANNOTATION_ACTION_TYPE_HIGHLIGHT
} EvAnnotationActionType;

GType      ev_annotation_action_get_type        (void);

GtkWidget *ev_annotation_action_new             (void);
void       ev_annotation_action_select_annotation (EvAnnotationAction     *annotation_action,
                                                   EvAnnotationActionType  annot_type);
G_END_DECLS

#endif
