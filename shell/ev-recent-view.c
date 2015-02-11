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
#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo-gobject.h>

#include "ev-recent-view.h"
#include "ev-file-helpers.h"
#include "gd-icon-utils.h"
#include "gd-two-lines-renderer.h"
#include "ev-document-misc.h"
#include "ev-document-model.h"
#include "ev-jobs.h"
#include "ev-job-scheduler.h"

#ifdef HAVE_LIBGNOME_DESKTOP
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-desktop-thumbnail.h>
#endif

typedef enum {
        EV_RECENT_VIEW_COLUMN_URI,
        EV_RECENT_VIEW_COLUMN_PRIMARY_TEXT,
        EV_RECENT_VIEW_COLUMN_SECONDARY_TEXT,
        EV_RECENT_VIEW_COLUMN_ICON,
        EV_RECENT_VIEW_COLUMN_ASYNC_DATA,
        NUM_COLUMNS
} EvRecentViewColumns;

struct _EvRecentViewPrivate {
        GtkWidget        *view;
        GtkListStore     *model;
        GtkRecentManager *recent_manager;
        GtkTreePath      *pressed_item_tree_path;
        guint             recent_manager_changed_handler_id;

#ifdef HAVE_LIBGNOME_DESKTOP
        GnomeDesktopThumbnailFactory *thumbnail_factory;
#endif
};

enum {
        ITEM_ACTIVATED,
        NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };

G_DEFINE_TYPE (EvRecentView, ev_recent_view, GTK_TYPE_SCROLLED_WINDOW)

#define ICON_VIEW_SIZE 128
#define MAX_RECENT_VIEW_ITEMS 20

typedef struct {
        EvRecentView        *ev_recent_view;
        char                *uri;
        time_t               mtime;
        GtkTreeRowReference *row;
        GCancellable        *cancellable;
        EvJob               *job;
        guint                needs_metadata : 1;
        guint                needs_thumbnail : 1;
} GetDocumentInfoAsyncData;

static void
get_document_info_async_data_free (GetDocumentInfoAsyncData *data)
{
        GtkTreePath *path;
        GtkTreeIter  iter;

        if (data->job) {
                g_signal_handlers_disconnect_by_data (data->job, data->ev_recent_view);
                ev_job_cancel (data->job);
                g_object_unref (data->job);
        }

        g_clear_object (&data->cancellable);
        g_free (data->uri);

        path = gtk_tree_row_reference_get_path (data->row);
        if (path) {
                gtk_tree_model_get_iter (GTK_TREE_MODEL (data->ev_recent_view->priv->model), &iter, path);
                gtk_list_store_set (data->ev_recent_view->priv->model, &iter,
                                    EV_RECENT_VIEW_COLUMN_ASYNC_DATA, NULL,
                                    -1);
                gtk_tree_path_free (path);
        }
        gtk_tree_row_reference_free (data->row);

        g_slice_free (GetDocumentInfoAsyncData, data);
}

static gboolean
ev_recent_view_clear_async_data (GtkTreeModel *model,
                                 GtkTreePath  *path,
                                 GtkTreeIter  *iter,
                                 EvRecentView *ev_recent_view)
{
        GetDocumentInfoAsyncData *data;

        gtk_tree_model_get (model, iter, EV_RECENT_VIEW_COLUMN_ASYNC_DATA, &data, -1);

        if (data != NULL)
                g_cancellable_cancel (data->cancellable);

        return FALSE;
}

