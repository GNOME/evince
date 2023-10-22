/* ev-annotation-window.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2009 Carlos Garcia Campos <carlosgc@gnome.org>
 * Copyright (C) 2007 Iñigo Martinez <inigomartinez@gmail.com>
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

#include <gtk/gtk.h>

#include "ev-annotation.h"

G_BEGIN_DECLS

typedef struct _EvAnnotationWindow      EvAnnotationWindow;
typedef struct _EvAnnotationWindowClass EvAnnotationWindowClass;

#define EV_TYPE_ANNOTATION_WINDOW              (ev_annotation_window_get_type())
#define EV_ANNOTATION_WINDOW(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_ANNOTATION_WINDOW, EvAnnotationWindow))
#define EV_ANNOTATION_WINDOW_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_ANNOTATION_WINDOW, EvAnnotationWindowClass))
#define EV_IS_ANNOTATION_WINDOW(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_ANNOTATION_WINDOW))
#define EV_IS_ANNOTATION_WINDOW_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_ANNOTATION_WINDOW))
#define EV_ANNOTATION_WINDOW_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_ANNOTATION_WINDOW, EvAnnotationWindowClass))

GType         ev_annotation_window_get_type       (void) G_GNUC_CONST;
GtkWidget    *ev_annotation_window_new            (EvAnnotation       *annot,
						   GtkWindow          *parent);
EvAnnotation *ev_annotation_window_get_annotation (EvAnnotationWindow *window);
gboolean      ev_annotation_window_is_open        (EvAnnotationWindow *window);
void          ev_annotation_window_get_rectangle  (EvAnnotationWindow *window,
						   EvRectangle        *rect);
void          ev_annotation_window_set_rectangle  (EvAnnotationWindow *window,
						   const EvRectangle  *rect);
void          ev_annotation_window_set_enable_spellchecking (EvAnnotationWindow *window,
                                                             gboolean spellcheck);
gboolean      ev_annotation_window_get_enable_spellchecking (EvAnnotationWindow *window);

G_END_DECLS
