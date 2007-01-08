/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2006 Carlos Garcia Campos <carlosgc@gnome.org>
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

#include "ev-link-dest.h"

enum {
	PROP_0,
	PROP_TYPE,
	PROP_PAGE,
	PROP_LEFT,
	PROP_TOP,
	PROP_BOTTOM,
	PROP_RIGHT,
	PROP_ZOOM,
	PROP_NAMED,
	PROP_PAGE_LABEL
};

struct _EvLinkDest {
	GObject base_instance;
	
	EvLinkDestPrivate *priv;
};

struct _EvLinkDestClass {
	GObjectClass base_class;
};

struct _EvLinkDestPrivate {
	EvLinkDestType type;
	int            page;
	double         top;
	double         left;
	double         bottom;
	double         right;
	double         zoom;
	gchar         *named;
	gchar	      *page_label;
};

G_DEFINE_TYPE (EvLinkDest, ev_link_dest, G_TYPE_OBJECT)

#define EV_LINK_DEST_GET_PRIVATE(object) \
        (G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_LINK_DEST, EvLinkDestPrivate))

GType
ev_link_dest_type_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GEnumValue values[] = {
			{ EV_LINK_DEST_TYPE_PAGE, "EV_LINK_DEST_TYPE_PAGE", "page" },
			{ EV_LINK_DEST_TYPE_XYZ, "EV_LINK_DEST_TYPE_XYZ", "xyz" },
			{ EV_LINK_DEST_TYPE_FIT, "EV_LINK_DEST_TYPE_FIT", "fit" },
			{ EV_LINK_DEST_TYPE_FITH, "EV_LINK_DEST_TYPE_FITH", "fith" },
			{ EV_LINK_DEST_TYPE_FITV, "EV_LINK_DEST_TYPE_FITV", "fitv" },
			{ EV_LINK_DEST_TYPE_FITR, "EV_LINK_DEST_TYPE_FITR", "fitr" },
			{ EV_LINK_DEST_TYPE_NAMED, "EV_LINK_DEST_TYPE_NAMED", "named" },
			{ EV_LINK_DEST_TYPE_PAGE_LABEL, "EV_LINK_DEST_TYPE_PAGE_LABEL", "page_label" },
			{ EV_LINK_DEST_TYPE_UNKNOWN, "EV_LINK_DEST_TYPE_UNKNOWN", "unknown" },
			{ 0, NULL, NULL }
		};

		type = g_enum_register_static ("EvLinkDestType", values);
	}

	return type;
}

EvLinkDestType
ev_link_dest_get_dest_type (EvLinkDest *self)
{
	g_return_val_if_fail (EV_IS_LINK_DEST (self), 0);

	return self->priv->type;
}

gint
ev_link_dest_get_page (EvLinkDest *self)
{
	g_return_val_if_fail (EV_IS_LINK_DEST (self), -1);

	return self->priv->page;
}

gdouble
ev_link_dest_get_top (EvLinkDest *self)
{
	g_return_val_if_fail (EV_IS_LINK_DEST (self), 0);

	return self->priv->top;
}

gdouble
ev_link_dest_get_left (EvLinkDest *self)
{
	g_return_val_if_fail (EV_IS_LINK_DEST (self), 0);

	return self->priv->left;
}

gdouble
ev_link_dest_get_bottom (EvLinkDest *self)
{
	g_return_val_if_fail (EV_IS_LINK_DEST (self), 0);

	return self->priv->bottom;
}

gdouble
ev_link_dest_get_right (EvLinkDest *self)
{
	g_return_val_if_fail (EV_IS_LINK_DEST (self), 0);

	return self->priv->right;
}

gdouble
ev_link_dest_get_zoom (EvLinkDest *self)
{
	g_return_val_if_fail (EV_IS_LINK_DEST (self), 0);

	return self->priv->zoom;
}

const gchar *
ev_link_dest_get_named_dest (EvLinkDest *self)
{
	g_return_val_if_fail (EV_IS_LINK_DEST (self), NULL);

	return self->priv->named;
}

const gchar *
ev_link_dest_get_page_label (EvLinkDest *self)
{
	g_return_val_if_fail (EV_IS_LINK_DEST (self), NULL);

	return self->priv->page_label;
}

