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
#include "ev-view-private.h"
#include "ev-page-accessible.h"

static void ev_view_accessible_action_iface_init    (AtkActionIface    *iface);
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

	gint previous_cursor_page;
	gint start_page;
	gint end_page;
	AtkObject *focused_element;

	GPtrArray *children;
};

G_DEFINE_TYPE_WITH_CODE (EvViewAccessible, ev_view_accessible, GTK_TYPE_CONTAINER_ACCESSIBLE,
			 G_ADD_PRIVATE (EvViewAccessible)
			 G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, ev_view_accessible_action_iface_init)
			 G_IMPLEMENT_INTERFACE (ATK_TYPE_DOCUMENT, ev_view_accessible_document_iface_init)
	)

static gint
get_relevant_page (EvView *view)
{
	return ev_view_is_caret_navigation_enabled (view) ? view->cursor_page : view->current_page;
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
		g_clear_object (&priv->model);
	}

	g_clear_handle_id (&priv->action_idle_handler, g_source_remove);

	for (i = 0; i < LAST_ACTION; i++)
		g_free (priv->action_descriptions [i]);

	clear_children (EV_VIEW_ACCESSIBLE (object));

	G_OBJECT_CLASS (ev_view_accessible_parent_class)->finalize (object);
}

static void
ev_view_accessible_initialize (AtkObject *obj,
			       gpointer   data)
{
	EvViewAccessiblePrivate *priv;

	if (ATK_OBJECT_CLASS (ev_view_accessible_parent_class)->initialize != NULL)
		ATK_OBJECT_CLASS (ev_view_accessible_parent_class)->initialize (obj, data);

	gtk_accessible_set_widget (GTK_ACCESSIBLE (obj), GTK_WIDGET (data));

	atk_object_set_name (obj, _("Document View"));
	atk_object_set_role (obj, ATK_ROLE_DOCUMENT_FRAME);

	priv = EV_VIEW_ACCESSIBLE (obj)->priv;
	priv->previous_cursor_page = -1;
	priv->start_page = 0;
	priv->end_page = -1;
}

gint
ev_view_accessible_get_n_pages (EvViewAccessible *self)
{
	return self->priv->children == NULL ? 0 : self->priv->children->len;
}

static AtkObject *
ev_view_accessible_ref_child (AtkObject *obj,
			      gint       i)
{
	EvViewAccessible *self;
	EvView *view;

	g_return_val_if_fail (EV_IS_VIEW_ACCESSIBLE (obj), NULL);
	self = EV_VIEW_ACCESSIBLE (obj);
	g_return_val_if_fail (i >= 0 || i < ev_view_accessible_get_n_pages (self), NULL);

	view = EV_VIEW (gtk_accessible_get_widget (GTK_ACCESSIBLE (obj)));
	if (view == NULL)
		return NULL;

	/* If a given page is requested, we assume that the text would
	 * be requested soon, so we need to be sure that is cached.*/
	if (view->page_cache)
		ev_page_cache_ensure_page (view->page_cache, i);

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
}

