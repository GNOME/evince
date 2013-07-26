/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2008 Carlos Garcia Campos <carlosgc@gnome.org>
 *  Copyright (C) 2005 Red Hat, Inc
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

#include "ev-keyring.h"
#include "ev-password-view.h"

enum {
	UNLOCK,
	LAST_SIGNAL
};
struct _EvPasswordViewPrivate {
	GtkWindow    *parent_window;
	GtkWidget    *label;
	GtkWidget    *password_entry;

	gchar        *password;
	GPasswordSave password_save;

	GFile        *uri_file;
};

#define EV_PASSWORD_VIEW_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_PASSWORD_VIEW, EvPasswordViewPrivate));

static guint password_view_signals [LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (EvPasswordView, ev_password_view, GTK_TYPE_VIEWPORT)

static void
ev_password_view_finalize (GObject *object)
{
	EvPasswordView *password_view = EV_PASSWORD_VIEW (object);

	if (password_view->priv->password) {
		g_free (password_view->priv->password);
		password_view->priv->password = NULL;
	}

	password_view->priv->parent_window = NULL;

	if (password_view->priv->uri_file) {
		g_object_unref (password_view->priv->uri_file);
		password_view->priv->uri_file = NULL;
	}

	G_OBJECT_CLASS (ev_password_view_parent_class)->finalize (object);
}

static void
ev_password_view_class_init (EvPasswordViewClass *class)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS (class);

	password_view_signals[UNLOCK] =
		g_signal_new ("unlock",
			      G_TYPE_FROM_CLASS (g_object_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (EvPasswordViewClass, unlock),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	g_type_class_add_private (g_object_class, sizeof (EvPasswordViewPrivate));

	g_object_class->finalize = ev_password_view_finalize;
}

static void
ev_password_view_clicked_cb (GtkWidget      *button,
			     EvPasswordView *password_view)
{
	ev_password_view_ask_password (password_view);
}

static void
ev_password_view_init (EvPasswordView *password_view)
{
	GtkWidget *align;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *image;
	GtkWidget *button;
	GtkWidget *label;
	gchar     *markup;

	password_view->priv = EV_PASSWORD_VIEW_GET_PRIVATE (password_view);

	password_view->priv->password_save = G_PASSWORD_SAVE_NEVER;
	
	gtk_widget_push_composite_child ();

	/* set ourselves up */
	align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 24);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 24);
	gtk_container_add (GTK_CONTAINER (password_view), align);
	gtk_container_add (GTK_CONTAINER (align), vbox);

	password_view->priv->label =
		(GtkWidget *) g_object_new (GTK_TYPE_LABEL,
					    "wrap", TRUE,
					    "selectable", TRUE,
					    NULL);
	gtk_box_pack_start (GTK_BOX (vbox), password_view->priv->label, FALSE, FALSE, 0);

	image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_AUTHENTICATION,
					  GTK_ICON_SIZE_DIALOG);
	gtk_box_pack_start (GTK_BOX (vbox), image, FALSE, FALSE, 0);

	label = gtk_label_new (NULL);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	markup = g_strdup_printf ("<span size=\"x-large\">%s</span>",
				  _("This document is locked and can only be read by entering the correct password."));
	gtk_label_set_markup (GTK_LABEL (label), markup);
	g_free (markup);
			      
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	button = gtk_button_new_with_mnemonic (_("_Unlock Document"));
	g_signal_connect (button, "clicked", G_CALLBACK (ev_password_view_clicked_cb), password_view);
	gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);

	gtk_widget_show_all (align);
	gtk_widget_pop_composite_child ();
}

/* Public functions */
void
ev_password_view_set_uri (EvPasswordView *password_view,
			  const char     *uri)
{
	gchar *markup, *file_name;
	GFile *file;

	g_return_if_fail (EV_IS_PASSWORD_VIEW (password_view));
	g_return_if_fail (uri != NULL);

	file = g_file_new_for_uri (uri);
	if (password_view->priv->uri_file &&
	    g_file_equal (file, password_view->priv->uri_file)) {
		g_object_unref (file);
		return;
	}
	if (password_view->priv->uri_file)
		g_object_unref (password_view->priv->uri_file);
	password_view->priv->uri_file = file;

	file_name = g_file_get_basename (password_view->priv->uri_file);
	markup = g_markup_printf_escaped ("<span size=\"x-large\" weight=\"bold\">%s</span>",
					  file_name);
	g_free (file_name);

	gtk_label_set_markup (GTK_LABEL (password_view->priv->label), markup);
	g_free (markup);
}

