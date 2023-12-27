/* ev-sidebar-bookmarks.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2010 Carlos Garcia Campos  <carlosgc@gnome.org>
 * Copyright (C) 2020 Germán Poo-Caamaño  <gpoo@gnome.org>
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

#include "config.h"

#include <glib/gi18n.h>

#include "ev-sidebar-bookmarks.h"

#include "ev-document.h"
#include "ev-document-misc.h"
#include "ev-sidebar-page.h"
#include "ev-utils.h"

enum {
	PROP_0,
	PROP_WIDGET,
	PROP_DOCUMENT_MODEL
};

enum {
        COLUMN_MARKUP,
        COLUMN_PAGE,
        N_COLUMNS
};

enum {
        ACTIVATED,
        N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

struct _EvSidebarBookmarksPrivate {
        EvDocumentModel *model;
        EvBookmarks     *bookmarks;

        GtkWidget       *tree_view;
        GtkWidget       *del_button;
        GtkWidget       *add_button;

        /* Popup menu */
        GtkWidget       *popup;
};

#define GET_PRIVATE(o) ev_sidebar_bookmarks_get_instance_private (o);

static void ev_sidebar_bookmarks_page_iface_init (EvSidebarPageInterface *iface);
static void ev_sidebar_bookmarks_page_changed (EvSidebarBookmarks *sidebar_bookmarks,
                                               gint                old_page,
                                               gint                new_page);
static void ev_sidebar_bookmarks_set_model (EvSidebarPage   *sidebar_page,
					    EvDocumentModel *model);
static void ev_sidebar_bookmarks_selection_changed (GtkTreeSelection   *selection,
                                		    EvSidebarBookmarks *sidebar_bookmarks);


G_DEFINE_TYPE_EXTENDED (EvSidebarBookmarks,
                        ev_sidebar_bookmarks,
                        GTK_TYPE_BOX,
                        0,
                        G_ADD_PRIVATE (EvSidebarBookmarks)
                        G_IMPLEMENT_INTERFACE (EV_TYPE_SIDEBAR_PAGE,
                                               ev_sidebar_bookmarks_page_iface_init))

static gint
ev_sidebar_bookmarks_get_selected_page (EvSidebarBookmarks *sidebar_bookmarks,
                                        GtkTreeSelection   *selection)
{
        GtkTreeModel *model;
        GtkTreeIter   iter;

        if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
                guint page;

                gtk_tree_model_get (model, &iter,
                                    COLUMN_PAGE, &page,
                                    -1);
                return page;
        }

        return -1;
}

static void
ev_bookmarks_popup_cmd_open_bookmark (GSimpleAction *action,
				      GVariant      *parameter,
                                      gpointer       sidebar_bookmarks)
{
        EvSidebarBookmarksPrivate *priv = GET_PRIVATE (sidebar_bookmarks);
        GtkTreeSelection          *selection;
        gint                       page;
        gint old_page = ev_document_model_get_page (priv->model);

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));
        page = ev_sidebar_bookmarks_get_selected_page (sidebar_bookmarks, selection);
        g_signal_emit (sidebar_bookmarks, signals[ACTIVATED], 0, old_page, page);
        ev_document_model_set_page (priv->model, page);
}

static void
ev_bookmarks_popup_cmd_rename_bookmark (GSimpleAction *action,
					GVariant      *parameter,
                                        gpointer       sidebar_bookmarks)
{
        EvSidebarBookmarksPrivate *priv = GET_PRIVATE (sidebar_bookmarks);
        GtkTreeView               *tree_view = GTK_TREE_VIEW (priv->tree_view);
        GtkTreeSelection          *selection;
        GtkTreeModel              *model;
        GtkTreeIter                iter;


        selection = gtk_tree_view_get_selection (tree_view);
        if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
                GtkTreePath *path;

                path = gtk_tree_model_get_path (model, &iter);
                gtk_tree_view_set_cursor (tree_view, path,
                                          gtk_tree_view_get_column (tree_view, 0),
                                          TRUE);
                gtk_tree_path_free (path);
        }
}

