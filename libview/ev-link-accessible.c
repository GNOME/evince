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
        EvPageAccessible *page;
        EvLink           *link;
        EvRectangle       area;

        EvHyperlink      *hyperlink;

        gchar      *name;
        gint        start_index;
        gint        end_index;
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
        EvView                  *view;
        EvRectangle             *areas = NULL;
        guint                    n_areas = 0;
        guint                    i;

        if (!hyperlink->link_impl)
                return -1;

        impl_priv = hyperlink->link_impl->priv;
        if (impl_priv->start_index != -1)
		return impl_priv->start_index;

	view = ev_page_accessible_get_view (impl_priv->page);
        if (!view->page_cache)
                return -1;

        ev_page_cache_get_text_layout (view->page_cache,
				       ev_page_accessible_get_page (impl_priv->page),
				       &areas, &n_areas);
        if (!areas)
                return -1;

        for (i = 0; i < n_areas; i++) {
                EvRectangle *rect = areas + i;
                gdouble      c_x, c_y;

                c_x = rect->x1 + (rect->x2 - rect->x1) / 2.;
                c_y = rect->y1 + (rect->y2 - rect->y1) / 2.;
                if (c_x >= impl_priv->area.x1 && c_x <= impl_priv->area.x2 &&
                    c_y >= impl_priv->area.y1 && c_y <= impl_priv->area.y2) {
                        impl_priv->start_index = i;
                        return i;
		}
        }

        return -1;
}

