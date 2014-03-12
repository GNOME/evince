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

#include "ev-selection.h"
#include "ev-page-cache.h"
#include "ev-view-accessible.h"
#include "ev-link-accessible.h"
#include "ev-view-private.h"
#include "ev-page-accessible.h"

static void ev_view_accessible_text_iface_init      (AtkTextIface      *iface);
static void ev_view_accessible_action_iface_init    (AtkActionIface    *iface);
static void ev_view_accessible_hypertext_iface_init (AtkHypertextIface *iface);
static void ev_view_accessible_document_iface_init  (AtkDocumentIface  *iface);

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

struct _EvViewAccessiblePrivate {
	EvDocumentModel *model;

	/* AtkAction */
	gchar        *action_descriptions[LAST_ACTION];
	guint         action_idle_handler;
	GtkScrollType idle_scroll;

	/* AtkHypertext */
	GHashTable    *links;

	gint previous_cursor_page;

	GPtrArray *children;
};

G_DEFINE_TYPE_WITH_CODE (EvViewAccessible, ev_view_accessible, GTK_TYPE_CONTAINER_ACCESSIBLE,
			 G_IMPLEMENT_INTERFACE (ATK_TYPE_TEXT, ev_view_accessible_text_iface_init)
			 G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, ev_view_accessible_action_iface_init)
			 G_IMPLEMENT_INTERFACE (ATK_TYPE_HYPERTEXT, ev_view_accessible_hypertext_iface_init)
			 G_IMPLEMENT_INTERFACE (ATK_TYPE_DOCUMENT, ev_view_accessible_document_iface_init)
	)

static gint
get_relevant_page (EvView *view)
{
	return ev_view_is_caret_navigation_enabled (view) ? view->cursor_page : view->current_page;
}

static void
clear_cache (EvViewAccessible *accessible)
{
	EvViewAccessiblePrivate* priv = accessible->priv;

	g_clear_pointer (&priv->links, (GDestroyNotify)g_hash_table_destroy);
}

static void
clear_children (EvViewAccessible *self)
{
	gint i;
	AtkObject *child;

	if (self->priv->children == NULL)
		return;

	for (i = 0; i < self->priv->children->len; i++) {
		child = g_ptr_array_index (self->priv->children, i);
		atk_object_notify_state_change (child, ATK_STATE_DEFUNCT, TRUE);
	}

	g_clear_pointer (&self->priv->children, g_ptr_array_unref);
}

static void
ev_view_accessible_finalize (GObject *object)
{
	EvViewAccessiblePrivate *priv = EV_VIEW_ACCESSIBLE (object)->priv;
	int i;

	if (priv->model) {
		g_signal_handlers_disconnect_by_data (priv->model, object);
		g_object_unref (priv->model);
		priv->model = NULL;
	}
	if (priv->action_idle_handler)
		g_source_remove (priv->action_idle_handler);
	for (i = 0; i < LAST_ACTION; i++)
		g_free (priv->action_descriptions [i]);
	clear_cache (EV_VIEW_ACCESSIBLE (object));
	clear_children (EV_VIEW_ACCESSIBLE (object));

	G_OBJECT_CLASS (ev_view_accessible_parent_class)->finalize (object);
}

static void
ev_view_accessible_initialize (AtkObject *obj,
			       gpointer   data)
{
	if (ATK_OBJECT_CLASS (ev_view_accessible_parent_class)->initialize != NULL)
		ATK_OBJECT_CLASS (ev_view_accessible_parent_class)->initialize (obj, data);

	gtk_accessible_set_widget (GTK_ACCESSIBLE (obj), GTK_WIDGET (data));

	atk_object_set_name (obj, _("Document View"));
	atk_object_set_role (obj, ATK_ROLE_DOCUMENT_FRAME);
}

static gint
ev_view_accessible_get_n_pages (EvViewAccessible *self)
{
	return self->priv->children == NULL ? 0 : self->priv->children->len;
}

static AtkObject *
ev_view_accessible_ref_child (AtkObject *obj,
			      gint       i)
{
	EvViewAccessible *self;

	g_return_val_if_fail (EV_IS_VIEW_ACCESSIBLE (obj), NULL);
	self = EV_VIEW_ACCESSIBLE (obj);
	g_return_val_if_fail (i >= 0 || i < ev_view_accessible_get_n_pages (self), NULL);

	return g_object_ref (g_ptr_array_index (self->priv->children, i));
}

static gint
ev_view_accessible_get_n_children (AtkObject *obj)
{
	return ev_view_accessible_get_n_pages (EV_VIEW_ACCESSIBLE (obj));
}

