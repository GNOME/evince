/* ev-sidebar-attachments.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2006 Carlos Garcia Campos
 *
 * Author:
 *   Carlos Garcia Campos <carlosgc@gnome.org>
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

#include <string.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include "ev-document-attachments.h"
#include "ev-document-misc.h"
#include "ev-jobs.h"
#include "ev-job-scheduler.h"
#include "ev-file-helpers.h"
#include "ev-sidebar-attachments.h"
#include "ev-sidebar-page.h"

enum {
	COLUMN_ICON,
	COLUMN_NAME,
	COLUMN_DESCRIPTION,
	COLUMN_ATTACHMENT,
	N_COLS
};

enum {
	PROP_0,
	PROP_WIDGET,
};

enum {
	SIGNAL_POPUP_MENU,
	N_SIGNALS
};

static guint signals[N_SIGNALS];

struct _EvSidebarAttachmentsPrivate {
	GtkWidget      *icon_view;
	GtkListStore   *model;

	/* Icons */
	GtkIconTheme   *icon_theme;
	GHashTable     *icon_cache;
};

static void ev_sidebar_attachments_page_iface_init (EvSidebarPageInterface *iface);

G_DEFINE_TYPE_EXTENDED (EvSidebarAttachments,
                        ev_sidebar_attachments,
                        GTK_TYPE_BOX,
                        0, 
                        G_IMPLEMENT_INTERFACE (EV_TYPE_SIDEBAR_PAGE, 
					       ev_sidebar_attachments_page_iface_init))

#define EV_SIDEBAR_ATTACHMENTS_GET_PRIVATE(object) \
                (G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_SIDEBAR_ATTACHMENTS, EvSidebarAttachmentsPrivate))

/* Icon cache */
static void
ev_sidebar_attachments_icon_cache_add (EvSidebarAttachments *ev_attachbar,
				       const gchar          *mime_type,
				       const GdkPixbuf      *pixbuf)
{
	g_assert (mime_type != NULL);
	g_assert (GDK_IS_PIXBUF (pixbuf));

	g_hash_table_insert (ev_attachbar->priv->icon_cache,
			     (gpointer)g_strdup (mime_type),
			     (gpointer)pixbuf);
			     
}

static GdkPixbuf *
icon_theme_get_pixbuf_from_mime_type (GtkIconTheme *icon_theme,
				      const gchar  *mime_type)
{
	const char *separator;
	GString *icon_name;
	GdkPixbuf *pixbuf;

	separator = strchr (mime_type, '/');
	if (!separator)
		return NULL; /* maybe we should return a GError with "invalid MIME-type" */

	icon_name = g_string_new ("gnome-mime-");
	g_string_append_len (icon_name, mime_type, separator - mime_type);
	g_string_append_c (icon_name, '-');
	g_string_append (icon_name, separator + 1);
	pixbuf = gtk_icon_theme_load_icon (icon_theme, icon_name->str, 48, 0, NULL);
	g_string_free (icon_name, TRUE);
	if (pixbuf)
		return pixbuf;
	
	icon_name = g_string_new ("gnome-mime-");
	g_string_append_len (icon_name, mime_type, separator - mime_type);
	pixbuf = gtk_icon_theme_load_icon (icon_theme, icon_name->str, 48, 0, NULL);
	g_string_free (icon_name, TRUE);
	
	return pixbuf;
}

static GdkPixbuf *
ev_sidebar_attachments_icon_cache_get (EvSidebarAttachments *ev_attachbar,
				       const gchar          *mime_type)
{
	GdkPixbuf *pixbuf = NULL;
	
	g_assert (mime_type != NULL);

	pixbuf = g_hash_table_lookup (ev_attachbar->priv->icon_cache,
				      mime_type);

	if (GDK_IS_PIXBUF (pixbuf))
		return pixbuf;

	pixbuf = icon_theme_get_pixbuf_from_mime_type (ev_attachbar->priv->icon_theme,
						       mime_type);

	if (GDK_IS_PIXBUF (pixbuf))
		ev_sidebar_attachments_icon_cache_add (ev_attachbar,
						       mime_type,
						       pixbuf);

	return pixbuf;
}

static gboolean
icon_cache_update_icon (gchar                *key,
			GdkPixbuf            *value,
			EvSidebarAttachments *ev_attachbar)
{
	GdkPixbuf *pixbuf = NULL;

	pixbuf = icon_theme_get_pixbuf_from_mime_type (ev_attachbar->priv->icon_theme,
						       key);

	ev_sidebar_attachments_icon_cache_add (ev_attachbar,
					       key,
					       pixbuf);
	
	return FALSE;
}