static void
ev_recent_view_clear_model (EvRecentView *ev_recent_view)
{
        EvRecentViewPrivate *priv = ev_recent_view->priv;

        gtk_tree_model_foreach (GTK_TREE_MODEL (priv->model),
                                (GtkTreeModelForeachFunc)ev_recent_view_clear_async_data,
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

#ifdef HAVE_LIBGNOME_DESKTOP
        g_clear_object (&priv->thumbnail_factory);
#endif

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
        GtkTreePath         *path;

        /* eat double/triple click events */
        if (event->type != GDK_BUTTON_RELEASE)
                return TRUE;

        if (priv->pressed_item_tree_path == NULL)
                return FALSE;

        path = gtk_icon_view_get_path_at_pos (GTK_ICON_VIEW (priv->view), event->x, event->y);
        if (path == NULL)
                return FALSE;

        if (gtk_tree_path_compare (path, priv->pressed_item_tree_path) == 0) {
                g_clear_pointer (&priv->pressed_item_tree_path, gtk_tree_path_free);
                gtk_icon_view_item_activated (GTK_ICON_VIEW (priv->view), path);
                gtk_tree_path_free (path);

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

        g_clear_pointer (&priv->pressed_item_tree_path, gtk_tree_path_free);
        priv->pressed_item_tree_path =
                gtk_icon_view_get_path_at_pos (GTK_ICON_VIEW (priv->view), event->x, event->y);

	return TRUE;
}

static void
on_icon_view_item_activated (GtkIconView  *iconview,
                             GtkTreePath  *path,
                             EvRecentView *ev_recent_view)
{
        EvRecentViewPrivate *priv = ev_recent_view->priv;
        GtkTreeIter          iter;
        gchar               *uri;

        if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->model), &iter, path))
                return;

        gtk_tree_model_get (GTK_TREE_MODEL (priv->model), &iter,
                            EV_RECENT_VIEW_COLUMN_URI, &uri,
                            -1);
        g_signal_emit (ev_recent_view, signals[ITEM_ACTIVATED], 0, uri);
        g_free (uri);
}

static void
add_thumbnail_to_model (GetDocumentInfoAsyncData *data,
                        cairo_surface_t          *thumbnail)
{
        EvRecentViewPrivate *priv = data->ev_recent_view->priv;
        GtkTreePath         *path;
        GtkTreeIter          iter;
        GtkBorder            border;
        cairo_surface_t     *surface;

        data->needs_thumbnail = FALSE;

        border.left = 4;
        border.right = 3;
        border.top = 3;
        border.bottom = 6;

        surface = gd_embed_image_in_frame (thumbnail,
                                           "resource:///org/gnome/evince/shell/ui/thumbnail-frame.png",
                                           &border, &border);

        path = gtk_tree_row_reference_get_path (data->row);
        if (path != NULL) {
                gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->model), &iter, path);
                gtk_list_store_set (priv->model, &iter,
                                    EV_RECENT_VIEW_COLUMN_ICON, surface,
                                    -1);
                gtk_tree_path_free (path);
        }

        cairo_surface_destroy (surface);
}

#ifdef HAVE_LIBGNOME_DESKTOP
static void
ev_rencent_view_ensure_desktop_thumbnail_factory (EvRecentView *ev_recent_view)
{
        if (ev_recent_view->priv->thumbnail_factory)
                return;

        ev_recent_view->priv->thumbnail_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);
}

static void
save_thumbnail_in_cache_thread (GTask                    *task,
                                EvRecentView             *ev_recent_view,
                                GetDocumentInfoAsyncData *data,
                                GCancellable             *cancellable)
{
        GdkPixbuf       *thumbnail;
        cairo_surface_t *surface;

        surface = EV_JOB_THUMBNAIL (data->job)->thumbnail_surface;
        thumbnail = gdk_pixbuf_get_from_surface (surface, 0, 0,
                                                 cairo_image_surface_get_width (surface),
                                                 cairo_image_surface_get_height (surface));

        gnome_desktop_thumbnail_factory_save_thumbnail (ev_recent_view->priv->thumbnail_factory,
                                                        thumbnail, data->uri, data->mtime);
        g_object_unref (thumbnail);

        g_task_return_boolean (task, TRUE);
}

static void
save_thumbnail_in_cache_cb (EvRecentView             *ev_recent_view,
                            GAsyncResult             *result,
                            GetDocumentInfoAsyncData *data)
{
        get_document_info_async_data_free (data);
}
#endif /* HAVE_LIBGNOME_DESKTOP */