static void
ev_view_accessible_class_init (EvViewAccessibleClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	AtkObjectClass *atk_class = ATK_OBJECT_CLASS (klass);

	object_class->finalize = ev_view_accessible_finalize;
	atk_class->initialize = ev_view_accessible_initialize;
	atk_class->get_n_children = ev_view_accessible_get_n_children;
	atk_class->ref_child = ev_view_accessible_ref_child;

	g_type_class_add_private (klass, sizeof (EvViewAccessiblePrivate));
}

static void
ev_view_accessible_init (EvViewAccessible *accessible)
{
	accessible->priv = G_TYPE_INSTANCE_GET_PRIVATE (accessible, EV_TYPE_VIEW_ACCESSIBLE, EvViewAccessiblePrivate);
}

/* ATs expect to be able to identify sentence boundaries based on content. Valid,
 * content-based boundaries may be present at the end of a newline, for instance
 * at the end of a heading within a document. Thus being able to distinguish hard
 * returns from soft returns is necessary. However, the text we get from Poppler
 * for non-tagged PDFs has "\n" inserted at the end of each line resulting in a
 * broken accessibility implementation w.r.t. sentences.
 */
static gboolean
treat_as_soft_return (EvView       *view,
		      PangoLogAttr *log_attrs,
		      gint          offset)
{
	EvRectangle *areas = NULL;
	guint n_areas = 0;
	gdouble line_spacing, this_line_height, next_word_width;
	EvRectangle *this_line_start;
	EvRectangle *this_line_end;
	EvRectangle *next_line_start;
	EvRectangle *next_line_end;
	EvRectangle *next_word_end;
	gint prev_offset, next_offset;


	if (!log_attrs[offset].is_white)
		return FALSE;

	ev_page_cache_get_text_layout (view->page_cache, get_relevant_page (view), &areas, &n_areas);
	if (n_areas <= offset + 1)
		return FALSE;

	prev_offset = offset - 1;
	next_offset = offset + 1;

	/* In wrapped text, the character at the start of the next line starts a word.
	 * Examples where this condition might fail include bullets and images. But it
	 * also includes things like "(", so also check the next character.
	 */
	if (!log_attrs[next_offset].is_word_start &&
	    (next_offset + 1 >= n_areas || !log_attrs[next_offset + 1].is_word_start))
		return FALSE;

	/* In wrapped text, the chars on either side of the newline have very similar heights.
	 * Examples where this condition might fail include a newline at the end of a heading,
	 * and a newline at the end of a paragraph that is followed by a heading.
	 */
	this_line_end = areas + prev_offset;
	next_line_start = areas + next_offset;;

	this_line_height = this_line_end->y2 - this_line_end->y1;
	if (ABS (this_line_height - (next_line_start->y2 - next_line_start->y1)) > 0.25)
		return FALSE;

	/* If there is significant white space between this line and the next, odds are this
	 * is not a soft return in wrapped text. Lines within a typical paragraph are at most
	 * double-spaced. If the spacing is more than that, assume a hard return is present.
	 */
	line_spacing = next_line_start->y1 - this_line_end->y2;
	if (line_spacing - this_line_height > 1)
		return FALSE;

	/* Lines within a typical paragraph have *reasonably* similar x1 coordinates. But
	 * we cannot count on them being nearly identical. Examples where indentation can
	 * be present in wrapped text include indenting the first line of the paragraph,
	 * and hanging indents (e.g. in the works cited within an academic paper). So we'll
	 * be somewhat tolerant here.
	 */
	for ( ; prev_offset > 0 && !log_attrs[prev_offset].is_mandatory_break; prev_offset--);
	this_line_start = areas + prev_offset;
	if (ABS (this_line_start->x1 - next_line_start->x1) > 20)
		return FALSE;

	/* Ditto for x2, but this line might be short due to a wide word on the next line. */
	for ( ; next_offset < n_areas && !log_attrs[next_offset].is_word_end; next_offset++);
	next_word_end = areas + next_offset;
	next_word_width = next_word_end->x2 - next_line_start->x1;

	for ( ; next_offset < n_areas && !log_attrs[next_offset + 1].is_mandatory_break; next_offset++);
	next_line_end = areas + next_offset;
	if (next_line_end->x2 - (this_line_end->x2 + next_word_width) > 20)
		return FALSE;

	return TRUE;
}