static void
ev_password_dialog_got_response (GtkDialog      *dialog,
				 gint            response_id,
				 EvPasswordView *password_view)
{
	gtk_widget_set_sensitive (GTK_WIDGET (password_view), TRUE);
	
	if (response_id == GTK_RESPONSE_OK) {
		g_free (password_view->priv->password);
		password_view->priv->password =
			g_strdup (gtk_entry_get_text (GTK_ENTRY (password_view->priv->password_entry)));
		
		g_signal_emit (password_view, password_view_signals[UNLOCK], 0);
	}
	
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
ev_password_dialog_remember_button_toggled (GtkToggleButton *button,
					    EvPasswordView  *password_view)
{
	if (gtk_toggle_button_get_active (button)) {
		gpointer data;
		
		data = g_object_get_data (G_OBJECT (button), "password-save");
		password_view->priv->password_save = GPOINTER_TO_INT (data);
	}
}

static void
ev_password_dialog_entry_changed_cb (GtkEditable *editable,
				     GtkDialog   *dialog)
{
	const char *text;

	text = gtk_entry_get_text (GTK_ENTRY (editable));

	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK,
					   (text != NULL && *text != '\0'));
}

static void
ev_password_dialog_entry_activated_cb (GtkEntry  *entry,
				       GtkDialog *dialog)
{
	gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}