static void
save_document_thumbnail_in_cache (GetDocumentInfoAsyncData *data)
{
#ifdef HAVE_LIBGNOME_DESKTOP
        GTask *task;

        ev_rencent_view_ensure_desktop_thumbnail_factory (data->ev_recent_view);
        task = g_task_new (data->ev_recent_view, data->cancellable,
                           (GAsyncReadyCallback)save_thumbnail_in_cache_cb, data);
        g_task_set_task_data (task, data, NULL);
        g_task_run_in_thread (task, (GTaskThreadFunc)save_thumbnail_in_cache_thread);
        g_object_unref (task);
#else
        get_document_info_async_data_free (data);
#endif /* HAVE_LIBGNOME_DESKTOP */
}

static void
thumbnail_job_completed_callback (EvJobThumbnail           *job,
                                  GetDocumentInfoAsyncData *data)
{
        if (g_cancellable_is_cancelled (data->cancellable)) {
                get_document_info_async_data_free (data);
                return;
        }

        add_thumbnail_to_model (data, job->thumbnail_surface);
        save_document_thumbnail_in_cache (data);
}

static void
document_load_job_completed_callback (EvJobLoad                *job_load,
                                      GetDocumentInfoAsyncData *data)
{
        EvRecentViewPrivate *priv = data->ev_recent_view->priv;
        EvDocument          *document = EV_JOB (job_load)->document;

        if (g_cancellable_is_cancelled (data->cancellable) ||
            ev_job_is_failed (EV_JOB (job_load))) {
                get_document_info_async_data_free (data);
                return;
        }

        g_clear_object (&data->job);

        if (data->needs_thumbnail) {
                gdouble width, height;
                gint    target_width, target_height;

                ev_document_get_page_size (document, 0, &width, &height);
                if (height < width) {
                        target_width = ICON_VIEW_SIZE;
                        target_height = (int)(ICON_VIEW_SIZE * height / width + 0.5);
                } else {
                        target_width = (int)(ICON_VIEW_SIZE * width / height + 0.5);
                        target_height = ICON_VIEW_SIZE;
                }

                data->job = ev_job_thumbnail_new_with_target_size (document, 0, 0, target_width, target_height);
                ev_job_thumbnail_set_has_frame (EV_JOB_THUMBNAIL (data->job), FALSE);
                ev_job_thumbnail_set_output_format (EV_JOB_THUMBNAIL (data->job), EV_JOB_THUMBNAIL_SURFACE);
                g_signal_connect (data->job, "finished",
                                  G_CALLBACK (thumbnail_job_completed_callback),
                                  data);
                ev_job_scheduler_push_job (data->job, EV_JOB_PRIORITY_HIGH);
        }

        if (data->needs_metadata) {
                const EvDocumentInfo *info;
                GtkTreePath          *path;
                GtkTreeIter           iter;
                GFile                *file;
                GFileInfo            *file_info = g_file_info_new ();

                path = gtk_tree_row_reference_get_path (data->row);
                if (path)
                        gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->model), &iter, path);

                info = ev_document_get_info (document);
                if (info->fields_mask & EV_DOCUMENT_INFO_TITLE && info->title && info->title[0] != '\0') {
                        if (path) {
                                gtk_list_store_set (priv->model, &iter,
                                                    EV_RECENT_VIEW_COLUMN_PRIMARY_TEXT, info->title,
                                                    -1);
                        }
                        g_file_info_set_attribute_string (file_info, "metadata::evince::title", info->title);
                } else {
                        g_file_info_set_attribute_string (file_info, "metadata::evince::title", "");
                }
                if (info->fields_mask & EV_DOCUMENT_INFO_AUTHOR && info->author && info->author[0] != '\0') {
                        if (path) {
                                gtk_list_store_set (priv->model, &iter,
                                                    EV_RECENT_VIEW_COLUMN_SECONDARY_TEXT, info->author,
                                                    -1);
                        }
                        g_file_info_set_attribute_string (file_info, "metadata::evince::author", info->author);
                } else {
                        g_file_info_set_attribute_string (file_info, "metadata::evince::author", "");
                }

                gtk_tree_path_free (path);

                file = g_file_new_for_uri (data->uri);
                g_file_set_attributes_async (file, file_info, G_FILE_QUERY_INFO_NONE, G_PRIORITY_DEFAULT, NULL, NULL, NULL);
                g_object_unref (file);
        }

        if (!data->job)
                get_document_info_async_data_free (data);
}

