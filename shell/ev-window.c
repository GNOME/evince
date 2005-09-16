/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2004 Martin Kretzschmar
 *  Copyright (C) 2004 Red Hat, Inc.
 *  Copyright (C) 2000, 2001, 2002, 2003, 2004 Marco Pesenti Gritti
 *  Copyright (C) 2003, 2004, 2005 Christian Persch
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
#include "ev-page-action.h"
#include "ev-sidebar.h"
#include "ev-sidebar-links.h"
#include "ev-sidebar-thumbnails.h"
#include "ev-view.h"
#include "ev-password.h"
#include "ev-password-view.h"
#include "ev-print-job.h"
#include "ev-properties-dialog.h"
#include "ev-ps-exporter.h"
#include "ev-document-thumbnails.h"
#include "ev-document-links.h"
#include "ev-document-fonts.h"
#include "ev-document-find.h"
#include "ev-document-security.h"
#include "ev-document-types.h"
#include "ev-job-queue.h"
#include "ev-jobs.h"
#include "ev-sidebar-page.h"
#include "eggfindbar.h"
#include "egg-recent-view-uimanager.h"
#include "egg-recent-view.h"
#include "egg-toolbar-editor.h"
#include "egg-editable-toolbar.h"
#include "egg-recent-model.h"
#include "egg-toolbars-model.h"
#include "ephy-zoom.h"
#include "ephy-zoom-action.h"
#include "ev-application.h"
#include "ev-stock-icons.h"
#include "ev-metadata-manager.h"
#include "ev-file-helpers.h"
#include "ev-utils.h"

#include <poppler.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gnome.h>
#include <libgnomeprintui/gnome-print-dialog.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <gconf/gconf-client.h>

#include <string.h>

typedef enum {
	PAGE_MODE_DOCUMENT,
	PAGE_MODE_PASSWORD
} EvWindowPageMode;

typedef enum {
	EV_CHROME_MENUBAR	= 1 << 0,
	EV_CHROME_TOOLBAR	= 1 << 1,
	EV_CHROME_FINDBAR	= 1 << 2,
	EV_CHROME_RAISE_TOOLBAR	= 1 << 3,
	EV_CHROME_NORMAL	= EV_CHROME_MENUBAR | EV_CHROME_TOOLBAR
} EvChrome;

struct _EvWindowPrivate {
	/* UI */
	EvChrome chrome;

	GtkWidget *main_box;
	GtkWidget *menubar;
	GtkWidget *toolbar_dock;
	GtkWidget *toolbar;
	GtkWidget *hpaned;
	GtkWidget *sidebar;
	GtkWidget *find_bar;
	GtkWidget *scrolled_window;
	GtkWidget *view;
	GtkWidget *page_view;
	GtkWidget *password_view;
	GtkWidget *sidebar_thumbs;
	GtkWidget *sidebar_links;

	/* Dialogs */
	GtkWidget *properties;

	/* UI Builders */
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;

	/* Fullscreen mode */
	GtkWidget *fullscreen_toolbar;
	GtkWidget *fullscreen_popup;
	GSource   *fullscreen_timeout_source;

	/* Document */
	char *uri;
	char *local_uri;
	EvDocument *document;
	EvPageCache *page_cache;

	EvWindowPageMode page_mode;

	/* These members are used temporarily when in PAGE_MODE_PASSWORD */
	EvDocument *password_document;
	GtkWidget *password_dialog;
	char *password_uri;

	/* Job used to load document */
	EvJob *xfer_job;
	EvJob *load_job;

	EggRecentViewUIManager *recent_view;
};

static const GtkTargetEntry ev_drop_types[] = {
	{ "text/uri-list", 0, 0 }
};


#define EV_WINDOW_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_WINDOW, EvWindowPrivate))

#define PAGE_SELECTOR_ACTION	"PageSelector"
#define ZOOM_CONTROL_ACTION	"ViewZoom"

#define GCONF_CHROME_TOOLBAR        "/apps/evince/show_toolbar"
#define GCONF_OVERRIDE_RESTRICTIONS "/apps/evince/override_restrictions"
#define GCONF_LOCKDOWN_SAVE         "/desktop/gnome/lockdown/disable_save_to_disk"
#define GCONF_LOCKDOWN_PRINT        "/desktop/gnome/lockdown/disable_printing"

#define FULLSCREEN_TIMEOUT 5 * 1000

#define SIDEBAR_DEFAULT_SIZE    132
#define LINKS_SIDEBAR_ID "links"
#define THUMBNAILS_SIDEBAR_ID "thumbnails"

#define PRINT_CONFIG_FILENAME	"ev-print-config.xml"

static void     ev_window_update_fullscreen_popup       (EvWindow         *window);
static void     ev_window_sidebar_visibility_changed_cb (EvSidebar        *ev_sidebar,
							 GParamSpec       *pspec,
							 EvWindow         *ev_window);
static void     ev_window_set_page_mode                 (EvWindow         *window,
							 EvWindowPageMode  page_mode);
static void	ev_window_load_job_cb  			(EvJobLoad *job,
							 gpointer data);
static void	ev_window_xfer_job_cb  			(EvJobXfer *job,
							 gpointer data);
static void     ev_window_sizing_mode_changed_cb        (EvView           *view,
							 GParamSpec       *pspec,
							 EvWindow         *ev_window);
static void     ev_window_zoom_changed_cb 	        (EvView           *view,
							 GParamSpec       *pspec,
							 EvWindow         *ev_window);
static void     ev_window_add_recent                    (EvWindow         *window,
							 const char       *filename);
static void     ev_window_run_fullscreen                (EvWindow         *window);
static void     ev_window_stop_fullscreen               (EvWindow         *window);
static void     ev_window_cmd_view_fullscreen           (GtkAction        *action,
							 EvWindow         *window);
static void     ev_window_run_presentation              (EvWindow         *window);
static void     ev_window_stop_presentation             (EvWindow         *window);
static void     ev_window_cmd_view_presentation         (GtkAction        *action,
							 EvWindow         *window);
static void     show_fullscreen_popup                   (EvWindow         *window);


G_DEFINE_TYPE (EvWindow, ev_window, GTK_TYPE_WINDOW)

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
	EvView *view;
	EvDocument *document;
	const EvDocumentInfo *info = NULL;
	EvWindowPageMode page_mode;
	gboolean sensitive, has_pages = FALSE, has_document;
	int n_pages = 0, page = -1;
	gboolean ok_to_print = TRUE;
	gboolean ok_to_copy = TRUE;
	gboolean has_properties = TRUE;
	gboolean override_restrictions = FALSE;
	GConfClient *client;

	view = EV_VIEW (ev_window->priv->view);

	document = ev_window->priv->document;

	if (document)
		info = ev_page_cache_get_info (ev_window->priv->page_cache);

	page_mode = ev_window->priv->page_mode;
	has_document = document != NULL;

	if (has_document && ev_window->priv->page_cache) {
		page = ev_page_cache_get_current_page (ev_window->priv->page_cache);
		n_pages = ev_page_cache_get_n_pages (ev_window->priv->page_cache);
		has_pages = has_document && n_pages > 0;
	}

	client = gconf_client_get_default ();
	override_restrictions = gconf_client_get_bool (client, 
						       GCONF_OVERRIDE_RESTRICTIONS, 
						       NULL);
	if (!override_restrictions && info && info->fields_mask & EV_DOCUMENT_INFO_PERMISSIONS) {
		ok_to_print = (info->permissions & EV_DOCUMENT_PERMISSIONS_OK_TO_PRINT);
		ok_to_copy = (info->permissions & EV_DOCUMENT_PERMISSIONS_OK_TO_COPY);
	}

	if (has_document && !EV_IS_PS_EXPORTER(document))
		ok_to_print = FALSE;

	if (!info || info->fields_mask == 0) {
		has_properties = FALSE;
	}
	
	if (gconf_client_get_bool (client, GCONF_LOCKDOWN_SAVE, NULL)) {
		ok_to_copy = FALSE;
	}

	if (gconf_client_get_bool (client, GCONF_LOCKDOWN_PRINT, NULL)) {
		ok_to_print = FALSE;
	}
	
	g_object_unref (client);

	/* File menu */
	/* "FileOpen": always sensitive */
	set_action_sensitive (ev_window, "FileSaveAs", has_document && ok_to_copy);
	set_action_sensitive (ev_window, "FilePrint", has_pages && ok_to_print);
	set_action_sensitive (ev_window, "FileProperties", has_document && has_properties);
	/* "FileCloseWindow": always sensitive */

        /* Edit menu */
	sensitive = has_pages && ev_document_can_get_text (document);
	set_action_sensitive (ev_window, "EditCopy", sensitive && ok_to_copy);
	set_action_sensitive (ev_window, "EditSelectAll", sensitive && ok_to_copy);
	set_action_sensitive (ev_window, "EditFind",
			      has_pages && EV_IS_DOCUMENT_FIND (document));
	set_action_sensitive (ev_window, "Slash",
			      has_pages && EV_IS_DOCUMENT_FIND (document));
	set_action_sensitive (ev_window, "EditFindNext",
			      ev_view_can_find_next (view));
	set_action_sensitive (ev_window, "EditRotateLeft", has_pages);
	set_action_sensitive (ev_window, "EditRotateRight", has_pages);

        /* View menu */
	set_action_sensitive (ev_window, "ViewContinuous", has_pages);
	set_action_sensitive (ev_window, "ViewDual", has_pages);
	set_action_sensitive (ev_window, "ViewZoomIn",
			      has_pages && ev_view_can_zoom_in (view));
	set_action_sensitive (ev_window, "ViewZoomOut",
			      has_pages && ev_view_can_zoom_out (view));
	set_action_sensitive (ev_window, "ViewBestFit", has_pages);
	set_action_sensitive (ev_window, "ViewPageWidth", has_pages);
	set_action_sensitive (ev_window, "ViewReload", has_pages);

        /* Go menu */
	if (document) {
		set_action_sensitive (ev_window, "GoPreviousPage", page > 0);
		set_action_sensitive (ev_window, "GoNextPage", page < n_pages - 1);
		set_action_sensitive (ev_window, "GoFirstPage", page > 0);
		set_action_sensitive (ev_window, "GoLastPage", page < n_pages - 1);
	} else {
  		set_action_sensitive (ev_window, "GoFirstPage", FALSE);
		set_action_sensitive (ev_window, "GoPreviousPage", FALSE);
		set_action_sensitive (ev_window, "GoNextPage", FALSE);
		set_action_sensitive (ev_window, "GoLastPage", FALSE);
	}

	/* Toolbar-specific actions: */
	set_action_sensitive (ev_window, PAGE_SELECTOR_ACTION, has_pages);
	set_action_sensitive (ev_window, ZOOM_CONTROL_ACTION,  has_pages);

	if (has_pages && ev_view_get_sizing_mode (view) == EV_SIZING_FREE) {
		GtkAction *action;
		float      zoom;
		float      real_zoom;

		action = gtk_action_group_get_action (ev_window->priv->action_group, 
						      ZOOM_CONTROL_ACTION);

		real_zoom = ev_view_get_zoom (EV_VIEW (ev_window->priv->view));
		zoom = ephy_zoom_get_nearest_zoom_level (real_zoom);

		ephy_zoom_action_set_zoom_level (EPHY_ZOOM_ACTION (action), zoom);
	}
}

static void
set_widget_visibility (GtkWidget *widget, gboolean visible)
{
	g_return_if_fail (GTK_IS_WIDGET (widget));
	
	if (visible)
		gtk_widget_show (widget);
	else
		gtk_widget_hide (widget);
}

static void
update_chrome_visibility (EvWindow *window)
{
	EvWindowPrivate *priv = window->priv;
	gboolean menubar, toolbar, findbar, fullscreen_toolbar;
	gboolean fullscreen_mode, presentation, fullscreen;

	presentation = ev_view_get_presentation (EV_VIEW (priv->view));
	fullscreen = ev_view_get_fullscreen (EV_VIEW (priv->view));
	fullscreen_mode = fullscreen || presentation;

	menubar = (priv->chrome & EV_CHROME_MENUBAR) != 0 && !fullscreen_mode;
	toolbar = ((priv->chrome & EV_CHROME_TOOLBAR) != 0  || 
		   (priv->chrome & EV_CHROME_RAISE_TOOLBAR) != 0) && !fullscreen_mode;
	fullscreen_toolbar = ((priv->chrome & EV_CHROME_TOOLBAR) != 0 ||
			      (priv->chrome & EV_CHROME_RAISE_TOOLBAR) != 0);
	findbar = (priv->chrome & EV_CHROME_FINDBAR) != 0;

	set_widget_visibility (priv->menubar, menubar);
	
	set_widget_visibility (priv->toolbar_dock, toolbar);
	set_action_sensitive (window, "EditToolbar", toolbar);

	set_widget_visibility (priv->find_bar, findbar);

	if (priv->fullscreen_popup != NULL) {
		if (fullscreen)
			show_fullscreen_popup (window);
		else
			set_widget_visibility (priv->fullscreen_popup, FALSE);

		set_widget_visibility (priv->fullscreen_toolbar, fullscreen_toolbar);
	}
}

static void
update_chrome_flag (EvWindow *window, EvChrome flag, const char *pref, gboolean active)
{
	EvWindowPrivate *priv = window->priv;
	GConfClient *client;
	
	if (active) {
		priv->chrome |= flag;
	}
	else {
		priv->chrome &= ~flag;
	}

	if (pref != NULL) {
		client = gconf_client_get_default ();
		gconf_client_set_bool (client, pref, active, NULL);
		g_object_unref (client);
	}

	update_chrome_visibility (window);
}

static void
ev_window_cmd_focus_page_selector (GtkAction *act, EvWindow *window)
{
	GtkAction *action;
	
	update_chrome_flag (window, EV_CHROME_RAISE_TOOLBAR, NULL, TRUE);
	set_action_sensitive (window, "ViewToolbar", FALSE);
	
	action = gtk_action_group_get_action (window->priv->action_group,
				     	      PAGE_SELECTOR_ACTION);
	ev_page_action_grab_focus (EV_PAGE_ACTION (action));
}

static void
ev_window_cmd_scroll_forward (GtkAction *action, EvWindow *window)
{
	ev_view_scroll (EV_VIEW (window->priv->view), EV_SCROLL_PAGE_FORWARD);
}

static void
ev_window_cmd_scroll_backward (GtkAction *action, EvWindow *window)
{
	ev_view_scroll (EV_VIEW (window->priv->view), EV_SCROLL_PAGE_BACKWARD);
}

