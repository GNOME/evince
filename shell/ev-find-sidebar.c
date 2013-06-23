/* ev-find-sidebar.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2013 Carlos Garcia Campos  <carlosgc@gnome.org>
 * Copyright (C) 2008 Sergey Pushkin  <pushkinsv@gmail.com >
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ev-find-sidebar.h"

struct _EvFindSidebarPrivate {
        GtkWidget *tree_view;

        guint selection_id;
        guint process_matches_idle_id;

        GtkTreePath *highlighted_result;
        gint         first_match_page;

        EvJobFind *job;
        gint       job_current_page;
        gint       current_page;
        gint       insert_position;
};

enum {
        TEXT_COLUMN,
        PAGE_COLUMN,
        RESULT_COLUMN,

        N_COLUMNS
};

enum {
        RESULT_ACTIVATED,
        N_SIGNALS
};

static guint signals[N_SIGNALS];

G_DEFINE_TYPE (EvFindSidebar, ev_find_sidebar, GTK_TYPE_BOX)

static void
ev_find_sidebar_cancel (EvFindSidebar *sidebar)
{
        EvFindSidebarPrivate *priv = sidebar->priv;

        if (priv->process_matches_idle_id > 0) {
                g_source_remove (priv->process_matches_idle_id);
                priv->process_matches_idle_id = 0;
        }
        g_clear_object (&priv->job);
}

static void
ev_find_sidebar_dispose (GObject *object)
{
        EvFindSidebar *sidebar = EV_FIND_SIDEBAR (object);

        ev_find_sidebar_cancel (sidebar);
        g_clear_pointer (&sidebar->priv->highlighted_result, (GDestroyNotify)gtk_tree_path_free);

        G_OBJECT_CLASS (ev_find_sidebar_parent_class)->dispose (object);
}

static void
ev_find_sidebar_class_init (EvFindSidebarClass *find_sidebar_class)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS (find_sidebar_class);

        g_object_class->dispose = ev_find_sidebar_dispose;

        signals[RESULT_ACTIVATED] =
                g_signal_new ("result-activated",
                              G_TYPE_FROM_CLASS (g_object_class),
                              G_SIGNAL_RUN_LAST,
                              0, NULL, NULL,
                              g_cclosure_marshal_generic,
                              G_TYPE_NONE, 2,
                              G_TYPE_INT,
                              G_TYPE_INT);

        g_type_class_add_private (g_object_class, sizeof (EvFindSidebarPrivate));
}

static void
selection_changed_callback (GtkTreeSelection *selection,
                            EvFindSidebar    *sidebar)
{
        EvFindSidebarPrivate *priv;
        GtkTreeModel         *model;
        GtkTreeIter           iter;

        priv = sidebar->priv;

        if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
                gint page;
                gint result;

                gtk_tree_model_get (model, &iter,
                                    PAGE_COLUMN, &page,
                                    RESULT_COLUMN, &result,
                                    -1);

                if (priv->highlighted_result)
                        gtk_tree_path_free (priv->highlighted_result);
                priv->highlighted_result = gtk_tree_model_get_path (model, &iter);

                g_signal_emit (sidebar, signals[RESULT_ACTIVATED], 0, page - 1, result);
        }
}

static void
ev_find_sidebar_init (EvFindSidebar *sidebar)
{
        EvFindSidebarPrivate *priv;
        GtkWidget            *swindow;
        GtkTreeViewColumn    *column;
        GtkCellRenderer      *renderer;
        GtkTreeModel         *model;
        GtkTreeSelection     *selection;

        sidebar->priv = G_TYPE_INSTANCE_GET_PRIVATE (sidebar, EV_TYPE_FIND_SIDEBAR, EvFindSidebarPrivate);
        priv = sidebar->priv;

        swindow = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
                                        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swindow),
                                             GTK_SHADOW_IN);

        model = GTK_TREE_MODEL (gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT));
        priv->tree_view = gtk_tree_view_new_with_model (model);
        g_object_unref (model);
        gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (priv->tree_view), FALSE);
        gtk_container_add (GTK_CONTAINER (swindow), priv->tree_view);
        gtk_widget_show (priv->tree_view);

        gtk_box_pack_start (GTK_BOX (sidebar), swindow, TRUE, TRUE, 0);
        gtk_widget_show (swindow);

        column = gtk_tree_view_column_new ();
        gtk_tree_view_column_set_expand (GTK_TREE_VIEW_COLUMN (column), TRUE);
        gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree_view), column);

        renderer = (GtkCellRenderer *)g_object_new (GTK_TYPE_CELL_RENDERER_TEXT,
                                                    "ellipsize",
                                                    PANGO_ELLIPSIZE_END,
                                                    NULL);
        gtk_tree_view_column_pack_start (GTK_TREE_VIEW_COLUMN (column), renderer, TRUE);
        gtk_tree_view_column_set_attributes (GTK_TREE_VIEW_COLUMN (column), renderer,
                                             "markup", TEXT_COLUMN,
                                             NULL);

        renderer = gtk_cell_renderer_text_new ();
        gtk_tree_view_column_pack_end (GTK_TREE_VIEW_COLUMN (column), renderer, FALSE);
        gtk_tree_view_column_set_attributes (GTK_TREE_VIEW_COLUMN (column), renderer,
                                             "text", PAGE_COLUMN,
                                             NULL);
        g_object_set (G_OBJECT (renderer), "style", PANGO_STYLE_ITALIC, NULL);

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));
        priv->selection_id = g_signal_connect (selection, "changed",
                                               G_CALLBACK (selection_changed_callback),
                                               sidebar);
}

GtkWidget *
ev_find_sidebar_new (void)
{
        return g_object_new (EV_TYPE_FIND_SIDEBAR,
                             "orientation", GTK_ORIENTATION_VERTICAL,
                             NULL);
}

static void
ev_find_sidebar_select_highlighted_result (EvFindSidebar *sidebar)
{
        EvFindSidebarPrivate *priv = sidebar->priv;
        GtkTreeSelection     *selection;

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));

        g_signal_handler_block (selection, priv->selection_id);
        gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->tree_view), priv->highlighted_result, NULL, FALSE);
        g_signal_handler_unblock (selection, priv->selection_id);
}

static gchar *
get_surrounding_text_markup (GtkTextIter *match_start,
                             GtkTextIter *match_end)
{
        gchar *match, *prec, *succ, *markup;
        GtkTextIter surrounding_start = *match_start;
        GtkTextIter surrounding_end = *match_end;

        gtk_text_iter_backward_word_start (&surrounding_start);
        gtk_text_iter_forward_word_ends (&surrounding_end, 2);

        match = gtk_text_iter_get_text (match_start, match_end);
        prec = gtk_text_iter_get_text (&surrounding_start, match_start);
        succ = gtk_text_iter_get_text (match_end, &surrounding_end);

        markup = g_markup_printf_escaped ("%s<span weight = \"bold\">%s</span>%s",
                                          prec, match, succ);
        g_free (match);
        g_free (prec);
        g_free (succ);

        return markup;
}

static gchar *
get_matched_line (EvDocument  *document,
                  EvPage      *page,
                  EvRectangle *match)
{
        EvRectangle rect;
        gchar      *tmp, *result;

        rect = *match;
        rect.y1 = rect.y2 = (rect.y1 + rect.y2) / 2;
        rect.x1 = rect.x2 = (rect.x1 + rect.x2) / 2;

        ev_document_doc_mutex_lock ();
        tmp = ev_selection_get_selected_text (EV_SELECTION (document),
                                              page,
                                              EV_SELECTION_STYLE_LINE,
                                              &rect);
        ev_document_doc_mutex_unlock ();
        result = g_utf8_normalize (tmp, -1, G_NORMALIZE_ALL);
        g_free (tmp);

        return result;

}

static gboolean
process_matches_idle (EvFindSidebar *sidebar)
{
        EvFindSidebarPrivate *priv = sidebar->priv;
        GtkTreeModel         *model;
        GtkTextBuffer        *buffer = NULL;
        GtkTextSearchFlags    search_flags = 0;
        gint                  current_page;
        EvDocument           *document;

        priv->process_matches_idle_id = 0;

        if (!ev_job_find_has_results (priv->job)) {
                if (ev_job_is_finished (EV_JOB (priv->job)))
                        g_clear_object (&priv->job);
                return FALSE;
        }

        document = EV_JOB (priv->job)->document;
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree_view));
        if (!priv->job->case_sensitive)
                search_flags = GTK_TEXT_SEARCH_CASE_INSENSITIVE;

        do {
                GList  *matches, *l;
                EvPage *page;
                gint    occ_number, k;
                gint    result;

                current_page = priv->current_page;
                priv->current_page = (priv->current_page + 1) % priv->job->n_pages;

                matches = priv->job->pages[current_page];
                if (!matches)
                        continue;

                if (priv->first_match_page == -1)
                        priv->first_match_page = current_page;

                page = ev_document_get_page (document, current_page);
                occ_number = 0;

                for (l = matches, result = 0; l; l = g_list_next (l), result++) {
                        EvRectangle *match = (EvRectangle *)l->data;
                        gchar       *matched_line;
                        gchar       *markup;
                        GtkTextIter  find_iter, start, end;
                        GtkTreeIter  iter;

                        matched_line = get_matched_line (document, page, match);
                        g_assert (matched_line != NULL);
                        if (!buffer)
                                buffer = gtk_text_buffer_new (NULL);
                        gtk_text_buffer_set_text (buffer, matched_line, -1);
                        g_free (matched_line);

                        gtk_text_buffer_get_start_iter (buffer, &find_iter);

                        /* search the proper occurrence of text in the line */
                        occ_number++;
                        for (k = 0; k < occ_number; k++) {
                                if (!gtk_text_iter_forward_search (&find_iter, priv->job->text, search_flags, &start, &end, NULL))
                                        break;

                                find_iter = end;
                                gtk_text_iter_forward_char (&find_iter);
                        }

                        /* additional search to determine that current match is the last one on the line */
                        if (!gtk_text_iter_forward_search (&find_iter, priv->job->text,
                                                           search_flags, NULL, NULL, NULL))
                                occ_number = 0;

                        if (k == 0) {
                                /* This should not be reached as the matched line should contain
                                 * the text_to_find at least once. However with poppler-0.22
                                 * we get some false positives, so we play safe by
                                 * just ignoring them */
                                continue;
                        }

                        markup = get_surrounding_text_markup (&start, &end);

                        if (current_page >= priv->job->start_page) {
                                gtk_list_store_append (GTK_LIST_STORE (model), &iter);
                        } else {
                                gtk_list_store_insert (GTK_LIST_STORE (model), &iter,
                                                       priv->insert_position);
                                priv->insert_position++;
                        }
                        gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                            TEXT_COLUMN, markup,
                                            PAGE_COLUMN, current_page + 1,
                                            RESULT_COLUMN, result,
                                            -1);
                        g_free (markup);
                }
        } while (current_page != priv->job_current_page);

        if (buffer)
                g_object_unref (buffer);

        if (ev_job_is_finished (EV_JOB (priv->job)) && priv->current_page == priv->job->start_page) {
                gint index = 0;
                gint i;

                for (i = 0; i < priv->first_match_page; i++)
                        index += ev_job_find_get_n_results (priv->job, i);

                priv->highlighted_result = gtk_tree_path_new_from_indices (index, -1);
                ev_find_sidebar_select_highlighted_result (sidebar);

                g_clear_object (&priv->job);
        }

        return FALSE;
}

