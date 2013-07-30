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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>
#include "ev-link-action.h"
#include "ev-document-type-builtins.h"

enum {
	PROP_0,
	PROP_TYPE,
	PROP_DEST,
	PROP_URI,
	PROP_FILENAME,
	PROP_PARAMS,
	PROP_NAME,
	PROP_SHOW_LIST,
	PROP_HIDE_LIST,
	PROP_TOGGLE_LIST
};

struct _EvLinkAction {
	GObject base_instance;

	EvLinkActionPrivate *priv;
};

struct _EvLinkActionClass {
	GObjectClass base_class;
};

struct _EvLinkActionPrivate {
	EvLinkActionType  type;
	EvLinkDest       *dest;
	gchar            *uri;
	gchar            *filename;
	gchar            *params;
	gchar            *name;
	GList            *show_list;
	GList            *hide_list;
	GList            *toggle_list;
};

G_DEFINE_TYPE (EvLinkAction, ev_link_action, G_TYPE_OBJECT)

#define EV_LINK_ACTION_GET_PRIVATE(object) \
        (G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_LINK_ACTION, EvLinkActionPrivate))

EvLinkActionType
ev_link_action_get_action_type (EvLinkAction *self)
{
	g_return_val_if_fail (EV_IS_LINK_ACTION (self), 0);

	return self->priv->type;
}

/**
 * ev_link_action_get_dest:
 * @self: an #EvLinkAction
 *
 * Returns: (transfer none): an #EvLinkDest
 */
EvLinkDest *
ev_link_action_get_dest (EvLinkAction *self)
{
	g_return_val_if_fail (EV_IS_LINK_ACTION (self), NULL);

	return self->priv->dest;
}

const gchar *
ev_link_action_get_uri (EvLinkAction *self)
{
	g_return_val_if_fail (EV_IS_LINK_ACTION (self), NULL);

	return self->priv->uri;
}

const gchar *
ev_link_action_get_filename (EvLinkAction *self)
{
	g_return_val_if_fail (EV_IS_LINK_ACTION (self), NULL);

	return self->priv->filename;
}

const gchar *
ev_link_action_get_params (EvLinkAction *self)
{
	g_return_val_if_fail (EV_IS_LINK_ACTION (self), NULL);

	return self->priv->params;
}

const gchar *
ev_link_action_get_name (EvLinkAction *self)
{
	g_return_val_if_fail (EV_IS_LINK_ACTION (self), NULL);

	return self->priv->name;
}

/**
 * ev_link_action_get_show_list:
 * @self: an #EvLinkAction
 *
 * Returns: (transfer none) (element-type EvLayer): a list of #EvLayer objects
 */
GList *
ev_link_action_get_show_list (EvLinkAction *self)
{
	g_return_val_if_fail (EV_IS_LINK_ACTION (self), NULL);

	return self->priv->show_list;
}

/**
 * ev_link_action_get_hide_list:
 * @self: an #EvLinkAction
 *
 * Returns: (transfer none) (element-type EvLayer): a list of #EvLayer objects
 */
GList *
ev_link_action_get_hide_list (EvLinkAction *self)
{
	g_return_val_if_fail (EV_IS_LINK_ACTION (self), NULL);

	return self->priv->hide_list;
}

/**
 * ev_link_action_get_toggle_list:
 * @self: an #EvLinkAction
 *
 * Returns: (transfer none) (element-type EvLayer): a list of #EvLayer objects
 */
GList *
ev_link_action_get_toggle_list (EvLinkAction *self)
{
	g_return_val_if_fail (EV_IS_LINK_ACTION (self), NULL);

	return self->priv->toggle_list;
}

static void
ev_link_action_get_property (GObject    *object,
			     guint       prop_id,
			     GValue     *value,
			     GParamSpec *param_spec)
{
	EvLinkAction *self;

	self = EV_LINK_ACTION (object);

	switch (prop_id) {
	        case PROP_TYPE:
		        g_value_set_enum (value, self->priv->type);
		        break;
	        case PROP_DEST:
		        g_value_set_object (value, self->priv->dest);
			break;
	        case PROP_URI:
			g_value_set_string (value, self->priv->uri);
			break;
	        case PROP_FILENAME:
			g_value_set_string (value, self->priv->filename);
			break;
	        case PROP_PARAMS:
			g_value_set_string (value, self->priv->params);
			break;
	        case PROP_NAME:
			g_value_set_string (value, self->priv->name);
			break;
	        case PROP_SHOW_LIST:
			g_value_set_pointer (value, self->priv->show_list);
			break;
	        case PROP_HIDE_LIST:
			g_value_set_pointer (value, self->priv->hide_list);
			break;
	        case PROP_TOGGLE_LIST:
			g_value_set_pointer (value, self->priv->toggle_list);
			break;
	        default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
							   prop_id,
							   param_spec);
			break;
	}
}