static void
ev_window_cmd_continuous (GtkAction *action, EvWindow *ev_window)
{
	gboolean continuous;

	ev_window_stop_presentation (ev_window);
	continuous = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	g_object_set (G_OBJECT (ev_window->priv->view),
		      "continuous", continuous,
		      NULL);
	update_action_sensitivity (ev_window);
}

static void
ev_window_cmd_dual (GtkAction *action, EvWindow *ev_window)
{
	gboolean dual_page;

	ev_window_stop_presentation (ev_window);
	dual_page = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	g_object_set (G_OBJECT (ev_window->priv->view),
		      "dual-page", dual_page,
		      NULL);
	update_action_sensitivity (ev_window);
}

static void
ev_window_cmd_view_best_fit (GtkAction *action, EvWindow *ev_window)
{
	ev_window_stop_presentation (ev_window);

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
		ev_view_set_sizing_mode (EV_VIEW (ev_window->priv->view), EV_SIZING_BEST_FIT);
	} else {
		ev_view_set_sizing_mode (EV_VIEW (ev_window->priv->view), EV_SIZING_FREE);
	}
	update_action_sensitivity (ev_window);
}

static void
ev_window_cmd_view_page_width (GtkAction *action, EvWindow *ev_window)
{
	ev_window_stop_presentation (ev_window);

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
		ev_view_set_sizing_mode (EV_VIEW (ev_window->priv->view), EV_SIZING_FIT_WIDTH);
	} else {
		ev_view_set_sizing_mode (EV_VIEW (ev_window->priv->view), EV_SIZING_FREE);
	}
	update_action_sensitivity (ev_window);
}

static void
update_sizing_buttons (EvWindow *window)
{
	GtkActionGroup *action_group = window->priv->action_group;
	GtkAction *action;
	gboolean best_fit, page_width;
	EvSizingMode sizing_mode;

	if (window->priv->view == NULL)
		return;

	g_object_get (window->priv->view,
		      "sizing_mode", &sizing_mode,
		      NULL);

	switch (sizing_mode) {
	case EV_SIZING_BEST_FIT:
		best_fit = TRUE;
		page_width = FALSE;
		break;
	case EV_SIZING_FIT_WIDTH:
		best_fit = FALSE;
		page_width = TRUE;
		break;

	default:
		best_fit = page_width = FALSE;
		break;
	}

	action = gtk_action_group_get_action (action_group, "ViewBestFit");
	g_signal_handlers_block_by_func
		(action, G_CALLBACK (ev_window_cmd_view_best_fit), window);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), best_fit);
	g_signal_handlers_unblock_by_func
		(action, G_CALLBACK (ev_window_cmd_view_best_fit), window);

	action = gtk_action_group_get_action (action_group, "ViewPageWidth");	
	g_signal_handlers_block_by_func
		(action, G_CALLBACK (ev_window_cmd_view_page_width), window);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), page_width);
	g_signal_handlers_unblock_by_func
		(action, G_CALLBACK (ev_window_cmd_view_page_width), window);

	action = gtk_action_group_get_action (window->priv->action_group, 
					      ZOOM_CONTROL_ACTION);	
	if (best_fit) {
		ephy_zoom_action_set_zoom_level (EPHY_ZOOM_ACTION (action), 
						 EPHY_ZOOM_BEST_FIT);
	} else if (page_width) {
		ephy_zoom_action_set_zoom_level (EPHY_ZOOM_ACTION (action), 
						 EPHY_ZOOM_FIT_WIDTH);
	}
}

void
ev_window_open_page_label (EvWindow   *ev_window, 
			   const char *label)
{
	if (ev_window->priv->page_cache) {
		ev_page_cache_set_page_label (ev_window->priv->page_cache, 
					      label);
	}
}

gboolean
ev_window_is_empty (const EvWindow *ev_window)
{
	g_return_val_if_fail (EV_IS_WINDOW (ev_window), FALSE);

	return (ev_window->priv->document == NULL) && 
		(ev_window->priv->load_job == NULL) &&
		(ev_window->priv->xfer_job == NULL);
}

static void
unable_to_load_dialog_response_cb (GtkWidget *dialog,
			           gint       response_id,
			           EvWindow  *ev_window)
{
	gtk_widget_destroy (dialog);
}

static void
unable_to_load (EvWindow   *ev_window,
		const char *error_message)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (GTK_WINDOW (ev_window),
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_CLOSE,
					 _("Unable to open document"));
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  "%s", error_message);
	g_signal_connect (dialog, "response",
			  G_CALLBACK (unable_to_load_dialog_response_cb),
			  ev_window);
	gtk_widget_show (dialog);
}

static void
update_window_title (EvDocument *document, GParamSpec *pspec, EvWindow *ev_window)
{
	char *title = NULL;
	char *doc_title = NULL;
	gboolean password_needed;

	password_needed = (ev_window->priv->password_document != NULL);
	if (document && ev_window->priv->page_cache) {
		doc_title = g_strdup (ev_page_cache_get_title (ev_window->priv->page_cache));

		/* Make sure we get a valid title back */
		if (doc_title) {
			if (doc_title[0] == '\000' ||
			    !g_utf8_validate (doc_title, -1, NULL)) {
				doc_title = NULL;
			}
		}
	}

	if (doc_title) {
		char *p;

		for (p = doc_title; *p; ++p) {
			/* an '\n' byte is always ASCII, no need for UTF-8 special casing */
			if (*p == '\n')
				*p = ' ';
		}
	}

	if (doc_title == NULL && ev_window->priv->uri) {
		char *display_name;

		display_name = gnome_vfs_format_uri_for_display (ev_window->priv->uri);
		doc_title = g_path_get_basename (display_name);
		g_free (display_name);
	}

	if (password_needed) {
		if (doc_title == NULL) {
			title = g_strdup (_("Document Viewer - Password Required"));
		} else {
			title = g_strdup_printf (_("%s - Password Required"), doc_title);
		}
	} else {
		if (doc_title == NULL) {
			title = g_strdup (_("Document Viewer"));
		} else {
			title = g_strdup (doc_title);
		}
	}

	gtk_window_set_title (GTK_WINDOW (ev_window), title);

	g_free (doc_title);
	g_free (title);
}

static void
find_changed_cb (EvDocument *document, int page, EvWindow *ev_window)
{
	update_action_sensitivity (ev_window);
}

static void
page_changed_cb (EvPageCache *page_cache,
		 gint         page,
		 EvWindow    *ev_window)
{
	update_action_sensitivity (ev_window);

	if (ev_window->priv->uri) {
		ev_metadata_manager_set_int (ev_window->priv->uri, "page", page);
	}
}

static void
update_document_mode (EvWindow *window, EvDocumentMode mode)
{
	if (mode == EV_DOCUMENT_MODE_PRESENTATION) {
		ev_window_run_presentation (window);
	}
	else if (mode == EV_DOCUMENT_MODE_FULL_SCREEN) {
		ev_window_run_fullscreen (window);
	}
}

static void
setup_document_from_metadata (EvWindow *window)
{
	char *uri = window->priv->uri;
	GValue page = { 0, };

	/* Page */
	if (uri && ev_metadata_manager_get (uri, "page", &page)) {
		ev_page_cache_set_current_page (window->priv->page_cache,
						g_value_get_int (&page));
	}
}

static void
ev_window_setup_document (EvWindow *ev_window)
{
	const EvDocumentInfo *info;
	EvDocument *document;
	EvView *view = EV_VIEW (ev_window->priv->view);
	EvSidebar *sidebar = EV_SIDEBAR (ev_window->priv->sidebar);
	GtkAction *action;

	document = ev_window->priv->document;
	ev_window->priv->page_cache = ev_page_cache_get (ev_window->priv->document);
	g_signal_connect (ev_window->priv->page_cache, "page-changed", G_CALLBACK (page_changed_cb), ev_window);

	g_signal_connect_object (G_OBJECT (document),
				 "notify::title",
				 G_CALLBACK (update_window_title),
				 ev_window, 0);
	if (EV_IS_DOCUMENT_FIND (document)) {
		g_signal_connect_object (G_OBJECT (document),
				         "find_changed",
				         G_CALLBACK (find_changed_cb),	
				         ev_window, 0);
	}

	ev_window_set_page_mode (ev_window, PAGE_MODE_DOCUMENT);

	ev_sidebar_set_document (sidebar, document);

	if (ev_page_cache_get_n_pages (ev_window->priv->page_cache) > 0) {
		ev_view_set_document (view, document);
	}

	update_window_title (document, NULL, ev_window);
	action = gtk_action_group_get_action (ev_window->priv->action_group, PAGE_SELECTOR_ACTION);
	ev_page_action_set_document (EV_PAGE_ACTION (action), document);
	update_action_sensitivity (ev_window);

	info = ev_page_cache_get_info (ev_window->priv->page_cache);
	update_document_mode (ev_window, info->mode);

	if (ev_window->priv->properties) {
		ev_properties_dialog_set_document (EV_PROPERTIES_DIALOG (ev_window->priv->properties),
					           ev_window->priv->document);
	}

	setup_document_from_metadata (ev_window);
}

static void
password_dialog_response (GtkWidget *password_dialog,
			  gint       response_id,
			  EvWindow  *ev_window)
{
	char *password;
	
	if (response_id == GTK_RESPONSE_OK) {
		EvDocument *document;
		gchar *uri;

		password = ev_password_dialog_get_password (EV_PASSWORD_DIALOG (password_dialog));
		if (password) {
			ev_document_doc_mutex_lock ();
			ev_document_security_set_password (EV_DOCUMENT_SECURITY (ev_window->priv->password_document),
							   password);
			ev_document_doc_mutex_unlock ();
		}
		g_free (password);

		ev_password_dialog_save_password (EV_PASSWORD_DIALOG (password_dialog));

		document = ev_window->priv->password_document;
		uri = ev_window->priv->password_uri;

		ev_window->priv->password_document = NULL;
		ev_window->priv->password_uri = NULL;
		
		ev_job_queue_add_job (ev_window->priv->load_job, EV_JOB_PRIORITY_HIGH);
		
    		gtk_widget_destroy (password_dialog);
			
		g_object_unref (document);
		g_free (uri);

		return;
	}

	gtk_widget_set_sensitive (ev_window->priv->password_view, TRUE);
	gtk_widget_destroy (password_dialog);
}

/* Called either by ev_window_load_job_cb or by the "unlock" callback on the
 * password_view page.  It assumes that ev_window->priv->password_* has been set
 * correctly.  These are cleared by password_dialog_response() */

static void
ev_window_popup_password_dialog (EvWindow *ev_window)
{
	g_assert (ev_window->priv->password_document);
	g_assert (ev_window->priv->password_uri);

	gtk_widget_set_sensitive (ev_window->priv->password_view, FALSE);

	update_window_title (ev_window->priv->password_document, NULL, ev_window);
	if (ev_window->priv->password_dialog == NULL) {
		ev_window->priv->password_dialog =
 			g_object_new (EV_TYPE_PASSWORD_DIALOG, "uri", ev_window->priv->password_uri, NULL);
		gtk_window_set_transient_for (GTK_WINDOW (ev_window->priv->password_dialog), GTK_WINDOW (ev_window));

		g_object_add_weak_pointer (G_OBJECT (ev_window->priv->password_dialog),
					   (gpointer *) &(ev_window->priv->password_dialog));
		g_signal_connect (ev_window->priv->password_dialog,
				  "response",
				  G_CALLBACK (password_dialog_response),
				  ev_window);
		gtk_widget_show (ev_window->priv->password_dialog);
	} else {
		ev_password_dialog_set_bad_pass (EV_PASSWORD_DIALOG (ev_window->priv->password_dialog));
	}
}


static void
ev_window_clear_jobs (EvWindow *ev_window)
{
    if (ev_window->priv->load_job != NULL) {

	if (!ev_window->priv->load_job->finished)
	        ev_job_queue_remove_job (ev_window->priv->load_job);

	g_signal_handlers_disconnect_by_func (ev_window->priv->load_job, ev_window_load_job_cb, ev_window);
    	g_object_unref (ev_window->priv->load_job);
	ev_window->priv->load_job = NULL;
    }

    if (ev_window->priv->xfer_job != NULL) {

	if (!ev_window->priv->xfer_job->finished)
	        ev_job_queue_remove_job (ev_window->priv->xfer_job);

	g_signal_handlers_disconnect_by_func (ev_window->priv->xfer_job, ev_window_xfer_job_cb, ev_window);
    	g_object_unref (ev_window->priv->xfer_job);
	ev_window->priv->xfer_job = NULL;
    }
}

static void
ev_window_clear_local_uri (EvWindow *ev_window)
{
    char *filename;
    
    if (ev_window->priv->local_uri) {
	    filename = g_filename_from_uri (ev_window->priv->local_uri, NULL, NULL);
	    if (filename != NULL) {
		    unlink (filename);
		    g_free (filename);
	    }
	    g_free (ev_window->priv->local_uri);
	    ev_window->priv->local_uri = NULL;
    }
}

/* This callback will executed when load job will be finished.
 *
 * Since the flow of the error dialog is very confusing, we assume that both
 * document and uri will go away after this function is called, and thus we need
 * to ref/dup them.  Additionally, it needs to clear
 * ev_window->priv->password_{uri,document}, and thus people who call this
 * function should _not_ necessarily expect those to exist after being
 * called. */
static void
ev_window_load_job_cb  (EvJobLoad *job,
			gpointer data)
{
	EvWindow *ev_window = EV_WINDOW (data);
	EvDocument *document = EV_JOB (job)->document;

	g_assert (document);
	g_assert (document != ev_window->priv->document);
	g_assert (job->uri);

	if (ev_window->priv->password_document) {
		g_object_unref (ev_window->priv->password_document);
		ev_window->priv->password_document = NULL;
	}
	if (ev_window->priv->password_uri) {
		g_free (ev_window->priv->password_uri);
		ev_window->priv->password_uri = NULL;
	}

	/* Success! */
	if (job->error == NULL) {
		if (ev_window->priv->document)
			g_object_unref (ev_window->priv->document);
		ev_window->priv->document = g_object_ref (document);
		ev_window_setup_document (ev_window);
		
		ev_window_add_recent (ev_window, ev_window->priv->uri);
		ev_window_clear_jobs (ev_window);
		
		return;
	}

	if (job->error->domain == EV_DOCUMENT_ERROR &&
	    job->error->code == EV_DOCUMENT_ERROR_ENCRYPTED) {
		gchar *base_name, *file_name;

		ev_window->priv->password_document = g_object_ref (document);
		ev_window->priv->password_uri = g_strdup (job->uri);

		file_name = gnome_vfs_format_uri_for_display (job->uri);
		base_name = g_path_get_basename (file_name);
		ev_password_view_set_file_name (EV_PASSWORD_VIEW (ev_window->priv->password_view),
						base_name);
		g_free (file_name);
		g_free (base_name);
		ev_window_set_page_mode (ev_window, PAGE_MODE_PASSWORD);

		ev_window_popup_password_dialog (ev_window);
	} else {
		unable_to_load (ev_window, job->error->message);
	}	

	return;
}

