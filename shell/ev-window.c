/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2004 Martin Kretzschmar
 *  Copyright (C) 2004 Red Hat, Inc.
 *
 *  Author:
 *    Martin Kretzschmar <martink@gnome.org>
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

#include "ev-window.h"
#include "ev-sidebar.h"
#include "ev-sidebar-bookmarks.h"
#include "ev-sidebar-thumbnails.h"
#include "ev-view.h"
#include "eggfindbar.h"

#include "pdf-document.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>

#include <string.h>

#include "ev-application.h"

enum {
	PROP_0,
	PROP_ATTRIBUTE
};

enum {
	SIGNAL,
	N_SIGNALS
};

struct _EvWindowPrivate {
	GtkWidget *main_box;
	GtkWidget *hpaned;
	GtkWidget *sidebar;
	GtkWidget *find_bar;
	GtkWidget *bonobo_widget;
	GtkWidget *view;
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	GtkWidget *statusbar;
	guint help_message_cid;
	
	EvDocument *document;
};

#if 0
/* enable these to add support for signals */
static guint ev_window_signals [N_SIGNALS] = { 0 };
#endif

static GObjectClass *parent_class = NULL;

G_DEFINE_TYPE (EvWindow, ev_window, GTK_TYPE_WINDOW)

#define EV_WINDOW_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_WINDOW, EvWindowPrivate))

#if 0
const char *
ev_window_get_attribute (EvWindow *self)
{
	g_return_val_if_fail (self != NULL && EV_IS_WINDOW (self), NULL);
	
	return self->priv->attribute;
}

void
ev_window_set_attribute (EvWindow* self, const char *attribute)
{
	g_assert (self != NULL && EV_IS_WINDOW (self));
	g_assert (attribute != NULL);

	if (self->priv->attribute != NULL) {
		g_free (self->priv->attribute);
	}

	self->priv->attribute = g_strdup (attribute);

	g_object_notify (G_OBJECT (self), "attribute");
}

static void
ev_window_get_property (GObject *object, guint prop_id, GValue *value,
			GParamSpec *param_spec)
{
	EvWindow *self;

	self = EV_WINDOW (object);

	switch (prop_id) {
	case PROP_ATTRIBUTE:
		g_value_set_string (value, self->priv->attribute);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
						   prop_id,
						   param_spec);
		break;
	}
}

static void
ev_window_set_property (GObject *object, guint prop_id, const GValue *value,
			GParamSpec *param_spec)
{
	EvWindow *self;
	
	self = EV_WINDOW (object);
	
	switch (prop_id) {
	case PROP_ATTRIBUTE:
		ev_window_set_attribute (self, g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
						   prop_id,
						   param_spec);
		break;
	}
}
#endif

static void
set_action_sensitive (EvWindow   *ev_window,
		      const char *name,
		      gboolean    sensitive)
{
	GtkAction *action = gtk_action_group_get_action (ev_window->priv->action_group,
							 name);
	gtk_action_set_sensitive (action, sensitive);
}

static void
update_action_sensitivity (EvWindow *ev_window)
{
	int n_pages;
	int page;

	if (ev_window->priv->document)
		n_pages = ev_document_get_n_pages (ev_window->priv->document);
	else
		n_pages = 1;

	page = ev_view_get_page (EV_VIEW (ev_window->priv->view));

	set_action_sensitive (ev_window, "GoFirstPage", page > 1);
	set_action_sensitive (ev_window, "GoPreviousPage", page > 1);
	set_action_sensitive (ev_window, "GoNextPage", page < n_pages);
	set_action_sensitive (ev_window, "GoLastPage", page < n_pages);
}

gboolean
ev_window_is_empty (const EvWindow *ev_window)
{
	g_return_val_if_fail (EV_IS_WINDOW (ev_window), FALSE);
	
	return ev_window->priv->bonobo_widget == NULL;
}

