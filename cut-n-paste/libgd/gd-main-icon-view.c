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

#include "gd-main-icon-view.h"
#include "gd-main-view-generic.h"
#include "gd-toggle-pixbuf-renderer.h"
#include "gd-two-lines-renderer.h"

#include <math.h>
#include <glib/gi18n.h>

#define VIEW_ITEM_WIDTH 140
#define VIEW_ITEM_WRAP_WIDTH 128
#define VIEW_COLUMN_SPACING 20
#define VIEW_MARGIN 16

struct _GdMainIconViewPrivate {
  GtkCellRenderer *pixbuf_cell;
  gboolean selection_mode;
};

static void gd_main_view_generic_iface_init (GdMainViewGenericIface *iface);
G_DEFINE_TYPE_WITH_CODE (GdMainIconView, gd_main_icon_view, GTK_TYPE_ICON_VIEW,
                         G_IMPLEMENT_INTERFACE (GD_TYPE_MAIN_VIEW_GENERIC,
                                                gd_main_view_generic_iface_init))

static GtkTreePath*
get_source_row (GdkDragContext *context)
{
  GtkTreeRowReference *ref;

  ref = g_object_get_data (G_OBJECT (context), "gtk-icon-view-source-row");

  if (ref)
    return gtk_tree_row_reference_get_path (ref);
  else
    return NULL;
}

static void
gd_main_icon_view_drag_data_get (GtkWidget *widget,
                                 GdkDragContext *drag_context,
                                 GtkSelectionData *data,
                                 guint info,
                                 guint time)
{
  GdMainIconView *self = GD_MAIN_ICON_VIEW (widget);
  GtkTreeModel *model = gtk_icon_view_get_model (GTK_ICON_VIEW (self));

  if (info != 0)
    return;

  _gd_main_view_generic_dnd_common (model, self->priv->selection_mode,
                                    get_source_row (drag_context), data);

  GTK_WIDGET_CLASS (gd_main_icon_view_parent_class)->drag_data_get (widget, drag_context,
                                                                    data, info, time);
}

static void
gd_main_icon_view_constructed (GObject *obj)
{
  GdMainIconView *self = GD_MAIN_ICON_VIEW (obj);
  GtkCellRenderer *cell;
  const GtkTargetEntry targets[] = {
    { (char *) "text/uri-list", GTK_TARGET_OTHER_APP, 0 }
  };

  G_OBJECT_CLASS (gd_main_icon_view_parent_class)->constructed (obj);

  gtk_widget_set_hexpand (GTK_WIDGET (self), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (self), TRUE);
  gtk_icon_view_set_selection_mode (GTK_ICON_VIEW (self), GTK_SELECTION_NONE);

  g_object_set (self,
                "column-spacing", VIEW_COLUMN_SPACING,
                "margin", VIEW_MARGIN,
                NULL);

  self->priv->pixbuf_cell = cell = gd_toggle_pixbuf_renderer_new ();
  g_object_set (cell,
                "xalign", 0.5,
                "yalign", 0.5,
                NULL);

  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (self), cell, FALSE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (self), cell,
                                 "active", GD_MAIN_COLUMN_SELECTED);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (self), cell,
                                 "pixbuf", GD_MAIN_COLUMN_ICON);

  cell = gd_two_lines_renderer_new ();
  g_object_set (cell,
                "xalign", 0.5,
                "alignment", PANGO_ALIGN_CENTER,
                "wrap-mode", PANGO_WRAP_WORD_CHAR,
                "wrap-width", VIEW_ITEM_WRAP_WIDTH,
                "text-lines", 3,
                NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (self), cell, FALSE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (self), cell,
                                 "text", GD_MAIN_COLUMN_PRIMARY_TEXT);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (self), cell,
                                 "line-two", GD_MAIN_COLUMN_SECONDARY_TEXT);

  gtk_icon_view_enable_model_drag_source (GTK_ICON_VIEW (self),
                                          GDK_BUTTON1_MASK,
                                          targets, 1,
                                          GDK_ACTION_COPY);
}

