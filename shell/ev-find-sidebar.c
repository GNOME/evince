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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ev-find-sidebar.h"
#include <string.h>

typedef struct {
        GtkWidget *tree_view;

        guint selection_id;
        guint process_matches_idle_id;

        GtkTreePath *highlighted_result;
        gint         first_match_page;

        EvJobFind *job;
        gint       job_current_page;
        gint       current_page;
        gint       insert_position;
} EvFindSidebarPrivate;

enum {
        TEXT_COLUMN,
	PAGE_LABEL_COLUMN,
        PAGE_COLUMN,
        RESULT_COLUMN,

        N_COLUMNS
};

enum {
        RESULT_ACTIVATED,
        N_SIGNALS
};

static guint signals[N_SIGNALS];

G_DEFINE_TYPE_WITH_PRIVATE (EvFindSidebar, ev_find_sidebar, GTK_TYPE_BOX)

#define GET_PRIVATE(o) ev_find_sidebar_get_instance_private (o)

static void
ev_find_sidebar_cancel (EvFindSidebar *sidebar)
{
        EvFindSidebarPrivate *priv = GET_PRIVATE (sidebar);

	g_clear_handle_id (&priv->process_matches_idle_id, g_source_remove);
        g_clear_object (&priv->job);
}

static void
ev_find_sidebar_dispose (GObject *object)
{
        EvFindSidebar *sidebar = EV_FIND_SIDEBAR (object);
        EvFindSidebarPrivate *priv = GET_PRIVATE (sidebar);

        ev_find_sidebar_cancel (sidebar);
        g_clear_pointer (&(priv->highlighted_result), gtk_tree_path_free);

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
}

static void
ev_find_sidebar_activate_result_at_iter (EvFindSidebar *sidebar,
                                         GtkTreeModel  *model,
                                         GtkTreeIter   *iter)
{
        EvFindSidebarPrivate *priv = GET_PRIVATE (sidebar);
        gint                  page;
        gint                  result;


        if (priv->highlighted_result)
                gtk_tree_path_free (priv->highlighted_result);
        priv->highlighted_result = gtk_tree_model_get_path (model, iter);

        gtk_tree_model_get (model, iter,
                            PAGE_COLUMN, &page,
                            RESULT_COLUMN, &result,
                            -1);
        g_signal_emit (sidebar, signals[RESULT_ACTIVATED], 0, page - 1, result);
}

static void
selection_changed_callback (GtkTreeSelection *selection,
                            EvFindSidebar    *sidebar)
{
        GtkTreeModel *model;
        GtkTreeIter   iter;

        if (gtk_tree_selection_get_selected (selection, &model, &iter))
                ev_find_sidebar_activate_result_at_iter (sidebar, model, &iter);
}

static void
sidebar_tree_button_press_cb (GtkGestureClick	*self,
			      gint		 n_press,
			      gdouble		 x,
			      gdouble		 y,
                              EvFindSidebar	*sidebar)
{
        EvFindSidebarPrivate *priv = GET_PRIVATE (sidebar);
	GtkWidget            *view;
        GtkTreeModel         *model;
        GtkTreePath          *path;
        GtkTreeIter           iter;

	view = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (self));

        gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (view), x, y, &path,
			NULL, NULL, NULL);
        if (!path)
                return;

        if (priv->highlighted_result &&
            gtk_tree_path_compare (priv->highlighted_result, path) != 0) {
                gtk_tree_path_free (path);
                return;
        }

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
        gtk_tree_model_get_iter (model, &iter, path);
        gtk_tree_path_free (path);

        ev_find_sidebar_activate_result_at_iter (sidebar, model, &iter);
}

static void
ev_find_sidebar_reset_model (EvFindSidebar *sidebar)
{
        EvFindSidebarPrivate *priv = GET_PRIVATE (sidebar);
        GtkListStore *model;

        model = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT);
        gtk_tree_view_set_model (GTK_TREE_VIEW (priv->tree_view),
                                 GTK_TREE_MODEL (model));
        g_object_unref (model);
}

