/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2004 Red Hat, Inc
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

#include <math.h>
#include <config.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <libgail-util/gail-util.h>

#include "ev-selection.h"
#include "ev-page-cache.h"
#include "ev-view-accessible.h"
#include "ev-view-private.h"

#define EV_TYPE_VIEW_ACCESSIBLE      (ev_view_accessible_get_type ())
#define EV_VIEW_ACCESSIBLE(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_VIEW_ACCESSIBLE, EvViewAccessible))
#define EV_IS_VIEW_ACCESSIBLE(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_VIEW_ACCESSIBLE))

static GType ev_view_accessible_get_type (void);

enum {
	ACTION_SCROLL_UP,
	ACTION_SCROLL_DOWN,
	LAST_ACTION
};

static const gchar *const ev_view_accessible_action_names[] =
{
	N_("Scroll Up"),
	N_("Scroll Down"),
	NULL
};

static const gchar *const ev_view_accessible_action_descriptions[] =
{
	N_("Scroll View Up"),
	N_("Scroll View Down"),
	NULL
};

typedef struct {
	/* Action */
	gchar *action_descriptions[LAST_ACTION];
	guint action_idle_handler;
	GtkScrollType idle_scroll;
	GtkTextBuffer *buffer;
	guint current_page;
} EvViewAccessiblePriv;

typedef GtkAccessibleClass EvViewAccessibleClass;
typedef GtkAccessible EvViewAccessible;

#define EV_VIEW_ACCESSIBLE_GET_PRIVATE(inst) (G_TYPE_INSTANCE_GET_PRIVATE ((inst), EV_TYPE_VIEW_ACCESSIBLE, EvViewAccessiblePriv))

static void
ev_view_accessible_finalize (GObject *object)
{
	EvViewAccessiblePriv *priv = EV_VIEW_ACCESSIBLE_GET_PRIVATE (object);
	int i;

	if (priv->action_idle_handler)
		g_source_remove (priv->action_idle_handler);
	for (i = 0; i < LAST_ACTION; i++)
		g_free (priv->action_descriptions [i]);
	if (priv->buffer)
		g_object_unref (priv->buffer);

}

static void ev_view_accessible_class_init (EvViewAccessibleClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = ev_view_accessible_finalize;

	g_type_class_add_private (klass, sizeof (EvViewAccessiblePriv));
}

static GtkTextBuffer *
ev_view_accessible_get_text_buffer (EvViewAccessible *accessible, EvView *view)
{
	EvPageCache *page_cache;
	const gchar *retval = NULL;
	EvViewAccessiblePriv* priv = EV_VIEW_ACCESSIBLE_GET_PRIVATE (accessible);

	page_cache = view->page_cache;
	if (!page_cache) {
		return NULL;
	}

	if (view->current_page == priv->current_page && priv->buffer) {
		return priv->buffer;
	}

	priv->current_page = view->current_page;

	if (!priv->buffer) {
		priv->buffer = gtk_text_buffer_new (NULL);
	}

	retval = ev_page_cache_get_text (page_cache, view->current_page);
	if (retval)
		gtk_text_buffer_set_text (priv->buffer, retval, -1);

	return priv->buffer;
}

static gchar*
ev_view_accessible_get_text (AtkText *text,
			     gint    start_pos,
			     gint    end_pos)
{
	GtkWidget *widget;
	GtkTextIter start, end;
	GtkTextBuffer *buffer;
	gchar *retval;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
	if (widget == NULL)
		/* State is defunct */
		return NULL;

	buffer = ev_view_accessible_get_text_buffer (EV_VIEW_ACCESSIBLE (text), EV_VIEW (widget));
	if (!buffer)
		return NULL;

	gtk_text_buffer_get_iter_at_offset (buffer, &start, start_pos);
	gtk_text_buffer_get_iter_at_offset (buffer, &end, end_pos);
	retval = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

	return retval;
}

