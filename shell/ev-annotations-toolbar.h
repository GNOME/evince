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

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define EV_TYPE_ANNOTATIONS_TOOLBAR              (ev_annotations_toolbar_get_type())
G_DECLARE_FINAL_TYPE (EvAnnotationsToolbar, ev_annotations_toolbar, EV, ANNOTATIONS_TOOLBAR, GtkBox);

GtkWidget *ev_annotations_toolbar_new                (void);
void       ev_annotations_toolbar_add_annot_finished (EvAnnotationsToolbar *toolbar);

G_END_DECLS
