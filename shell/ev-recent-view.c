/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2013 Aakash Goenka
 *  Copyright (C) 2014 Carlos Garcia Campos
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo-gobject.h>

#include "ev-recent-view.h"
#include "ev-file-helpers.h"
#include "gd-icon-utils.h"
#include "gd-main-view-generic.h"
#include "gd-main-icon-view.h"
#include "ev-document-misc.h"
#include "ev-document-model.h"
#include "ev-jobs.h"
#include "ev-job-scheduler.h"

typedef enum {
        EV_RECENT_VIEW_JOB_COLUMN = GD_MAIN_COLUMN_LAST,
        NUM_COLUMNS
} EvRecentViewColumns;

struct _EvRecentViewPrivate {
        GtkWidget        *view;
        GtkListStore     *model;
        GtkRecentManager *recent_manager;
        GtkTreePath      *pressed_item_tree_path;
        guint             recent_manager_changed_handler_id;
};

enum {
        ITEM_ACTIVATED,
        NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };

G_DEFINE_TYPE (EvRecentView, ev_recent_view, GTK_TYPE_SCROLLED_WINDOW)

#define ICON_VIEW_SIZE 128
#define MAX_RECENT_VIEW_ITEMS 20

static gboolean
ev_recent_view_clear_job (GtkTreeModel *model,
                          GtkTreePath  *path,
                          GtkTreeIter  *iter,
                          EvRecentView *ev_recent_view)
{
        EvJob *job;

        gtk_tree_model_get (model, iter, EV_RECENT_VIEW_JOB_COLUMN, &job, -1);

        if (job != NULL) {
                ev_job_cancel (job);
                g_signal_handlers_disconnect_by_data (job, ev_recent_view);
                g_object_unref (job);
        }

        return FALSE;
}

static void
ev_recent_view_clear_model (EvRecentView *ev_recent_view)
{
        EvRecentViewPrivate *priv = ev_recent_view->priv;

        gtk_tree_model_foreach (GTK_TREE_MODEL (priv->model),
                                (GtkTreeModelForeachFunc)ev_recent_view_clear_job,
                                ev_recent_view);
        gtk_list_store_clear (priv->model);
}

static void
ev_recent_view_dispose (GObject *obj)
{
        EvRecentView        *ev_recent_view = EV_RECENT_VIEW (obj);
        EvRecentViewPrivate *priv = ev_recent_view->priv;

        if (priv->model) {
                ev_recent_view_clear_model (ev_recent_view);
                g_object_unref (priv->model);
                priv->model = NULL;
        }

        if (priv->recent_manager_changed_handler_id) {
                g_signal_handler_disconnect (priv->recent_manager,
                                             priv->recent_manager_changed_handler_id);
                priv->recent_manager_changed_handler_id = 0;
        }
        priv->recent_manager = NULL;

        G_OBJECT_CLASS (ev_recent_view_parent_class)->dispose (obj);
}

static gint
compare_recent_items (GtkRecentInfo *a,
                      GtkRecentInfo *b)
{
        gboolean     has_ev_a, has_ev_b;
        const gchar *evince = g_get_application_name ();

        has_ev_a = gtk_recent_info_has_application (a, evince);
        has_ev_b = gtk_recent_info_has_application (b, evince);

        if (has_ev_a && has_ev_b) {
                time_t time_a, time_b;

                time_a = gtk_recent_info_get_modified (a);
                time_b = gtk_recent_info_get_modified (b);

                return (time_b - time_a);
        } else if (has_ev_a) {
                return -1;
        } else if (has_ev_b) {
                return 1;
        }

        return 0;
}