static void
ev_find_sidebar_init (EvFindSidebar *sidebar)
{
        EvFindSidebarPrivate *priv;
        GtkWidget            *swindow;
        GtkTreeViewColumn    *column;
        GtkCellRenderer      *renderer;
        GtkTreeSelection     *selection;
	GtkEventController   *controller;

        priv = GET_PRIVATE (sidebar);

        swindow = gtk_scrolled_window_new ();
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
                                        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

        priv->tree_view = gtk_tree_view_new ();
        ev_find_sidebar_reset_model (sidebar);

        gtk_tree_view_set_search_column (GTK_TREE_VIEW (priv->tree_view), -1);
        gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (priv->tree_view), FALSE);
	gtk_widget_set_hexpand (priv->tree_view, TRUE);
        gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (swindow), priv->tree_view);

        gtk_box_prepend (GTK_BOX (sidebar), swindow);

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
                                             "text", PAGE_LABEL_COLUMN,
                                             NULL);
        g_object_set (G_OBJECT (renderer), "style", PANGO_STYLE_ITALIC, NULL);

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));
        priv->selection_id = g_signal_connect (selection, "changed",
                                               G_CALLBACK (selection_changed_callback),
                                               sidebar);

	controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
        g_signal_connect (controller, "pressed",
                          G_CALLBACK (sidebar_tree_button_press_cb),
                          sidebar);
	gtk_widget_add_controller (priv->tree_view, controller);
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
        EvFindSidebarPrivate *priv = GET_PRIVATE (sidebar);
        GtkTreeSelection     *selection;

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));

        g_signal_handler_block (selection, priv->selection_id);
        gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->tree_view), priv->highlighted_result, NULL, FALSE);
        g_signal_handler_unblock (selection, priv->selection_id);
}

static void
ev_find_sidebar_highlight_first_match_of_page (EvFindSidebar *sidebar,
                                               gint           page)
{
        EvFindSidebarPrivate *priv = GET_PRIVATE (sidebar);
        gint                  index = 0;
        gint                  i;

        if (!priv->job)
                return;

        for (i = 0; i < page; i++)
                index += ev_job_find_get_n_main_results (priv->job, i);

        if (priv->highlighted_result)
                gtk_tree_path_free (priv->highlighted_result);
        priv->highlighted_result = gtk_tree_path_new_from_indices (index, -1);
        ev_find_sidebar_select_highlighted_result (sidebar);
}

static gchar *
sanitized_substring (const gchar  *text,
                     gint          start,
                     gint          end)
{
        const gchar *p;
        const gchar *start_ptr;
        const gchar *end_ptr;
        guint        len = 0;
        gchar       *retval;

        if (end - start <= 0)
                return NULL;

        start_ptr = g_utf8_offset_to_pointer (text, start);
        end_ptr = g_utf8_offset_to_pointer (start_ptr, end - start);

        retval = g_malloc (end_ptr - start_ptr + 1);
        p = start_ptr;

        while (p != end_ptr) {
                const gchar *next;

                next = g_utf8_next_char (p);

                if (next != end_ptr) {
                        GUnicodeBreakType break_type;

                        break_type = g_unichar_break_type (g_utf8_get_char (p));
                        if (break_type == G_UNICODE_BREAK_HYPHEN && *next == '\n') {
                                p = g_utf8_next_char (next);
                                continue;
                        }
                }

                if (*p != '\n') {
                        strncpy (retval + len, p, next - p);
                        len += next - p;
                } else {
                        *(retval + len) = ' ';
                        len++;
                }

                p = next;
        }

        if (len == 0) {
                g_free (retval);

                return NULL;
        }

        retval[len] = 0;

        return retval;
}

static gchar *
get_surrounding_text_markup (const gchar  *text,
                             const gchar  *find_text,
                             gboolean      case_sensitive,
                             PangoLogAttr *log_attrs,
                             gint          log_attrs_length,
                             gint          offset,
                             gboolean      has_nextline,
                             gboolean      hyphen_was_ignored)
{
        gint   iter;
        gchar *prec = NULL;
        gchar *succ = NULL;
        gchar *match = NULL;
        gchar *markup;
        gint   max_chars;

        iter = MAX (0, offset - 1);
        while (!log_attrs[iter].is_word_start && iter > 0)
                iter--;

        prec = sanitized_substring (text, iter, offset);

        iter = offset;
        offset += g_utf8_strlen (find_text, -1);

        if (has_nextline || g_utf8_offset_to_pointer (text, offset-1)[0] == '\n') {
                if (has_nextline) {
                        offset += 1; /* for newline */
                        if (hyphen_was_ignored)
                                offset += 1; /* for hyphen */
                }
                match = sanitized_substring (text, iter, offset);
        } else if (!case_sensitive)
                match = g_utf8_substring (text, iter, offset);

        iter = MIN (log_attrs_length, offset + 1);
        max_chars = MIN (log_attrs_length - 1, iter + 100);
        while (TRUE) {
                gint word = iter;

                while (!log_attrs[word].is_word_end && word < max_chars)
                        word++;

                if (word > max_chars)
                        break;

                iter = word + 1;
        }

        succ = sanitized_substring (text, offset, iter);

        markup = g_markup_printf_escaped ("%s<span weight=\"bold\">%s</span>%s",
                                          prec ? prec : "", match ? match : find_text, succ ? succ : "");
        g_free (prec);
        g_free (succ);
        g_free (match);

        return markup;
}