static gunichar
ev_view_accessible_get_character_at_offset (AtkText *text,
					    gint    offset)
{
	GtkWidget *widget;
	GtkTextIter start, end;
	GtkTextBuffer *buffer;
	gchar *string;
	gunichar unichar;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
	if (widget == NULL)
		/* State is defunct */
		return '\0';

	buffer = ev_view_accessible_get_text_buffer (EV_VIEW_ACCESSIBLE (text), EV_VIEW (widget));
	if (!buffer)
		return '\0';

	if (offset >= gtk_text_buffer_get_char_count (buffer))
		return '\0';

	gtk_text_buffer_get_iter_at_offset (buffer, &start, offset);
	end = start;
	gtk_text_iter_forward_char (&end);
	string = gtk_text_buffer_get_slice (buffer, &start, &end, FALSE);
	unichar = g_utf8_get_char (string);
	g_free(string);

	return unichar;
}

static gchar*
ev_view_accessible_get_text_before_offset (AtkText	    *text,
					   gint		    offset,
					   AtkTextBoundary  boundary_type,
					   gint		    *start_offset,
					   gint		    *end_offset)
{
	GtkWidget *widget;
	gpointer layout = NULL;
	GailTextUtil *gail_text = NULL;
	gchar *retval = NULL;
	GtkTextBuffer *buffer;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
	if (widget == NULL)
		/* State is defunct */
		return NULL;

	buffer = ev_view_accessible_get_text_buffer (EV_VIEW_ACCESSIBLE (text), EV_VIEW (widget));
	if (!buffer)
		return NULL;

	gail_text = gail_text_util_new ();
	gail_text_util_buffer_setup (gail_text, buffer);
	retval = gail_text_util_get_text (gail_text, layout,
	                                  GAIL_BEFORE_OFFSET, boundary_type,
	                                  offset, start_offset, end_offset);
	g_object_unref (gail_text);

	return retval;
}

static gchar*
ev_view_accessible_get_text_at_offset (AtkText          *text,
				       gint             offset,
				       AtkTextBoundary  boundary_type,
				       gint             *start_offset,
				       gint             *end_offset)
{
	GtkWidget *widget;
	gpointer layout = NULL;
	GailTextUtil *gail_text = NULL;
	gchar *retval = NULL;
	GtkTextBuffer *buffer;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
	if (widget == NULL)
		/* State is defunct */
		return NULL;

	buffer = ev_view_accessible_get_text_buffer (EV_VIEW_ACCESSIBLE (text), EV_VIEW (widget));
	if (!buffer)
		return NULL;

	gail_text = gail_text_util_new ();
	gail_text_util_buffer_setup (gail_text, buffer);
	retval = gail_text_util_get_text (gail_text, layout,
	                                  GAIL_AT_OFFSET, boundary_type,
	                                  offset, start_offset, end_offset);
	g_object_unref (gail_text);

	return retval;
}

static gchar*
ev_view_accessible_get_text_after_offset (AtkText	    *text,
					  gint		    offset,
					  AtkTextBoundary   boundary_type,
					  gint		    *start_offset,
					  gint		    *end_offset)
{
	GtkWidget *widget;
	gpointer layout = NULL;
	GailTextUtil *gail_text = NULL;
	gchar *retval = NULL;
	GtkTextBuffer *buffer;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
	if (widget == NULL)
		/* State is defunct */
		return NULL;

	buffer = ev_view_accessible_get_text_buffer (EV_VIEW_ACCESSIBLE (text), EV_VIEW (widget));
	if (!buffer)
		return NULL;

	gail_text = gail_text_util_new ();
	gail_text_util_buffer_setup (gail_text, buffer);
	retval = gail_text_util_get_text (gail_text, layout,
	                                  GAIL_AFTER_OFFSET, boundary_type,
	                                  offset, start_offset, end_offset);
	g_object_unref (gail_text);

	return retval;
}