void
ev_window_open (EvWindow *ev_window, const char *uri)
{
	EvDocument *document = g_object_new (PDF_TYPE_DOCUMENT, NULL);
	GError *error = NULL;

	if (ev_document_load (document, uri, &error)) {
		if (ev_window->priv->document)
			g_object_unref (ev_window->priv->document);
		ev_window->priv->document = document;

		ev_view_set_document (EV_VIEW (ev_window->priv->view),
				      document);

		update_action_sensitivity (ev_window);
		
	} else {
		GtkWidget *dialog;

		g_object_unref (document);

		dialog = gtk_message_dialog_new (GTK_WINDOW (ev_window),
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
						 _("Unable to open document"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_error_free (error);
	}
	
#if 0
	char *mime_type;
	BonoboObject *bonobo_control;
	CORBA_Environment ev;
	Bonobo_PersistFile pf;

	mime_type = gnome_vfs_get_mime_type (uri);

	g_return_if_fail (mime_type != NULL); /* FIXME set error */

	if (!strcmp (mime_type, "application/pdf")) {
		bonobo_control = create_gpdf_control ();
	} else if (!strcmp (mime_type, "application/postscript")) {
		bonobo_control = create_ggv_control ();
	} else if (!strcmp (mime_type, "application/x-gzip")) {
		g_message ("Cannot open gzip-compressed file %s.", uri);
		goto finally;
	} else if (!strcmp (mime_type, "application/x-bzip")) {
		g_message ("Cannot open bzip2-compressed file %s.", uri);
		goto finally;
	} else {
		g_warning ("Don't know how to open %s file %s.",
			   mime_type, uri); /* FIXME set error */
		goto finally;
	}

	ev_window->priv->bonobo_widget = bonobo_widget_new_control_from_objref (
		bonobo_object_corba_objref (bonobo_control), CORBA_OBJECT_NIL);
	gtk_box_pack_start (GTK_BOX (ev_window->priv->main_box),
			    ev_window->priv->bonobo_widget,
			    TRUE, TRUE, 0);
	CORBA_exception_init (&ev);
	pf = bonobo_object_query_interface (
		bonobo_control, "IDL:Bonobo/PersistFile:1.0", &ev);
	Bonobo_PersistFile_load (pf, uri, &ev);
	gtk_widget_show (ev_window->priv->bonobo_widget);
	bonobo_object_release_unref (pf, &ev);
	bonobo_object_unref (bonobo_control);
	CORBA_exception_free (&ev);

finally:
	g_free (mime_type);
#endif
}

static void
ev_window_cmd_file_open (GtkAction *action, EvWindow *ev_window)
{
	ev_application_open (EV_APP, NULL);
}

static void
ev_window_cmd_file_print (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));
        /* FIXME */
}

static void
ev_window_cmd_file_close_window (GtkAction *action, EvWindow *ev_window)
{
	g_return_if_fail (EV_IS_WINDOW (ev_window));

	gtk_widget_destroy (GTK_WIDGET (ev_window));
}

static void
ev_window_cmd_edit_find (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

	gtk_widget_show (ev_window->priv->find_bar);
	egg_find_bar_grab_focus (EGG_FIND_BAR (ev_window->priv->find_bar));
}

static void
ev_window_cmd_edit_copy (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

        /* FIXME */
}

static void
ev_window_cmd_view_fullscreen (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

        /* FIXME */
}

static void
ev_window_cmd_view_zoom_in (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

        /* FIXME */
}

static void
ev_window_cmd_view_zoom_out (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

        /* FIXME */
}

static void
ev_window_cmd_view_normal_size (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

        /* FIXME */
}

static void
ev_window_cmd_view_best_fit (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

        /* FIXME */
}

static void
ev_window_cmd_view_page_width (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

        /* FIXME */
}

static void
ev_window_cmd_go_back (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

        /* FIXME */
}

static void
ev_window_cmd_go_forward (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

        /* FIXME */
}