static void
ev_window_xfer_job_cb  (EvJobXfer *job,
			gpointer data)
{
	EvWindow *ev_window = EV_WINDOW (data);


	
	if (job->error != NULL) {
		unable_to_load (ev_window, job->error->message);
		ev_window_clear_jobs (ev_window);
	} else {
		char *uri;
		
		EvDocument *document = g_object_ref (EV_JOB (job)->document);
		
		if (job->local_uri) {
			ev_window->priv->local_uri = g_strdup (job->local_uri);
			uri = ev_window->priv->local_uri;
		} else {
			ev_window->priv->local_uri = NULL;
			uri = ev_window->priv->uri;
		}
		
		ev_window_clear_jobs (ev_window);
		
		ev_window->priv->load_job = ev_job_load_new (document, uri);
		g_signal_connect (ev_window->priv->load_job,
				  "finished",
				  G_CALLBACK (ev_window_load_job_cb),
				  ev_window);
		ev_job_queue_add_job (ev_window->priv->load_job, EV_JOB_PRIORITY_HIGH);
		g_object_unref (document);
	}		
}

static void
update_sidebar_visibility (EvWindow *window)
{
	char *uri = window->priv->uri;
	GValue sidebar_visibility = { 0, };

	if (uri && ev_metadata_manager_get (uri, "sidebar_visibility", &sidebar_visibility)) {
		set_widget_visibility (window->priv->sidebar,
				       g_value_get_boolean (&sidebar_visibility));
	}
}

static void
setup_view_from_metadata (EvWindow *window)
{
	EvView *view = EV_VIEW (window->priv->view);
	char *uri = window->priv->uri;
	GEnumValue *enum_value;
	GValue width = { 0, };
	GValue height = { 0, };
	GValue maximized = { 0, };
	GValue x = { 0, };
	GValue y = { 0, };
	GValue sizing_mode = { 0, };
	GValue zoom = { 0, };
	GValue continuous = { 0, };
	GValue dual_page = { 0, };
	GValue presentation = { 0, };
	GValue fullscreen = { 0, };
	GValue rotation = { 0, };
	GValue sidebar_size = { 0, };
	GValue sidebar_page = { 0, };

	if (window->priv->uri == NULL) {
		return;
	}

	/* Window size */
	if (!GTK_WIDGET_VISIBLE (window)) {
		gboolean restore_size = TRUE;

		if (ev_metadata_manager_get (uri, "window_maximized", &maximized)) {
			if (g_value_get_boolean (&maximized)) {
				gtk_window_maximize (GTK_WINDOW (window));
				restore_size = FALSE;
			}
		}

		if (restore_size &&
		    ev_metadata_manager_get (uri, "window_x", &x) &&
		    ev_metadata_manager_get (uri, "window_y", &y) &&
		    ev_metadata_manager_get (uri, "window_width", &width) &&
	            ev_metadata_manager_get (uri, "window_height", &height)) {
			gtk_window_set_default_size (GTK_WINDOW (window),
						     g_value_get_int (&width),
						     g_value_get_int (&height));
			gtk_window_move (GTK_WINDOW (window), g_value_get_int (&x),
					 g_value_get_int (&y));
		}
	}

	/* Sizing mode */
	if (ev_metadata_manager_get (uri, "sizing_mode", &sizing_mode)) {
		enum_value = g_enum_get_value_by_nick
			(EV_SIZING_MODE_CLASS, g_value_get_string (&sizing_mode));
		g_value_unset (&sizing_mode);
		ev_view_set_sizing_mode (view, enum_value->value);
	}

	/* Zoom */
	if (ev_metadata_manager_get (uri, "zoom", &zoom) &&
	    ev_view_get_sizing_mode (view) == EV_SIZING_FREE) {
		ev_view_set_zoom (view, g_value_get_double (&zoom), FALSE);
	}

	/* Continuous */
	if (ev_metadata_manager_get (uri, "continuous", &continuous)) {
		ev_view_set_continuous (view, g_value_get_boolean (&continuous));
	}

	/* Dual page */
	if (ev_metadata_manager_get (uri, "dual-page", &dual_page)) {
		ev_view_set_dual_page (view, g_value_get_boolean (&dual_page));
	}

	/* Presentation */
	if (ev_metadata_manager_get (uri, "presentation", &presentation)) {
		if (g_value_get_boolean (&presentation)) {
			ev_window_run_presentation (window);
		}
	}

	/* Fullscreen */
	if (ev_metadata_manager_get (uri, "fullscreen", &fullscreen)) {
		if (g_value_get_boolean (&fullscreen)) {
			ev_window_run_fullscreen (window);
		}
	}

	/* Rotation */
	if (ev_metadata_manager_get (uri, "rotation", &rotation)) {
		if (g_value_get_int (&rotation)) {
			switch (g_value_get_int (&rotation)) {
			case 90:
				ev_view_set_rotation (view, 90);
				break;
			case 180:
				ev_view_set_rotation (view, 180);
				break;
			case 270:
				ev_view_set_rotation (view, 270);
				break;
			default:
				break;
			}
		}
	}

	/* Sidebar */
	if (ev_metadata_manager_get (uri, "sidebar_size", &sidebar_size)) {
		gtk_paned_set_position (GTK_PANED (window->priv->hpaned),
					g_value_get_int (&sidebar_size));
	}

	if (ev_metadata_manager_get (uri, "sidebar_page", &sidebar_page)) {
		const char *page_id = g_value_get_string (&sidebar_page);

		if (strcmp (page_id, "links") == 0) {
			ev_sidebar_set_page (EV_SIDEBAR (window->priv->sidebar),
					     window->priv->sidebar_links);
		} else if (strcmp (page_id, "thumbnails")) {
			ev_sidebar_set_page (EV_SIDEBAR (window->priv->sidebar),
					     window->priv->sidebar_thumbs);
		}
	}

	update_sidebar_visibility (window);
}

void
ev_window_open_uri (EvWindow *ev_window, const char *uri)
{
	if (ev_window->priv->password_dialog)
		gtk_widget_destroy (ev_window->priv->password_dialog);

	g_free (ev_window->priv->uri);
	ev_window->priv->uri = g_strdup (uri);

	setup_view_from_metadata (ev_window);
	
	ev_window_clear_jobs (ev_window);
	ev_window_clear_local_uri (ev_window);
	
	ev_window->priv->xfer_job = ev_job_xfer_new (uri);
	g_signal_connect (ev_window->priv->xfer_job,
			  "finished",
			  G_CALLBACK (ev_window_xfer_job_cb),
			  ev_window);
	ev_job_queue_add_job (ev_window->priv->xfer_job, EV_JOB_PRIORITY_HIGH);
}

static void
file_open_dialog_response_cb (GtkWidget *chooser,
			      gint       response_id,
			      EvWindow  *ev_window)
{
	if (response_id == GTK_RESPONSE_OK) {
		GSList *uris;

		uris = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (chooser));

		ev_application_open_uri_list (EV_APP, uris, GDK_CURRENT_TIME);
	
		g_slist_foreach (uris, (GFunc)g_free, NULL);	
		g_slist_free (uris);
	}

	gtk_widget_destroy (chooser);
}

static void
ev_window_cmd_file_open (GtkAction *action, EvWindow *window)
{
	GtkWidget *chooser;

	chooser = gtk_file_chooser_dialog_new (_("Open Document"),
					       GTK_WINDOW (window),
					       GTK_FILE_CHOOSER_ACTION_OPEN,
					       GTK_STOCK_CANCEL,
					       GTK_RESPONSE_CANCEL,
					       GTK_STOCK_OPEN, GTK_RESPONSE_OK,
					       NULL);

	ev_document_types_add_filters (chooser, NULL);
	gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (chooser), TRUE);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), FALSE);

	g_signal_connect (chooser, "response",
			  G_CALLBACK (file_open_dialog_response_cb),
			  window);

	gtk_widget_show (chooser);
}

static void
ev_window_cmd_recent_file_activate (GtkAction *action,
        	                    EvWindow *ev_window)
{
	char *uri;
	EggRecentItem *item;

	item = egg_recent_view_uimanager_get_item (ev_window->priv->recent_view,
						   action);

	uri = egg_recent_item_get_uri (item);

	ev_application_open_uri (EV_APP, uri, NULL, GDK_CURRENT_TIME, NULL);
	
	g_free (uri);
}

static void
ev_window_add_recent (EvWindow *window, const char *filename)
{
	EggRecentItem *item;

	item = egg_recent_item_new_from_uri (filename);
	egg_recent_item_add_group (item, "Evince");
	egg_recent_model_add_full (ev_application_get_recent_model (EV_APP), item);
}

static void
ev_window_setup_recent (EvWindow *ev_window)
{

	ev_window->priv->recent_view = egg_recent_view_uimanager_new (ev_window->priv->ui_manager,
								      "/MainMenu/FileMenu/RecentFilesMenu",
								      G_CALLBACK (ev_window_cmd_recent_file_activate), 
								      ev_window);	

        egg_recent_view_uimanager_show_icons (EGG_RECENT_VIEW_UIMANAGER (ev_window->priv->recent_view), FALSE);

	egg_recent_view_set_model (EGG_RECENT_VIEW (ev_window->priv->recent_view),
				   ev_application_get_recent_model (EV_APP));

	egg_recent_view_uimanager_set_trailing_sep (ev_window->priv->recent_view, TRUE);
	
	g_signal_connect (ev_window->priv->recent_view, "activate",
			G_CALLBACK (ev_window_cmd_recent_file_activate), ev_window);
}

/* FIXME
static gboolean
overwrite_existing_file (GtkWindow *window, const gchar *file_name)
{
	GtkWidget *msgbox;
	gchar *utf8_file_name;
	AtkObject *obj;
	gint ret;

	utf8_file_name = g_filename_to_utf8 (file_name, -1, NULL, NULL, NULL);
	msgbox = gtk_message_dialog_new (
		window,
		(GtkDialogFlags)GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_QUESTION,
		GTK_BUTTONS_NONE,
		_("A file named \"%s\" already exists."),
		utf8_file_name);
	g_free (utf8_file_name);

	gtk_message_dialog_format_secondary_text (
		GTK_MESSAGE_DIALOG (msgbox),
		_("Do you want to replace it with the one you are saving?"));

	gtk_dialog_add_button (GTK_DIALOG (msgbox),
			       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

	gtk_dialog_add_button (GTK_DIALOG (msgbox),
			       _("_Replace"), GTK_RESPONSE_YES);

	gtk_dialog_set_default_response (GTK_DIALOG (msgbox),
					 GTK_RESPONSE_CANCEL);

	obj = gtk_widget_get_accessible (msgbox);

	if (GTK_IS_ACCESSIBLE (obj))
		atk_object_set_name (obj, _("Question"));

	ret = gtk_dialog_run (GTK_DIALOG (msgbox));
	gtk_widget_destroy (msgbox);

	return (ret == GTK_RESPONSE_YES);
}
*/

static void
save_error_dialog (GtkWindow *window, const gchar *file_name)
{
	GtkWidget *error_dialog;

	error_dialog = gtk_message_dialog_new (
		window,
		(GtkDialogFlags)GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_ERROR,
		GTK_BUTTONS_CLOSE,
		_("The file could not be saved as \"%s\"."),
		file_name);

	/* Easy way to make the text bold while keeping the string
	 * above free from pango markup (FIXME ?) */
	gtk_message_dialog_format_secondary_text (
		GTK_MESSAGE_DIALOG (error_dialog), " ");

	gtk_dialog_run (GTK_DIALOG (error_dialog));
	gtk_widget_destroy (error_dialog);
}

static void
file_save_dialog_response_cb (GtkWidget *fc,
			      gint       response_id,
			      EvWindow  *ev_window)
{
	gboolean success;

	if (response_id == GTK_RESPONSE_OK) {
		const char *uri;

		uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (fc));

		ev_document_doc_mutex_lock ();
		success = ev_document_save (ev_window->priv->document, uri, NULL);
		ev_document_doc_mutex_unlock ();

		if (!success) {
			save_error_dialog (GTK_WINDOW (fc), uri);
		}
	}

	gtk_widget_destroy (fc);
}

static void
ev_window_cmd_save_as (GtkAction *action, EvWindow *ev_window)
{
	GtkWidget *fc;
	gchar *base_name;
	gchar *file_name;

	fc = gtk_file_chooser_dialog_new (
		_("Save a Copy"),
		GTK_WINDOW (ev_window), GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_OK,
		NULL);

	ev_document_types_add_filters (fc, ev_window->priv->document);
	gtk_dialog_set_default_response (GTK_DIALOG (fc), GTK_RESPONSE_OK);
	
	file_name = gnome_vfs_format_uri_for_display (ev_window->priv->uri);
	base_name = g_path_get_basename (file_name);
	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (fc), base_name);
	g_free (file_name);
	g_free (base_name);

	g_signal_connect (fc, "response",
			  G_CALLBACK (file_save_dialog_response_cb),
			  ev_window);

	gtk_widget_show (fc);
}

static gboolean
using_pdf_printer (GnomePrintConfig *config)
{
	const guchar *driver;

	driver = gnome_print_config_get (
		config, (const guchar *)"Settings.Engine.Backend.Driver");

	if (driver) {
		if (!strcmp ((const gchar *)driver, "gnome-print-pdf"))
			return TRUE;
		else
			return FALSE;
	}

	return FALSE;
}

static gboolean
using_postscript_printer (GnomePrintConfig *config)
{
	const guchar *driver;
	const guchar *transport;

	driver = gnome_print_config_get (
		config, (const guchar *)"Settings.Engine.Backend.Driver");

	transport = gnome_print_config_get (
		config, (const guchar *)"Settings.Transport.Backend");

	if (driver) {
		if (!strcmp ((const gchar *)driver, "gnome-print-ps"))
			return TRUE;
		else
			return FALSE;
	} else 	if (transport) { /* these transports default to PostScript */
		if (!strcmp ((const gchar *)transport, "CUPS"))
			return TRUE;
		else if (!strcmp ((const gchar *)transport, "LPD"))
			return TRUE;
	}

	return FALSE;
}