static void
ev_view_accessible_get_range_for_boundary (AtkText          *text,
					   AtkTextBoundary   boundary_type,
					   gint              offset,
					   gint             *start_offset,
					   gint             *end_offset)
{
	GtkWidget *widget;
	EvView *view;
	gint start = 0;
	gint end = 0;
	PangoLogAttr *log_attrs = NULL;
	gulong n_attrs;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
	if (widget == NULL)
		return;

	view = EV_VIEW (widget);
	if (!view->page_cache)
		return;

	ev_page_cache_get_text_log_attrs (view->page_cache, get_relevant_page (view), &log_attrs, &n_attrs);
	if (!log_attrs)
		return;

	switch (boundary_type) {
	case ATK_TEXT_BOUNDARY_CHAR:
		start = offset;
		end = offset + 1;
		break;
	case ATK_TEXT_BOUNDARY_WORD_START:
		for (start = offset; start >= 0 && !log_attrs[start].is_word_start; start--);
		for (end = offset + 1; end <= n_attrs && !log_attrs[end].is_word_start; end++);
		break;
	case ATK_TEXT_BOUNDARY_SENTENCE_START:
		for (start = offset; start >= 0; start--) {
			if (log_attrs[start].is_mandatory_break && treat_as_soft_return (view, log_attrs, start - 1))
				continue;
			if (log_attrs[start].is_sentence_start)
				break;
		}
		for (end = offset + 1; end <= n_attrs; end++) {
			if (log_attrs[end].is_mandatory_break && treat_as_soft_return (view, log_attrs, end - 1))
				continue;
			if (log_attrs[end].is_sentence_start)
				break;
		}
		break;
	case ATK_TEXT_BOUNDARY_LINE_START:
		for (start = offset; start >= 0 && !log_attrs[start].is_mandatory_break; start--);
		for (end = offset + 1; end <= n_attrs && !log_attrs[end].is_mandatory_break; end++);
		break;
	default:
		/* The "END" boundary types are deprecated */
		break;
	}

	*start_offset = start;
	*end_offset = end;
}

static gchar *
ev_view_accessible_get_substring (AtkText *text,
				  gint     start_offset,
				  gint     end_offset)
{
	GtkWidget *widget;
	EvView *view;
	gchar *substring, *normalized;
	const gchar* page_text;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
	if (widget == NULL)
		return NULL;

	view = EV_VIEW (widget);
	if (!view->page_cache)
		return NULL;

	page_text = ev_page_cache_get_text (view->page_cache, get_relevant_page (view));
	start_offset = MAX (0, start_offset);
	if (end_offset < 0 || end_offset > g_utf8_strlen (page_text, -1))
		end_offset = strlen (page_text);

	substring = g_utf8_substring (page_text, start_offset, end_offset);
	normalized = g_utf8_normalize (substring, -1, G_NORMALIZE_NFKC);
	g_free (substring);

	return normalized;
}

static gunichar
ev_view_accessible_get_character_at_offset (AtkText *text,
					    gint     offset)
{
	gchar *string;
	gunichar unichar;

	string = ev_view_accessible_get_substring (text, offset, offset + 1);
	unichar = g_utf8_get_char (string);
	g_free(string);

	return unichar;
}

static gchar *
ev_view_accessible_get_text (AtkText *text,
			     gint     start_pos,
			     gint     end_pos)
{
	return ev_view_accessible_get_substring (text, start_pos, end_pos);
}

static gchar *
ev_view_accessible_get_text_at_offset (AtkText        *text,
                                       gint            offset,
                                       AtkTextBoundary boundary_type,
                                       gint           *start_offset,
                                       gint           *end_offset)
{
	gchar *retval;

	ev_view_accessible_get_range_for_boundary (text, boundary_type, offset, start_offset, end_offset);
	retval = ev_view_accessible_get_substring (text, *start_offset, *end_offset);

	/* If newlines appear inside the text of a sentence (i.e. between the start and
	 * end offsets returned by ev_view_accessible_get_substring), it interferes with
	 * the prosody of text-to-speech based-solutions such as a screen reader because
	 * speech synthesizers tend to pause after the newline char as if it were the end
	 * of the sentence.
	 */
        if (boundary_type == ATK_TEXT_BOUNDARY_SENTENCE_START)
		g_strdelimit (retval, "\n", ' ');

	return retval;
}

static gint
ev_view_accessible_get_character_count (AtkText *text)
{
	GtkWidget *widget;
	EvView *view;
	gint retval;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
	if (widget == NULL)
		/* State is defunct */
		return 0;

	view = EV_VIEW (widget);
	retval = g_utf8_strlen (ev_page_cache_get_text (view->page_cache, get_relevant_page (view)), -1);

	return retval;
}

static gint
ev_view_accessible_get_caret_offset (AtkText *text)
{
	GtkWidget *widget;
	EvView *view;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
	if (widget == NULL)
		/* State is defunct */
		return -1;

	view = EV_VIEW (widget);
	if (view->caret_enabled)
		return view->cursor_offset;

	return -1;
}

static gboolean
ev_view_accessible_set_caret_offset (AtkText *text, gint offset)
{
	GtkWidget *widget;
	EvView    *view;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
	if (widget == NULL)
		/* State is defunct */
		return FALSE;

	view = EV_VIEW (widget);
	ev_view_set_caret_cursor_position (view,
					   view->cursor_page,
					   offset);

	return TRUE;
}

