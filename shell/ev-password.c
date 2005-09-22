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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gnome-keyring.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include "ev-password.h"

enum {
	PROP_0,
	PROP_URI,
};

struct _EvPasswordDialogPrivate {
	
	gchar *uri;
    
	GtkWidget *bad_label;
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *check_default;
	GtkWidget *check_session;
};

#define EV_PASSWORD_DIALOG_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_PASSWORD_DIALOG, EvPasswordDialogPrivate));

G_DEFINE_TYPE (EvPasswordDialog, ev_password_dialog, GTK_TYPE_DIALOG)


static void ev_password_dialog_entry_changed_cb   (GtkEditable *editable,
				                   EvPasswordDialog *dialog);
static void ev_password_dialog_entry_activated_cb (GtkEntry *entry,
						   EvPasswordDialog *dialog);
static void ev_password_set_bad_password_label    (EvPasswordDialog *dialog,
				                   gchar     *message);
static void ev_password_search_in_keyring         (EvPasswordDialog *dialog, 
						   const gchar *uri);

static void
ev_password_dialog_set_property (GObject      *object,
			         guint         prop_id,
			         const GValue *value,
			         GParamSpec   *pspec)
{
	EvPasswordDialog *dialog = EV_PASSWORD_DIALOG (object);
	char *format;
	char *markup;
	char *base_name;
	char *file_name;

	switch (prop_id)
	{
	case PROP_URI:
		dialog->priv->uri = g_strdup (g_value_get_string (value));

		file_name = gnome_vfs_format_uri_for_display (dialog->priv->uri);
		base_name = g_path_get_basename (file_name);
		format = g_strdup_printf ("<span size=\"larger\" weight=\"bold\">%s</span>\n\n%s",
					  _("Password required"),
					  _("The document <i>%s</i> is locked and requires a password before it can be opened."));
		markup = g_markup_printf_escaped (format, base_name);

		gtk_label_set_markup (GTK_LABEL (dialog->priv->label), markup);

		g_free (base_name);
		g_free (file_name);
		g_free (format);
		g_free (markup);

		ev_password_search_in_keyring (dialog, dialog->priv->uri);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ev_password_dialog_finalize (GObject *object)
{
	EvPasswordDialog *dialog = EV_PASSWORD_DIALOG (object);

	if (dialog->priv->uri) {
		g_free (dialog->priv->uri);
		dialog->priv->uri = NULL;
	}

	G_OBJECT_CLASS (ev_password_dialog_parent_class)->finalize (object);
}


static void
ev_password_dialog_class_init (EvPasswordDialogClass *class)
{
	GObjectClass *g_object_class;
	GtkWidgetClass *widget_class;

	g_object_class = G_OBJECT_CLASS (class);
	widget_class = GTK_WIDGET_CLASS (class);

	g_type_class_add_private (g_object_class, sizeof (EvPasswordDialogPrivate));

	g_object_class->set_property = ev_password_dialog_set_property;
	g_object_class->finalize = ev_password_dialog_finalize;
	
	g_object_class_install_property (g_object_class,
					 PROP_URI,
					 g_param_spec_string ("uri",
							      "Document URI",
							      "Encrypted document URI",
							      NULL,
							      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}


static void
ev_password_dialog_init (EvPasswordDialog *dialog)
{
	GtkWidget *hbox;
	const char *glade_file = DATADIR "/evince-password.glade";
	GladeXML *xml;

	dialog->priv = EV_PASSWORD_DIALOG_GET_PRIVATE (dialog);
		
	gtk_window_set_title (GTK_WINDOW (dialog), _("Enter password"));
	gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 12);

	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL,
			       GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_OK,
			       GTK_RESPONSE_OK);

	xml = glade_xml_new (glade_file, "hbox-contents", NULL);
	g_assert (xml);
	
	hbox = glade_xml_get_widget (xml, "hbox-contents");
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), hbox);

	dialog->priv->bad_label = glade_xml_get_widget (xml, "bad_password_label");
	dialog->priv->check_default = glade_xml_get_widget (xml, "savesession_checkbutton");
	dialog->priv->check_session = glade_xml_get_widget (xml, "savekeyring_checkbutton");
	dialog->priv->entry = glade_xml_get_widget (xml, "password_entry");
	dialog->priv->label = glade_xml_get_widget (xml, "password_label");

	g_signal_connect (dialog->priv->entry, "changed", G_CALLBACK (ev_password_dialog_entry_changed_cb), dialog);
	g_signal_connect (dialog->priv->entry, "activate", G_CALLBACK (ev_password_dialog_entry_activated_cb), dialog);

	ev_password_set_bad_password_label (dialog, " ");
	
	if (!gnome_keyring_is_available ()) {
		gtk_widget_hide (dialog->priv->check_default);
		gtk_widget_hide (dialog->priv->check_session);
	}

	g_object_unref (xml);
}

