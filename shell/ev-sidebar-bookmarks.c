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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <glib/gi18n.h>

#include "ev-sidebar-bookmarks.h"

#include "ev-document.h"
#include "ev-sidebar-page.h"

enum {
        PROP_0,
        PROP_WIDGET
};

enum {
        COLUMN_MARKUP,
        COLUMN_PAGE,
        N_COLUMNS
};

enum {
        ADD_BOOKMARK,
        N_SIGNALS
};

struct _EvSidebarBookmarksPrivate {
        EvDocumentModel *model;
        EvBookmarks     *bookmarks;

        GtkWidget       *tree_view;
        GtkWidget       *del_button;
        GtkWidget       *add_button;
};

static void ev_sidebar_bookmarks_page_iface_init (EvSidebarPageInterface *iface);

G_DEFINE_TYPE_EXTENDED (EvSidebarBookmarks,
                        ev_sidebar_bookmarks,
                        GTK_TYPE_VBOX,
                        0,
                        G_IMPLEMENT_INTERFACE (EV_TYPE_SIDEBAR_PAGE,
                                               ev_sidebar_bookmarks_page_iface_init))

static guint signals[N_SIGNALS];

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

        if (!priv->bookmarks)
                return;

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
}

static void
ev_sidebar_bookmarks_changed (EvBookmarks        *bookmarks,
                              EvSidebarBookmarks *sidebar_bookmarks)
{
        ev_sidebar_bookmarks_update (sidebar_bookmarks);
}

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
ev_sidebar_bookmarks_add_clicked (GtkWidget          *button,
                                  EvSidebarBookmarks *sidebar_bookmarks)
{
        /* Let the window add the bookmark since
         * since we don't know the page title
         */
        g_signal_emit (sidebar_bookmarks, signals[ADD_BOOKMARK], 0);
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
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swindow),
                                             GTK_SHADOW_IN);
        gtk_box_pack_start (GTK_BOX (sidebar_bookmarks), swindow, TRUE, TRUE, 0);
        gtk_widget_show (swindow);

        model = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_UINT);
        priv->tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
        g_object_unref (model);
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

        priv->add_button = gtk_button_new_from_stock (GTK_STOCK_ADD);
        g_signal_connect (priv->add_button, "clicked",
                          G_CALLBACK (ev_sidebar_bookmarks_add_clicked),
                          sidebar_bookmarks);
        gtk_widget_set_sensitive (priv->add_button, FALSE);
        gtk_box_pack_start (GTK_BOX (hbox), priv->add_button, TRUE, TRUE, 6);
        gtk_widget_show (priv->add_button);

        priv->del_button = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
        g_signal_connect (priv->del_button, "clicked",
                          G_CALLBACK (ev_sidebar_bookmarks_del_clicked),
                          sidebar_bookmarks);
        gtk_widget_set_sensitive (priv->del_button, FALSE);
        gtk_box_pack_start (GTK_BOX (hbox), priv->del_button, TRUE, TRUE, 6);
        gtk_widget_show (priv->del_button);

        gtk_box_pack_end (GTK_BOX (sidebar_bookmarks), hbox, FALSE, TRUE, 0);
        gtk_widget_show (hbox);
        gtk_widget_show (GTK_WIDGET (sidebar_bookmarks));
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
        GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

        g_object_class->get_property = ev_sidebar_bookmarks_get_property;
        g_object_class->dispose = ev_sidebar_bookmarks_dispose;

        g_type_class_add_private (g_object_class, sizeof (EvSidebarBookmarksPrivate));

        g_object_class_override_property (g_object_class, PROP_WIDGET, "main-widget");

        signals[ADD_BOOKMARK] =
                g_signal_new ("add-bookmark",
                              G_TYPE_FROM_CLASS (g_object_class),
                              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                              G_STRUCT_OFFSET (EvSidebarBookmarksClass, add_bookmark),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0,
                              G_TYPE_NONE);
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
