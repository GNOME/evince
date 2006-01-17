/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors:
 *   James Willcox <jwillcox@cs.indiana.edu>
 *   Paolo Bacchilega <paobac@cvs.gnome.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs.h>
#ifndef USE_STABLE_LIBGNOMEUI
#include <libgnomeui/gnome-icon-theme.h>
#endif
#include <gconf/gconf-client.h>
#include "egg-recent-model.h"
#include "egg-recent-view.h"
#include "egg-recent-view-uimanager.h"
#include "egg-recent-util.h"
#include "egg-recent-item.h"

#define EGG_RECENT_NAME_PREFIX "EggRecentAction"
#define EGG_RECENT_ACTION "EggRecentFile"
#define EGG_RECENT_SEPARATOR (NULL)

#ifndef EGG_COMPILATION
#include <glib/gi18n.h>
#else
#define _(x) (x)
#define N_(x) (x)
#endif

#define DEFAULT_LABEL_WIDTH_CHARS 30

struct _EggRecentViewUIManager {
	GObject         parent_instance;

	GCallback       action_callback;
	gpointer        action_user_data;

	gboolean        leading_sep;
	gboolean        trailing_sep;

	GtkUIManager   *uimanager;
	GtkActionGroup *action_group;
	guint           merge_id;
	gulong          changed_cb_id;

	gchar          *path;

	gboolean        show_icons;
	gboolean        show_numbers;
#ifndef USE_STABLE_LIBGNOMEUI
	GnomeIconTheme *theme;
#endif

	EggUIManagerTooltipFunc tooltip_func;
	gpointer        tooltip_func_data;

	EggRecentModel *model;
	GConfClient    *client;
	GtkIconSize     icon_size;

	gint            label_width;
};


struct _EggRecentViewUIManagerMenuData {
	EggRecentViewUIManager *view;
	EggRecentItem *item;
};

typedef struct _EggRecentViewUIManagerMenuData EggRecentViewUIManagerMenuData;

enum {
	ACTIVATE,
	LAST_SIGNAL
};

/* GObject properties */
enum {
	PROP_BOGUS,
	PROP_UIMANAGER,
	PROP_PATH,
	PROP_SHOW_ICONS,
	PROP_SHOW_NUMBERS,
	PROP_LABEL_WIDTH
};

static guint view_signals[LAST_SIGNAL] = { 0 };

static void
connect_proxy_cb (GtkActionGroup         *action_group,
		  GtkAction              *action,
		  GtkWidget              *proxy,
		  EggRecentViewUIManager *view)
{
	if (GTK_IS_MENU_ITEM (proxy))
	{
		GtkWidget *label;

		label = GTK_BIN (proxy)->child;

		gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
		gtk_label_set_max_width_chars (GTK_LABEL (label), view->label_width);
	}
}

static void
egg_recent_view_uimanager_clear (EggRecentViewUIManager *view)
{
	if (view->merge_id != 0) {
		gtk_ui_manager_remove_ui (view->uimanager, view->merge_id);
		view->merge_id = 0;
	}

	if (view->action_group != NULL) {
		gtk_ui_manager_remove_action_group (view->uimanager, view->action_group);
		g_object_unref (view->action_group);
		view->action_group = NULL;
	}

	gtk_ui_manager_ensure_update (view->uimanager);
}