static gint
ev_view_accessible_get_character_count (AtkText *text)
{
	GtkWidget *widget;
	GtkTextBuffer *buffer;
	gint retval;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
	if (widget == NULL)
		/* State is defunct */
		return 0;

	buffer = ev_view_accessible_get_text_buffer (EV_VIEW_ACCESSIBLE (text), EV_VIEW (widget));
	if (!buffer)
		return 0;

	retval = gtk_text_buffer_get_char_count (buffer);

	return retval;
}

static gint
ev_view_accessible_get_caret_offset (AtkText *text)
{
	GtkWidget *widget;
	GtkTextBuffer *buffer;
	GtkTextMark *cursor_mark;
	GtkTextIter cursor_itr;
	gint retval;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
	if (widget == NULL)
		/* State is defunct */
		return 0;

	buffer = ev_view_accessible_get_text_buffer (EV_VIEW_ACCESSIBLE (text), EV_VIEW (widget));
	if (!buffer)
		return 0;

	cursor_mark = gtk_text_buffer_get_insert (buffer);
	gtk_text_buffer_get_iter_at_mark (buffer, &cursor_itr, cursor_mark);
	retval = gtk_text_iter_get_offset (&cursor_itr);

	return retval;
}

static gboolean
ev_view_accessible_set_caret_offset (AtkText *text, gint offset)
{
	GtkWidget *widget;
	GtkTextBuffer *buffer;
	GtkTextIter pos_itr;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
	if (widget == NULL)
		/* State is defunct */
		return FALSE;

	buffer = ev_view_accessible_get_text_buffer (EV_VIEW_ACCESSIBLE (text), EV_VIEW (widget));
	if (!buffer)
		return FALSE;

	gtk_text_buffer_get_iter_at_offset (buffer, &pos_itr, offset);
	gtk_text_buffer_place_cursor (buffer, &pos_itr);

	return TRUE;
}

static AtkAttributeSet*
ev_view_accessible_get_run_attributes (AtkText *text,
				       gint    offset,
				       gint    *start_offset,
				       gint    *end_offset)
{
	GtkWidget *widget;
	GtkTextBuffer *buffer;
	AtkAttributeSet *retval;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
	if (widget == NULL)
		/* State is defunct */
		return NULL;

	buffer = ev_view_accessible_get_text_buffer (EV_VIEW_ACCESSIBLE (text), EV_VIEW (widget));
	if (!buffer)
		return NULL;

	retval = gail_misc_buffer_get_run_attributes (buffer, offset,
	                                              start_offset, end_offset);

	return retval;
}

static AtkAttributeSet*
ev_view_accessible_get_default_attributes (AtkText *text)
{
	GtkWidget *widget;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
	if (widget == NULL)
		/* State is defunct */
		return NULL;
	return NULL;
}

static void
ev_view_accessible_get_character_extents (AtkText      *text,
					  gint         offset,
					  gint         *x,
					  gint         *y,
					  gint         *width,
					  gint         *height,
					  AtkCoordType coords)
{
	GtkWidget *widget, *toplevel;
	EvRectangle *areas = NULL;
	EvRectangle *rect = NULL;
	guint n_areas = 0;
	EvPageCache *page_cache;
	gint x_widget, y_widget, x_window, y_window;
	gdouble scale;
	GtkBorder border;
	GdkRectangle page_area;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
	if (widget == NULL)
		/* State is defunct */
		return;

	page_cache = EV_VIEW (widget)->page_cache;
	if (!page_cache)
		return;

	ev_view_get_page_extents (EV_VIEW (widget), EV_VIEW (widget)->current_page,
	                          &page_area, &border);

	scale = EV_VIEW (widget)->scale;
	ev_page_cache_get_text_layout (page_cache, EV_VIEW (widget)->current_page, &areas, &n_areas);
	if (!areas)
		return;

	if (offset >= n_areas)
		return;

	rect = areas + offset;
	*x = (int)(rect->x1 * scale);
	*y = (int)(rect->y1 * scale);

	*width = (int)(fabs (rect->x2 - rect->x1) * scale);
	*height = (int)(fabs (rect->y2 - rect->y1) * scale);

	toplevel = gtk_widget_get_toplevel (widget);
	gtk_widget_translate_coordinates (widget, toplevel, 0, 0, &x_widget, &y_widget);
	*x += x_widget;
	*y += y_widget;

	if (coords == ATK_XY_SCREEN) {
		gdk_window_get_origin (gtk_widget_get_window (toplevel), &x_window, &y_window);
		*x += x_window;
		*y += y_window;
	}

	*x -= EV_VIEW (widget)->scroll_x;
	*y -= EV_VIEW (widget)->scroll_y;

	*x += page_area.x;
	*y += page_area.y;
}