static gboolean
on_button_release_event (GtkWidget      *view,
                         GdkEventButton *event,
                         EvRecentView   *ev_recent_view)
{
        EvRecentViewPrivate *priv = ev_recent_view->priv;
        GdMainViewGeneric   *generic = GD_MAIN_VIEW_GENERIC (priv->view);
        GtkTreePath         *path;

        /* eat double/triple click events */
        if (event->type != GDK_BUTTON_RELEASE)
                return TRUE;

        if (priv->pressed_item_tree_path == NULL)
                return FALSE;

        path = gd_main_view_generic_get_path_at_pos (generic, event->x, event->y);
        if (path == NULL)
                return FALSE;

        if (gtk_tree_path_compare (path, priv->pressed_item_tree_path) == 0) {
                GtkTreeIter iter;
                gchar      *uri;

                g_clear_pointer (&priv->pressed_item_tree_path, gtk_tree_path_free);

                if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->model), &iter, path)) {
                        gtk_tree_path_free (path);

                        return TRUE;
                }
                gtk_tree_path_free (path);

                gtk_tree_model_get (GTK_TREE_MODEL (priv->model), &iter,
                                    GD_MAIN_COLUMN_URI, &uri,
                                    -1);
                gtk_list_store_set (priv->model,
                                    &iter,
                                    GD_MAIN_COLUMN_SELECTED, TRUE,
                                    -1);
                g_signal_emit (ev_recent_view, signals[ITEM_ACTIVATED], 0, uri);
                g_free (uri);

                return TRUE;
        }

        g_clear_pointer (&priv->pressed_item_tree_path, gtk_tree_path_free);
        gtk_tree_path_free (path);

        return FALSE;
}

static gboolean
on_button_press_event (GtkWidget      *view,
                       GdkEventButton *event,
                       EvRecentView   *ev_recent_view)
{
        EvRecentViewPrivate *priv = ev_recent_view->priv;
        GdMainViewGeneric   *generic = GD_MAIN_VIEW_GENERIC (priv->view);

        g_clear_pointer (&priv->pressed_item_tree_path, gtk_tree_path_free);
        priv->pressed_item_tree_path = gd_main_view_generic_get_path_at_pos (generic, event->x, event->y);

	return TRUE;
}

static void
thumbnail_job_completed_callback (EvJobThumbnail *job,
                                  EvRecentView   *ev_recent_view)
{
        EvRecentViewPrivate *priv = ev_recent_view->priv;
        GtkTreeRowReference *row;
        GtkTreePath         *path;
        GtkTreeIter          iter;
        cairo_surface_t     *surface;
        GtkBorder            border;

        border.left = 4;
        border.right = 3;
        border.top = 3;
        border.bottom = 6;

        surface = gd_embed_image_in_frame (job->thumbnail_surface,
                                           "resource:///org/gnome/evince/shell/ui/thumbnail-frame.png",
                                           &border, &border);

        row = (GtkTreeRowReference *) g_object_get_data (G_OBJECT (job), "row-reference");
        path = gtk_tree_row_reference_get_path (row);
        gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->model), &iter, path);
        gtk_tree_path_free (path);

        gtk_list_store_set (priv->model, &iter,
                            GD_MAIN_COLUMN_ICON, surface,
                            EV_RECENT_VIEW_JOB_COLUMN, NULL,
                            -1);
        cairo_surface_destroy (surface);
}

