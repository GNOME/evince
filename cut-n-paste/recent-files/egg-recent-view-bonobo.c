/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/**
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <libbonoboui.h>
#include <libgnomevfs/gnome-vfs.h>
#ifndef USE_STABLE_LIBGNOMEUI
#include <libgnomeui/gnome-icon-theme.h>
#endif
#include <gconf/gconf-client.h>
#include "egg-recent-model.h"
#include "egg-recent-view.h"
#include "egg-recent-view-bonobo.h"
#include "egg-recent-util.h"
#include "egg-recent-item.h"

struct _EggRecentViewBonobo {
	GObject parent_instance;	/* We emit signals */

	BonoboUIComponent *uic;
	gchar *path;			/* The menu path where our stuff
					 *  will go
					 */

	gulong changed_cb_id;

	gchar *uid;			/* unique id used for the verb name */

	gboolean show_icons;
	gboolean show_numbers;
#ifndef USE_STABLE_LIBGNOMEUI
	GnomeIconTheme *theme;
#endif
	EggRecentViewBonoboTooltipFunc tooltip_func;
	gpointer tooltip_func_data;

	EggRecentModel *model;
	GConfClient *client;
	GtkIconSize icon_size;
};


struct _EggRecentViewBonoboMenuData {
	EggRecentViewBonobo *view;
	EggRecentItem *item;
};

typedef struct _EggRecentViewBonoboMenuData EggRecentViewBonoboMenuData;

enum {
	ACTIVATE,
	LAST_SIGNAL
};

/* GObject properties */
enum {
	PROP_BOGUS,
	PROP_UI_COMPONENT,
	PROP_MENU_PATH,
	PROP_SHOW_ICONS,
	PROP_SHOW_NUMBERS
};

static guint egg_recent_view_bonobo_signals[LAST_SIGNAL] = { 0 };

static void
egg_recent_view_bonobo_clear (EggRecentViewBonobo *view)
{
	gint i=1;
	gboolean done=FALSE;
	EggRecentModel *model;

	g_return_if_fail (view->uic);

	model = egg_recent_view_get_model (EGG_RECENT_VIEW (view));
	
	while (!done)
	{
		gchar *verb_name = g_strdup_printf ("%s-%d", view->uid, i);
		gchar *item_path = g_strconcat (view->path, "/", verb_name, NULL);
		if (bonobo_ui_component_path_exists (view->uic, item_path, NULL))
			bonobo_ui_component_rm (view->uic, item_path, NULL);
		else
			done=TRUE;

		g_free (item_path);
		g_free (verb_name);

		i++;
	}
}

static void
egg_recent_view_bonobo_menu_cb (BonoboUIComponent *uic, gpointer data, const char *cname)
{
	EggRecentViewBonoboMenuData *md = (EggRecentViewBonoboMenuData *) data;
	EggRecentItem *item;

	g_return_if_fail (md);
	g_return_if_fail (md->item);
	g_return_if_fail (md->view);
	g_return_if_fail (EGG_IS_RECENT_VIEW_BONOBO (md->view));

	item = md->item;
	egg_recent_item_ref (item);

	g_signal_emit (G_OBJECT(md->view),
		       egg_recent_view_bonobo_signals[ACTIVATE], 0,
		       item);

	egg_recent_item_unref (item);
}

static void
egg_recent_view_bonobo_menu_data_destroy_cb (gpointer data, GClosure *closure)
{
	EggRecentViewBonoboMenuData *md = data;

	egg_recent_item_unref (md->item);
	g_free (md);
}


