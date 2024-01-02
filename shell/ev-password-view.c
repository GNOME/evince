/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2018 Germán Poo-Caamaño <gpoo@gnome.org>
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
	CANCELLED,
	LAST_SIGNAL
};
typedef struct {
	GtkWidget    *password_entry;

	gchar        *password;
	GPasswordSave password_save;

	char         *filename;
} EvPasswordViewPrivate;

static guint password_view_signals [LAST_SIGNAL] = { 0 };

#define GET_PRIVATE(o) ev_password_view_get_instance_private (o)

G_DEFINE_TYPE_WITH_PRIVATE (EvPasswordView, ev_password_view, GTK_TYPE_BOX)

static void ev_password_view_clicked_cb (GtkWidget      *button,
					 EvPasswordView *password_view);

static void
ev_password_view_finalize (GObject *object)
{
	EvPasswordView *password_view = EV_PASSWORD_VIEW (object);
	EvPasswordViewPrivate *priv = GET_PRIVATE (password_view);

	g_clear_pointer (&priv->password, g_free);
	g_clear_pointer (&priv->filename, g_free);

	G_OBJECT_CLASS (ev_password_view_parent_class)->finalize (object);
}

static void
ev_password_view_class_init (EvPasswordViewClass *class)
{
	GObjectClass *g_object_class;
	GtkWidgetClass *widget_class;

	g_object_class = G_OBJECT_CLASS (class);
	widget_class = GTK_WIDGET_CLASS (class);

	password_view_signals[UNLOCK] =
		g_signal_new ("unlock",
			      G_TYPE_FROM_CLASS (g_object_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	password_view_signals[CANCELLED] =
		g_signal_new ("cancelled",
			      G_TYPE_FROM_CLASS (g_object_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	gtk_widget_class_set_template_from_resource (widget_class,
				"/org/gnome/evince/ui/password-view.ui");
	gtk_widget_class_bind_template_callback (widget_class,
						 ev_password_view_clicked_cb);

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
	EvPasswordViewPrivate *priv = GET_PRIVATE (password_view);

	priv->password_save = G_PASSWORD_SAVE_NEVER;

	gtk_widget_init_template (GTK_WIDGET (password_view));
}

/* Public functions */
void
ev_password_view_set_filename (EvPasswordView *password_view,
			       const char     *filename)
{
	EvPasswordViewPrivate *priv = GET_PRIVATE (password_view);

	g_return_if_fail (EV_IS_PASSWORD_VIEW (password_view));
	g_return_if_fail (filename != NULL);

	if (g_strcmp0 (priv->filename, filename) == 0)
		return;

	g_free (priv->filename);
	priv->filename = g_strdup (filename);
}

static void
ev_password_dialog_got_response (GtkDialog      *dialog,
				 gint            response_id,
				 EvPasswordView *password_view)
{
	EvPasswordViewPrivate *priv = GET_PRIVATE (password_view);

	gtk_widget_set_sensitive (GTK_WIDGET (password_view), TRUE);

	if (response_id == GTK_RESPONSE_OK) {
		g_free (priv->password);
		priv->password =
			g_strdup (gtk_editable_get_text (GTK_EDITABLE (priv->password_entry)));

		g_signal_emit (password_view, password_view_signals[UNLOCK], 0);
	} else if (response_id == GTK_RESPONSE_CANCEL ||
		   response_id == GTK_RESPONSE_CLOSE ||
		   response_id == GTK_RESPONSE_DELETE_EVENT) {
		g_signal_emit (password_view, password_view_signals[CANCELLED], 0);
	}

	gtk_window_destroy (GTK_WINDOW (dialog));
}

static void
ev_password_dialog_remember_button_toggled (GtkCheckButton *button,
					    EvPasswordView  *password_view)
{
	EvPasswordViewPrivate *priv = GET_PRIVATE (password_view);

	if (gtk_check_button_get_active (button)) {
		gpointer data;

		data = g_object_get_data (G_OBJECT (button), "password-save");
		priv->password_save = GPOINTER_TO_INT (data);
	}
}

static void
ev_password_dialog_entry_changed_cb (GtkEditable *editable,
				     GtkDialog   *dialog)
{
	const char *text;

	text = gtk_editable_get_text (GTK_EDITABLE (editable));

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
	EvPasswordViewPrivate *priv = GET_PRIVATE (password_view);
	GtkWindow *parent_window;

        text = g_markup_printf_escaped (_("The document “%s” is locked and requires a password before it can be opened."),
                                        priv->filename);

	parent_window = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (password_view)));

	dialog = GTK_MESSAGE_DIALOG (gtk_message_dialog_new (parent_window,
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
	gtk_box_prepend (GTK_BOX (message_area), grid);

	gtk_widget_set_halign (grid, GTK_ALIGN_CENTER);

	label = gtk_label_new_with_mnemonic (_("_Password:"));
	g_object_set (G_OBJECT (label), "xalign", 0., "yalign", 0.5, NULL);

	password_entry = gtk_password_entry_new ();
	gtk_password_entry_set_show_peek_icon (GTK_PASSWORD_ENTRY (password_entry), TRUE);
	g_object_set (G_OBJECT (password_entry), "width-chars", 32, NULL);
	g_signal_connect (password_entry, "changed",
			  G_CALLBACK (ev_password_dialog_entry_changed_cb),
			  dialog);
	g_signal_connect (password_entry, "activate",
			  G_CALLBACK (ev_password_dialog_entry_activated_cb),
			  dialog);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

	gtk_grid_attach (GTK_GRID (grid), password_entry, 1, 0, 1, 1);
	gtk_widget_set_hexpand (password_entry, TRUE);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label),
				       password_entry);

	priv->password_entry = password_entry;

	if (ev_keyring_is_available ()) {
		GtkWidget  *choice;
		GtkWidget  *remember_box;
		GtkWidget  *group;

		remember_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
		gtk_box_prepend (GTK_BOX (message_area), remember_box);
		gtk_widget_set_halign (remember_box, GTK_ALIGN_CENTER);

		choice = gtk_check_button_new_with_mnemonic (_("Forget password _immediately"));
		gtk_check_button_set_active (GTK_CHECK_BUTTON (choice),
					      priv->password_save == G_PASSWORD_SAVE_NEVER);
		g_object_set_data (G_OBJECT (choice), "password-save",
				   GINT_TO_POINTER (G_PASSWORD_SAVE_NEVER));
		g_signal_connect (choice, "toggled",
				  G_CALLBACK (ev_password_dialog_remember_button_toggled),
				  password_view);
		gtk_box_append (GTK_BOX (remember_box), choice);

		group = choice;
		choice = gtk_check_button_new_with_mnemonic (_("Remember password until you _log out"));
		gtk_check_button_set_group (GTK_CHECK_BUTTON (choice), GTK_CHECK_BUTTON (group));
		gtk_check_button_set_active (GTK_CHECK_BUTTON (choice),
					      priv->password_save == G_PASSWORD_SAVE_FOR_SESSION);
		g_object_set_data (G_OBJECT (choice), "password-save",
				   GINT_TO_POINTER (G_PASSWORD_SAVE_FOR_SESSION));
		g_signal_connect (choice, "toggled",
				  G_CALLBACK (ev_password_dialog_remember_button_toggled),
				  password_view);
		gtk_box_append (GTK_BOX (remember_box), choice);

		group = choice;
		choice = gtk_check_button_new_with_mnemonic (_("Remember _forever"));
		gtk_check_button_set_group (GTK_CHECK_BUTTON (choice), GTK_CHECK_BUTTON (group));
		gtk_check_button_set_active (GTK_CHECK_BUTTON (choice),
					      priv->password_save == G_PASSWORD_SAVE_PERMANENTLY);
		g_object_set_data (G_OBJECT (choice), "password-save",
				   GINT_TO_POINTER (G_PASSWORD_SAVE_PERMANENTLY));
		g_signal_connect (choice, "toggled",
				  G_CALLBACK (ev_password_dialog_remember_button_toggled),
				  password_view);
		gtk_box_append (GTK_BOX (remember_box), choice);
	}

	g_signal_connect (dialog, "response",
			  G_CALLBACK (ev_password_dialog_got_response),
			  password_view);

	gtk_window_present (GTK_WINDOW (dialog));
}

const gchar *
ev_password_view_get_password (EvPasswordView *password_view)
{
	EvPasswordViewPrivate *priv = GET_PRIVATE (password_view);

	return priv->password;
}

GPasswordSave
ev_password_view_get_password_save_flags (EvPasswordView *password_view)
{
	EvPasswordViewPrivate *priv = GET_PRIVATE (password_view);

	return priv->password_save;
}

EvPasswordView *
ev_password_view_new (void)
{
	return g_object_new (EV_TYPE_PASSWORD_VIEW, NULL);
}