static void
ev_view_accessible_init (EvViewAccessible *accessible)
{
	accessible->priv = ev_view_accessible_get_instance_private (accessible);
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

static gboolean
ev_view_accessible_idle_do_action (gpointer data)
{
	EvViewAccessiblePrivate* priv = EV_VIEW_ACCESSIBLE (data)->priv;
	EvView *view = EV_VIEW (gtk_accessible_get_widget (GTK_ACCESSIBLE (data)));

	g_signal_emit_by_name (view, "scroll", priv->idle_scroll, GTK_ORIENTATION_VERTICAL);
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

static void
ev_view_accessible_cursor_moved (EvView *view,
				 gint page,
				 gint offset,
				 EvViewAccessible *accessible)
{
	EvViewAccessiblePrivate* priv = accessible->priv;
	EvPageAccessible *page_accessible = NULL;

	if (priv->previous_cursor_page != page) {
		AtkObject *current_page = NULL;

		if (priv->previous_cursor_page >= 0) {
			AtkObject *previous_page = NULL;
			previous_page = g_ptr_array_index (priv->children,
							   priv->previous_cursor_page);
			atk_object_notify_state_change (previous_page, ATK_STATE_FOCUSED, FALSE);
		}

		priv->previous_cursor_page = page;
		current_page = g_ptr_array_index (priv->children, page);
		atk_object_notify_state_change (current_page, ATK_STATE_FOCUSED, TRUE);

#if ATK_CHECK_VERSION (2, 11, 2)
		/* +1 as user start to count on 1, but evince starts on 0 */
		g_signal_emit_by_name (accessible, "page-changed", page + 1);
#endif
	}

	page_accessible = g_ptr_array_index (priv->children, page);
	g_signal_emit_by_name (page_accessible, "text-caret-moved", offset);
}

static void
ev_view_accessible_selection_changed (EvView *view,
				      EvViewAccessible *view_accessible)
{
	AtkObject *page_accessible;

	page_accessible = g_ptr_array_index (view_accessible->priv->children,
					     get_relevant_page (view));
	g_signal_emit_by_name (page_accessible, "text-selection-changed");
}

static void
page_changed_cb (EvDocumentModel  *model,
		 gint              old_page,
		 gint              new_page,
		 EvViewAccessible *accessible)
{
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

	/* Check for potential integer overflow in allocation - Issue #2094 */
	if ((gsize)n_pages > G_MAXSIZE / sizeof(EvPageAccessible))
		g_error ("Exiting program due to abnormal page count detected: %d", n_pages);

	self->priv->children = g_ptr_array_new_full (n_pages, (GDestroyNotify) g_object_unref);
	for (i = 0; i < n_pages; i++) {
		child = ev_page_accessible_new (self, i);
		g_ptr_array_add (self->priv->children, child);
	}

        /* When a document is reloaded, it may have less pages.
         * We need to update the end page accordingly to avoid
         * invalid access to self->priv->children
         * See https://bugzilla.gnome.org/show_bug.cgi?id=735744
         */
	if (self->priv->end_page >= n_pages)
		self->priv->end_page = n_pages - 1;
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

static gboolean
ev_view_accessible_focus_changed (GtkWidget        *widget,
				  GdkEventFocus    *event,
				  EvViewAccessible *self)
{
	AtkObject *page_accessible;

	g_return_val_if_fail (EV_IS_VIEW (widget), FALSE);
	g_return_val_if_fail (EV_IS_VIEW_ACCESSIBLE (self), FALSE);

	if (self->priv->children == NULL || self->priv->children->len == 0)
		return FALSE;

	page_accessible = g_ptr_array_index (self->priv->children,
					     get_relevant_page (EV_VIEW (widget)));
	atk_object_notify_state_change (page_accessible,
					ATK_STATE_FOCUSED, event->in);

	return FALSE;
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
	g_signal_connect (widget, "focus-in-event",
			  G_CALLBACK (ev_view_accessible_focus_changed),
			  accessible);
	g_signal_connect (widget, "focus-out-event",
			  G_CALLBACK (ev_view_accessible_focus_changed),
			  accessible);

	view = EV_VIEW (widget);
	if (view->model)
		ev_view_accessible_set_model (EV_VIEW_ACCESSIBLE (accessible),
					      view->model);

	return accessible;
}

gint
ev_view_accessible_get_relevant_page (EvViewAccessible *accessible)
{
	EvView *view;

	g_return_val_if_fail (EV_IS_VIEW_ACCESSIBLE (accessible), -1);

	view = EV_VIEW (gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible)));

	return get_relevant_page (view);
}

void
_transform_doc_rect_to_atk_rect (EvViewAccessible *accessible,
				 gint              page,
				 EvRectangle      *doc_rect,
				 EvRectangle      *atk_rect,
				 AtkCoordType      coord_type)
{
	EvView *view;
	GdkRectangle view_rect;
	GtkWidget *widget, *toplevel;
	gint x_widget, y_widget;

	view = EV_VIEW (gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible)));
	_ev_view_transform_doc_rect_to_view_rect (view, page, doc_rect, &view_rect);
	view_rect.x -= view->scroll_x;
	view_rect.y -= view->scroll_y;

	widget = GTK_WIDGET (view);
	toplevel = gtk_widget_get_toplevel (widget);
	gtk_widget_translate_coordinates (widget, toplevel, 0, 0, &x_widget, &y_widget);
	view_rect.x += x_widget;
	view_rect.y += y_widget;

	if (coord_type == ATK_XY_SCREEN) {
		gint x_window, y_window;
		gdk_window_get_origin (gtk_widget_get_window (toplevel), &x_window, &y_window);
		view_rect.x += x_window;
		view_rect.y += y_window;
	}

	atk_rect->x1 = view_rect.x;
	atk_rect->y1 = view_rect.y;
	atk_rect->x2 = view_rect.x + view_rect.width;
	atk_rect->y2 = view_rect.y + view_rect.height;
}