static void
egg_recent_view_bonobo_set_list (EggRecentViewBonobo *view, GList *list)
{
	BonoboUIComponent* ui_component;
	unsigned int i;
	gchar *label = NULL;
	gchar *verb_name = NULL;
	gchar *tip = NULL;
	gchar *escaped_name = NULL;
	gchar *item_path = NULL;
	gchar *base_uri;
	gchar *utf8_uri;
	gchar *cmd;
	gchar *xml_escaped_name;
	EggRecentViewBonoboMenuData *md;
	EggRecentModel *model;
	GClosure *closure;

	g_return_if_fail (view);

	ui_component = view->uic;
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (ui_component));


	model = egg_recent_view_get_model (EGG_RECENT_VIEW (view));

	egg_recent_view_bonobo_clear (view);

	
	bonobo_ui_component_freeze (ui_component, NULL);

	for (i = 1; i <= g_list_length (list); ++i)
	{
		EggRecentItem *item = (EggRecentItem *)g_list_nth_data (list, i-1);	

		utf8_uri = egg_recent_item_get_uri_for_display (item);
		if (utf8_uri == NULL)
			continue;
		
		/* this is what gets passed to our private "activate" callback */
		md = (EggRecentViewBonoboMenuData *)g_malloc (sizeof (EggRecentViewBonoboMenuData));
		md->view = view;
		md->item = item;

		egg_recent_item_ref (md->item);

		base_uri = g_path_get_basename (utf8_uri);
		xml_escaped_name = g_markup_escape_text (base_uri,
							 strlen (base_uri));
	
		escaped_name = egg_recent_util_escape_underlines (xml_escaped_name);
		g_free (xml_escaped_name);

		tip = NULL;
		if (view->tooltip_func != NULL) {
			gchar *tmp_tip;
			tip = view->tooltip_func (item,
						  view->tooltip_func_data);
			tmp_tip = g_markup_escape_text (tip, strlen (tip));
			g_free (tip);
			tip = tmp_tip;
		}

		if (tip == NULL)
			tip = g_strdup ("");

		verb_name = g_strdup_printf ("%s-%d", view->uid, i);

		if (view->show_icons) {
			GdkPixbuf *pixbuf;
			gchar *mime_type;
			gchar *uri;

			mime_type = egg_recent_item_get_mime_type (item);
			uri = egg_recent_item_get_uri (item);
#ifndef USE_STABLE_LIBGNOMEUI
			{
				int width, height;

				gtk_icon_size_lookup_for_settings
					(gtk_settings_get_default (),
					 view->icon_size,
					 &width, &height);
				pixbuf = egg_recent_util_get_icon
							(view->theme,
							 uri, mime_type,
							 height);
			}
#else
			pixbuf = NULL;
#endif


			if (pixbuf != NULL) {
				gchar *pixbuf_xml;

				/* Riiiiight.... */
				pixbuf_xml = bonobo_ui_util_pixbuf_to_xml (pixbuf);
				
				cmd = g_strdup_printf ("<cmd name=\"%s\" pixtype=\"pixbuf\" pixname=\"%s\"/>", verb_name, pixbuf_xml);

				g_free (pixbuf_xml);
				g_object_unref (pixbuf);
			} else {
				cmd = g_strdup_printf ("<cmd name=\"%s\"/> ",
					       	       verb_name);
			}

			g_free (mime_type);
			g_free (uri);
		} else
			cmd = g_strdup_printf ("<cmd name=\"%s\"/> ",
					       verb_name);
		bonobo_ui_component_set_translate (ui_component, "/commands/", cmd, NULL);

		closure = g_cclosure_new (G_CALLBACK (egg_recent_view_bonobo_menu_cb),
					  md, egg_recent_view_bonobo_menu_data_destroy_cb);
					  
		bonobo_ui_component_add_verb_full (ui_component, verb_name,
						   closure); 
	        
		if (view->show_numbers) {
			if (i < 10)
				label = g_strdup_printf ("_%d. %s", i,
							 escaped_name);
			else
				label = g_strdup_printf ("%d. %s", i, escaped_name);
		} else {
			label = g_strdup (escaped_name);
		}
			
		
		
		item_path = g_strconcat (view->path, "/", verb_name, NULL);

		if (bonobo_ui_component_path_exists (ui_component, item_path, NULL))
		{
			bonobo_ui_component_set_prop (ui_component, item_path, 
					              "label", label, NULL);

			bonobo_ui_component_set_prop (ui_component, item_path, 
					              "tip", tip, NULL);
		}
		else
		{
			gchar *xml;
			
			xml = g_strdup_printf ("<menuitem name=\"%s\" "
						"verb=\"%s\""
						" _label=\"%s\"  _tip=\"%s\" "
						"hidden=\"0\" />", 
						verb_name, verb_name, label,
						tip);

			bonobo_ui_component_set_translate (ui_component, view->path, xml, NULL);

			g_free (xml); 
		}
		
		g_free (label);
		g_free (verb_name);
		g_free (tip);
		g_free (escaped_name);
		g_free (item_path);
		g_free (utf8_uri);
		g_free (base_uri);
		g_free (cmd);

	}


	bonobo_ui_component_thaw (ui_component, NULL);
}