static gint
ev_view_accessible_get_offset_at_point (AtkText      *text,
					gint         x,
					gint         y,
					AtkCoordType coords)
{
	GtkWidget *widget, *toplevel;
	EvRectangle *areas = NULL;
	EvRectangle *rect = NULL;
	guint n_areas = 0;
	guint i = 0;
	EvPageCache *page_cache;
	gint x_window, y_window, x_widget, y_widget;
	gint offset=-1, rx, ry;
	gdouble scale;
	GtkBorder border;
	GdkRectangle page_area;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
	if (widget == NULL)
		/* State is defunct */
		return -1;

	page_cache = EV_VIEW (widget)->page_cache;
	if (!page_cache)
		return -1;

	ev_view_get_page_extents (EV_VIEW (widget), EV_VIEW (widget)->current_page,
	                          &page_area, &border);

	scale = EV_VIEW (widget)->scale;
	ev_page_cache_get_text_layout (page_cache, EV_VIEW (widget)->current_page, &areas, &n_areas);
	if (!areas)
		return -1;

	rx = x;
	ry = y;

	rx -= page_area.x;
	ry -= page_area.y;

	rx += EV_VIEW (widget)->scroll_x;
	ry += EV_VIEW (widget)->scroll_y;

	toplevel = gtk_widget_get_toplevel (widget);
	gtk_widget_translate_coordinates (widget, toplevel, 0, 0, &x_widget, &y_widget);
	rx -= x_widget;
	ry -= y_widget;

	if (coords == ATK_XY_SCREEN) {
		gdk_window_get_origin (gtk_widget_get_window (toplevel), &x_window, &y_window);
		rx -= x_window;
		ry -= y_window;
	}

	rx /= scale;
	ry /= scale;

	for (i = 0; i < n_areas; i++) {
		rect = areas + i;
		if (rx >= rect->x1 && rx <= rect->x2 &&
		    ry >= rect->y1 && ry <= rect->y2)
			offset = i;
	}

	return offset;
}

static gint
ev_view_accessible_get_n_selections (AtkText *text)
{
	GtkWidget *widget;
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	gint select_start, select_end;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
	if (widget == NULL)
		/* State is defunct */
		return -1;

	buffer = ev_view_accessible_get_text_buffer (EV_VIEW_ACCESSIBLE (text), EV_VIEW (widget));
	if (!buffer)
		return -1;

	gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
	select_start = gtk_text_iter_get_offset (&start);
	select_end = gtk_text_iter_get_offset (&end);

	if (select_start != select_end)
		return 1;
	else
		return 0;
}

static gchar*
ev_view_accessible_get_selection (AtkText *text,
				  gint    selection_num,
				  gint    *start_pos,
				  gint    *end_pos)
{
	GtkWidget *widget;
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	gchar *retval = NULL;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
	if (widget == NULL)
		/* State is defunct */
		return NULL;

	if (selection_num != 0)
		return NULL;

	buffer = ev_view_accessible_get_text_buffer (EV_VIEW_ACCESSIBLE (text), EV_VIEW (widget));
	if (!buffer)
		return NULL;

	gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
	*start_pos = gtk_text_iter_get_offset (&start);
	*end_pos = gtk_text_iter_get_offset (&end);

	if (*start_pos != *end_pos)
		retval = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

	return retval;
}