static GnomePrintConfig *
load_print_config_from_file (void)
{
	GnomePrintConfig *print_config = NULL;
	char *file_name, *contents = NULL;

	file_name = g_build_filename (ev_dot_dir (), PRINT_CONFIG_FILENAME,
				      NULL);

	if (g_file_get_contents (file_name, &contents, NULL, NULL)) {
		print_config = gnome_print_config_from_string (contents, 0);
		g_free (contents);
	}

	if (print_config == NULL) {
		print_config = gnome_print_config_default ();
	}

	g_free (file_name);

	return print_config;
}

static void
save_print_config_to_file (GnomePrintConfig *config)
{
	char *file_name, *str;

	g_return_if_fail (config != NULL);

	str = gnome_print_config_to_string (config, 0);
	if (str == NULL) return;

	file_name = g_build_filename (ev_dot_dir (),
				      PRINT_CONFIG_FILENAME,
				      NULL);

#ifdef HAVE_G_FILE_SET_CONTENTS
	g_file_set_contents (file_name, str, -1, NULL);
#else
	ev_file_set_contents (file_name, str, -1, NULL);
#endif

	g_free (file_name);
	g_free (str);
}

static void
ev_window_print (EvWindow *window)
{
	EvPageCache *page_cache;
	int last_page;

	page_cache = ev_page_cache_get (window->priv->document);
	last_page = ev_page_cache_get_n_pages (page_cache);

	ev_window_print_range (window, 1, -1);
}

const char *
ev_window_get_uri (EvWindow *ev_window)
{
	return ev_window->priv->uri;
}

void
ev_window_print_range (EvWindow *ev_window, int first_page, int last_page)
{
	GnomePrintConfig *config;
	GnomePrintJob *job;
	GtkWidget *print_dialog;
	gchar *pages_label;
	EvPrintJob *print_job = NULL;
	EvPageCache *page_cache;

        g_return_if_fail (EV_IS_WINDOW (ev_window));
	g_return_if_fail (ev_window->priv->document != NULL);

	page_cache = ev_page_cache_get (ev_window->priv->document);
	if (last_page == -1) {
		last_page = ev_page_cache_get_n_pages (page_cache);
	}

	config = load_print_config_from_file ();
	job = gnome_print_job_new (config);

	print_dialog = gnome_print_dialog_new (job, (guchar *) _("Print"),
					       (GNOME_PRINT_DIALOG_RANGE |
						GNOME_PRINT_DIALOG_COPIES));

	pages_label = g_strconcat (_("Pages"), " ", NULL);
	gnome_print_dialog_construct_range_page (GNOME_PRINT_DIALOG (print_dialog),
						 GNOME_PRINT_RANGE_ALL |
						 GNOME_PRINT_RANGE_RANGE,
						 first_page, last_page,
						 NULL, (const guchar *)pages_label);
	g_free (pages_label);
						 
	gtk_dialog_set_response_sensitive (GTK_DIALOG (print_dialog),
					   GNOME_PRINT_DIALOG_RESPONSE_PREVIEW,
					   FALSE);

	while (TRUE) {
		int response;
		response = gtk_dialog_run (GTK_DIALOG (print_dialog));

		if (response != GNOME_PRINT_DIALOG_RESPONSE_PRINT)
			break;

		/* FIXME: Change this when we have the first backend
		 * that can print more than postscript
		 */
		if (using_pdf_printer (config)) {
			GtkWidget *dialog;

			dialog = gtk_message_dialog_new (
				GTK_WINDOW (print_dialog), GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
				_("Generating PDF is not supported"));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			
			continue;
		} else if (!using_postscript_printer (config)) {
			GtkWidget *dialog;

			dialog = gtk_message_dialog_new (
				GTK_WINDOW (print_dialog), GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
				_("Printing is not supported on this printer."));
			gtk_message_dialog_format_secondary_text (
				GTK_MESSAGE_DIALOG (dialog),
				_("You were trying to print to a printer using the \"%s\" driver. This program requires a PostScript printer driver."),
				gnome_print_config_get (
					config, (guchar *)"Settings.Engine.Backend.Driver"));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);

			continue;
		}

		save_print_config_to_file (config);

		print_job = g_object_new (EV_TYPE_PRINT_JOB,
					  "gnome_print_job", job,
					  "document", ev_window->priv->document,
					  "print_dialog", print_dialog,
					  NULL);
		break;
	}

	g_object_unref (job);

	gtk_widget_destroy (print_dialog);

	if (print_job != NULL) {
		ev_print_job_print (print_job, GTK_WINDOW (ev_window));
		g_object_unref (print_job);
	}

	g_object_unref (config);
}

static void
ev_window_cmd_file_print (GtkAction *action, EvWindow *ev_window)
{
	ev_window_print (ev_window);
}

static void
ev_window_cmd_file_properties (GtkAction *action, EvWindow *ev_window)
{
	if (ev_window->priv->properties == NULL) {
		ev_window->priv->properties = ev_properties_dialog_new ();
		ev_properties_dialog_set_document (EV_PROPERTIES_DIALOG (ev_window->priv->properties),
					           ev_window->priv->document);
		g_object_add_weak_pointer (G_OBJECT (ev_window->priv->properties),
					   (gpointer *) &(ev_window->priv->properties));
		gtk_window_set_transient_for (GTK_WINDOW (ev_window->priv->properties),
					      GTK_WINDOW (ev_window));
	}

	gtk_widget_show (ev_window->priv->properties);
}
					
static void
ev_window_cmd_file_close_window (GtkAction *action, EvWindow *ev_window)
{
	g_return_if_fail (EV_IS_WINDOW (ev_window));

	gtk_widget_destroy (GTK_WIDGET (ev_window));
}

static void
find_not_supported_dialog (EvWindow   *ev_window)
{
	GtkWidget *dialog;

	/* If you change this so it isn't modal, be sure you don't
	 * allow multiple copies of the dialog...
	 */

 	dialog = gtk_message_dialog_new (GTK_WINDOW (ev_window),
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_CLOSE,
					 _("The \"Find\" feature will not work with this document"));
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
 						  _("Searching for text is only supported for PDF documents."));
	gtk_dialog_run (GTK_DIALOG (dialog));
 	gtk_widget_destroy (dialog);
}

static void
ev_window_cmd_edit_select_all (GtkAction *action, EvWindow *ev_window)
{
	g_return_if_fail (EV_IS_WINDOW (ev_window));

	ev_view_select_all (EV_VIEW (ev_window->priv->view));
}

static void
ev_window_cmd_edit_find (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

	if (ev_window->priv->document == NULL) {
		g_printerr ("We should have set the Find menu item insensitive since there's no document\n");
	} else if (!EV_IS_DOCUMENT_FIND (ev_window->priv->document)) {
		find_not_supported_dialog (ev_window);
	} else {
		update_chrome_flag (ev_window, EV_CHROME_FINDBAR, NULL, TRUE);

		gtk_widget_grab_focus (ev_window->priv->find_bar);
	}
}

static void
ev_window_cmd_edit_find_next (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

	ev_view_find_next (EV_VIEW (ev_window->priv->view));
}

static void
ev_window_cmd_edit_copy (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

	ev_view_copy (EV_VIEW (ev_window->priv->view));
}

static void
ev_window_update_fullscreen_popup (EvWindow *window)
{
	GtkWidget *popup = window->priv->fullscreen_popup;
	int popup_width, popup_height;
	GdkScreen *screen;
	GdkRectangle screen_rect;
	gboolean toolbar;

	g_return_if_fail (popup != NULL);

	if (GTK_WIDGET (window)->window == NULL)
		return;

	toolbar = (window->priv->chrome & EV_CHROME_TOOLBAR) != 0 || 
		  (window->priv->chrome & EV_CHROME_RAISE_TOOLBAR) != 0;
	popup_width = popup->requisition.width;
	popup_height = popup->requisition.height;

	screen = gtk_widget_get_screen (GTK_WIDGET (window));
	gdk_screen_get_monitor_geometry (screen,
			gdk_screen_get_monitor_at_window
                        (screen,
                         GTK_WIDGET (window)->window),
                         &screen_rect);
	if (toolbar) {
		gtk_widget_set_size_request (popup,
					     screen_rect.width,
					     -1);
		gtk_window_move (GTK_WINDOW (popup),
				 screen_rect.x,
				 screen_rect.y);

	} else {
		if (gtk_widget_get_direction (popup) == GTK_TEXT_DIR_RTL)
		{
			gtk_window_move (GTK_WINDOW (popup),
					 screen_rect.x,
					 screen_rect.y);
		} else {
			gtk_window_move (GTK_WINDOW (popup),
					 screen_rect.x + screen_rect.width - popup_width,
					 screen_rect.y);
		}
	}
}

static void
screen_size_changed_cb (GdkScreen *screen,
			EvWindow *window)
{
	ev_window_update_fullscreen_popup (window);
}

static void
ev_window_sidebar_position_change_cb (GObject *object, GParamSpec *pspec,
				      EvWindow *ev_window)
{
	if (ev_window->priv->uri) {
		ev_metadata_manager_set_int (ev_window->priv->uri, "sidebar_size",
					     gtk_paned_get_position (GTK_PANED (object)));
	}
}

static void
destroy_fullscreen_popup (EvWindow *window)
{
	if (window->priv->fullscreen_popup != NULL)
	{
		gtk_widget_destroy (window->priv->fullscreen_popup);
		window->priv->fullscreen_popup = NULL;
	}
}

static void
exit_fullscreen_button_clicked_cb (GtkWidget *button, EvWindow *window)
{
	GtkAction *action;

	action = gtk_action_group_get_action (window->priv->action_group, "ViewFullscreen");
	g_return_if_fail (action != NULL);

	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), FALSE);
}

static void
fullscreen_popup_size_request_cb (GtkWidget *popup, GtkRequisition *req, EvWindow *window)
{
	ev_window_update_fullscreen_popup (window);
}

static gboolean
fullscreen_timeout_cb (gpointer data)
{
	EvWindow *window = EV_WINDOW (data);

	g_object_set (window->priv->fullscreen_popup, "visible", FALSE, NULL);
	ev_view_hide_cursor (EV_VIEW (window->priv->view));
	g_source_unref (window->priv->fullscreen_timeout_source);
	window->priv->fullscreen_timeout_source = NULL;

	return FALSE;
}

static void
fullscreen_set_timeout (EvWindow *window)
{
	GSource *source;

	if (window->priv->fullscreen_timeout_source != NULL) {
		g_source_unref (window->priv->fullscreen_timeout_source);
		g_source_destroy (window->priv->fullscreen_timeout_source);
	}

	source = g_timeout_source_new (FULLSCREEN_TIMEOUT);
	g_source_set_callback (source, fullscreen_timeout_cb, window, NULL);
	g_source_attach (source, NULL);
	window->priv->fullscreen_timeout_source = source;
}

static void
fullscreen_clear_timeout (EvWindow *window)
{
	if (window->priv->fullscreen_timeout_source != NULL) {
		g_source_unref (window->priv->fullscreen_timeout_source);
		g_source_destroy (window->priv->fullscreen_timeout_source);
	}

	window->priv->fullscreen_timeout_source = NULL;
	ev_view_show_cursor (EV_VIEW (window->priv->view));
}


static void
show_fullscreen_popup (EvWindow *window)
{
	if (!GTK_WIDGET_VISIBLE (window->priv->fullscreen_popup)) {
		g_object_set (window->priv->fullscreen_popup, "visible", TRUE, NULL);
		ev_view_show_cursor (EV_VIEW (window->priv->view));
	}

	fullscreen_set_timeout (window);	
}

static gboolean
fullscreen_motion_notify_cb (GtkWidget *widget,
			     GdkEventMotion *event,
			     gpointer user_data)
{
	EvWindow *window = EV_WINDOW (user_data);

	show_fullscreen_popup (window);

	return FALSE;
}

static gboolean
fullscreen_leave_notify_cb (GtkWidget *widget,
			    GdkEventCrossing *event,
			    gpointer user_data)
{
	EvWindow *window = EV_WINDOW (user_data);

	fullscreen_clear_timeout (window);

	return FALSE;
}