static void
egg_recent_view_uimanager_set_list (EggRecentViewUIManager *view, GList *list)
{
	GList  *scan;
	guint   index = 1;

	g_return_if_fail (view);

	egg_recent_view_uimanager_clear (view);

	if (view->merge_id == 0)
		view->merge_id = gtk_ui_manager_new_merge_id (view->uimanager);

	if (view->action_group == NULL) {
		gchar *group = g_strdup_printf ("EggRecentActions%u", 
						view->merge_id);
		view->action_group = gtk_action_group_new (group);
		g_signal_connect (view->action_group, "connect-proxy",
				  G_CALLBACK (connect_proxy_cb), view);
		gtk_ui_manager_insert_action_group (view->uimanager,
						    view->action_group, -1);
		g_free (group);
	}

	if (view->leading_sep) {
		gchar *action = g_strdup_printf ("EggRecentLeadingSeparator%u",
						 view->merge_id);
		gtk_ui_manager_add_ui (view->uimanager, 
				       view->merge_id, 
				       view->path,
				       action,
				       EGG_RECENT_SEPARATOR,
				       GTK_UI_MANAGER_AUTO, 
				       FALSE);
		g_free (action);
	}

	for (scan = list; scan; scan = scan->next, index++) {
		EggRecentItem *item = scan->data;
		GtkAction     *action;
		gchar         *name;
		gchar         *uri;
		gchar         *basename;
		gchar         *escaped;
		gchar         *label;
		gchar         *tooltip = NULL;

		uri = egg_recent_item_get_uri_for_display (item);
		if (uri == NULL)
			continue;

		name = g_strdup_printf (EGG_RECENT_NAME_PREFIX"%u-%u", 
					view->merge_id,
					index);

		if (view->tooltip_func != NULL)
			tooltip = (*view->tooltip_func) (item, view->tooltip_func_data);

		if (!tooltip)
			tooltip = g_strdup_printf (_("Open '%s'"), uri);

		basename = egg_recent_item_get_short_name (item);
		escaped = egg_recent_util_escape_underlines (basename);
		g_free (basename);
		g_free (uri);

		if (view->show_numbers) {
			if (index >= 10)
				label = g_strdup_printf ("%d.  %s", 
							 index, 
							 escaped);
			else
				label = g_strdup_printf ("_%d.  %s", 
							 index, 
							 escaped);
			g_free (escaped);
		} else 
			label = escaped;

		action = g_object_new (GTK_TYPE_ACTION,
				       "name", name,
				       "label", label,
				       (view->show_icons)? "stock_id": NULL, 
				       GTK_STOCK_OPEN,
				       NULL);
		if (tooltip != NULL) {
			g_object_set (action, "tooltip", tooltip, NULL);
			g_free (tooltip);
		}
		egg_recent_item_ref (item);
		g_object_set_data_full (G_OBJECT (action), 
					"egg_recent_uri", 
					item, 
					(GFreeFunc) egg_recent_item_unref);

		if (view->action_callback != NULL) {
			GClosure *closure;
 			closure = g_cclosure_new (view->action_callback, view->action_user_data, NULL);
			g_signal_connect_closure (action, "activate", closure, FALSE);
		}

		gtk_action_group_add_action (view->action_group, action);
		g_object_unref (action);

		gtk_ui_manager_add_ui (view->uimanager, 
				       view->merge_id, 
				       view->path,
				       name,
				       name,
				       GTK_UI_MANAGER_AUTO, 
				       FALSE);

		g_free (name);
		g_free (label);
	}

	if (view->trailing_sep) {
		gchar *action = g_strdup_printf ("EggRecentTrailingSeparator%u",
						view->merge_id);
		gtk_ui_manager_add_ui (view->uimanager, 
				       view->merge_id, 
				       view->path,
				       action,
				       EGG_RECENT_SEPARATOR,
				       GTK_UI_MANAGER_AUTO, 
				       FALSE);
		g_free (action);
	}
}