static gint
ev_hyperlink_get_end_index (AtkHyperlink *atk_hyperlink)
{
        EvHyperlink             *hyperlink = EV_HYPERLINK (atk_hyperlink);
        EvLinkAccessiblePrivate *impl_priv;
        EvView                  *view;
        EvRectangle             *areas = NULL;
        guint                    n_areas = 0;
        guint                    i;
        gint                     start_index;

        if (!hyperlink->link_impl)
                return -1;

        impl_priv = hyperlink->link_impl->priv;
        if (impl_priv->end_index != -1)
		return impl_priv->end_index;

        start_index = ev_hyperlink_get_start_index (atk_hyperlink);
        if (start_index == -1)
		return -1;

	view = ev_page_accessible_get_view (impl_priv->page);
        if (!view->page_cache)
                return -1;

        ev_page_cache_get_text_layout (view->page_cache,
				       ev_page_accessible_get_page (impl_priv->page),
				       &areas, &n_areas);
        if (!areas)
                return -1;

        for (i = start_index + 1; i < n_areas; i++) {
                EvRectangle *rect = areas + i;
                gdouble      c_x, c_y;

                c_x = rect->x1 + (rect->x2 - rect->x1) / 2.;
                c_y = rect->y1 + (rect->y2 - rect->y1) / 2.;
                if (c_x < impl_priv->area.x1 || c_x > impl_priv->area.x2 ||
                    c_y < impl_priv->area.y1 || c_y > impl_priv->area.y2) {
                        impl_priv->end_index = i;
                        return i;
		}
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
static void ev_link_accessible_component_iface_init      (AtkComponentIface     *iface);

G_DEFINE_TYPE_WITH_CODE (EvLinkAccessible, ev_link_accessible, ATK_TYPE_OBJECT,
			 G_ADD_PRIVATE (EvLinkAccessible)
			 G_IMPLEMENT_INTERFACE (ATK_TYPE_HYPERLINK_IMPL, ev_link_accessible_hyperlink_impl_iface_init)
			 G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, ev_link_accessible_action_interface_init)
			 G_IMPLEMENT_INTERFACE (ATK_TYPE_COMPONENT, ev_link_accessible_component_iface_init))

static const gchar *
ev_link_accessible_get_name (AtkObject *atk_object)
{
	EvLinkAccessiblePrivate *priv;
	gint start_index;
	gint end_index;

	priv = EV_LINK_ACCESSIBLE (atk_object)->priv;
	if (priv->name)
		return priv->name;

	start_index = ev_hyperlink_get_start_index (ATK_HYPERLINK (priv->hyperlink));
	end_index = ev_hyperlink_get_end_index (ATK_HYPERLINK (priv->hyperlink));
	priv->name = atk_text_get_text (ATK_TEXT (atk_object_get_parent (atk_object)), start_index, end_index);

	return priv->name;
}

static AtkObject *
ev_link_accessible_get_parent (AtkObject *atk_object)
{
	EvLinkAccessiblePrivate *priv = EV_LINK_ACCESSIBLE (atk_object)->priv;

	return ATK_OBJECT (priv->page);
}

static AtkStateSet *
ev_link_accessible_ref_state_set (AtkObject *atk_object)
{
	AtkStateSet *state_set;
	AtkStateSet *copy_set;
	AtkStateSet *page_accessible_state_set;
	EvLinkAccessible *self;
	EvViewAccessible *view_accessible;
	EvView *view;
	gint page;

	self = EV_LINK_ACCESSIBLE (atk_object);
	state_set = ATK_OBJECT_CLASS (ev_link_accessible_parent_class)->ref_state_set (atk_object);
	atk_state_set_clear_states (state_set);

	page_accessible_state_set = atk_object_ref_state_set (ATK_OBJECT (self->priv->page));
	copy_set = atk_state_set_or_sets (state_set, page_accessible_state_set);

	view_accessible = ev_page_accessible_get_view_accessible (self->priv->page);
	page = ev_page_accessible_get_page (self->priv->page);
	if (!ev_view_accessible_is_doc_rect_showing (view_accessible, page, &self->priv->area))
		atk_state_set_remove_state (copy_set, ATK_STATE_SHOWING);

	view = ev_page_accessible_get_view (self->priv->page);
	if (!view->focused_element || view->focused_element->data != self->priv->link)
		atk_state_set_remove_state (copy_set, ATK_STATE_FOCUSED);

	g_object_unref (state_set);
	g_object_unref (page_accessible_state_set);

	return copy_set;
}

static void
ev_link_accessible_finalize (GObject *object)
{
        EvLinkAccessible *link = EV_LINK_ACCESSIBLE (object);

        g_clear_object (&link->priv->hyperlink);
        g_free (link->priv->name);

        G_OBJECT_CLASS (ev_link_accessible_parent_class)->finalize (object);
}

static void
ev_link_accessible_class_init (EvLinkAccessibleClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        AtkObjectClass *atk_class = ATK_OBJECT_CLASS (klass);

        object_class->finalize = ev_link_accessible_finalize;

        atk_class->get_parent = ev_link_accessible_get_parent;
        atk_class->get_name = ev_link_accessible_get_name;
        atk_class->ref_state_set = ev_link_accessible_ref_state_set;
}

static void
ev_link_accessible_init (EvLinkAccessible *link)
{
        atk_object_set_role (ATK_OBJECT (link), ATK_ROLE_LINK);
        link->priv = ev_link_accessible_get_instance_private (link);
        link->priv->start_index = -1;
        link->priv->end_index = -1;
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
	EvView *view;

	view = ev_page_accessible_get_view (priv->page);
	if (!ev_link_get_action (priv->link))
		return FALSE;

	ev_view_handle_link (view, priv->link);

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

static void
ev_link_accessible_get_extents (AtkComponent *atk_component,
				gint         *x,
				gint         *y,
				gint         *width,
				gint         *height,
				AtkCoordType coord_type)
{
	EvLinkAccessible *self;
	EvViewAccessible *view_accessible;
	gint page;
	EvRectangle atk_rect;

	self = EV_LINK_ACCESSIBLE (atk_component);
	view_accessible = ev_page_accessible_get_view_accessible (self->priv->page);
	page = ev_page_accessible_get_page (self->priv->page);
	_transform_doc_rect_to_atk_rect (view_accessible, page, &self->priv->area, &atk_rect, coord_type);
	*x = atk_rect.x1;
	*y = atk_rect.y1;
	*width = atk_rect.x2 - atk_rect.x1;
	*height = atk_rect.y2 - atk_rect.y1;
}

static gboolean
ev_link_accessible_grab_focus (AtkComponent *atk_component)
{
	EvLinkAccessible *self;
	EvView *view;
	EvMappingList *link_mapping;
	EvMapping *mapping;
	gint page;

	self = EV_LINK_ACCESSIBLE (atk_component);
	view = ev_page_accessible_get_view (self->priv->page);
	page = ev_page_accessible_get_page (self->priv->page);
	link_mapping = ev_page_cache_get_link_mapping (view->page_cache, page);
	mapping = ev_mapping_list_find (link_mapping, self->priv->link);
	_ev_view_set_focused_element (view, mapping, page);

	return TRUE;
}

static void
ev_link_accessible_component_iface_init (AtkComponentIface *iface)
{
	iface->get_extents = ev_link_accessible_get_extents;
	iface->grab_focus = ev_link_accessible_grab_focus;
}

EvLinkAccessible *
ev_link_accessible_new (EvPageAccessible *page,
                        EvLink           *link,
                        EvRectangle      *area)
{
        EvLinkAccessible *atk_link;

        atk_link = g_object_new (EV_TYPE_LINK_ACCESSIBLE, NULL);
        atk_link->priv->page = page;
        atk_link->priv->link = g_object_ref (link);
        atk_link->priv->area = *area;

        return EV_LINK_ACCESSIBLE (atk_link);
}
