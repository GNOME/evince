/* this file is part of evince, a gnome document viewer
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ev-password.h"
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#define EV_PASSWORD_DATA "ev-password-data"

static void
ev_password_set_bad_password_label (GtkWidget *password,
				    gchar     *message)
{
	GladeXML *xml;
	GtkWidget *label;
	gchar *markup;

	xml = g_object_get_data (G_OBJECT (password), EV_PASSWORD_DATA);
	g_assert (xml);

	label = glade_xml_get_widget (xml, "bad_password_label");
	markup = g_strdup_printf ("<span color=\"red\" size=\"smaller\">%s</span>",
				  message);
	gtk_label_set_markup (GTK_LABEL (label), markup);
	g_free (markup);
}

static void
ev_window_password_entry_changed_cb (GtkEditable *editable,
				     GtkWidget   *password)
{
	const char *text;
	GtkWidget *button;
	GladeXML *xml;

	xml = g_object_get_data (G_OBJECT (password), EV_PASSWORD_DATA);
	g_assert (xml);

	text = gtk_entry_get_text (GTK_ENTRY (editable));
	button = glade_xml_get_widget (xml, "ok_button");

	if (text == NULL || *text == '\0')
		gtk_widget_set_sensitive (button, FALSE);
	else
		gtk_widget_set_sensitive (button, TRUE);

	ev_password_set_bad_password_label (password, " ");
}

GtkWidget *
ev_password_dialog_new (GtkWidget  *toplevel,
			const char *uri)
{
	const char *glade_file = DATADIR "/evince-password.glade";
	GladeXML *xml;
	GtkWidget *dialog, *label;
	GtkWidget *entry;
	char *format;
	char *markup;

	xml = glade_xml_new (glade_file, NULL, NULL);
	if (xml == NULL) {
		dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
						 _("Unable to find glade file"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  _("The glade file, %s, cannot be found.  Please check that your installation is complete."),
							  glade_file);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return NULL;
	}
	
	dialog = glade_xml_get_widget (xml, "password_dialog");
	label = glade_xml_get_widget (xml, "password_label");
	entry = glade_xml_get_widget (xml, "password_entry");

	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
	g_signal_connect (entry, "changed", G_CALLBACK (ev_window_password_entry_changed_cb), dialog);
	format = g_strdup_printf ("<span size=\"larger\" weight=\"bold\">%s</span>\n\n%s",
				  _("Password required"),
				  _("The document <i>%s</i> is locked and requires a password before it can be opened."));
	markup = g_markup_printf_escaped (format, uri);
	gtk_label_set_markup (GTK_LABEL (label), markup);
	g_free (format);
	g_free (markup);

	g_object_set_data_full (G_OBJECT (dialog), EV_PASSWORD_DATA, xml, g_object_unref);
	ev_password_set_bad_password_label (dialog, " ");

	return dialog;
}

char *
ev_password_dialog_get_password (GtkWidget *password)
{
	GladeXML *xml;
	GtkWidget *entry;

	xml = g_object_get_data (G_OBJECT (password), EV_PASSWORD_DATA);
	g_assert (xml);

	entry = glade_xml_get_widget (xml, "password_entry");

	return g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
}

void
ev_password_dialog_set_bad_pass (GtkWidget *password)
{
	GladeXML *xml;
	GtkWidget *entry;

	xml = g_object_get_data (G_OBJECT (password), EV_PASSWORD_DATA);
	g_assert (xml);

	entry = glade_xml_get_widget (xml, "password_entry");
	gtk_entry_set_text (GTK_ENTRY (entry), "");
	ev_password_set_bad_password_label (password, _("Incorrect password"));
}