static void
ev_link_action_set_property (GObject      *object,
			     guint         prop_id,
			     const GValue *value,
			     GParamSpec   *param_spec)
{
	EvLinkAction *self = EV_LINK_ACTION (object);

	switch (prop_id) {
	        case PROP_TYPE:
			self->priv->type = g_value_get_enum (value);
			break;
	        case PROP_DEST:
			self->priv->dest = g_value_dup_object (value);
			break;
	        case PROP_URI:
			g_free (self->priv->uri);
			self->priv->uri = g_value_dup_string (value);
			break;
	        case PROP_FILENAME:
			g_free (self->priv->filename);
			self->priv->filename = g_value_dup_string (value);
			break;
	        case PROP_PARAMS:
			g_free (self->priv->params);
			self->priv->params = g_value_dup_string (value);
			break;
	        case PROP_NAME:
			g_free (self->priv->name);
			self->priv->name = g_value_dup_string (value);
			break;
	        case PROP_SHOW_LIST:
			self->priv->show_list = g_value_get_pointer (value);
			break;
	        case PROP_HIDE_LIST:
			self->priv->hide_list = g_value_get_pointer (value);
			break;
	        case PROP_TOGGLE_LIST:
			self->priv->toggle_list = g_value_get_pointer (value);
			break;
	        default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
							   prop_id,
							   param_spec);
			break;
	}
}

static void
ev_link_action_finalize (GObject *object)
{
	EvLinkActionPrivate *priv;

	priv = EV_LINK_ACTION (object)->priv;

	g_clear_object (&priv->dest);

	if (priv->uri) {
		g_free (priv->uri);
		priv->uri = NULL;
	}

	if (priv->filename) {
		g_free (priv->filename);
		priv->filename = NULL;
	}

	if (priv->params) {
		g_free (priv->params);
		priv->params = NULL;
	}

	if (priv->name) {
		g_free (priv->name);
		priv->name = NULL;
	}

	if (priv->show_list) {
		g_list_foreach (priv->show_list, (GFunc)g_object_unref, NULL);
		g_list_free (priv->show_list);
		priv->show_list = NULL;
	}

	if (priv->hide_list) {
		g_list_foreach (priv->hide_list, (GFunc)g_object_unref, NULL);
		g_list_free (priv->hide_list);
		priv->hide_list = NULL;
	}

	if (priv->toggle_list) {
		g_list_foreach (priv->toggle_list, (GFunc)g_object_unref, NULL);
		g_list_free (priv->toggle_list);
		priv->toggle_list = NULL;
	}

	G_OBJECT_CLASS (ev_link_action_parent_class)->finalize (object);
}

static void
ev_link_action_init (EvLinkAction *ev_link_action)
{
	ev_link_action->priv = EV_LINK_ACTION_GET_PRIVATE (ev_link_action);

	ev_link_action->priv->dest = NULL;
	ev_link_action->priv->uri = NULL;
	ev_link_action->priv->filename = NULL;
	ev_link_action->priv->params = NULL;
	ev_link_action->priv->name = NULL;
}

