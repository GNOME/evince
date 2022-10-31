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
#include "ev-thumbnail-item.h"
#include "ev-file-helpers.h"
#include "ev-document-misc.h"
#include "ev-document-model.h"
#include "ev-jobs.h"
#include "ev-job-scheduler.h"

#ifdef HAVE_LIBGNOME_DESKTOP
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-desktop-thumbnail.h>
#endif

typedef struct {
	GtkWidget        *view;
	GtkWidget        *stack;
	GListStore       *model;
	GtkRecentManager *recent_manager;
	gulong            recent_manager_changed_handler_id;

#ifdef HAVE_LIBGNOME_DESKTOP
	GnomeDesktopThumbnailFactory *thumbnail_factory;
#endif
} EvRecentViewPrivate;

enum {
        ITEM_ACTIVATED,
        NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };

G_DEFINE_TYPE_WITH_PRIVATE (EvRecentView, ev_recent_view, ADW_TYPE_BIN);

#define ICON_VIEW_SIZE 128
#define MAX_RECENT_VIEW_ITEMS 64
#define GET_PRIVATE(o) ev_recent_view_get_instance_private (o);

typedef struct {
        EvRecentView        *ev_recent_view;
        char                *uri;
        time_t               mtime;
	int                  scale;
	EvThumbnailItem     *item;
        GCancellable        *cancellable;
        EvJob               *job;
        guint                needs_metadata : 1;
        guint                needs_thumbnail : 1;
} GetDocumentInfoAsyncData;

static void
get_document_info_async_data_free (GetDocumentInfoAsyncData *data)
{
        if (data->job) {
                g_signal_handlers_disconnect_by_data (data->job, data->ev_recent_view);
                ev_job_cancel (data->job);
                g_object_unref (data->job);
        }

        g_clear_object (&data->cancellable);
        g_free (data->uri);

	g_object_unref (data->item);
        g_object_unref (data->ev_recent_view);

        g_slice_free (GetDocumentInfoAsyncData, data);
}

static void
ev_recent_view_clear_model (EvRecentView *ev_recent_view)
{
        EvRecentViewPrivate *priv = GET_PRIVATE (ev_recent_view);

        g_list_store_remove_all (priv->model);
}

static void
ev_recent_view_dispose (GObject *obj)
{
        EvRecentView        *ev_recent_view = EV_RECENT_VIEW (obj);
        EvRecentViewPrivate *priv = GET_PRIVATE (ev_recent_view);

        if (priv->model) {
                ev_recent_view_clear_model (ev_recent_view);
        }

	g_clear_signal_handler (&priv->recent_manager_changed_handler_id,
				priv->recent_manager);
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
                GDateTime *time_a, *time_b;

                time_a = gtk_recent_info_get_modified (a);
                time_b = gtk_recent_info_get_modified (b);

                return g_date_time_compare (time_b, time_a);
        } else if (has_ev_a) {
                return -1;
        } else if (has_ev_b) {
                return 1;
        }

        return 0;
}

static void
grid_view_item_activated_cb (GtkGridView  *gridview,
                             guint	   position,
                             EvRecentView *ev_recent_view)
{
        EvRecentViewPrivate *priv = GET_PRIVATE (ev_recent_view);
	EvThumbnailItem        *recent_item =
			g_list_model_get_item (G_LIST_MODEL (priv->model), position);
        gchar               *uri;

	if (!recent_item)
		return;

	g_object_get (recent_item, "uri", &uri, NULL);

	if (!uri)
		return;

        g_signal_emit (ev_recent_view, signals[ITEM_ACTIVATED], 0, uri);
        g_free (uri);
	g_object_unref (recent_item);
}

static void
add_thumbnail_to_model (GetDocumentInfoAsyncData *data,
                        GdkPixbuf                *thumbnail)
{
	GdkTexture          *texture;

        data->needs_thumbnail = FALSE;

	texture = gdk_texture_new_for_pixbuf (thumbnail);
	ev_thumbnail_item_set_paintable (data->item, GDK_PAINTABLE (texture));
	g_object_unref (texture);
}

#ifdef HAVE_LIBGNOME_DESKTOP
static void
ev_recent_view_ensure_desktop_thumbnail_factory (EvRecentView *ev_recent_view)
{
	EvRecentViewPrivate *priv = GET_PRIVATE (ev_recent_view);

        if (priv->thumbnail_factory)
                return;

        priv->thumbnail_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);
}

