/* ev-progress-message-area.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2008 Carlos Garcia Campos <carlosgc@gnome.org>
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

#include <gtk/gtk.h>

#include "ev-progress-message-area.h"

typedef struct {
	GtkWidget *label;
	GtkWidget *progress_bar;
} EvProgressMessageAreaPrivate;

enum {
	PROP_0,
	PROP_STATUS,
	PROP_FRACTION
};

static void ev_progress_message_area_set_property (GObject      *object,
						   guint         prop_id,
						   const GValue *value,
						   GParamSpec   *pspec);
static void ev_progress_message_area_get_property (GObject      *object,
						   guint         prop_id,
						   GValue       *value,
						   GParamSpec   *pspec);

G_DEFINE_TYPE_WITH_PRIVATE (EvProgressMessageArea, ev_progress_message_area,
			    EV_TYPE_MESSAGE_AREA)

static void
ev_progress_message_area_class_init (EvProgressMessageAreaClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

	gobject_class->set_property = ev_progress_message_area_set_property;
	gobject_class->get_property = ev_progress_message_area_get_property;

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/evince/ui/progress-message-area.ui");
	gtk_widget_class_bind_template_child_private (widget_class, EvProgressMessageArea, label);
	gtk_widget_class_bind_template_child_private (widget_class, EvProgressMessageArea, progress_bar);

	g_object_class_install_property (gobject_class,
					 PROP_STATUS,
					 g_param_spec_string ("status",
							      "Status",
							      "The status text of the progress area",
							      NULL,
							      G_PARAM_READWRITE |
                                                              G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class,
					 PROP_FRACTION,
					 g_param_spec_double ("fraction",
							      "Fraction",
							      "The fraction of total work that has been completed",
							      0.0, 1.0, 0.0,
							      G_PARAM_READWRITE |
                                                              G_PARAM_STATIC_STRINGS));
}

static void
ev_progress_message_area_init (EvProgressMessageArea *area)
{
	gtk_widget_init_template (GTK_WIDGET (area));
}

static void
ev_progress_message_area_set_property (GObject      *object,
				       guint         prop_id,
				       const GValue *value,
				       GParamSpec   *pspec)
{
	EvProgressMessageArea *area = EV_PROGRESS_MESSAGE_AREA (object);

	switch (prop_id) {
	case PROP_STATUS:
		ev_progress_message_area_set_status (area, g_value_get_string (value));
		break;
	case PROP_FRACTION:
		ev_progress_message_area_set_fraction (area, g_value_get_double (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
ev_progress_message_area_get_property (GObject    *object,
				       guint       prop_id,
				       GValue     *value,
				       GParamSpec *pspec)
{
	EvProgressMessageArea *area = EV_PROGRESS_MESSAGE_AREA (object);
	EvProgressMessageAreaPrivate *priv;

	priv = ev_progress_message_area_get_instance_private (area);

	switch (prop_id) {
	case PROP_STATUS:
		g_value_set_string (value, gtk_label_get_label (GTK_LABEL (priv->label)));
		break;
	case PROP_FRACTION: {
		gdouble fraction;

		fraction = gtk_progress_bar_get_fraction (GTK_PROGRESS_BAR (priv->progress_bar));
		g_value_set_double (value, fraction);
	}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

GtkWidget *
ev_progress_message_area_new (const gchar *icon_name,
			      const gchar *text,
			      const gchar *first_button_text,
			      ...)
{
	GtkWidget *widget;
	GtkWidget *info_bar;

	widget = g_object_new (EV_TYPE_PROGRESS_MESSAGE_AREA,
			       "text", text,
			       NULL);
	info_bar = ev_message_area_get_info_bar (EV_MESSAGE_AREA (widget));
	gtk_info_bar_set_message_type (GTK_INFO_BAR (info_bar), GTK_MESSAGE_OTHER);


	if (first_button_text) {
		va_list args;

		va_start (args, first_button_text);
		_ev_message_area_add_buttons_valist (EV_MESSAGE_AREA (widget),
						     first_button_text,
						     args);
		va_end (args);
	}

	ev_message_area_set_image_from_icon_name (EV_MESSAGE_AREA (widget),
						  icon_name);

	return widget;
}

void
ev_progress_message_area_set_status (EvProgressMessageArea *area,
				     const gchar           *str)
{
	EvProgressMessageAreaPrivate *priv;

	g_return_if_fail (EV_IS_PROGRESS_MESSAGE_AREA (area));

	priv = ev_progress_message_area_get_instance_private (area);

	gtk_label_set_text (GTK_LABEL (priv->label), str);

	g_object_notify (G_OBJECT (area), "status");
}

void
ev_progress_message_area_set_fraction (EvProgressMessageArea *area,
				       gdouble                fraction)
{
	EvProgressMessageAreaPrivate *priv;

	g_return_if_fail (EV_IS_PROGRESS_MESSAGE_AREA (area));

	priv = ev_progress_message_area_get_instance_private (area);

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (priv->progress_bar),
				       fraction);
	g_object_notify (G_OBJECT (area), "fraction");
}
