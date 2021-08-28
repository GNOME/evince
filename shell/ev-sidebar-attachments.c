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
};

enum {
	SIGNAL_POPUP_MENU,
	SIGNAL_SAVE_ATTACHMENT,
	N_SIGNALS
};

enum {
	EV_DND_TARGET_XDS,
	EV_DND_TARGET_TEXT_URI_LIST
};
static guint signals[N_SIGNALS];

#define XDS_ATOM                gdk_atom_intern_static_string  ("XdndDirectSave0")
#define TEXT_ATOM               gdk_atom_intern_static_string  ("text/plain")
#define STRING_ATOM             gdk_atom_intern_static_string  ("STRING")
#define MAX_XDS_ATOM_VAL_LEN    4096
#define XDS_ERROR               'E'
#define XDS_SUCCESS             'S'

struct _EvSidebarAttachmentsPrivate {
	GtkWidget      *icon_view;
	GtkListStore   *model;
};

static void ev_sidebar_attachments_page_iface_init (EvSidebarPageInterface *iface);

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

	g_signal_emit (ev_attachbar, signals[SIGNAL_POPUP_MENU], 0, x, y, attach_list);

	return TRUE;
}

static void
on_icon_view_item_activated (GtkIconView		*self,
			     GtkTreePath		*path,
			     EvSidebarAttachments	*ev_attachbar)
{

	EvAttachment *attachment;
	GtkTreeIter iter;
	GError *error = NULL;

	gtk_tree_model_get_iter (GTK_TREE_MODEL (ev_attachbar->priv->model),
				 &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL (ev_attachbar->priv->model), &iter,
			    COLUMN_ATTACHMENT, &attachment,
			    -1);
	gtk_icon_view_select_path (GTK_ICON_VIEW (ev_attachbar->priv->icon_view), path);

	if (!attachment)
		return;

	ev_attachment_open (attachment,
				gtk_widget_get_display (GTK_WIDGET (ev_attachbar)),
				GDK_CURRENT_TIME,
				&error);

	if (error) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	g_object_unref (attachment);
}

static void
on_secondary_button_clicked (GtkGestureClick		*self,
			     gint			 n_press,
			     gdouble			 x,
			     gdouble			 y,
			     EvSidebarAttachments	*ev_attachbar)
{
	ev_sidebar_attachments_popup_menu_show (ev_attachbar, x, y);
}

#if 0

static gchar *
read_xds_property (GdkDragContext *context)
{
	guchar *prop_text;
	gint    length;
	gchar  *retval = NULL;

	g_assert (context != NULL);

	if (gdk_property_get (gdk_drag_context_get_source_window (context), XDS_ATOM, TEXT_ATOM,
	                      0, MAX_XDS_ATOM_VAL_LEN, FALSE,
	                      NULL, NULL, &length, &prop_text)
	    && prop_text) {

		/* g_strndup will null terminate the string */
		retval = g_strndup ((const gchar *) prop_text, length);
		g_free (prop_text);
	}

	return retval;
}

static void
write_xds_property (GdkDragContext *context,
                    const gchar *value)
{
	g_assert (context != NULL);

	if (value)
		gdk_property_change (gdk_drag_context_get_source_window (context), XDS_ATOM,
		                     TEXT_ATOM, 8, GDK_PROP_MODE_REPLACE,
		                     (const guchar *) value, strlen (value));
	else
		gdk_property_delete (gdk_drag_context_get_source_window (context), XDS_ATOM);
}
#endif

/*
 * Copied from add_custom_button_to_dialog () in gtk+, from file
 * gtkfilechooserwidget.c
 */
static void
ev_add_custom_button_to_dialog (GtkDialog   *dialog,
                                const gchar *mnemonic_label,
                                gint         response_id)
{
	GtkWidget *button;

	button = gtk_button_new_with_mnemonic (mnemonic_label);
	// gtk_widget_set_can_default (button, TRUE);
	gtk_widget_show (button);

	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, response_id);
}