static gchar *
get_page_text (EvDocument   *document,
               EvPage       *page,
               EvRectangle **areas,
               guint        *n_areas)
{
        gchar   *text;
        gboolean success;

        ev_document_doc_mutex_lock ();
        text = ev_document_text_get_text (EV_DOCUMENT_TEXT (document), page);
        success = ev_document_text_get_text_layout (EV_DOCUMENT_TEXT (document), page, areas, n_areas);
        ev_document_doc_mutex_unlock ();

        if (!success) {
                g_free (text);
                return NULL;
        }

        return text;
}

static gint
get_match_offset (EvRectangle *areas,
                  guint        n_areas,
                  EvFindRectangle *match,
                  gint         offset)
{
        gdouble x, y;
        gint i;

        x = match->x1;
        y = (match->y1 + match->y2) / 2;

        i = offset;

        do {
                EvRectangle *area = areas + i;
                gdouble area_y = (area->y1 + area->y2) / 2;
                gdouble area_x = (area->x1 + area->x2) / 2;

                if (x >= area->x1 && x < area->x2 &&
                    y >= area->y1 && y <= area->y2 &&
                    area_x >= match->x1 && area_x <= match->x2 &&
                    area_y >= match->y1 && area_y <= match->y2) {
                        return i;
                }

                i = (i + 1) % n_areas;
        } while (i != offset);

        return -1;
}

static void
process_matches_idle (EvFindSidebar *sidebar)
{
        EvFindSidebarPrivate *priv = GET_PRIVATE (sidebar);
        GtkTreeModel         *model;
        gint                  current_page;
        EvDocument           *document;

        priv->process_matches_idle_id = 0;

        if (!ev_job_find_has_results (priv->job)) {
                if (ev_job_is_finished (EV_JOB (priv->job)))
                        g_clear_object (&priv->job);
		return;
        }

        document = EV_JOB (priv->job)->document;
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree_view));

        do {
                GList        *matches, *l;
                EvPage       *page;
                gint          result;
                gchar        *page_label;
                gchar        *page_text;
                EvRectangle  *areas = NULL;
                guint         n_areas;
                PangoLogAttr *text_log_attrs;
                gulong        text_log_attrs_length;
                gint          offset;

                current_page = priv->current_page;
                priv->current_page = (priv->current_page + 1) % priv->job->n_pages;

                matches = priv->job->pages[current_page];
                if (!matches)
                        continue;

                page = ev_document_get_page (document, current_page);
		page_label = ev_document_get_page_label (document, current_page);
                page_text = get_page_text (document, page, &areas, &n_areas);
                g_object_unref (page);
                if (!page_text)
                        continue;

                text_log_attrs_length = g_utf8_strlen (page_text, -1);
                text_log_attrs = g_new0 (PangoLogAttr, text_log_attrs_length + 1);
                pango_get_log_attrs (page_text, -1, -1, NULL, text_log_attrs, text_log_attrs_length + 1);

                if (priv->first_match_page == -1)
                        priv->first_match_page = current_page;

                offset = 0;

                for (l = matches, result = 0; l; l = g_list_next (l), result++) {
                        EvFindRectangle *match = (EvFindRectangle *)l->data;
                        gchar       *markup;
                        GtkTreeIter  iter;
                        gint         new_offset;

                        if (l->prev && ((EvFindRectangle *)l->prev->data)->next_line)
                                continue; /* Skip as this is second part of a multi-line match */

                        new_offset = get_match_offset (areas, n_areas, match, offset);
                        if (new_offset == -1) {
                                /* It may happen that a text match has no corresponding text area available,
                                 * (due to limitations/bugs of Poppler's TextPage->getSelectionWords() used by
                                 * poppler-glib poppler_page_get_text_layout_for_area() function) so in that
                                 * case we just show matched text because we cannot retrieve surrounding text.
                                 * Issue #1943 and related #1545 */
                                markup = g_strdup_printf ("<b>%s</b>", priv->job->text);
                        } else {
                                offset = new_offset;
                                markup = get_surrounding_text_markup (page_text,
                                                                      priv->job->text,
								      priv->job->options & EV_FIND_CASE_SENSITIVE,
                                                                      text_log_attrs,
                                                                      text_log_attrs_length,
                                                                      offset,
                                                                      match->next_line,
                                                                      match->after_hyphen);
                        }

                        if (current_page >= priv->job->start_page) {
                                gtk_list_store_append (GTK_LIST_STORE (model), &iter);
                        } else {
                                gtk_list_store_insert (GTK_LIST_STORE (model), &iter,
                                                       priv->insert_position);
                                priv->insert_position++;
                        }

                        gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                                            TEXT_COLUMN, markup,
					    PAGE_LABEL_COLUMN, page_label,
                                            PAGE_COLUMN, current_page + 1,
                                            RESULT_COLUMN, result,
                                            -1);
                        g_free (markup);
                }

                g_free (page_label);
                g_free (page_text);
                g_free (text_log_attrs);
                g_free (areas);
        } while (current_page != priv->job_current_page);

        if (ev_job_is_finished (EV_JOB (priv->job)) && priv->current_page == priv->job->start_page)
                ev_find_sidebar_highlight_first_match_of_page (sidebar, priv->first_match_page);
}