static void
ev_bookmarks_popup_cmd_delete_bookmark (GSimpleAction *action,
					GVariant      *parameter,
                                        gpointer       sidebar_bookmarks)
{
        EvSidebarBookmarksPrivate *priv = GET_PRIVATE (sidebar_bookmarks);
        GtkTreeSelection          *selection;
        gint                       page;
        EvBookmark                 bm;

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));
        page = ev_sidebar_bookmarks_get_selected_page (sidebar_bookmarks, selection);
        bm.page = page;
        bm.title = NULL;
        ev_bookmarks_delete (priv->bookmarks, &bm);
	if (gtk_widget_get_sensitive (priv->del_button))
		gtk_widget_set_sensitive (priv->del_button, FALSE);
}

static GActionGroup *
create_action_group (EvSidebarBookmarks *sidebar_bookmarks) {
	const GActionEntry popup_entries[] = {
		{ "open-bookmark", ev_bookmarks_popup_cmd_open_bookmark },
		{ "rename-bookmark", ev_bookmarks_popup_cmd_rename_bookmark },
		{ "delete-bookmark", ev_bookmarks_popup_cmd_delete_bookmark }
	};
	GSimpleActionGroup *group;

	group = g_simple_action_group_new ();
	g_action_map_add_action_entries (G_ACTION_MAP (group),
					 popup_entries,
					 G_N_ELEMENTS (popup_entries),
					 sidebar_bookmarks);

	return G_ACTION_GROUP (group);
}

static gint
compare_bookmarks (EvBookmark *a,
                   EvBookmark *b)
{
        if (a->page < b->page)
                return -1;
        if (a->page > b->page)
                return 1;
        return 0;
}

static void
ev_sidebar_bookmarks_update (EvSidebarBookmarks *sidebar_bookmarks)
{
        EvSidebarBookmarksPrivate *priv = GET_PRIVATE (sidebar_bookmarks);
        GtkListStore              *model;
        GList                     *items, *l;
        GtkTreeIter                iter;
        GtkTreeView               *tree_view = GTK_TREE_VIEW (priv->tree_view);
        GtkTreeSelection          *selection = gtk_tree_view_get_selection (tree_view);

        model = GTK_LIST_STORE (gtk_tree_view_get_model (tree_view));

        g_signal_handlers_block_by_func (selection,
                                         ev_sidebar_bookmarks_selection_changed,
                                         sidebar_bookmarks);
        gtk_list_store_clear (model);
        g_signal_handlers_unblock_by_func (selection,
                                         ev_sidebar_bookmarks_selection_changed,
                                         sidebar_bookmarks);

        if (!priv->bookmarks) {
                g_object_set (priv->tree_view, "has-tooltip", FALSE, NULL);
                return;
        }

        items = ev_bookmarks_get_bookmarks (priv->bookmarks);
        items = g_list_sort (items, (GCompareFunc)compare_bookmarks);
        for (l = items; l; l = g_list_next (l)) {
                EvBookmark *bm = (EvBookmark *)l->data;

                gtk_list_store_append (model, &iter);
                gtk_list_store_set (model, &iter,
                                    COLUMN_MARKUP, bm->title,
                                    COLUMN_PAGE, bm->page,
                                    -1);
        }
        g_list_free (items);
        g_object_set (priv->tree_view, "has-tooltip", TRUE, NULL);
}

static void
ev_sidebar_bookmarks_changed (EvBookmarks        *bookmarks,
                              EvSidebarBookmarks *sidebar_bookmarks)
{
        ev_sidebar_bookmarks_update (sidebar_bookmarks);
}