static AtkAttributeSet *
add_attribute (AtkAttributeSet  *attr_set,
               AtkTextAttribute  attr_type,
               gchar            *attr_value)
{
  AtkAttribute *attr = g_new (AtkAttribute, 1);

  attr->name = g_strdup (atk_text_attribute_get_name (attr_type));
  attr->value = attr_value;

  return g_slist_prepend (attr_set, attr);
}

static AtkAttributeSet *
get_run_attributes (PangoAttrList   *attrs,
		    const gchar     *text,
		    gint             offset,
		    gint            *start_offset,
		    gint            *end_offset)
{
	AtkAttributeSet   *atk_attr_set = NULL;
	PangoAttrString   *pango_string;
	PangoAttrInt      *pango_int;
	PangoAttrColor    *pango_color;
	PangoAttrIterator *iter;
	gint               i, start, end;
	gboolean           has_attrs = FALSE;
	glong              text_length;
	gchar             *attr_value;

	text_length = g_utf8_strlen (text, -1);
	if (offset < 0 || offset >= text_length)
		return NULL;

	/* Check if there are attributes for the offset,
	 * and set the attributes range if positive */
	iter = pango_attr_list_get_iterator (attrs);
	i = g_utf8_offset_to_pointer (text, offset) - text;

	do {
		pango_attr_iterator_range (iter, &start, &end);
		if (i >= start && i < end) {
			*start_offset = g_utf8_pointer_to_offset (text, text + start);
			if (end == G_MAXINT) /* Last iterator */
				end = text_length;
			*end_offset = g_utf8_pointer_to_offset (text, text + end);
			 has_attrs = TRUE;
		}
	} while (!has_attrs && pango_attr_iterator_next (iter));

	if (!has_attrs) {
		pango_attr_iterator_destroy (iter);
		return NULL;
	}

	/* Create the AtkAttributeSet from the Pango attributes */
	pango_string = (PangoAttrString *) pango_attr_iterator_get (iter, PANGO_ATTR_FAMILY);
	if (pango_string) {
		attr_value = g_strdup (pango_string->value);
		atk_attr_set = add_attribute (atk_attr_set, ATK_TEXT_ATTR_FAMILY_NAME, attr_value);
	}

	pango_int = (PangoAttrInt *) pango_attr_iterator_get (iter, PANGO_ATTR_SIZE);
	if (pango_int) {
		attr_value = g_strdup_printf ("%i", pango_int->value / PANGO_SCALE);
		atk_attr_set = add_attribute (atk_attr_set, ATK_TEXT_ATTR_SIZE, attr_value);
	}

	pango_int = (PangoAttrInt *) pango_attr_iterator_get (iter, PANGO_ATTR_UNDERLINE);
	if (pango_int) {
		atk_attr_set = add_attribute (atk_attr_set,
					      ATK_TEXT_ATTR_UNDERLINE,
					      g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_UNDERLINE,
										      pango_int->value)));
	}

	pango_color = (PangoAttrColor *) pango_attr_iterator_get (iter, PANGO_ATTR_FOREGROUND);
	if (pango_color) {
		attr_value = g_strdup_printf ("%u,%u,%u",
					      pango_color->color.red,
					      pango_color->color.green,
					      pango_color->color.blue);
		atk_attr_set = add_attribute (atk_attr_set, ATK_TEXT_ATTR_FG_COLOR, attr_value);
	}

	pango_attr_iterator_destroy (iter);

	return atk_attr_set;
}

static AtkAttributeSet*
ev_view_accessible_get_run_attributes (AtkText *text,
				       gint     offset,
				       gint    *start_offset,
				       gint    *end_offset)
{
	EvView        *view;
	GtkWidget     *widget;
	PangoAttrList *attrs;
	const gchar   *page_text;

	if (offset < 0)
		return NULL;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
	if (!widget)
		return NULL;

	view = EV_VIEW (widget);
	if (!view->page_cache)
		return NULL;

	page_text = ev_page_cache_get_text (view->page_cache, get_relevant_page (view));
	if (!page_text)
		return NULL;

	attrs = ev_page_cache_get_text_attrs (view->page_cache, get_relevant_page (view));
	if (!attrs)
		return NULL;

	return get_run_attributes (attrs, page_text, offset, start_offset, end_offset);
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
	EvView *view;
	EvRectangle *areas = NULL;
	EvRectangle *doc_rect;
	guint n_areas = 0;
	gint x_widget, y_widget;
	GdkRectangle view_rect;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
	if (widget == NULL)
		/* State is defunct */
		return;

	view = EV_VIEW (widget);
	if (!view->page_cache)
		return;

	ev_page_cache_get_text_layout (view->page_cache, get_relevant_page (view), &areas, &n_areas);
	if (!areas || offset >= n_areas)
		return;

	doc_rect = areas + offset;
	_ev_view_transform_doc_rect_to_view_rect (view, get_relevant_page (view), doc_rect, &view_rect);
	view_rect.x -= view->scroll_x;
	view_rect.y -= view->scroll_y;

	toplevel = gtk_widget_get_toplevel (widget);
	gtk_widget_translate_coordinates (widget, toplevel, 0, 0, &x_widget, &y_widget);
	view_rect.x += x_widget;
	view_rect.y += y_widget;

	if (coords == ATK_XY_SCREEN) {
		gint x_window, y_window;

		gdk_window_get_origin (gtk_widget_get_window (toplevel), &x_window, &y_window);
		view_rect.x += x_window;
		view_rect.y += y_window;
	}

	*x = view_rect.x;
	*y = view_rect.y;
	*width = view_rect.width;
	*height = view_rect.height;
}

