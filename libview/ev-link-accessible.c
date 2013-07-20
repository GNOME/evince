/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2013 Carlos Garcia Campos <carlosgc@gnome.org>
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

#include "ev-link-accessible.h"
#include "ev-view-private.h"

typedef struct _EvHyperlink      EvHyperlink;
typedef struct _EvHyperlinkClass EvHyperlinkClass;

struct _EvLinkAccessiblePrivate {
        EvViewAccessible *view;
        EvLink           *link;
        EvRectangle       area;

        EvHyperlink      *hyperlink;
};

struct _EvHyperlink {
        AtkHyperlink parent;

        EvLinkAccessible *link_impl;
};

struct _EvHyperlinkClass {
        AtkHyperlinkClass parent_class;
};

#define EV_TYPE_HYPERLINK (ev_hyperlink_get_type ())
#define EV_HYPERLINK(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_HYPERLINK, EvHyperlink))

static GType ev_hyperlink_get_type              (void);

G_DEFINE_TYPE (EvHyperlink, ev_hyperlink, ATK_TYPE_HYPERLINK)

static gchar *
ev_hyperlink_get_uri (AtkHyperlink *atk_hyperlink,
                      gint          i)
{
        EvHyperlink             *hyperlink = EV_HYPERLINK (atk_hyperlink);
        EvLinkAccessiblePrivate *impl_priv;
        EvLinkAction            *action;

        if (!hyperlink->link_impl)
                return NULL;

        impl_priv = hyperlink->link_impl->priv;
        action = ev_link_get_action (impl_priv->link);

        return action ? g_strdup (ev_link_action_get_uri (action)) : NULL;
}

static gint
ev_hyperlink_get_n_anchors (AtkHyperlink *atk_hyperlink)
{
        return 1;
}

static gboolean
ev_hyperlink_is_valid (AtkHyperlink *atk_hyperlink)
{
        return TRUE;
}

static AtkObject *
ev_hyperlink_get_object (AtkHyperlink *atk_hyperlink,
                         gint          i)
{
        EvHyperlink *hyperlink = EV_HYPERLINK (atk_hyperlink);

        return hyperlink->link_impl ? ATK_OBJECT (hyperlink->link_impl) : NULL;
}

static gint
ev_hyperlink_get_start_index (AtkHyperlink *atk_hyperlink)
{
        EvHyperlink             *hyperlink = EV_HYPERLINK (atk_hyperlink);
        EvLinkAccessiblePrivate *impl_priv;
        GtkWidget               *widget;
        EvView                  *view;
        EvRectangle             *areas = NULL;
        guint                    n_areas = 0;
        guint                    i;

        if (!hyperlink->link_impl)
                return -1;

        impl_priv = hyperlink->link_impl->priv;
        widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (impl_priv->view));
        if (widget == NULL)
                /* State is defunct */
                return -1;

        view = EV_VIEW (widget);
        if (!view->page_cache)
                return -1;

        ev_page_cache_get_text_layout (view->page_cache, view->current_page, &areas, &n_areas);
        if (!areas)
                return -1;

        for (i = 0; i < n_areas; i++) {
                EvRectangle *rect = areas + i;
                gdouble      c_x, c_y;

                c_x = rect->x1 + (rect->x2 - rect->x1) / 2.;
                c_y = rect->y1 + (rect->y2 - rect->y1) / 2.;
                if (c_x >= impl_priv->area.x1 && c_x <= impl_priv->area.x2 &&
                    c_y >= impl_priv->area.y1 && c_y <= impl_priv->area.y2)
                        return i;
        }

        return -1;
}

static gint
ev_hyperlink_get_end_index (AtkHyperlink *atk_hyperlink)
{
        EvHyperlink             *hyperlink = EV_HYPERLINK (atk_hyperlink);
        EvLinkAccessiblePrivate *impl_priv;
        GtkWidget               *widget;
        EvView                  *view;
        EvRectangle             *areas = NULL;
        guint                    n_areas = 0;
        guint                    i;

        if (!hyperlink->link_impl)
                return -1;

        impl_priv = hyperlink->link_impl->priv;
        widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (impl_priv->view));
        if (widget == NULL)
                /* State is defunct */
                return -1;

        view = EV_VIEW (widget);
        if (!view->page_cache)
                return -1;

        ev_page_cache_get_text_layout (view->page_cache, view->current_page, &areas, &n_areas);
        if (!areas)
                return -1;

        for (i = n_areas - 1; i >= 0; i--) {
                EvRectangle *rect = areas + i;
                gdouble      c_x, c_y;

                c_x = rect->x1 + (rect->x2 - rect->x1) / 2.;
                c_y = rect->y1 + (rect->y2 - rect->y1) / 2.;
                if (c_x >= impl_priv->area.x1 && c_x <= impl_priv->area.x2 &&
                    c_y >= impl_priv->area.y1 && c_y <= impl_priv->area.y2)
                        return i + 1;
        }

        return -1;
}

