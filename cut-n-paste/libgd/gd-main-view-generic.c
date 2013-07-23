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

#include "gd-main-view-generic.h"

enum {
  VIEW_SELECTION_CHANGED,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };

typedef GdMainViewGenericIface GdMainViewGenericInterface;
G_DEFINE_INTERFACE (GdMainViewGeneric, gd_main_view_generic, GTK_TYPE_WIDGET)

static void
gd_main_view_generic_default_init (GdMainViewGenericInterface *iface)
{
  signals[VIEW_SELECTION_CHANGED] =
    g_signal_new ("view-selection-changed",
                  GD_TYPE_MAIN_VIEW_GENERIC,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

/**
 * gd_main_view_generic_set_model:
 * @self:
 * @model: (allow-none):
 *
 */
void
gd_main_view_generic_set_model (GdMainViewGeneric *self,
                                GtkTreeModel *model)
{
  GdMainViewGenericInterface *iface;

  iface = GD_MAIN_VIEW_GENERIC_GET_IFACE (self);

  (* iface->set_model) (self, model);
}

GtkTreePath *
gd_main_view_generic_get_path_at_pos (GdMainViewGeneric *self,
                                      gint x,
                                      gint y)
{
  GdMainViewGenericInterface *iface;

  iface = GD_MAIN_VIEW_GENERIC_GET_IFACE (self);

  return (* iface->get_path_at_pos) (self, x, y);
}

void
gd_main_view_generic_set_selection_mode (GdMainViewGeneric *self,
                                         gboolean selection_mode)
{
  GdMainViewGenericInterface *iface;

  iface = GD_MAIN_VIEW_GENERIC_GET_IFACE (self);

  (* iface->set_selection_mode) (self, selection_mode);
}


typedef struct {
  GtkTreePath *rubberband_start;
  GtkTreePath *rubberband_end;
} RubberbandInfo;

static void
rubber_band_info_destroy (RubberbandInfo *info)
{
  g_clear_pointer (&info->rubberband_start,
		   gtk_tree_path_free);
  g_clear_pointer (&info->rubberband_end,
		   gtk_tree_path_free);
  g_slice_free (RubberbandInfo, info);
}

static RubberbandInfo*
get_rubber_band_info (GdMainViewGeneric *self)
{
  RubberbandInfo *info;

  info = g_object_get_data (G_OBJECT (self), "gd-main-view-generic-rubber-band");
  if (info == NULL)
    {
      info = g_slice_new0 (RubberbandInfo);
      g_object_set_data_full (G_OBJECT (self), "gd-main-view-generic-rubber-band",
			      info, (GDestroyNotify)rubber_band_info_destroy);
    }

  return info;
}

void
gd_main_view_generic_set_rubberband_range (GdMainViewGeneric *self,
					   GtkTreePath *start,
					   GtkTreePath *end)
{
  RubberbandInfo *info;

  info = get_rubber_band_info (self);

  if (start == NULL || end == NULL)
    {
      g_clear_pointer (&info->rubberband_start,
		       gtk_tree_path_free);
      g_clear_pointer (&info->rubberband_end,
		       gtk_tree_path_free);
    }
  else
    {
      if (gtk_tree_path_compare (start, end) < 0)
	{
	  info->rubberband_start = gtk_tree_path_copy (start);
	  info->rubberband_end = gtk_tree_path_copy (end);
	}
      else
	{
	  info->rubberband_start = gtk_tree_path_copy (end);
	  info->rubberband_end = gtk_tree_path_copy (start);
	}
    }

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
_gd_main_view_generic_get_rubberband_range (GdMainViewGeneric *self,
					    GtkTreePath **start,
					    GtkTreePath **end)
{
  RubberbandInfo *info;

  info = get_rubber_band_info (self);

  *start = info->rubberband_start;
  *end = info->rubberband_end;
}

void
gd_main_view_generic_scroll_to_path (GdMainViewGeneric *self,
                                     GtkTreePath *path)
{
  GdMainViewGenericInterface *iface;

  iface = GD_MAIN_VIEW_GENERIC_GET_IFACE (self);

  (* iface->scroll_to_path) (self, path);
}

/**
 * gd_main_view_generic_get_model:
 *
 * Returns: (transfer none): The associated model
 */
GtkTreeModel *
gd_main_view_generic_get_model (GdMainViewGeneric *self)
{
  GdMainViewGenericInterface *iface;

  iface = GD_MAIN_VIEW_GENERIC_GET_IFACE (self);

  return (* iface->get_model) (self);
}

static gboolean
build_selection_uris_foreach (GtkTreeModel *model,
                              GtkTreePath *path,
                              GtkTreeIter *iter,
                              gpointer user_data)
{
  GPtrArray *ptr_array = user_data;
  gchar *uri;
  gboolean is_selected;

  gtk_tree_model_get (model, iter,
                      GD_MAIN_COLUMN_URI, &uri,
                      GD_MAIN_COLUMN_SELECTED, &is_selected,
                      -1);

  if (is_selected)
    g_ptr_array_add (ptr_array, uri);
  else
    g_free (uri);

  return FALSE;
}

static gchar **
model_get_selection_uris (GtkTreeModel *model)
{
  GPtrArray *ptr_array = g_ptr_array_new ();

  gtk_tree_model_foreach (model,
                          build_selection_uris_foreach,
                          ptr_array);
  
  g_ptr_array_add (ptr_array, NULL);
  return (gchar **) g_ptr_array_free (ptr_array, FALSE);
}

static gboolean
set_selection_foreach (GtkTreeModel *model,
                       GtkTreePath *path,
                       GtkTreeIter *iter,
                       gpointer user_data)
{
  gboolean selection = GPOINTER_TO_INT (user_data);

  gtk_list_store_set (GTK_LIST_STORE (model), iter,
                      GD_MAIN_COLUMN_SELECTED, selection,
                      -1);

  return FALSE;
}

static void
set_all_selection (GdMainViewGeneric *self,
                   GtkTreeModel *model,
                   gboolean selection)
{
  GtkTreeModel *actual_model;

  if (!model)
    return;

  if (GTK_IS_TREE_MODEL_FILTER (model))
    actual_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
  else
    actual_model = model;

  gtk_tree_model_foreach (actual_model,
                          set_selection_foreach,
                          GINT_TO_POINTER (selection));
  g_signal_emit (self, signals[VIEW_SELECTION_CHANGED], 0);
}

void
gd_main_view_generic_select_all (GdMainViewGeneric *self)
{
  GtkTreeModel *model = gd_main_view_generic_get_model (self);

  set_all_selection (self, model, TRUE);
}

void
gd_main_view_generic_unselect_all (GdMainViewGeneric *self)
{
  GtkTreeModel *model = gd_main_view_generic_get_model (self);

  set_all_selection (self, model, FALSE);
}

void
_gd_main_view_generic_dnd_common (GtkTreeModel *model,
                                  gboolean selection_mode,
                                  GtkTreePath *path,
                                  GtkSelectionData *data)
{
  gchar **uris;

  if (selection_mode)
    {
      uris = model_get_selection_uris (model);
    }
  else
    {
      GtkTreeIter iter;
      gboolean res;
      gchar *uri = NULL;

      if (path != NULL)
        {
          res = gtk_tree_model_get_iter (model, &iter, path);
          if (res)
            gtk_tree_model_get (model, &iter,
                                GD_MAIN_COLUMN_URI, &uri,
                                -1);
        }

      uris = g_new0 (gchar *, 2);
      uris[0] = uri;
      uris[1] = NULL;
    }

  gtk_selection_data_set_uris (data, uris);
  g_strfreev (uris);
}
