/* ev-annotation-properties-dialog.h
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

#include <evince-document.h>

G_BEGIN_DECLS

#define EV_TYPE_ANNOTATION_PROPERTIES_DIALOG         (ev_annotation_properties_dialog_get_type())
#define EV_ANNOTATION_PROPERTIES_DIALOG(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), EV_TYPE_ANNOTATION_PROPERTIES_DIALOG, EvAnnotationPropertiesDialog))
#define EV_ANNOTATION_PROPERTIES_DIALOG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), EV_TYPE_ANNOTATION_PROPERTIES_DIALOG, EvAnnotationPropertiesDialogClass))
#define EV_IS_ANNOTATION_PROPERTIES_DIALOG(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), EV_TYPE_ANNOTATION_PROPERTIES_DIALOG))
#define EV_IS_ANNOTATION_PROPERTIES_DIALOG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), EV_TYPE_ANNOTATION_PROPERTIES_DIALOG))
#define EV_ANNOTATION_PROPERTIES_DIALOG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), EV_TYPE_ANNOTATION_PROPERTIES_DIALOG, EvAnnotationPropertiesDialogClass))

typedef struct _EvAnnotationPropertiesDialog      EvAnnotationPropertiesDialog;
typedef struct _EvAnnotationPropertiesDialogClass EvAnnotationPropertiesDialogClass;

GType                      ev_annotation_properties_dialog_get_type             (void) G_GNUC_CONST;
GtkWidget                 *ev_annotation_properties_dialog_new                  (EvAnnotationType              annot_type);
GtkWidget                 *ev_annotation_properties_dialog_new_with_annotation  (EvAnnotation                 *annot);

const gchar               *ev_annotation_properties_dialog_get_author           (EvAnnotationPropertiesDialog *dialog);
void                       ev_annotation_properties_dialog_get_rgba             (EvAnnotationPropertiesDialog *dialog,
                                                                                 GdkRGBA                      *rgba);
gdouble                    ev_annotation_properties_dialog_get_opacity          (EvAnnotationPropertiesDialog *dialog);
gboolean                   ev_annotation_properties_dialog_get_popup_is_open    (EvAnnotationPropertiesDialog *dialog);
EvAnnotationTextIcon       ev_annotation_properties_dialog_get_text_icon        (EvAnnotationPropertiesDialog *dialog);
EvAnnotationTextMarkupType ev_annotation_properties_dialog_get_text_markup_type (EvAnnotationPropertiesDialog *dialog);

G_END_DECLS