static void
load_document_and_get_document_info (GetDocumentInfoAsyncData *data)
{
        data->job = EV_JOB (ev_job_load_new (data->uri));
        g_signal_connect (data->job, "finished",
                          G_CALLBACK (document_load_job_completed_callback),
                          data);
        ev_job_scheduler_push_job (data->job, EV_JOB_PRIORITY_HIGH);
}

#ifdef HAVE_LIBGNOME_DESKTOP
static void
get_thumbnail_from_cache_thread (GTask                    *task,
                                 EvRecentView             *ev_recent_view,
                                 GetDocumentInfoAsyncData *data,
                                 GCancellable             *cancellable)
{
        GFile           *file;
        GFileInfo       *info;
        gchar           *path;
        GdkPixbuf       *thumbnail;
        cairo_surface_t *surface = NULL;

        if (g_task_return_error_if_cancelled (task))
                return;

        file = g_file_new_for_uri (data->uri);
        info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_MODIFIED, G_FILE_QUERY_INFO_NONE,
                                  data->cancellable, NULL);
        g_object_unref (file);

        if (!info) {
                g_task_return_pointer (task, NULL, NULL);
                return;
        }

        data->mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
        g_object_unref (info);

        path = gnome_desktop_thumbnail_factory_lookup (ev_recent_view->priv->thumbnail_factory,
                                                       data->uri, data->mtime);
        if (!path) {
                g_task_return_pointer (task, NULL, NULL);
                return;
        }

        thumbnail = gdk_pixbuf_new_from_file (path, NULL);
        g_free (path);

        if (thumbnail) {
                gint width, height;
                gint target_width, target_height;

                width = gdk_pixbuf_get_width (thumbnail);
                height = gdk_pixbuf_get_height (thumbnail);

                if (height < width) {
                        target_width = ICON_VIEW_SIZE;
                        target_height = (int)(ICON_VIEW_SIZE * height / width + 0.5);
                } else {
                        target_width = (int)(ICON_VIEW_SIZE * width / height + 0.5);
                        target_height = ICON_VIEW_SIZE;
                }

                if (width < target_width || height < target_height) {
                        GdkPixbuf *scaled;

                        scaled = gdk_pixbuf_scale_simple (thumbnail,
                                                          target_width,
                                                          target_height,
                                                          GDK_INTERP_TILES);
                        g_object_unref (thumbnail);
                        thumbnail = scaled;
                } else if (width != target_width || height != target_height) {
                        GdkPixbuf *scaled;

                        scaled = gnome_desktop_thumbnail_scale_down_pixbuf (thumbnail,
                                                                            target_width,
                                                                            target_height);
                        g_object_unref (thumbnail);
                        thumbnail = scaled;
                }

                surface = ev_document_misc_surface_from_pixbuf (thumbnail);
                g_object_unref (thumbnail);
        }

        g_task_return_pointer (task, surface, (GDestroyNotify)cairo_surface_destroy);
}

static void
get_thumbnail_from_cache_cb (EvRecentView             *ev_recent_view,
                             GAsyncResult             *result,
                             GetDocumentInfoAsyncData *data)
{
        GTask           *task = G_TASK (result);
        cairo_surface_t *thumbnail;

        if (g_cancellable_is_cancelled (data->cancellable)) {
                get_document_info_async_data_free (data);
                return;
        }

        thumbnail = g_task_propagate_pointer (task, NULL);
        if (thumbnail) {
                add_thumbnail_to_model (data, thumbnail);
                cairo_surface_destroy (thumbnail);
        }

        if (data->needs_metadata || data->needs_thumbnail)
                load_document_and_get_document_info (data);
        else
                get_document_info_async_data_free (data);
}
#endif /* HAVE_LIBGNOME_DESKTOP */