static void
document_load_job_completed_callback (EvJobLoad    *job_load,
                                      EvRecentView *ev_recent_view)
{
        EvRecentViewPrivate *priv = ev_recent_view->priv;
        GtkTreeRowReference *row;
        GtkTreePath         *path;
        GtkTreeIter          iter;
        EvDocument          *document;

        document = EV_JOB (job_load)->document;
        row = (GtkTreeRowReference *) g_object_get_data (G_OBJECT (job_load), "row-reference");
        path = gtk_tree_row_reference_get_path (row);
        gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->model), &iter, path);
        gtk_tree_path_free (path);

        if (document) {
                EvJob                *job_thumbnail;
                gdouble               height;
                gdouble               width;
                gint                  target_width;
                gint                  target_height;
                const EvDocumentInfo *info;

                ev_document_get_page_size (document, 0, &width, &height);
                if (height < width) {
                        target_width = ICON_VIEW_SIZE;
                        target_height = (int)(ICON_VIEW_SIZE * height / width + 0.5);
                } else {
                        target_width = (int)(ICON_VIEW_SIZE * width / height + 0.5);
                        target_height = ICON_VIEW_SIZE;
                }

                job_thumbnail = ev_job_thumbnail_new_with_target_size (document, 0, 0, target_width, target_height);

                ev_job_thumbnail_set_has_frame (EV_JOB_THUMBNAIL (job_thumbnail), FALSE);
                ev_job_thumbnail_set_output_format (EV_JOB_THUMBNAIL (job_thumbnail), EV_JOB_THUMBNAIL_SURFACE);

                g_object_set_data_full (G_OBJECT (job_thumbnail), "row-reference",
                                        gtk_tree_row_reference_copy (row),
                                        (GDestroyNotify)gtk_tree_row_reference_free);

                g_signal_connect (job_thumbnail, "finished",
                                  G_CALLBACK (thumbnail_job_completed_callback),
                                  ev_recent_view);

                info = ev_document_get_info (document);
                if (info->fields_mask & EV_DOCUMENT_INFO_TITLE && info->title && info->title[0] != '\0')
                        gtk_list_store_set (priv->model, &iter,
                                            GD_MAIN_COLUMN_PRIMARY_TEXT, info->title,
                                            -1);
                if (info->fields_mask & EV_DOCUMENT_INFO_AUTHOR && info->author && info->author[0] != '\0')
                        gtk_list_store_set (priv->model, &iter,
                                            GD_MAIN_COLUMN_SECONDARY_TEXT, info->author,
                                            -1);

                gtk_list_store_set (priv->model, &iter,
                                    EV_RECENT_VIEW_JOB_COLUMN, job_thumbnail,
                                    -1);

                ev_job_scheduler_push_job (EV_JOB (job_thumbnail), EV_JOB_PRIORITY_HIGH);
                g_object_unref (job_thumbnail);
        } else {
                gtk_list_store_set (priv->model, &iter,
                                    EV_RECENT_VIEW_JOB_COLUMN, NULL,
                                    -1);
        }
}

static void
ev_recent_view_refresh (EvRecentView *ev_recent_view)
{
        GList               *items, *l;
        guint                n_items = 0;
        const gchar         *evince = g_get_application_name ();
        EvRecentViewPrivate *priv = ev_recent_view->priv;
        GdMainViewGeneric   *generic = GD_MAIN_VIEW_GENERIC (priv->view);

        items = gtk_recent_manager_get_items (priv->recent_manager);
        items = g_list_sort (items, (GCompareFunc) compare_recent_items);

        gtk_list_store_clear (priv->model);

        for (l = items; l && l->data; l = g_list_next (l)) {
                EvJob           *job_load;
                GtkRecentInfo   *info;
                const gchar     *uri;
                GdkPixbuf       *pixbuf;
                cairo_surface_t *thumbnail = NULL;
                GtkTreeIter      iter;

                info = (GtkRecentInfo *) l->data;

                if (!gtk_recent_info_has_application (info, evince))
                        continue;

                if (gtk_recent_info_is_local (info) && !gtk_recent_info_exists (info))
                        continue;

                uri = gtk_recent_info_get_uri (info);
                pixbuf = gtk_recent_info_get_icon (info, ICON_VIEW_SIZE);
                if (pixbuf) {
                        thumbnail = ev_document_misc_surface_from_pixbuf (pixbuf);
                        g_object_unref (pixbuf);
                }
                job_load = ev_job_load_new (uri);
                g_signal_connect (job_load, "finished",
                                  G_CALLBACK (document_load_job_completed_callback),
                                  ev_recent_view);

                gtk_list_store_append (priv->model, &iter);

                gtk_list_store_set (priv->model, &iter,
                                    GD_MAIN_COLUMN_URI, uri,
                                    GD_MAIN_COLUMN_PRIMARY_TEXT, gtk_recent_info_get_display_name (info),
                                    GD_MAIN_COLUMN_SECONDARY_TEXT, NULL,
                                    GD_MAIN_COLUMN_ICON, thumbnail,
                                    GD_MAIN_COLUMN_MTIME, gtk_recent_info_get_modified (info),
                                    GD_MAIN_COLUMN_SELECTED, FALSE,
                                    EV_RECENT_VIEW_JOB_COLUMN, job_load,
                                    -1);

                if (job_load) {
                        GtkTreePath         *path;
                        GtkTreeRowReference *row;

                        path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->model), &iter);
                        row = gtk_tree_row_reference_new (GTK_TREE_MODEL (priv->model), path);
                        gtk_tree_path_free (path);

                        g_object_set_data_full (G_OBJECT (job_load), "row-reference", row,
                                                (GDestroyNotify)gtk_tree_row_reference_free);

                        ev_job_scheduler_push_job (EV_JOB (job_load), EV_JOB_PRIORITY_HIGH);
                        g_object_unref (job_load);
                }

                if (thumbnail != NULL)
                        cairo_surface_destroy (thumbnail);

                if (++n_items == MAX_RECENT_VIEW_ITEMS)
                        break;
        }

        g_list_free_full (items, (GDestroyNotify)gtk_recent_info_unref);

        gd_main_view_generic_set_model (generic, GTK_TREE_MODEL (priv->model));
}