static void
path_from_line_rects (cairo_t *cr,
		      GdkRectangle *lines,
		      int n_lines)
{
  int start_line, end_line;
  GdkRectangle *r;
  int i;

  /* Join rows vertically by extending to the middle */
  for (i = 0; i < n_lines - 1; i++)
    {
      GdkRectangle *r1 = &lines[i];
      GdkRectangle *r2 = &lines[i+1];
      int gap = r2->y - (r1->y + r1->height);
      int old_y;

      r1->height += gap / 2;
      old_y = r2->y;
      r2->y = r1->y + r1->height;
      r2->height += old_y - r2->y;
    }

  cairo_new_path (cr);
  start_line = 0;

  do
    {
      for (i = start_line; i < n_lines; i++)
	{
	  r = &lines[i];
	  if (i == start_line)
	    cairo_move_to (cr, r->x + r->width, r->y);
	  else
	    cairo_line_to (cr, r->x + r->width, r->y);
	  cairo_line_to (cr, r->x + r->width, r->y + r->height);

	  if (i < n_lines - 1 &&
	      (r->x + r->width < lines[i+1].x ||
	       r->x > lines[i+1].x + lines[i+1].width))
	    {
	      i++;
	      break;
	    }
	}
      end_line = i;
      for (i = end_line - 1; i >= start_line; i--)
	{
	  r = &lines[i];
	  cairo_line_to (cr, r->x, r->y + r->height);
	  cairo_line_to (cr, r->x, r->y);
	}
      cairo_close_path (cr);
      start_line = end_line;
    }
  while (end_line < n_lines);
}

static gboolean
gd_main_icon_view_draw (GtkWidget *widget,
			cairo_t   *cr)
{
  GdMainIconView *self = GD_MAIN_ICON_VIEW (widget);
  GtkAllocation allocation;
  GtkStyleContext *context;
  GdkRectangle line_rect;
  GdkRectangle rect;
  GtkTreePath *path;
  GArray *lines;
  GtkTreePath *rubberband_start, *rubberband_end;

  GTK_WIDGET_CLASS (gd_main_icon_view_parent_class)->draw (widget, cr);

  _gd_main_view_generic_get_rubberband_range (GD_MAIN_VIEW_GENERIC (self),
					      &rubberband_start, &rubberband_end);

  if (rubberband_start)
    {
      cairo_save (cr);

      context = gtk_widget_get_style_context (widget);

      gtk_style_context_save (context);
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_RUBBERBAND);

      path = gtk_tree_path_copy (rubberband_start);

      line_rect.width = 0;
      lines = g_array_new (FALSE, FALSE, sizeof (GdkRectangle));

      while (gtk_tree_path_compare (path, rubberband_end) <= 0)
	{
	  if (gtk_icon_view_get_cell_rect (GTK_ICON_VIEW (widget),
					   path,
					   NULL, &rect))
	    {
	      if (line_rect.width == 0)
		line_rect = rect;
	      else
		{
		  if (rect.y == line_rect.y)
		    gdk_rectangle_union (&rect, &line_rect, &line_rect);
		  else
		    {
		      g_array_append_val (lines, line_rect);
		      line_rect = rect;
		    }
		}
	    }
	  gtk_tree_path_next (path);
	}

      if (line_rect.width != 0)
	g_array_append_val (lines, line_rect);

      if (lines->len > 0)
	{
	  GtkStateFlags state;
	  cairo_path_t *path;
	  GtkBorder border;
	  GdkRGBA border_color;

	  path_from_line_rects (cr, (GdkRectangle *)lines->data, lines->len);

	  /* For some reason we need to copy and reapply the path, or it gets
	     eaten by gtk_render_background() */
	  path = cairo_copy_path (cr);

	  cairo_save (cr);
	  cairo_clip (cr);
	  gtk_widget_get_allocation (widget, &allocation);
	  gtk_render_background (context, cr,
				 0, 0,
				 allocation.width, allocation.height);
	  cairo_restore (cr);

	  cairo_append_path (cr, path);
	  cairo_path_destroy (path);

	  state = gtk_widget_get_state_flags (widget);
	  gtk_style_context_get_border_color (context,
					      state,
					      &border_color);
	  gtk_style_context_get_border (context, state,
					&border);

	  cairo_set_line_width (cr, border.left);
	  gdk_cairo_set_source_rgba (cr, &border_color);
	  cairo_stroke (cr);
	}
      g_array_free (lines, TRUE);

      gtk_tree_path_free (path);

      gtk_style_context_restore (context);
      cairo_restore (cr);
    }

  return FALSE;
}