static void
get_document_thumbnail_from_cache (GetDocumentInfoAsyncData *data)
{
#ifdef HAVE_LIBGNOME_DESKTOP
        GTask *task;

        ev_rencent_view_ensure_desktop_thumbnail_factory (data->ev_recent_view);
        task = g_task_new (data->ev_recent_view, data->cancellable,
                           (GAsyncReadyCallback)get_thumbnail_from_cache_cb, data);
        g_task_set_task_data (task, data, NULL);
        g_task_run_in_thread (task, (GTaskThreadFunc)get_thumbnail_from_cache_thread);
        g_object_unref (task);
#else
        load_document_and_get_document_info (data);
#endif /* HAVE_LIBGNOME_DESKTOP */
}

static void
get_document_info (GetDocumentInfoAsyncData *data)
{
        if (data->needs_thumbnail) {
                get_document_thumbnail_from_cache (data);
                return;
        }

        if (data->needs_metadata) {
                load_document_and_get_document_info (data);
                return;
        }

        get_document_info_async_data_free (data);
}

static void
document_query_info_cb (GFile                    *file,
                        GAsyncResult             *result,
                        GetDocumentInfoAsyncData *data)
{
        GFileInfo  *info;
        const char *title = NULL;
        const char *author = NULL;
        char      **attrs;
        guint       i;

        if (g_cancellable_is_cancelled (data->cancellable)) {
                get_document_info_async_data_free (data);
                return;
        }

        info = g_file_query_info_finish (file, result, NULL);
        if (!info) {
                get_document_info (data);
                return;
        }

        if (!g_file_info_has_namespace (info, "metadata")) {
                get_document_info (data);
                g_object_unref (info);

                return;
        }

        attrs = g_file_info_list_attributes (info, "metadata");
        for (i = 0; attrs[i]; i++) {
                if (g_str_equal (attrs[i], "metadata::evince::title")) {
                        title = g_file_info_get_attribute_string (info, attrs[i]);
                } else if (g_str_equal (attrs[i], "metadata::evince::author")) {
                        author = g_file_info_get_attribute_string (info, attrs[i]);
                }

                if (title && author)
                        break;
        }
        g_strfreev (attrs);

        if (title || author) {
                GtkTreePath *path;

                data->needs_metadata = FALSE;

                path = gtk_tree_row_reference_get_path (data->row);
                if (path) {
                        GtkTreeIter  iter;

                        gtk_tree_model_get_iter (GTK_TREE_MODEL (data->ev_recent_view->priv->model), &iter, path);

                        if (title && title[0] != '\0') {
                                gtk_list_store_set (data->ev_recent_view->priv->model, &iter,
                                                    EV_RECENT_VIEW_COLUMN_PRIMARY_TEXT, title,
                                                    -1);
                        }

                        if (author && author[0] != '\0') {
                                gtk_list_store_set (data->ev_recent_view->priv->model, &iter,
                                                    EV_RECENT_VIEW_COLUMN_SECONDARY_TEXT, author,
                                                    -1);
                        }

                        gtk_tree_path_free (path);
                }
        }

        g_object_unref (info);

        get_document_info (data);
}

static GetDocumentInfoAsyncData *
ev_recent_view_get_document_info (EvRecentView  *ev_recent_view,
                                  const gchar   *uri,
                                  GtkTreePath   *path)
{
        GFile                    *file;
        GetDocumentInfoAsyncData *data;

        data = g_slice_new0 (GetDocumentInfoAsyncData);
        data->ev_recent_view = ev_recent_view;
        data->uri = g_strdup (uri);
        data->row = gtk_tree_row_reference_new (GTK_TREE_MODEL (ev_recent_view->priv->model), path);;
        data->cancellable = g_cancellable_new ();
        data->needs_metadata = TRUE;
        data->needs_thumbnail = TRUE;

        file = g_file_new_for_uri (uri);
        g_file_query_info_async (file, "metadata::*", G_FILE_QUERY_INFO_NONE,
                                 G_PRIORITY_DEFAULT, data->cancellable,
                                 (GAsyncReadyCallback)document_query_info_cb,
                                 data);
        g_object_unref (file);

        return data;
}