#if 0
/* Presents an overwrite confirmation dialog; returns whether the file
 * should be overwritten.
 * Taken from confirm_dialog_should_accept_filename () in gtk+, from file
 * gtkfilechooserwidget.c
 */
static gboolean
ev_sidebar_attachments_confirm_overwrite (EvSidebarAttachments   *attachbar,
                                          const gchar            *uri)
{
	GtkWidget *toplevel, *dialog;
	int        response;
	gchar     *filename, *basename;
	GFile     *file;

	filename = g_filename_from_uri (uri, NULL, NULL);
	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
		g_free (filename);
		return TRUE;
	}
	g_free (filename);

	file = g_file_new_for_uri (uri);
	basename = g_file_get_basename (file);
	g_object_unref (file);

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (attachbar));
	dialog = gtk_message_dialog_new (gtk_widget_is_toplevel (toplevel) ? GTK_WINDOW (toplevel) : NULL,
	                                 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
	                                 GTK_MESSAGE_QUESTION,
	                                 GTK_BUTTONS_NONE,
	                                 _("A file named “%s” already exists. Do you want to "
	                                   "replace it?"),
	                                 basename);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
	                                          _("The file “%s” already exists. Replacing"
	                                            " it will overwrite its contents."),
	                                          uri);

	gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Cancel"), GTK_RESPONSE_CANCEL);
	ev_add_custom_button_to_dialog (GTK_DIALOG (dialog), _("_Replace"),
	                                GTK_RESPONSE_ACCEPT);
        /* We are mimicking GtkFileChooserWidget behaviour, hence we keep the
         * code synced and act like that.
         */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                                 GTK_RESPONSE_ACCEPT,
                                                 GTK_RESPONSE_CANCEL,
                                                 -1);
G_GNUC_END_IGNORE_DEPRECATIONS
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

	if (gtk_window_has_group (GTK_WINDOW (toplevel)))
		gtk_window_group_add_window (gtk_window_get_group (GTK_WINDOW (toplevel)),
		                             GTK_WINDOW (dialog));

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);

	return (response == GTK_RESPONSE_ACCEPT);
}
#endif

static EvAttachment *
ev_sidebar_attachments_get_selected_attachment (EvSidebarAttachments *ev_attachbar)
{
	EvAttachment *attachment;
	GList        *selected = NULL;
	GtkTreeIter   iter;
	GtkTreePath  *path;

	selected = gtk_icon_view_get_selected_items (GTK_ICON_VIEW (ev_attachbar->priv->icon_view));

	if (!selected)
		return NULL;

	if (!selected->data) {
		g_list_free (selected);
		return NULL;
	}

	path = (GtkTreePath *) selected->data;

	gtk_tree_model_get_iter (GTK_TREE_MODEL (ev_attachbar->priv->model),
	                         &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL (ev_attachbar->priv->model), &iter,
	                    COLUMN_ATTACHMENT, &attachment,
	                    -1);

	gtk_tree_path_free (path);
	g_list_free (selected);

	return attachment;
}

#if 0

static void
ev_sidebar_attachments_drag_begin (GtkWidget            *widget,
                                   GdkDragContext       *drag_context,
                                   EvSidebarAttachments *ev_attachbar)
{
	EvAttachment         *attachment;
	gchar                *filename;

	attachment = ev_sidebar_attachments_get_selected_attachment (ev_attachbar);

        if (!attachment)
                return;

	filename = g_build_filename (ev_attachment_get_name (attachment), NULL);
	write_xds_property (drag_context, filename);

	g_free (filename);
	g_object_unref (attachment);
}