static void
ev_link_dest_get_property (GObject    *object,
			   guint       prop_id,
			   GValue     *value,
			   GParamSpec *param_spec)
{
	EvLinkDest *self;

	self = EV_LINK_DEST (object);

	switch (prop_id) {
	        case PROP_TYPE:
			g_value_set_enum (value, self->priv->type);
			break;
	        case PROP_PAGE:
			g_value_set_int (value, self->priv->page);
			break;
	        case PROP_TOP:
			g_value_set_double (value, self->priv->top);
			break;
	        case PROP_LEFT:
			g_value_set_double (value, self->priv->left);
			break;
	        case PROP_BOTTOM:
			g_value_set_double (value, self->priv->bottom);
			break;
	        case PROP_RIGHT:
			g_value_set_double (value, self->priv->left);
			break;
	        case PROP_ZOOM:
			g_value_set_double (value, self->priv->zoom);
			break;
	        case PROP_NAMED:
			g_value_set_string (value, self->priv->named);
			break;
	        case PROP_PAGE_LABEL:
			g_value_set_string (value, self->priv->page_label);
			break;
	        default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
							   prop_id,
							   param_spec);
			break;
	}
}

static void
ev_link_dest_set_property (GObject      *object,
			   guint         prop_id,
			   const GValue *value,
			   GParamSpec   *param_spec)
{
	EvLinkDest *self = EV_LINK_DEST (object);

	switch (prop_id) {
	        case PROP_TYPE:
			self->priv->type = g_value_get_enum (value);
			break;
	        case PROP_PAGE:
			self->priv->page = g_value_get_int (value);
			break;
	        case PROP_TOP:
			self->priv->top = g_value_get_double (value);
			break;
	        case PROP_LEFT:
			self->priv->left = g_value_get_double (value);
			break;
	        case PROP_BOTTOM:
			self->priv->bottom = g_value_get_double (value);
			break;
	        case PROP_RIGHT:
			self->priv->right = g_value_get_double (value);
			break;
	        case PROP_ZOOM:
			self->priv->zoom = g_value_get_double (value);
			break;
	        case PROP_NAMED:
			self->priv->named = g_value_dup_string (value);
			break;
	        case PROP_PAGE_LABEL:
			self->priv->page_label = g_value_dup_string (value);
			break;
	        default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
							   prop_id,
							   param_spec);
			break;
	}
}

static void
ev_link_dest_finalize (GObject *object)
{
	EvLinkDestPrivate *priv;

	priv = EV_LINK_DEST (object)->priv;

	if (priv->named) {
		g_free (priv->named);
		priv->named = NULL;
	}
	if (priv->page_label) {
		g_free (priv->page_label);
		priv->page_label = NULL;
	}

	G_OBJECT_CLASS (ev_link_dest_parent_class)->finalize (object);
}

static void
ev_link_dest_init (EvLinkDest *ev_link_dest)
{
	ev_link_dest->priv = EV_LINK_DEST_GET_PRIVATE (ev_link_dest);

	ev_link_dest->priv->named = NULL;
}