static void
ev_hyperlink_class_init (EvHyperlinkClass *klass)
{
        AtkHyperlinkClass *atk_link_class = ATK_HYPERLINK_CLASS (klass);

        atk_link_class->get_uri = ev_hyperlink_get_uri;
        atk_link_class->get_n_anchors = ev_hyperlink_get_n_anchors;
        atk_link_class->is_valid = ev_hyperlink_is_valid;
        atk_link_class->get_object = ev_hyperlink_get_object;
        atk_link_class->get_start_index = ev_hyperlink_get_start_index;
        atk_link_class->get_end_index = ev_hyperlink_get_end_index;
}

static void
ev_hyperlink_init (EvHyperlink *link)
{
}

static void ev_link_accessible_hyperlink_impl_iface_init (AtkHyperlinkImplIface *iface);
static void ev_link_accessible_action_interface_init     (AtkActionIface        *iface);

G_DEFINE_TYPE_WITH_CODE (EvLinkAccessible, ev_link_accessible, ATK_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (ATK_TYPE_HYPERLINK_IMPL, ev_link_accessible_hyperlink_impl_iface_init)
			 G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, ev_link_accessible_action_interface_init))

static void
ev_link_accessible_finalize (GObject *object)
{
        EvLinkAccessible *link = EV_LINK_ACCESSIBLE (object);

        g_clear_object (&link->priv->hyperlink);

        G_OBJECT_CLASS (ev_link_accessible_parent_class)->finalize (object);
}

static void
ev_link_accessible_class_init (EvLinkAccessibleClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = ev_link_accessible_finalize;

        g_type_class_add_private (klass, sizeof (EvLinkAccessiblePrivate));
}

static void
ev_link_accessible_init (EvLinkAccessible *link)
{
        link->priv = G_TYPE_INSTANCE_GET_PRIVATE (link, EV_TYPE_LINK_ACCESSIBLE, EvLinkAccessiblePrivate);
}

static AtkHyperlink *
ev_link_accessible_get_hyperlink (AtkHyperlinkImpl *hyperlink_impl)
{
        EvLinkAccessible *link = EV_LINK_ACCESSIBLE (hyperlink_impl);

        if (link->priv->hyperlink)
                return ATK_HYPERLINK (link->priv->hyperlink);

        link->priv->hyperlink = g_object_new (EV_TYPE_HYPERLINK, NULL);

        link->priv->hyperlink->link_impl = link;
        g_object_add_weak_pointer (G_OBJECT (link), (gpointer *)&link->priv->hyperlink->link_impl);

        return ATK_HYPERLINK (link->priv->hyperlink);
}

static void
ev_link_accessible_hyperlink_impl_iface_init (AtkHyperlinkImplIface *iface)
{
        iface->get_hyperlink = ev_link_accessible_get_hyperlink;
}

static gboolean
ev_link_accessible_action_do_action (AtkAction *atk_action,
				     gint      i)
{
	EvLinkAccessiblePrivate *priv = EV_LINK_ACCESSIBLE (atk_action)->priv;
	GtkWidget               *widget;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (priv->view));
	if (widget == NULL)
		/* State is defunct */
		return FALSE;

	if (!ev_link_get_action (priv->link))
		return FALSE;

	ev_view_handle_link (EV_VIEW (widget), priv->link);

	return TRUE;
}

static gint
ev_link_accessible_action_get_n_actions (AtkAction *atk_action)
{
	return 1;
}

static const gchar *
ev_link_accessible_action_get_description (AtkAction *atk_action,
					   gint      i)
{
	/* TODO */
	return NULL;
}

static const gchar *
ev_link_accessible_action_get_name (AtkAction *atk_action,
				    gint      i)
{
	return i == 0 ? "activate" : NULL;
}

static void
ev_link_accessible_action_interface_init (AtkActionIface *iface)
{
	iface->do_action = ev_link_accessible_action_do_action;
	iface->get_n_actions = ev_link_accessible_action_get_n_actions;
	iface->get_description = ev_link_accessible_action_get_description;
	iface->get_name = ev_link_accessible_action_get_name;
}

EvLinkAccessible *
ev_link_accessible_new (EvViewAccessible *view,
                        EvLink           *link,
                        EvRectangle      *area)
{
        EvLinkAccessible *atk_link;

        atk_link = g_object_new (EV_TYPE_LINK_ACCESSIBLE, NULL);
        atk_link->priv->view = view;
        atk_link->priv->link = g_object_ref (link);
        atk_link->priv->area = *area;

        return EV_LINK_ACCESSIBLE (atk_link);
}