static void
egg_recent_view_uimanager_set_empty_list (EggRecentViewUIManager *view)
{
	gboolean   is_embedded;

	g_return_if_fail (view);

	egg_recent_view_uimanager_clear (view);

	if (view->merge_id == 0)
		view->merge_id = gtk_ui_manager_new_merge_id (view->uimanager);

	if (view->action_group == NULL) {
		gchar *group = g_strdup_printf ("EggRecentActions%u", 
						view->merge_id);
		view->action_group = gtk_action_group_new (group);
		g_signal_connect (view->action_group, "connect-proxy",
				  G_CALLBACK (connect_proxy_cb), view);
		gtk_ui_manager_insert_action_group (view->uimanager,
						    view->action_group, -1);
		g_free (group);
	}

	if (view->leading_sep) {
		gchar *sep_action = g_strdup_printf ("EggRecentLeadingSeparator%u",
						     view->merge_id);
		gtk_ui_manager_add_ui (view->uimanager, 
				       view->merge_id, 
				       view->path,
				       sep_action,
				       EGG_RECENT_SEPARATOR,
				       GTK_UI_MANAGER_AUTO, 
				       FALSE);
		g_free (sep_action);
	}

	is_embedded = (view->leading_sep && view->trailing_sep);

	if (is_embedded) {
		GtkAction *action;
		gchar     *name;

		name = g_strdup_printf (EGG_RECENT_NAME_PREFIX "%u-0", view->merge_id);

		action = g_object_new (GTK_TYPE_ACTION,
				       "name", name,
				       "label", _("Empty"),
				       "sensitive", FALSE,
				       NULL);
		
		gtk_action_group_add_action (view->action_group, action);
		g_object_unref (action);

		gtk_ui_manager_add_ui (view->uimanager, 
				       view->merge_id, 
				       view->path,
				       name,
				       name,
				       GTK_UI_MANAGER_AUTO, 
				       FALSE);

		g_free (name);
	}

	if (view->trailing_sep) {
		gchar *sep_action = g_strdup_printf ("EggRecentTrailingSeparator%u",
						     view->merge_id);
		gtk_ui_manager_add_ui (view->uimanager, 
				       view->merge_id, 
				       view->path,
				       sep_action,
				       EGG_RECENT_SEPARATOR,
				       GTK_UI_MANAGER_AUTO, 
				       FALSE);
		g_free (sep_action);
	}
}

static void
model_changed_cb (EggRecentModel         *model,  
		  GList                  *list, 
		  EggRecentViewUIManager *view)
{
	if (list != NULL)
		egg_recent_view_uimanager_set_list (view, list);
	else
		egg_recent_view_uimanager_set_empty_list (view);

	gtk_ui_manager_ensure_update (view->uimanager);
}

static EggRecentModel *
egg_recent_view_uimanager_get_model (EggRecentView *view_parent)
{
	EggRecentViewUIManager *view;
	
	g_return_val_if_fail (view_parent != NULL, NULL);
	view = EGG_RECENT_VIEW_UIMANAGER (view_parent);
	return view->model;
}

static void
egg_recent_view_uimanager_set_model (EggRecentView  *view_parent,
				     EggRecentModel *model)
{
	EggRecentViewUIManager *view;
	
	g_return_if_fail (view_parent != NULL);
	view = EGG_RECENT_VIEW_UIMANAGER (view_parent);

	if (view->model != NULL) {
		if (view->changed_cb_id != 0)
			g_signal_handler_disconnect (G_OBJECT (view->model),
						     view->changed_cb_id);
		g_object_unref (view->model);
	}
	
	view->model = model;
	g_object_ref (view->model);

	view->changed_cb_id = g_signal_connect_object (G_OBJECT (model),
						       "changed",
						       G_CALLBACK (model_changed_cb),
						       view, 0);

	egg_recent_model_changed (view->model);
}

void
egg_recent_view_uimanager_set_leading_sep (EggRecentViewUIManager *view, 
					   gboolean                val)
{
	view->leading_sep = val;
	egg_recent_view_uimanager_clear (view);
	if (view->model)
		egg_recent_model_changed (view->model);
}

void
egg_recent_view_uimanager_set_trailing_sep (EggRecentViewUIManager *view,
					    gboolean                val)
{
	view->trailing_sep = val;
	egg_recent_view_uimanager_clear (view);
	if (view->model)
		egg_recent_model_changed (view->model);
}

