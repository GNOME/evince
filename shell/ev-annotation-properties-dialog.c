/* ev-annotation-properties-dialog.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2010 Carlos Garcia Campos  <carlosgc@gnome.org>
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

#include <config.h>

#include <glib/gi18n.h>

#include "ev-annotation-properties-dialog.h"

enum {
	PROP_0,
	PROP_ANNOT_TYPE
};

struct _EvAnnotationPropertiesDialog {
	GtkDialog        base_instance;

	EvAnnotationType annot_type;
	EvAnnotation    *annot;

	GtkWidget       *grid;

	GtkWidget       *author;
	GtkWidget       *color;
	GtkWidget       *opacity;
	GtkWidget       *popup_state;

	/* Text Annotations */
	GtkWidget       *icon;

        /* Text Markup Annotations */
        GtkWidget       *text_markup_type;
};

struct _EvAnnotationPropertiesDialogClass {
	GtkDialogClass base_class;
};

G_DEFINE_TYPE (EvAnnotationPropertiesDialog, ev_annotation_properties_dialog, GTK_TYPE_DIALOG)

static void
ev_annotation_properties_dialog_dispose (GObject *object)
{
	EvAnnotationPropertiesDialog *dialog = EV_ANNOTATION_PROPERTIES_DIALOG (object);

	g_clear_object (&dialog->annot);

	G_OBJECT_CLASS (ev_annotation_properties_dialog_parent_class)->dispose (object);
}