static void
ev_sidebar_attachments_drag_data_get (GtkWidget            *widget,
				      GdkDragContext       *drag_context,
				      GtkSelectionData     *data,
				      guint                 info,
				      guint                 time,
				      EvSidebarAttachments *ev_attachbar)
{
	EvAttachment         *attachment;

	attachment = ev_sidebar_attachments_get_selected_attachment (ev_attachbar);

        if (!attachment)
                return;

	if (info == EV_DND_TARGET_XDS) {
		guchar to_send = XDS_ERROR;
		gchar *uri;

		uri = read_xds_property (drag_context);
		if (!uri) {
			g_object_unref (attachment);
			return;
		}

		if (ev_sidebar_attachments_confirm_overwrite (ev_attachbar, uri)) {
			gboolean success;

			g_signal_emit (ev_attachbar,
			               signals[SIGNAL_SAVE_ATTACHMENT],
			               0,
			               attachment,
			               uri,
			               &success);

			if (success)
				to_send = XDS_SUCCESS;
		}
		g_free (uri);
		gtk_selection_data_set (data, STRING_ATOM, 8, &to_send, sizeof (to_send));
	} else {
		GError *error = NULL;
		GFile  *file;
		gchar  *uri_list[2];
		gchar  *template;

                /* FIXMEchpe: convert to filename encoding first! */
                template = g_strdup_printf ("%s.XXXXXX", ev_attachment_get_name (attachment));
                file = ev_mkstemp_file (template, &error);
                g_free (template);

		if (file != NULL && ev_attachment_save (attachment, file, &error)) {
			uri_list[0] = g_file_get_uri (file);
			uri_list[1] = NULL; /* NULL-terminate */
			g_object_unref (file);
		}

		if (error) {
			g_warning ("%s", error->message);
			g_error_free (error);
		}
		gtk_selection_data_set_uris (data, uri_list);
		g_free (uri_list[0]);
	}
	g_object_unref (attachment);
}
#endif

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


	if (ev_attachbar->priv->model) {
		g_object_unref (ev_attachbar->priv->model);
		ev_attachbar->priv->model = NULL;
	}

	G_OBJECT_CLASS (ev_sidebar_attachments_parent_class)->dispose (object);
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

	gtk_widget_class_bind_template_callback (widget_class, on_icon_view_item_activated);
	gtk_widget_class_bind_template_callback (widget_class, on_secondary_button_clicked);

	g_object_class->get_property = ev_sidebar_attachments_get_property;
	g_object_class->dispose = ev_sidebar_attachments_dispose;

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

	g_object_class_override_property (g_object_class,
					  PROP_WIDGET,
					  "main-widget");
}

static void
ev_sidebar_attachments_init (EvSidebarAttachments *ev_attachbar)
{
#if 0
	static const GtkTargetEntry targets[] = { {"text/uri-list", 0, EV_DND_TARGET_TEXT_URI_LIST},
	                                          {"XdndDirectSave0", 0, EV_DND_TARGET_XDS}};
#endif
	ev_attachbar->priv = ev_sidebar_attachments_get_instance_private (ev_attachbar);

	gtk_widget_init_template (GTK_WIDGET (ev_attachbar));

#if 0
	/* Drag and Drop */
	gtk_icon_view_enable_model_drag_source (
		GTK_ICON_VIEW (ev_attachbar->priv->icon_view),
		GDK_BUTTON1_MASK,
        	targets, G_N_ELEMENTS (targets),
		GDK_ACTION_MOVE);

	g_signal_connect (ev_attachbar->priv->icon_view,
			  "drag-data-get",
			  G_CALLBACK (ev_sidebar_attachments_drag_data_get),
			  (gpointer) ev_attachbar);

	g_signal_connect (ev_attachbar->priv->icon_view,
	                  "drag-begin",
	                  G_CALLBACK (ev_sidebar_attachments_drag_begin),
	                  (gpointer) ev_attachbar);
#endif
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

		gtk_list_store_append (ev_attachbar->priv->model, &iter);
		gtk_list_store_set (ev_attachbar->priv->model, &iter,
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
	EvJob *job;

	if (!EV_IS_DOCUMENT_ATTACHMENTS (document))
		return;

	if (!ev_document_attachments_has_attachments (EV_DOCUMENT_ATTACHMENTS (document)))
		return;

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