static void
egg_recent_view_uimanager_set_property (GObject      *object,
					guint         prop_id,
					const GValue *value,
					GParamSpec   *pspec)
{
	EggRecentViewUIManager *view = EGG_RECENT_VIEW_UIMANAGER (object);

	switch (prop_id) {
	case PROP_UIMANAGER:
		egg_recent_view_uimanager_set_uimanager (view, (GtkUIManager*)g_value_get_object (value));
		break;
	case PROP_PATH:
		egg_recent_view_uimanager_set_path (view, g_value_get_string (value));
		break;
	case PROP_SHOW_ICONS:
		egg_recent_view_uimanager_show_icons (view, g_value_get_boolean (value));
		break;
	case PROP_SHOW_NUMBERS:
		egg_recent_view_uimanager_show_numbers (view, g_value_get_boolean (value));
		break;
	case PROP_LABEL_WIDTH:
		egg_recent_view_uimanager_set_label_width (view, g_value_get_int (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
egg_recent_view_uimanager_get_property (GObject    *object,
					guint       prop_id,
					GValue     *value,
					GParamSpec *pspec)
{
	EggRecentViewUIManager *view = EGG_RECENT_VIEW_UIMANAGER (object);

	switch (prop_id) {
	case PROP_UIMANAGER:
		g_value_set_object (value, view->uimanager);
		break;
	case PROP_PATH:
		g_value_set_string (value, view->path);
		break;
	case PROP_SHOW_ICONS:
		g_value_set_boolean (value, view->show_icons);
		break;
	case PROP_SHOW_NUMBERS:
		g_value_set_boolean (value, view->show_numbers);
		break;
	case PROP_LABEL_WIDTH:
		g_value_set_int (value, view->label_width);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
egg_recent_view_uimanager_finalize (GObject *object)
{
	EggRecentViewUIManager *view = EGG_RECENT_VIEW_UIMANAGER (object);

	if (view->changed_cb_id != 0) {
		g_signal_handler_disconnect (G_OBJECT (view->model),
					     view->changed_cb_id);
		view->changed_cb_id = 0;
	}

	g_free (view->path);

	egg_recent_view_uimanager_clear (view);

	if (view->action_group != NULL) {
		g_object_unref (view->action_group);
		view->action_group = NULL;
	}

	if (view->uimanager != NULL) {
		g_object_unref (view->uimanager);
		view->uimanager = NULL;
	}

	if (view->model != NULL) {
		g_object_unref (view->model);
		view->model = NULL;
	}

#ifndef USE_STABLE_LIBGNOMEUI
	if (view->theme != NULL) {
		g_object_unref (view->theme);
		view->theme = NULL;
	}
#endif

	if (view->client != NULL) {
		g_object_unref (view->client);
		view->client = NULL;
	}
}

static void
egg_recent_view_uimanager_class_init (EggRecentViewUIManagerClass * klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = egg_recent_view_uimanager_set_property;
	object_class->get_property = egg_recent_view_uimanager_get_property;
	object_class->finalize     = egg_recent_view_uimanager_finalize;

	view_signals[ACTIVATE] = g_signal_new ("activate",
					       G_OBJECT_CLASS_TYPE (object_class),
					       G_SIGNAL_RUN_LAST,
					       G_STRUCT_OFFSET (EggRecentViewUIManagerClass, activate),
					       NULL, NULL,
					       g_cclosure_marshal_VOID__BOXED,
					       G_TYPE_NONE, 1,
					       EGG_TYPE_RECENT_ITEM);

	g_object_class_install_property (object_class,
					 PROP_UIMANAGER,
					 g_param_spec_object ("uimanager",
						 	      "UI Manager",
							      "The UI manager this object will use to update.the UI",
							      GTK_TYPE_UI_MANAGER,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_PATH,
					 g_param_spec_string ("path",
						 	      "Path",
							      "The UI path this object will update.",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_SHOW_ICONS,
					 g_param_spec_boolean ("show-icons",
							       "Show Icons",
							       "Whether or not to show icons",
							       FALSE,
							       G_PARAM_READWRITE));
	
	g_object_class_install_property (object_class,
					 PROP_SHOW_NUMBERS,
					 g_param_spec_boolean ("show-numbers",
							       "Show Numbers",
							       "Whether or not to show numbers",
							       TRUE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_LABEL_WIDTH,
					 g_param_spec_int ("label-width",
						 	   "Label Width",
							   "The desired width of the menu label, in characters",
							   -1,
							   G_MAXINT,
							   DEFAULT_LABEL_WIDTH_CHARS,
							   G_PARAM_READWRITE));
	

	klass->activate = NULL;
}

static void
egg_recent_view_init (EggRecentViewClass *iface)
{
	iface->do_get_model = egg_recent_view_uimanager_get_model;
	iface->do_set_model = egg_recent_view_uimanager_set_model;
}

static void
show_menus_changed_cb (GConfClient            *client,
		       guint                   cnxn_id,
		       GConfEntry             *entry,
		       EggRecentViewUIManager *view)
{
	GConfValue *value;

	value = gconf_entry_get_value (entry);
	g_return_if_fail (value->type == GCONF_VALUE_BOOL);

	egg_recent_view_uimanager_show_icons (view, gconf_value_get_bool (value));
}

#ifndef USE_STABLE_LIBGNOMEUI
static void
theme_changed_cb (GnomeIconTheme         *theme, 
		  EggRecentViewUIManager *view)
{
	if (view->model != NULL)
		egg_recent_model_changed (view->model);
}
#endif

static void
egg_recent_view_uimanager_init (EggRecentViewUIManager * view)
{
	view->client = gconf_client_get_default ();

	view->show_icons = gconf_client_get_bool (view->client,
						  "/desktop/gnome/interface/menus_have_icons",
						  NULL);
	
	gconf_client_add_dir (view->client, "/desktop/gnome/interface",
			      GCONF_CLIENT_PRELOAD_NONE,
			      NULL);
	gconf_client_notify_add (view->client,
				 "/desktop/gnome/interface/menus_have_icons",
				 (GConfClientNotifyFunc)show_menus_changed_cb,
				 view, NULL, NULL);


	view->leading_sep = FALSE;
	view->trailing_sep = FALSE;
	view->show_numbers = TRUE;

	view->uimanager = NULL;
	view->action_group = NULL;
	view->merge_id = 0;
	view->changed_cb_id = 0;

	view->path = NULL;

#ifndef USE_STABLE_LIBGNOMEUI
	view->theme = gnome_icon_theme_new ();
	gnome_icon_theme_set_allow_svg (view->theme, TRUE);
	g_signal_connect_object (view->theme, "changed",
				 G_CALLBACK (theme_changed_cb), view, 0);
#endif

	view->tooltip_func = NULL;
	view->tooltip_func_data = NULL;

	view->icon_size = GTK_ICON_SIZE_MENU;
	view->label_width = DEFAULT_LABEL_WIDTH_CHARS;
}

void
egg_recent_view_uimanager_set_icon_size (EggRecentViewUIManager *view,
					 GtkIconSize             icon_size)
{
	if (view->icon_size != icon_size) {
		view->icon_size = icon_size;
		egg_recent_model_changed (view->model);
	} else {
		view->icon_size = icon_size;
	}
}

GtkIconSize
egg_recent_view_uimanager_get_icon_size (EggRecentViewUIManager *view)
{
	return view->icon_size;
}

void
egg_recent_view_uimanager_show_icons (EggRecentViewUIManager *view,
				      gboolean                show)
{
	view->show_icons = show;
	if (view->model != NULL)
		egg_recent_model_changed (view->model);
}

void
egg_recent_view_uimanager_show_numbers (EggRecentViewUIManager *view, 
					gboolean                show)
{
	view->show_numbers = show;
	if (view->model != NULL)
		egg_recent_model_changed (view->model);
}

void
egg_recent_view_uimanager_set_tooltip_func (EggRecentViewUIManager   *view,
					    EggUIManagerTooltipFunc   func,
					    gpointer                  user_data)
{
	view->tooltip_func = func;
	view->tooltip_func_data = user_data;
	if (view->model)
		egg_recent_model_changed (view->model);
}

/**
 * egg_recent_view_uimanager_set_uimanager:
 * @view: A EggRecentViewUIManager object.
 * @uimanager: The ui manager used to put the menu items in.
 *
 * Use this function to change the ui manager used to update the menu.
 *
 */
void
egg_recent_view_uimanager_set_uimanager (EggRecentViewUIManager *view,
					 GtkUIManager           *uimanager)
{
	g_return_if_fail (EGG_IS_RECENT_VIEW_UIMANAGER (view));
	g_return_if_fail (uimanager != NULL);

	if (view->uimanager != NULL)
		g_object_unref (view->uimanager);
	view->uimanager = uimanager;
	g_object_ref (view->uimanager);
}

/**
 * egg_recent_view_uimanager_get_uimanager:
 * @view: A EggRecentViewUIManager object.
 *
 */
GtkUIManager*
egg_recent_view_uimanager_get_uimanager (EggRecentViewUIManager *view)
{
	g_return_val_if_fail (EGG_IS_RECENT_VIEW_UIMANAGER (view), NULL);
	return view->uimanager;
}

/**
 * egg_recent_view_uimanager_set_path:
 * @view: A EggRecentViewUIManager object.
 * @path: The path to put the menu items in.
 *
 * Use this function to change the path where the recent
 * documents appear in.
 *
 */
void
egg_recent_view_uimanager_set_path (EggRecentViewUIManager *view,
				    const gchar            *path)
{
	g_return_if_fail (EGG_IS_RECENT_VIEW_UIMANAGER (view));
	g_return_if_fail (path);

	g_free (view->path);
	view->path = g_strdup (path);
}

/**
 * egg_recent_view_uimanager_get_path:
 * @view: A EggRecentViewUIManager object.
 *
 */
G_CONST_RETURN gchar*
egg_recent_view_uimanager_get_path (EggRecentViewUIManager *view)
{
	g_return_val_if_fail (EGG_IS_RECENT_VIEW_UIMANAGER (view), NULL);
	return view->path;
}

void
egg_recent_view_uimanager_set_label_width (EggRecentViewUIManager *view,
					   gint                    chars)
{
	g_return_if_fail (EGG_IS_RECENT_VIEW_UIMANAGER (view));
	view->label_width = chars;
}

gint
egg_recent_view_uimanager_get_label_width (EggRecentViewUIManager *view)
{
	g_return_val_if_fail (EGG_IS_RECENT_VIEW_UIMANAGER (view), DEFAULT_LABEL_WIDTH_CHARS);
	return view->label_width;
}

void
egg_recent_view_uimanager_set_action_func (EggRecentViewUIManager   *view,
					   GCallback                 callback,
					   gpointer                  user_data)
{
	g_return_if_fail (EGG_IS_RECENT_VIEW_UIMANAGER (view));
	view->action_callback = callback;
	view->action_user_data = user_data;
}

/**
 * egg_recent_view_uimanager_new:
 * @appname: The name of your application.
 * @limit:  The maximum number of items allowed.
 *
 * This creates a new EggRecentViewUIManager object.
 *
 * Returns: a EggRecentViewUIManager object
 */
EggRecentViewUIManager *
egg_recent_view_uimanager_new (GtkUIManager  *uimanager,
			       const gchar   *path,
			       GCallback      callback,
			       gpointer       user_data)
{
	GObject *view;

	g_return_val_if_fail (uimanager, NULL);
	g_return_val_if_fail (path, NULL);

	view = g_object_new (egg_recent_view_uimanager_get_type (),
			     "uimanager", uimanager,
			     "path", path,
			     NULL);

	g_return_val_if_fail (view, NULL);
	
	egg_recent_view_uimanager_set_action_func (EGG_RECENT_VIEW_UIMANAGER (view),
						   callback,
						   user_data);

	return EGG_RECENT_VIEW_UIMANAGER (view);
}

GType
egg_recent_view_uimanager_get_type (void)
{
	static GType egg_recent_view_uimanager_type = 0;

	if(!egg_recent_view_uimanager_type) {
		static const GTypeInfo egg_recent_view_uimanager_info = {
			sizeof (EggRecentViewUIManagerClass),
			NULL, /* base init */
			NULL, /* base finalize */
			(GClassInitFunc)egg_recent_view_uimanager_class_init, /* class init */
			NULL, /* class finalize */
			NULL, /* class data */
			sizeof (EggRecentViewUIManager),
			0,
			(GInstanceInitFunc) egg_recent_view_uimanager_init
		};

		static const GInterfaceInfo view_info =
		{
			(GInterfaceInitFunc) egg_recent_view_init,
			NULL,
			NULL
		};

		egg_recent_view_uimanager_type = g_type_register_static (G_TYPE_OBJECT,
									 "EggRecentViewUIManager",
									 &egg_recent_view_uimanager_info, 0);
		g_type_add_interface_static (egg_recent_view_uimanager_type,
					     EGG_TYPE_RECENT_VIEW,
					     &view_info);
	}

	return egg_recent_view_uimanager_type;
}

EggRecentItem*
egg_recent_view_uimanager_get_item (EggRecentViewUIManager   *view,
				    GtkAction                *action)
{
	return g_object_get_data (G_OBJECT(action), "egg_recent_uri");
}