static void
ev_sidebar_bookmarks_selection_changed (GtkTreeSelection   *selection,
                                        EvSidebarBookmarks *sidebar_bookmarks)
{
        EvSidebarBookmarksPrivate *priv = GET_PRIVATE (sidebar_bookmarks);
        gint                       page;

        page = ev_sidebar_bookmarks_get_selected_page (sidebar_bookmarks, selection);
        if (page >= 0) {
                gint old_page = ev_document_model_get_page (priv->model);
                g_signal_emit (sidebar_bookmarks, signals[ACTIVATED], 0, old_page, page);
                ev_document_model_set_page (priv->model, page);
                gtk_widget_set_sensitive (priv->del_button, TRUE);
        } else {
                gtk_widget_set_sensitive (priv->del_button, FALSE);
        }
}

static void ev_sidebar_bookmarks_page_changed (EvSidebarBookmarks *sidebar_bookmarks,
                                               gint                old_page,
                                               gint                new_page)
{
        EvSidebarBookmarksPrivate *priv = GET_PRIVATE (sidebar_bookmarks);
        GtkTreeSelection          *selection;
        gint                       selected_page;

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));
        selected_page = ev_sidebar_bookmarks_get_selected_page (sidebar_bookmarks, selection);

        if (selected_page != new_page)
                gtk_tree_selection_unselect_all (selection);
}

static void
ev_sidebar_bookmarks_del_clicked (GtkWidget          *button,
                EvSidebarBookmarks *sidebar_bookmarks)
{
        EvSidebarBookmarksPrivate *priv = GET_PRIVATE (sidebar_bookmarks);
        GtkTreeSelection          *selection;
        gint                       page;
        EvBookmark                 bm;

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));
        page = ev_sidebar_bookmarks_get_selected_page (sidebar_bookmarks, selection);
        if (page < 0)
                return;

        bm.page = page;
        bm.title = NULL;
        ev_bookmarks_delete (priv->bookmarks, &bm);
	if (gtk_widget_get_sensitive (priv->del_button))
		gtk_widget_set_sensitive (priv->del_button, FALSE);
}

static void
ev_sidebar_bookmarks_bookmark_renamed (GtkCellRendererText *renderer,
                                       const gchar         *path_string,
                                       const gchar         *new_text,
                                       EvSidebarBookmarks  *sidebar_bookmarks)
{
        EvSidebarBookmarksPrivate *priv = GET_PRIVATE (sidebar_bookmarks);
        GtkTreePath               *path = gtk_tree_path_new_from_string (path_string);
        GtkTreeModel              *model;
        GtkTreeIter                iter;
        guint                      page;
        EvBookmark                 bm;

        if (!new_text || new_text[0] == '\0')
                return;

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree_view));
        gtk_tree_model_get_iter (model, &iter, path);
        gtk_tree_model_get (model, &iter,
                            COLUMN_PAGE, &page,
                            -1);
        gtk_tree_path_free (path);

        bm.page = page;
        bm.title = g_markup_escape_text (new_text, -1);
        ev_bookmarks_update (priv->bookmarks, &bm);
}

static gboolean
ev_sidebar_bookmarks_query_tooltip (GtkWidget          *widget,
                                    gint                x,
                                    gint                y,
                                    gboolean            keyboard_tip,
                                    GtkTooltip         *tooltip,
                                    EvSidebarBookmarks *sidebar_bookmarks)
{

        EvSidebarBookmarksPrivate *priv = GET_PRIVATE (sidebar_bookmarks);
        GtkTreeModel              *model;
        GtkTreeIter                iter;
        GtkTreePath               *path = NULL;
        EvDocument                *document;
        guint                      page;
        gchar                     *page_label;
        gchar                     *text;

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree_view));
        if (!gtk_tree_view_get_tooltip_context (GTK_TREE_VIEW (priv->tree_view),
                                                x, y, keyboard_tip,
                                                &model, &path, &iter))
                return FALSE;

        gtk_tree_model_get (model, &iter,
                            COLUMN_PAGE, &page,
                            -1);

        document = ev_document_model_get_document (priv->model);
        page_label = ev_document_get_page_label (document, page);
        text = g_strdup_printf (_("Page %s"), page_label);
        gtk_tooltip_set_text (tooltip, text);
        g_free (text);
        g_free (page_label);

        gtk_tree_view_set_tooltip_row (GTK_TREE_VIEW (priv->tree_view),
                                       tooltip, path);
        gtk_tree_path_free (path);

        return TRUE;
}