static gint
ev_view_accessible_get_offset_at_point (AtkText      *text,
					gint         x,
					gint         y,
					AtkCoordType coords)
{
	GtkWidget *widget, *toplevel;
	EvView *view;
	EvRectangle *areas = NULL;
	EvRectangle *rect = NULL;
	guint n_areas = 0;
	guint i;
	gint x_widget, y_widget;
	gint offset=-1;
	GdkPoint view_point;
	gdouble doc_x, doc_y;
	GtkBorder border;
	GdkRectangle page_area;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
	if (widget == NULL)
		/* State is defunct */
		return -1;

	view = EV_VIEW (widget);
	if (!view->page_cache)
		return -1;

	ev_page_cache_get_text_layout (view->page_cache, get_relevant_page (view), &areas, &n_areas);
	if (!areas)
		return -1;

	view_point.x = x;
	view_point.y = y;
	toplevel = gtk_widget_get_toplevel (widget);
	gtk_widget_translate_coordinates (widget, toplevel, 0, 0, &x_widget, &y_widget);
	view_point.x -= x_widget;
	view_point.y -= y_widget;

	if (coords == ATK_XY_SCREEN) {
		gint x_window, y_window;

		gdk_window_get_origin (gtk_widget_get_window (toplevel), &x_window, &y_window);
		view_point.x -= x_window;
		view_point.y -= y_window;
	}

	ev_view_get_page_extents (view, get_relevant_page (view), &page_area, &border);
	_ev_view_transform_view_point_to_doc_point (view, &view_point, &page_area, &border, &doc_x, &doc_y);

	for (i = 0; i < n_areas; i++) {
		rect = areas + i;
		if (doc_x >= rect->x1 && doc_x <= rect->x2 &&
		    doc_y >= rect->y1 && doc_y <= rect->y2)
			offset = i;
	}

	return offset;
}

static gint
ev_view_accessible_get_n_selections (AtkText *text)
{
	GtkWidget *widget;
	EvView *view;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
	if (widget == NULL)
		/* State is defunct */
		return -1;

	view = EV_VIEW (widget);
	if (!EV_IS_SELECTION (view->document) || !view->selection_info.selections)
		return 0;

	return 1;
}

static gboolean
get_selection_bounds (EvView          *view,
		      EvViewSelection *selection,
		      gint            *start_offset,
		      gint            *end_offset)
{
	cairo_rectangle_int_t rect;
	gint start, end;

	if (!selection->covered_region || cairo_region_is_empty (selection->covered_region))
		return FALSE;

	cairo_region_get_rectangle (selection->covered_region, 0, &rect);
	start = _ev_view_get_caret_cursor_offset_at_doc_point (view,
							       selection->page,
							       rect.x / view->scale,
							       (rect.y + (rect.height / 2)) / view->scale);
	if (start == -1)
		return FALSE;

	cairo_region_get_rectangle (selection->covered_region,
				    cairo_region_num_rectangles (selection->covered_region) - 1,
				    &rect);
	end = _ev_view_get_caret_cursor_offset_at_doc_point (view,
							     selection->page,
							     (rect.x + rect.width) / view->scale,
							     (rect.y + (rect.height / 2)) / view->scale);
	if (end == -1)
		return FALSE;

	*start_offset = start;
	*end_offset = end;

	return TRUE;
}