void
ev_password_view_ask_password (EvPasswordView *password_view)
{
	GtkDialog *dialog;
	GtkWidget *content_area, *action_area;
	GtkWidget *entry_container;
	GtkWidget *hbox, *main_vbox, *vbox, *icon;
	GtkWidget *grid;
	GtkWidget *label;
	gchar     *text, *markup, *file_name;

	gtk_widget_set_sensitive (GTK_WIDGET (password_view), FALSE);
	
	dialog = GTK_DIALOG (gtk_dialog_new ());
	content_area = gtk_dialog_get_content_area (dialog);
	action_area = gtk_dialog_get_action_area (dialog);

	/* Set the dialog up with HIG properties */
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (content_area), 2); /* 2 * 5 + 2 = 12 */
	gtk_container_set_border_width (GTK_CONTAINER (action_area), 5);
	gtk_box_set_spacing (GTK_BOX (action_area), 6);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Enter password"));
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), GTK_STOCK_DIALOG_AUTHENTICATION);
	gtk_window_set_transient_for (GTK_WINDOW (dialog), password_view->priv->parent_window);
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

	gtk_dialog_add_buttons (dialog,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				_("_Unlock Document"), GTK_RESPONSE_OK,
				NULL);
	gtk_dialog_set_default_response (dialog, GTK_RESPONSE_OK);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
					   GTK_RESPONSE_OK, FALSE);
	gtk_dialog_set_alternative_button_order (dialog,
						 GTK_RESPONSE_OK,
						 GTK_RESPONSE_CANCEL,
						 -1);
	
	/* Build contents */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
	gtk_box_pack_start (GTK_BOX (content_area), hbox, TRUE, TRUE, 0);
	gtk_widget_show (hbox);

	icon = gtk_image_new_from_stock (GTK_STOCK_DIALOG_AUTHENTICATION,
					 GTK_ICON_SIZE_DIALOG);

	gtk_misc_set_alignment (GTK_MISC (icon), 0.5, 0.0);
	gtk_box_pack_start (GTK_BOX (hbox), icon, FALSE, FALSE, 0);
	gtk_widget_show (icon);

	main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 18);
	gtk_box_pack_start (GTK_BOX (hbox), main_vbox, TRUE, TRUE, 0);
	gtk_widget_show (main_vbox);

	label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	file_name = g_file_get_basename (password_view->priv->uri_file);
        text = g_markup_printf_escaped (_("The document “%s” is locked and requires a password before it can be opened."),
                                        file_name);
        markup = g_strdup_printf ("<span size=\"larger\" weight=\"bold\">%s</span>\n\n%s",
				  _("Password required"),
                                  text);
	gtk_label_set_markup (GTK_LABEL (label), markup);
	g_free (text);
	g_free (markup);
	g_free (file_name);
	gtk_box_pack_start (GTK_BOX (main_vbox), label,
			    FALSE, FALSE, 0);
	gtk_widget_show (label);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start (GTK_BOX (main_vbox), vbox, FALSE, FALSE, 0);
	gtk_widget_show (vbox);

	/* The table that holds the entries */
	entry_container = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);

	gtk_alignment_set_padding (GTK_ALIGNMENT (entry_container),
				   0, 0, 0, 0);
	
	gtk_box_pack_start (GTK_BOX (vbox), entry_container,
			    FALSE, FALSE, 0);
	gtk_widget_show (entry_container);

	grid = gtk_grid_new ();
	gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
	gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
	gtk_container_add (GTK_CONTAINER (entry_container), grid);
	gtk_widget_show (grid);

	label = gtk_label_new_with_mnemonic (_("_Password:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

	password_view->priv->password_entry = gtk_entry_new ();
	gtk_entry_set_visibility (GTK_ENTRY (password_view->priv->password_entry), FALSE);
	g_signal_connect (password_view->priv->password_entry, "changed",
			  G_CALLBACK (ev_password_dialog_entry_changed_cb),
			  dialog);
	g_signal_connect (password_view->priv->password_entry, "activate",
			  G_CALLBACK (ev_password_dialog_entry_activated_cb),
			  dialog);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
	gtk_widget_show (label);

	gtk_grid_attach (GTK_GRID (grid), password_view->priv->password_entry, 1, 0, 1, 1);
        gtk_widget_set_hexpand (password_view->priv->password_entry, TRUE);
	gtk_widget_show (password_view->priv->password_entry);
	
	gtk_label_set_mnemonic_widget (GTK_LABEL (label),
				       password_view->priv->password_entry);

	if (ev_keyring_is_available ()) {
		GtkWidget  *choice;
		GtkWidget  *remember_box;
		GSList     *group;

		remember_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
		gtk_box_pack_start (GTK_BOX (vbox), remember_box,
				    FALSE, FALSE, 0);
		gtk_widget_show (remember_box);

		choice = gtk_radio_button_new_with_mnemonic (NULL, _("Forget password _immediately"));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (choice),
					      password_view->priv->password_save == G_PASSWORD_SAVE_NEVER);
		g_object_set_data (G_OBJECT (choice), "password-save",
				   GINT_TO_POINTER (G_PASSWORD_SAVE_NEVER));
		g_signal_connect (choice, "toggled",
				  G_CALLBACK (ev_password_dialog_remember_button_toggled),
				  password_view);
		gtk_box_pack_start (GTK_BOX (remember_box), choice, FALSE, FALSE, 0);
		gtk_widget_show (choice);

		group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (choice));
		choice = gtk_radio_button_new_with_mnemonic (group, _("Remember password until you _log out"));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (choice),
					      password_view->priv->password_save == G_PASSWORD_SAVE_FOR_SESSION);
		g_object_set_data (G_OBJECT (choice), "password-save",
				   GINT_TO_POINTER (G_PASSWORD_SAVE_FOR_SESSION));
		g_signal_connect (choice, "toggled",
				  G_CALLBACK (ev_password_dialog_remember_button_toggled),
				  password_view);
		gtk_box_pack_start (GTK_BOX (remember_box), choice, FALSE, FALSE, 0);
		gtk_widget_show (choice);

		group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (choice));
		choice = gtk_radio_button_new_with_mnemonic (group, _("Remember _forever"));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (choice),
					      password_view->priv->password_save == G_PASSWORD_SAVE_PERMANENTLY);
		g_object_set_data (G_OBJECT (choice), "password-save",
				   GINT_TO_POINTER (G_PASSWORD_SAVE_PERMANENTLY));
		g_signal_connect (choice, "toggled",
				  G_CALLBACK (ev_password_dialog_remember_button_toggled),
				  password_view);
		gtk_box_pack_start (GTK_BOX (remember_box), choice, FALSE, FALSE, 0);
		gtk_widget_show (choice);
	}

	g_signal_connect (dialog, "response",
			  G_CALLBACK (ev_password_dialog_got_response),
			  password_view);
	
	gtk_widget_show (GTK_WIDGET (dialog));
}

const gchar *
ev_password_view_get_password (EvPasswordView *password_view)
{
	return password_view->priv->password;
}

GPasswordSave
ev_password_view_get_password_save_flags (EvPasswordView *password_view)
{
	return password_view->priv->password_save;
}

GtkWidget *
ev_password_view_new (GtkWindow *parent)
{
	EvPasswordView *retval;

	retval = EV_PASSWORD_VIEW (g_object_new (EV_TYPE_PASSWORD_VIEW, NULL));

	retval->priv->parent_window = parent;

	return GTK_WIDGET (retval);
}