static GtkWidget *
ev_window_get_exit_fullscreen_button (EvWindow *window)
{
	GtkWidget *button, *icon, *label, *hbox;

	button = gtk_button_new ();
	g_signal_connect (button, "clicked",
			  G_CALLBACK (exit_fullscreen_button_clicked_cb),
			  window);
	gtk_widget_show (button);

	hbox = gtk_hbox_new (FALSE, 2);
	gtk_widget_show (hbox);
	gtk_container_add (GTK_CONTAINER (button), hbox);

	icon = gtk_image_new_from_stock (EV_STOCK_LEAVE_FULLSCREEN, GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (icon);
	gtk_box_pack_start (GTK_BOX (hbox), icon, FALSE, FALSE, 0);

	label = gtk_label_new (_("Leave Fullscreen"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	return button;
}

static GtkWidget *
ev_window_create_fullscreen_popup (EvWindow *window)
{
	GtkWidget *popup;
	GtkWidget *hbox;
	GtkWidget *button;
	GdkScreen *screen;

	window->priv->fullscreen_toolbar = egg_editable_toolbar_new_with_model
			(window->priv->ui_manager, ev_application_get_toolbars_model (EV_APP));

	popup = gtk_window_new (GTK_WINDOW_POPUP);
	hbox = gtk_hbox_new (FALSE, 0);
	button = ev_window_get_exit_fullscreen_button (window);

	gtk_container_add (GTK_CONTAINER (popup), hbox);
	gtk_box_pack_start (GTK_BOX (hbox), window->priv->fullscreen_toolbar,
			    TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

	gtk_widget_show (button);
	gtk_widget_show (hbox);

	gtk_window_set_resizable (GTK_WINDOW (popup), FALSE);

	screen = gtk_widget_get_screen (GTK_WIDGET (window));
	g_signal_connect_object (screen, "size-changed",
			         G_CALLBACK (screen_size_changed_cb),
				 window, 0);
	g_signal_connect_object (popup, "size_request",
			         G_CALLBACK (fullscreen_popup_size_request_cb),
				 window, 0);

	gtk_window_set_screen (GTK_WINDOW (popup),
			       gtk_widget_get_screen (GTK_WIDGET (window)));

	return popup;
}


static void
ev_window_update_fullscreen_action (EvWindow *window)
{
	GtkAction *action;

	action = gtk_action_group_get_action (window->priv->action_group, "ViewFullscreen");
	g_signal_handlers_block_by_func
		(action, G_CALLBACK (ev_window_cmd_view_fullscreen), window);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      ev_view_get_fullscreen (EV_VIEW (window->priv->view)));
	g_signal_handlers_unblock_by_func
		(action, G_CALLBACK (ev_window_cmd_view_fullscreen), window);
}

static void
ev_window_run_fullscreen (EvWindow *window)
{
	ev_view_set_fullscreen (EV_VIEW (window->priv->view), TRUE);
	if (window->priv->fullscreen_popup == NULL)
		window->priv->fullscreen_popup
			= ev_window_create_fullscreen_popup (window);

	update_chrome_visibility (window);
	gtk_widget_hide (window->priv->sidebar);

	g_object_set (G_OBJECT (window->priv->scrolled_window),
		      "shadow-type", GTK_SHADOW_NONE,
		      NULL);
	
	g_signal_connect (window->priv->view,
			  "motion-notify-event",
			  G_CALLBACK (fullscreen_motion_notify_cb),
			  window);
	g_signal_connect (window->priv->view,
			  "leave-notify-event",
			  G_CALLBACK (fullscreen_leave_notify_cb),
			  window);
	fullscreen_set_timeout (window);

	gtk_widget_grab_focus (window->priv->view);
	ev_window_update_fullscreen_action (window);
	gtk_window_fullscreen (GTK_WINDOW (window));
	ev_window_update_fullscreen_popup (window);

	if (window->priv->uri) {
		ev_metadata_manager_set_boolean (window->priv->uri, "fullscreen", TRUE);
	}
}

static void
ev_window_stop_fullscreen (EvWindow *window)
{
	EvView *view = EV_VIEW (window->priv->view);

	if (!ev_view_get_fullscreen (EV_VIEW (view)))
		return;

	ev_view_set_fullscreen (view, FALSE);
	g_object_set (G_OBJECT (window->priv->scrolled_window),
		      "shadow-type", GTK_SHADOW_IN,
		      NULL);

	fullscreen_clear_timeout (window);

	g_signal_handlers_disconnect_by_func (view,
					      (gpointer) fullscreen_motion_notify_cb,
					      window);
	g_signal_handlers_disconnect_by_func (view,
					      (gpointer) fullscreen_leave_notify_cb,
					      window);
	ev_window_update_fullscreen_action (window);
	gtk_window_unfullscreen (GTK_WINDOW (window));
	update_chrome_visibility (window);
	update_sidebar_visibility (window);

	if (window->priv->uri) {
		ev_metadata_manager_set_boolean (window->priv->uri, "fullscreen", FALSE);
	}
}

static void
ev_window_cmd_view_fullscreen (GtkAction *action, EvWindow *window)
{
	gboolean fullscreen;

        g_return_if_fail (EV_IS_WINDOW (window));
	ev_window_stop_presentation (window);

	fullscreen = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	if (fullscreen) {
		ev_window_run_fullscreen (window);
	} else {
		ev_window_stop_fullscreen (window);
	}
}

static void
ev_window_update_presentation_action (EvWindow *window)
{
	GtkAction *action;

	action = gtk_action_group_get_action (window->priv->action_group, "ViewPresentation");
	g_signal_handlers_block_by_func
		(action, G_CALLBACK (ev_window_cmd_view_presentation), window);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      ev_view_get_presentation (EV_VIEW (window->priv->view)));
	g_signal_handlers_unblock_by_func
		(action, G_CALLBACK (ev_window_cmd_view_presentation), window);
}

static void
ev_window_run_presentation (EvWindow *window)
{
	g_object_set (G_OBJECT (window->priv->scrolled_window),
		      "shadow-type", GTK_SHADOW_NONE,
		      NULL);

	gtk_widget_grab_focus (window->priv->view);
	ev_view_set_presentation (EV_VIEW (window->priv->view), TRUE);
	gtk_window_fullscreen (GTK_WINDOW (window));
	ev_window_update_presentation_action (window);
	update_chrome_visibility (window);
	gtk_widget_hide (window->priv->sidebar);

	if (window->priv->uri) {
		ev_metadata_manager_set_boolean (window->priv->uri, "presentation", TRUE);
	}
}

static void
ev_window_stop_presentation (EvWindow *window)
{
	if (!ev_view_get_presentation (EV_VIEW (window->priv->view)))
		return;

	g_object_set (G_OBJECT (window->priv->scrolled_window),
		      "shadow-type", GTK_SHADOW_IN,
		      NULL);
	ev_view_set_presentation (EV_VIEW (window->priv->view), FALSE);
	gtk_window_unfullscreen (GTK_WINDOW (window));
	ev_window_update_presentation_action (window);
	update_chrome_visibility (window);
	update_sidebar_visibility (window);

	if (window->priv->uri) {
		ev_metadata_manager_set_boolean (window->priv->uri, "presentation", FALSE);
	}
}

static void
ev_window_cmd_view_presentation (GtkAction *action, EvWindow *window)
{
	gboolean presentation;

        g_return_if_fail (EV_IS_WINDOW (window));
	ev_window_stop_fullscreen (window);

	presentation = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	if (presentation) {
		ev_window_run_presentation (window);
	} else {
		ev_window_stop_presentation (window);
	}
}

static gboolean
ev_window_focus_in_event (GtkWidget *widget, GdkEventFocus *event)
{
	EvWindow *window = EV_WINDOW (widget);
	EvWindowPrivate *priv = window->priv;
	gboolean fullscreen;

	g_object_get (priv->view,
		      "fullscreen", &fullscreen,
		      NULL);

	if (fullscreen)
		show_fullscreen_popup (window);

	return GTK_WIDGET_CLASS (ev_window_parent_class)->focus_in_event (widget, event);
}

static gboolean
ev_window_focus_out_event (GtkWidget *widget, GdkEventFocus *event)
{
	EvWindow *window = EV_WINDOW (widget);
	EvWindowPrivate *priv = window->priv;
	gboolean fullscreen;

	g_object_get (priv->view,
		      "fullscreen", &fullscreen,
		      NULL);

	if (fullscreen)
		gtk_widget_hide (priv->fullscreen_popup);

	return GTK_WIDGET_CLASS (ev_window_parent_class)->focus_out_event (widget, event);
}

static void
ev_window_screen_changed (GtkWidget *widget,
			  GdkScreen *old_screen)
{
	EvWindow *window = EV_WINDOW (widget);
	EvWindowPrivate *priv = window->priv;
	GdkScreen *screen;

	if (GTK_WIDGET_CLASS (ev_window_parent_class)->screen_changed) {
		GTK_WIDGET_CLASS (ev_window_parent_class)->screen_changed (widget, old_screen);
	}

	if (priv->fullscreen_popup != NULL) {
		g_signal_handlers_disconnect_by_func
			(old_screen, G_CALLBACK (screen_size_changed_cb), window);

		screen = gtk_widget_get_screen (widget);
		g_signal_connect_object (screen, "size-changed",
					 G_CALLBACK (screen_size_changed_cb),
					 window, 0);
		gtk_window_set_screen (GTK_WINDOW (priv->fullscreen_popup), screen);

		ev_window_update_fullscreen_popup (window);
	}
}

static void
ev_window_set_page_mode (EvWindow         *window,
			 EvWindowPageMode  page_mode)
{
	GtkWidget *child = NULL;
	GtkWidget *real_child;

	if (window->priv->page_mode == page_mode)
		return;

	window->priv->page_mode = page_mode;

	switch (page_mode) {
	case PAGE_MODE_DOCUMENT:
		child = window->priv->view;
		break;
	case PAGE_MODE_PASSWORD:
		child = window->priv->password_view;
		break;
	default:
		g_assert_not_reached ();
	}

	real_child = gtk_bin_get_child (GTK_BIN (window->priv->scrolled_window));
	if (child != real_child) {
		gtk_container_remove (GTK_CONTAINER (window->priv->scrolled_window),
				      real_child);
		gtk_container_add (GTK_CONTAINER (window->priv->scrolled_window),
				   child);
	}
	update_action_sensitivity (window);
}

static void
ev_window_cmd_edit_toolbar_cb (GtkDialog *dialog, gint response, gpointer data)
{
	EvWindow *ev_window = EV_WINDOW (data);
        egg_editable_toolbar_set_edit_mode
			(EGG_EDITABLE_TOOLBAR (ev_window->priv->toolbar), FALSE);
	ev_application_save_toolbars_model (EV_APP);
        gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
ev_window_cmd_edit_rotate_left (GtkAction *action, EvWindow *ev_window)
{
	ev_view_rotate_left (EV_VIEW (ev_window->priv->view));
}

static void
ev_window_cmd_edit_rotate_right (GtkAction *action, EvWindow *ev_window)
{
	ev_view_rotate_right (EV_VIEW (ev_window->priv->view));
}

static void
ev_window_cmd_edit_toolbar (GtkAction *action, EvWindow *ev_window)
{
	GtkWidget *dialog;
	GtkWidget *editor;

	dialog = gtk_dialog_new_with_buttons (_("Toolbar Editor"),
					      GTK_WINDOW (ev_window), 
				              GTK_DIALOG_DESTROY_WITH_PARENT, 
					      GTK_STOCK_CLOSE,
					      GTK_RESPONSE_CLOSE, 
					      NULL);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 500, 400);
	  
	editor = egg_toolbar_editor_new (ev_window->priv->ui_manager,
					 ev_application_get_toolbars_model (EV_APP));
	gtk_container_set_border_width (GTK_CONTAINER (editor), 5);
	gtk_box_set_spacing (GTK_BOX (EGG_TOOLBAR_EDITOR (editor)), 5);
             
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), editor);
	egg_toolbar_editor_load_actions (EGG_TOOLBAR_EDITOR (editor),
				         DATADIR "/evince-toolbar.xml");

	egg_editable_toolbar_set_edit_mode
		(EGG_EDITABLE_TOOLBAR (ev_window->priv->toolbar), TRUE);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (ev_window_cmd_edit_toolbar_cb),
			  ev_window);
	gtk_widget_show_all (dialog);
}

static void
ev_window_cmd_view_zoom_in (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

	ev_view_set_sizing_mode (EV_VIEW (ev_window->priv->view), EV_SIZING_FREE);
	ev_view_zoom_in (EV_VIEW (ev_window->priv->view));
}

static void
ev_window_cmd_view_zoom_out (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

	ev_view_set_sizing_mode (EV_VIEW (ev_window->priv->view), EV_SIZING_FREE);
	ev_view_zoom_out (EV_VIEW (ev_window->priv->view));
}

static void
ev_window_cmd_go_previous_page (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

	ev_page_cache_prev_page (ev_window->priv->page_cache);
}

static void
ev_window_cmd_go_next_page (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

	ev_page_cache_next_page (ev_window->priv->page_cache);
}

static void
ev_window_cmd_go_first_page (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

	ev_page_cache_set_current_page (ev_window->priv->page_cache, 0);
}

static void
ev_window_cmd_go_last_page (GtkAction *action, EvWindow *ev_window)
{
	int n_pages;

        g_return_if_fail (EV_IS_WINDOW (ev_window));

	n_pages = ev_page_cache_get_n_pages (ev_window->priv->page_cache);
	ev_page_cache_set_current_page (ev_window->priv->page_cache, n_pages - 1);
}

static void
ev_window_cmd_go_forward (GtkAction *action, EvWindow *ev_window)
{
	int n_pages, current_page;
	
        g_return_if_fail (EV_IS_WINDOW (ev_window));

	n_pages = ev_page_cache_get_n_pages (ev_window->priv->page_cache);
	current_page = ev_page_cache_get_current_page (ev_window->priv->page_cache);
	
	if (current_page + 10 < n_pages)
		ev_page_cache_set_current_page (ev_window->priv->page_cache, current_page + 10);
}

static void
ev_window_cmd_go_backward (GtkAction *action, EvWindow *ev_window)
{
	int current_page;
	
        g_return_if_fail (EV_IS_WINDOW (ev_window));

	current_page = ev_page_cache_get_current_page (ev_window->priv->page_cache);
	
	if (current_page - 10 >= 0)
		ev_page_cache_set_current_page (ev_window->priv->page_cache, current_page - 10);
}

static void
ev_window_cmd_view_reload (GtkAction *action, EvWindow *ev_window)
{
	char *uri;
	int page;

	g_return_if_fail (EV_IS_WINDOW (ev_window));

	page = ev_page_cache_get_current_page (ev_window->priv->page_cache);
	uri = g_strdup (ev_window->priv->uri);

	ev_window_open_uri (ev_window, uri);

	/* In case the number of pages in the document has changed. */
	page = CLAMP (page, 0, ev_page_cache_get_n_pages (ev_window->priv->page_cache));

	ev_page_cache_set_current_page (ev_window->priv->page_cache, page);

	g_free (uri);
}

static void
ev_window_cmd_help_contents (GtkAction *action, EvWindow *ev_window)
{
	GError *error = NULL;

        g_return_if_fail (EV_IS_WINDOW (ev_window));

	gnome_help_display ("evince.xml", NULL, &error);

	if(error != NULL) {
		g_warning (error->message);
		g_error_free (error);
	}
}

static void
ev_window_cmd_leave_fullscreen (GtkAction *action, EvWindow *window)
{
	gtk_window_unfullscreen (GTK_WINDOW (window));
}

static void
ev_window_cmd_escape (GtkAction *action, EvWindow *window)
{
	GtkWidget *widget;

	widget = gtk_window_get_focus (GTK_WINDOW (window));
	if (widget && gtk_widget_get_ancestor (widget, EGG_TYPE_FIND_BAR)) {
		update_chrome_flag (window, EV_CHROME_FINDBAR, NULL, FALSE);
		gtk_widget_grab_focus (window->priv->view);
	} else {
		gboolean fullscreen;
		gboolean presentation;

		g_object_get (window->priv->view,
			      "fullscreen", &fullscreen,
			      "presentation", &presentation,
			      NULL);

		if (fullscreen)
			ev_window_stop_fullscreen (window);
		if (presentation)
			ev_window_stop_presentation (window);

		if (fullscreen && presentation)
			g_warning ("Both fullscreen and presentation set somehow");
	}
}