static gchar *
ev_view_accessible_get_selection (AtkText *text,
				  gint     selection_num,
				  gint    *start_pos,
				  gint    *end_pos)
{
	GtkWidget *widget;
	EvView    *view;
	gchar *selected_text = NULL;
	gchar *normalized_text = NULL;
	GList *l;

	*start_pos = -1;
	*end_pos = -1;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
	if (widget == NULL)
		/* State is defunct */
		return NULL;

	if (selection_num != 0)
		return NULL;

	view = EV_VIEW (widget);
	if (!EV_IS_SELECTION (view->document) || !view->selection_info.selections)
		return NULL;


	for (l = view->selection_info.selections; l != NULL; l = l->next) {
		EvViewSelection *selection = (EvViewSelection *)l->data;
		gint start, end;

		if (selection->page != get_relevant_page (view))
			continue;

		if (get_selection_bounds (view, selection, &start, &end) && start != end) {
			EvPage *page;

			page = ev_document_get_page (view->document, selection->page);

			ev_document_doc_mutex_lock ();
			selected_text = ev_selection_get_selected_text (EV_SELECTION (view->document),
									page,
									selection->style,
									&(selection->rect));

			ev_document_doc_mutex_unlock ();

			g_object_unref (page);

			*start_pos = start;
			*end_pos = end;
		}

		break;
	}

	if (selected_text) {
		normalized_text = g_utf8_normalize (selected_text, -1, G_NORMALIZE_NFKC);
		g_free (selected_text);
	}

	return normalized_text;
}

/* ATK allows for multiple, non-contiguous selections within a single AtkText
 * object. Unless and until Evince supports this, selection numbers are ignored.
 */
static gboolean
ev_view_accessible_remove_selection (AtkText *text,
				     gint     selection_num)
{
	GtkWidget *widget;
	EvView *view;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
	if (widget == NULL)
		/* State is defunct */
		return FALSE;

	view = EV_VIEW (widget);
	if (ev_view_get_has_selection (view)) {
		_ev_view_clear_selection (view);
		return TRUE;
	}

	return FALSE;
}

static gboolean
ev_view_accessible_set_selection (AtkText *text,
				  gint	   selection_num,
				  gint     start_pos,
				  gint     end_pos)
{
	GtkWidget *widget;
	EvView *view;
	EvRectangle *areas = NULL;
	guint n_areas = 0;
	GdkRectangle start_rect, end_rect;
	GdkPoint start_point, end_point;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
	if (widget == NULL)
		/* State is defunct */
		return FALSE;

	view = EV_VIEW (widget);
	ev_page_cache_get_text_layout (view->page_cache, get_relevant_page (view), &areas, &n_areas);
	if (start_pos < 0 || end_pos >= n_areas)
		return FALSE;

	_ev_view_transform_doc_rect_to_view_rect (view, get_relevant_page (view), areas + start_pos, &start_rect);
	_ev_view_transform_doc_rect_to_view_rect (view, get_relevant_page (view), areas + end_pos - 1, &end_rect);
	start_point.x = start_rect.x;
	start_point.y = start_rect.y;
	end_point.x = end_rect.x + end_rect.width;
	end_point.y = end_rect.y + end_rect.height;
	_ev_view_set_selection (view, &start_point, &end_point);

	return TRUE;
}

static gboolean
ev_view_accessible_add_selection (AtkText *text,
				  gint     start_pos,
				  gint     end_pos)
{
	return ev_view_accessible_set_selection (text, 0, start_pos, end_pos);

}

#if ATK_CHECK_VERSION (2, 11, 3)
static gint
ev_view_accessible_get_page_count (AtkDocument *atk_document)
{
	g_return_val_if_fail (EV_IS_VIEW_ACCESSIBLE (atk_document), -1);

	return ev_view_accessible_get_n_pages (EV_VIEW_ACCESSIBLE (atk_document));
}

static gint
ev_view_accessible_get_current_page_number (AtkDocument *atk_document)
{
	GtkWidget *widget;

	g_return_val_if_fail (EV_IS_VIEW_ACCESSIBLE (atk_document), -1);

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (atk_document));
	if (widget == NULL)
		return -1;

	/* +1 as user starts to count on 1, but evince starts on 0 */
	return get_relevant_page (EV_VIEW (widget)) + 1;
}
#endif

static void
ev_view_accessible_document_iface_init (AtkDocumentIface *iface)
{
#if ATK_CHECK_VERSION (2, 11, 3)
	iface->get_current_page_number = ev_view_accessible_get_current_page_number;
	iface->get_page_count = ev_view_accessible_get_page_count;
#endif
}

static void
ev_view_accessible_text_iface_init (AtkTextIface * iface)
{
	iface->get_text = ev_view_accessible_get_text;
	iface->get_character_at_offset = ev_view_accessible_get_character_at_offset;
	iface->get_text_at_offset = ev_view_accessible_get_text_at_offset;
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
}

