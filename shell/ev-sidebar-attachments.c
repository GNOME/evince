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
#include <adwaita.h>

#include "ev-document-attachments.h"
#include "ev-document-misc.h"
#include "ev-jobs.h"
#include "ev-job-scheduler.h"
#include "ev-file-helpers.h"
#include "ev-sidebar-attachments.h"
#include "ev-sidebar-page.h"
#include "ev-shell-marshal.h"

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
	PROP_DOCUMENT_MODEL,
};

enum {
	SIGNAL_POPUP_MENU,
	SIGNAL_SAVE_ATTACHMENT,
	N_SIGNALS
};

static guint signals[N_SIGNALS];

struct _EvSidebarAttachmentsPrivate {
	GtkWidget      *icon_view;
	GtkListStore   *model;
};

#define GET_PRIVATE(t) (ev_sidebar_attachments_get_instance_private (t))

static void ev_sidebar_attachments_page_iface_init (EvSidebarPageInterface *iface);
static void ev_sidebar_attachments_set_model (EvSidebarPage   *page,
					      EvDocumentModel *model);

G_DEFINE_TYPE_EXTENDED (EvSidebarAttachments,
                        ev_sidebar_attachments,
                        GTK_TYPE_BOX,
                        0,
                        G_ADD_PRIVATE (EvSidebarAttachments)
                        G_IMPLEMENT_INTERFACE (EV_TYPE_SIDEBAR_PAGE,
					       ev_sidebar_attachments_page_iface_init))

static gboolean
ev_sidebar_attachments_popup_menu_show (EvSidebarAttachments *ev_attachbar,
					gdouble               x,
					gdouble               y)
{
	EvSidebarAttachmentsPrivate *priv = GET_PRIVATE (ev_attachbar);
	GtkIconView *icon_view;
	GtkTreePath *path;
	GList       *selected = NULL, *l;
	GList       *attach_list = NULL;

	icon_view = GTK_ICON_VIEW (priv->icon_view);

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

		gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->model),
					 &iter, path);
		gtk_tree_model_get (GTK_TREE_MODEL (priv->model), &iter,
				    COLUMN_ATTACHMENT, &attachment,
				    -1);

		if (attachment)
			attach_list = g_list_prepend (attach_list, attachment);

		gtk_tree_path_free (path);
	}

	g_list_free (selected);

	if (!attach_list)
		return FALSE;

	g_signal_emit (ev_attachbar, signals[SIGNAL_POPUP_MENU], 0, x, y, attach_list);

	return TRUE;
}

static void
icon_view_item_activated_cb (GtkIconView		*self,
			     GtkTreePath		*path,
			     EvSidebarAttachments	*ev_attachbar)
{
	EvSidebarAttachmentsPrivate *priv = GET_PRIVATE (ev_attachbar);
	EvAttachment *attachment;
	GtkTreeIter iter;
	GError *error = NULL;
	GdkDisplay *display = gtk_widget_get_display (GTK_WIDGET (ev_attachbar));
	GdkAppLaunchContext *context = gdk_display_get_app_launch_context (display);

	gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->model),
				 &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL (priv->model), &iter,
			    COLUMN_ATTACHMENT, &attachment,
			    -1);
	gtk_icon_view_select_path (GTK_ICON_VIEW (priv->icon_view), path);

	if (!attachment)
		return;

	ev_attachment_open (attachment, G_APP_LAUNCH_CONTEXT (context), &error);

	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	g_object_unref (attachment);
	g_clear_object (&context);
}

static void
secondary_button_clicked_cb (GtkGestureClick		*self,
			     gint			 n_press,
			     gdouble			 x,
			     gdouble			 y,
			     EvSidebarAttachments	*ev_attachbar)
{
	ev_sidebar_attachments_popup_menu_show (ev_attachbar, x, y);
}

/* Presents an overwrite confirmation dialog; returns whether the file
 * should be overwritten.
 */
static GtkWidget *
ev_sidebar_attachments_confirm_overwrite_dialog_new (EvSidebarAttachments   *attachbar,
						     const gchar            *uri)
{
	GtkWidget *dialog;
	GtkNative *native;
	gchar     *basename;
	g_autoptr (GFile) file = NULL;

	file = g_file_new_for_uri (uri);
	basename = g_file_get_basename (file);

	native = gtk_widget_get_native (GTK_WIDGET (attachbar));
	dialog = adw_message_dialog_new (native ? GTK_WINDOW (native) : NULL,
	                                 _("A file named “%s” already exists. Do you want to "
	                                   "replace it?"),
	                                 basename);

	adw_message_dialog_format_body (ADW_MESSAGE_DIALOG (dialog),
	                                _("The file “%s” already exists. Replacing"
	                                  " it will overwrite its contents."), uri);

	adw_message_dialog_add_responses (ADW_MESSAGE_DIALOG (dialog),
                                	"cancel",  _("_Cancel"),
                                	"replace", _("_Replace"),
                                	NULL);

	adw_message_dialog_set_response_appearance (ADW_MESSAGE_DIALOG (dialog), "replace", ADW_RESPONSE_DESTRUCTIVE);

	adw_message_dialog_set_default_response (ADW_MESSAGE_DIALOG (dialog), "cancel");
	adw_message_dialog_set_close_response (ADW_MESSAGE_DIALOG (dialog), "cancel");

	g_object_set_data_full (G_OBJECT (dialog), "uri", g_strdup (uri), g_free);

	return dialog;
}