static void
gd_main_icon_view_class_init (GdMainIconViewClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);
  GtkBindingSet *binding_set;
  GdkModifierType activate_modifiers[] = { GDK_SHIFT_MASK, GDK_CONTROL_MASK, GDK_SHIFT_MASK | GDK_CONTROL_MASK };
  int i;

  binding_set = gtk_binding_set_by_class (klass);

  oclass->constructed = gd_main_icon_view_constructed;
  wclass->drag_data_get = gd_main_icon_view_drag_data_get;
  wclass->draw = gd_main_icon_view_draw;

  gtk_widget_class_install_style_property (wclass,
                                           g_param_spec_int ("check-icon-size",
                                                             "Check icon size",
                                                             "Check icon size",
                                                             -1, G_MAXINT, 40,
                                                             G_PARAM_READWRITE));

  g_type_class_add_private (klass, sizeof (GdMainIconViewPrivate));


  for (i = 0; i < G_N_ELEMENTS (activate_modifiers); i++)
    {
      gtk_binding_entry_add_signal (binding_set, GDK_KEY_space, activate_modifiers[i],
				    "activate-cursor-item", 0);
      gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Space, activate_modifiers[i],
				    "activate-cursor-item", 0);
      gtk_binding_entry_add_signal (binding_set, GDK_KEY_Return, activate_modifiers[i],
				    "activate-cursor-item", 0);
      gtk_binding_entry_add_signal (binding_set, GDK_KEY_ISO_Enter, activate_modifiers[i],
				    "activate-cursor-item", 0);
      gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Enter, activate_modifiers[i],
				    "activate-cursor-item", 0);
    }
}

static void
gd_main_icon_view_init (GdMainIconView *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GD_TYPE_MAIN_ICON_VIEW, GdMainIconViewPrivate);
}

static GtkTreePath *
gd_main_icon_view_get_path_at_pos (GdMainViewGeneric *mv,
                                   gint x,
                                   gint y)
{
  return gtk_icon_view_get_path_at_pos (GTK_ICON_VIEW (mv), x, y);
}

static void
gd_main_icon_view_set_selection_mode (GdMainViewGeneric *mv,
                                      gboolean selection_mode)
{
  GdMainIconView *self = GD_MAIN_ICON_VIEW (mv);

  self->priv->selection_mode = selection_mode;

  g_object_set (self->priv->pixbuf_cell,
                "toggle-visible", selection_mode,
                NULL);
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
gd_main_icon_view_scroll_to_path (GdMainViewGeneric *mv,
                                  GtkTreePath *path)
{
  gtk_icon_view_scroll_to_path (GTK_ICON_VIEW (mv), path, TRUE, 0.5, 0.5);
}

static void
gd_main_icon_view_set_model (GdMainViewGeneric *mv,
                             GtkTreeModel *model)
{
  gtk_icon_view_set_model (GTK_ICON_VIEW (mv), model);
}

static GtkTreeModel *
gd_main_icon_view_get_model (GdMainViewGeneric *mv)
{
  return gtk_icon_view_get_model (GTK_ICON_VIEW (mv));
}

static void
gd_main_view_generic_iface_init (GdMainViewGenericIface *iface)
{
  iface->set_model = gd_main_icon_view_set_model;
  iface->get_model = gd_main_icon_view_get_model;
  iface->get_path_at_pos = gd_main_icon_view_get_path_at_pos;
  iface->scroll_to_path = gd_main_icon_view_scroll_to_path;
  iface->set_selection_mode = gd_main_icon_view_set_selection_mode;
}

GtkWidget *
gd_main_icon_view_new (void)
{
  return g_object_new (GD_TYPE_MAIN_ICON_VIEW, NULL);
}