static void
model_changed_cb (EggRecentModel *model, GList *list, EggRecentViewBonobo *view)
{
	if (list != NULL)
		egg_recent_view_bonobo_set_list (view, list);
	else
		egg_recent_view_bonobo_clear (view);
}


static EggRecentModel *
egg_recent_view_bonobo_get_model (EggRecentView *view_parent)
{
	EggRecentViewBonobo *view;
	
	g_return_val_if_fail (view_parent, NULL);
	view = EGG_RECENT_VIEW_BONOBO (view_parent);
	
	return view->model;
}

static void
egg_recent_view_bonobo_set_model (EggRecentView *view_parent, EggRecentModel *model)
{
	EggRecentViewBonobo *view;
	
	g_return_if_fail (view_parent);
	view = EGG_RECENT_VIEW_BONOBO (view_parent);
	
	if (view->model)
		g_signal_handler_disconnect (G_OBJECT (view->model),
					     view->changed_cb_id);
	
	view->model = model;
	g_object_ref (view->model);
	view->changed_cb_id = g_signal_connect_object (G_OBJECT (model),
						"changed",
						G_CALLBACK (model_changed_cb),
						view, 0);

	egg_recent_model_changed (view->model);
}

static void
egg_recent_view_bonobo_set_property (GObject *object,
			   guint prop_id,
			   const GValue *value,
			   GParamSpec *pspec)
{
	EggRecentViewBonobo *view = EGG_RECENT_VIEW_BONOBO (object);

	switch (prop_id)
	{
		case PROP_UI_COMPONENT:
			egg_recent_view_bonobo_set_ui_component (EGG_RECENT_VIEW_BONOBO (view),
						       BONOBO_UI_COMPONENT (g_value_get_object (value)));
		break;
		case PROP_MENU_PATH:
			view->path = g_strdup (g_value_get_string (value));
		break;
		case PROP_SHOW_ICONS:
			egg_recent_view_bonobo_show_icons (view,
						g_value_get_boolean (value));
		default:
		case PROP_SHOW_NUMBERS:
			egg_recent_view_bonobo_show_numbers (view,
						g_value_get_boolean (value));
		break;
		break;
	}
}

