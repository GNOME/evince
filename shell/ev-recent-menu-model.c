/*
 * Copyright 2014 Canonical Ltd
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
 *
 * Author: Lars Uebernickel <lars.uebernickel@canonical.com>
 */

#include "ev-recent-menu-model.h"

typedef struct
{
  GMenu *menu;
  GtkRecentManager *manager;
  gchar *action_name;
  gchar *application;
} EvRecentMenuModel;

static void
ev_recent_menu_model_update (GtkRecentManager *manager,
                             gpointer          data)
{
  EvRecentMenuModel *recent_menu = data;
  GList *items, *it;
  guint n_items = 0;

  g_menu_remove_all (recent_menu->menu);

  items = gtk_recent_manager_get_items (recent_menu->manager);

  for (it = items; it && n_items < 5; it = it->next)
    {
      GtkRecentInfo *info = it->data;
      gchar *label;
      GIcon *icon;
      GMenuItem *item;

      if (!gtk_recent_info_has_application (info, recent_menu->application))
        continue;

      label = g_strdup_printf ("_%d. %s", n_items + 1, gtk_recent_info_get_display_name (info));

      item = g_menu_item_new (label, NULL);
      g_menu_item_set_action_and_target (item, recent_menu->action_name,
                                         "s", gtk_recent_info_get_uri (info));

      icon = gtk_recent_info_get_gicon (info);
      if (icon)
        {
          g_menu_item_set_icon (item, icon);
          g_object_unref (icon);
        }

      g_menu_append_item (recent_menu->menu, item);

      g_free (label);
      g_object_unref (item);

      n_items++;
    }

  g_list_free_full (items, (GDestroyNotify) gtk_recent_info_unref);
}

static void
ev_recent_menu_model_destroy (gpointer  data,
                              GObject  *menu)
{
  EvRecentMenuModel *recent_menu = data;

  g_signal_handlers_disconnect_by_func (recent_menu->manager, ev_recent_menu_model_update, recent_menu);
  g_object_unref (recent_menu->manager);
  g_free (recent_menu->application);
  g_free (recent_menu->action_name);

  g_slice_free (EvRecentMenuModel, recent_menu);
}

GMenuModel *
ev_recent_menu_model_new (GtkRecentManager *manager,
                          const gchar      *action_name,
                          const gchar      *application)
{
  EvRecentMenuModel *recent_menu;

  recent_menu = g_slice_new0 (EvRecentMenuModel);
  recent_menu->menu = g_menu_new ();
  recent_menu->manager = g_object_ref (manager);
  recent_menu->action_name = g_strdup (action_name);
  recent_menu->application = g_strdup (application);

  g_object_weak_ref (G_OBJECT (recent_menu->menu), ev_recent_menu_model_destroy, recent_menu);
  g_signal_connect (manager, "changed", G_CALLBACK (ev_recent_menu_model_update), recent_menu);

  return G_MENU_MODEL (recent_menu->menu);
}