static void
ev_sidebar_bookmarks_button_press_cb (GtkGestureClick    *self,
				      gint		  n_press,
				      gdouble		  x,
				      gdouble		  y,
				      EvSidebarBookmarks *sidebar_bookmarks)
{
	EvSidebarBookmarksPrivate *priv = GET_PRIVATE (sidebar_bookmarks);
	GtkTreeView               *tree_view = GTK_TREE_VIEW (priv->tree_view);
	GtkTreeSelection          *selection = gtk_tree_view_get_selection (tree_view);
	GdkEvent *event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (self));
	GtkTreePath *path;

	if (!gdk_event_triggers_context_menu (event))
		return;

	if (!gtk_tree_view_get_path_at_pos (tree_view, x, y,
					    &path, NULL, NULL, NULL))
		return;

	g_signal_handlers_block_by_func (selection,
					 ev_sidebar_bookmarks_selection_changed,
					 sidebar_bookmarks);
	gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);
	g_signal_handlers_unblock_by_func (selection,
					   ev_sidebar_bookmarks_selection_changed,
					   sidebar_bookmarks);
	gtk_tree_path_free (path);

	if (!gtk_widget_get_sensitive (priv->del_button))
		gtk_widget_set_sensitive (priv->del_button, TRUE);

	gtk_popover_set_pointing_to (GTK_POPOVER (priv->popup), &(const GdkRectangle){ x, y, 1, 1 });
        gtk_popover_popup (GTK_POPOVER (priv->popup));
}

static void
ev_sidebar_bookmarks_dispose (GObject *object)
{
        EvSidebarBookmarks *sidebar_bookmarks = EV_SIDEBAR_BOOKMARKS (object);
        EvSidebarBookmarksPrivate *priv = GET_PRIVATE (sidebar_bookmarks);

	g_clear_object (&priv->model);
	g_clear_object (&priv->bookmarks);

        G_OBJECT_CLASS (ev_sidebar_bookmarks_parent_class)->dispose (object);
}

static void
ev_sidebar_bookmarks_init (EvSidebarBookmarks *sidebar_bookmarks)
{
        EvSidebarBookmarksPrivate *priv = GET_PRIVATE (sidebar_bookmarks);

	gtk_widget_init_template (GTK_WIDGET (sidebar_bookmarks));

	gtk_widget_insert_action_group (priv->popup,
					"bookmarks",
					create_action_group (sidebar_bookmarks));
}