static void
find_job_updated_cb (EvJobFind     *job,
                     gint           page,
                     EvFindSidebar *sidebar)
{
        sidebar->priv->job_current_page = page;
}

static void
find_job_cancelled_cb (EvJob         *job,
                       EvFindSidebar *sidebar)
{
        ev_find_sidebar_cancel (sidebar);
}

void
ev_find_sidebar_start (EvFindSidebar *sidebar,
                       EvJobFind     *job)
{
        EvFindSidebarPrivate *priv = sidebar->priv;

        if (priv->job == job)
                return;

        if (priv->process_matches_idle_id)
                g_source_remove (priv->process_matches_idle_id);
        priv->process_matches_idle_id = 0;
        gtk_list_store_clear (GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree_view))));

        g_clear_object (&priv->job);
        priv->job = g_object_ref (job);
        g_signal_connect_object (job, "updated",
                                 G_CALLBACK (find_job_updated_cb),
                                 sidebar, 0);
        g_signal_connect_object (job, "cancelled",
                                 G_CALLBACK (find_job_cancelled_cb),
                                 sidebar, 0);
        priv->job_current_page = -1;
        priv->first_match_page = -1;
        priv->current_page = job->start_page;
        priv->insert_position = 0;
        g_clear_pointer (&priv->highlighted_result, (GDestroyNotify)gtk_tree_path_free);
}

