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
	PROP_TOGGLE_LIST,
	PROP_RESET_FIELDS,
	PROP_EXCLUDE_RESET_FIELDS
};

struct _EvLinkAction {
	GObject base_instance;
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
	GList            *reset_fields;
	gboolean          exclude_reset_fields;
};

typedef struct _EvLinkActionPrivate EvLinkActionPrivate;

#define GET_PRIVATE(o) ev_link_action_get_instance_private (o)

G_DEFINE_TYPE_WITH_PRIVATE (EvLinkAction, ev_link_action, G_TYPE_OBJECT)

EvLinkActionType
ev_link_action_get_action_type (EvLinkAction *self)
{
	g_return_val_if_fail (EV_IS_LINK_ACTION (self), 0);
	EvLinkActionPrivate *priv = GET_PRIVATE (self);

	return priv->type;
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
	EvLinkActionPrivate *priv = GET_PRIVATE (self);

	return priv->dest;
}

const gchar *
ev_link_action_get_uri (EvLinkAction *self)
{
	g_return_val_if_fail (EV_IS_LINK_ACTION (self), NULL);
	EvLinkActionPrivate *priv = GET_PRIVATE (self);

	return priv->uri;
}

const gchar *
ev_link_action_get_filename (EvLinkAction *self)
{
	g_return_val_if_fail (EV_IS_LINK_ACTION (self), NULL);
	EvLinkActionPrivate *priv = GET_PRIVATE (self);

	return priv->filename;
}

const gchar *
ev_link_action_get_params (EvLinkAction *self)
{
	g_return_val_if_fail (EV_IS_LINK_ACTION (self), NULL);
	EvLinkActionPrivate *priv = GET_PRIVATE (self);

	return priv->params;
}

const gchar *
ev_link_action_get_name (EvLinkAction *self)
{
	g_return_val_if_fail (EV_IS_LINK_ACTION (self), NULL);
	EvLinkActionPrivate *priv = GET_PRIVATE (self);

	return priv->name;
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
	EvLinkActionPrivate *priv = GET_PRIVATE (self);

	return priv->show_list;
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
	EvLinkActionPrivate *priv = GET_PRIVATE (self);

	return priv->hide_list;
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
	EvLinkActionPrivate *priv = GET_PRIVATE (self);

	return priv->toggle_list;
}

/**
 * ev_link_action_get_reset_fields:
 * @self: an #EvLinkAction
 *
 * Returns: (transfer none) (element-type gchar*): a list of fields to reset
 */
GList *
ev_link_action_get_reset_fields (EvLinkAction *self)
{
	g_return_val_if_fail (EV_IS_LINK_ACTION (self), NULL);
	EvLinkActionPrivate *priv = GET_PRIVATE (self);

	return priv->reset_fields;
}

/**
 * ev_link_action_get_exclude_reset_fields:
 * @self: an #EvLinkAction
 *
 * Returns: whether to exclude reset fields when resetting form
 */
gboolean
ev_link_action_get_exclude_reset_fields (EvLinkAction *self)
{
	g_return_val_if_fail (EV_IS_LINK_ACTION (self), FALSE);
	EvLinkActionPrivate *priv = GET_PRIVATE (self);

	return priv->exclude_reset_fields;
}