static void
ev_sidebar_attachments_icon_cache_refresh (EvSidebarAttachments *ev_attachbar)
{
	g_hash_table_foreach_remove (ev_attachbar->priv->icon_cache,
				     (GHRFunc) icon_cache_update_icon,
				     ev_attachbar);
}

static EvAttachment *
ev_sidebar_attachments_get_attachment_at_pos (EvSidebarAttachments *ev_attachbar,
					      gint                  x,
					      gint                  y)
{
	GtkTreePath  *path = NULL;
	GtkTreeIter   iter;
	EvAttachment *attachment = NULL;

	path = gtk_icon_view_get_path_at_pos (GTK_ICON_VIEW (ev_attachbar->priv->icon_view),
					      x, y);
	if (!path) {
		return NULL;
	}

	gtk_tree_model_get_iter (GTK_TREE_MODEL (ev_attachbar->priv->model),
				 &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL (ev_attachbar->priv->model), &iter,
			    COLUMN_ATTACHMENT, &attachment,
			    -1);

	gtk_icon_view_select_path (GTK_ICON_VIEW (ev_attachbar->priv->icon_view),
				   path);
	
	gtk_tree_path_free (path);

	return attachment;
}

static gboolean
ev_sidebar_attachments_popup_menu_show (EvSidebarAttachments *ev_attachbar,
					gint                  x,
					gint                  y)
{
	GtkIconView *icon_view;
	GtkTreePath *path;
	GList       *selected = NULL, *l;
	GList       *attach_list = NULL;

	icon_view = GTK_ICON_VIEW (ev_attachbar->priv->icon_view);
	
	path = gtk_icon_view_get_path_at_pos (icon_view, x, y);
	if (!path)
		return FALSE;

	if (!gtk_icon_view_path_is_selected (icon_view, path)) {
		gtk_icon_view_unselect_all (icon_view);
		gtk_icon_view_select_path (icon_view, path);
	}

	gtk_tree_path_free (path);
	
	selected = gtk_icon_view_get_selected_items (icon_view);
	if (!selected)
		return FALSE;

	for (l = selected; l && l->data; l = g_list_next (l)) {
		GtkTreeIter   iter;
		EvAttachment *attachment = NULL;

		path = (GtkTreePath *) l->data;

		gtk_tree_model_get_iter (GTK_TREE_MODEL (ev_attachbar->priv->model),
					 &iter, path);
		gtk_tree_model_get (GTK_TREE_MODEL (ev_attachbar->priv->model), &iter,
				    COLUMN_ATTACHMENT, &attachment,
				    -1);

		if (attachment)
			attach_list = g_list_prepend (attach_list, attachment);

		gtk_tree_path_free (path);
	}

	g_list_free (selected);

	if (!attach_list)
		return FALSE;

	g_signal_emit (ev_attachbar, signals[SIGNAL_POPUP_MENU], 0, attach_list);

	return TRUE;
}

static gboolean
ev_sidebar_attachments_popup_menu (GtkWidget *widget)
{
	EvSidebarAttachments *ev_attachbar = EV_SIDEBAR_ATTACHMENTS (widget);
	gint                  x, y;

	ev_document_misc_get_pointer_position (widget, &x, &y);

	return ev_sidebar_attachments_popup_menu_show (ev_attachbar, x, y);
}

static gboolean
ev_sidebar_attachments_button_press (EvSidebarAttachments *ev_attachbar,
				     GdkEventButton       *event,
				     GtkWidget            *icon_view)
{
	if (!gtk_widget_has_focus (icon_view)) {
		gtk_widget_grab_focus (icon_view);
	}
	
	if (event->button == 2)
		return FALSE;

	switch (event->button) {
	        case 1:
			if (event->type == GDK_2BUTTON_PRESS) {
				GError *error = NULL;
				EvAttachment *attachment;
				
				attachment = ev_sidebar_attachments_get_attachment_at_pos (ev_attachbar,
											   event->x,
											   event->y);
				if (!attachment)
					return FALSE;
				
				ev_attachment_open (attachment,
						    gtk_widget_get_screen (GTK_WIDGET (ev_attachbar)),
						    event->time,
						    &error);
				
				if (error) {
					g_warning ("%s", error->message);
					g_error_free (error);
				}
				
				g_object_unref (attachment);
				
				return TRUE;
			}
			break;
	        case 3: 
			return ev_sidebar_attachments_popup_menu_show (ev_attachbar, event->x, event->y);
	}

	return FALSE;
}