static void
ev_window_cmd_go_previous_page (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

	ev_view_set_page (EV_VIEW (ev_window->priv->view),
			  ev_view_get_page (EV_VIEW (ev_window->priv->view)) - 1);
}

static void
ev_window_cmd_go_next_page (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

	ev_view_set_page (EV_VIEW (ev_window->priv->view),
			  ev_view_get_page (EV_VIEW (ev_window->priv->view)) + 1);
}

static void
ev_window_cmd_go_first_page (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

	ev_view_set_page (EV_VIEW (ev_window->priv->view), 1);
}

static void
ev_window_cmd_go_last_page (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

	ev_view_set_page (EV_VIEW (ev_window->priv->view), G_MAXINT);
}

static void
ev_window_cmd_help_contents (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

        /* FIXME */
}

static void
ev_window_cmd_help_about (GtkAction *action, EvWindow *ev_window)
{
	const char *authors[] = {
		N_("Many..."),
		NULL
	};

	const char *documenters[] = {
		N_("Not so many..."),
		NULL
	};

	const char *license[] = {
		N_("Evince is free software; you can redistribute it and/or modify\n"
		   "it under the terms of the GNU General Public License as published by\n"
		   "the Free Software Foundation; either version 2 of the License, or\n"
		   "(at your option) any later version.\n"),		
		N_("Evince is distributed in the hope that it will be useful,\n"
		   "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		   "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
		   "GNU General Public License for more details.\n"),
		N_("You should have received a copy of the GNU General Public License\n"
		   "along with Evince; if not, write to the Free Software Foundation, Inc.,\n"
		   "59 Temple Place, Suite 330, Boston, MA  02111-1307  USA\n")
	};

	char *license_trans;

#ifdef ENABLE_NLS
	const char **p;

	for (p = authors; *p; ++p)
		*p = _(*p);

	for (p = documenters; *p; ++p)
		*p = _(*p);
#endif

	license_trans = g_strconcat (_(license[0]), "\n", _(license[1]), "\n",
				     _(license[2]), "\n", NULL);

	gtk_show_about_dialog (
		GTK_WINDOW (ev_window),
		"name", _("Evince"),
		"version", VERSION,
		"copyright",
		_("\xc2\xa9 1996-2004 The Evince authors"),
		"license", license_trans,
		"website", "http://www.gnome.org/projects/evince",
		"comments", _("PostScript and PDF File Viewer."),
		"authors", authors,
		"documenters", documenters,
		"translator-credits", _("translator-credits"),
		NULL);

	g_free (license_trans);
}

static void
ev_window_view_toolbar_cb (GtkAction *action, EvWindow *ev_window)
{
	g_object_set (
		G_OBJECT (gtk_ui_manager_get_widget (
				  ev_window->priv->ui_manager,
				  "/ToolBar")),
		"visible",
		gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)),
		NULL);
}

static void
ev_window_view_statusbar_cb (GtkAction *action, EvWindow *ev_window)
{
	g_object_set (
		ev_window->priv->statusbar,
		"visible",
		gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)),
		NULL);
}

static void
ev_window_view_sidebar_cb (GtkAction *action, EvWindow *ev_window)
{
        /* FIXME */
}

static void
menu_item_select_cb (GtkMenuItem *proxy, EvWindow *ev_window)
{
	GtkAction *action;
	char *message;

	action = g_object_get_data (G_OBJECT (proxy), "gtk-action");
	g_return_if_fail (action != NULL);
	
	g_object_get (G_OBJECT (action), "tooltip", &message, NULL);
	if (message) {
		gtk_statusbar_push (GTK_STATUSBAR (ev_window->priv->statusbar),
				    ev_window->priv->help_message_cid, message);
		g_free (message);
	}
}

static void
menu_item_deselect_cb (GtkMenuItem *proxy, EvWindow *ev_window)
{
	gtk_statusbar_pop (GTK_STATUSBAR (ev_window->priv->statusbar),
			   ev_window->priv->help_message_cid);
}