static void
ev_link_dest_class_init (EvLinkDestClass *ev_link_dest_class)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS (ev_link_dest_class);

	g_object_class->set_property = ev_link_dest_set_property;
	g_object_class->get_property = ev_link_dest_get_property;

	g_object_class->finalize = ev_link_dest_finalize;

	g_type_class_add_private (g_object_class, sizeof (EvLinkDestPrivate));

	g_object_class_install_property (g_object_class,
					 PROP_TYPE,
					 g_param_spec_enum  ("type",
							     "Dest Type",
							     "The destination type",
							     EV_TYPE_LINK_DEST_TYPE,
							     EV_LINK_DEST_TYPE_UNKNOWN,
							     G_PARAM_READWRITE |
							     G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (g_object_class,
					 PROP_PAGE,
					 g_param_spec_int ("page",
							   "Dest Page",
							   "The destination page",
							   -1,
							   G_MAXINT,
							   0,
							   G_PARAM_READWRITE |
							   G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (g_object_class,
					 PROP_LEFT,
					 g_param_spec_double ("left",
							      "Left coordinate",
							      "The left coordinate",
							      -G_MAXDOUBLE,
							      G_MAXDOUBLE,
							      0,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (g_object_class,
					 PROP_TOP,
					 g_param_spec_double ("top",
							      "Top coordinate",
							      "The top coordinate",
							      -G_MAXDOUBLE,
							      G_MAXDOUBLE,
							      0,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (g_object_class,
					 PROP_BOTTOM,
					 g_param_spec_double ("bottom",
							      "Bottom coordinate",
							      "The bottom coordinate",
							      -G_MAXDOUBLE,
							      G_MAXDOUBLE,
							      0,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (g_object_class,
					 PROP_RIGHT,
					 g_param_spec_double ("right",
							      "Right coordinate",
							      "The right coordinate",
							      -G_MAXDOUBLE,
							      G_MAXDOUBLE,
							      0,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (g_object_class,
					 PROP_ZOOM,
					 g_param_spec_double ("zoom",
							      "Zoom",
							      "Zoom",
							      0,
							      G_MAXDOUBLE,
							      0,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (g_object_class,
					 PROP_NAMED,
					 g_param_spec_string ("named",
							      "Named destination",
							      "The named destination",
							      NULL,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (g_object_class,
					 PROP_PAGE_LABEL,
					 g_param_spec_string ("page_label",
							      "Label of the page",
							      "The label of the destination page",
							      NULL,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY));
}

EvLinkDest *
ev_link_dest_new_page (gint page)
{
	return EV_LINK_DEST (g_object_new (EV_TYPE_LINK_DEST,
					   "page", page,
					   "type", EV_LINK_DEST_TYPE_PAGE,
					   NULL));
}

EvLinkDest *
ev_link_dest_new_xyz (gint    page,
		      gdouble left,
		      gdouble top,
		      gdouble zoom)
{
	return EV_LINK_DEST (g_object_new (EV_TYPE_LINK_DEST,
					   "page", page,
					   "type", EV_LINK_DEST_TYPE_XYZ,
					   "left", left,
					   "top", top,
					   "zoom", zoom,
					   NULL));
}

EvLinkDest *
ev_link_dest_new_fit (gint page)
{
	return EV_LINK_DEST (g_object_new (EV_TYPE_LINK_DEST,
					   "page", page,
					   "type", EV_LINK_DEST_TYPE_FIT,
					   NULL));
}

EvLinkDest *
ev_link_dest_new_fith (gint    page,
		       gdouble top)
{
	return EV_LINK_DEST (g_object_new (EV_TYPE_LINK_DEST,
					   "page", page,
					   "type", EV_LINK_DEST_TYPE_FITH,
					   "top", top,
					   NULL));
}

EvLinkDest *
ev_link_dest_new_fitv (gint    page,
		       gdouble left)
{
	return EV_LINK_DEST (g_object_new (EV_TYPE_LINK_DEST,
					   "page", page,
					   "type", EV_LINK_DEST_TYPE_FITV,
					   "left", left,
					   NULL));
}

EvLinkDest *
ev_link_dest_new_fitr (gint    page,
		       gdouble left,
		       gdouble bottom,
		       gdouble right,
		       gdouble top)
{
	return EV_LINK_DEST (g_object_new (EV_TYPE_LINK_DEST,
					   "page", page,
					   "type", EV_LINK_DEST_TYPE_FITR,
					   "left", left,
					   "bottom", bottom,
					   "right", right,
					   "top", top,
					   NULL));
}

EvLinkDest *
ev_link_dest_new_named (const gchar *named_dest)
{
	return EV_LINK_DEST (g_object_new (EV_TYPE_LINK_DEST,
					   "named", named_dest,
					   "type", EV_LINK_DEST_TYPE_NAMED,
					   NULL));
}

EvLinkDest *
ev_link_dest_new_page_label (const gchar *page_label)
{
	return EV_LINK_DEST (g_object_new (EV_TYPE_LINK_DEST,
					   "page_label", page_label,
					   "type", EV_LINK_DEST_TYPE_PAGE_LABEL,
					   NULL));
}