static gboolean
ev_view_accessible_add_selection (AtkText *text,
				  gint    start_pos,
				  gint    end_pos)
{
	GtkWidget *widget;
	GtkTextBuffer *buffer;
	GtkTextIter pos_itr;
	GtkTextIter start, end;
	gint select_start, select_end;
	gboolean retval = FALSE;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
	if (widget == NULL)
		/* State is defunct */
		return FALSE;

	buffer = ev_view_accessible_get_text_buffer (EV_VIEW_ACCESSIBLE (text), EV_VIEW (widget));
	if (!buffer)
		return FALSE;

	gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
	select_start = gtk_text_iter_get_offset (&start);
	select_end = gtk_text_iter_get_offset (&end);

	/* If there is already a selection, then don't allow
	 * another to be added
	 */
	if (select_start == select_end) {
		gtk_text_buffer_get_iter_at_offset (buffer, &pos_itr, start_pos);
		gtk_text_buffer_move_mark_by_name (buffer, "selection_bound", &pos_itr);
		gtk_text_buffer_get_iter_at_offset (buffer, &pos_itr, end_pos);
		gtk_text_buffer_move_mark_by_name (buffer, "insert", &pos_itr);

		retval = TRUE;
	}

	return retval;
}

static gboolean
ev_view_accessible_remove_selection (AtkText *text,
				     gint    selection_num)
{
	GtkWidget *widget;
	GtkTextBuffer *buffer;
	GtkTextMark *cursor_mark;
	GtkTextIter cursor_itr;
	GtkTextIter start, end;
	gint select_start, select_end;
	gboolean retval = FALSE;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
	if (widget == NULL)
		/* State is defunct */
		return FALSE;

	buffer = ev_view_accessible_get_text_buffer (EV_VIEW_ACCESSIBLE (text), EV_VIEW (widget));
	if (!buffer)
		return FALSE;

	gtk_text_buffer_get_selection_bounds(buffer, &start, &end);
	select_start = gtk_text_iter_get_offset(&start);
	select_end = gtk_text_iter_get_offset(&end);

	if (select_start != select_end) {
		/* Setting the start & end of the selected region
		 * to the caret position turns off the selection.
		 */
		cursor_mark = gtk_text_buffer_get_insert (buffer);
		gtk_text_buffer_get_iter_at_mark (buffer, &cursor_itr, cursor_mark);
		gtk_text_buffer_move_mark_by_name (buffer, "selection_bound", &cursor_itr);

		retval = TRUE;
	}

	return retval;
}

static gboolean
ev_view_accessible_set_selection (AtkText *text,
				  gint	  selection_num,
				  gint    start_pos,
				  gint    end_pos)
{
	GtkWidget *widget;
	GtkTextBuffer *buffer;
	GtkTextIter pos_itr;
	GtkTextIter start, end;
	gint select_start, select_end;
	gboolean retval = FALSE;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
	if (widget == NULL)
		/* State is defunct */
		return FALSE;

	buffer = ev_view_accessible_get_text_buffer (EV_VIEW_ACCESSIBLE (text), EV_VIEW (widget));
	if (!buffer)
		return FALSE;

	gtk_text_buffer_get_selection_bounds(buffer, &start, &end);
	select_start = gtk_text_iter_get_offset(&start);
	select_end = gtk_text_iter_get_offset(&end);

	if (select_start != select_end) {
		gtk_text_buffer_get_iter_at_offset (buffer, &pos_itr, start_pos);
		gtk_text_buffer_move_mark_by_name (buffer, "selection_bound", &pos_itr);
		gtk_text_buffer_get_iter_at_offset (buffer, &pos_itr, end_pos);
		gtk_text_buffer_move_mark_by_name (buffer, "insert", &pos_itr);

		retval = TRUE;
	}

	return retval;
}