static void
ev_annotation_properties_dialog_set_property (GObject      *object,
					      guint         prop_id,
					      const GValue *value,
					      GParamSpec   *pspec)
{
	EvAnnotationPropertiesDialog *dialog = EV_ANNOTATION_PROPERTIES_DIALOG (object);

	switch (prop_id) {
	case PROP_ANNOT_TYPE:
		dialog->annot_type = g_value_get_enum (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
ev_annotation_properties_dialog_constructed (GObject *object)
{
	EvAnnotationPropertiesDialog *dialog = EV_ANNOTATION_PROPERTIES_DIALOG (object);
	GtkWidget *grid = dialog->grid;
	GtkWidget *label;

	switch (dialog->annot_type) {
	case EV_ANNOTATION_TYPE_TEXT:
		label = gtk_label_new (_("Icon:"));
		g_object_set (G_OBJECT (label), "xalign", 0., "yalign", 0.5, NULL);
		gtk_grid_attach (GTK_GRID (grid), label, 0, 4, 1, 1);

		dialog->icon = gtk_drop_down_new_from_strings ((const char *[]) {
								_("Note"),
								_("Comment"),
								_("Key"),
								_("Help"),
								_("New Paragraph"),
								_("Paragraph"),
								_("Insert"),
								_("Cross"),
								_("Circle"),
								_("Unknown"),
								NULL });
		gtk_drop_down_set_selected (GTK_DROP_DOWN (dialog->icon), 0);
		gtk_grid_attach (GTK_GRID (grid), dialog->icon, 1, 4, 1, 1);
                gtk_widget_set_hexpand (dialog->icon, TRUE);

		break;
	case EV_ANNOTATION_TYPE_ATTACHMENT:
		/* TODO */
                break;
        case EV_ANNOTATION_TYPE_TEXT_MARKUP:
                label = gtk_label_new (_("Markup type:"));
                g_object_set (G_OBJECT (label), "xalign", 0., "yalign", 0.5, NULL);
                gtk_grid_attach (GTK_GRID (grid), label, 0, 5, 1, 1);

		dialog->text_markup_type = gtk_drop_down_new_from_strings ((const char *[]) {
								_("Highlight"),
								_("Strike out"),
								_("Underline"),
								_("Squiggly"),
								NULL });
		gtk_drop_down_set_selected (GTK_DROP_DOWN (dialog->text_markup_type), 0);
                gtk_grid_attach (GTK_GRID (grid), dialog->text_markup_type, 1, 5, 1, 1);
                gtk_widget_set_hexpand (dialog->text_markup_type, TRUE);
                break;
	default:
		break;
	}
	G_OBJECT_CLASS (ev_annotation_properties_dialog_parent_class)->constructed (object);
}

static void
ev_annotation_properties_dialog_init (EvAnnotationPropertiesDialog *annot_dialog)
{
	GtkDialog *dialog = GTK_DIALOG (annot_dialog);
	GtkWidget *content_area;
	GtkWidget *label;
	GtkWidget *grid;
        const GdkRGBA yellow = { 1., 1., 0., 1. };

	gtk_window_set_title (GTK_WINDOW (annot_dialog), _("Annotation Properties"));
	gtk_window_set_destroy_with_parent (GTK_WINDOW (annot_dialog), TRUE);
	gtk_dialog_add_buttons (dialog,
				_("_Close"), GTK_RESPONSE_CANCEL,
				_("_Apply"), GTK_RESPONSE_APPLY,
				NULL);
	gtk_dialog_set_default_response (dialog, GTK_RESPONSE_APPLY);

	content_area = gtk_dialog_get_content_area (dialog);
	gtk_box_set_spacing (GTK_BOX (content_area), 12);

	gtk_widget_set_margin_start (content_area, 6);
	gtk_widget_set_margin_end (content_area, 6);
	gtk_widget_set_margin_top (content_area, 6);
	gtk_widget_set_margin_bottom (content_area, 6);

	grid = gtk_grid_new ();
	annot_dialog->grid = grid;
	gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
	gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
	gtk_box_prepend (GTK_BOX (content_area), grid);

	label = gtk_label_new (_("Author:"));
	g_object_set (G_OBJECT (label), "xalign", 0., "yalign", 0.5, NULL);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

	annot_dialog->author = gtk_entry_new ();
	gtk_editable_set_text (GTK_EDITABLE (annot_dialog->author), g_get_real_name ());
	gtk_grid_attach (GTK_GRID (grid), annot_dialog->author, 1, 0, 1, 1);
        gtk_widget_set_hexpand (annot_dialog->author, TRUE);

	label = gtk_label_new (_("Color:"));
	g_object_set (G_OBJECT (label), "xalign", 0., "yalign", 0.5, NULL);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);

	annot_dialog->color = gtk_color_dialog_button_new (gtk_color_dialog_new ());
	gtk_color_dialog_button_set_rgba (GTK_COLOR_DIALOG_BUTTON (annot_dialog->color), &yellow);

	gtk_grid_attach (GTK_GRID (grid), annot_dialog->color, 1, 1, 1, 1);
        gtk_widget_set_hexpand (annot_dialog->color, TRUE);

	label = gtk_label_new (_("Opacity:"));
	g_object_set (G_OBJECT (label), "xalign", 0., "yalign", 0.5, NULL);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);

	annot_dialog->opacity = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                                          0, 100, 5);
	gtk_range_set_value (GTK_RANGE (annot_dialog->opacity), 100);
	gtk_grid_attach (GTK_GRID (grid), annot_dialog->opacity, 1, 2, 1, 1);
        gtk_widget_set_hexpand (annot_dialog->opacity, TRUE);

	label = gtk_label_new (_("Initial window state:"));
	g_object_set (G_OBJECT (label), "xalign", 0., "yalign", 0.5, NULL);
	gtk_grid_attach (GTK_GRID (grid), label, 0, 3, 1, 1);

	annot_dialog->popup_state = gtk_drop_down_new_from_strings ((const char *[]) {
									_("Open"),
									_("Close"),
									NULL });
	gtk_drop_down_set_selected (GTK_DROP_DOWN (annot_dialog->popup_state), 1);
	gtk_grid_attach (GTK_GRID (grid), annot_dialog->popup_state, 1, 3, 1, 1);
        gtk_widget_set_hexpand (annot_dialog->popup_state, TRUE);
}

static void
ev_annotation_properties_dialog_class_init (EvAnnotationPropertiesDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = ev_annotation_properties_dialog_dispose;
	object_class->constructed = ev_annotation_properties_dialog_constructed;
	object_class->set_property = ev_annotation_properties_dialog_set_property;

	g_object_class_install_property (object_class,
					 PROP_ANNOT_TYPE,
					 g_param_spec_enum ("annot-type",
							    "AnnotType",
							    "The type of annotation",
							    EV_TYPE_ANNOTATION_TYPE,
							    EV_ANNOTATION_TYPE_TEXT,
							    G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                                                            G_PARAM_STATIC_STRINGS));
}