static void
find_job_updated_cb (EvJobFind     *job,
                     gint           page,
                     EvFindSidebar *sidebar)
{
        EvFindSidebarPrivate *priv = GET_PRIVATE (sidebar);

        priv->job_current_page = page;
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
        EvFindSidebarPrivate *priv = GET_PRIVATE (sidebar);

        if (priv->job == job)
                return;

        ev_find_sidebar_clear (sidebar);
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
}

void
ev_find_sidebar_restart (EvFindSidebar *sidebar,
                         gint           page)
{
        EvFindSidebarPrivate *priv = GET_PRIVATE (sidebar);
        gint                  first_match_page = -1;
        gint                  i;

        if (!priv->job)
                return;

        for (i = 0; i < priv->job->n_pages; i++) {
                int index;

                index = page + i;

                if (index >= priv->job->n_pages)
                        index -= priv->job->n_pages;

                if (priv->job->pages[index]) {
                        first_match_page = index;
                        break;
                }
        }

        if (first_match_page != -1)
                ev_find_sidebar_highlight_first_match_of_page (sidebar, first_match_page);
}

void
ev_find_sidebar_update (EvFindSidebar *sidebar)
{
        EvFindSidebarPrivate *priv = GET_PRIVATE (sidebar);

        if (!priv->job)
                return;

        if (priv->process_matches_idle_id == 0)
                priv->process_matches_idle_id = g_idle_add_once ((GSourceOnceFunc)process_matches_idle, sidebar);
}

void
ev_find_sidebar_clear (EvFindSidebar *sidebar)
{
        EvFindSidebarPrivate *priv = GET_PRIVATE (sidebar);

        ev_find_sidebar_cancel (sidebar);

        /* It seems it's more efficient to set a new model in the tree view instead of
         * clearing the model that would emit row-deleted signal for every row in the model
         */
        ev_find_sidebar_reset_model (sidebar);
        g_clear_pointer (&priv->highlighted_result, gtk_tree_path_free);
}

void
ev_find_sidebar_previous (EvFindSidebar *sidebar)
{
        EvFindSidebarPrivate *priv = GET_PRIVATE (sidebar);

        if (!priv->highlighted_result)
                return;

        if (!gtk_tree_path_prev (priv->highlighted_result)) {
                GtkTreeModel *model;
                GtkTreeIter   iter;

                model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree_view));
                gtk_tree_model_get_iter (model, &iter, priv->highlighted_result);
                while (gtk_tree_model_iter_next (model, &iter))
                        gtk_tree_path_next (priv->highlighted_result);
        }
        ev_find_sidebar_select_highlighted_result (sidebar);
}

void
ev_find_sidebar_next (EvFindSidebar *sidebar)
{
        EvFindSidebarPrivate *priv = GET_PRIVATE (sidebar);
        GtkTreeModel         *model;
        GtkTreeIter           iter;

        if (!priv->highlighted_result)
                return;

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree_view));
        gtk_tree_model_get_iter (model, &iter, priv->highlighted_result);
        if (gtk_tree_model_iter_next (model, &iter)) {
                gtk_tree_path_next (priv->highlighted_result);
        } else {
                gtk_tree_path_free (priv->highlighted_result);
                priv->highlighted_result = gtk_tree_path_new_first ();
        }
        ev_find_sidebar_select_highlighted_result (sidebar);
}
