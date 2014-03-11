/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2014 Igalia S.L.
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
 *
 * Author: Alejandro Piñeiro Iglesias <apinheiro@igalia.com>
 */

#include <config.h>

#include "ev-page-accessible.h"

struct _EvPageAccessiblePrivate {
        EvViewAccessible *view_accessible;
	gint              page;
};

enum {
	PROP_0,
	PROP_VIEW_ACCESSIBLE,
	PROP_PAGE,
};


G_DEFINE_TYPE (EvPageAccessible, ev_page_accessible, ATK_TYPE_OBJECT)

gint
ev_page_accessible_get_page (EvPageAccessible *page_accessible)
{
	g_return_val_if_fail (EV_IS_PAGE_ACCESSIBLE (page_accessible), -1);

	return page_accessible->priv->page;
}

EvViewAccessible *
ev_page_accessible_get_view_accessible (EvPageAccessible *page_accessible)
{
	g_return_val_if_fail (EV_IS_PAGE_ACCESSIBLE (page_accessible), NULL);

	return page_accessible->priv->view_accessible;
}

static AtkObject *
ev_page_accessible_get_parent (AtkObject *obj)
{
	EvPageAccessible *self;

	g_return_val_if_fail (EV_IS_PAGE_ACCESSIBLE (obj), NULL);

	self = EV_PAGE_ACCESSIBLE (obj);

	return ATK_OBJECT (self->priv->view_accessible);
}

static void
ev_page_accessible_set_property (GObject      *object,
				 guint         prop_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
	EvPageAccessible *accessible = EV_PAGE_ACCESSIBLE (object);

	switch (prop_id) {
	case PROP_VIEW_ACCESSIBLE:
		accessible->priv->view_accessible = EV_VIEW_ACCESSIBLE (g_value_get_object (value));
		break;
	case PROP_PAGE:
		accessible->priv->page = g_value_get_int (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
ev_page_accessible_get_property (GObject    *object,
				 guint       prop_id,
				 GValue     *value,
				 GParamSpec *pspec)
{
	EvPageAccessible *accessible = EV_PAGE_ACCESSIBLE (object);

	switch (prop_id) {
	case PROP_VIEW_ACCESSIBLE:
		g_value_set_object (value, ev_page_accessible_get_view_accessible (accessible));
		break;
	case PROP_PAGE:
		g_value_set_int (value, ev_page_accessible_get_page (accessible));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
ev_page_accessible_class_init (EvPageAccessibleClass *klass)
{
	GObjectClass   *g_object_class = G_OBJECT_CLASS (klass);
	AtkObjectClass *atk_class = ATK_OBJECT_CLASS (klass);

        g_type_class_add_private (klass, sizeof (EvPageAccessiblePrivate));

	atk_class->get_parent  = ev_page_accessible_get_parent;

	g_object_class->get_property = ev_page_accessible_get_property;
	g_object_class->set_property = ev_page_accessible_set_property;

	g_object_class_install_property (g_object_class,
					 PROP_VIEW_ACCESSIBLE,
					 g_param_spec_object ("view-accessible",
							      "View Accessible",
							      "The view accessible associated to this page",
							      EV_TYPE_VIEW_ACCESSIBLE,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_PAGE,
					 g_param_spec_int ("page",
							   "Page",
							   "Page index this page represents",
							   -1, G_MAXINT, -1,
							   G_PARAM_READWRITE |
							   G_PARAM_CONSTRUCT_ONLY |
                                                           G_PARAM_STATIC_STRINGS));

}

static void
ev_page_accessible_init (EvPageAccessible *page)
{
	atk_object_set_role (ATK_OBJECT (page), ATK_ROLE_PAGE);

        page->priv = G_TYPE_INSTANCE_GET_PRIVATE (page, EV_TYPE_PAGE_ACCESSIBLE, EvPageAccessiblePrivate);
}

EvPageAccessible *
ev_page_accessible_new (EvViewAccessible *view_accessible,
                        gint              page)
{
        EvPageAccessible *atk_page;

	g_return_val_if_fail (EV_IS_VIEW_ACCESSIBLE (view_accessible), NULL);
	g_return_val_if_fail (page >= 0, NULL);

        atk_page = g_object_new (EV_TYPE_PAGE_ACCESSIBLE,
				 "view-accessible", view_accessible,
				 "page", page,
				 NULL);

        return EV_PAGE_ACCESSIBLE (atk_page);
}