void
ev_find_sidebar_update (EvFindSidebar *sidebar)
{
        EvFindSidebarPrivate *priv = sidebar->priv;

        if (!priv->job)
                return;

        if (priv->process_matches_idle_id == 0)
                priv->process_matches_idle_id = g_idle_add ((GSourceFunc)process_matches_idle, sidebar);
}

void
ev_find_sidebar_clear (EvFindSidebar *sidebar)
{
        EvFindSidebarPrivate *priv = sidebar->priv;

        gtk_list_store_clear (GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree_view))));
        g_clear_pointer (&priv->highlighted_result, (GDestroyNotify)gtk_tree_path_free);
}

void
ev_find_sidebar_previous (EvFindSidebar *sidebar)
{
        EvFindSidebarPrivate *priv = sidebar->priv;

        if (!priv->highlighted_result)
                return;

        gtk_tree_path_prev (priv->highlighted_result);
        ev_find_sidebar_select_highlighted_result (sidebar);
}

void
ev_find_sidebar_next (EvFindSidebar *sidebar)
{
        EvFindSidebarPrivate *priv = sidebar->priv;

        if (!priv->highlighted_result)
                return;

        gtk_tree_path_next (priv->highlighted_result);
        ev_find_sidebar_select_highlighted_result (sidebar);
}