static void
ev_password_set_bad_password_label (EvPasswordDialog *dialog,
				    gchar     *message)
{
	gchar *markup;

	markup = g_strdup_printf ("<span color=\"red\" size=\"smaller\">%s</span>",
				  message);
	gtk_label_set_markup (GTK_LABEL (dialog->priv->bad_label), markup);
	g_free (markup);
}

static void
ev_password_dialog_entry_changed_cb (GtkEditable *editable,
				     EvPasswordDialog *dialog)
{
	const char *text;

	text = gtk_entry_get_text (GTK_ENTRY (editable));

	if (text == NULL || *text == '\0')
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), 
						   GTK_RESPONSE_OK, FALSE);
	else
		gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), 
						   GTK_RESPONSE_OK, TRUE);

	ev_password_set_bad_password_label (dialog, " ");
}

static void
ev_password_dialog_entry_activated_cb (GtkEntry *entry,
				       EvPasswordDialog *dialog)
{
	gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}

static void
ev_password_item_created_callback (GnomeKeyringResult result,
				   guint32            val,
				   gpointer           data)
{
	/* Nothing yet */
	return;
}				   

void
ev_password_dialog_save_password (EvPasswordDialog *dialog)
{
	GnomeKeyringAttributeList *attributes;
	GnomeKeyringAttribute attribute;
	gchar *name;
	gchar *unescaped_uri;

	attributes = gnome_keyring_attribute_list_new ();

	attribute.name = g_strdup ("type");
	attribute.type = GNOME_KEYRING_ATTRIBUTE_TYPE_STRING;
	attribute.value.string = g_strdup ("document_password");
	g_array_append_val (attributes, attribute);
 
	attribute.name = g_strdup ("uri");
	attribute.type = GNOME_KEYRING_ATTRIBUTE_TYPE_STRING;
	attribute.value.string = g_strdup (dialog->priv->uri);
	g_array_append_val (attributes, attribute);
	
	unescaped_uri = gnome_vfs_unescape_string_for_display (dialog->priv->uri);
	name = g_strdup_printf (_("Password for document %s"), unescaped_uri);	

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->check_default))) {
		gnome_keyring_item_create (NULL,
					   GNOME_KEYRING_ITEM_GENERIC_SECRET,
					   name,
					   attributes,
					   ev_password_dialog_get_password (dialog),
				    	   TRUE, ev_password_item_created_callback, 
					   NULL, NULL);
	}

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->check_session))) {
		gnome_keyring_item_create ("session",
					   GNOME_KEYRING_ITEM_GENERIC_SECRET,
					   name,
					   attributes,
    				           ev_password_dialog_get_password (dialog),
				    	   TRUE, ev_password_item_created_callback, 
					   NULL, NULL);
	}
	
	gnome_keyring_attribute_list_free (attributes);
	g_free (name);
	g_free (unescaped_uri);
	
	return;
}

static void 
ev_password_keyring_found_cb (GnomeKeyringResult result,
			      GList             *list,
			      gpointer           data)

{
	GnomeKeyringFound *found;
	EvPasswordDialog *dialog = EV_PASSWORD_DIALOG (data);

	if (result != GNOME_KEYRING_RESULT_OK || list == NULL) 
		    return;

	found = list->data;
	gtk_entry_set_text (GTK_ENTRY (dialog->priv->entry), found->secret);
}			  

static void
ev_password_search_in_keyring (EvPasswordDialog *dialog, const gchar *uri)
{
	GnomeKeyringAttributeList *attributes;
	GnomeKeyringAttribute attribute;
	
	attributes = gnome_keyring_attribute_list_new ();

	attribute.name = g_strdup ("type");
	attribute.type = GNOME_KEYRING_ATTRIBUTE_TYPE_STRING;
	attribute.value.string = g_strdup ("document_password");
	g_array_append_val (attributes, attribute);
 
	attribute.name = g_strdup ("uri");
	attribute.type = GNOME_KEYRING_ATTRIBUTE_TYPE_STRING;
	attribute.value.string = g_strdup (uri);
	g_array_append_val (attributes, attribute);
         
	gnome_keyring_find_items  (GNOME_KEYRING_ITEM_GENERIC_SECRET,
				   attributes,
				   ev_password_keyring_found_cb,
				   g_object_ref (dialog),
				   g_object_unref);
	gnome_keyring_attribute_list_free (attributes);
	return;
}

char *
ev_password_dialog_get_password (EvPasswordDialog *dialog)
{
	return g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->priv->entry)));
}

void
ev_password_dialog_set_bad_pass (EvPasswordDialog *dialog)
{
	gtk_entry_set_text (GTK_ENTRY (dialog->priv->entry), "");
	ev_password_set_bad_password_label (dialog, _("Incorrect password"));
}


