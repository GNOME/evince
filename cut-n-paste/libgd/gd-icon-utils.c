/*
 * Copyright (c) 2011, 2012 Red Hat, Inc.
 *
 * Gnome Documents is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * Gnome Documents is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with Gnome Documents; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include "gd-icon-utils.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <string.h>
#include <math.h>

#define _BG_MIN_SIZE 20
#define _EMBLEM_MIN_SIZE 8

/**
 * gd_create_symbolic_icon:
 * @name:
 *
 * Returns: (transfer full):
 */
GIcon *
gd_create_symbolic_icon (const gchar *name,
                         gint base_size)
{
  gchar *symbolic_name;
  GIcon *icon, *retval = NULL;
  cairo_surface_t *surface;
  cairo_t *cr;
  GtkStyleContext *style;
  GtkWidgetPath *path;
  GdkPixbuf *pixbuf;
  GtkIconTheme *theme;
  GtkIconInfo *info;
  gint bg_size;
  gint emblem_size;
  gint total_size;

  total_size = base_size / 2;
  bg_size = MAX (total_size / 2, _BG_MIN_SIZE);
  emblem_size = MAX (bg_size - 8, _EMBLEM_MIN_SIZE);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, total_size, total_size);
  cr = cairo_create (surface);

  style = gtk_style_context_new ();

  path = gtk_widget_path_new ();
  gtk_widget_path_append_type (path, GTK_TYPE_ICON_VIEW);
  gtk_style_context_set_path (style, path);
  gtk_widget_path_unref (path);

  gtk_style_context_add_class (style, "documents-icon-bg");

  gtk_render_background (style, cr, (total_size - bg_size) / 2, (total_size - bg_size) / 2, bg_size, bg_size);

  symbolic_name = g_strconcat (name, "-symbolic", NULL);
  icon = g_themed_icon_new_with_default_fallbacks (symbolic_name);
  g_free (symbolic_name);

  theme = gtk_icon_theme_get_default();
  info = gtk_icon_theme_lookup_by_gicon (theme, icon, emblem_size,
                                         GTK_ICON_LOOKUP_FORCE_SIZE);
  g_object_unref (icon);

  if (info == NULL)
    goto out;

  pixbuf = gtk_icon_info_load_symbolic_for_context (info, style, NULL, NULL);
  g_object_unref (info);

  if (pixbuf == NULL)
    goto out;

  gtk_render_icon (style, cr, pixbuf, (total_size - emblem_size) / 2,  (total_size - emblem_size) / 2);
  g_object_unref (pixbuf);

  retval = G_ICON (gdk_pixbuf_get_from_surface (surface, 0, 0, total_size, total_size));

 out:
  g_object_unref (style);
  cairo_surface_destroy (surface);
  cairo_destroy (cr);

  return retval;
}

/**
 * gd_embed_image_in_frame:
 * @source_image:
 * @frame_image_url:
 * @slice_width:
 * @border_width:
 *
 * Returns: (transfer full):
 */
cairo_surface_t *
gd_embed_image_in_frame (cairo_surface_t *source_image,
                         const gchar *frame_image_url,
                         GtkBorder *slice_width,
                         GtkBorder *border_width)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  int source_width, source_height;
  int dest_width, dest_height;
  gchar *css_str;
  GtkCssProvider *provider;
  GtkStyleContext *context;
  GError *error = NULL;
  GdkPixbuf *retval;
  GtkWidgetPath *path;

  source_width = cairo_image_surface_get_width (source_image);
  source_height = cairo_image_surface_get_height (source_image);

  dest_width = source_width +  border_width->left + border_width->right;
  dest_height = source_height + border_width->top + border_width->bottom;

  css_str = g_strdup_printf (".embedded-image { border-image: url(\"%s\") %d %d %d %d / %dpx %dpx %dpx %dpx }",
                             frame_image_url,
                             slice_width->top, slice_width->right, slice_width->bottom, slice_width->left,
                             border_width->top, border_width->right, border_width->bottom, border_width->left);
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css_str, -1, &error);

  if (error != NULL)
    {
      g_warning ("Unable to create the thumbnail frame image: %s", error->message);
      g_error_free (error);
      g_free (css_str);

      return cairo_surface_reference (source_image);
    }

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, dest_width, dest_height);
  cr = cairo_create (surface);

  context = gtk_style_context_new ();
  path = gtk_widget_path_new ();
  gtk_widget_path_append_type (path, GTK_TYPE_ICON_VIEW);

  gtk_style_context_set_path (context, path);
  gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (provider), 600);

  gtk_render_icon_surface (context, cr,
                           source_image,
                           border_width->left, border_width->top);

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "embedded-image");

  gtk_render_frame (context, cr,
                    0, 0,
                    dest_width, dest_height);

  gtk_style_context_restore (context);
  cairo_destroy (cr);

  gtk_widget_path_unref (path);
  g_object_unref (provider);
  g_object_unref (context);
  g_free (css_str);

  return surface;
}