static void ev_view_accessible_text_iface_init (AtkTextIface * iface)
{
	g_return_if_fail (iface != NULL);

	iface->get_text = ev_view_accessible_get_text;
	iface->get_character_at_offset = ev_view_accessible_get_character_at_offset;
	iface->get_text_before_offset = ev_view_accessible_get_text_before_offset;
	iface->get_text_at_offset = ev_view_accessible_get_text_at_offset;
	iface->get_text_after_offset = ev_view_accessible_get_text_after_offset;
	iface->get_caret_offset = ev_view_accessible_get_caret_offset;
	iface->set_caret_offset = ev_view_accessible_set_caret_offset;
	iface->get_character_count = ev_view_accessible_get_character_count;
	iface->get_n_selections = ev_view_accessible_get_n_selections;
	iface->get_selection = ev_view_accessible_get_selection;
	iface->add_selection = ev_view_accessible_add_selection;
	iface->remove_selection = ev_view_accessible_remove_selection;
	iface->set_selection = ev_view_accessible_set_selection;
	iface->get_run_attributes = ev_view_accessible_get_run_attributes;
	iface->get_default_attributes = ev_view_accessible_get_default_attributes;
	iface->get_character_extents = ev_view_accessible_get_character_extents;
	iface->get_offset_at_point = ev_view_accessible_get_offset_at_point;
	return;
}

static gboolean
ev_view_accessible_idle_do_action (gpointer data)
{
	EvViewAccessiblePriv* priv = EV_VIEW_ACCESSIBLE_GET_PRIVATE (data);

	ev_view_scroll (EV_VIEW (gtk_accessible_get_widget (GTK_ACCESSIBLE (data))),
	                priv->idle_scroll,
	                FALSE);
	priv->action_idle_handler = 0;
	return FALSE;
}

static gboolean
ev_view_accessible_action_do_action (AtkAction *action,
				     gint      i)
{
	EvViewAccessiblePriv* priv = EV_VIEW_ACCESSIBLE_GET_PRIVATE (action);

	if (gtk_accessible_get_widget (GTK_ACCESSIBLE (action)) == NULL)
		return FALSE;

	if (priv->action_idle_handler)
		return FALSE;

	switch (i) {
	case ACTION_SCROLL_UP:
		priv->idle_scroll = GTK_SCROLL_PAGE_BACKWARD;
		break;
	case ACTION_SCROLL_DOWN:
		priv->idle_scroll = GTK_SCROLL_PAGE_FORWARD;
		break;
	default:
		return FALSE;
	}
	priv->action_idle_handler = g_idle_add (ev_view_accessible_idle_do_action,
	                                        action);
	return TRUE;
}

static gint
ev_view_accessible_action_get_n_actions (AtkAction *action)
{
	return LAST_ACTION;
}

static const gchar *
ev_view_accessible_action_get_description (AtkAction *action,
					   gint      i)
{
	EvViewAccessiblePriv* priv = EV_VIEW_ACCESSIBLE_GET_PRIVATE (action);

	if (i < 0 || i >= LAST_ACTION)
		return NULL;

	if (priv->action_descriptions[i])
		return priv->action_descriptions[i];
	else
		return ev_view_accessible_action_descriptions[i];
}

static const gchar *
ev_view_accessible_action_get_name (AtkAction *action,
				    gint      i)
{
	if (i < 0 || i >= LAST_ACTION)
		return NULL;

	return ev_view_accessible_action_names[i];
}

static gboolean
ev_view_accessible_action_set_description (AtkAction   *action,
					   gint        i,
					   const gchar *description)
{
	EvViewAccessiblePriv* priv = EV_VIEW_ACCESSIBLE_GET_PRIVATE (action);
	gchar *old_description;

	if (i < 0 || i >= LAST_ACTION)
		return FALSE;

	old_description = priv->action_descriptions[i];
	priv->action_descriptions[i] = g_strdup (description);
	g_free (old_description);

	return TRUE;
}

