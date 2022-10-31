/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2022 Qiu Wenbo
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

#include "ev-thumbnail-item.h"



struct _EvThumbnailItemPrivate
{
	gchar *primary_text;
	gchar *secondary_text;
	gchar *uri;
	gchar *uri_display;

	GdkPaintable *paintable;
};

typedef struct _EvThumbnailItemPrivate EvThumbnailItemPrivate;
#define GET_PRIVATE(o) ev_thumbnail_item_get_instance_private (o)

G_DEFINE_TYPE_WITH_PRIVATE (EvThumbnailItem, ev_thumbnail_item, G_TYPE_OBJECT)

enum {
	PROP_0,
	PROP_PRIMARY_TEXT,
	PROP_SECONDARY_TEXT,
	PROP_URI,
	PROP_URI_DISPLAY,
	PROP_PAINTABLE,
};

void ev_thumbnail_item_set_primary_text (EvThumbnailItem *ev_recent_item,
				      const gchar *primary_text)
{
	EvThumbnailItemPrivate *priv = GET_PRIVATE (ev_recent_item);

	priv->primary_text = g_strdup (primary_text);

	g_object_notify (G_OBJECT (ev_recent_item), "primary-text");
}

void ev_thumbnail_item_set_secondary_text (EvThumbnailItem *ev_recent_item,
					   const gchar *secondary_text)
{
	EvThumbnailItemPrivate *priv = GET_PRIVATE (ev_recent_item);

	priv->secondary_text = g_strdup (secondary_text);

	g_object_notify (G_OBJECT (ev_recent_item), "secondary-text");
}

void ev_thumbnail_item_set_paintable (EvThumbnailItem *ev_recent_item,
				 GdkPaintable *paintable)
{
	EvThumbnailItemPrivate *priv = GET_PRIVATE (ev_recent_item);

	if (priv->paintable == paintable)
		return;

	g_clear_object (&priv->paintable);
	priv->paintable = paintable ? g_object_ref (paintable) : NULL;

	g_object_notify (G_OBJECT (ev_recent_item), "paintable");
}

static void ev_thumbnail_item_set_property (GObject      *object,
					 guint         prop_id,
					 const GValue *value,
					 GParamSpec   *pspec)
{
	EvThumbnailItem *recent_item = EV_THUMBNAIL_ITEM (object);
	EvThumbnailItemPrivate *priv = GET_PRIVATE (recent_item);

	switch (prop_id) {
		case PROP_PRIMARY_TEXT:
			ev_thumbnail_item_set_primary_text (recent_item,
					g_value_get_string (value));
			break;
		case PROP_SECONDARY_TEXT:
			ev_thumbnail_item_set_secondary_text (recent_item,
					g_value_get_string (value));
			break;
		case PROP_URI:
			priv->uri = g_strdup (g_value_get_string (value));
			break;
		case PROP_URI_DISPLAY:
			priv->uri_display = g_strdup (g_value_get_string (value));
			break;
		case PROP_PAINTABLE:
			ev_thumbnail_item_set_paintable (recent_item, g_value_get_object (value));
			break;
	        default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void ev_thumbnail_item_get_property (GObject    *object,
					 guint       prop_id,
					 GValue     *value,
					 GParamSpec *pspec)
{
	EvThumbnailItem *recent_item = EV_THUMBNAIL_ITEM (object);
	EvThumbnailItemPrivate *priv = GET_PRIVATE (recent_item);

	switch (prop_id) {
		case PROP_PRIMARY_TEXT:
			g_value_set_string (value, priv->primary_text);
			break;
		case PROP_SECONDARY_TEXT:
			g_value_set_string (value, priv->secondary_text);
			break;
		case PROP_URI:
			g_value_set_string (value, priv->uri);
			break;
		case PROP_URI_DISPLAY:
			g_value_set_string (value, priv->uri_display);
			break;
		case PROP_PAINTABLE:
			g_value_set_object (value, priv->paintable);
			break;
	        default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
ev_thumbnail_item_dispose (GObject *object)
{
	EvThumbnailItem *recent_item = EV_THUMBNAIL_ITEM (object);
	EvThumbnailItemPrivate *priv = GET_PRIVATE (recent_item);

	g_free (priv->primary_text);
	g_free (priv->secondary_text);
	g_free (priv->uri);
	g_free (priv->uri_display);

	g_clear_object (&priv->paintable);
}

static void ev_thumbnail_item_init (EvThumbnailItem *ev_recent_item)
{
}

static void ev_thumbnail_item_class_init (EvThumbnailItemClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = ev_thumbnail_item_set_property;
	object_class->get_property = ev_thumbnail_item_get_property;
	object_class->dispose = ev_thumbnail_item_dispose;

	g_object_class_install_property (object_class,
					 PROP_PRIMARY_TEXT,
					 g_param_spec_string ("primary-text",
							      "Primary Text",
							      "The primary title",
							      NULL,
							      G_PARAM_READWRITE |
                                                              G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class,
					 PROP_SECONDARY_TEXT,
					 g_param_spec_string ("secondary-text",
							      "Secondary Text",
							      "The secondary title",
							      NULL,
							      G_PARAM_READWRITE |
                                                              G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class,
					 PROP_URI,
					 g_param_spec_string ("uri",
							      "Uri",
							      "The URI of the recent document",
							      NULL,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class,
					 PROP_URI_DISPLAY,
					 g_param_spec_string ("uri-display",
							      "Displayable Uri",
							      "The unescaped URI of the recent document",
							      NULL,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class,
					 PROP_PAINTABLE,
					 g_param_spec_object ("paintable",
							      "Paintable",
							      "The thumbnail of the recent document",
							      GDK_TYPE_PAINTABLE,
							      G_PARAM_READWRITE));
}
