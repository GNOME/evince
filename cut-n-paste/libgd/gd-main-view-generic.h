/*
 * Copyright (c) 2011 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by 
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public 
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License 
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#ifndef __GD_MAIN_VIEW_GENERIC_H__
#define __GD_MAIN_VIEW_GENERIC_H__

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum {
  GD_MAIN_COLUMN_ID,
  GD_MAIN_COLUMN_URI,
  GD_MAIN_COLUMN_PRIMARY_TEXT,
  GD_MAIN_COLUMN_SECONDARY_TEXT,
  GD_MAIN_COLUMN_ICON,
  GD_MAIN_COLUMN_MTIME,
  GD_MAIN_COLUMN_SELECTED,

  GD_MAIN_COLUMN_LAST
} GdMainColumns;

#define GD_TYPE_MAIN_VIEW_GENERIC gd_main_view_generic_get_type()

#define GD_MAIN_VIEW_GENERIC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   GD_TYPE_MAIN_VIEW_GENERIC, GdMainViewGeneric))

#define GD_MAIN_VIEW_GENERIC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   GD_TYPE_MAIN_VIEW_GENERIC, GdMainViewGenericIface))

#define GD_IS_MAIN_VIEW_GENERIC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   GD_TYPE_MAIN_VIEW_GENERIC))

#define GD_IS_MAIN_VIEW_GENERIC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   GD_TYPE_MAIN_VIEW_GENERIC))

#define GD_MAIN_VIEW_GENERIC_GET_IFACE(obj) \
  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), \
   GD_TYPE_MAIN_VIEW_GENERIC, GdMainViewGenericIface))

typedef struct _GdMainViewGeneric GdMainViewGeneric;
typedef struct _GdMainViewGenericIface GdMainViewGenericIface;

struct _GdMainViewGenericIface
{
  GTypeInterface base_iface;

  /* vtable */
  void          (* set_model)            (GdMainViewGeneric  *self,
                                          GtkTreeModel       *model);
  GtkTreeModel * (* get_model)           (GdMainViewGeneric *self);

  GtkTreePath * (* get_path_at_pos)      (GdMainViewGeneric *self,
                                          gint               x, 
                                          gint               y);
  void          (* scroll_to_path)       (GdMainViewGeneric *self,
                                          GtkTreePath       *path);
  void          (* set_selection_mode)   (GdMainViewGeneric *self,
                                          gboolean           selection_mode);
};

GType gd_main_view_generic_get_type (void) G_GNUC_CONST;

void gd_main_view_generic_set_model (GdMainViewGeneric *self,
                                     GtkTreeModel *model);
GtkTreeModel * gd_main_view_generic_get_model (GdMainViewGeneric *self);

void gd_main_view_generic_scroll_to_path (GdMainViewGeneric *self,
                                          GtkTreePath *path);
void gd_main_view_generic_set_selection_mode (GdMainViewGeneric *self,
                                              gboolean selection_mode);
GtkTreePath * gd_main_view_generic_get_path_at_pos (GdMainViewGeneric *self,
                                                    gint x,
                                                    gint y);
void gd_main_view_generic_select_all (GdMainViewGeneric *self);
void gd_main_view_generic_unselect_all (GdMainViewGeneric *self);
void gd_main_view_generic_set_rubberband_range (GdMainViewGeneric *self,
						GtkTreePath *start,
						GtkTreePath *end);

/* private */
void _gd_main_view_generic_dnd_common (GtkTreeModel *model,
                                       gboolean selection_mode,
                                       GtkTreePath *path,
                                       GtkSelectionData *data);
void _gd_main_view_generic_get_rubberband_range (GdMainViewGeneric *self,
						 GtkTreePath **start,
						 GtkTreePath **end);

G_END_DECLS

#endif /* __GD_MAIN_VIEW_GENERIC_H__ */