static void
update_view_size (EvView *view, EvWindow *window)
{
	int width, height;
	GtkRequisition vsb_requisition;
	GtkRequisition hsb_requisition;
	int scrollbar_spacing;

	/* Calculate the width available for the */
	width = window->priv->scrolled_window->allocation.width;
	height = window->priv->scrolled_window->allocation.height;

	if (gtk_scrolled_window_get_shadow_type (GTK_SCROLLED_WINDOW (window->priv->scrolled_window)) == GTK_SHADOW_IN) {
		width -= 2 * window->priv->view->style->xthickness;
		height -= 2 * window->priv->view->style->ythickness;
	}

	gtk_widget_size_request (GTK_SCROLLED_WINDOW (window->priv->scrolled_window)->vscrollbar,
				 &vsb_requisition);
	gtk_widget_size_request (GTK_SCROLLED_WINDOW (window->priv->scrolled_window)->hscrollbar,
				 &hsb_requisition);
	gtk_widget_style_get (window->priv->scrolled_window,
			      "scrollbar_spacing", &scrollbar_spacing,
			      NULL);

	ev_view_set_zoom_for_size (EV_VIEW (window->priv->view),
				   MAX (1, width),
				   MAX (1, height),
				   vsb_requisition.width + scrollbar_spacing,
				   hsb_requisition.height + scrollbar_spacing);
}

static void
save_sizing_mode (EvWindow *window)
{
	EvSizingMode mode;
	GEnumValue *enum_value;

	if (window->priv->uri) {
		mode = ev_view_get_sizing_mode (EV_VIEW (window->priv->view));
		enum_value = g_enum_get_value (EV_SIZING_MODE_CLASS, mode);

		ev_metadata_manager_set_string (window->priv->uri, "sizing_mode",
						enum_value->value_nick);
	}
}

static void     
ev_window_sizing_mode_changed_cb (EvView *view, GParamSpec *pspec,
		 		  EvWindow   *ev_window)
{
	GtkWidget *scrolled_window;
	EvSizingMode sizing_mode;

	g_object_get (ev_window->priv->view,
		      "sizing-mode", &sizing_mode,
		      NULL);

	scrolled_window = ev_window->priv->scrolled_window;

	g_signal_handlers_disconnect_by_func (ev_window->priv->view, update_view_size, ev_window);

	if (sizing_mode != EV_SIZING_FREE)
	    	update_view_size (NULL, ev_window);

	switch (sizing_mode) {
	case EV_SIZING_BEST_FIT:
		g_object_set (G_OBJECT (scrolled_window),
			      "hscrollbar-policy", GTK_POLICY_NEVER,
			      "vscrollbar-policy", GTK_POLICY_AUTOMATIC,
			      NULL);
		g_signal_connect (ev_window->priv->view, "zoom_invalid",
				  G_CALLBACK (update_view_size),
				  ev_window);
		break;
	case EV_SIZING_FIT_WIDTH:
		g_object_set (G_OBJECT (scrolled_window),
			      "hscrollbar-policy", GTK_POLICY_NEVER,
			      "vscrollbar-policy", GTK_POLICY_AUTOMATIC,
			      NULL);
		g_signal_connect (ev_window->priv->view, "zoom_invalid",
				  G_CALLBACK (update_view_size),
				  ev_window);
		break;
	case EV_SIZING_FREE:
		g_object_set (G_OBJECT (scrolled_window),
			      "hscrollbar-policy", GTK_POLICY_AUTOMATIC,
			      "vscrollbar-policy", GTK_POLICY_AUTOMATIC,
			      NULL);
		break;
	}

	update_sizing_buttons (ev_window);
	save_sizing_mode (ev_window);
}

static void     
ev_window_zoom_changed_cb (EvView *view, GParamSpec *pspec, EvWindow *ev_window)
{
        update_action_sensitivity (ev_window);

	if (ev_view_get_sizing_mode (view) == EV_SIZING_FREE) {
		ev_metadata_manager_set_double (ev_window->priv->uri, "zoom",
					        ev_view_get_zoom (view));
	}
}

static void
ev_window_update_continuous_action (EvWindow *window)
{
	GtkAction *action;

	action = gtk_action_group_get_action (window->priv->action_group, "ViewContinuous");
	g_signal_handlers_block_by_func
		(action, G_CALLBACK (ev_window_cmd_continuous), window);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      ev_view_get_continuous (EV_VIEW (window->priv->view)));
	g_signal_handlers_unblock_by_func
		(action, G_CALLBACK (ev_window_cmd_continuous), window);
}

static void
ev_window_update_dual_page_action (EvWindow *window)
{
	GtkAction *action;

	action = gtk_action_group_get_action (window->priv->action_group, "ViewDual");
	g_signal_handlers_block_by_func
		(action, G_CALLBACK (ev_window_cmd_dual), window);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      ev_view_get_dual_page (EV_VIEW (window->priv->view)));
	g_signal_handlers_unblock_by_func
		(action, G_CALLBACK (ev_window_cmd_dual), window);
}

static void     
ev_window_continuous_changed_cb (EvView *view, GParamSpec *pspec, EvWindow *ev_window)
{
	ev_window_update_continuous_action (ev_window);

	if (ev_window->priv->uri) {
		ev_metadata_manager_set_boolean (ev_window->priv->uri, "continuous",
					         ev_view_get_continuous (EV_VIEW (ev_window->priv->view)));
	}
}

static void     
ev_window_rotation_changed_cb (EvView *view, GParamSpec *pspec, EvWindow *window)
{
	int rotation;

	rotation = ev_view_get_rotation (EV_VIEW (window->priv->view));

	if (window->priv->uri) {
		ev_metadata_manager_set_int (window->priv->uri, "rotation",
					     rotation);
	}

	ev_sidebar_thumbnails_refresh (EV_SIDEBAR_THUMBNAILS (window->priv->sidebar_thumbs),
				       rotation);
}

static void     
ev_window_dual_mode_changed_cb (EvView *view, GParamSpec *pspec, EvWindow *ev_window)
{
	ev_window_update_dual_page_action (ev_window);

	if (ev_window->priv->uri) {
		ev_metadata_manager_set_boolean (ev_window->priv->uri, "dual-page",
					         ev_view_get_dual_page (EV_VIEW (ev_window->priv->view)));
	}
}

static char *
build_comments_string (void)
{
	PopplerBackend backend;
	const char *backend_name;
	const char *version;

	backend = poppler_get_backend ();
	version = poppler_get_version ();
	switch (backend) {
		case POPPLER_BACKEND_CAIRO:
			backend_name = "cairo";
			break;
		case POPPLER_BACKEND_SPLASH:
			backend_name = "splash";
			break;
		default:
			backend_name = "unknown";
			break;
	}

	return g_strdup_printf (_("PostScript and PDF File Viewer.\n"
				  "Using poppler %s (%s)"),
				version, backend_name);
}