static void
ev_sidebar_attachments_update_icons (EvSidebarAttachments *ev_attachbar,
				     gpointer              user_data)
{
	GtkTreeIter iter;
	gboolean    valid;

	ev_sidebar_attachments_icon_cache_refresh (ev_attachbar);
	
	valid = gtk_tree_model_get_iter_first (
		GTK_TREE_MODEL (ev_attachbar->priv->model),
		&iter);

	while (valid) {
		EvAttachment *attachment = NULL;
		GdkPixbuf    *pixbuf = NULL;
		const gchar  *mime_type;

		gtk_tree_model_get (GTK_TREE_MODEL (ev_attachbar->priv->model), &iter,
				    COLUMN_ATTACHMENT, &attachment,
				    -1);

		mime_type = ev_attachment_get_mime_type (attachment);

		if (attachment)
			g_object_unref (attachment);

		pixbuf = ev_sidebar_attachments_icon_cache_get (ev_attachbar,
								mime_type);

		gtk_list_store_set (ev_attachbar->priv->model, &iter,
				    COLUMN_ICON, pixbuf,
				    -1);

		valid = gtk_tree_model_iter_next (
			GTK_TREE_MODEL (ev_attachbar->priv->model),
			&iter);
	}
}

static void
ev_sidebar_attachments_screen_changed (GtkWidget *widget,
				       GdkScreen *old_screen)
{
	EvSidebarAttachments *ev_attachbar = EV_SIDEBAR_ATTACHMENTS (widget);
	GdkScreen            *screen;

	if (!ev_attachbar->priv->icon_theme)
		return;
	
	screen = gtk_widget_get_screen (widget);
	if (screen == old_screen)
		return;

	if (old_screen) {
		g_signal_handlers_disconnect_by_func (
			gtk_icon_theme_get_for_screen (old_screen),
			G_CALLBACK (ev_sidebar_attachments_update_icons),
			ev_attachbar);
	}

	ev_attachbar->priv->icon_theme = gtk_icon_theme_get_for_screen (screen);
	g_signal_connect_swapped (ev_attachbar->priv->icon_theme,
				  "changed",
				  G_CALLBACK (ev_sidebar_attachments_update_icons),
				  (gpointer) ev_attachbar);

	if (GTK_WIDGET_CLASS (ev_sidebar_attachments_parent_class)->screen_changed) {
		GTK_WIDGET_CLASS (ev_sidebar_attachments_parent_class)->screen_changed (widget, old_screen);
	}
}

static void
ev_sidebar_attachments_drag_data_get (GtkWidget        *widget,
				      GdkDragContext   *drag_context,
				      GtkSelectionData *data,
				      guint             info,
				      guint             time,
				      gpointer          user_data)
{
	EvSidebarAttachments *ev_attachbar = EV_SIDEBAR_ATTACHMENTS (user_data);
	GList                *selected = NULL, *l;
        GPtrArray            *uris;
        char                **uri_list;

	selected = gtk_icon_view_get_selected_items (GTK_ICON_VIEW (ev_attachbar->priv->icon_view));
	if (!selected)
		return;

        uris = g_ptr_array_new ();
	
	for (l = selected; l && l->data; l = g_list_next (l)) {
		EvAttachment *attachment;
		GtkTreePath  *path;
		GtkTreeIter   iter;
		GFile        *file;
		gchar        *template;
		GError       *error = NULL;
		
		path = (GtkTreePath *) l->data;

		gtk_tree_model_get_iter (GTK_TREE_MODEL (ev_attachbar->priv->model),
					 &iter, path);
		gtk_tree_model_get (GTK_TREE_MODEL (ev_attachbar->priv->model), &iter,
				    COLUMN_ATTACHMENT, &attachment,
				    -1);

                /* FIXMEchpe: convert to filename encoding first! */
                template = g_strdup_printf ("%s.XXXXXX", ev_attachment_get_name (attachment));
                file = ev_mkstemp_file (template, &error);
                g_free (template);
		
		if (file != NULL && ev_attachment_save (attachment, file, &error)) {
			gchar *uri;

			uri = g_file_get_uri (file);
                        g_ptr_array_add (uris, uri);
		}
	
		if (error) {
			g_warning ("%s", error->message);
			g_error_free (error);
		}

		gtk_tree_path_free (path);
		g_object_unref (file);
		g_object_unref (attachment);
	}

        g_ptr_array_add (uris, NULL); /* NULL-terminate */
        uri_list = (char **) g_ptr_array_free (uris, FALSE);
        gtk_selection_data_set_uris (data, uri_list);
        g_strfreev (uri_list);

	g_list_free (selected);
}

