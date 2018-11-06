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

/* Define a maximum width in case there is a file with a very long name */
#define MAX_WIDHT_LABEL 64
/* Define a maximum width for password entry */
#define MAX_WIDHT_PASSWORD_ENTRY 32

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

	char         *filename;
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

	g_clear_pointer (&password_view->priv->filename, g_free);

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
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 24);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 64);
	gtk_container_add (GTK_CONTAINER (password_view), vbox);
	gtk_widget_set_halign (vbox, GTK_ALIGN_CENTER);
	gtk_widget_set_valign (vbox, GTK_ALIGN_CENTER);

	label = gtk_label_new (NULL);
	gtk_box_pack_start (GTK_BOX (vbox), label,
			    FALSE, FALSE, 0);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_max_width_chars (GTK_LABEL (label), MAX_WIDHT_LABEL);
	gtk_label_set_line_wrap_mode (GTK_LABEL (label), PANGO_WRAP_WORD_CHAR);
	password_view->priv->label = label;

	image = gtk_image_new_from_icon_name ("dialog-password-symbolic",
					      GTK_ICON_SIZE_DIALOG);
	gtk_box_pack_start (GTK_BOX (vbox), image, FALSE, FALSE, 0);

	label = gtk_label_new (NULL);
	markup = g_strdup_printf ("<span size=\"x-large\">%s</span>",
				  _("This document is locked and can only be read by entering the correct password."));
	gtk_label_set_markup (GTK_LABEL (label), markup);
	g_free (markup);

	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_max_width_chars (GTK_LABEL (label), MAX_WIDHT_LABEL);
	gtk_label_set_line_wrap_mode (GTK_LABEL (label), PANGO_WRAP_WORD_CHAR);

	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	button = gtk_button_new_with_mnemonic (_("_Unlock Document"));
	g_signal_connect (button, "clicked", G_CALLBACK (ev_password_view_clicked_cb), password_view);
	gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);

	gtk_widget_show_all (vbox);
	gtk_widget_pop_composite_child ();
}

/* Public functions */
void
ev_password_view_set_filename (EvPasswordView *password_view,
			       const char     *filename)
{
	gchar *markup;

	g_return_if_fail (EV_IS_PASSWORD_VIEW (password_view));
	g_return_if_fail (filename != NULL);

	if (g_strcmp0 (password_view->priv->filename, filename) == 0)
		return;

	g_free (password_view->priv->filename);
	password_view->priv->filename = g_strdup (filename);

	markup = g_markup_printf_escaped ("<span size=\"x-large\" weight=\"bold\">%s</span>",
					  filename);
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
	GtkMessageDialog *dialog;
	GtkWidget *message_area;
	GtkWidget *grid, *label;
	GtkWidget *password_entry;
	gchar     *text;

        text = g_markup_printf_escaped (_("The document “%s” is locked and requires a password before it can be opened."),
                                        password_view->priv->filename);

	dialog = GTK_MESSAGE_DIALOG (gtk_message_dialog_new (password_view->priv->parent_window,
		                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		                         GTK_MESSAGE_QUESTION,
		                         GTK_BUTTONS_NONE,
					 _("Password required")));
	gtk_message_dialog_format_secondary_markup (dialog, "%s", text);
	g_free (text);

	message_area = gtk_message_dialog_get_message_area (dialog);

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				_("_Cancel"), GTK_RESPONSE_CANCEL,
				_("_Unlock"), GTK_RESPONSE_OK,
				NULL);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
					   GTK_RESPONSE_OK, FALSE);

	grid = gtk_grid_new ();
	gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
	gtk_box_pack_start (GTK_BOX (message_area), grid,
			    FALSE, FALSE, 6);
	gtk_widget_set_halign (grid, GTK_ALIGN_CENTER);
	gtk_widget_show (grid);

	label = gtk_label_new_with_mnemonic (_("_Password:"));
	g_object_set (G_OBJECT (label), "xalign", 0., "yalign", 0.5, NULL);

	password_entry = gtk_entry_new ();
	gtk_entry_set_visibility (GTK_ENTRY (password_entry), FALSE);
	g_object_set (G_OBJECT (password_entry), "width-chars", 32, NULL);
	g_signal_connect (password_entry, "changed",
			  G_CALLBACK (ev_password_dialog_entry_changed_cb),
			  dialog);
	g_signal_connect (password_entry, "activate",
			  G_CALLBACK (ev_password_dialog_entry_activated_cb),
			  dialog);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
	gtk_widget_show (label);

	gtk_grid_attach (GTK_GRID (grid), password_entry, 1, 0, 1, 1);
        gtk_widget_set_hexpand (password_entry, TRUE);
	gtk_widget_show (password_entry);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label),
				       password_entry);

	password_view->priv->password_entry = password_entry;

	if (ev_keyring_is_available ()) {
		GtkWidget  *choice;
		GtkWidget  *remember_box;
		GSList     *group;

		remember_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
		gtk_box_pack_start (GTK_BOX (message_area), remember_box,
				    FALSE, FALSE, 0);
		gtk_widget_set_halign (remember_box, GTK_ALIGN_CENTER);
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