static void
egg_recent_view_bonobo_get_property (GObject *object,
			   guint prop_id,
			   GValue *value,
			   GParamSpec *pspec)
{
	EggRecentViewBonobo *view = EGG_RECENT_VIEW_BONOBO (object);

	switch (prop_id)
	{
		case PROP_UI_COMPONENT:
			g_value_set_pointer (value, view->uic);
		break;
		case PROP_MENU_PATH:
			g_value_set_string (value, g_strdup (view->path));
		break;
		case PROP_SHOW_ICONS:
			g_value_set_boolean (value, view->show_icons);
		break;
		case PROP_SHOW_NUMBERS:
			g_value_set_boolean (value, view->show_numbers);
		break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
egg_recent_view_bonobo_finalize (GObject *object)
{
	EggRecentViewBonobo *view = EGG_RECENT_VIEW_BONOBO (object);

	g_free (view->path);
	g_free (view->uid);

	g_object_unref (view->model);
	g_object_unref (view->uic);
#ifndef USE_STABLE_LIBGNOMEUI
	g_object_unref (view->theme);
#endif
	g_object_unref (view->client);
}

static void
egg_recent_view_bonobo_class_init (EggRecentViewBonoboClass * klass)
{
	GObjectClass *object_class;

	
	object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = egg_recent_view_bonobo_set_property;
	object_class->get_property = egg_recent_view_bonobo_get_property;
	object_class->finalize     = egg_recent_view_bonobo_finalize;

	egg_recent_view_bonobo_signals[ACTIVATE] = g_signal_new ("activate",
			G_OBJECT_CLASS_TYPE (object_class),
			G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET (EggRecentViewBonoboClass, activate),
			NULL, NULL,
			g_cclosure_marshal_VOID__BOXED,
			G_TYPE_NONE, 1,
			EGG_TYPE_RECENT_ITEM);

	g_object_class_install_property (object_class,
					 PROP_UI_COMPONENT,
					 g_param_spec_object ("ui-component",
					   "UI Component",
					   "BonoboUIComponent for menus.",
					   bonobo_ui_component_get_type(),
					   G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_MENU_PATH,
					 g_param_spec_string ("ui-path",
					   "Path",
					   "The path to put the menu items.",
					   "/menus/File/EggRecentDocuments",
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



	klass->activate = NULL;
}

static void
egg_recent_view_init (EggRecentViewClass *iface)
{
	iface->do_get_model = egg_recent_view_bonobo_get_model;
	iface->do_set_model = egg_recent_view_bonobo_set_model;
}

static void
show_menus_changed_cb (GConfClient *client,
                       guint cnxn_id,
                       GConfEntry *entry,
                       EggRecentViewBonobo *view)
{
        GConfValue *value;

        value = gconf_entry_get_value (entry);

        g_return_if_fail (value->type == GCONF_VALUE_BOOL);

        egg_recent_view_bonobo_show_icons (view,
                                gconf_value_get_bool (value));

}

#ifndef USE_STABLE_LIBGNOMEUI
static void
theme_changed_cb (GnomeIconTheme *theme, EggRecentViewBonobo *view)
{
	if (view->model != NULL)
		egg_recent_model_changed (view->model);
}
#endif

static void
egg_recent_view_bonobo_init (EggRecentViewBonobo *view)
{
	view->uid = egg_recent_util_get_unique_id ();
#ifndef USE_STABLE_LIBGNOMEUI
	view->theme = gnome_icon_theme_new ();
	gnome_icon_theme_set_allow_svg (view->theme, TRUE);
	g_signal_connect_object (view->theme, "changed",
				 G_CALLBACK (theme_changed_cb), view, 0);
#endif

	view->client = gconf_client_get_default ();
	view->show_icons =
		gconf_client_get_bool (view->client,
			"/desktop/gnome/interface/menus_have_icons",
			NULL);

	gconf_client_add_dir (view->client, "/desktop/gnome/interface",
			      GCONF_CLIENT_PRELOAD_NONE,
			      NULL);
	gconf_client_notify_add (view->client,
			"/desktop/gnome/interface/menus_have_icons",
			(GConfClientNotifyFunc)show_menus_changed_cb,
			view, NULL, NULL);

	view->tooltip_func = NULL;
	view->tooltip_func_data = NULL;

	view->icon_size = GTK_ICON_SIZE_MENU;
}

void
egg_recent_view_bonobo_set_icon_size (EggRecentViewBonobo *view,
				      GtkIconSize icon_size)
{
	if (view->icon_size != icon_size) {
		view->icon_size = icon_size;
		egg_recent_model_changed (view->model);
	} else {
		view->icon_size = icon_size;
	}
}
                                                                              
GtkIconSize
egg_recent_view_bonobo_get_icon_size (EggRecentViewBonobo *view)
{
	return view->icon_size;
}

void
egg_recent_view_bonobo_show_icons (EggRecentViewBonobo *view, gboolean show)
{
	view->show_icons = show;

	if (view->model)
		egg_recent_model_changed (view->model);
}

void
egg_recent_view_bonobo_show_numbers (EggRecentViewBonobo *view, gboolean show)
{
	view->show_numbers = show;

	if (view->model)
		egg_recent_model_changed (view->model);
}

void
egg_recent_view_bonobo_set_ui_component (EggRecentViewBonobo *view, BonoboUIComponent *uic)
{
	g_return_if_fail (view);
	g_return_if_fail (uic);

	view->uic = uic;

	g_object_ref (view->uic);
}

void
egg_recent_view_bonobo_set_ui_path (EggRecentViewBonobo *view, const gchar *path)
{
	g_return_if_fail (view);
	g_return_if_fail (path);

	view->path = g_strdup (path);
}

const BonoboUIComponent *
egg_recent_view_bonobo_get_ui_component (EggRecentViewBonobo *view)
{
	g_return_val_if_fail (view, NULL);

	return view->uic;
}

gchar *
egg_recent_view_bonobo_get_ui_path (EggRecentViewBonobo *view)
{
	g_return_val_if_fail (view, NULL);

	return g_strdup (view->path);
}

void
egg_recent_view_bonobo_set_tooltip_func (EggRecentViewBonobo *view,
					 EggRecentViewBonoboTooltipFunc func,
					 gpointer user_data)
{
	view->tooltip_func = func;
	view->tooltip_func_data = user_data;
	
	if (view->model)
		egg_recent_model_changed (view->model);
}

/**
 * egg_recent_view_bonobo_new:
 * @appname: The name of your application.
 * @limit:  The maximum number of items allowed.
 *
 * This creates a new EggRecentViewBonobo object.
 *
 * Returns: a EggRecentViewBonobo object
 */
EggRecentViewBonobo *
egg_recent_view_bonobo_new (BonoboUIComponent *uic, const gchar *path)
{
	EggRecentViewBonobo *view;

	g_return_val_if_fail (uic, NULL);
	g_return_val_if_fail (path, NULL);

	view = EGG_RECENT_VIEW_BONOBO (g_object_new (egg_recent_view_bonobo_get_type (),
					   "ui-path", path,
					   "ui-component", uic,
					   "show-icons", FALSE,
					   "show-numbers", TRUE, NULL));

	g_return_val_if_fail (view, NULL);
	
	return view;
}

/**
 * egg_recent_view_bonobo_get_type:
 * @:
 *
 * This returns a GType representing a EggRecentViewBonobo object.
 *
 * Returns: a GType
 */
GType
egg_recent_view_bonobo_get_type (void)
{
	static GType egg_recent_view_bonobo_type = 0;

	if(!egg_recent_view_bonobo_type) {
		static const GTypeInfo egg_recent_view_bonobo_info = {
			sizeof (EggRecentViewBonoboClass),
			NULL, /* base init */
			NULL, /* base finalize */
			(GClassInitFunc)egg_recent_view_bonobo_class_init, /* class init */
			NULL, /* class finalize */
			NULL, /* class data */
			sizeof (EggRecentViewBonobo),
			0,
			(GInstanceInitFunc) egg_recent_view_bonobo_init
		};

		static const GInterfaceInfo view_info =
		{
			(GInterfaceInitFunc) egg_recent_view_init,
			NULL,
			NULL
		};

		egg_recent_view_bonobo_type = g_type_register_static (G_TYPE_OBJECT,
							"EggRecentViewBonobo",
							&egg_recent_view_bonobo_info, 0);
		g_type_add_interface_static (egg_recent_view_bonobo_type,
					     EGG_TYPE_RECENT_VIEW,
					     &view_info);
	}

	return egg_recent_view_bonobo_type;
}