static void
ev_sidebar_attachments_get_property (GObject    *object,
				     guint       prop_id,
			    	     GValue     *value,
		      	             GParamSpec *pspec)
{
	EvSidebarAttachments *ev_sidebar_attachments;
  
	ev_sidebar_attachments = EV_SIDEBAR_ATTACHMENTS (object);

	switch (prop_id) {
	        case PROP_WIDGET:
			g_value_set_object (value, ev_sidebar_attachments->priv->icon_view);
			break;
	        default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
ev_sidebar_attachments_dispose (GObject *object)
{
	EvSidebarAttachments *ev_attachbar = EV_SIDEBAR_ATTACHMENTS (object);

	if (ev_attachbar->priv->icon_theme) {
		g_signal_handlers_disconnect_by_func (
			ev_attachbar->priv->icon_theme, 
			G_CALLBACK (ev_sidebar_attachments_update_icons),
			ev_attachbar);
		ev_attachbar->priv->icon_theme = NULL;
	}
	
	if (ev_attachbar->priv->model) {
		g_object_unref (ev_attachbar->priv->model);
		ev_attachbar->priv->model = NULL;
	}

	if (ev_attachbar->priv->icon_cache) {
		g_hash_table_destroy (ev_attachbar->priv->icon_cache);
		ev_attachbar->priv->icon_cache = NULL;
	}

	G_OBJECT_CLASS (ev_sidebar_attachments_parent_class)->dispose (object);
}

static void
ev_sidebar_attachments_class_init (EvSidebarAttachmentsClass *ev_attachbar_class)
{
	GObjectClass   *g_object_class;
	GtkWidgetClass *gtk_widget_class;

	g_object_class = G_OBJECT_CLASS (ev_attachbar_class);
	gtk_widget_class = GTK_WIDGET_CLASS (ev_attachbar_class);

	g_object_class->get_property = ev_sidebar_attachments_get_property;
	g_object_class->dispose = ev_sidebar_attachments_dispose;
	gtk_widget_class->popup_menu = ev_sidebar_attachments_popup_menu;
	gtk_widget_class->screen_changed = ev_sidebar_attachments_screen_changed;

	g_type_class_add_private (g_object_class, sizeof (EvSidebarAttachmentsPrivate));

	/* Signals */
	signals[SIGNAL_POPUP_MENU] =
		g_signal_new ("popup",
			      G_TYPE_FROM_CLASS (g_object_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (EvSidebarAttachmentsClass, popup_menu),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);

	g_object_class_override_property (g_object_class,
					  PROP_WIDGET,
					  "main-widget");
}

static void
ev_sidebar_attachments_init (EvSidebarAttachments *ev_attachbar)
{
	GtkWidget *swindow;
	
	ev_attachbar->priv = EV_SIDEBAR_ATTACHMENTS_GET_PRIVATE (ev_attachbar);

	swindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swindow),
					     GTK_SHADOW_IN);
	/* Data Model */
	ev_attachbar->priv->model = gtk_list_store_new (N_COLS,
							GDK_TYPE_PIXBUF, 
							G_TYPE_STRING,  
							G_TYPE_STRING,
							EV_TYPE_ATTACHMENT);

	/* Icon View */
	ev_attachbar->priv->icon_view =
		gtk_icon_view_new_with_model (GTK_TREE_MODEL (ev_attachbar->priv->model));
	gtk_icon_view_set_selection_mode (GTK_ICON_VIEW (ev_attachbar->priv->icon_view),
					  GTK_SELECTION_MULTIPLE);
	gtk_icon_view_set_columns (GTK_ICON_VIEW (ev_attachbar->priv->icon_view), -1);
	g_object_set (G_OBJECT (ev_attachbar->priv->icon_view),
		      "text-column", COLUMN_NAME,
		      "pixbuf-column", COLUMN_ICON,
		      NULL);
	g_signal_connect_swapped (ev_attachbar->priv->icon_view,
				  "button-press-event",
				  G_CALLBACK (ev_sidebar_attachments_button_press),
				  (gpointer) ev_attachbar);

	gtk_container_add (GTK_CONTAINER (swindow),
			   ev_attachbar->priv->icon_view);

        gtk_box_pack_start (GTK_BOX (ev_attachbar), swindow, TRUE, TRUE, 0);
	gtk_widget_show_all (GTK_WIDGET (ev_attachbar));

	/* Icon Theme */
	ev_attachbar->priv->icon_theme = NULL;

	/* Icon Cache */
	ev_attachbar->priv->icon_cache = g_hash_table_new_full (g_str_hash,
								g_str_equal,
								g_free,
								g_object_unref);

	/* Drag and Drop */
	gtk_icon_view_enable_model_drag_source (
		GTK_ICON_VIEW (ev_attachbar->priv->icon_view),
		GDK_BUTTON1_MASK,
		NULL, 0,
		GDK_ACTION_COPY);
        gtk_drag_source_add_uri_targets (ev_attachbar->priv->icon_view);

	g_signal_connect (ev_attachbar->priv->icon_view,
			  "drag-data-get",
			  G_CALLBACK (ev_sidebar_attachments_drag_data_get),
			  (gpointer) ev_attachbar);	
}

GtkWidget *
ev_sidebar_attachments_new (void)
{
	GtkWidget *ev_attachbar;

	ev_attachbar = g_object_new (EV_TYPE_SIDEBAR_ATTACHMENTS,
                                     "orientation", GTK_ORIENTATION_VERTICAL,
                                     NULL);

	return ev_attachbar;
}

static void
job_finished_callback (EvJobAttachments     *job,
		       EvSidebarAttachments *ev_attachbar)
{
	GList *l;
	
	for (l = job->attachments; l && l->data; l = g_list_next (l)) {
		EvAttachment *attachment;
		GtkTreeIter   iter;
		GdkPixbuf    *pixbuf = NULL;
		const gchar  *mime_type;

		attachment = EV_ATTACHMENT (l->data);

		mime_type = ev_attachment_get_mime_type (attachment);
		pixbuf = ev_sidebar_attachments_icon_cache_get (ev_attachbar,
								mime_type);

		gtk_list_store_append (ev_attachbar->priv->model, &iter);
		gtk_list_store_set (ev_attachbar->priv->model, &iter,
				    COLUMN_NAME, ev_attachment_get_name (attachment),
				    COLUMN_ICON, pixbuf,
				    COLUMN_ATTACHMENT, attachment, 
				    -1);
	}

	g_object_unref (job);
}


static void
ev_sidebar_attachments_document_changed_cb (EvDocumentModel      *model,
					    GParamSpec           *pspec,
					    EvSidebarAttachments *ev_attachbar)
{
	EvDocument *document = ev_document_model_get_document (model);
	EvJob *job;

	if (!EV_IS_DOCUMENT_ATTACHMENTS (document))
		return;

	if (!ev_document_attachments_has_attachments (EV_DOCUMENT_ATTACHMENTS (document)))
		return;

	if (!ev_attachbar->priv->icon_theme) {
		GdkScreen *screen;

		screen = gtk_widget_get_screen (GTK_WIDGET (ev_attachbar));
		ev_attachbar->priv->icon_theme = gtk_icon_theme_get_for_screen (screen);
		g_signal_connect_swapped (ev_attachbar->priv->icon_theme,
					  "changed",
					  G_CALLBACK (ev_sidebar_attachments_update_icons),
					  (gpointer) ev_attachbar);
	}
		
	gtk_list_store_clear (ev_attachbar->priv->model);

	job = ev_job_attachments_new (document);
	g_signal_connect (job, "finished",
			  G_CALLBACK (job_finished_callback),
			  ev_attachbar);
	g_signal_connect (job, "cancelled",
			  G_CALLBACK (g_object_unref),
			  NULL);
	/* The priority doesn't matter for this job */
	ev_job_scheduler_push_job (job, EV_JOB_PRIORITY_NONE);
}

static void
ev_sidebar_attachments_set_model (EvSidebarPage   *page,
				  EvDocumentModel *model)
{
	g_signal_connect (model, "notify::document",
			  G_CALLBACK (ev_sidebar_attachments_document_changed_cb),
			  page);
}

static gboolean
ev_sidebar_attachments_support_document (EvSidebarPage   *sidebar_page,
					 EvDocument      *document)
{
	return (EV_IS_DOCUMENT_ATTACHMENTS (document) &&
		ev_document_attachments_has_attachments (EV_DOCUMENT_ATTACHMENTS (document)));
}

static const gchar*
ev_sidebar_attachments_get_label (EvSidebarPage *sidebar_page)
{
	return _("Attachments");
}

static void
ev_sidebar_attachments_page_iface_init (EvSidebarPageInterface *iface)
{
	iface->support_document = ev_sidebar_attachments_support_document;
	iface->set_model = ev_sidebar_attachments_set_model;
	iface->get_label = ev_sidebar_attachments_get_label;
}