static void
connect_proxy_cb (GtkUIManager *ui_manager, GtkAction *action,
		  GtkWidget *proxy, EvWindow *ev_window)
{
	if (GTK_IS_MENU_ITEM (proxy)) {
		g_signal_connect (proxy, "select",
				  G_CALLBACK (menu_item_select_cb), ev_window);
		g_signal_connect (proxy, "deselect",
				  G_CALLBACK (menu_item_deselect_cb),
				  ev_window);
	}
}

static void
disconnect_proxy_cb (GtkUIManager *ui_manager, GtkAction *action,
		     GtkWidget *proxy, EvWindow *ev_window)
{
	if (GTK_IS_MENU_ITEM (proxy)) {
		g_signal_handlers_disconnect_by_func
			(proxy, G_CALLBACK (menu_item_select_cb), ev_window);
		g_signal_handlers_disconnect_by_func
			(proxy, G_CALLBACK (menu_item_deselect_cb), ev_window);
	}
}

static void
view_page_changed_cb (EvView   *view,
		      EvWindow *ev_window)
{
	update_action_sensitivity (ev_window);
}

static void
find_bar_previous_cb (EggFindBar *find_bar,
		      EvWindow   *ev_window)
{
	/* FIXME - highlight previous result */
	g_printerr ("Find Previous\n");

}

static void
find_bar_next_cb (EggFindBar *find_bar,
		  EvWindow   *ev_window)
{
	/* FIXME - highlight next result */
	g_printerr ("Find Next\n");
}

static void
find_bar_close_cb (EggFindBar *find_bar,
		   EvWindow   *ev_window)
{
	gtk_widget_hide (ev_window->priv->find_bar);
}

static void
find_bar_search_changed_cb (EggFindBar *find_bar,
			    GParamSpec *param,
			    EvWindow   *ev_window)
{
	gboolean case_sensitive;
	gboolean visible;
	const char *search_string;

	g_return_if_fail (EV_IS_WINDOW (ev_window));
	
	/* Either the string or case sensitivity could have changed,
	 * we connect this callback to both. We also connect it
	 * to ::visible so when the find bar is hidden, we should
	 * pretend the search string is NULL/""
	 */

	case_sensitive = egg_find_bar_get_case_sensitive (find_bar);
	visible = GTK_WIDGET_VISIBLE (find_bar);
	search_string = egg_find_bar_get_search_string (find_bar);
	
#if 0
	g_printerr ("search for '%s'\n", search_string ? search_string : "(nil)");
#endif

	/* We don't require begin/end find calls to be matched up, it's really
	 * start_find and cancel_any_find_that_may_not_be_finished
	 */
	if (visible && search_string) {
		ev_document_begin_find (ev_window->priv->document, search_string, case_sensitive);
	} else {
		ev_document_end_find (ev_window->priv->document);
	}
}