static EvAttachment *
ev_sidebar_attachments_get_selected_attachment (EvSidebarAttachments *ev_attachbar)
{
	EvSidebarAttachmentsPrivate *priv = GET_PRIVATE (ev_attachbar);
	EvAttachment *attachment;
	GList        *selected = NULL;
	GtkTreeIter   iter;
	GtkTreePath  *path;

	selected = gtk_icon_view_get_selected_items (GTK_ICON_VIEW (priv->icon_view));

	if (!selected)
		return NULL;

	if (!selected->data) {
		g_list_free (selected);
		return NULL;
	}

	path = (GtkTreePath *) selected->data;

	gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->model),
	                         &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL (priv->model), &iter,
	                    COLUMN_ATTACHMENT, &attachment,
	                    -1);

	gtk_tree_path_free (path);
	g_list_free (selected);

	return attachment;
}


static GdkContentProvider *
ev_sidebar_attachments_drag_prepare (GtkDragSource		*source,
                                     double		 	x,
				     double		 	y,
                                     EvSidebarAttachments	*ev_attachbar)
{
	g_autoptr (EvAttachment) attachment;
	g_autoptr (GFile)     file = NULL;
	g_autoptr (GError)    error = NULL;
	gchar                *template;
	gchar		     *tempdir;

	attachment = ev_sidebar_attachments_get_selected_attachment (ev_attachbar);

        if (!attachment)
                return NULL;

	tempdir = ev_mkdtemp ("attachments.XXXXXX", &error);
	if (!tempdir) {
		g_warning ("%s", error->message);
		return NULL;
	}

	/* FIXMEchpe: convert to filename encoding first! */
	template = g_build_filename (tempdir, ev_attachment_get_name (attachment), NULL);
	file = g_file_new_for_path (template);
	g_free (template);
	g_free (tempdir);

	if (file == NULL || !ev_attachment_save (attachment, file, &error)) {
		g_warning ("%s", error->message);
		return NULL;
	}

	return gdk_content_provider_new_typed (G_TYPE_FILE, file);
}

typedef struct {
	EvAttachment *attachment;
	EvSidebarAttachments *attachbar;
	gchar *uri;
} EvConfirmWriteData;

static void
response_cb (AdwMessageDialog	*self,
	     gchar		*response,
	     gpointer		 user_data)
{
	EvConfirmWriteData *data = user_data;

	if (g_strcmp0 (response, "replace"))
		g_signal_emit (data->attachbar,
		               signals[SIGNAL_SAVE_ATTACHMENT],
		               0,
		               data->attachment,
		               data->uri,
		               NULL);

	g_clear_object (&data->attachbar);
	g_clear_object (&data->attachment);
	g_free (data->uri);
	g_free (data);

	gtk_window_destroy (GTK_WINDOW (self));
}

static gboolean
ev_sidebar_attachments_drop_cb (GtkDropTarget	*self,
				const GValue	*value,
				gdouble		 x,
				gdouble		 y,
				gpointer	 user_data)
{
	EvSidebarAttachments *ev_attachbar = EV_SIDEBAR_ATTACHMENTS (user_data);
	EvAttachment *attachment = ev_sidebar_attachments_get_selected_attachment (ev_attachbar);
	g_autoptr (GFile) file = NULL;
	g_autofree gchar *uri = NULL;
	GtkWidget *dialog;
	g_autofree gchar *filename = NULL;
	EvConfirmWriteData *data = NULL;

	if (!G_VALUE_HOLDS (value, G_TYPE_FILE) || !attachment)
		return FALSE;

	file = g_value_get_object (value);
	uri = g_file_get_uri (file);

	filename = g_filename_from_uri (uri, NULL, NULL);
	if (!g_file_test (filename, G_FILE_TEST_EXISTS))
		return FALSE;

	dialog = ev_sidebar_attachments_confirm_overwrite_dialog_new (ev_attachbar, uri);

	data = g_new (EvConfirmWriteData, 1);
	data->attachment = g_object_ref (attachment);
	data->attachbar = g_object_ref (ev_attachbar);
	data->uri = g_strdup (uri);
	g_signal_connect (dialog, "response", G_CALLBACK (response_cb), attachment);

	g_clear_object (&attachment);

	return TRUE;
}

