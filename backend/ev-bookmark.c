/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2005 Red Hat, Inc.
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

#include "ev-bookmark.h"

enum {
	PROP_0,
	PROP_TITLE,
	PROP_TYPE,
	PROP_PAGE,
	PROP_URI
};

struct _EvBookmarkPrivate {
	char *title;
	char *uri;
	EvBookmarkType type;
	int page;
};

static GObjectClass *parent_class = NULL;

G_DEFINE_TYPE (EvBookmark, ev_bookmark, G_TYPE_OBJECT)

#define EV_BOOKMARK_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_BOOKMARK, EvBookmarkPrivate))

GType
ev_bookmark_type_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GEnumValue values[] = {
			{ EV_BOOKMARK_TYPE_TITLE, "EV_BOOKMARK_TYPE_TITLE", "title" },
			{ EV_BOOKMARK_TYPE_LINK, "EV_BOOKMARK_TYPE_LINK", "link" },
			{ EV_BOOKMARK_TYPE_EXTERNAL_URI, "EV_BOOKMARK_TYPE_EXTERNAL_URI", "external" },
			{ 0, NULL, NULL }
                };

                type = g_enum_register_static ("EvBookmarkType", values);
        }

        return type;
}

const char *
ev_bookmark_get_title (EvBookmark *self)
{
	g_return_val_if_fail (EV_IS_BOOKMARK (self), NULL);
	
	return self->priv->title;
}

void
ev_bookmark_set_title (EvBookmark* self, const char *title)
{
	g_assert (EV_IS_BOOKMARK (self));
	g_assert (title != NULL);

	if (self->priv->title != NULL) {
		g_free (self->priv->title);
	}

	self->priv->title = g_strdup (title);

	g_object_notify (G_OBJECT (self), "title");
}

const char *
ev_bookmark_get_uri (EvBookmark *self)
{
	g_return_val_if_fail (EV_IS_BOOKMARK (self), NULL);
	
	return self->priv->uri;
}

void
ev_bookmark_set_uri (EvBookmark* self, const char *uri)
{
	g_assert (EV_IS_BOOKMARK (self));
	g_assert (uri != NULL);

	if (self->priv->uri != NULL) {
		g_free (self->priv->uri);
	}

	self->priv->uri = g_strdup (uri);

	g_object_notify (G_OBJECT (self), "uri");
}

EvBookmarkType
ev_bookmark_get_bookmark_type (EvBookmark *self)
{
	g_return_val_if_fail (EV_IS_BOOKMARK (self), 0);
	
	return self->priv->type;
}

void
ev_bookmark_set_bookmark_type (EvBookmark* self, EvBookmarkType type)
{
	g_assert (EV_IS_BOOKMARK (self));

	self->priv->type = type;

	g_object_notify (G_OBJECT (self), "type");
}

int
ev_bookmark_get_page (EvBookmark *self)
{
	g_return_val_if_fail (EV_IS_BOOKMARK (self), 0);
	
	return self->priv->page;
}

void
ev_bookmark_set_page (EvBookmark* self, int page)
{
	g_assert (EV_IS_BOOKMARK (self));

	self->priv->page = page;

	g_object_notify (G_OBJECT (self), "page");
}

static void
ev_bookmark_get_property (GObject *object, guint prop_id, GValue *value,
			  GParamSpec *param_spec)
{
	EvBookmark *self;

	self = EV_BOOKMARK (object);

	switch (prop_id) {
	case PROP_TITLE:
		g_value_set_string (value, self->priv->title);
		break;
	case PROP_URI:
		g_value_set_string (value, self->priv->uri);
		break;
	case PROP_TYPE:
		g_value_set_enum (value, self->priv->type);
		break;
	case PROP_PAGE:
		g_value_set_int (value, self->priv->page);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
						   prop_id,
						   param_spec);
		break;
	}
}

static void
ev_bookmark_set_property (GObject *object, guint prop_id, const GValue *value,
			  GParamSpec *param_spec)
{
	EvBookmark *self;
	
	self = EV_BOOKMARK (object);
	
	switch (prop_id) {
	case PROP_TITLE:
		ev_bookmark_set_title (self, g_value_get_string (value));
		break;
	case PROP_URI:
		ev_bookmark_set_uri (self, g_value_get_string (value));
		break;
	case PROP_TYPE:
		ev_bookmark_set_bookmark_type (self, g_value_get_enum (value));
		break;
	case PROP_PAGE:
		ev_bookmark_set_page (self, g_value_get_int (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
						   prop_id,
						   param_spec);
		break;
	}
}

static void
ev_window_dispose (GObject *object)
{
	EvBookmarkPrivate *priv;

	g_return_if_fail (EV_IS_BOOKMARK (object));

	priv = EV_BOOKMARK (object)->priv;

	if (priv->title) {
		g_free (priv->title);
		priv->title = NULL;
	}

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
ev_bookmark_init (EvBookmark *ev_bookmark)
{
	ev_bookmark->priv = EV_BOOKMARK_GET_PRIVATE (ev_bookmark);

	ev_bookmark->priv->type = EV_BOOKMARK_TYPE_TITLE;
}

static void
ev_bookmark_class_init (EvBookmarkClass *ev_window_class)
{
	GObjectClass *g_object_class;

	parent_class = g_type_class_peek_parent (ev_window_class);

	g_object_class = G_OBJECT_CLASS (ev_window_class);
	g_object_class->dispose = ev_window_dispose;
	g_object_class->set_property = ev_bookmark_set_property;
	g_object_class->get_property = ev_bookmark_get_property;

	g_type_class_add_private (g_object_class, sizeof (EvBookmarkPrivate));

	g_object_class_install_property (g_object_class,
					 PROP_TITLE,
					 g_param_spec_string ("title",
				     	 		      "Bookmark Title",
				     			      "The bookmark title",
							      NULL,
				     			      G_PARAM_READWRITE));

	g_object_class_install_property (g_object_class,
					 PROP_URI,
					 g_param_spec_string ("uri",
				     	 		      "Bookmark URI",
				     			      "The bookmark URI",
							      NULL,
				     			      G_PARAM_READWRITE));

	g_object_class_install_property (g_object_class,
					 PROP_TYPE,
			 		 g_param_spec_enum  ("type",
                              				     "Bookmark Type",
							     "The bookmark type",
							     EV_TYPE_BOOKMARK_TYPE,
							     EV_BOOKMARK_TYPE_TITLE,
							     G_PARAM_READWRITE));

	g_object_class_install_property (g_object_class,
					 PROP_PAGE,
					 g_param_spec_int ("page",
							   "Bookmark Page",
							   "The bookmark page",
							    0,
							    G_MAXINT,
							    0,
							    G_PARAM_READWRITE));
}

EvBookmark *
ev_bookmark_new_title (const char *title)
{
	return EV_BOOKMARK (g_object_new (EV_TYPE_BOOKMARK,
					  "title", title,
					  "type", EV_BOOKMARK_TYPE_TITLE,
					  NULL));
}

EvBookmark *
ev_bookmark_new_link (const char *title, int page)
{
	return EV_BOOKMARK (g_object_new (EV_TYPE_BOOKMARK,
					  "title", title,
					  "page", page,
					  "type", EV_BOOKMARK_TYPE_LINK,
					  NULL));
}

EvBookmark *
ev_bookmark_new_external (const char *title, const char *uri)
{
	return EV_BOOKMARK (g_object_new (EV_TYPE_BOOKMARK,
					  "title", title,
					  "uri", uri,
					  "type", EV_BOOKMARK_TYPE_EXTERNAL_URI,
					  NULL));
}