static void
ev_recent_view_refresh (EvRecentView *ev_recent_view)
{
        GList               *items, *l;
        guint                n_items = 0;
        const gchar         *evince = g_get_application_name ();
        EvRecentViewPrivate *priv = ev_recent_view->priv;

        items = gtk_recent_manager_get_items (priv->recent_manager);
        items = g_list_sort (items, (GCompareFunc) compare_recent_items);

        gtk_list_store_clear (priv->model);

        for (l = items; l && l->data; l = g_list_next (l)) {
                GetDocumentInfoAsyncData *data;
                GtkTreePath              *path;
                GtkRecentInfo            *info;
                const gchar              *uri;
                GdkPixbuf                *pixbuf;
                cairo_surface_t          *thumbnail = NULL;
                GtkTreeIter               iter;

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

                gtk_list_store_append (priv->model, &iter);
                path = gtk_tree_model_get_path (GTK_TREE_MODEL (priv->model), &iter);
                data = ev_recent_view_get_document_info (ev_recent_view, uri, path);
                gtk_tree_path_free (path);

                gtk_list_store_set (priv->model, &iter,
                                    EV_RECENT_VIEW_COLUMN_URI, uri,
                                    EV_RECENT_VIEW_COLUMN_PRIMARY_TEXT, gtk_recent_info_get_display_name (info),
                                    EV_RECENT_VIEW_COLUMN_SECONDARY_TEXT, NULL,
                                    EV_RECENT_VIEW_COLUMN_ICON, thumbnail,
                                    EV_RECENT_VIEW_COLUMN_ASYNC_DATA, data,
                                    -1);

                if (thumbnail != NULL)
                        cairo_surface_destroy (thumbnail);

                if (++n_items == MAX_RECENT_VIEW_ITEMS)
                        break;
        }

        g_list_free_full (items, (GDestroyNotify)gtk_recent_info_unref);
}

static void
ev_recent_view_constructed (GObject *object)
{
        EvRecentView        *ev_recent_view = EV_RECENT_VIEW (object);
        EvRecentViewPrivate *priv = ev_recent_view->priv;
        GtkCellRenderer     *renderer;

        G_OBJECT_CLASS (ev_recent_view_parent_class)->constructed (object);

        priv->view = gtk_icon_view_new_with_model (GTK_TREE_MODEL (priv->model));

        gtk_icon_view_set_column_spacing (GTK_ICON_VIEW (priv->view), 20);
        gtk_icon_view_set_margin (GTK_ICON_VIEW (priv->view), 16);
        gtk_icon_view_set_selection_mode (GTK_ICON_VIEW (priv->view), GTK_SELECTION_NONE);
        gtk_widget_set_hexpand (priv->view, TRUE);
        gtk_widget_set_vexpand (priv->view, TRUE);

        renderer = gtk_cell_renderer_pixbuf_new ();
        g_object_set (renderer, "xalign", 0.5, "yalign", 0.5, NULL);

        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->view), renderer, FALSE);
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (priv->view), renderer,
                                       "surface", EV_RECENT_VIEW_COLUMN_ICON);

        renderer = gd_two_lines_renderer_new ();
        g_object_set (renderer,
                      "xalign", 0.5,
                      "alignment", PANGO_ALIGN_CENTER,
                      "wrap-mode", PANGO_WRAP_WORD_CHAR,
                      "wrap-width", 128,
                      "text-lines", 3,
                      NULL);
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->view), renderer, FALSE);
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (priv->view), renderer,
                                       "text", EV_RECENT_VIEW_COLUMN_PRIMARY_TEXT);
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (priv->view), renderer,
                                       "line-two", EV_RECENT_VIEW_COLUMN_SECONDARY_TEXT);

        g_signal_connect (priv->view, "button-press-event",
                          G_CALLBACK (on_button_press_event),
                          ev_recent_view);
        g_signal_connect (priv->view, "button-release-event",
                          G_CALLBACK (on_button_release_event),
                          ev_recent_view);
        g_signal_connect (priv->view, "item-activated",
                          G_CALLBACK (on_icon_view_item_activated),
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
                                          CAIRO_GOBJECT_TYPE_SURFACE,
                                          G_TYPE_POINTER);

        gtk_widget_set_hexpand (GTK_WIDGET (ev_recent_view), TRUE);
        gtk_widget_set_vexpand (GTK_WIDGET (ev_recent_view), TRUE);
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