gboolean
ev_view_accessible_is_doc_rect_showing (EvViewAccessible *accessible,
					gint              page,
					EvRectangle      *doc_rect)
{
	EvView *view;
	GdkRectangle view_rect;
	GtkAllocation allocation;
	gint x, y;
	gboolean hidden;

	view = EV_VIEW (gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible)));
	if (page < view->start_page || page > view->end_page)
		return FALSE;

	gtk_widget_get_allocation (GTK_WIDGET (view), &allocation);
	x = gtk_adjustment_get_value (view->hadjustment);
	y = gtk_adjustment_get_value (view->vadjustment);

	_ev_view_transform_doc_rect_to_view_rect (view, page, doc_rect, &view_rect);
	hidden = view_rect.x + view_rect.width < x || view_rect.x > x + allocation.width ||
		view_rect.y + view_rect.height < y || view_rect.y > y + allocation.height;

	return !hidden;
}

void
ev_view_accessible_set_page_range (EvViewAccessible *accessible,
				   gint start,
				   gint end)
{
	gint i;
	AtkObject *page;

	g_return_if_fail (EV_IS_VIEW_ACCESSIBLE (accessible));

	for (i = accessible->priv->start_page; i <= accessible->priv->end_page; i++) {
		if (i < start || i > end) {
			page = g_ptr_array_index (accessible->priv->children, i);
			atk_object_notify_state_change (page, ATK_STATE_SHOWING, FALSE);
		}
	}

	for (i = start; i <= end; i++) {
		if (i < accessible->priv->start_page || i > accessible->priv->end_page) {
			page = g_ptr_array_index (accessible->priv->children, i);
			atk_object_notify_state_change (page, ATK_STATE_SHOWING, TRUE);
		}
	}

	accessible->priv->start_page = start;
	accessible->priv->end_page = end;
}

void
ev_view_accessible_set_focused_element (EvViewAccessible *accessible,
					EvMapping        *new_focus,
					gint              new_focus_page)
{
	EvPageAccessible *page;

	if (accessible->priv->focused_element) {
		atk_object_notify_state_change (accessible->priv->focused_element, ATK_STATE_FOCUSED, FALSE);
		accessible->priv->focused_element = NULL;
	}

	if (!new_focus || new_focus_page == -1)
		return;

	page = g_ptr_array_index (accessible->priv->children, new_focus_page);
	accessible->priv->focused_element = ev_page_accessible_get_accessible_for_mapping (page, new_focus);
	if (accessible->priv->focused_element)
		atk_object_notify_state_change (accessible->priv->focused_element, ATK_STATE_FOCUSED, TRUE);
}

void
ev_view_accessible_update_element_state (EvViewAccessible *accessible,
					 EvMapping        *element,
					 gint              element_page)
{
	EvPageAccessible *page;

	page = g_ptr_array_index (accessible->priv->children, element_page);
	ev_page_accessible_update_element_state (page, element);
}