static void
ev_window_cmd_help_about (GtkAction *action, EvWindow *ev_window)
{
	const char *authors[] = {
		"Martin Kretzschmar <m_kretzschmar@gmx.net>",
		"Jonathan Blandford <jrb@gnome.org>",
		"Marco Pesenti Gritti <marco@gnome.org>",
		"Nickolay V. Shmyrev <nshmyrev@yandex.ru>",
		"Bryan Clark <clarkbw@gnome.org>",
		NULL
	};

	const char *documenters[] = {
		"Nickolay V. Shmyrev <nshmyrev@yandex.ru>",
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
	char *comments;

#ifdef ENABLE_NLS
	const char **p;

	for (p = authors; *p; ++p)
		*p = _(*p);

	for (p = documenters; *p; ++p)
		*p = _(*p);
#endif

	license_trans = g_strconcat (_(license[0]), "\n", _(license[1]), "\n",
				     _(license[2]), "\n", NULL);
	comments = build_comments_string ();

	gtk_show_about_dialog (
		GTK_WINDOW (ev_window),
		"name", _("Evince"),
		"version", VERSION,
		"copyright",
		_("\xc2\xa9 1996-2005 The Evince authors"),
		"license", license_trans,
		"website", "http://www.gnome.org/projects/evince",
		"comments", comments,
		"authors", authors,
		"documenters", documenters,
		"translator-credits", _("translator-credits"),
		NULL);

	g_free (comments);
	g_free (license_trans);
}

static void
ev_window_view_toolbar_cb (GtkAction *action, EvWindow *ev_window)
{
	update_chrome_flag (ev_window, EV_CHROME_TOOLBAR,
			    GCONF_CHROME_TOOLBAR,
			    gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
}

static void
ev_window_view_sidebar_cb (GtkAction *action, EvWindow *ev_window)
{
	set_widget_visibility (ev_window->priv->sidebar,
			       gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
}

static void
ev_window_sidebar_current_page_changed_cb (EvSidebar  *ev_sidebar,
					   GParamSpec *pspec,
					   EvWindow   *ev_window)
{
	GtkWidget *current_page;
	const char *id;

	g_object_get (G_OBJECT (ev_sidebar), "current_page", &current_page, NULL);

	if (current_page == ev_window->priv->sidebar_links) {
		id = LINKS_SIDEBAR_ID;
	} else if (current_page == ev_window->priv->sidebar_thumbs) {
		id = THUMBNAILS_SIDEBAR_ID;
	} else {
		g_assert_not_reached();
	}

	g_object_unref (current_page);

	if (ev_window->priv->uri) {
		ev_metadata_manager_set_string (ev_window->priv->uri, "sidebar_page", id);
	}
}

static void
ev_window_sidebar_visibility_changed_cb (EvSidebar *ev_sidebar, GParamSpec *pspec,
					 EvWindow   *ev_window)
{
	EvView *view = EV_VIEW (ev_window->priv->view);
	GtkAction *action;

	action = gtk_action_group_get_action (ev_window->priv->action_group, "ViewSidebar");
	
	g_signal_handlers_block_by_func
		(action, G_CALLBACK (ev_window_view_sidebar_cb), ev_window);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      GTK_WIDGET_VISIBLE (ev_sidebar));
	g_signal_handlers_unblock_by_func
		(action, G_CALLBACK (ev_window_view_sidebar_cb), ev_window);

	if (!ev_view_get_presentation (view) && !ev_view_get_fullscreen (view)) {
		ev_metadata_manager_set_boolean (ev_window->priv->uri, "sidebar_visibility",
					         GTK_WIDGET_VISIBLE (ev_sidebar));
	}
}

static void
view_find_status_changed_cb (EvView     *view,
			     GParamSpec *pspec,
			     EvWindow   *ev_window)
{
	const char *text;

	text = ev_view_get_find_status (view);
	egg_find_bar_set_status_text (EGG_FIND_BAR (ev_window->priv->find_bar),
				      text);
}

static void
find_bar_previous_cb (EggFindBar *find_bar,
		      EvWindow   *ev_window)
{
	ev_view_find_previous (EV_VIEW (ev_window->priv->view));
}

static void
find_bar_next_cb (EggFindBar *find_bar,
		  EvWindow   *ev_window)
{
	ev_view_find_next (EV_VIEW (ev_window->priv->view));
}

static void
find_bar_close_cb (EggFindBar *find_bar,
		   EvWindow   *ev_window)
{
	update_chrome_flag (ev_window, EV_CHROME_FINDBAR, NULL, FALSE);
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

	if (ev_window->priv->document &&
	    EV_IS_DOCUMENT_FIND (ev_window->priv->document)) {
		if (visible && search_string && search_string[0]) {
			ev_document_doc_mutex_lock ();
			ev_document_find_begin (EV_DOCUMENT_FIND (ev_window->priv->document), 
						ev_page_cache_get_current_page (ev_window->priv->page_cache),
						search_string,
						case_sensitive);
			ev_document_doc_mutex_unlock ();
		} else {
			ev_document_doc_mutex_lock ();
			ev_document_find_cancel (EV_DOCUMENT_FIND (ev_window->priv->document));
			ev_document_doc_mutex_unlock ();

			egg_find_bar_set_status_text (EGG_FIND_BAR (ev_window->priv->find_bar),
						      NULL);
			gtk_widget_queue_draw (GTK_WIDGET (ev_window->priv->view));
		}
	}
}

static void
zoom_control_changed_cb (EphyZoomAction *action,
			 float           zoom,
			 EvWindow       *ev_window)
{
	EvSizingMode mode;
	
	g_return_if_fail (EV_IS_WINDOW (ev_window));

	if (zoom == EPHY_ZOOM_BEST_FIT) {
		mode = EV_SIZING_BEST_FIT;
	} else if (zoom == EPHY_ZOOM_FIT_WIDTH) {
		mode = EV_SIZING_FIT_WIDTH;
	} else {
		mode = EV_SIZING_FREE;
	}
	
	ev_view_set_sizing_mode (EV_VIEW (ev_window->priv->view), mode);
	
	if (mode == EV_SIZING_FREE) {
		ev_view_set_zoom (EV_VIEW (ev_window->priv->view), zoom, FALSE);
	}
}

static void
ev_window_finalize (GObject *object)
{
	gboolean empty = TRUE;
	GList *list, *windows;


	windows = gtk_window_list_toplevels ();

	for (list = windows; list; list = list->next) {
		if (EV_IS_WINDOW (list->data)) {
			empty = FALSE;
			break;
		}
	}
	
	if (empty)
		ev_application_shutdown (EV_APP);
	
	g_list_free (windows);
	G_OBJECT_CLASS (ev_window_parent_class)->finalize (object);
}

static void
ev_window_dispose (GObject *object)
{
	EvWindow *window = EV_WINDOW (object);
	EvWindowPrivate *priv = window->priv;

	if (priv->recent_view) {
		g_object_unref (priv->recent_view);
		priv->recent_view = NULL;
	}

	if (priv->ui_manager) {
		g_object_unref (priv->ui_manager);
		priv->ui_manager = NULL;
	}

	if (priv->action_group) {
		g_object_unref (priv->action_group);
		priv->action_group = NULL;
	}

	if (priv->page_cache) {
		g_signal_handlers_disconnect_by_func (priv->page_cache, page_changed_cb, window);
		priv->page_cache = NULL;
	}

	if (priv->document) {
		g_object_unref (priv->document);
		priv->document = NULL;
	}
	
	if (priv->view) {
		g_object_unref (priv->view);
		priv->view = NULL;
	}

	if (priv->page_view) {
		g_object_unref (priv->page_view);
		priv->page_view = NULL;
	}

	if (priv->load_job || priv->xfer_job) {
		ev_window_clear_jobs (window);
	}
	
	if (priv->local_uri) {
		ev_window_clear_local_uri (window);
	}

	if (priv->password_document) {
		g_object_unref (priv->password_document);
		priv->password_document = NULL;
	}
	
	if (priv->password_uri) {
		g_free (priv->password_uri);
		priv->password_uri = NULL;
	}

	if (priv->password_dialog) {
		gtk_widget_destroy (priv->password_dialog);
	}

	if (priv->find_bar) {
		g_signal_handlers_disconnect_by_func
			(window->priv->find_bar,
			 G_CALLBACK (find_bar_close_cb),
			 window);
		priv->find_bar = NULL;
	}

	if (priv->uri) {
		g_free (priv->uri);
		priv->uri = NULL;
	}

	if (window->priv->fullscreen_timeout_source) {
		g_source_unref (window->priv->fullscreen_timeout_source);
		g_source_destroy (window->priv->fullscreen_timeout_source);
		window->priv->fullscreen_timeout_source = NULL;
	}
	destroy_fullscreen_popup (window);

	G_OBJECT_CLASS (ev_window_parent_class)->dispose (object);
}

static void
ev_window_class_init (EvWindowClass *ev_window_class)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (ev_window_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (ev_window_class);

	g_object_class->dispose = ev_window_dispose;
	g_object_class->finalize = ev_window_finalize;

	widget_class->focus_in_event = ev_window_focus_in_event;
	widget_class->focus_out_event = ev_window_focus_out_event;
	widget_class->screen_changed = ev_window_screen_changed;

	g_type_class_add_private (g_object_class, sizeof (EvWindowPrivate));
}

/* Normal items */
static const GtkActionEntry entries[] = {
	{ "File", NULL, N_("_File") },
        { "Edit", NULL, N_("_Edit") },
	{ "View", NULL, N_("_View") },
        { "Go", NULL, N_("_Go") },
	{ "Help", NULL, N_("_Help") },

	/* File menu */
	{ "FileOpen", GTK_STOCK_OPEN, N_("_Open..."), "<control>O",
	  N_("Open an existing document"),
	  G_CALLBACK (ev_window_cmd_file_open) },
       	{ "FileSaveAs", GTK_STOCK_SAVE_AS, N_("_Save a Copy..."), NULL,
	  N_("Save the current document with a new filename"),
	  G_CALLBACK (ev_window_cmd_save_as) },
	{ "FilePrint", GTK_STOCK_PRINT, N_("_Print..."), "<control>P",
	  N_("Print this document"),
	  G_CALLBACK (ev_window_cmd_file_print) },
	{ "FileProperties", GTK_STOCK_PROPERTIES, N_("P_roperties"), "<alt>Return",
	  N_("View the properties of this document"),
	  G_CALLBACK (ev_window_cmd_file_properties) },			      
	{ "FileCloseWindow", GTK_STOCK_CLOSE, NULL, "<control>W",
	  N_("Close this window"),
	  G_CALLBACK (ev_window_cmd_file_close_window) },

        /* Edit menu */
        { "EditCopy", GTK_STOCK_COPY, NULL, "<control>C",
          N_("Copy text from the document"),
          G_CALLBACK (ev_window_cmd_edit_copy) },
 	{ "EditSelectAll", NULL, N_("Select _All"), "<control>A",
	  N_("Select the entire page"),
	  G_CALLBACK (ev_window_cmd_edit_select_all) },
        { "EditFind", GTK_STOCK_FIND, NULL, "<control>F",
          N_("Find a word or phrase in the document"),
          G_CALLBACK (ev_window_cmd_edit_find) },
	{ "EditFindNext", NULL, N_("Find Ne_xt"), "<control>G",
	  N_("Find next occurrence of the word or phrase"),
	  G_CALLBACK (ev_window_cmd_edit_find_next) },
        { "EditToolbar", NULL, N_("T_oolbar"), NULL,
          N_("Customize the toolbar"),
          G_CALLBACK (ev_window_cmd_edit_toolbar) },
	{ "EditRotateLeft", NULL, N_("Rotate _Left"), NULL,
	  N_("Rotate the document to the left"),
	  G_CALLBACK (ev_window_cmd_edit_rotate_left) },
	{ "EditRotateRight", NULL, N_("Rotate _Right"), NULL,
	  N_("Rotate the document to the right"),
	  G_CALLBACK (ev_window_cmd_edit_rotate_right) },

        /* View menu */
        { "ViewZoomIn", GTK_STOCK_ZOOM_IN, NULL, "<control>plus",
          N_("Enlarge the document"),
          G_CALLBACK (ev_window_cmd_view_zoom_in) },
        { "ViewZoomOut", GTK_STOCK_ZOOM_OUT, NULL, "<control>minus",
          N_("Shrink the document"),
          G_CALLBACK (ev_window_cmd_view_zoom_out) },
        { "ViewReload", GTK_STOCK_REFRESH, N_("_Reload"), "<control>R",
          N_("Reload the document"),
          G_CALLBACK (ev_window_cmd_view_reload) },

        /* Go menu */
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
	{ "HelpContents", GTK_STOCK_HELP, N_("_Contents"), "F1",
	  N_("Display help for the viewer application"),
	  G_CALLBACK (ev_window_cmd_help_contents) },

	{ "HelpAbout", GTK_STOCK_ABOUT, N_("_About"), NULL,
	  N_("Display credits for the document viewer creators"),
	  G_CALLBACK (ev_window_cmd_help_about) },

	/* Toolbar-only */
	{ "LeaveFullscreen", EV_STOCK_LEAVE_FULLSCREEN, N_("Leave Fullscreen"), NULL,
	  N_("Leave fullscreen mode"),
	  G_CALLBACK (ev_window_cmd_leave_fullscreen) },

	/* Accellerators */
	{ "Escape", NULL, "", "Escape", "",
	  G_CALLBACK (ev_window_cmd_escape) },
        { "Slash", GTK_STOCK_FIND, NULL, "slash",
          N_("Find a word or phrase in the document"),
          G_CALLBACK (ev_window_cmd_edit_find) },
        { "PageDown", NULL, "", "Page_Down",
          N_("Scroll one page forward"),
          G_CALLBACK (ev_window_cmd_scroll_forward) },
        { "PageUp", NULL, "", "Page_Up",
          N_("Scroll one page backward"),
          G_CALLBACK (ev_window_cmd_scroll_backward) },
        { "Space", NULL, "", "space",
          N_("Scroll one page forward"),
          G_CALLBACK (ev_window_cmd_scroll_forward) },
        { "ShiftSpace", NULL, "", "<shift>space",
          N_("Scroll one page backward"),
          G_CALLBACK (ev_window_cmd_scroll_backward) },
        { "BackSpace", NULL, "", "BackSpace",
          N_("Scroll one page backward"),
          G_CALLBACK (ev_window_cmd_scroll_backward) },
        { "ShiftBackSpace", NULL, "", "<shift>BackSpace",
          N_("Scroll one page forward"),
          G_CALLBACK (ev_window_cmd_scroll_forward) },
        { "Plus", GTK_STOCK_ZOOM_IN, NULL, "plus",
          N_("Enlarge the document"),
          G_CALLBACK (ev_window_cmd_view_zoom_in) },
        { "CtrlEqual", GTK_STOCK_ZOOM_IN, NULL, "<control>equal",
          N_("Enlarge the document"),
          G_CALLBACK (ev_window_cmd_view_zoom_in) },
        { "Minus", GTK_STOCK_ZOOM_OUT, NULL, "minus",
          N_("Shrink the document"),
          G_CALLBACK (ev_window_cmd_view_zoom_out) },
        { "FocusPageSelector", NULL, "", "<control>l",
          N_("Focus the page selector"),
          G_CALLBACK (ev_window_cmd_focus_page_selector) },
        { "GoBackwardFast", NULL, "", "<shift>Page_Up",
          N_("Go ten pages backward"),
          G_CALLBACK (ev_window_cmd_go_backward) },
        { "GoForwardFast", NULL, "", "<shift>Page_Down",
          N_("Go ten pages forward"),
          G_CALLBACK (ev_window_cmd_go_forward) },
        { "KpPlus", GTK_STOCK_ZOOM_IN, NULL, "KP_Add",
          N_("Enlarge the document"),
          G_CALLBACK (ev_window_cmd_view_zoom_in) },
        { "KpMinus", GTK_STOCK_ZOOM_OUT, NULL, "KP_Subtract",
          N_("Shrink the document"),
          G_CALLBACK (ev_window_cmd_view_zoom_out) },
};

/* Toggle items */
static const GtkToggleActionEntry toggle_entries[] = {
	/* View Menu */
	{ "ViewToolbar", NULL, N_("_Toolbar"), "<shift><control>T",
	  N_("Show or hide the toolbar"),
	  G_CALLBACK (ev_window_view_toolbar_cb), TRUE },
        { "ViewSidebar", NULL, N_("Side _Pane"), "F9",
	  N_("Show or hide the side pane"),
	  G_CALLBACK (ev_window_view_sidebar_cb), TRUE },
        { "ViewContinuous", EV_STOCK_VIEW_CONTINUOUS, N_("_Continuous"), NULL,
	  N_("Show the entire document"),
	  G_CALLBACK (ev_window_cmd_continuous), TRUE },
        { "ViewDual", EV_STOCK_VIEW_DUAL, N_("_Dual"), NULL,
	  N_("Show two pages at once"),
	  G_CALLBACK (ev_window_cmd_dual), FALSE },
        { "ViewFullscreen", NULL, N_("_Fullscreen"), "F11",
          N_("Expand the window to fill the screen"),
          G_CALLBACK (ev_window_cmd_view_fullscreen) },
        { "ViewPresentation", NULL, N_("_Presentation"), "F5",
          N_("Run document as a presentation"),
          G_CALLBACK (ev_window_cmd_view_presentation) },
        { "ViewBestFit", EV_STOCK_ZOOM_PAGE, N_("_Best Fit"), NULL,
          N_("Make the current document fill the window"),
          G_CALLBACK (ev_window_cmd_view_best_fit) },
        { "ViewPageWidth", EV_STOCK_ZOOM_WIDTH, N_("Fit Page _Width"), NULL,
          N_("Make the current document fill the window width"),
          G_CALLBACK (ev_window_cmd_view_page_width) },
};

static void
drag_data_received_cb (GtkWidget *widget, GdkDragContext *context,
		       gint x, gint y, GtkSelectionData *selection_data,
		       guint info, guint time, gpointer gdata)
{
	GList  *uri_list = NULL;
	GSList *uris = NULL;
	gchar  *uri;

	uri_list = gnome_vfs_uri_list_parse ((gchar *) selection_data->data);

	if (uri_list) {
		while (uri_list) {
			uri = gnome_vfs_uri_to_string (uri_list->data, GNOME_VFS_URI_HIDE_NONE);
			uris = g_slist_append (uris, (gpointer) uri);
			
			uri_list = g_list_next (uri_list);
		}

		gnome_vfs_uri_list_free (uri_list);
		
		ev_application_open_uri_list (EV_APP, uris, 0);
		
		g_slist_free (uris);

		gtk_drag_finish (context, TRUE, FALSE, time);
	}
}

static void
activate_link_cb (EvPageAction *page_action, EvLink *link, EvWindow *window)
{
	g_return_if_fail (EV_IS_WINDOW (window));

	ev_view_goto_link (EV_VIEW (window->priv->view), link);
	gtk_widget_grab_focus (window->priv->view);
}

static gboolean
activate_label_cb (EvPageAction *page_action, char *label, EvWindow *window)
{
	g_return_val_if_fail (EV_IS_WINDOW (window), FALSE);

	gtk_widget_grab_focus (window->priv->view);

	return ev_page_cache_set_page_label (window->priv->page_cache, label);
}

static void
register_custom_actions (EvWindow *window, GtkActionGroup *group)
{
	GtkAction *action;

	action = g_object_new (EV_TYPE_PAGE_ACTION,
			       "name", PAGE_SELECTOR_ACTION,
			       "label", _("Page"),
			       "tooltip", _("Select Page"),
			       "visible_overflown", FALSE,
			       NULL);
	g_signal_connect (action, "activate_link",
			  G_CALLBACK (activate_link_cb), window);
	g_signal_connect (action, "activate_label",
			  G_CALLBACK (activate_label_cb), window);
	gtk_action_group_add_action (group, action);
	g_object_unref (action);

	action = g_object_new (EPHY_TYPE_ZOOM_ACTION,
			       "name", ZOOM_CONTROL_ACTION,
			       "label", _("Zoom"),
			       "stock_id", GTK_STOCK_ZOOM_IN,
			       "tooltip", _("Adjust the zoom level"),
			       "zoom", 1.0,
			       NULL);
	g_signal_connect (action, "zoom_to_level",
			  G_CALLBACK (zoom_control_changed_cb), window);
	gtk_action_group_add_action (group, action);
	g_object_unref (action);
}

static void
set_action_properties (GtkActionGroup *action_group)
{
	GtkAction *action;

	action = gtk_action_group_get_action (action_group, "GoPreviousPage");
	/*translators: this is the label for toolbar button*/
	g_object_set (action, "short_label", _("Previous"), NULL);
	g_object_set (action, "is-important", TRUE, NULL);

	action = gtk_action_group_get_action (action_group, "GoNextPage");
	g_object_set (action, "is-important", TRUE, NULL);
	/*translators: this is the label for toolbar button*/
	g_object_set (action, "short_label", _("Next"), NULL);

	action = gtk_action_group_get_action (action_group, "ViewZoomIn");
	/*translators: this is the label for toolbar button*/
	g_object_set (action, "short_label", _("Zoom In"), NULL);
	action = gtk_action_group_get_action (action_group, "ViewZoomIn");

	action = gtk_action_group_get_action (action_group, "ViewZoomOut");
	/*translators: this is the label for toolbar button*/
	g_object_set (action, "short_label", _("Zoom Out"), NULL);
	action = gtk_action_group_get_action (action_group, "ViewZoomIn");

	action = gtk_action_group_get_action (action_group, "ViewBestFit");
	/*translators: this is the label for toolbar button*/
	g_object_set (action, "short_label", _("Best Fit"), NULL);
	action = gtk_action_group_get_action (action_group, "ViewZoomIn");

	action = gtk_action_group_get_action (action_group, "ViewPageWidth");
	/*translators: this is the label for toolbar button*/
	g_object_set (action, "short_label", _("Fit Width"), NULL);
	action = gtk_action_group_get_action (action_group, "ViewZoomIn");

	action = gtk_action_group_get_action (action_group, "LeaveFullscreen");
	g_object_set (action, "is-important", TRUE, NULL);
}

static void
set_chrome_actions (EvWindow *window)
{
	EvWindowPrivate *priv = window->priv;
	GtkActionGroup *action_group = priv->action_group;
	GtkAction *action;

	action= gtk_action_group_get_action (action_group, "ViewToolbar");
	g_signal_handlers_block_by_func
		(action, G_CALLBACK (ev_window_view_toolbar_cb), window);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      (priv->chrome & EV_CHROME_TOOLBAR) != 0);
	g_signal_handlers_unblock_by_func
		(action, G_CALLBACK (ev_window_view_toolbar_cb), window);
}

static EvChrome
load_chrome (void)
{
	EvChrome chrome = EV_CHROME_NORMAL;
	GConfClient *client;
	GConfValue *value;

	client = gconf_client_get_default ();

	value = gconf_client_get (client, GCONF_CHROME_TOOLBAR, NULL);
	if (value != NULL) {
		if (value->type == GCONF_VALUE_BOOL && !gconf_value_get_bool (value)) {
			chrome &= ~EV_CHROME_TOOLBAR;
		}
		gconf_value_free (value);
	}

	g_object_unref (client);

	return chrome;
}

static void
sidebar_widget_model_set (EvSidebarLinks *ev_sidebar_links,
			  GParamSpec     *pspec,
			  EvWindow       *ev_window)
{
	GtkTreeModel *model;
	GtkAction *action;

	g_object_get (G_OBJECT (ev_sidebar_links),
		      "model", &model,
		      NULL);

	action = gtk_action_group_get_action (ev_window->priv->action_group, PAGE_SELECTOR_ACTION);
	ev_page_action_set_model (EV_PAGE_ACTION (action), model);
	g_object_unref (model);
}


static void
ev_window_set_view_accels_sensitivity (EvWindow *window, gboolean sensitive)
{
	if (window->priv->action_group) {
		set_action_sensitive (window, "PageDown", sensitive);
		set_action_sensitive (window, "PageUp", sensitive);
		set_action_sensitive (window, "Space", sensitive);
		set_action_sensitive (window, "ShiftSpace", sensitive);
		set_action_sensitive (window, "BackSpace", sensitive);
		set_action_sensitive (window, "ShiftBackSpace", sensitive);
		set_action_sensitive (window, "Slash", sensitive);
		set_action_sensitive (window, "Plus", sensitive);
		set_action_sensitive (window, "Minus", sensitive);
		set_action_sensitive (window, "KpPlus", sensitive);
		set_action_sensitive (window, "KpMinus", sensitive);
	}
}

static gboolean
view_actions_focus_in_cb (GtkWidget *widget, GdkEventFocus *event, EvWindow *window)
{
	update_chrome_flag (window, EV_CHROME_RAISE_TOOLBAR, NULL, FALSE);
	set_action_sensitive (window, "ViewToolbar", TRUE);

	ev_window_set_view_accels_sensitivity (window, TRUE);

	return FALSE;
}

static gboolean
view_actions_focus_out_cb (GtkWidget *widget, GdkEventFocus *event, EvWindow *window)
{
	ev_window_set_view_accels_sensitivity (window, FALSE);

	return FALSE;
}

static void
sidebar_page_main_widget_update_cb (GObject *ev_sidebar_page,
				    GParamSpec         *pspec,
				    EvWindow           *ev_window)
{
	GtkWidget *widget;
	
	g_object_get (ev_sidebar_page, "main_widget", &widget, NULL);

    	if (widget != NULL) {		
		g_signal_connect_object (widget, "focus_in_event",
				         G_CALLBACK (view_actions_focus_in_cb),
					 ev_window, 0);
		g_signal_connect_object (widget, "focus_out_event",
				         G_CALLBACK (view_actions_focus_out_cb),
					 ev_window, 0);
		g_object_unref (widget);
	}
}

static gboolean
window_state_event_cb (EvWindow *window, GdkEventWindowState *event, gpointer dummy)
{
	char *uri = window->priv->uri;

	if (uri && !(event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN)) {
		gboolean maximized;

		maximized = event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED;
		ev_metadata_manager_set_boolean (uri, "window_maximized", maximized);
	}

	return FALSE;
}

static gboolean
window_configure_event_cb (EvWindow *window, GdkEventConfigure *event, gpointer dummy)
{
	char *uri = window->priv->uri;
	GdkWindowState state;
	int x, y, width, height;

	state = gdk_window_get_state (GTK_WIDGET (window)->window);

	if (uri && !(state & GDK_WINDOW_STATE_FULLSCREEN)) {
		gtk_window_get_position (GTK_WINDOW (window), &x, &y);
		gtk_window_get_size (GTK_WINDOW (window), &width, &height);

		ev_metadata_manager_set_int (uri, "window_x", x);
		ev_metadata_manager_set_int (uri, "window_y", y);
		ev_metadata_manager_set_int (uri, "window_width", width);
		ev_metadata_manager_set_int (uri, "window_height", height);
	}

	return FALSE;
}

static void
sidebar_links_link_activated_cb (EvSidebarLinks *sidebar_links, EvLink *link, EvWindow *window)
{
	ev_view_goto_link (EV_VIEW (window->priv->view), link);
}

static void
ev_window_init (EvWindow *ev_window)
{
	GtkActionGroup *action_group;
	GtkAccelGroup *accel_group;
	GError *error = NULL;
	GtkWidget *sidebar_widget, *toolbar_dock;

	g_signal_connect (ev_window, "configure_event",
			  G_CALLBACK (window_configure_event_cb), NULL);
	g_signal_connect (ev_window, "window_state_event",
			  G_CALLBACK (window_state_event_cb), NULL);

	ev_window->priv = EV_WINDOW_GET_PRIVATE (ev_window);

	ev_window->priv->page_mode = PAGE_MODE_DOCUMENT;
	update_window_title (NULL, NULL, ev_window);

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
	set_action_properties (action_group);
	register_custom_actions (ev_window, action_group);

	ev_window->priv->ui_manager = gtk_ui_manager_new ();
	gtk_ui_manager_insert_action_group (ev_window->priv->ui_manager,
					    action_group, 0);

	accel_group =
		gtk_ui_manager_get_accel_group (ev_window->priv->ui_manager);
	gtk_window_add_accel_group (GTK_WINDOW (ev_window), accel_group);

	ev_window_set_view_accels_sensitivity (ev_window, FALSE);

	if (!gtk_ui_manager_add_ui_from_file (ev_window->priv->ui_manager,
					      DATADIR"/evince-ui.xml",
					      &error)) {
		g_warning ("building menus failed: %s", error->message);
		g_error_free (error);
	}

	ev_window->priv->menubar =
		 gtk_ui_manager_get_widget (ev_window->priv->ui_manager,
					    "/MainMenu");
	gtk_box_pack_start (GTK_BOX (ev_window->priv->main_box),
			    ev_window->priv->menubar,
			    FALSE, FALSE, 0);

	/* This sucks, but there is no way to have a draw=no, expand=true separator
	 * in a GtkUIManager-built toolbar. So, just add another toolbar.
	 * See gtk+ bug 166489.
	 */
	toolbar_dock = ev_window->priv->toolbar_dock = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (ev_window->priv->main_box), toolbar_dock,
			    FALSE, FALSE, 0);
	gtk_widget_show (toolbar_dock);

	ev_window->priv->toolbar = egg_editable_toolbar_new_with_model
				(ev_window->priv->ui_manager, ev_application_get_toolbars_model (EV_APP));
	egg_editable_toolbar_show (EGG_EDITABLE_TOOLBAR (ev_window->priv->toolbar),
				   "DefaultToolBar");
	gtk_box_pack_start (GTK_BOX (toolbar_dock), ev_window->priv->toolbar,
			    TRUE, TRUE, 0);
	gtk_widget_show (ev_window->priv->toolbar);

	/* Add the main area */
	ev_window->priv->hpaned = gtk_hpaned_new ();
	g_signal_connect (ev_window->priv->hpaned,
			  "notify::position",
			  G_CALLBACK (ev_window_sidebar_position_change_cb),
			  ev_window);
	
	gtk_paned_set_position (GTK_PANED (ev_window->priv->hpaned), SIDEBAR_DEFAULT_SIZE);
	gtk_box_pack_start (GTK_BOX (ev_window->priv->main_box), ev_window->priv->hpaned,
			    TRUE, TRUE, 0);
	gtk_widget_show (ev_window->priv->hpaned);
	
	ev_window->priv->sidebar = ev_sidebar_new ();
	gtk_paned_pack1 (GTK_PANED (ev_window->priv->hpaned),
			 ev_window->priv->sidebar, FALSE, FALSE);
	gtk_widget_show (ev_window->priv->sidebar);

	/* Stub sidebar, for now */
	sidebar_widget = ev_sidebar_links_new ();
	ev_window->priv->sidebar_links = sidebar_widget;
	g_signal_connect (sidebar_widget,
			  "notify::model",
			  G_CALLBACK (sidebar_widget_model_set),
			  ev_window);
	g_signal_connect (sidebar_widget,
			  "link_activated",
			  G_CALLBACK (sidebar_links_link_activated_cb),
			  ev_window);
	sidebar_page_main_widget_update_cb (G_OBJECT (sidebar_widget), NULL, ev_window);
	gtk_widget_show (sidebar_widget);
	ev_sidebar_add_page (EV_SIDEBAR (ev_window->priv->sidebar),
			     sidebar_widget);

	sidebar_widget = ev_sidebar_thumbnails_new ();
	ev_window->priv->sidebar_thumbs = sidebar_widget;
	g_signal_connect (sidebar_widget,
			  "notify::main-widget",
			  G_CALLBACK (sidebar_page_main_widget_update_cb),
			  ev_window);
	sidebar_page_main_widget_update_cb (G_OBJECT (sidebar_widget), NULL, ev_window);
	gtk_widget_show (sidebar_widget);
	ev_sidebar_add_page (EV_SIDEBAR (ev_window->priv->sidebar),
			     sidebar_widget);


	ev_window->priv->scrolled_window =
		GTK_WIDGET (g_object_new (GTK_TYPE_SCROLLED_WINDOW,
					  "shadow-type", GTK_SHADOW_IN,
					  NULL));
	gtk_widget_show (ev_window->priv->scrolled_window);

	gtk_paned_add2 (GTK_PANED (ev_window->priv->hpaned),
			ev_window->priv->scrolled_window);

	ev_window->priv->view = ev_view_new ();
	ev_window->priv->password_view = ev_password_view_new ();
	g_signal_connect_swapped (ev_window->priv->password_view,
				  "unlock",
				  G_CALLBACK (ev_window_popup_password_dialog),
				  ev_window);
	g_signal_connect_object (ev_window->priv->view, "focus_in_event",
			         G_CALLBACK (view_actions_focus_in_cb),
				 ev_window, 0);
	g_signal_connect_object (ev_window->priv->view, "focus_out_event",
			         G_CALLBACK (view_actions_focus_out_cb),
			         ev_window, 0);
	gtk_widget_show (ev_window->priv->view);
	gtk_widget_show (ev_window->priv->password_view);


	/* We own a ref on these widgets, as we can swap them in and out */
	g_object_ref (ev_window->priv->view);
	//g_object_ref (ev_window->priv->page_view);
	g_object_ref (ev_window->priv->password_view);

	gtk_container_add (GTK_CONTAINER (ev_window->priv->scrolled_window),
			   ev_window->priv->view);

	g_signal_connect (ev_window->priv->view,
			  "notify::find-status",
			  G_CALLBACK (view_find_status_changed_cb),
			  ev_window);
	g_signal_connect (ev_window->priv->view,
			  "notify::sizing-mode",
			  G_CALLBACK (ev_window_sizing_mode_changed_cb),
			  ev_window);
	g_signal_connect (ev_window->priv->view,
			  "notify::zoom",
			  G_CALLBACK (ev_window_zoom_changed_cb),
			  ev_window);
	g_signal_connect (ev_window->priv->view,
			  "notify::dual-page",
			  G_CALLBACK (ev_window_dual_mode_changed_cb),
			  ev_window);
	g_signal_connect (ev_window->priv->view,
			  "notify::continuous",
			  G_CALLBACK (ev_window_continuous_changed_cb),
			  ev_window);
	g_signal_connect (ev_window->priv->view,
			  "notify::rotation",
			  G_CALLBACK (ev_window_rotation_changed_cb),
			  ev_window);

	ev_window->priv->find_bar = egg_find_bar_new ();
	gtk_box_pack_end (GTK_BOX (ev_window->priv->main_box),
			  ev_window->priv->find_bar,
			  FALSE, TRUE, 0);

	ev_window_setup_recent (ev_window);
	ev_window->priv->chrome = load_chrome ();
	set_chrome_actions (ev_window);
	update_chrome_visibility (ev_window);

	/* Connect sidebar signals */
	g_signal_connect (ev_window->priv->sidebar,
			  "notify::visible",
			  G_CALLBACK (ev_window_sidebar_visibility_changed_cb),
			  ev_window);
	g_signal_connect (ev_window->priv->sidebar,
			  "notify::current-page",
			  G_CALLBACK (ev_window_sidebar_current_page_changed_cb),
			  ev_window);

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

	/* Give focus to the document view */
	gtk_widget_grab_focus (ev_window->priv->view);

	/* Drag and Drop */
	gtk_drag_dest_unset (GTK_WIDGET (ev_window));
	gtk_drag_dest_set (GTK_WIDGET (ev_window), GTK_DEST_DEFAULT_ALL, ev_drop_types,
			   sizeof (ev_drop_types) / sizeof (ev_drop_types[0]),
			   GDK_ACTION_COPY);
	g_signal_connect (G_OBJECT (ev_window), "drag_data_received",
			  G_CALLBACK (drag_data_received_cb), NULL);

	/* Set it to something random to force a change */

        ev_window_sizing_mode_changed_cb (EV_VIEW (ev_window->priv->view), NULL, ev_window);
	update_action_sensitivity (ev_window);
}

GtkWidget *
ev_window_new (void)
{
	GtkWidget *ev_window;

	ev_window = GTK_WIDGET (g_object_new (EV_TYPE_WINDOW,
					      "type", GTK_WINDOW_TOPLEVEL,
					      "default-width", 600,
					      "default-height", 600,
					      NULL));

	return ev_window;
}