static gboolean
ev_view_accessible_idle_do_action (gpointer data)
{
	EvViewAccessiblePrivate* priv = EV_VIEW_ACCESSIBLE (data)->priv;

	ev_view_scroll (EV_VIEW (gtk_accessible_get_widget (GTK_ACCESSIBLE (data))),
	                priv->idle_scroll,
	                FALSE);
	priv->action_idle_handler = 0;
	return FALSE;
}

static gboolean
ev_view_accessible_action_do_action (AtkAction *action,
				     gint       i)
{
	EvViewAccessiblePrivate* priv = EV_VIEW_ACCESSIBLE (action)->priv;

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
					   gint       i)
{
	EvViewAccessiblePrivate* priv = EV_VIEW_ACCESSIBLE (action)->priv;

	if (i < 0 || i >= LAST_ACTION)
		return NULL;

	if (priv->action_descriptions[i])
		return priv->action_descriptions[i];
	else
		return ev_view_accessible_action_descriptions[i];
}

static const gchar *
ev_view_accessible_action_get_name (AtkAction *action,
				    gint       i)
{
	if (i < 0 || i >= LAST_ACTION)
		return NULL;

	return ev_view_accessible_action_names[i];
}

static gboolean
ev_view_accessible_action_set_description (AtkAction   *action,
					   gint         i,
					   const gchar *description)
{
	EvViewAccessiblePrivate* priv = EV_VIEW_ACCESSIBLE (action)->priv;
	gchar *old_description;

	if (i < 0 || i >= LAST_ACTION)
		return FALSE;

	old_description = priv->action_descriptions[i];
	priv->action_descriptions[i] = g_strdup (description);
	g_free (old_description);

	return TRUE;
}

static void
ev_view_accessible_action_iface_init (AtkActionIface * iface)
{
	iface->do_action = ev_view_accessible_action_do_action;
	iface->get_n_actions = ev_view_accessible_action_get_n_actions;
	iface->get_description = ev_view_accessible_action_get_description;
	iface->get_name = ev_view_accessible_action_get_name;
	iface->set_description = ev_view_accessible_action_set_description;
}

static GHashTable *
ev_view_accessible_get_links (EvViewAccessible *accessible,
			      EvView           *view)
{
	EvViewAccessiblePrivate* priv = accessible->priv;

	if (priv->links)
		return priv->links;

	priv->links = g_hash_table_new_full (g_direct_hash,
					     g_direct_equal,
					     NULL,
					     (GDestroyNotify)g_object_unref);
	return priv->links;
}

static AtkHyperlink *
ev_view_accessible_get_link (AtkHypertext *hypertext,
			     gint          link_index)
{
	GtkWidget        *widget;
	EvView           *view;
	GHashTable       *links;
	EvMappingList    *link_mapping;
	gint              n_links;
	EvMapping        *mapping;
	EvLinkAccessible *atk_link;

	if (link_index < 0)
		return NULL;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (hypertext));
	if (widget == NULL)
		/* State is defunct */
		return NULL;

	view = EV_VIEW (widget);
	if (!EV_IS_DOCUMENT_LINKS (view->document))
		return NULL;

	links = ev_view_accessible_get_links (EV_VIEW_ACCESSIBLE (hypertext), view);

	atk_link = g_hash_table_lookup (links, GINT_TO_POINTER (link_index));
	if (atk_link)
		return atk_hyperlink_impl_get_hyperlink (ATK_HYPERLINK_IMPL (atk_link));

	link_mapping = ev_page_cache_get_link_mapping (view->page_cache, get_relevant_page (view));
	if (!link_mapping)
		return NULL;

	n_links = ev_mapping_list_length (link_mapping);
	if (link_index > n_links - 1)
		return NULL;

	mapping = ev_mapping_list_nth (link_mapping, n_links - link_index - 1);
	atk_link = ev_link_accessible_new (EV_VIEW_ACCESSIBLE (hypertext),
					   EV_LINK (mapping->data),
					   &mapping->area);
	g_hash_table_insert (links, GINT_TO_POINTER (link_index), atk_link);

	return atk_hyperlink_impl_get_hyperlink (ATK_HYPERLINK_IMPL (atk_link));
}

static gint
ev_view_accessible_get_n_links (AtkHypertext *hypertext)
{
	GtkWidget     *widget;
	EvView        *view;
	EvMappingList *link_mapping;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (hypertext));
	if (widget == NULL)
		/* State is defunct */
		return 0;

	view = EV_VIEW (widget);
	if (!EV_IS_DOCUMENT_LINKS (view->document))
		return 0;

	link_mapping = ev_page_cache_get_link_mapping (view->page_cache, get_relevant_page (view));

	return link_mapping ? ev_mapping_list_length (link_mapping) : 0;
}