static void
save_thumbnail_in_cache_thread (GTask                    *task,
                                EvRecentView             *ev_recent_view,
                                GetDocumentInfoAsyncData *data,
                                GCancellable             *cancellable)
{
        GdkPixbuf       *thumbnail;
        cairo_surface_t *surface;
	EvRecentViewPrivate *priv = GET_PRIVATE (ev_recent_view);
#if defined(GNOME_DESKTOP_PLATFORM_VERSION) && GNOME_DESKTOP_PLATFORM_VERSION >= 43
        GError *error = NULL;
#endif

	surface = EV_JOB_THUMBNAIL_CAIRO (data->job)->thumbnail_surface;
        thumbnail = gdk_pixbuf_get_from_surface (surface, 0, 0,
                                                 cairo_image_surface_get_width (surface),
                                                 cairo_image_surface_get_height (surface));

#if defined(GNOME_DESKTOP_PLATFORM_VERSION) && GNOME_DESKTOP_PLATFORM_VERSION >= 43
        gnome_desktop_thumbnail_factory_save_thumbnail (priv->thumbnail_factory,
                                                        thumbnail, data->uri, data->mtime, NULL, &error);
        if (error) {
                g_warning ("Failed to save thumbnail %s: %s", data->uri, error->message);
                g_error_free (error);
        }
#else
        gnome_desktop_thumbnail_factory_save_thumbnail (priv->thumbnail_factory,
                                                        thumbnail, data->uri, data->mtime);
#endif
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

        ev_recent_view_ensure_desktop_thumbnail_factory (data->ev_recent_view);
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
thumbnail_job_completed_callback (EvJobThumbnailCairo      *job,
                                  GetDocumentInfoAsyncData *data)
{
	GdkPixbuf *pixbuf;

        if (g_cancellable_is_cancelled (data->cancellable) ||
            ev_job_is_failed (EV_JOB (job))) {
                get_document_info_async_data_free (data);
                return;
        }

	pixbuf = gdk_pixbuf_get_from_surface (job->thumbnail_surface, 0, 0,
			cairo_image_surface_get_width (job->thumbnail_surface),
			cairo_image_surface_get_height (job->thumbnail_surface));

	add_thumbnail_to_model (data, pixbuf);
        save_document_thumbnail_in_cache (data);
}

static void
document_load_job_completed_callback (EvJobLoad                *job_load,
                                      GetDocumentInfoAsyncData *data)
{
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

		target_width *= data->scale;
		target_height *= data->scale;

		data->job = ev_job_thumbnail_cairo_new_with_target_size (document, 0, 0,
									 target_width,
									 target_height);

                g_signal_connect (data->job, "finished",
                                  G_CALLBACK (thumbnail_job_completed_callback),
                                  data);
                ev_job_scheduler_push_job (data->job, EV_JOB_PRIORITY_HIGH);
        }

        if (data->needs_metadata) {
                const EvDocumentInfo *info;
                GFile                *file;
                GFileInfo            *file_info = g_file_info_new ();

                info = ev_document_get_info (document);
                if (info->fields_mask & EV_DOCUMENT_INFO_TITLE && info->title && info->title[0] != '\0') {
			ev_thumbnail_item_set_primary_text (data->item, info->title);
                        g_file_info_set_attribute_string (file_info, "metadata::evince::title", info->title);
                } else {
                        g_file_info_set_attribute_string (file_info, "metadata::evince::title", "");
                }
                if (info->fields_mask & EV_DOCUMENT_INFO_AUTHOR && info->author && info->author[0] != '\0') {
			ev_thumbnail_item_set_secondary_text (data->item, info->author);
                        g_file_info_set_attribute_string (file_info, "metadata::evince::author", info->author);
                } else {
                        g_file_info_set_attribute_string (file_info, "metadata::evince::author", "");
                }

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
	EvRecentViewPrivate *priv = GET_PRIVATE (ev_recent_view);

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

        path = gnome_desktop_thumbnail_factory_lookup (priv->thumbnail_factory,
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

		target_width *= data->scale;
		target_height *= data->scale;

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

                        scaled = gdk_pixbuf_scale_simple (thumbnail,
                                                          target_width,
                                                          target_height,
                                                          GDK_INTERP_HYPER);
                        g_object_unref (thumbnail);
                        thumbnail = scaled;
                }
        }

        g_task_return_pointer (task, thumbnail, (GDestroyNotify)g_object_unref);
}

static void
get_thumbnail_from_cache_cb (EvRecentView             *ev_recent_view,
                             GAsyncResult             *result,
                             GetDocumentInfoAsyncData *data)
{
        GTask           *task = G_TASK (result);
        GdkPixbuf       *thumbnail;

        if (g_cancellable_is_cancelled (data->cancellable)) {
                get_document_info_async_data_free (data);
                return;
        }

        thumbnail = g_task_propagate_pointer (task, NULL);
        if (thumbnail) {
                add_thumbnail_to_model (data, thumbnail);
		g_object_unref (thumbnail);
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

        ev_recent_view_ensure_desktop_thumbnail_factory (data->ev_recent_view);
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
        char       *title = NULL;
        char       *author = NULL;
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
                        title = (gchar *) g_file_info_get_attribute_string (info, attrs[i]);
                } else if (g_str_equal (attrs[i], "metadata::evince::author")) {
                        author = (gchar *) g_file_info_get_attribute_string (info, attrs[i]);
                }

                if (title && author)
                        break;
        }
        g_strfreev (attrs);

        if (title || author) {
                data->needs_metadata = FALSE;

                if (data->item) {
                        if (title && (g_strstrip (title))[0] != '\0')
				ev_thumbnail_item_set_primary_text (data->item, title);

                        if (author && (g_strstrip (author))[0] != '\0')
				ev_thumbnail_item_set_secondary_text (data->item, author);
                }
        }

        g_object_unref (info);

        get_document_info (data);
}