static void
ev_link_action_class_init (EvLinkActionClass *ev_link_action_class)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS (ev_link_action_class);

	g_object_class->set_property = ev_link_action_set_property;
	g_object_class->get_property = ev_link_action_get_property;

	g_object_class->finalize = ev_link_action_finalize;

	g_type_class_add_private (g_object_class, sizeof (EvLinkActionPrivate));

	g_object_class_install_property (g_object_class,
					 PROP_TYPE,
					 g_param_spec_enum  ("type",
							     "Action Type",
							     "The link action type",
							     EV_TYPE_LINK_ACTION_TYPE,
							     EV_LINK_ACTION_TYPE_GOTO_DEST,
							     G_PARAM_READWRITE |
							     G_PARAM_CONSTRUCT_ONLY |
                                                             G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_DEST,
					 g_param_spec_object ("dest",
							      "Action destination",
							      "The link action destination",
							      EV_TYPE_LINK_DEST,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_URI,
					 g_param_spec_string ("uri",
							      "Link Action URI",
							      "The link action URI",
							      NULL,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_FILENAME,
					 g_param_spec_string ("filename",
							      "Filename",
							      "The link action filename",
							      NULL,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_PARAMS,
					 g_param_spec_string ("params",
							      "Params",
							      "The link action params",
							      NULL,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_NAME,
					 g_param_spec_string ("name",
							      "Name",
							      "The link action name",
							      NULL,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_SHOW_LIST,
					 g_param_spec_pointer ("show-list",
							       "ShowList",
							       "The list of layers that should be shown",
							       G_PARAM_READWRITE |
							       G_PARAM_CONSTRUCT_ONLY |
                                                               G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_HIDE_LIST,
					 g_param_spec_pointer ("hide-list",
							       "HideList",
							       "The list of layers that should be hidden",
							       G_PARAM_READWRITE |
							       G_PARAM_CONSTRUCT_ONLY |
                                                               G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_TOGGLE_LIST,
					 g_param_spec_pointer ("toggle-list",
							       "ToggleList",
							       "The list of layers that should be toggled",
							       G_PARAM_READWRITE |
							       G_PARAM_CONSTRUCT_ONLY |
                                                               G_PARAM_STATIC_STRINGS));
}

EvLinkAction *
ev_link_action_new_dest (EvLinkDest *dest)
{
	return EV_LINK_ACTION (g_object_new (EV_TYPE_LINK_ACTION,
					     "dest", dest,
					     "type", EV_LINK_ACTION_TYPE_GOTO_DEST,
					     NULL));
}

EvLinkAction *
ev_link_action_new_remote (EvLinkDest  *dest,
			   const gchar *filename)
{
	return EV_LINK_ACTION (g_object_new (EV_TYPE_LINK_ACTION,
					     "dest", dest,
					     "filename", filename,
					     "type", EV_LINK_ACTION_TYPE_GOTO_REMOTE,
					     NULL));
}

EvLinkAction *
ev_link_action_new_external_uri (const gchar *uri)
{
	return EV_LINK_ACTION (g_object_new (EV_TYPE_LINK_ACTION,
					     "uri", uri,
					     "type", EV_LINK_ACTION_TYPE_EXTERNAL_URI,
					     NULL));
}

EvLinkAction *
ev_link_action_new_launch (const gchar *filename,
			   const gchar *params)
{
	return EV_LINK_ACTION (g_object_new (EV_TYPE_LINK_ACTION,
					     "filename", filename,
					     "params", params,
					     "type", EV_LINK_ACTION_TYPE_LAUNCH,
					     NULL));
}

EvLinkAction *
ev_link_action_new_named (const gchar *name)
{
	return EV_LINK_ACTION (g_object_new (EV_TYPE_LINK_ACTION,
					     "name", name,
					     "type", EV_LINK_ACTION_TYPE_NAMED,
					     NULL));
}

/**
 * ev_link_action_new_layers_state:
 * @show_list: (element-type EvLayer): a list of #EvLayer objects
 * @hide_list: (element-type EvLayer): a list of #EvLayer objects
 * @toggle_list: (element-type EvLayer): a list of #EvLayer objects
 *
 * Returns: (transfer full): a new #EvLinkAction
 */
EvLinkAction *
ev_link_action_new_layers_state (GList *show_list,
				 GList *hide_list,
				 GList *toggle_list)
{
	return EV_LINK_ACTION (g_object_new (EV_TYPE_LINK_ACTION,
					     "show-list", show_list,
					     "hide-list", hide_list,
					     "toggle-list", toggle_list,
					     "type", EV_LINK_ACTION_TYPE_LAYERS_STATE,
					     NULL));
}

/**
 * ev_link_action_equal:
 * @a: a #EvLinkAction
 * @b: a #EvLinkAction
 *
 * Checks whether @a and @b are equal.
 *
 * Returns: %TRUE iff @a and @b are equal
 *
 * Since: 3.8
 */
gboolean
ev_link_action_equal (EvLinkAction *a,
                      EvLinkAction *b)
{
        g_return_val_if_fail (EV_IS_LINK_ACTION (a), FALSE);
        g_return_val_if_fail (EV_IS_LINK_ACTION (b), FALSE);

        if (a == b)
                return TRUE;

        if (a->priv->type != b->priv->type)
                return FALSE;

        switch (a->priv->type) {
        case EV_LINK_ACTION_TYPE_GOTO_DEST:
                return ev_link_dest_equal (a->priv->dest, b->priv->dest);

        case EV_LINK_ACTION_TYPE_GOTO_REMOTE:
                return ev_link_dest_equal (a->priv->dest, b->priv->dest) &&
                        !g_strcmp0 (a->priv->filename, b->priv->filename);

        case EV_LINK_ACTION_TYPE_EXTERNAL_URI:
                return !g_strcmp0 (a->priv->uri, b->priv->uri);

        case EV_LINK_ACTION_TYPE_LAUNCH:
                return !g_strcmp0 (a->priv->filename, b->priv->filename) &&
                        !g_strcmp0 (a->priv->params, b->priv->params);

        case EV_LINK_ACTION_TYPE_NAMED:
                return !g_strcmp0 (a->priv->name, b->priv->name);

        default:
                return FALSE;
        }

        return FALSE;
}