static void
ev_recent_view_constructed (GObject *object)
{
        EvRecentView        *ev_recent_view = EV_RECENT_VIEW (object);
        EvRecentViewPrivate *priv = ev_recent_view->priv;

        G_OBJECT_CLASS (ev_recent_view_parent_class)->constructed (object);

        priv->view = gd_main_icon_view_new ();
        g_signal_connect (priv->view, "button-press-event",
                          G_CALLBACK (on_button_press_event),
                          ev_recent_view);
        g_signal_connect (priv->view, "button-release-event",
                          G_CALLBACK (on_button_release_event),
                          ev_recent_view);

        gtk_style_context_add_class (gtk_widget_get_style_context (priv->view), "content-view");
        gtk_container_add (GTK_CONTAINER (ev_recent_view), priv->view);
        gtk_widget_show (priv->view);

        ev_recent_view_refresh (ev_recent_view);
}

static void
ev_recent_view_init (EvRecentView *ev_recent_view)
{
        EvRecentViewPrivate *priv;

        ev_recent_view->priv = G_TYPE_INSTANCE_GET_PRIVATE (ev_recent_view, EV_TYPE_RECENT_VIEW, EvRecentViewPrivate);

        priv = ev_recent_view->priv;
        priv->recent_manager = gtk_recent_manager_get_default ();
        priv->model = gtk_list_store_new (NUM_COLUMNS,
                                          G_TYPE_STRING,
                                          G_TYPE_STRING,
                                          G_TYPE_STRING,
                                          G_TYPE_STRING,
                                          CAIRO_GOBJECT_TYPE_SURFACE,
                                          G_TYPE_LONG,
                                          G_TYPE_BOOLEAN,
                                          EV_TYPE_JOB,
                                          G_TYPE_BOOLEAN);

        gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (priv->model),
                                              GD_MAIN_COLUMN_MTIME,
                                              GTK_SORT_DESCENDING);

        gtk_widget_set_hexpand (GTK_WIDGET (ev_recent_view), TRUE);
        gtk_widget_set_vexpand (GTK_WIDGET (ev_recent_view), TRUE);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (ev_recent_view), GTK_SHADOW_IN);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (ev_recent_view),
                                        GTK_POLICY_NEVER,
                                        GTK_POLICY_AUTOMATIC);
        priv->recent_manager_changed_handler_id =
                g_signal_connect_swapped (priv->recent_manager,
                                          "changed",
                                          G_CALLBACK (ev_recent_view_refresh),
                                          ev_recent_view);
}

static void
ev_recent_view_class_init (EvRecentViewClass *klass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

        g_object_class->constructed = ev_recent_view_constructed;
        g_object_class->dispose = ev_recent_view_dispose;

        signals[ITEM_ACTIVATED] =
                  g_signal_new ("item-activated",
                                EV_TYPE_RECENT_VIEW,
                                G_SIGNAL_RUN_LAST,
                                0, NULL, NULL,
                                g_cclosure_marshal_generic,
                                G_TYPE_NONE, 1,
                                G_TYPE_STRING);

        g_type_class_add_private (klass, sizeof (EvRecentViewPrivate));
}

GtkWidget *
ev_recent_view_new (void)
{
        return GTK_WIDGET (g_object_new (EV_TYPE_RECENT_VIEW, NULL));
}
