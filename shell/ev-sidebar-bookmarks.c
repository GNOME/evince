/* ev-sidebar-bookmarks.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2010 Carlos Garcia Campos  <carlosgc@gnome.org>
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
        PROP_WIDGET
};

enum {
        COLUMN_MARKUP,
        COLUMN_PAGE,
        N_COLUMNS
};

struct _EvSidebarBookmarksPrivate {
        EvDocumentModel *model;
        EvBookmarks     *bookmarks;

        GtkWidget       *tree_view;
        GtkWidget       *del_button;
        GtkWidget       *add_button;

        /* Popup menu */
        GtkWidget       *popup;
        GtkUIManager    *ui_manager;
        GtkActionGroup  *action_group;
};

static void ev_sidebar_bookmarks_page_iface_init (EvSidebarPageInterface *iface);

G_DEFINE_TYPE_EXTENDED (EvSidebarBookmarks,
                        ev_sidebar_bookmarks,
                        GTK_TYPE_BOX,
                        0,
                        G_IMPLEMENT_INTERFACE (EV_TYPE_SIDEBAR_PAGE,
                                               ev_sidebar_bookmarks_page_iface_init))

static const gchar popup_menu_ui[] =
        "<popup name=\"BookmarksPopup\" action=\"BookmarksPopupAction\">\n"
        "  <menuitem name=\"OpenBookmark\" action=\"OpenBookmark\"/>\n"
        "  <separator/>\n"
        "  <menuitem name=\"RenameBookmark\" action=\"RenameBookmark\"/>\n"
        "  <menuitem name=\"RemoveBookmark\" action=\"RemoveBookmark\"/>\n"
        "</popup>\n";

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
ev_bookmarks_popup_cmd_open_bookmark (GtkAction          *action,
                                      EvSidebarBookmarks *sidebar_bookmarks)
{
        EvSidebarBookmarksPrivate *priv = sidebar_bookmarks->priv;
        GtkTreeSelection          *selection;
        gint                       page;

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));
        page = ev_sidebar_bookmarks_get_selected_page (sidebar_bookmarks, selection);
        ev_document_model_set_page (priv->model, page);
}

static void
ev_bookmarks_popup_cmd_rename_bookmark (GtkAction          *action,
                                        EvSidebarBookmarks *sidebar_bookmarks)
{
        EvSidebarBookmarksPrivate *priv = sidebar_bookmarks->priv;
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
ev_bookmarks_popup_cmd_remove_bookmark (GtkAction          *action,
                                        EvSidebarBookmarks *sidebar_bookmarks)
{
        EvSidebarBookmarksPrivate *priv = sidebar_bookmarks->priv;
        GtkTreeSelection          *selection;
        gint                       page;
        EvBookmark                 bm;

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));
        page = ev_sidebar_bookmarks_get_selected_page (sidebar_bookmarks, selection);
        bm.page = page;
        bm.title = NULL;
        ev_bookmarks_delete (priv->bookmarks, &bm);
}

static const GtkActionEntry popup_entries[] = {
        { "OpenBookmark", GTK_STOCK_OPEN, N_("_Open Bookmark"), NULL,
          NULL, G_CALLBACK (ev_bookmarks_popup_cmd_open_bookmark) },
        { "RenameBookmark", NULL, N_("_Rename Bookmark"), NULL,
          NULL, G_CALLBACK (ev_bookmarks_popup_cmd_rename_bookmark) },
        { "RemoveBookmark", NULL, N_("_Remove Bookmark"), NULL,
          NULL, G_CALLBACK (ev_bookmarks_popup_cmd_remove_bookmark) }
};

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
        EvSidebarBookmarksPrivate *priv = sidebar_bookmarks->priv;
        GtkListStore              *model;
        GList                     *items, *l;
        GtkTreeIter                iter;

        model = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree_view)));
        gtk_list_store_clear (model);

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
        EvSidebarBookmarksPrivate *priv = sidebar_bookmarks->priv;
        gint                       page;

        page = ev_sidebar_bookmarks_get_selected_page (sidebar_bookmarks, selection);
        if (page >= 0) {
                ev_document_model_set_page (priv->model, page);
                gtk_widget_set_sensitive (priv->del_button, TRUE);
        } else {
                gtk_widget_set_sensitive (priv->del_button, FALSE);
        }
}

static void
ev_sidebar_bookmarks_del_clicked (GtkWidget          *button,
                                  EvSidebarBookmarks *sidebar_bookmarks)
{
        EvSidebarBookmarksPrivate *priv = sidebar_bookmarks->priv;
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
}