static void
ev_window_dispose (GObject *object)
{
	EvWindowPrivate *priv;

	g_return_if_fail (object != NULL && EV_IS_WINDOW (object));

	priv = EV_WINDOW (object)->priv;

	if (priv->ui_manager) {
		g_object_unref (priv->ui_manager);
		priv->ui_manager = NULL;
	}

	if (priv->action_group) {
		g_object_unref (priv->action_group);
		priv->action_group = NULL;
	}
	
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
ev_window_class_init (EvWindowClass *ev_window_class)
{
	GObjectClass *g_object_class;

	parent_class = g_type_class_peek_parent (ev_window_class);

	g_object_class = G_OBJECT_CLASS (ev_window_class);
	g_object_class->dispose = ev_window_dispose;

	g_type_class_add_private (g_object_class, sizeof (EvWindowPrivate));

#if 0
	/* setting up signal system */
	ev_window_class->signal = ev_window_signal;

	ev_window_signals [SIGNAL] = g_signal_new (
		"signal",
		EV_TYPE_WINDOW,
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EvWindowClass,
				 signal),
		NULL,
		NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE,
		0);
	/* setting up property system */
	g_object_class->set_property = ev_window_set_property;
	g_object_class->get_property = ev_window_get_property;

	g_object_class_install_property (
		g_object_class,
		PROP_ATTRIBUTE,
		g_param_spec_string ("attribute",
				     "Attribute",
				     "A simple unneccessary attribute that "
				     "does nothing special except being a "
				     "demonstration for the correct implem"
				     "entation of a GObject property",
				     "default_value",
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
#endif
}

/* Normal items */
static GtkActionEntry entries[] = {
	{ "File", NULL, N_("_File") },
        { "Edit", NULL, N_("_Edit") },
	{ "View", NULL, N_("_View") },
        { "Go", NULL, N_("_Go") },
	{ "Help", NULL, N_("_Help") },

	/* File menu */
	{ "FileOpen", GTK_STOCK_OPEN, N_("_Open"), "<control>O",
	  N_("Open a file"),
	  G_CALLBACK (ev_window_cmd_file_open) },
        { "FilePrint", GTK_STOCK_PRINT, N_("_Print"), "<control>P",
	  N_("Print this document"),
	  G_CALLBACK (ev_window_cmd_file_print) },
	{ "FileCloseWindow", GTK_STOCK_CLOSE, N_("_Close"), "<control>W",
	  N_("Close this window"),
	  G_CALLBACK (ev_window_cmd_file_close_window) },

        /* Edit menu */
        { "EditCopy", GTK_STOCK_COPY, N_("_Copy"), "<control>C",
          N_("Copy text from the document"),
          G_CALLBACK (ev_window_cmd_edit_copy) },
        
        { "EditFind", GTK_STOCK_FIND, N_("_Find"), "<control>F",
          N_("Find a word or phrase in the document"),
          G_CALLBACK (ev_window_cmd_edit_find) },

        /* View menu */
        { "ViewFullscreen", NULL, N_("_Fullscreen"), "F11",
          N_("Expand the window to fill the screen"),
          G_CALLBACK (ev_window_cmd_view_fullscreen) },
        { "ViewZoomIn", GTK_STOCK_ZOOM_IN, N_("Zoom _In"), "<control>plus",
          N_("Enlarge the document"),
          G_CALLBACK (ev_window_cmd_view_zoom_in) },
        { "ViewZoomOut", GTK_STOCK_ZOOM_OUT, N_("Zoom _Out"), "<control>minus",
          N_("Shrink the document"),
          G_CALLBACK (ev_window_cmd_view_zoom_out) },
        { "ViewNormalSize", GTK_STOCK_ZOOM_100, N_("_Normal Size"), "<control>0",
          N_("Zoom to the normal size"),
          G_CALLBACK (ev_window_cmd_view_normal_size) },
        { "ViewBestFit", GTK_STOCK_ZOOM_FIT, N_("_Best Fit"), NULL,
          N_("Zoom to fit the document to the current window"),
          G_CALLBACK (ev_window_cmd_view_best_fit) },
        { "ViewPageWidth", NULL, N_("Fit Page _Width"), NULL,
          N_("Zoom to fit the width of the current window "),
          G_CALLBACK (ev_window_cmd_view_page_width) },

        /* Go menu */
        { "GoBack", GTK_STOCK_GO_BACK, N_("_Back"), "<mod1>Left",
          N_("Go to the page viewed before this one"),
          G_CALLBACK (ev_window_cmd_go_back) },
        { "GoForward", GTK_STOCK_GO_FORWARD, N_("Fo_rward"), "<mod1>Right",
          N_("Go to the page viewed before this one"),
          G_CALLBACK (ev_window_cmd_go_forward) },
        { "GoPreviousPage", GTK_STOCK_GO_BACK, N_("_Previous Page"), "<control>Page_Up",
          N_("Go to the previous page"),
          G_CALLBACK (ev_window_cmd_go_previous_page) },
        { "GoNextPage", GTK_STOCK_GO_FORWARD, N_("_Next Page"), "<control>Page_Down",
          N_("Go to the next page"),
          G_CALLBACK (ev_window_cmd_go_next_page) },
        { "GoFirstPage", GTK_STOCK_GOTO_FIRST, N_("_First Page"), "<control>Home",
          N_("Go to the first page"),
          G_CALLBACK (ev_window_cmd_go_first_page) },        
        { "GoLastPage", GTK_STOCK_GOTO_LAST, N_("_Last Page"), "<control>End",
          N_("Go to the last page"),
          G_CALLBACK (ev_window_cmd_go_last_page) },
        
	/* Help menu */
	{ "HelpContents", GTK_STOCK_HELP, N_("_Contents"), NULL,
	  N_("Display help for the viewer application"),
	  G_CALLBACK (ev_window_cmd_help_contents) },
        
	{ "HelpAbout", GTK_STOCK_ABOUT, N_("_About"), NULL,
	  N_("Display credits for the document viewer creators"),
	  G_CALLBACK (ev_window_cmd_help_about) },
};

/* Toggle items */
static GtkToggleActionEntry toggle_entries[] = {
	/* View Menu */
	{ "ViewToolbar", NULL, N_("_Toolbar"), "<shift><control>T",
	  N_("Show or hide toolbar"),
	  G_CALLBACK (ev_window_view_toolbar_cb), TRUE },
	{ "ViewStatusbar", NULL, N_("_Statusbar"), NULL,
	  N_("Show or hide statusbar"),
	  G_CALLBACK (ev_window_view_statusbar_cb), TRUE },
        { "ViewSidebar", NULL, N_("Side_bar"), "F9",
	  N_("Show or hide sidebar"),
	  G_CALLBACK (ev_window_view_sidebar_cb), FALSE },
};

static void
ev_window_init (EvWindow *ev_window)
{
	GtkActionGroup *action_group;
	GtkAccelGroup *accel_group;
	GError *error = NULL;
	GtkWidget *scrolled_window;
	GtkWidget *menubar;
	GtkWidget *toolbar;
	GtkWidget *sidebar_widget;

	ev_window->priv = EV_WINDOW_GET_PRIVATE (ev_window);

	gtk_window_set_title (GTK_WINDOW (ev_window), _("Document Viewer"));

	ev_window->priv->main_box = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (ev_window), ev_window->priv->main_box);
	gtk_widget_show (ev_window->priv->main_box);
	
	action_group = gtk_action_group_new ("MenuActions");
	ev_window->priv->action_group = action_group;
	gtk_action_group_set_translation_domain (action_group, NULL);
	gtk_action_group_add_actions (action_group, entries,
				      G_N_ELEMENTS (entries), ev_window);
	gtk_action_group_add_toggle_actions (action_group, toggle_entries,
					     G_N_ELEMENTS (toggle_entries),
					     ev_window);

	ev_window->priv->ui_manager = gtk_ui_manager_new ();
	gtk_ui_manager_insert_action_group (ev_window->priv->ui_manager,
					    action_group, 0);

	accel_group =
		gtk_ui_manager_get_accel_group (ev_window->priv->ui_manager);
	gtk_window_add_accel_group (GTK_WINDOW (ev_window), accel_group);

	g_signal_connect (ev_window->priv->ui_manager, "connect_proxy",
			  G_CALLBACK (connect_proxy_cb), ev_window);
	g_signal_connect (ev_window->priv->ui_manager, "disconnect_proxy",
			  G_CALLBACK (disconnect_proxy_cb), ev_window);

	if (!gtk_ui_manager_add_ui_from_file (ev_window->priv->ui_manager,
					      DATADIR"/evince-ui.xml",
					      &error)) {
		g_message ("building menus failed: %s", error->message);
		g_error_free (error);
	}

	menubar = gtk_ui_manager_get_widget (ev_window->priv->ui_manager,
					     "/MainMenu");
	gtk_box_pack_start (GTK_BOX (ev_window->priv->main_box), menubar,
			    FALSE, FALSE, 0);

	toolbar = gtk_ui_manager_get_widget (ev_window->priv->ui_manager,
					     "/ToolBar");
	gtk_box_pack_start (GTK_BOX (ev_window->priv->main_box), toolbar,
			    FALSE, FALSE, 0);

	/* Add the main area */
	ev_window->priv->hpaned = gtk_hpaned_new ();
	gtk_widget_show (ev_window->priv->hpaned);
	gtk_box_pack_start (GTK_BOX (ev_window->priv->main_box), ev_window->priv->hpaned,
			    TRUE, TRUE, 0);

	ev_window->priv->sidebar = ev_sidebar_new ();
	gtk_widget_show (ev_window->priv->sidebar);
	gtk_paned_add1 (GTK_PANED (ev_window->priv->hpaned),
			ev_window->priv->sidebar);

	/* Stub sidebar, for now */
	sidebar_widget = ev_sidebar_bookmarks_new ();
	gtk_widget_show (sidebar_widget);
	ev_sidebar_add_page (EV_SIDEBAR (ev_window->priv->sidebar),
			     "bookmarks",
			     _("Bookmarks"),
			     sidebar_widget);

	sidebar_widget = ev_sidebar_thumbnails_new ();
	gtk_widget_show (sidebar_widget);
	ev_sidebar_add_page (EV_SIDEBAR (ev_window->priv->sidebar),
			     "thumbnails",
			     _("Thumbnails"),
			     sidebar_widget);
	
	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolled_window);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	gtk_paned_add2 (GTK_PANED (ev_window->priv->hpaned),
			scrolled_window);

	ev_window->priv->view = ev_view_new ();
	gtk_widget_show (ev_window->priv->view);
	gtk_container_add (GTK_CONTAINER (scrolled_window),
			   ev_window->priv->view);
	g_signal_connect (ev_window->priv->view,
			  "page-changed",
			  G_CALLBACK (view_page_changed_cb),
			  ev_window);

	ev_window->priv->statusbar = gtk_statusbar_new ();
	gtk_widget_show (ev_window->priv->statusbar);
	gtk_box_pack_end (GTK_BOX (ev_window->priv->main_box),
			  ev_window->priv->statusbar,
			  FALSE, TRUE, 0);
	ev_window->priv->help_message_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR (ev_window->priv->statusbar), "help_message");

	ev_window->priv->find_bar = egg_find_bar_new ();
	gtk_box_pack_end (GTK_BOX (ev_window->priv->main_box),
			  ev_window->priv->find_bar,
			  FALSE, TRUE, 0);
	
	/* Connect to find bar signals */
	g_signal_connect (ev_window->priv->find_bar,
			  "previous",
			  G_CALLBACK (find_bar_previous_cb),
			  ev_window);
	g_signal_connect (ev_window->priv->find_bar,
			  "next",
			  G_CALLBACK (find_bar_next_cb),
			  ev_window);
	g_signal_connect (ev_window->priv->find_bar,
			  "close",
			  G_CALLBACK (find_bar_close_cb),
			  ev_window);
	g_signal_connect (ev_window->priv->find_bar,
			  "notify::search-string",
			  G_CALLBACK (find_bar_search_changed_cb),
			  ev_window);
	g_signal_connect (ev_window->priv->find_bar,
			  "notify::case-sensitive",
			  G_CALLBACK (find_bar_search_changed_cb),
			  ev_window);
	g_signal_connect (ev_window->priv->find_bar,
			  "notify::visible",
			  G_CALLBACK (find_bar_search_changed_cb),
			  ev_window);
	
	update_action_sensitivity (ev_window);
}