static void
ev_link_action_get_property (GObject    *object,
			     guint       prop_id,
			     GValue     *value,
			     GParamSpec *param_spec)
{
	EvLinkAction *self = EV_LINK_ACTION (object);
	EvLinkActionPrivate *priv = GET_PRIVATE (self);

	switch (prop_id) {
	        case PROP_TYPE:
		        g_value_set_enum (value, priv->type);
		        break;
	        case PROP_DEST:
		        g_value_set_object (value, priv->dest);
			break;
	        case PROP_URI:
			g_value_set_string (value, priv->uri);
			break;
	        case PROP_FILENAME:
			g_value_set_string (value, priv->filename);
			break;
	        case PROP_PARAMS:
			g_value_set_string (value, priv->params);
			break;
	        case PROP_NAME:
			g_value_set_string (value, priv->name);
			break;
	        case PROP_SHOW_LIST:
			g_value_set_pointer (value, priv->show_list);
			break;
	        case PROP_HIDE_LIST:
			g_value_set_pointer (value, priv->hide_list);
			break;
	        case PROP_TOGGLE_LIST:
			g_value_set_pointer (value, priv->toggle_list);
			break;
	        case PROP_RESET_FIELDS:
			g_value_set_pointer (value, priv->reset_fields);
			break;
	        case PROP_EXCLUDE_RESET_FIELDS:
			g_value_set_boolean (value, priv->exclude_reset_fields);
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
	EvLinkActionPrivate *priv = GET_PRIVATE (self);

	switch (prop_id) {
	        case PROP_TYPE:
			priv->type = g_value_get_enum (value);
			break;
	        case PROP_DEST:
			priv->dest = g_value_dup_object (value);
			break;
	        case PROP_URI:
			g_free (priv->uri);
			priv->uri = g_value_dup_string (value);
			break;
	        case PROP_FILENAME:
			g_free (priv->filename);
			priv->filename = g_value_dup_string (value);
			break;
	        case PROP_PARAMS:
			g_free (priv->params);
			priv->params = g_value_dup_string (value);
			break;
	        case PROP_NAME:
			g_free (priv->name);
			priv->name = g_value_dup_string (value);
			break;
	        case PROP_SHOW_LIST:
			priv->show_list = g_value_get_pointer (value);
			break;
	        case PROP_HIDE_LIST:
			priv->hide_list = g_value_get_pointer (value);
			break;
	        case PROP_TOGGLE_LIST:
			priv->toggle_list = g_value_get_pointer (value);
			break;
	        case PROP_RESET_FIELDS:
			priv->reset_fields = g_value_get_pointer (value);
			break;
	        case PROP_EXCLUDE_RESET_FIELDS:
			priv->exclude_reset_fields = g_value_get_boolean (value);
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
	EvLinkActionPrivate *priv = GET_PRIVATE (EV_LINK_ACTION (object));

	g_clear_object (&priv->dest);

	g_clear_pointer (&priv->uri, g_free);
	g_clear_pointer (&priv->filename, g_free);
	g_clear_pointer (&priv->params, g_free);
	g_clear_pointer (&priv->name, g_free);

	g_list_free_full (g_steal_pointer (&priv->show_list), g_object_unref);
	g_list_free_full (g_steal_pointer (&priv->hide_list), g_object_unref);
	g_list_free_full (g_steal_pointer (&priv->toggle_list), g_object_unref);
	g_list_free_full (g_steal_pointer (&priv->reset_fields), g_free);

	G_OBJECT_CLASS (ev_link_action_parent_class)->finalize (object);
}

static void
ev_link_action_init (EvLinkAction *ev_link_action)
{
}

static void
ev_link_action_class_init (EvLinkActionClass *ev_link_action_class)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS (ev_link_action_class);

	g_object_class->set_property = ev_link_action_set_property;
	g_object_class->get_property = ev_link_action_get_property;

	g_object_class->finalize = ev_link_action_finalize;

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
	g_object_class_install_property (g_object_class,
					 PROP_RESET_FIELDS,
					 g_param_spec_pointer ("reset-fields",
							       "ResetFields",
							       "The list of fields that should be/should not be reset",
							       G_PARAM_READWRITE |
							       G_PARAM_CONSTRUCT_ONLY |
							       G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (g_object_class,
					 PROP_EXCLUDE_RESET_FIELDS,
					 g_param_spec_boolean ("exclude-reset-fields",
							       "ExcludeResetFields",
							       "Whether to exclude/include reset-fields when resetting form",
							       FALSE,
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
 * ev_link_action_new_reset_form:
 * @fields: (element-type gchar*): a list of fields to reset
 * @exclude_fields: whether to exclude reset fields when resetting form
 *
 * Returns: (transfer full): a new #EvLinkAction
 */
EvLinkAction *
ev_link_action_new_reset_form (GList    *reset_fields,
			       gboolean  exclude_reset_fields)
{
	return EV_LINK_ACTION (g_object_new (EV_TYPE_LINK_ACTION,
					     "exclude-reset-fields", exclude_reset_fields,
					     "reset-fields", reset_fields,
					     "type", EV_LINK_ACTION_TYPE_RESET_FORM,
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
	EvLinkActionPrivate *a_priv = GET_PRIVATE (a);
	EvLinkActionPrivate *b_priv = GET_PRIVATE (b);

        if (a == b)
                return TRUE;

        if (a_priv->type != b_priv->type)
                return FALSE;

        switch (a_priv->type) {
        case EV_LINK_ACTION_TYPE_GOTO_DEST:
                return ev_link_dest_equal (a_priv->dest, b_priv->dest);

        case EV_LINK_ACTION_TYPE_GOTO_REMOTE:
                return ev_link_dest_equal (a_priv->dest, b_priv->dest) &&
                        !g_strcmp0 (a_priv->filename, b_priv->filename);

        case EV_LINK_ACTION_TYPE_EXTERNAL_URI:
                return !g_strcmp0 (a_priv->uri, b_priv->uri);

        case EV_LINK_ACTION_TYPE_LAUNCH:
                return !g_strcmp0 (a_priv->filename, b_priv->filename) &&
                        !g_strcmp0 (a_priv->params, b_priv->params);

        case EV_LINK_ACTION_TYPE_NAMED:
                return !g_strcmp0 (a_priv->name, b_priv->name);

        default:
                return FALSE;
        }

        return FALSE;
}