static void
ev_sidebar_bookmarks_bookmark_renamed (GtkCellRendererText *renderer,
                                       const gchar         *path_string,
                                       const gchar         *new_text,
                                       EvSidebarBookmarks  *sidebar_bookmarks)
{
        EvSidebarBookmarksPrivate *priv = sidebar_bookmarks->priv;
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
        bm.title = g_strdup (new_text);
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
        EvSidebarBookmarksPrivate *priv = sidebar_bookmarks->priv;
        GtkTreeModel              *model;
        GtkTreeIter                iter;
        GtkTreePath               *path = NULL;
        EvDocument                *document;
        guint                      page;
        gchar                     *page_label;
        gchar                     *text;

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree_view));
        if (!gtk_tree_view_get_tooltip_context (GTK_TREE_VIEW (priv->tree_view),
                                                &x, &y, keyboard_tip,
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

static gboolean
ev_sidebar_bookmarks_popup_menu_show (EvSidebarBookmarks *sidebar_bookmarks,
                                      gint                x,
                                      gint                y,
                                      gboolean            keyboard_mode)
{
        EvSidebarBookmarksPrivate *priv = sidebar_bookmarks->priv;
        GtkTreeView               *tree_view = GTK_TREE_VIEW (sidebar_bookmarks->priv->tree_view);
        GtkTreeSelection          *selection = gtk_tree_view_get_selection (tree_view);

        if (keyboard_mode) {
                if (!gtk_tree_selection_get_selected (selection, NULL, NULL))
                        return FALSE;
        } else {
                GtkTreePath *path;

                if (!gtk_tree_view_get_path_at_pos (tree_view, x, y, &path, NULL, NULL, NULL))
                        return FALSE;

                g_signal_handlers_block_by_func (selection,
                                                 ev_sidebar_bookmarks_selection_changed,
                                                 sidebar_bookmarks);
                gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);
                g_signal_handlers_unblock_by_func (selection,
                                                   ev_sidebar_bookmarks_selection_changed,
                                                   sidebar_bookmarks);
                gtk_tree_path_free (path);
        }

        if (!priv->popup)
                priv->popup = gtk_ui_manager_get_widget (priv->ui_manager, "/BookmarksPopup");

        gtk_menu_popup (GTK_MENU (priv->popup),
                        NULL, NULL,
                        keyboard_mode ? ev_gui_menu_position_tree_selection : NULL,
                        keyboard_mode ? tree_view : NULL,
                        keyboard_mode ? 0 : 3,
                        gtk_get_current_event_time ());
        return TRUE;
}

static gboolean
ev_sidebar_bookmarks_button_press (GtkWidget          *widget,
                                   GdkEventButton     *event,
                                   EvSidebarBookmarks *sidebar_bookmarks)
{
        if (event->button != 3)
                return FALSE;

        return ev_sidebar_bookmarks_popup_menu_show (sidebar_bookmarks, event->x, event->y, FALSE);
}

static gboolean
ev_sidebar_bookmarks_popup_menu (GtkWidget *widget)
{
        EvSidebarBookmarks *sidebar_bookmarks = EV_SIDEBAR_BOOKMARKS (widget);
        gint                x, y;

        ev_document_misc_get_pointer_position (widget, &x, &y);
        return ev_sidebar_bookmarks_popup_menu_show (sidebar_bookmarks, x, y, TRUE);
}

static void
ev_sidebar_bookmarks_dispose (GObject *object)
{
        EvSidebarBookmarks *sidebar_bookmarks = EV_SIDEBAR_BOOKMARKS (object);
        EvSidebarBookmarksPrivate *priv = sidebar_bookmarks->priv;

        if (priv->model) {
                g_object_unref (priv->model);
                priv->model = NULL;
        }

        if (priv->bookmarks) {
                g_object_unref (priv->bookmarks);
                priv->bookmarks = NULL;
        }

        if (priv->action_group) {
                g_object_unref (priv->action_group);
                priv->action_group = NULL;
        }

        if (priv->ui_manager) {
                g_object_unref (priv->ui_manager);
                priv->ui_manager = NULL;
        }

        G_OBJECT_CLASS (ev_sidebar_bookmarks_parent_class)->dispose (object);
}

static void
ev_sidebar_bookmarks_init (EvSidebarBookmarks *sidebar_bookmarks)
{
        EvSidebarBookmarksPrivate *priv;
        GtkWidget                 *swindow;
        GtkWidget                 *hbox;
        GtkListStore              *model;
        GtkCellRenderer           *renderer;
        GtkTreeSelection          *selection;

        sidebar_bookmarks->priv = G_TYPE_INSTANCE_GET_PRIVATE (sidebar_bookmarks,
                                                               EV_TYPE_SIDEBAR_BOOKMARKS,
                                                               EvSidebarBookmarksPrivate);
        priv = sidebar_bookmarks->priv;

        gtk_box_set_spacing (GTK_BOX (sidebar_bookmarks), 6);

        swindow = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swindow),
                                             GTK_SHADOW_IN);
        gtk_box_pack_start (GTK_BOX (sidebar_bookmarks), swindow, TRUE, TRUE, 0);
        gtk_widget_show (swindow);

        model = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_UINT);
        priv->tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
        g_object_unref (model);
        g_signal_connect (priv->tree_view, "query-tooltip",
                          G_CALLBACK (ev_sidebar_bookmarks_query_tooltip),
                          sidebar_bookmarks);
        g_signal_connect (priv->tree_view,
                          "button-press-event",
                          G_CALLBACK (ev_sidebar_bookmarks_button_press),
                          sidebar_bookmarks);
        gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (priv->tree_view), FALSE);
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));
        g_signal_connect (selection, "changed",
                          G_CALLBACK (ev_sidebar_bookmarks_selection_changed),
                          sidebar_bookmarks);

        renderer = gtk_cell_renderer_text_new ();
        g_object_set (renderer,
                      "ellipsize", PANGO_ELLIPSIZE_END,
                      "editable", TRUE,
                      NULL);
        g_signal_connect (renderer, "edited",
                          G_CALLBACK (ev_sidebar_bookmarks_bookmark_renamed),
                          sidebar_bookmarks);
        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (priv->tree_view),
                                                     0, NULL, renderer,
                                                     "markup", COLUMN_MARKUP,
                                                     NULL);
        gtk_container_add (GTK_CONTAINER (swindow), priv->tree_view);
        gtk_widget_show (priv->tree_view);

        hbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);

        priv->add_button = gtk_button_new_with_label (_("Add"));
        gtk_actionable_set_action_name (GTK_ACTIONABLE (priv->add_button),
                                        "win.add-bookmark");
        gtk_widget_set_sensitive (priv->add_button, FALSE);
        gtk_box_pack_start (GTK_BOX (hbox), priv->add_button, TRUE, TRUE, 6);
        gtk_widget_show (priv->add_button);

        priv->del_button = gtk_button_new_with_label (_("Remove"));
        g_signal_connect (priv->del_button, "clicked",
                          G_CALLBACK (ev_sidebar_bookmarks_del_clicked),
                          sidebar_bookmarks);
        gtk_widget_set_sensitive (priv->del_button, FALSE);
        gtk_box_pack_start (GTK_BOX (hbox), priv->del_button, TRUE, TRUE, 6);
        gtk_widget_show (priv->del_button);

        gtk_box_pack_end (GTK_BOX (sidebar_bookmarks), hbox, FALSE, TRUE, 0);
        gtk_widget_show (hbox);
        gtk_widget_show (GTK_WIDGET (sidebar_bookmarks));

        /* Popup menu */
        priv->action_group = gtk_action_group_new ("BookmarsPopupActions");
        gtk_action_group_set_translation_domain (priv->action_group, NULL);
        gtk_action_group_add_actions (priv->action_group, popup_entries,
                                      G_N_ELEMENTS (popup_entries),
                                      sidebar_bookmarks);
        priv->ui_manager = gtk_ui_manager_new ();
        gtk_ui_manager_insert_action_group (priv->ui_manager,
                                            priv->action_group, 0);
        gtk_ui_manager_add_ui_from_string (priv->ui_manager, popup_menu_ui, -1, NULL);
}

