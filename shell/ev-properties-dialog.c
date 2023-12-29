/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "ev-document-fonts.h"
#include "ev-properties-dialog.h"
#include "ev-properties-fonts.h"
#include "ev-properties-view.h"
#include "ev-properties-license.h"

struct _EvPropertiesDialog {
	GtkDialog base_instance;

	EvDocument *document;
	GtkWidget *notebook;
	GtkWidget *general_page;
	GtkWidget *fonts_page;
	GtkWidget *license_page;
};

struct _EvPropertiesDialogClass {
	GtkDialogClass base_class;
};

G_DEFINE_TYPE (EvPropertiesDialog, ev_properties_dialog, GTK_TYPE_DIALOG)

static void
ev_properties_dialog_class_init (EvPropertiesDialogClass *properties_class)
{
}

static void
ev_properties_dialog_init (EvPropertiesDialog *properties)
{
	GtkBox *content_area;

	content_area = GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (properties)));

	gtk_window_set_title (GTK_WINDOW (properties), _("Properties"));
	gtk_window_set_destroy_with_parent (GTK_WINDOW (properties), TRUE);

	properties->notebook = gtk_notebook_new ();
	gtk_notebook_set_show_border (GTK_NOTEBOOK (properties->notebook), FALSE);
	gtk_box_prepend (content_area, properties->notebook);
}

void
ev_properties_dialog_set_document (EvPropertiesDialog *properties,
			           EvDocument         *document)
{
	GtkWidget *label;
	const EvDocumentInfo *info;

	properties->document = document;

	info = ev_document_get_info (document);

	if (properties->general_page == NULL) {
		label = gtk_label_new (_("General"));
		properties->general_page = ev_properties_view_new (document);
		gtk_notebook_append_page (GTK_NOTEBOOK (properties->notebook),
					  properties->general_page, label);
	}
	ev_properties_view_set_info (EV_PROPERTIES_VIEW (properties->general_page), info);

	if (EV_IS_DOCUMENT_FONTS (document)) {
		if (properties->fonts_page == NULL) {
			label = gtk_label_new (_("Fonts"));
			properties->fonts_page = ev_properties_fonts_new ();
			gtk_notebook_append_page (GTK_NOTEBOOK (properties->notebook),
						  properties->fonts_page, label);
		}

		ev_properties_fonts_set_document
			(EV_PROPERTIES_FONTS (properties->fonts_page), document);
	}

	if (info->fields_mask & EV_DOCUMENT_INFO_LICENSE && info->license) {
		if (properties->license_page == NULL) {
			label = gtk_label_new (_("Document License"));
			properties->license_page = ev_properties_license_new ();
			gtk_notebook_append_page (GTK_NOTEBOOK (properties->notebook),
						  properties->license_page, label);
		}

		ev_properties_license_set_license
			(EV_PROPERTIES_LICENSE (properties->license_page), info->license);
	}
}

GtkWidget *
ev_properties_dialog_new (void)
{
	EvPropertiesDialog *properties;

	properties = g_object_new (EV_TYPE_PROPERTIES_DIALOG,
				   "use-header-bar", TRUE, NULL);

	return GTK_WIDGET (properties);
}