static void
ev_sidebar_attachments_get_property (GObject    *object,
				     guint       prop_id,
			    	     GValue     *value,
		      	             GParamSpec *pspec)
{
	EvSidebarAttachments *ev_attachbar = EV_SIDEBAR_ATTACHMENTS (object);
	EvSidebarAttachmentsPrivate *priv = GET_PRIVATE (ev_attachbar);

	switch (prop_id) {
	        case PROP_WIDGET:
			g_value_set_object (value, priv->icon_view);
			break;
	        default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
ev_sidebar_attachments_set_property (GObject      *object,
			       guint         prop_id,
			       const GValue *value,
			       GParamSpec   *pspec)
{
	EvSidebarAttachments *sidebar_attachments = EV_SIDEBAR_ATTACHMENTS (object);

	switch (prop_id)
	{
	case PROP_DOCUMENT_MODEL:
		ev_sidebar_attachments_set_model (EV_SIDEBAR_PAGE (sidebar_attachments),
			EV_DOCUMENT_MODEL (g_value_get_object (value)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ev_sidebar_attachments_class_init (EvSidebarAttachmentsClass *ev_attachbar_class)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (ev_attachbar_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (ev_attachbar_class);

	gtk_widget_class_set_template_from_resource (widget_class,
			"/org/gnome/evince/ui/sidebar-attachments.ui");
	gtk_widget_class_bind_template_child_private (widget_class, EvSidebarAttachments, model);
	gtk_widget_class_bind_template_child_private (widget_class, EvSidebarAttachments, icon_view);

	gtk_widget_class_bind_template_callback (widget_class, icon_view_item_activated_cb);
	gtk_widget_class_bind_template_callback (widget_class, secondary_button_clicked_cb);
	gtk_widget_class_bind_template_callback (widget_class, ev_sidebar_attachments_drag_prepare);
	gtk_widget_class_bind_template_callback (widget_class, ev_sidebar_attachments_drop_cb);

	g_object_class->get_property = ev_sidebar_attachments_get_property;
	g_object_class->set_property = ev_sidebar_attachments_set_property;

	/* Signals */
	signals[SIGNAL_POPUP_MENU] =
		g_signal_new ("popup",
			      G_TYPE_FROM_CLASS (g_object_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (EvSidebarAttachmentsClass, popup_menu),
			      NULL, NULL,
			      ev_shell_marshal_VOID__DOUBLE_DOUBLE_POINTER,
			      G_TYPE_NONE, 3,
			      G_TYPE_DOUBLE,
			      G_TYPE_DOUBLE,
			      G_TYPE_POINTER);

	signals[SIGNAL_SAVE_ATTACHMENT] =
		g_signal_new ("save-attachment",
			      G_TYPE_FROM_CLASS (g_object_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (EvSidebarAttachmentsClass, save_attachment),
			      NULL, NULL,
			      g_cclosure_marshal_generic,
			      G_TYPE_BOOLEAN, 2,
			      G_TYPE_OBJECT,
		              G_TYPE_STRING);

	g_object_class_override_property (g_object_class, PROP_WIDGET, "main-widget");
	g_object_class_override_property (g_object_class, PROP_DOCUMENT_MODEL, "document-model");
}

static void
ev_sidebar_attachments_init (EvSidebarAttachments *ev_attachbar)
{
	gtk_widget_init_template (GTK_WIDGET (ev_attachbar));
}

GtkWidget *
ev_sidebar_attachments_new (void)
{
	return GTK_WIDGET (g_object_new (EV_TYPE_SIDEBAR_ATTACHMENTS, NULL));
}

static void
job_finished_callback (EvJobAttachments     *job,
		       EvSidebarAttachments *ev_attachbar)
{
	EvSidebarAttachmentsPrivate *priv = GET_PRIVATE (ev_attachbar);
	GList *l;

	for (l = job->attachments; l && l->data; l = g_list_next (l)) {
		EvAttachment *attachment;
		GtkTreeIter   iter;
		GIcon        *gicon;
		const gchar  *mime_type;
		gchar        *description;

		attachment = EV_ATTACHMENT (l->data);

		mime_type = ev_attachment_get_mime_type (attachment);
		gicon = g_content_type_get_symbolic_icon (mime_type);
		description =  g_markup_printf_escaped ("%s",
							 ev_attachment_get_description (attachment));

		gtk_list_store_append (priv->model, &iter);
		gtk_list_store_set (priv->model, &iter,
				    COLUMN_NAME, ev_attachment_get_name (attachment),
				    COLUMN_DESCRIPTION, description,
				    COLUMN_ICON, gicon,
				    COLUMN_ATTACHMENT, attachment,
				    -1);
		g_free (description);
	}

	g_object_unref (job);
}


static void
ev_sidebar_attachments_document_changed_cb (EvDocumentModel      *model,
					    GParamSpec           *pspec,
					    EvSidebarAttachments *ev_attachbar)
{
	EvDocument *document = ev_document_model_get_document (model);
	EvSidebarAttachmentsPrivate *priv = GET_PRIVATE (ev_attachbar);
	EvJob *job;

	if (!EV_IS_DOCUMENT_ATTACHMENTS (document))
		return;

	if (!ev_document_attachments_has_attachments (EV_DOCUMENT_ATTACHMENTS (document)))
		return;

	gtk_list_store_clear (priv->model);

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

static void
ev_sidebar_attachments_page_iface_init (EvSidebarPageInterface *iface)
{
	iface->support_document = ev_sidebar_attachments_support_document;
	iface->set_model = ev_sidebar_attachments_set_model;
}