static gint
ev_view_accessible_get_link_index (AtkHypertext *hypertext,
				   gint          offset)
{
	guint i;

	for (i = 0; i < ev_view_accessible_get_n_links (hypertext); i++) {
		AtkHyperlink *hyperlink;
		gint          start_index, end_index;

		hyperlink = ev_view_accessible_get_link (hypertext, i);
		start_index = atk_hyperlink_get_start_index (hyperlink);
		end_index = atk_hyperlink_get_end_index (hyperlink);

		if (start_index <= offset && end_index >= offset)
			return i;
	}

	return -1;
}

static void
ev_view_accessible_hypertext_iface_init (AtkHypertextIface *iface)
{
	iface->get_link = ev_view_accessible_get_link;
	iface->get_n_links = ev_view_accessible_get_n_links;
	iface->get_link_index = ev_view_accessible_get_link_index;
}

static void
ev_view_accessible_cursor_moved (EvView *view,
				 gint page,
				 gint offset,
				 EvViewAccessible *accessible)
{
	EvViewAccessiblePrivate* priv = accessible->priv;

	if (priv->previous_cursor_page != page) {
		priv->previous_cursor_page = page;
		clear_cache (accessible);
#if ATK_CHECK_VERSION (2, 11, 2)
		/* +1 as user start to count on 1, but evince starts on 0 */
		g_signal_emit_by_name (accessible, "page-changed", page + 1);
#endif
	}

	g_signal_emit_by_name (accessible, "text-caret-moved", offset);
}

static void
ev_view_accessible_selection_changed (EvView *view,
				      EvViewAccessible *accessible)
{
	g_signal_emit_by_name (accessible, "text-selection-changed");
}

static void
page_changed_cb (EvDocumentModel  *model,
		 gint              old_page,
		 gint              new_page,
		 EvViewAccessible *accessible)
{
	clear_cache (accessible);

#if ATK_CHECK_VERSION (2, 11, 2)
	EvView *view;

	view = EV_VIEW (gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible)));
	if (!ev_view_is_caret_navigation_enabled (view))
		g_signal_emit_by_name (accessible, "page-changed", new_page + 1);
#endif
}

static void
initialize_children (EvViewAccessible *self)
{
	gint i;
	EvPageAccessible *child;
	gint n_pages;
	EvDocument *ev_document;

	ev_document = ev_document_model_get_document (self->priv->model);
	n_pages = ev_document_get_n_pages (ev_document);

	self->priv->children = g_ptr_array_new_full (n_pages, (GDestroyNotify) g_object_unref);
	for (i = 0; i < n_pages; i++) {
		child = ev_page_accessible_new (self, i);
		g_ptr_array_add (self->priv->children, child);
	}
}

static void
document_changed_cb (EvDocumentModel  *model,
		     GParamSpec       *pspec,
		     EvViewAccessible *accessible)
{
	EvDocument *document = ev_document_model_get_document (model);

	clear_children (accessible);

	if (document == NULL)
		return;

	clear_cache (accessible);
	initialize_children (accessible);

	/* Inside this callback the document is already loaded. We
	 * don't have here an "just before" and "just after"
	 * signal. We emit both in a row, as usual ATs uses reload to
	 * know that current content has changed, and load-complete to
	 * know that the content is already available.
	 */
	g_signal_emit_by_name (accessible, "reload");
	g_signal_emit_by_name (accessible, "load-complete");
}

void
ev_view_accessible_set_model (EvViewAccessible *accessible,
			      EvDocumentModel  *model)
{
	EvViewAccessiblePrivate* priv = accessible->priv;

	if (priv->model == model)
		return;

	if (priv->model) {
		g_signal_handlers_disconnect_by_data (priv->model, accessible);
		g_object_unref (priv->model);
	}

	priv->model = g_object_ref (model);

	document_changed_cb (model, NULL, accessible);
	g_signal_connect (priv->model, "page-changed",
			  G_CALLBACK (page_changed_cb),
			  accessible);
	g_signal_connect (priv->model, "notify::document",
			  G_CALLBACK (document_changed_cb),
			  accessible);
}

AtkObject *
ev_view_accessible_new (GtkWidget *widget)
{
	AtkObject *accessible;
	EvView    *view;

	g_return_val_if_fail (EV_IS_VIEW (widget), NULL);

	accessible = g_object_new (EV_TYPE_VIEW_ACCESSIBLE, NULL);
	atk_object_initialize (accessible, widget);

	g_signal_connect (widget, "cursor-moved",
			  G_CALLBACK (ev_view_accessible_cursor_moved),
			  accessible);
	g_signal_connect (widget, "selection-changed",
			  G_CALLBACK (ev_view_accessible_selection_changed),
			  accessible);

	view = EV_VIEW (widget);
	if (view->model)
		ev_view_accessible_set_model (EV_VIEW_ACCESSIBLE (accessible),
					      view->model);

	return accessible;
}