static void
ev_sidebar_bookmarks_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
        EvSidebarBookmarks *sidebar_bookmarks = EV_SIDEBAR_BOOKMARKS (object);
	EvSidebarBookmarksPrivate *priv = GET_PRIVATE (sidebar_bookmarks);

        switch (prop_id) {
        case PROP_WIDGET:
                g_value_set_object (value, priv->tree_view);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
ev_sidebar_bookmarks_set_property (GObject      *object,
				   guint         prop_id,
				   const GValue *value,
				   GParamSpec   *pspec)
{
	EvSidebarBookmarks *sidebar_bookmarks = EV_SIDEBAR_BOOKMARKS (object);

	switch (prop_id)
	{
	case PROP_DOCUMENT_MODEL:
		ev_sidebar_bookmarks_set_model (EV_SIDEBAR_PAGE (sidebar_bookmarks),
			EV_DOCUMENT_MODEL (g_value_get_object (value)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ev_sidebar_bookmarks_class_init (EvSidebarBookmarksClass *klass)
{
        GObjectClass   *g_object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        g_object_class->get_property = ev_sidebar_bookmarks_get_property;
        g_object_class->set_property = ev_sidebar_bookmarks_set_property;
        g_object_class->dispose = ev_sidebar_bookmarks_dispose;

	gtk_widget_class_set_template_from_resource (widget_class,
			"/org/gnome/evince/ui/sidebar-bookmarks.ui");
	gtk_widget_class_bind_template_child_private (widget_class, EvSidebarBookmarks, tree_view);
	gtk_widget_class_bind_template_child_private (widget_class, EvSidebarBookmarks, del_button);
	gtk_widget_class_bind_template_child_private (widget_class, EvSidebarBookmarks, add_button);
	gtk_widget_class_bind_template_child_private (widget_class, EvSidebarBookmarks, popup);

	gtk_widget_class_bind_template_callback (widget_class, ev_sidebar_bookmarks_del_clicked);
	gtk_widget_class_bind_template_callback (widget_class, ev_sidebar_bookmarks_bookmark_renamed);
	gtk_widget_class_bind_template_callback (widget_class, ev_sidebar_bookmarks_query_tooltip);
	gtk_widget_class_bind_template_callback (widget_class, ev_sidebar_bookmarks_selection_changed);
	gtk_widget_class_bind_template_callback (widget_class, ev_sidebar_bookmarks_query_tooltip);
	gtk_widget_class_bind_template_callback (widget_class, ev_sidebar_bookmarks_button_press_cb);

        g_object_class_override_property (g_object_class, PROP_WIDGET, "main-widget");
	g_object_class_override_property (g_object_class, PROP_DOCUMENT_MODEL, "document-model");

	/* Signals */
        signals[ACTIVATED] =
                g_signal_new ("bookmark-activated",
                              EV_TYPE_SIDEBAR_BOOKMARKS,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (EvSidebarBookmarksClass, activated),
                              NULL, NULL,
                              NULL,
                              G_TYPE_NONE, 2,
                              G_TYPE_INT, G_TYPE_INT);
}

GtkWidget *
ev_sidebar_bookmarks_new (void)
{
        return GTK_WIDGET (g_object_new (EV_TYPE_SIDEBAR_BOOKMARKS, NULL));
}

void
ev_sidebar_bookmarks_set_bookmarks (EvSidebarBookmarks *sidebar_bookmarks,
                                    EvBookmarks        *bookmarks)
{
        EvSidebarBookmarksPrivate *priv = GET_PRIVATE (sidebar_bookmarks);

        g_return_if_fail (EV_IS_BOOKMARKS (bookmarks));

        if (priv->bookmarks == bookmarks)
                return;

        if (priv->bookmarks)
                g_object_unref (priv->bookmarks);
        priv->bookmarks = g_object_ref (bookmarks);
        g_signal_connect (priv->bookmarks, "changed",
                          G_CALLBACK (ev_sidebar_bookmarks_changed),
                          sidebar_bookmarks);

        gtk_widget_set_sensitive (priv->add_button, TRUE);
        ev_sidebar_bookmarks_update (sidebar_bookmarks);
}

/* EvSidebarPageIface */
static void
ev_sidebar_bookmarks_set_model (EvSidebarPage   *sidebar_page,
                                EvDocumentModel *model)
{
        EvSidebarBookmarks *sidebar_bookmarks = EV_SIDEBAR_BOOKMARKS (sidebar_page);
        EvSidebarBookmarksPrivate *priv = GET_PRIVATE (sidebar_bookmarks);

        if (priv->model == model)
                return;

        if (priv->model)
                g_object_unref (priv->model);
        priv->model = g_object_ref (model);
        g_signal_connect_swapped (model, "page-changed",
                                  G_CALLBACK (ev_sidebar_bookmarks_page_changed),
                                  sidebar_page);
}

static gboolean
ev_sidebar_bookmarks_support_document (EvSidebarPage *sidebar_page,
                                       EvDocument    *document)
{
        return TRUE;
}

static void
ev_sidebar_bookmarks_page_iface_init (EvSidebarPageInterface *iface)
{
        iface->support_document = ev_sidebar_bookmarks_support_document;
        iface->set_model = ev_sidebar_bookmarks_set_model;
}