static void ev_view_accessible_action_iface_init (AtkActionIface * iface)
{
	iface->do_action = ev_view_accessible_action_do_action;
	iface->get_n_actions = ev_view_accessible_action_get_n_actions;
	iface->get_description = ev_view_accessible_action_get_description;
	iface->get_name = ev_view_accessible_action_get_name;
	iface->set_description = ev_view_accessible_action_set_description;
}

GType ev_view_accessible_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		GTypeInfo tinfo = {
			0,	/* class size */
			(GBaseInitFunc) NULL,	/* base init */
			(GBaseFinalizeFunc) NULL,	/* base finalize */
			(GClassInitFunc) ev_view_accessible_class_init,	/* class init */
			(GClassFinalizeFunc) NULL,	/* class finalize */
			NULL,	/* class data */
			0,	/* instance size */
			0,	/* nb preallocs */
			(GInstanceInitFunc) NULL,	/* instance init */
			NULL	/* value table */
		};

		const GInterfaceInfo atk_text_info = {
			(GInterfaceInitFunc)
			ev_view_accessible_text_iface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};

		const GInterfaceInfo atk_action_info = {
			(GInterfaceInitFunc)
			ev_view_accessible_action_iface_init,
			(GInterfaceFinalizeFunc) NULL,
			NULL
		};
		/*
		 * Figure out the size of the class and instance
		 * we are deriving from
		 */
		AtkObjectFactory *factory;
		GType derived_type;
		GTypeQuery query;
		GType derived_atk_type;

		derived_type = g_type_parent (EV_TYPE_VIEW);
		factory = atk_registry_get_factory (atk_get_default_registry (),
		                                    derived_type);
		derived_atk_type = atk_object_factory_get_accessible_type (factory);

		g_type_query (derived_atk_type, &query);
		tinfo.class_size = query.class_size;
		tinfo.instance_size = query.instance_size;

		type = g_type_register_static (derived_atk_type, "EvViewAccessible",
		                               &tinfo, 0);
		g_type_add_interface_static (type, ATK_TYPE_TEXT,
		                             &atk_text_info);
		g_type_add_interface_static (type, ATK_TYPE_ACTION,
		                             &atk_action_info);
	}

	return type;
}

static AtkObject *ev_view_accessible_new(GObject * obj)
{
	AtkObject *accessible;

	g_return_val_if_fail(EV_IS_VIEW (obj), NULL);

	accessible = g_object_new (ev_view_accessible_get_type (), NULL);
	atk_object_initialize (accessible, obj);

	atk_object_set_name (ATK_OBJECT (accessible), _("Document View"));
	atk_object_set_role (ATK_OBJECT (accessible), ATK_ROLE_DOCUMENT_FRAME);

	return accessible;
}

typedef AtkObjectFactory      EvViewAccessibleFactory;
typedef AtkObjectFactoryClass EvViewAccessibleFactoryClass;

static void ev_view_accessible_factory_init (EvViewAccessibleFactory *factory)
{
}

static GType ev_view_accessible_factory_get_accessible_type(void)
{
	return ev_view_accessible_get_type();
}

static AtkObject *ev_view_accessible_factory_create_accessible (GObject * obj)
{
	return ev_view_accessible_new(obj);
}

static void ev_view_accessible_factory_class_init (AtkObjectFactoryClass * klass)
{
	klass->create_accessible = ev_view_accessible_factory_create_accessible;
	klass->get_accessible_type = ev_view_accessible_factory_get_accessible_type;
}

G_DEFINE_TYPE (EvViewAccessibleFactory, ev_view_accessible_factory, ATK_TYPE_OBJECT_FACTORY)