static void
ev_sidebar_bookmarks_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
        EvSidebarBookmarks *sidebar_bookmarks;

        sidebar_bookmarks = EV_SIDEBAR_BOOKMARKS (object);

        switch (prop_id) {
        case PROP_WIDGET:
                g_value_set_object (value, sidebar_bookmarks->priv->tree_view);
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
        g_object_class->dispose = ev_sidebar_bookmarks_dispose;

        widget_class->popup_menu = ev_sidebar_bookmarks_popup_menu;

        g_type_class_add_private (g_object_class, sizeof (EvSidebarBookmarksPrivate));

        g_object_class_override_property (g_object_class, PROP_WIDGET, "main-widget");
}

GtkWidget *
ev_sidebar_bookmarks_new (void)
{
        return GTK_WIDGET (g_object_new (EV_TYPE_SIDEBAR_BOOKMARKS,
                                         "orientation", GTK_ORIENTATION_VERTICAL,
                                         NULL));
}

void
ev_sidebar_bookmarks_set_bookmarks (EvSidebarBookmarks *sidebar_bookmarks,
                                    EvBookmarks        *bookmarks)
{
        EvSidebarBookmarksPrivate *priv = sidebar_bookmarks->priv;

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
        EvSidebarBookmarksPrivate *priv = sidebar_bookmarks->priv;

        if (priv->model == model)
                return;

        if (priv->model)
                g_object_unref (priv->model);
        priv->model = g_object_ref (model);
}

static gboolean
ev_sidebar_bookmarks_support_document (EvSidebarPage *sidebar_page,
                                       EvDocument    *document)
{
        return TRUE;
}

static const gchar *
ev_sidebar_bookmarks_get_label (EvSidebarPage *sidebar_page)
{
        return _("Bookmarks");
}

static void
ev_sidebar_bookmarks_page_iface_init (EvSidebarPageInterface *iface)
{
        iface->support_document = ev_sidebar_bookmarks_support_document;
        iface->set_model = ev_sidebar_bookmarks_set_model;
        iface->get_label = ev_sidebar_bookmarks_get_label;
}