GtkWidget *
ev_annotation_properties_dialog_new (EvAnnotationType annot_type)
{
	return GTK_WIDGET (g_object_new (EV_TYPE_ANNOTATION_PROPERTIES_DIALOG,
					 "annot-type", annot_type,
					 "use-header-bar", TRUE,
					 NULL));
}

GtkWidget *
ev_annotation_properties_dialog_new_with_annotation (EvAnnotation *annot)
{
	EvAnnotationPropertiesDialog *dialog;
	const gchar                  *label;
	gdouble                       opacity;
	gboolean                      is_open;
	GdkRGBA                       rgba;

	dialog = (EvAnnotationPropertiesDialog *)ev_annotation_properties_dialog_new (ev_annotation_get_annotation_type (annot));
	dialog->annot = g_object_ref (annot);

	label = ev_annotation_markup_get_label (EV_ANNOTATION_MARKUP (annot));
	if (label)
		gtk_editable_set_text (GTK_EDITABLE (dialog->author), label);

	ev_annotation_get_rgba (annot, &rgba);
	gtk_color_dialog_button_set_rgba (GTK_COLOR_DIALOG_BUTTON (dialog->color), &rgba);

	opacity = ev_annotation_markup_get_opacity (EV_ANNOTATION_MARKUP (annot));
	gtk_range_set_value (GTK_RANGE (dialog->opacity), opacity * 100);

	is_open = ev_annotation_markup_get_popup_is_open (EV_ANNOTATION_MARKUP (annot));
	gtk_drop_down_set_selected (GTK_DROP_DOWN (dialog->popup_state), is_open ? 0 : 1);

	if (EV_IS_ANNOTATION_TEXT (annot)) {
		EvAnnotationText *annot_text = EV_ANNOTATION_TEXT (annot);

		gtk_drop_down_set_selected (GTK_DROP_DOWN (dialog->icon),
					    ev_annotation_text_get_icon (annot_text));
	} else if (EV_IS_ANNOTATION_TEXT_MARKUP (annot)) {
                EvAnnotationTextMarkup *annot_markup = EV_ANNOTATION_TEXT_MARKUP (annot);

		gtk_drop_down_set_selected (GTK_DROP_DOWN (dialog->text_markup_type),
					    ev_annotation_text_markup_get_markup_type (annot_markup));
        }

	return GTK_WIDGET (dialog);
}

const gchar *
ev_annotation_properties_dialog_get_author (EvAnnotationPropertiesDialog *dialog)
{
	return gtk_editable_get_text (GTK_EDITABLE (dialog->author));
}

void
ev_annotation_properties_dialog_get_rgba (EvAnnotationPropertiesDialog *dialog,
					  GdkRGBA                      *rgba)
{
	*rgba = *gtk_color_dialog_button_get_rgba (GTK_COLOR_DIALOG_BUTTON (dialog->color));
}

gdouble
ev_annotation_properties_dialog_get_opacity (EvAnnotationPropertiesDialog *dialog)
{
	return gtk_range_get_value (GTK_RANGE (dialog->opacity)) / 100;
}

gboolean
ev_annotation_properties_dialog_get_popup_is_open (EvAnnotationPropertiesDialog *dialog)
{
	return gtk_drop_down_get_selected (GTK_DROP_DOWN (dialog->popup_state)) == 0;
}

EvAnnotationTextIcon
ev_annotation_properties_dialog_get_text_icon (EvAnnotationPropertiesDialog *dialog)
{
	return gtk_drop_down_get_selected (GTK_DROP_DOWN (dialog->icon));
}

EvAnnotationTextMarkupType
ev_annotation_properties_dialog_get_text_markup_type (EvAnnotationPropertiesDialog *dialog)
{
	return gtk_drop_down_get_selected (GTK_DROP_DOWN (dialog->text_markup_type));
}
