/* ev-message-area.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2007 Carlos Garcia Campos <carlosgc@gnome.org>
 * Copyright (C) 2018 Germán Poo-Caamaño <gpoo@gnome.org>
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

#include "ev-message-area.h"

typedef struct {
	GtkWidget *info_bar;
	GtkWidget *main_box;
	GtkWidget *image;
	GtkWidget *label;
	GtkWidget *secondary_label;

	guint      message_type : 3;
} EvMessageAreaPrivate;

enum {
	PROP_0,
	PROP_TEXT,
	PROP_SECONDARY_TEXT,
	PROP_IMAGE
};

static void ev_message_area_set_property (GObject      *object,
					  guint         prop_id,
					  const GValue *value,
					  GParamSpec   *pspec);
static void ev_message_area_get_property (GObject      *object,
					  guint         prop_id,
					  GValue       *value,
					  GParamSpec   *pspec);
static void ev_message_area_buildable_iface_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (EvMessageArea, ev_message_area, ADW_TYPE_BIN,
                         G_ADD_PRIVATE (EvMessageArea)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                ev_message_area_buildable_iface_init))

#define GET_PRIVATE(o) ev_message_area_get_instance_private (o);

static void
ev_message_area_class_init (EvMessageAreaClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

	gobject_class->set_property = ev_message_area_set_property;
	gobject_class->get_property = ev_message_area_get_property;

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/evince/ui/message-area.ui");
	gtk_widget_class_bind_template_child_private (widget_class, EvMessageArea, info_bar);
	gtk_widget_class_bind_template_child_private (widget_class, EvMessageArea, main_box);
	gtk_widget_class_bind_template_child_private (widget_class, EvMessageArea, image);
	gtk_widget_class_bind_template_child_private (widget_class, EvMessageArea, label);
	gtk_widget_class_bind_template_child_private (widget_class, EvMessageArea, secondary_label);

	gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_ALERT);

	g_object_class_install_property (gobject_class,
					 PROP_TEXT,
					 g_param_spec_string ("text",
							      "Text",
							      "The primary text of the message dialog",
							      NULL,
							      G_PARAM_READWRITE |
                                                              G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class,
					 PROP_SECONDARY_TEXT,
					 g_param_spec_string ("secondary-text",
							      "Secondary Text",
							      "The secondary text of the message dialog",
							      NULL,
							      G_PARAM_READWRITE |
                                                              G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class,
					 PROP_IMAGE,
					 g_param_spec_object ("image",
							      "Image",
							      "The image",
							      GTK_TYPE_WIDGET,
							      G_PARAM_READWRITE |
                                                              G_PARAM_STATIC_STRINGS));
}

static void
ev_message_area_init (EvMessageArea *area)
{
	gtk_widget_init_template (GTK_WIDGET (area));
}

static void
ev_message_area_set_image_for_type (EvMessageArea *area,
				    GtkMessageType type)
{
	const gchar *icon_name = NULL;
	EvMessageAreaPrivate *priv = GET_PRIVATE (area);

	switch (type) {
	case GTK_MESSAGE_INFO:
		icon_name = "dialog-information-symbolic";
		break;
	case GTK_MESSAGE_QUESTION:
		icon_name = "dialog-question-symbolic";
		break;
	case GTK_MESSAGE_WARNING:
		icon_name = "dialog-warning-symbolic";
		break;
	case GTK_MESSAGE_ERROR:
		icon_name = "dialog-error-symbolic";
		break;
	case GTK_MESSAGE_OTHER:
		break;
	default:
		g_warning ("Unknown GtkMessageType %u", type);
		break;
	}

	if (icon_name)
		gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
					      icon_name);

	if (icon_name)
		gtk_accessible_update_property (GTK_ACCESSIBLE (area),
			GTK_ACCESSIBLE_PROPERTY_LABEL, icon_name, -1);
}

static void
ev_message_area_set_property (GObject      *object,
			      guint         prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
	EvMessageArea *area = EV_MESSAGE_AREA (object);

	switch (prop_id) {
	case PROP_TEXT:
		ev_message_area_set_text (area, g_value_get_string (value));
		break;
	case PROP_SECONDARY_TEXT:
		ev_message_area_set_secondary_text (area, g_value_get_string (value));
		break;
	case PROP_IMAGE:
		ev_message_area_set_image (area, (GtkWidget *)g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
ev_message_area_get_property (GObject     *object,
			      guint        prop_id,
			      GValue      *value,
			      GParamSpec  *pspec)
{
	EvMessageArea *area = EV_MESSAGE_AREA (object);
	EvMessageAreaPrivate *priv = GET_PRIVATE (area);

	switch (prop_id) {
	case PROP_TEXT:
		g_value_set_string (value, gtk_label_get_label (GTK_LABEL (priv->label)));
		break;
	case PROP_SECONDARY_TEXT:
		g_value_set_string (value, gtk_label_get_label (GTK_LABEL (priv->secondary_label)));
		break;
	case PROP_IMAGE:
		g_value_set_object (value, priv->image);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static GtkBuildableIface *parent_buildable_iface;

static GObject *
ev_message_area_buildable_get_internal_child (GtkBuildable *buildable,
                             GtkBuilder   *builder,
                             const char   *childname)
{
        EvMessageArea *area = EV_MESSAGE_AREA (buildable);

        if (g_strcmp0 (childname, "main_box") == 0)
                return G_OBJECT (_ev_message_area_get_main_box (area));

        return parent_buildable_iface->get_internal_child (buildable, builder, childname);
}

static void
ev_message_area_buildable_iface_init (GtkBuildableIface *iface)
{
        parent_buildable_iface = g_type_interface_peek_parent (iface);

        iface->get_internal_child = ev_message_area_buildable_get_internal_child;
}

void
_ev_message_area_add_buttons_valist (EvMessageArea *area,
				     const gchar   *first_button_text,
				     va_list        args)
{
	EvMessageAreaPrivate *priv = GET_PRIVATE (area);
	const gchar* text;
	gint response_id;

	if (first_button_text == NULL)
		return;

	text = first_button_text;
	response_id = va_arg (args, gint);

	while (text != NULL) {
		gtk_info_bar_add_button (GTK_INFO_BAR (priv->info_bar), text, response_id);

		text = va_arg (args, gchar*);
		if (text == NULL)
			break;

		response_id = va_arg (args, int);
	}
}

GtkWidget *
_ev_message_area_get_main_box (EvMessageArea *area)
{
	EvMessageAreaPrivate *priv;

	priv = GET_PRIVATE (area);
	return priv->main_box;
}

GtkWidget *
ev_message_area_get_info_bar (EvMessageArea *area)
{
	EvMessageAreaPrivate *priv = GET_PRIVATE (area);

	return priv->info_bar;
}

GtkWidget *
ev_message_area_new (GtkMessageType type,
		     const gchar   *text,
		     const gchar   *first_button_text,
		     ...)
{
	GtkWidget *widget = g_object_new (EV_TYPE_MESSAGE_AREA, "text", text, NULL);
	GtkWidget *info_bar = ev_message_area_get_info_bar (EV_MESSAGE_AREA (widget));
	gtk_info_bar_set_message_type (GTK_INFO_BAR (info_bar), type);
	ev_message_area_set_image_for_type (EV_MESSAGE_AREA (widget), type);
	if (first_button_text) {
		va_list args;

		va_start (args, first_button_text);
		_ev_message_area_add_buttons_valist (EV_MESSAGE_AREA (widget),
						     first_button_text, args);
		va_end (args);
	}

	return widget;
}

void
ev_message_area_set_image (EvMessageArea *area,
			   GtkWidget     *image)
{
	GtkWidget *parent;
	EvMessageAreaPrivate *priv;

	g_return_if_fail (EV_IS_MESSAGE_AREA (area));

	priv = GET_PRIVATE (area);
	priv->message_type = GTK_MESSAGE_OTHER;

	parent = gtk_widget_get_parent (priv->image);
	g_assert (GTK_IS_BOX (parent));
	gtk_box_remove (GTK_BOX (parent), image);
	gtk_box_prepend (GTK_BOX (parent), priv->image);
	gtk_box_reorder_child_after (GTK_BOX (parent), image, NULL);

	priv->image = image;

	g_object_notify (G_OBJECT (area), "image");
}

void
ev_message_area_set_image_from_icon_name (EvMessageArea *area,
					  const gchar   *icon_name)
{
	EvMessageAreaPrivate *priv;

	g_return_if_fail (EV_IS_MESSAGE_AREA (area));
	g_return_if_fail (icon_name != NULL);

	priv = GET_PRIVATE (area);

	gtk_image_set_from_icon_name (GTK_IMAGE (priv->image),
				      icon_name);
}

void
ev_message_area_set_text (EvMessageArea *area,
			  const gchar   *str)
{
	EvMessageAreaPrivate *priv;

	g_return_if_fail (EV_IS_MESSAGE_AREA (area));

	priv = GET_PRIVATE (area);

	if (str) {
		gchar *msg, *escaped;

		escaped = g_markup_escape_text (str, -1);
		msg = g_strdup_printf ("<b>%s</b>", escaped);
		gtk_label_set_markup (GTK_LABEL (priv->label), msg);
		g_free (msg);
		g_free (escaped);
	} else {
		gtk_label_set_markup (GTK_LABEL (priv->label), NULL);
	}

	g_object_notify (G_OBJECT (area), "text");
}

void
ev_message_area_set_secondary_text (EvMessageArea *area,
				    const gchar   *str)
{
	EvMessageAreaPrivate *priv;

	g_return_if_fail (EV_IS_MESSAGE_AREA (area));

	priv = GET_PRIVATE (area);

	if (str) {
		gchar *msg;

		msg = g_strdup_printf ("<small>%s</small>", str);
		gtk_label_set_markup (GTK_LABEL (priv->secondary_label), msg);
		g_free (msg);
	} else {
		gtk_label_set_markup (GTK_LABEL (priv->secondary_label), NULL);
	}

	gtk_widget_set_visible (priv->secondary_label, str != NULL);

	g_object_notify (G_OBJECT (area), "secondary-text");
}