static GetDocumentInfoAsyncData *
ev_recent_view_get_document_info (EvRecentView  *ev_recent_view,
                                  const gchar   *uri,
                                  EvThumbnailItem  *item)
{
        GFile                    *file;
        GetDocumentInfoAsyncData *data;

        data = g_slice_new0 (GetDocumentInfoAsyncData);
        data->ev_recent_view = g_object_ref (ev_recent_view);
        data->uri = g_strdup (uri);
        data->item = g_object_ref (item);
        data->cancellable = g_cancellable_new ();
        data->needs_metadata = TRUE;
        data->needs_thumbnail = TRUE;
	data->scale = gtk_widget_get_scale_factor (GTK_WIDGET (ev_recent_view));

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
        EvRecentViewPrivate *priv = GET_PRIVATE (ev_recent_view);

        items = gtk_recent_manager_get_items (priv->recent_manager);
        items = g_list_sort (items, (GCompareFunc) compare_recent_items);

	g_list_store_remove_all (priv->model);

        for (l = items; l && l->data; l = g_list_next (l)) {
                GtkRecentInfo            *info;
                const gchar              *uri;
		gchar			 *uri_display;
		GIcon                    *icon;
		EvThumbnailItem		 *recent_item;
		GdkPaintable             *paintable = NULL;
		GtkIconTheme             *theme;

                info = (GtkRecentInfo *) l->data;

                if (!gtk_recent_info_has_application (info, evince))
                        continue;

                if (gtk_recent_info_is_local (info) && !gtk_recent_info_exists (info))
                        continue;

                uri = gtk_recent_info_get_uri (info);
		uri_display = gtk_recent_info_get_uri_display (info);
                icon = gtk_recent_info_get_gicon (info);

		theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
		paintable = GDK_PAINTABLE(gtk_icon_theme_lookup_by_gicon (theme, icon, ICON_VIEW_SIZE,
			gtk_widget_get_scale_factor (GTK_WIDGET (ev_recent_view)),
			gtk_widget_get_direction (GTK_WIDGET (ev_recent_view)),
			GTK_ICON_LOOKUP_FORCE_SYMBOLIC | GTK_ICON_LOOKUP_PRELOAD));

		recent_item = g_object_new (EV_TYPE_THUMBNAIL_ITEM,
			"primary-text", gtk_recent_info_get_display_name (info),
			"uri", uri,
			"uri-display", uri_display,
			"secondary-text", NULL,
			"paintable", paintable,
			NULL);

		g_clear_pointer (&uri_display, g_free);
		g_clear_object (&icon);
		g_clear_object (&paintable);

                ev_recent_view_get_document_info (ev_recent_view, uri, recent_item);
		g_list_store_append (priv->model, recent_item);

                if (++n_items == MAX_RECENT_VIEW_ITEMS)
                        break;
        }

        g_list_free_full (items, (GDestroyNotify)gtk_recent_info_unref);

	if (n_items > 0)
		adw_view_stack_set_visible_child_name (ADW_VIEW_STACK (priv->stack), "recent");
	else
		adw_view_stack_set_visible_child_name (ADW_VIEW_STACK (priv->stack), "empty");
}

static void
ev_recent_view_constructed (GObject *object)
{
        EvRecentView        *ev_recent_view = EV_RECENT_VIEW (object);

        G_OBJECT_CLASS (ev_recent_view_parent_class)->constructed (object);

        ev_recent_view_refresh (ev_recent_view);
}

static void
ev_recent_view_init (EvRecentView *ev_recent_view)
{
        EvRecentViewPrivate *priv = GET_PRIVATE (ev_recent_view);

        gtk_widget_init_template (GTK_WIDGET (ev_recent_view));

        priv->recent_manager = gtk_recent_manager_get_default ();
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
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        g_object_class->constructed = ev_recent_view_constructed;
        g_object_class->dispose = ev_recent_view_dispose;

	g_type_ensure (EV_TYPE_THUMBNAIL_ITEM);
        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/evince/ui/recent-view.ui");
        gtk_widget_class_bind_template_child_private (widget_class, EvRecentView, model);
        gtk_widget_class_bind_template_child_private (widget_class, EvRecentView, view);
        gtk_widget_class_bind_template_child_private (widget_class, EvRecentView, stack);
	gtk_widget_class_bind_template_callback (widget_class, grid_view_item_activated_cb);

        signals[ITEM_ACTIVATED] =
                  g_signal_new ("item-activated",
                                EV_TYPE_RECENT_VIEW,
                                G_SIGNAL_RUN_LAST,
                                0, NULL, NULL,
                                g_cclosure_marshal_generic,
                                G_TYPE_NONE, 1,
                                G_TYPE_STRING);
}

GtkWidget *
ev_recent_view_new (void)
{
        return GTK_WIDGET (g_object_new (EV_TYPE_RECENT_VIEW, NULL));
}
