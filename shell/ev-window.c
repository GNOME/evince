/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2008 Carlos Garcia Campos
 *  Copyright (C) 2004 Martin Kretzschmar
 *  Copyright (C) 2004 Red Hat, Inc.
 *  Copyright (C) 2000, 2001, 2002, 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005, 2009 Christian Persch
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#ifdef WITH_GCONF
#include <gconf/gconf-client.h>
#endif

#include "egg-editable-toolbar.h"
#include "egg-toolbar-editor.h"
#include "egg-toolbars-model.h"

#include "eggfindbar.h"

#include "ephy-zoom-action.h"
#include "ephy-zoom.h"

#include "ev-application.h"
#include "ev-document-factory.h"
#include "ev-document-find.h"
#include "ev-document-fonts.h"
#include "ev-document-images.h"
#include "ev-document-links.h"
#include "ev-document-thumbnails.h"
#include "ev-document-type-builtins.h"
#include "ev-file-exporter.h"
#include "ev-file-helpers.h"
#include "ev-file-monitor.h"
#include "ev-history.h"
#include "ev-image.h"
#include "ev-job-scheduler.h"
#include "ev-jobs.h"
#include "ev-message-area.h"
#include "ev-metadata-manager.h"
#include "ev-navigation-action.h"
#include "ev-open-recent-action.h"
#include "ev-page-action.h"
#include "ev-password-view.h"
#include "ev-properties-dialog.h"
#include "ev-sidebar-attachments.h"
#include "ev-sidebar.h"
#include "ev-sidebar-links.h"
#include "ev-sidebar-page.h"
#include "ev-sidebar-thumbnails.h"
#include "ev-sidebar-layers.h"
#include "ev-stock-icons.h"
#include "ev-utils.h"
#include "ev-keyring.h"
#include "ev-view.h"
#include "ev-view-type-builtins.h"
#include "ev-window.h"
#include "ev-window-title.h"
#include "ev-print-operation.h"
#include "ev-progress-message-area.h"

#ifdef ENABLE_DBUS
#include "ev-media-player-keys.h"
#endif /* ENABLE_DBUS */

#ifdef ENABLE_PDF
#include <poppler.h>
#endif

typedef enum {
	PAGE_MODE_DOCUMENT,
	PAGE_MODE_PASSWORD
} EvWindowPageMode;

typedef enum {
	EV_CHROME_MENUBAR	= 1 << 0,
	EV_CHROME_TOOLBAR	= 1 << 1,
	EV_CHROME_FINDBAR	= 1 << 2,
	EV_CHROME_RAISE_TOOLBAR	= 1 << 3,
	EV_CHROME_FULLSCREEN_TOOLBAR	= 1 << 4,
	EV_CHROME_SIDEBAR	= 1 << 5,
	EV_CHROME_NORMAL	= EV_CHROME_MENUBAR | EV_CHROME_TOOLBAR | EV_CHROME_SIDEBAR
} EvChrome;

typedef enum {
	EV_SAVE_DOCUMENT,
	EV_SAVE_ATTACHMENT,
	EV_SAVE_IMAGE
} EvSaveType;

struct _EvWindowPrivate {
	/* UI */
	EvChrome chrome;

	GtkWidget *main_box;
	GtkWidget *menubar;
	GtkWidget *toolbar;
	GtkWidget *hpaned;
	GtkWidget *view_box;
	GtkWidget *sidebar;
	GtkWidget *find_bar;
	GtkWidget *scrolled_window;
	GtkWidget *view;
	GtkWidget *message_area;
	GtkWidget *password_view;
	GtkWidget *sidebar_thumbs;
	GtkWidget *sidebar_links;
	GtkWidget *sidebar_attachments;
	GtkWidget *sidebar_layers;

	/* Menubar accels */
	guint           menubar_accel_keyval;
	GdkModifierType menubar_accel_modifier;

	/* Progress Messages */
	guint progress_idle;
	GCancellable *progress_cancellable;

	/* Dialogs */
	GtkWidget *properties;
	GtkWidget *print_dialog;

	/* UI Builders */
	GtkActionGroup   *action_group;
	GtkActionGroup   *view_popup_action_group;
	GtkActionGroup   *attachment_popup_action_group;
	GtkRecentManager *recent_manager;
	GtkActionGroup   *recent_action_group;
	guint             recent_ui_id;
	GtkUIManager     *ui_manager;

	/* Fullscreen mode */
	GtkWidget *fullscreen_toolbar;

	/* Presentation mode */
	guint      presentation_timeout_id;

	/* Popup view */
	GtkWidget *view_popup;
	EvLink    *link;
	EvImage   *image;

	/* Popup attachment */
	GtkWidget    *attachment_popup;
	GList        *attach_list;

	/* Document */
	char *uri;
	glong uri_mtime;
	char *local_uri;
	gboolean in_reload;
	EvFileMonitor *monitor;
	guint setup_document_idle;
	
	EvDocument *document;
	EvHistory *history;
	EvPageCache *page_cache;
	EvWindowPageMode page_mode;
	EvWindowTitle *title;

	/* Load params */
	EvLinkDest       *dest;
	gchar            *search_string;
	EvWindowRunMode   window_mode;

	EvJob            *load_job;
	EvJob            *reload_job;
	EvJob            *thumbnail_job;
	EvJob            *save_job;
	EvJob            *find_job;

	/* Printing */
	GQueue           *print_queue;
	GtkPrintSettings *print_settings;
	GtkPageSetup     *print_page_setup;
	gboolean          close_after_print;
};

#define EV_WINDOW_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_WINDOW, EvWindowPrivate))

#define PAGE_SELECTOR_ACTION	"PageSelector"
#define ZOOM_CONTROL_ACTION	"ViewZoom"
#define NAVIGATION_ACTION	"Navigation"

#define GCONF_OVERRIDE_RESTRICTIONS "/apps/evince/override_restrictions"
#define GCONF_LOCKDOWN_SAVE         "/desktop/gnome/lockdown/disable_save_to_disk"
#define GCONF_LOCKDOWN_PRINT        "/desktop/gnome/lockdown/disable_printing"

#define PRESENTATION_TIMEOUT 5

#define SIDEBAR_DEFAULT_SIZE    132
#define LINKS_SIDEBAR_ID "links"
#define THUMBNAILS_SIDEBAR_ID "thumbnails"
#define ATTACHMENTS_SIDEBAR_ID "attachments"
#define LAYERS_SIDEBAR_ID "layers"

static const gchar *document_print_settings[] = {
	GTK_PRINT_SETTINGS_N_COPIES,
	GTK_PRINT_SETTINGS_COLLATE,
	GTK_PRINT_SETTINGS_REVERSE,
	GTK_PRINT_SETTINGS_NUMBER_UP,
	GTK_PRINT_SETTINGS_SCALE,
	GTK_PRINT_SETTINGS_PRINT_PAGES,
	GTK_PRINT_SETTINGS_PAGE_RANGES,
	GTK_PRINT_SETTINGS_PAGE_SET,
	GTK_PRINT_SETTINGS_OUTPUT_URI
};

static void	ev_window_update_actions	 	(EvWindow         *ev_window);
static void     ev_window_sidebar_visibility_changed_cb (EvSidebar        *ev_sidebar,
							 GParamSpec       *pspec,
							 EvWindow         *ev_window);
static void     ev_window_set_page_mode                 (EvWindow         *window,
							 EvWindowPageMode  page_mode);
static void	ev_window_load_job_cb  			(EvJob            *job,
							 gpointer          data);
static void     ev_window_reload_document               (EvWindow         *window,
							 EvLinkDest *dest);
static void     ev_window_reload_job_cb                 (EvJob            *job,
							 EvWindow         *window);
static void     ev_window_set_icon_from_thumbnail       (EvJobThumbnail   *job,
							 EvWindow         *ev_window);
static void     ev_window_save_job_cb                   (EvJob            *save,
							 EvWindow         *window);
static void     ev_window_sizing_mode_changed_cb        (EvView           *view,
							 GParamSpec       *pspec,
							 EvWindow         *ev_window);
static void     ev_window_zoom_changed_cb 	        (EvView           *view,
							 GParamSpec       *pspec,
							 EvWindow         *ev_window);
static void     ev_window_add_recent                    (EvWindow         *window,
							 const char       *filename);
static void     ev_window_run_fullscreen                (EvWindow         *window);
static void     ev_window_stop_fullscreen               (EvWindow         *window,
							 gboolean          unfullscreen_window);
static void     ev_window_cmd_view_fullscreen           (GtkAction        *action,
							 EvWindow         *window);
static void     ev_window_run_presentation              (EvWindow         *window);
static void     ev_window_stop_presentation             (EvWindow         *window,
							 gboolean          unfullscreen_window);
static void     ev_window_cmd_view_presentation         (GtkAction        *action,
							 EvWindow         *window);
static void     ev_view_popup_cmd_open_link             (GtkAction        *action,
							 EvWindow         *window);
static void     ev_view_popup_cmd_open_link_new_window  (GtkAction        *action,
							 EvWindow         *window);
static void     ev_view_popup_cmd_copy_link_address     (GtkAction        *action,
							 EvWindow         *window);
static void     ev_view_popup_cmd_save_image_as         (GtkAction        *action,
							 EvWindow         *window);
static void     ev_view_popup_cmd_copy_image            (GtkAction        *action,
							 EvWindow         *window);
static void	ev_attachment_popup_cmd_open_attachment (GtkAction        *action,
							 EvWindow         *window);
static void	ev_attachment_popup_cmd_save_attachment_as (GtkAction     *action, 
							 EvWindow         *window);
static void	ev_window_cmd_view_best_fit 		(GtkAction 	  *action, 
							 EvWindow 	  *ev_window);
static void	ev_window_cmd_view_page_width 		(GtkAction 	  *action, 
							 EvWindow 	  *ev_window);
static void	view_handle_link_cb 			(EvView           *view, 
							 EvLink           *link, 
							 EvWindow         *window);
static void     ev_window_update_find_status_message    (EvWindow         *ev_window);
static void     ev_window_cmd_edit_find                 (GtkAction        *action,
							 EvWindow         *ev_window);
static void     find_bar_search_changed_cb              (EggFindBar       *find_bar,
							 GParamSpec       *param,
							 EvWindow         *ev_window);
static void     ev_window_load_file_remote              (EvWindow         *ev_window,
							 GFile            *source_file);
static void     ev_window_media_player_key_pressed      (EvWindow         *window,
							 const gchar      *key,
							 gpointer          user_data);
static void     ev_window_save_print_page_setup         (EvWindow         *window);

G_DEFINE_TYPE (EvWindow, ev_window, GTK_TYPE_WINDOW)

static void
ev_window_set_action_sensitive (EvWindow   *ev_window,
		    	        const char *name,
		  	        gboolean    sensitive)
{
	GtkAction *action = gtk_action_group_get_action (ev_window->priv->action_group,
							 name);
	gtk_action_set_sensitive (action, sensitive);
}


static void
ev_window_setup_action_sensitivity (EvWindow *ev_window)
{
	EvDocument *document = ev_window->priv->document;
	const EvDocumentInfo *info = NULL;

	gboolean has_document = FALSE;
	gboolean ok_to_print = TRUE;
	gboolean ok_to_copy = TRUE;
	gboolean has_properties = TRUE;
	gboolean override_restrictions = TRUE;
	gboolean can_get_text = FALSE;
	gboolean has_pages = FALSE;
	gboolean can_find = FALSE;
#ifdef WITH_GCONF
	GConfClient *client;
#endif

	if (document) {
		has_document = TRUE;
		info = ev_page_cache_get_info (ev_window->priv->page_cache);
	}

	if (has_document && ev_window->priv->page_cache) {
		has_pages = ev_page_cache_get_n_pages (ev_window->priv->page_cache) > 0;
	}

	if (!info || info->fields_mask == 0) {
		has_properties = FALSE;
	}

	if (has_document && EV_IS_SELECTION (document)) {
		can_get_text = TRUE;
	}
	
	if (has_pages && EV_IS_DOCUMENT_FIND (document)) {
		can_find = TRUE;
	}

#ifdef WITH_GCONF
	client = gconf_client_get_default ();
	override_restrictions = gconf_client_get_bool (client, 
						       GCONF_OVERRIDE_RESTRICTIONS, 
						       NULL);
#endif
	if (!override_restrictions && info && info->fields_mask & EV_DOCUMENT_INFO_PERMISSIONS) {
		ok_to_print = (info->permissions & EV_DOCUMENT_PERMISSIONS_OK_TO_PRINT);
		ok_to_copy = (info->permissions & EV_DOCUMENT_PERMISSIONS_OK_TO_COPY);
	}

	if (has_document && !ev_print_operation_exists_for_document(document))
		ok_to_print = FALSE;

#ifdef WITH_GCONF
	if (gconf_client_get_bool (client, GCONF_LOCKDOWN_SAVE, NULL)) {
		ok_to_copy = FALSE;
	}

	if (gconf_client_get_bool (client, GCONF_LOCKDOWN_PRINT, NULL)) {
		ok_to_print = FALSE;
	}

	g_object_unref (client);
#endif

	/* File menu */
	ev_window_set_action_sensitive (ev_window, "FileOpenCopy", has_document);
	ev_window_set_action_sensitive (ev_window, "FileSaveAs", has_document && ok_to_copy);
	ev_window_set_action_sensitive (ev_window, "FilePageSetup", has_pages && ok_to_print);
	ev_window_set_action_sensitive (ev_window, "FilePrint", has_pages && ok_to_print);
	ev_window_set_action_sensitive (ev_window, "FileProperties", has_document && has_properties);

        /* Edit menu */
	ev_window_set_action_sensitive (ev_window, "EditSelectAll", has_pages && can_get_text);
	ev_window_set_action_sensitive (ev_window, "EditFind", can_find);
	ev_window_set_action_sensitive (ev_window, "Slash", can_find);
	ev_window_set_action_sensitive (ev_window, "EditRotateLeft", has_pages);
	ev_window_set_action_sensitive (ev_window, "EditRotateRight", has_pages);

        /* View menu */
	ev_window_set_action_sensitive (ev_window, "ViewContinuous", has_pages);
	ev_window_set_action_sensitive (ev_window, "ViewDual", has_pages);
	ev_window_set_action_sensitive (ev_window, "ViewBestFit", has_pages);
	ev_window_set_action_sensitive (ev_window, "ViewPageWidth", has_pages);
	ev_window_set_action_sensitive (ev_window, "ViewReload", has_pages);
	ev_window_set_action_sensitive (ev_window, "ViewAutoscroll", has_pages);

	/* Toolbar-specific actions: */
	ev_window_set_action_sensitive (ev_window, PAGE_SELECTOR_ACTION, has_pages);
	ev_window_set_action_sensitive (ev_window, ZOOM_CONTROL_ACTION,  has_pages);
	ev_window_set_action_sensitive (ev_window, NAVIGATION_ACTION,  FALSE);

        ev_window_update_actions (ev_window);
}

static void
ev_window_update_actions (EvWindow *ev_window)
{
	EvView *view = EV_VIEW (ev_window->priv->view);
	int n_pages = 0, page = -1;
	gboolean has_pages = FALSE;
	gboolean presentation_mode;
	gboolean can_find_in_page = FALSE;

	if (ev_window->priv->document && ev_window->priv->page_cache) {
		page = ev_page_cache_get_current_page (ev_window->priv->page_cache);
		n_pages = ev_page_cache_get_n_pages (ev_window->priv->page_cache);
		has_pages = n_pages > 0;
	}

	can_find_in_page = (ev_window->priv->find_job &&
			    ev_job_find_has_results (EV_JOB_FIND (ev_window->priv->find_job)));

	ev_window_set_action_sensitive (ev_window, "EditCopy",
					has_pages &&
					ev_view_get_has_selection (view));
	ev_window_set_action_sensitive (ev_window, "EditFindNext",
					has_pages && can_find_in_page);
	ev_window_set_action_sensitive (ev_window, "EditFindPrevious",
					has_pages && can_find_in_page);
        ev_window_set_action_sensitive (ev_window, "F3",
                                        has_pages && can_find_in_page);

	presentation_mode = ev_view_get_presentation (view);
	
	ev_window_set_action_sensitive (ev_window, "ViewZoomIn",
					has_pages &&
					ev_view_can_zoom_in (view) &&
					!presentation_mode);
	ev_window_set_action_sensitive (ev_window, "ViewZoomOut",
					has_pages &&
					ev_view_can_zoom_out (view) &&
					!presentation_mode);
	
        /* Go menu */
	if (has_pages) {
		ev_window_set_action_sensitive (ev_window, "GoPreviousPage", page > 0);
		ev_window_set_action_sensitive (ev_window, "GoNextPage", page < n_pages - 1);
		ev_window_set_action_sensitive (ev_window, "GoFirstPage", page > 0);
		ev_window_set_action_sensitive (ev_window, "GoLastPage", page < n_pages - 1);
	} else {
  		ev_window_set_action_sensitive (ev_window, "GoFirstPage", FALSE);
		ev_window_set_action_sensitive (ev_window, "GoPreviousPage", FALSE);
		ev_window_set_action_sensitive (ev_window, "GoNextPage", FALSE);
		ev_window_set_action_sensitive (ev_window, "GoLastPage", FALSE);
	}

	if (has_pages &&
	    ev_view_get_sizing_mode (view) != EV_SIZING_FIT_WIDTH &&
	    ev_view_get_sizing_mode (view) != EV_SIZING_BEST_FIT) {
		GtkAction *action;
		float      zoom;
		float      real_zoom;

		action = gtk_action_group_get_action (ev_window->priv->action_group, 
						      ZOOM_CONTROL_ACTION);

		real_zoom = ev_view_get_zoom (EV_VIEW (ev_window->priv->view));
		real_zoom *= 72.0 / get_screen_dpi (GTK_WINDOW (ev_window));
		zoom = ephy_zoom_get_nearest_zoom_level (real_zoom);

		ephy_zoom_action_set_zoom_level (EPHY_ZOOM_ACTION (action), zoom);
	}
}

static void
ev_window_set_view_accels_sensitivity (EvWindow *window, gboolean sensitive)
{
	gboolean can_find;

	can_find = window->priv->document && 
	    EV_IS_DOCUMENT_FIND (window->priv->document);

	if (window->priv->action_group) {
		ev_window_set_action_sensitive (window, "PageDown", sensitive);
		ev_window_set_action_sensitive (window, "PageUp", sensitive);
		ev_window_set_action_sensitive (window, "Space", sensitive);
		ev_window_set_action_sensitive (window, "ShiftSpace", sensitive);
		ev_window_set_action_sensitive (window, "BackSpace", sensitive);
		ev_window_set_action_sensitive (window, "ShiftBackSpace", sensitive);
		ev_window_set_action_sensitive (window, "Return", sensitive);
		ev_window_set_action_sensitive (window, "ShiftReturn", sensitive);
		ev_window_set_action_sensitive (window, "Plus", sensitive);
		ev_window_set_action_sensitive (window, "Minus", sensitive);
		ev_window_set_action_sensitive (window, "KpPlus", sensitive);
		ev_window_set_action_sensitive (window, "KpMinus", sensitive);
		ev_window_set_action_sensitive (window, "Equal", sensitive);

		ev_window_set_action_sensitive (window, "Slash", sensitive && can_find);
	}
}

static void
set_widget_visibility (GtkWidget *widget, gboolean visible)
{
	g_assert (GTK_IS_WIDGET (widget));
	
	if (visible)
		gtk_widget_show (widget);
	else
		gtk_widget_hide (widget);
}

static void
update_chrome_visibility (EvWindow *window)
{
	EvWindowPrivate *priv = window->priv;
	gboolean menubar, toolbar, findbar, fullscreen_toolbar, sidebar;
	gboolean fullscreen_mode, presentation, fullscreen;

	presentation = ev_view_get_presentation (EV_VIEW (priv->view));
	fullscreen = ev_view_get_fullscreen (EV_VIEW (priv->view));
	fullscreen_mode = fullscreen || presentation;

	menubar = (priv->chrome & EV_CHROME_MENUBAR) != 0 && !fullscreen_mode;
	toolbar = ((priv->chrome & EV_CHROME_TOOLBAR) != 0  || 
		   (priv->chrome & EV_CHROME_RAISE_TOOLBAR) != 0) && !fullscreen_mode;
	fullscreen_toolbar = ((priv->chrome & EV_CHROME_FULLSCREEN_TOOLBAR) != 0 || 
			      (priv->chrome & EV_CHROME_RAISE_TOOLBAR) != 0) && fullscreen;
	findbar = (priv->chrome & EV_CHROME_FINDBAR) != 0;
	sidebar = (priv->chrome & EV_CHROME_SIDEBAR) != 0 && !presentation;

	set_widget_visibility (priv->menubar, menubar);	
	set_widget_visibility (priv->toolbar, toolbar);
	set_widget_visibility (priv->find_bar, findbar);
	set_widget_visibility (priv->sidebar, sidebar);
	
	ev_window_set_action_sensitive (window, "EditToolbar", toolbar);

	if (priv->fullscreen_toolbar != NULL) {
		set_widget_visibility (priv->fullscreen_toolbar, fullscreen_toolbar);
	}
}

static void
update_chrome_flag (EvWindow *window, EvChrome flag, gboolean active)
{
	EvWindowPrivate *priv = window->priv;
	
	if (active) {
		priv->chrome |= flag;
	} else {
		priv->chrome &= ~flag;
	}
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

/**
 * ev_window_is_empty:
 * @ev_window: The instance of the #EvWindow.
 *
 * It does look if there is any document loaded or if there is any job to load
 * a document.
 *
 * Returns: %TRUE if there isn't any document loaded or any any documente to be
 *          loaded, %FALSE in other case.
 */
gboolean
ev_window_is_empty (const EvWindow *ev_window)
{
	g_return_val_if_fail (EV_IS_WINDOW (ev_window), FALSE);

	return (ev_window->priv->document == NULL) && 
		(ev_window->priv->load_job == NULL);
}

static void
ev_window_set_message_area (EvWindow  *window,
			    GtkWidget *area)
{
	if (window->priv->message_area == area)
		return;

	if (window->priv->message_area)
		gtk_widget_destroy (window->priv->message_area);
	window->priv->message_area = area;

	if (!area)
		return;

	gtk_box_pack_start (GTK_BOX (window->priv->view_box),
			    window->priv->message_area,
			    FALSE, FALSE, 0);
	gtk_box_reorder_child (GTK_BOX (window->priv->view_box),
			       window->priv->message_area, 0);
	g_object_add_weak_pointer (G_OBJECT (window->priv->message_area),
				   (gpointer) &(window->priv->message_area));
}

static void
ev_window_message_area_response_cb (EvMessageArea *area,
				    gint           response_id,
				    EvWindow      *window)
{
	ev_window_set_message_area (window, NULL);
}

static void
ev_window_error_message (EvWindow    *window,
			 GError      *error,
			 const gchar *format,
			 ...)
{
	GtkWidget *area;
	va_list    args;
	gchar     *msg = NULL;

	if (window->priv->message_area)
		return;

	va_start (args, format);
	msg = g_strdup_vprintf (format, args);
	va_end (args);
	
	area = ev_message_area_new (GTK_MESSAGE_ERROR,
				    msg,
				    GTK_STOCK_CLOSE,
				    GTK_RESPONSE_CLOSE,
				    NULL);
	g_free (msg);
	
	if (error)
		ev_message_area_set_secondary_text (EV_MESSAGE_AREA (area), error->message);
	g_signal_connect (area, "response",
			  G_CALLBACK (ev_window_message_area_response_cb),
			  window);
	gtk_widget_show (area);
	ev_window_set_message_area (window, area);
}

static void
ev_window_warning_message (EvWindow    *window,
			   const gchar *format,
			   ...)
{
	GtkWidget *area;
	va_list    args;
	gchar     *msg = NULL;

	if (window->priv->message_area)
		return;

	va_start (args, format);
	msg = g_strdup_vprintf (format, args);
	va_end (args);

	area = ev_message_area_new (GTK_MESSAGE_WARNING,
				    msg,
				    GTK_STOCK_CLOSE,
				    GTK_RESPONSE_CLOSE,
				    NULL);
	g_free (msg);
	
	g_signal_connect (area, "response",
			  G_CALLBACK (ev_window_message_area_response_cb),
			  window);
	gtk_widget_show (area);
	ev_window_set_message_area (window, area);
}

static void
page_changed_cb (EvPageCache *page_cache,
		 gint         page,
		 EvWindow    *ev_window)
{
	ev_window_update_actions (ev_window);

	ev_window_update_find_status_message (ev_window);

	if (!ev_window_is_empty (ev_window))
		ev_metadata_manager_set_int (ev_window->priv->uri, "page", page);
}

typedef struct _FindTask {
	const gchar *page_label;
	gchar *chapter;
} FindTask;

static gboolean
ev_window_find_chapter (GtkTreeModel *tree_model,
		        GtkTreePath  *path,
		        GtkTreeIter  *iter,
		        gpointer      data)
{
	FindTask *task = (FindTask *)data;
	gchar *page_string;
	
	gtk_tree_model_get (tree_model, iter,
			    EV_DOCUMENT_LINKS_COLUMN_PAGE_LABEL, &page_string, 
			    -1);
	
	if (!page_string)
		return FALSE;
	
	if (!strcmp (page_string, task->page_label)) {
		gtk_tree_model_get (tree_model, iter,
				    EV_DOCUMENT_LINKS_COLUMN_MARKUP, &task->chapter, 
				    -1);
		g_free (page_string);
		return TRUE;
	}
	
	g_free (page_string);
	return FALSE;
}

static void
ev_window_add_history (EvWindow *window, gint page, EvLink *link)
{
	gchar *page_label = NULL;
	gchar *link_title;
	FindTask find_task;
	EvLink *real_link;
	EvLinkAction *action;
	EvLinkDest *dest;
	
	if (window->priv->history == NULL)
		return;

	if (!EV_IS_DOCUMENT_LINKS (window->priv->document))
		return;
	
	if (link) {
		action = g_object_ref (ev_link_get_action (link));
		dest = ev_link_action_get_dest (action);
		page = ev_link_dest_get_page (dest);
		page_label = ev_view_page_label_from_dest (EV_VIEW (window->priv->view), dest);
	} else {
		dest = ev_link_dest_new_page (page);
		action = ev_link_action_new_dest (dest);
		page_label = ev_page_cache_get_page_label (window->priv->page_cache, page);
	}

	if (!page_label)
		return;
	
	find_task.page_label = page_label;
	find_task.chapter = NULL;
	
	if (ev_document_links_has_document_links (EV_DOCUMENT_LINKS (window->priv->document))) {
		GtkTreeModel *model;
	
		g_object_get (G_OBJECT (window->priv->sidebar_links), "model", &model, NULL);
		
		if (model) {
			gtk_tree_model_foreach (model,
						ev_window_find_chapter,
						&find_task);
	
			g_object_unref (model);
		}
	}

	if (find_task.chapter)
		link_title = g_strdup_printf (_("Page %s - %s"), page_label, find_task.chapter);
	else
		link_title = g_strdup_printf (_("Page %s"), page_label);
	
	real_link = ev_link_new (link_title, action);
	
	ev_history_add_link (window->priv->history, real_link);

	g_free (find_task.chapter);
	g_free (link_title);
	g_free (page_label);
	g_object_unref (real_link);
}

static void
view_handle_link_cb (EvView *view, EvLink *link, EvWindow *window)
{
	int current_page = ev_page_cache_get_current_page (window->priv->page_cache);
	
	ev_window_add_history (window, 0, link);
	ev_window_add_history (window, current_page, NULL);
}

static void
history_changed_cb (EvPageCache *page_cache,
		    gint         page,
		    EvWindow 	*window)
{
	int current_page = ev_page_cache_get_current_page (window->priv->page_cache);

	ev_window_add_history (window, page, NULL);
	ev_window_add_history (window, current_page, NULL);

	return;
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
setup_chrome_from_metadata (EvWindow *window)
{
	EvChrome chrome = EV_CHROME_NORMAL;
	GValue show_toolbar = { 0, };

	if (ev_metadata_manager_get (NULL, "show_toolbar", &show_toolbar, FALSE)) {
		if (!g_value_get_boolean (&show_toolbar))
			chrome &= ~EV_CHROME_TOOLBAR;
		g_value_unset (&show_toolbar);
	}
	window->priv->chrome = chrome;
}

static void
setup_sidebar_from_metadata (EvWindow *window)
{
	gchar      *uri = window->priv->uri;
	EvDocument *document = window->priv->document;
	GtkWidget  *sidebar = window->priv->sidebar;
	GtkWidget  *links = window->priv->sidebar_links;
	GtkWidget  *thumbs = window->priv->sidebar_thumbs;
	GtkWidget  *attachments = window->priv->sidebar_attachments;
	GtkWidget  *layers = window->priv->sidebar_layers;
	GValue      sidebar_size = { 0, };
	GValue      sidebar_page = { 0, };
	GValue      sidebar_visibility = { 0, };

	if (ev_metadata_manager_get (uri, "sidebar_size", &sidebar_size, FALSE)) {
		gtk_paned_set_position (GTK_PANED (window->priv->hpaned),
					g_value_get_int (&sidebar_size));
		g_value_unset(&sidebar_size);
	}
	
	if (document && ev_metadata_manager_get (uri, "sidebar_page", &sidebar_page, TRUE)) {
		const char *page_id = g_value_get_string (&sidebar_page);

		if (strcmp (page_id, LINKS_SIDEBAR_ID) == 0 && ev_sidebar_page_support_document (EV_SIDEBAR_PAGE (links), document)) {
			ev_sidebar_set_page (EV_SIDEBAR (sidebar), links);
		} else if (strcmp (page_id, THUMBNAILS_SIDEBAR_ID) == 0 && ev_sidebar_page_support_document (EV_SIDEBAR_PAGE (thumbs), document)) {
			ev_sidebar_set_page (EV_SIDEBAR (sidebar), thumbs);
		} else if (strcmp (page_id, ATTACHMENTS_SIDEBAR_ID) == 0 && ev_sidebar_page_support_document (EV_SIDEBAR_PAGE (attachments), document)) {
			ev_sidebar_set_page (EV_SIDEBAR (sidebar), attachments);
		} else if (strcmp (page_id, LAYERS_SIDEBAR_ID) == 0 && ev_sidebar_page_support_document (EV_SIDEBAR_PAGE (layers), document)) {
			ev_sidebar_set_page (EV_SIDEBAR (sidebar), layers);
		}
		g_value_unset (&sidebar_page);
	} else if (document) {
		if (ev_sidebar_page_support_document (EV_SIDEBAR_PAGE (links), document)) {
			ev_sidebar_set_page (EV_SIDEBAR (sidebar), links);
		} else if (ev_sidebar_page_support_document (EV_SIDEBAR_PAGE (thumbs), document)) {
			ev_sidebar_set_page (EV_SIDEBAR (sidebar), thumbs);
		} else if (ev_sidebar_page_support_document (EV_SIDEBAR_PAGE (attachments), document)) {
			ev_sidebar_set_page (EV_SIDEBAR (sidebar), attachments);
		} else if (ev_sidebar_page_support_document (EV_SIDEBAR_PAGE (layers), document)) {
			ev_sidebar_set_page (EV_SIDEBAR (sidebar), layers);
		}
	}

	if (ev_metadata_manager_get (uri, "sidebar_visibility", &sidebar_visibility, FALSE)) {
		update_chrome_flag (window, EV_CHROME_SIDEBAR, g_value_get_boolean (&sidebar_visibility));
		g_value_unset (&sidebar_visibility);
		update_chrome_visibility (window);
	}
}

static void
setup_document_from_metadata (EvWindow *window)
{
	gchar *uri = window->priv->uri;
	GValue page = { 0, };
	GValue width = { 0, };
	GValue height = { 0, };
	GValue width_ratio = { 0, };
	GValue height_ratio = { 0, };

	/* View the previously shown page, but make sure to not open a document on
	 * the last page, since closing it on the last page most likely means the
	 * user was finished reading the document. In that case, reopening should
	 * show the first page. */
	if (uri && ev_metadata_manager_get (uri, "page", &page, TRUE)) {
		gint n_pages;
		gint new_page;
		
		n_pages = ev_page_cache_get_n_pages (window->priv->page_cache);
		new_page = CLAMP (g_value_get_int (&page), 0, n_pages - 1);
		ev_page_cache_set_current_page (window->priv->page_cache,
						new_page);
		g_value_unset (&page);
	}

	setup_sidebar_from_metadata (window);

	if (ev_metadata_manager_get (uri, "window_width", &width, TRUE) &&
	    ev_metadata_manager_get (uri, "window_height", &height, TRUE))
		return; /* size was already set in setup_size_from_metadata */

	if (ev_metadata_manager_get (uri, "window_width_ratio", &width_ratio, FALSE) &&
	    ev_metadata_manager_get (uri, "window_height_ratio", &height_ratio, FALSE)) {
		gint       document_width;
		gint       document_height;
		GdkScreen *screen;
		gint       request_width;
		gint       request_height;

		ev_page_cache_get_max_width (window->priv->page_cache, 
					     0, 1.0,
					     &document_width);
		ev_page_cache_get_max_height (window->priv->page_cache, 
					     0, 1.0,
					     &document_height);			
		
		request_width = g_value_get_double (&width_ratio) * document_width;
		request_height = g_value_get_double (&height_ratio) * document_height;
		
		screen = gtk_window_get_screen (GTK_WINDOW (window));
		
		if (screen) {
			request_width = MIN (request_width, gdk_screen_get_width (screen));
			request_height = MIN (request_width, gdk_screen_get_height (screen));
		}
		
		if (request_width > 0 && request_height > 0) {
			gtk_window_resize (GTK_WINDOW (window),
					   request_width,
					   request_height);
		}
	    	g_value_unset (&width_ratio);
		g_value_unset (&height_ratio);
	}
}

static void
setup_size_from_metadata (EvWindow *window)
{
	char *uri = window->priv->uri;
	GValue width = { 0, };
	GValue height = { 0, };
	GValue maximized = { 0, };
	GValue x = { 0, };
	GValue y = { 0, };

	if (ev_metadata_manager_get (uri, "window_maximized", &maximized, FALSE)) {
		if (g_value_get_boolean (&maximized)) {
			gtk_window_maximize (GTK_WINDOW (window));
			g_value_unset (&maximized);
			return;
		} else {
			gtk_window_unmaximize (GTK_WINDOW (window));
		}
		g_value_unset (&maximized);
	}

	if (ev_metadata_manager_get (uri, "window_x", &x, TRUE) &&
	    ev_metadata_manager_get (uri, "window_y", &y, TRUE)) {
		gtk_window_move (GTK_WINDOW (window), g_value_get_int (&x),
				 g_value_get_int (&y));
	        g_value_unset (&x);
	        g_value_unset (&y);
	}

        if (ev_metadata_manager_get (uri, "window_width", &width, TRUE) &&
	    ev_metadata_manager_get (uri, "window_height", &height, TRUE)) {
		gtk_window_resize (GTK_WINDOW (window),
				   g_value_get_int (&width),
				   g_value_get_int (&height));
	    	g_value_unset (&width);
		g_value_unset (&height);
	}
}

static void
setup_view_from_metadata (EvWindow *window)
{
	EvView *view = EV_VIEW (window->priv->view);
	gchar *uri = window->priv->uri;
	GEnumValue *enum_value;
	GValue sizing_mode = { 0, };
	GValue zoom = { 0, };
	GValue continuous = { 0, };
	GValue dual_page = { 0, };
	GValue presentation = { 0, };
	GValue fullscreen = { 0, };
	GValue rotation = { 0, };

	/* Sizing mode */
	if (ev_metadata_manager_get (uri, "sizing_mode", &sizing_mode, FALSE)) {
		enum_value = g_enum_get_value_by_nick
			(g_type_class_peek (EV_TYPE_SIZING_MODE), g_value_get_string (&sizing_mode));
		g_value_unset (&sizing_mode);
		ev_view_set_sizing_mode (view, enum_value->value);
	}

	/* Zoom */
	if (ev_metadata_manager_get (uri, "zoom", &zoom, FALSE) &&
	    ev_view_get_sizing_mode (view) == EV_SIZING_FREE) {
		gdouble zoom_value;

		zoom_value = g_value_get_double (&zoom);
		zoom_value *= get_screen_dpi (GTK_WINDOW (window)) / 72.0;
		ev_view_set_zoom (view, zoom_value, FALSE);
		g_value_unset (&zoom);
	}

	/* Continuous */
	if (ev_metadata_manager_get (uri, "continuous", &continuous, FALSE)) {
		ev_view_set_continuous (view, g_value_get_boolean (&continuous));
		g_value_unset (&continuous);
	}

	/* Dual page */
	if (ev_metadata_manager_get (uri, "dual-page", &dual_page, FALSE)) {
		ev_view_set_dual_page (view, g_value_get_boolean (&dual_page));
		g_value_unset (&dual_page);
	}

	/* Presentation */
	if (ev_metadata_manager_get (uri, "presentation", &presentation, FALSE)) {
		if (g_value_get_boolean (&presentation) && uri) {
			ev_window_run_presentation (window);
		}
		g_value_unset (&presentation);
	}

	/* Fullscreen */
	if (ev_metadata_manager_get (uri, "fullscreen", &fullscreen, FALSE)) {
		if (g_value_get_boolean (&fullscreen) && uri) {
			ev_window_run_fullscreen (window);
		}
		g_value_unset (&fullscreen);
	}

	/* Rotation */
	if (ev_metadata_manager_get (uri, "rotation", &rotation, TRUE)) {
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
		g_value_unset (&rotation);
	}
}

static void
ev_window_clear_thumbnail_job (EvWindow *ev_window)
{
	if (ev_window->priv->thumbnail_job != NULL) {
		if (!ev_job_is_finished (ev_window->priv->thumbnail_job))
			ev_job_cancel (ev_window->priv->thumbnail_job);
		
		g_signal_handlers_disconnect_by_func (ev_window->priv->thumbnail_job,
						      ev_window_set_icon_from_thumbnail,
						      ev_window);
		g_object_unref (ev_window->priv->thumbnail_job);
		ev_window->priv->thumbnail_job = NULL;
	}
}

static void
ev_window_set_icon_from_thumbnail (EvJobThumbnail *job,
				   EvWindow       *ev_window)
{
	if (job->thumbnail) {
		gtk_window_set_icon (GTK_WINDOW (ev_window),
				     job->thumbnail);
	}

	ev_window_clear_thumbnail_job (ev_window);
}

static void
ev_window_refresh_window_thumbnail (EvWindow *ev_window, int rotation)
{
	gint page_width, page_height;
	gdouble scale;
	EvDocument *document = ev_window->priv->document;
	
	if (!EV_IS_DOCUMENT_THUMBNAILS (document) ||
	    ev_page_cache_get_n_pages (ev_window->priv->page_cache) <= 0 ||
	    ev_page_cache_check_dimensions (ev_window->priv->page_cache)) {
		return;
	}
	
	ev_window_clear_thumbnail_job (ev_window);
	
	ev_page_cache_get_size (ev_window->priv->page_cache,
				0, 0, 1.0,
				&page_width, &page_height);
	scale = (gdouble)128 / (gdouble)page_width;
	
	ev_window->priv->thumbnail_job = ev_job_thumbnail_new (document, 0, rotation, scale);
	g_signal_connect (ev_window->priv->thumbnail_job, "finished",
			  G_CALLBACK (ev_window_set_icon_from_thumbnail),
			  ev_window);
	ev_job_scheduler_push_job (ev_window->priv->thumbnail_job, EV_JOB_PRIORITY_NONE);
}

static gboolean
ev_window_setup_document (EvWindow *ev_window)
{
	const EvDocumentInfo *info;
	EvDocument *document = ev_window->priv->document;
	EvSidebar *sidebar = EV_SIDEBAR (ev_window->priv->sidebar);
	GtkAction *action;

	ev_window->priv->setup_document_idle = 0;
	
	ev_window_refresh_window_thumbnail (ev_window, 0);

	ev_window_set_page_mode (ev_window, PAGE_MODE_DOCUMENT);
	ev_window_title_set_document (ev_window->priv->title, document);
	ev_window_title_set_uri (ev_window->priv->title, ev_window->priv->uri);

	ev_sidebar_set_document (sidebar, document);

	action = gtk_action_group_get_action (ev_window->priv->action_group, PAGE_SELECTOR_ACTION);
	ev_page_action_set_document (EV_PAGE_ACTION (action), document);
	ev_window_setup_action_sensitivity (ev_window);

	if (ev_window->priv->history)
		g_object_unref (ev_window->priv->history);
	ev_window->priv->history = ev_history_new ();
	action = gtk_action_group_get_action (ev_window->priv->action_group, NAVIGATION_ACTION);
        ev_navigation_action_set_history (EV_NAVIGATION_ACTION (action), ev_window->priv->history);
	
	if (ev_window->priv->properties) {
		ev_properties_dialog_set_document (EV_PROPERTIES_DIALOG (ev_window->priv->properties),
						   ev_window->priv->uri,
					           ev_window->priv->document);
	}
	
	info = ev_page_cache_get_info (ev_window->priv->page_cache);
	update_document_mode (ev_window, info->mode);

	gtk_widget_grab_focus (ev_window->priv->view);

	return FALSE;
}

static void
ev_window_set_document (EvWindow *ev_window, EvDocument *document)
{
	EvView *view = EV_VIEW (ev_window->priv->view);

	if (ev_window->priv->document)
		g_object_unref (ev_window->priv->document);
	ev_window->priv->document = g_object_ref (document);

	ev_window_set_message_area (ev_window, NULL);
	
	ev_window->priv->page_cache = ev_page_cache_get (ev_window->priv->document);
	g_signal_connect (ev_window->priv->page_cache, "page-changed",
			  G_CALLBACK (page_changed_cb), ev_window);
	g_signal_connect (ev_window->priv->page_cache, "history-changed",
			  G_CALLBACK (history_changed_cb), ev_window);

	if (ev_window->priv->in_reload && ev_window->priv->dest) {
		gint page;

		/* Restart the current page */
		page = CLAMP (ev_link_dest_get_page (ev_window->priv->dest),
			      0,
			      ev_page_cache_get_n_pages (ev_window->priv->page_cache) - 1);
		ev_page_cache_set_current_page (ev_window->priv->page_cache, page);
		g_object_unref (ev_window->priv->dest);
		ev_window->priv->dest = NULL;
	}

	if (ev_page_cache_get_n_pages (ev_window->priv->page_cache) <= 0) {
		ev_window_warning_message (ev_window, "%s",
					   _("The document contains no pages"));
	} else if (ev_page_cache_check_dimensions (ev_window->priv->page_cache)) {
		ev_window_warning_message (ev_window, "%s",
					   _("The document contains only empty pages"));
	} else {
		ev_view_set_document (view, document);
	}

	if (ev_window->priv->setup_document_idle > 0)
		g_source_remove (ev_window->priv->setup_document_idle);

	ev_window->priv->setup_document_idle = g_idle_add ((GSourceFunc)ev_window_setup_document, ev_window);
}

static void
ev_window_document_changed (EvWindow *ev_window,
			    gpointer  user_data)
{
	ev_window_reload_document (ev_window, NULL);
}

static void
ev_window_password_view_unlock (EvWindow *ev_window)
{
	const gchar *password;
	
	g_assert (ev_window->priv->load_job);

	password = ev_password_view_get_password (EV_PASSWORD_VIEW (ev_window->priv->password_view));
	ev_job_load_set_password (EV_JOB_LOAD (ev_window->priv->load_job), password);
	ev_job_scheduler_push_job (ev_window->priv->load_job, EV_JOB_PRIORITY_NONE);
}

static void
ev_window_clear_load_job (EvWindow *ev_window)
{
	if (ev_window->priv->load_job != NULL) {
		if (!ev_job_is_finished (ev_window->priv->load_job))
			ev_job_cancel (ev_window->priv->load_job);
		
		g_signal_handlers_disconnect_by_func (ev_window->priv->load_job, ev_window_load_job_cb, ev_window);
		g_object_unref (ev_window->priv->load_job);
		ev_window->priv->load_job = NULL;
	}
}

static void
ev_window_clear_reload_job (EvWindow *ev_window)
{
	if (ev_window->priv->reload_job != NULL) {
		if (!ev_job_is_finished (ev_window->priv->reload_job))
			ev_job_cancel (ev_window->priv->reload_job);
		
		g_signal_handlers_disconnect_by_func (ev_window->priv->reload_job, ev_window_reload_job_cb, ev_window);
		g_object_unref (ev_window->priv->reload_job);
		ev_window->priv->reload_job = NULL;
	}
}

static void
ev_window_clear_local_uri (EvWindow *ev_window)
{
	if (ev_window->priv->local_uri) {
		ev_tmp_uri_unlink (ev_window->priv->local_uri);
		g_free (ev_window->priv->local_uri);
		ev_window->priv->local_uri = NULL;
	}
}

static void
ev_window_clear_temp_symlink (EvWindow *ev_window)
{
	GFile *file, *tempdir;

	if (!ev_window->priv->uri)
		return;

	file = g_file_new_for_uri (ev_window->priv->uri);
	tempdir = g_file_new_for_path (ev_tmp_dir ());

	if (g_file_has_prefix (file, tempdir)) {
		GFileInfo *file_info;
		GError    *error = NULL;

		file_info = g_file_query_info (file,
					       G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK,
					       G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
					       NULL, &error);
		if (file_info) {
			if (g_file_info_get_is_symlink (file_info))
				g_file_delete (file, NULL, NULL);
			g_object_unref (file_info);
		} else {
			g_warning ("Error deleting temp symlink: %s\n", error->message);
			g_error_free (error);
		}
	}

	g_object_unref (file);
	g_object_unref (tempdir);
}

static void
ev_window_handle_link (EvWindow *ev_window,
		       EvLinkDest *dest)
{
	if (dest) {
		EvLink *link;
		EvLinkAction *link_action;

		link_action = ev_link_action_new_dest (dest);
		link = ev_link_new (NULL, link_action);
		ev_view_handle_link (EV_VIEW (ev_window->priv->view), link);
		g_object_unref (link);
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
ev_window_load_job_cb (EvJob *job,
		       gpointer data)
{
	EvWindow *ev_window = EV_WINDOW (data);
	EvDocument *document = EV_JOB (job)->document;
	EvJobLoad *job_load = EV_JOB_LOAD (job);

	g_assert (job_load->uri);

	ev_view_set_loading (EV_VIEW (ev_window->priv->view), FALSE);

	/* Success! */
	if (!ev_job_is_failed (job)) {
		ev_window_set_document (ev_window, document);

		setup_document_from_metadata (ev_window);
		setup_view_from_metadata (ev_window);

		ev_window_add_recent (ev_window, ev_window->priv->uri);

		ev_window_title_set_type (ev_window->priv->title,
					  EV_WINDOW_TITLE_DOCUMENT);
		if (job_load->password) {
			GPasswordSave flags;

			flags = ev_password_view_get_password_save_flags (
				EV_PASSWORD_VIEW (ev_window->priv->password_view));
			ev_keyring_save_password (ev_window->priv->uri,
						  job_load->password,
						  flags);
		}

		ev_window_handle_link (ev_window, ev_window->priv->dest);
		/* Already unrefed by ev_link_action
		 * FIXME: link action should inc dest ref counting
		 * or not unref it at all
		 */
		ev_window->priv->dest = NULL;

		switch (ev_window->priv->window_mode) {
		        case EV_WINDOW_MODE_FULLSCREEN:
				ev_window_run_fullscreen (ev_window);
				break;
		        case EV_WINDOW_MODE_PRESENTATION:
				ev_window_run_presentation (ev_window);
				break;
		        default:
				break;
		}

		if (ev_window->priv->search_string && EV_IS_DOCUMENT_FIND (document)) {
			ev_window_cmd_edit_find (NULL, ev_window);
			egg_find_bar_set_search_string (EGG_FIND_BAR (ev_window->priv->find_bar),
							ev_window->priv->search_string);
		}

		g_free (ev_window->priv->search_string);
		ev_window->priv->search_string = NULL;

		/* Create a monitor for the document */
		ev_window->priv->monitor = ev_file_monitor_new (ev_window->priv->uri);
		g_signal_connect_swapped (ev_window->priv->monitor, "changed",
					  G_CALLBACK (ev_window_document_changed),
					  ev_window);
		
		ev_window_clear_load_job (ev_window);
		return;
	}

	if (g_error_matches (job->error, EV_DOCUMENT_ERROR, EV_DOCUMENT_ERROR_ENCRYPTED)) {
		gchar *password;
		
		setup_view_from_metadata (ev_window);
		
		/* First look whether password is in keyring */
		password = ev_keyring_lookup_password (ev_window->priv->uri);
		if (password) {
			if (job_load->password && strcmp (password, job_load->password) == 0) {
				/* Password in kering is wrong */
				ev_job_load_set_password (job_load, NULL);
				/* FIXME: delete password from keyring? */
			} else {
				ev_job_load_set_password (job_load, password);
				ev_job_scheduler_push_job (job, EV_JOB_PRIORITY_NONE);
				g_free (password);
				return;
			}

			g_free (password);
		}

		/* We need to ask the user for a password */
		ev_window_title_set_uri (ev_window->priv->title,
					 ev_window->priv->uri);
		ev_window_title_set_type (ev_window->priv->title,
					  EV_WINDOW_TITLE_PASSWORD);

		ev_password_view_set_uri (EV_PASSWORD_VIEW (ev_window->priv->password_view),
					  job_load->uri);

		ev_window_set_page_mode (ev_window, PAGE_MODE_PASSWORD);

		ev_job_load_set_password (job_load, NULL);
		ev_password_view_ask_password (EV_PASSWORD_VIEW (ev_window->priv->password_view));
	} else {
		ev_window_error_message (ev_window, job->error, 
					 "%s", _("Unable to open document"));
		ev_window_clear_load_job (ev_window);
	}	
}

static void
ev_window_reload_job_cb (EvJob    *job,
			 EvWindow *ev_window)
{
	GtkWidget *widget;
	EvLinkDest *dest = NULL;

	if (ev_job_is_failed (job)) {
		ev_window_clear_reload_job (ev_window);
		ev_window->priv->in_reload = FALSE;
		g_object_unref (ev_window->priv->dest);
		ev_window->priv->dest = NULL;
		
		return;
	}

	if (ev_window->priv->dest) {
		dest = g_object_ref (ev_window->priv->dest);
	}
	ev_window_set_document (ev_window, job->document);

	ev_window_handle_link (ev_window, dest);

	/* Restart the search after reloading */
	widget = gtk_window_get_focus (GTK_WINDOW (ev_window));
	if (widget && gtk_widget_get_ancestor (widget, EGG_TYPE_FIND_BAR)) {
		find_bar_search_changed_cb (EGG_FIND_BAR (ev_window->priv->find_bar),
					    NULL, ev_window);
	}
	
	ev_window_clear_reload_job (ev_window);
	ev_window->priv->in_reload = FALSE;
}

/**
 * ev_window_get_uri:
 * @ev_window: The instance of the #EvWindow.
 *
 * It returns the uri of the document showed in the #EvWindow.
 *
 * Returns: the uri of the document showed in the #EvWindow.
 */
const char *
ev_window_get_uri (EvWindow *ev_window)
{
	return ev_window->priv->uri;
}

/**
 * ev_window_close_dialogs:
 * @ev_window: The window where dialogs will be closed.
 *
 * It looks for password, print and properties dialogs and closes them and
 * frees them from memory. If there is any print job it does free it too.
 */
static void
ev_window_close_dialogs (EvWindow *ev_window)
{
	if (ev_window->priv->print_dialog)
		gtk_widget_destroy (ev_window->priv->print_dialog);
	ev_window->priv->print_dialog = NULL;
	
	if (ev_window->priv->properties)
		gtk_widget_destroy (ev_window->priv->properties);
	ev_window->priv->properties = NULL;
}

static void
ev_window_clear_progress_idle (EvWindow *ev_window)
{
	if (ev_window->priv->progress_idle > 0)
		g_source_remove (ev_window->priv->progress_idle);
	ev_window->priv->progress_idle = 0;
}

static void
reset_progress_idle (EvWindow *ev_window)
{
	ev_window->priv->progress_idle = 0;
}

static void
ev_window_show_progress_message (EvWindow   *ev_window,
				 guint       interval,
				 GSourceFunc function)
{
	if (ev_window->priv->progress_idle > 0)
		g_source_remove (ev_window->priv->progress_idle);
	ev_window->priv->progress_idle =
		g_timeout_add_seconds_full (G_PRIORITY_DEFAULT,
					    interval, function,
					    ev_window,
					    (GDestroyNotify)reset_progress_idle);
}

static void
ev_window_reset_progress_cancellable (EvWindow *ev_window)
{
	if (ev_window->priv->progress_cancellable)
		g_cancellable_reset (ev_window->priv->progress_cancellable);
	else
		ev_window->priv->progress_cancellable = g_cancellable_new ();
}

static void
ev_window_progress_response_cb (EvProgressMessageArea *area,
				gint                   response,
				EvWindow              *ev_window)
{
	if (response == GTK_RESPONSE_CANCEL)
		g_cancellable_cancel (ev_window->priv->progress_cancellable);
	ev_window_set_message_area (ev_window, NULL);
}

static gboolean 
show_loading_progress (EvWindow *ev_window)
{
	GtkWidget *area;
	gchar     *text;
	gchar 	  *display_name;
	
	if (ev_window->priv->message_area)
		return FALSE;

	text = g_uri_unescape_string (ev_window->priv->uri, NULL);
	display_name = g_markup_escape_text (text, -1);
	g_free (text);
	text = g_strdup_printf (_("Loading document from “%s”"),
				display_name);

	area = ev_progress_message_area_new (GTK_STOCK_OPEN,
					     text,
					     GTK_STOCK_CLOSE,
					     GTK_RESPONSE_CLOSE,
					     GTK_STOCK_CANCEL,
					     GTK_RESPONSE_CANCEL,
					     NULL);
	g_signal_connect (area, "response",
			  G_CALLBACK (ev_window_progress_response_cb),
			  ev_window);
	gtk_widget_show (area);
	ev_window_set_message_area (ev_window, area);

	g_free (text);
	g_free (display_name);

	return FALSE;
}

static void
ev_window_load_remote_failed (EvWindow *ev_window,
			      GError   *error)
{
	ev_view_set_loading (EV_VIEW (ev_window->priv->view), FALSE);
	ev_window->priv->in_reload = FALSE;
	ev_window_error_message (ev_window, error, 
				 "%s", _("Unable to open document"));
	g_free (ev_window->priv->local_uri);
	ev_window->priv->local_uri = NULL;
	ev_window->priv->uri_mtime = 0;
}

static void
set_uri_mtime (GFile        *source,
	       GAsyncResult *async_result,
	       EvWindow     *ev_window)
{
	GFileInfo *info;
	GError *error = NULL;

	info = g_file_query_info_finish (source, async_result, &error);

	if (error) {
		ev_window->priv->uri_mtime = 0;
		g_error_free (error);
	} else {
		GTimeVal mtime;
		
		g_file_info_get_modification_time (info, &mtime);
		ev_window->priv->uri_mtime = mtime.tv_sec;
		g_object_unref (info);
	}

	g_object_unref (source);
}

static void
mount_volume_ready_cb (GFile        *source,
		       GAsyncResult *async_result,
		       EvWindow     *ev_window)
{
	GError *error = NULL;

	g_file_mount_enclosing_volume_finish (source, async_result, &error);

	if (error) {
		ev_window_load_remote_failed (ev_window, error);
		g_object_unref (source);
		g_error_free (error);
	} else {
		/* Volume successfully mounted,
		   try opening the file again */
		ev_window_load_file_remote (ev_window, source);
	}
}

static void
window_open_file_copy_ready_cb (GFile        *source,
				GAsyncResult *async_result,
				EvWindow     *ev_window)
{
	GError *error = NULL;

	ev_window_clear_progress_idle (ev_window);
	ev_window_set_message_area (ev_window, NULL);

	g_file_copy_finish (source, async_result, &error);
	if (!error) {
		ev_job_scheduler_push_job (ev_window->priv->load_job, EV_JOB_PRIORITY_NONE);
		g_file_query_info_async (source,
					 G_FILE_ATTRIBUTE_TIME_MODIFIED,
					 0, G_PRIORITY_DEFAULT,
					 NULL,
					 (GAsyncReadyCallback)set_uri_mtime,
					 ev_window);
		return;
	}

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_MOUNTED)) {
		GMountOperation *operation;

		operation = gtk_mount_operation_new (GTK_WINDOW (ev_window));
		g_file_mount_enclosing_volume (source,
					       G_MOUNT_MOUNT_NONE,
					       operation, NULL,
					       (GAsyncReadyCallback)mount_volume_ready_cb,
					       ev_window);
		g_object_unref (operation);
	} else if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		ev_window_clear_load_job (ev_window);
		ev_window_clear_local_uri (ev_window);
		g_free (ev_window->priv->uri);
		ev_window->priv->uri = NULL;
		g_object_unref (source);
		
		ev_view_set_loading (EV_VIEW (ev_window->priv->view), FALSE);
	} else {
		ev_window_load_remote_failed (ev_window, error);
		g_object_unref (source);
	}
	
	g_error_free (error);
}

static void
window_open_file_copy_progress_cb (goffset   n_bytes,
				   goffset   total_bytes,
				   EvWindow *ev_window)
{
	gchar *status;
	gdouble fraction;
	
	if (!ev_window->priv->message_area)
		return;

	if (total_bytes <= 0)
		return;

	fraction = n_bytes / (gdouble)total_bytes;
	status = g_strdup_printf (_("Downloading document (%d%%)"),
				  (gint)(fraction * 100));
	
	ev_progress_message_area_set_status (EV_PROGRESS_MESSAGE_AREA (ev_window->priv->message_area),
					     status);
	ev_progress_message_area_set_fraction (EV_PROGRESS_MESSAGE_AREA (ev_window->priv->message_area),
					       fraction);

	g_free (status);
}

static void
ev_window_load_file_remote (EvWindow *ev_window,
			    GFile    *source_file)
{
	GFile *target_file;
	
	if (!ev_window->priv->local_uri) {
		gchar *tmp_name;
		gchar *base_name;

		/* We'd like to keep extension of source uri since
		 * it helps to resolve some mime types, say cbz */
		tmp_name = ev_tmp_filename (NULL);
		base_name = g_file_get_basename (source_file);
		ev_window->priv->local_uri = g_strconcat ("file:", tmp_name, "-",
							  base_name, NULL);
		ev_job_load_set_uri (EV_JOB_LOAD (ev_window->priv->load_job),
				     ev_window->priv->local_uri);
		g_free (base_name);
		g_free (tmp_name);
	}

	ev_window_reset_progress_cancellable (ev_window);
	
	target_file = g_file_new_for_uri (ev_window->priv->local_uri);
	g_file_copy_async (source_file, target_file,
			   0, G_PRIORITY_DEFAULT,
			   ev_window->priv->progress_cancellable,
			   (GFileProgressCallback)window_open_file_copy_progress_cb,
			   ev_window, 
			   (GAsyncReadyCallback)window_open_file_copy_ready_cb,
			   ev_window);
	g_object_unref (target_file);

	ev_window_show_progress_message (ev_window, 1,
					 (GSourceFunc)show_loading_progress);
}

void
ev_window_open_uri (EvWindow       *ev_window,
		    const char     *uri,
		    EvLinkDest     *dest,
		    EvWindowRunMode mode,
		    const gchar    *search_string)
{
	GFile *source_file;

	ev_window->priv->in_reload = FALSE;
	
	if (ev_window->priv->uri &&
	    g_ascii_strcasecmp (ev_window->priv->uri, uri) == 0) {
		ev_window_reload_document (ev_window, dest);
		return;
	}

	if (ev_window->priv->monitor) {
		g_object_unref (ev_window->priv->monitor);
		ev_window->priv->monitor = NULL;
	}
	
	ev_window_close_dialogs (ev_window);
	ev_window_clear_load_job (ev_window);
	ev_window_clear_local_uri (ev_window);
	ev_view_set_loading (EV_VIEW (ev_window->priv->view), TRUE);

	ev_window->priv->window_mode = mode;

	if (ev_window->priv->uri)
		g_free (ev_window->priv->uri);
	ev_window->priv->uri = g_strdup (uri);

	if (ev_window->priv->search_string)
		g_free (ev_window->priv->search_string);
	ev_window->priv->search_string = search_string ?
		g_strdup (search_string) : NULL;

	if (ev_window->priv->dest)
		g_object_unref (ev_window->priv->dest);
	ev_window->priv->dest = dest ? g_object_ref (dest) : NULL;

	setup_size_from_metadata (ev_window);

	ev_window->priv->load_job = ev_job_load_new (uri);
	g_signal_connect (ev_window->priv->load_job,
			  "finished",
			  G_CALLBACK (ev_window_load_job_cb),
			  ev_window);

	source_file = g_file_new_for_uri (uri);
	if (!g_file_is_native (source_file) && !ev_window->priv->local_uri) {
		ev_window_load_file_remote (ev_window, source_file);
	} else {
		g_object_unref (source_file);
		ev_job_scheduler_push_job (ev_window->priv->load_job, EV_JOB_PRIORITY_NONE);
	}
}

static void
ev_window_reload_local (EvWindow *ev_window)
{
	const gchar *uri;
	
	uri = ev_window->priv->local_uri ? ev_window->priv->local_uri : ev_window->priv->uri;
	ev_window->priv->reload_job = ev_job_load_new (uri);
	g_signal_connect (ev_window->priv->reload_job, "finished",
			  G_CALLBACK (ev_window_reload_job_cb),
			  ev_window);
	ev_job_scheduler_push_job (ev_window->priv->reload_job, EV_JOB_PRIORITY_NONE);
}

static gboolean 
show_reloading_progress (EvWindow *ev_window)
{
	GtkWidget *area;
	gchar     *text;
	
	if (ev_window->priv->message_area)
		return FALSE;
	
	text = g_strdup_printf (_("Reloading document from %s"),
				ev_window->priv->uri);
	area = ev_progress_message_area_new (GTK_STOCK_REFRESH,
					     text,
					     GTK_STOCK_CLOSE,
					     GTK_RESPONSE_CLOSE,
					     GTK_STOCK_CANCEL,
					     GTK_RESPONSE_CANCEL,
					     NULL);
	g_signal_connect (area, "response",
			  G_CALLBACK (ev_window_progress_response_cb),
			  ev_window);
	gtk_widget_show (area);
	ev_window_set_message_area (ev_window, area);
	g_free (text);

	return FALSE;
}

static void
reload_remote_copy_ready_cb (GFile        *remote,
			     GAsyncResult *async_result,
			     EvWindow     *ev_window)
{
	GError *error = NULL;
	
	ev_window_clear_progress_idle (ev_window);
	
	g_file_copy_finish (remote, async_result, &error);
	if (error) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			ev_window_error_message (ev_window, error,
						 "%s", _("Failed to reload document."));
		g_error_free (error);
	} else {
		ev_window_reload_local (ev_window);
	}
		
	g_object_unref (remote);
}

static void
reload_remote_copy_progress_cb (goffset   n_bytes,
				goffset   total_bytes,
				EvWindow *ev_window)
{
	gchar *status;
	gdouble fraction;
	
	if (!ev_window->priv->message_area)
		return;

	if (total_bytes <= 0)
		return;

	fraction = n_bytes / (gdouble)total_bytes;
	status = g_strdup_printf (_("Downloading document (%d%%)"),
				  (gint)(fraction * 100));
	
	ev_progress_message_area_set_status (EV_PROGRESS_MESSAGE_AREA (ev_window->priv->message_area),
					     status);
	ev_progress_message_area_set_fraction (EV_PROGRESS_MESSAGE_AREA (ev_window->priv->message_area),
					       fraction);

	g_free (status);
}

static void
query_remote_uri_mtime_cb (GFile        *remote,
			   GAsyncResult *async_result,
			   EvWindow     *ev_window)
{
	GFileInfo *info;
	GTimeVal   mtime;
	GError    *error = NULL;

	info = g_file_query_info_finish (remote, async_result, &error);
	if (error) {
		g_error_free (error);
		g_object_unref (remote);
		ev_window_reload_local (ev_window);

		return;
	}
	
	g_file_info_get_modification_time (info, &mtime);
	if (ev_window->priv->uri_mtime != mtime.tv_sec) {
		GFile *target_file;
			
		/* Remote file has changed */
		ev_window->priv->uri_mtime = mtime.tv_sec;

		ev_window_reset_progress_cancellable (ev_window);
		
		target_file = g_file_new_for_uri (ev_window->priv->local_uri);
		g_file_copy_async (remote, target_file,
				   G_FILE_COPY_OVERWRITE,
				   G_PRIORITY_DEFAULT,
				   ev_window->priv->progress_cancellable,
				   (GFileProgressCallback)reload_remote_copy_progress_cb,
				   ev_window, 
				   (GAsyncReadyCallback)reload_remote_copy_ready_cb,
				   ev_window);
		g_object_unref (target_file);
		ev_window_show_progress_message (ev_window, 1,
						 (GSourceFunc)show_reloading_progress);
	} else {
		g_object_unref (remote);
		ev_window_reload_local (ev_window);
	}
	
	g_object_unref (info);
}

static void
ev_window_reload_remote (EvWindow *ev_window)
{
	GFile *remote;
	
	remote = g_file_new_for_uri (ev_window->priv->uri);
	/* Reload the remote uri only if it has changed */
	g_file_query_info_async (remote,
				 G_FILE_ATTRIBUTE_TIME_MODIFIED,
				 0, G_PRIORITY_DEFAULT,
				 NULL,
				 (GAsyncReadyCallback)query_remote_uri_mtime_cb,
				 ev_window);
}

static void
ev_window_reload_document (EvWindow *ev_window,
			   EvLinkDest *dest)
{
	gint page;

	
	ev_window_clear_reload_job (ev_window);
	ev_window->priv->in_reload = TRUE;

	page = ev_page_cache_get_current_page (ev_window->priv->page_cache);
	
	if (ev_window->priv->dest)
		g_object_unref (ev_window->priv->dest);
	/* FIXME: save the scroll position too (xyz dest) */
	ev_window->priv->dest = dest ? g_object_ref (dest) : ev_link_dest_new_page (page);

	if (ev_window->priv->local_uri) {
		ev_window_reload_remote (ev_window);
	} else {
		ev_window_reload_local (ev_window);
	}
}

static void
file_open_dialog_response_cb (GtkWidget *chooser,
			      gint       response_id,
			      EvWindow  *ev_window)
{
	if (response_id == GTK_RESPONSE_OK) {
		GSList *uris;
		gchar  *uri;

		uris = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (chooser));

		ev_application_open_uri_list (EV_APP, uris,
					      gtk_window_get_screen (GTK_WINDOW (ev_window)),
					      GDK_CURRENT_TIME);

		g_slist_foreach (uris, (GFunc)g_free, NULL);
		g_slist_free (uris);

		uri = gtk_file_chooser_get_current_folder_uri (GTK_FILE_CHOOSER (chooser));
		ev_application_set_filechooser_uri (EV_APP,
						    GTK_FILE_CHOOSER_ACTION_OPEN,
						    uri);
		g_free (uri);
	}

	gtk_widget_destroy (chooser);
}

static void
ev_window_cmd_file_open (GtkAction *action, EvWindow *window)
{
	GtkWidget   *chooser;
	const gchar *default_uri;
	gchar       *parent_uri = NULL;

	chooser = gtk_file_chooser_dialog_new (_("Open Document"),
					       GTK_WINDOW (window),
					       GTK_FILE_CHOOSER_ACTION_OPEN,
					       GTK_STOCK_CANCEL,
					       GTK_RESPONSE_CANCEL,
					       GTK_STOCK_OPEN, GTK_RESPONSE_OK,
					       NULL);

	ev_document_factory_add_filters (chooser, NULL);
	gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (chooser), TRUE);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), FALSE);

	default_uri = ev_application_get_filechooser_uri (EV_APP, GTK_FILE_CHOOSER_ACTION_OPEN);
	if (!default_uri && window->priv->uri) {
		GFile *file, *parent;

		file = g_file_new_for_uri (window->priv->uri);
		parent = g_file_get_parent (file);
		if (parent) {
			parent_uri = g_file_get_uri (parent);
			default_uri = parent_uri;
			g_object_unref (parent);
		}
		g_object_unref (file);
	}

	if (default_uri) {
		gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (chooser), default_uri);
	} else {
		const gchar *folder;

		folder = g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS);
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser),
						     folder ? folder : g_get_home_dir ());
	}
	g_free (parent_uri);

	g_signal_connect (chooser, "response",
			  G_CALLBACK (file_open_dialog_response_cb),
			  window);

	gtk_widget_show (chooser);
}

static gchar *
ev_window_create_tmp_symlink (const gchar *filename, GError **error)
{
	gchar  *tmp_filename = NULL;
	gchar  *name;
	guint   i = 0;
	GError *link_error = NULL;
	GFile  *tmp_file = NULL;

	name = g_path_get_basename (filename);
	
	do {
		gchar *basename;

		if (tmp_filename)
			g_free (tmp_filename);
		if (tmp_file)
			g_object_unref (tmp_file);
		g_clear_error (&link_error);

		basename = g_strdup_printf ("%s-%d", name, i++);
		tmp_filename = g_build_filename (ev_tmp_dir (),
						 basename, NULL);
		
		g_free (basename);
		tmp_file = g_file_new_for_path (tmp_filename);
	} while (!g_file_make_symbolic_link (tmp_file, filename, NULL, &link_error) &&
		 g_error_matches (link_error, G_IO_ERROR, G_IO_ERROR_EXISTS));
	
	g_free (name);
	g_object_unref (tmp_file);

	if (link_error) {
		g_propagate_prefixed_error (error, 
					    link_error,
					    _("Couldn't create symlink “%s”: "),
					    tmp_filename);
		g_free (tmp_filename);
		
		return NULL;
	}

	return tmp_filename;
}

static void
ev_window_cmd_file_open_copy_at_dest (EvWindow *window, EvLinkDest *dest)
{
	GError      *error = NULL;
	gchar       *symlink_uri;
	gchar       *old_filename;
	gchar       *new_filename;
	const gchar *uri_unc;

	uri_unc = g_object_get_data (G_OBJECT (window->priv->document),
				     "uri-uncompressed");
	old_filename = g_filename_from_uri (uri_unc ? uri_unc : window->priv->uri,
					    NULL, NULL);
	new_filename = ev_window_create_tmp_symlink (old_filename, &error);

	if (error) {
		ev_window_error_message (window, error, 
					 "%s", _("Cannot open a copy."));
		g_error_free (error);
		g_free (old_filename);
		g_free (new_filename);

		return;
	}
		
	g_free (old_filename);

	symlink_uri = g_filename_to_uri (new_filename, NULL, NULL);
	g_free (new_filename);

	ev_application_open_uri_at_dest (EV_APP,
					 symlink_uri,
					 gtk_window_get_screen (GTK_WINDOW (window)),
					 dest,
					 0,
					 NULL, 
					 GDK_CURRENT_TIME);
	g_free (symlink_uri);
}

static void
ev_window_cmd_file_open_copy (GtkAction *action, EvWindow *window)
{
	EvPageCache *page_cache;
	EvLinkDest  *dest;
	gint         current_page;

	page_cache = ev_page_cache_get (window->priv->document);
	current_page = ev_page_cache_get_current_page (page_cache);
	
	dest = ev_link_dest_new_page (current_page);
	ev_window_cmd_file_open_copy_at_dest (window, dest);
	g_object_unref (dest);
}

static void
ev_window_cmd_recent_file_activate (GtkAction *action,
				    EvWindow  *window)
{
	GtkRecentInfo *info;
	const gchar   *uri;

	info = g_object_get_data (G_OBJECT (action), "gtk-recent-info");
	g_assert (info != NULL);
	
	uri = gtk_recent_info_get_uri (info);
	
	ev_application_open_uri_at_dest (EV_APP, uri,
					 gtk_window_get_screen (GTK_WINDOW (window)),
					 NULL, 0, NULL, GDK_CURRENT_TIME);
}

static void
ev_window_open_recent_action_item_activated (EvOpenRecentAction *action,
					     const gchar        *uri,
					     EvWindow           *window)
{
	ev_application_open_uri_at_dest (EV_APP, uri,
					 gtk_window_get_screen (GTK_WINDOW (window)),
					 NULL, 0, NULL, GDK_CURRENT_TIME);
}

static void
ev_window_add_recent (EvWindow *window, const char *filename)
{
	gtk_recent_manager_add_item (window->priv->recent_manager, filename);
}

static gint
compare_recent_items (GtkRecentInfo *a, GtkRecentInfo *b)
{
	gboolean     has_ev_a, has_ev_b;
	const gchar *evince = g_get_application_name ();

	has_ev_a = gtk_recent_info_has_application (a, evince);
	has_ev_b = gtk_recent_info_has_application (b, evince);
	
	if (has_ev_a && has_ev_b) {
		time_t time_a, time_b;

		time_a = gtk_recent_info_get_modified (a);
		time_b = gtk_recent_info_get_modified (b);

		return (time_b - time_a);
	} else if (has_ev_a) {
		return -1;
	} else if (has_ev_b) {
		return 1;
	}

	return 0;
}

/*
 * Doubles underscore to avoid spurious menu accels.
 */
static gchar * 
ev_window_get_recent_file_label (gint index, const gchar *filename)
{
	GString *str;
	gint length;
	const gchar *p;
	const gchar *end;
	gboolean is_rtl;
	
	is_rtl = (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL);

	g_return_val_if_fail (filename != NULL, NULL);
	
	length = strlen (filename);
	str = g_string_sized_new (length + 10);
	g_string_printf (str, "%s_%d.  ", is_rtl ? "\xE2\x80\x8F" : "", index);

	p = filename;
	end = filename + length;
 
	while (p != end) {
		const gchar *next;
		next = g_utf8_next_char (p);
 
		switch (*p) {
			case '_':
				g_string_append (str, "__");
				break;
			default:
				g_string_append_len (str, p, next - p);
				break;
		}
 
		p = next;
	}
 
	return g_string_free (str, FALSE);
}

static void
ev_window_setup_recent (EvWindow *ev_window)
{
	GList        *items, *l;
	guint         n_items = 0;
	const gchar  *evince = g_get_application_name ();
	static guint  i = 0;

	if (ev_window->priv->recent_ui_id > 0) {
		gtk_ui_manager_remove_ui (ev_window->priv->ui_manager,
					  ev_window->priv->recent_ui_id);
		gtk_ui_manager_ensure_update (ev_window->priv->ui_manager);
	}
	ev_window->priv->recent_ui_id = gtk_ui_manager_new_merge_id (ev_window->priv->ui_manager);

	if (ev_window->priv->recent_action_group) {
		gtk_ui_manager_remove_action_group (ev_window->priv->ui_manager,
						    ev_window->priv->recent_action_group);
		g_object_unref (ev_window->priv->recent_action_group);
	}
	ev_window->priv->recent_action_group = gtk_action_group_new ("RecentFilesActions");
	gtk_ui_manager_insert_action_group (ev_window->priv->ui_manager,
					    ev_window->priv->recent_action_group, 0);

	items = gtk_recent_manager_get_items (ev_window->priv->recent_manager);
	items = g_list_sort (items, (GCompareFunc) compare_recent_items);

	for (l = items; l && l->data; l = g_list_next (l)) {
		GtkRecentInfo *info;
		GtkAction     *action;
		gchar         *action_name;
		gchar         *label;

		info = (GtkRecentInfo *) l->data;

		if (!gtk_recent_info_has_application (info, evince) ||
		    (gtk_recent_info_is_local (info) && !gtk_recent_info_exists (info)))
			continue;

		action_name = g_strdup_printf ("RecentFile%u", i++);
		label = ev_window_get_recent_file_label (
			n_items + 1, gtk_recent_info_get_display_name (info));
		
		action = g_object_new (GTK_TYPE_ACTION,
				       "name", action_name,
				       "label", label,
				       NULL);

		g_object_set_data_full (G_OBJECT (action),
					"gtk-recent-info",
					gtk_recent_info_ref (info),
					(GDestroyNotify) gtk_recent_info_unref);
		
		g_signal_connect (action, "activate",
				  G_CALLBACK (ev_window_cmd_recent_file_activate),
				  (gpointer) ev_window);

		gtk_action_group_add_action (ev_window->priv->recent_action_group,
					     action);
		g_object_unref (action);

		gtk_ui_manager_add_ui (ev_window->priv->ui_manager,
				       ev_window->priv->recent_ui_id,
				       "/MainMenu/FileMenu/RecentFilesMenu",
				       label,
				       action_name,
				       GTK_UI_MANAGER_MENUITEM,
				       FALSE);
		g_free (action_name);
		g_free (label);

		if (++n_items == 5)
			break;
	}
	
	g_list_foreach (items, (GFunc) gtk_recent_info_unref, NULL);
	g_list_free (items);
}

static gboolean 
show_saving_progress (GFile *dst)
{
	EvWindow  *ev_window;
	GtkWidget *area;
	gchar     *text;
	gchar     *uri;
	EvSaveType save_type;

	ev_window = EV_WINDOW (g_object_get_data (G_OBJECT (dst), "ev-window"));
	ev_window->priv->progress_idle = 0;
	
	if (ev_window->priv->message_area)
		return FALSE;

	save_type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (dst), "save-type"));
	uri = g_file_get_uri (dst);
	switch (save_type) {
	case EV_SAVE_DOCUMENT:
		text = g_strdup_printf (_("Saving document to %s"), uri);
		break;
	case EV_SAVE_ATTACHMENT:
		text = g_strdup_printf (_("Saving attachment to %s"), uri);
		break;
	case EV_SAVE_IMAGE:
		text = g_strdup_printf (_("Saving image to %s"), uri);
		break;
	default:
		g_assert_not_reached ();
	}
	g_free (uri);
	area = ev_progress_message_area_new (GTK_STOCK_SAVE,
					     text,
					     GTK_STOCK_CLOSE,
					     GTK_RESPONSE_CLOSE,
					     GTK_STOCK_CANCEL,
					     GTK_RESPONSE_CANCEL,
					     NULL);
	g_signal_connect (area, "response",
			  G_CALLBACK (ev_window_progress_response_cb),
			  ev_window);
	gtk_widget_show (area);
	ev_window_set_message_area (ev_window, area);
	g_free (text);

	return FALSE;
}

static void
window_save_file_copy_ready_cb (GFile        *src,
				GAsyncResult *async_result,
				GFile        *dst)
{
	EvWindow *ev_window;
	GError   *error = NULL;

	ev_window = EV_WINDOW (g_object_get_data (G_OBJECT (dst), "ev-window"));
	ev_window_clear_progress_idle (ev_window);
	
	if (g_file_copy_finish (src, async_result, &error)) {
		ev_tmp_file_unlink (src);
		return;
	}

	if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		gchar *name;
		
		name = g_file_get_basename (dst);
		ev_window_error_message (ev_window, error,
					 _("The file could not be saved as “%s”."),
					 name);
		g_free (name);
	}
	ev_tmp_file_unlink (src);
	g_error_free (error);
}

static void
window_save_file_copy_progress_cb (goffset n_bytes,
				   goffset total_bytes,
				   GFile  *dst)
{
	EvWindow  *ev_window;
	EvSaveType save_type;
	gchar     *status;
	gdouble    fraction;

	ev_window = EV_WINDOW (g_object_get_data (G_OBJECT (dst), "ev-window"));
	
	if (!ev_window->priv->message_area)
		return;

	if (total_bytes <= 0)
		return;

	fraction = n_bytes / (gdouble)total_bytes;
	save_type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (dst), "save-type"));

	switch (save_type) {
	case EV_SAVE_DOCUMENT:
		status = g_strdup_printf (_("Uploading document (%d%%)"),
					  (gint)(fraction * 100));
		break;
	case EV_SAVE_ATTACHMENT:
		status = g_strdup_printf (_("Uploading attachment (%d%%)"),
					  (gint)(fraction * 100));
		break;
	case EV_SAVE_IMAGE:
		status = g_strdup_printf (_("Uploading image (%d%%)"),
					  (gint)(fraction * 100));
		break;
	default:
		g_assert_not_reached ();
	}
	
	ev_progress_message_area_set_status (EV_PROGRESS_MESSAGE_AREA (ev_window->priv->message_area),
					     status);
	ev_progress_message_area_set_fraction (EV_PROGRESS_MESSAGE_AREA (ev_window->priv->message_area),
					       fraction);

	g_free (status);
}

static void
ev_window_save_remote (EvWindow  *ev_window,
		       EvSaveType save_type,
		       GFile     *src,
		       GFile     *dst)
{
	ev_window_reset_progress_cancellable (ev_window);
	g_object_set_data (G_OBJECT (dst), "ev-window", ev_window);
	g_object_set_data (G_OBJECT (dst), "save-type", GINT_TO_POINTER (save_type));
	g_file_copy_async (src, dst,
			   G_FILE_COPY_OVERWRITE,
			   G_PRIORITY_DEFAULT,
			   ev_window->priv->progress_cancellable,
			   (GFileProgressCallback)window_save_file_copy_progress_cb,
			   dst,
			   (GAsyncReadyCallback)window_save_file_copy_ready_cb,
			   dst);
	ev_window->priv->progress_idle =
		g_timeout_add_seconds_full (G_PRIORITY_DEFAULT,
					    1,
					    (GSourceFunc)show_saving_progress,
					    dst,
					    NULL);
}

static void
ev_window_clear_save_job (EvWindow *ev_window)
{
	if (ev_window->priv->save_job != NULL) {
		if (!ev_job_is_finished (ev_window->priv->save_job))
			ev_job_cancel (ev_window->priv->save_job);
		
		g_signal_handlers_disconnect_by_func (ev_window->priv->save_job,
						      ev_window_save_job_cb,
						      ev_window);
		g_object_unref (ev_window->priv->save_job);
		ev_window->priv->save_job = NULL;
	}
}

static void
ev_window_save_job_cb (EvJob     *job,
		       EvWindow  *window)
{
	if (ev_job_is_failed (job)) {
		ev_window_error_message (window, job->error,
					 _("The file could not be saved as “%s”."),
					 EV_JOB_SAVE (job)->uri);
	}

	ev_window_clear_save_job (window);
}

static void
file_save_dialog_response_cb (GtkWidget *fc,
			      gint       response_id,
			      EvWindow  *ev_window)
{
	gchar *uri;
	GFile *file, *parent;

	if (response_id != GTK_RESPONSE_OK) {
		gtk_widget_destroy (fc);
		return;
	}

	uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (fc));
	file = g_file_new_for_uri (uri);
	parent = g_file_get_parent (file);
	g_object_unref (file);
	if (parent) {
		gchar *folder_uri;

		folder_uri = g_file_get_uri (parent);
		ev_application_set_filechooser_uri (EV_APP,
						    GTK_FILE_CHOOSER_ACTION_SAVE,
						    folder_uri);
		g_free (folder_uri);
		g_object_unref (parent);
	}

	/* FIXME: remote copy should be done here rather than in the save job, 
	 * so that we can track progress and cancel the operation
	 */

	ev_window_clear_save_job (ev_window);
	ev_window->priv->save_job = ev_job_save_new (ev_window->priv->document,
						     uri, ev_window->priv->uri);
	g_signal_connect (ev_window->priv->save_job, "finished",
			  G_CALLBACK (ev_window_save_job_cb),
			  ev_window);
	/* The priority doesn't matter for this job */
	ev_job_scheduler_push_job (ev_window->priv->save_job, EV_JOB_PRIORITY_NONE);

	g_free (uri);
	gtk_widget_destroy (fc);
}

static void
ev_window_cmd_save_as (GtkAction *action, EvWindow *ev_window)
{
	GtkWidget *fc;
	gchar *base_name;
	GFile *file;
	const gchar *default_uri;

	fc = gtk_file_chooser_dialog_new (
		_("Save a Copy"),
		GTK_WINDOW (ev_window), GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_OK,
		NULL);

	ev_document_factory_add_filters (fc, ev_window->priv->document);
	gtk_dialog_set_default_response (GTK_DIALOG (fc), GTK_RESPONSE_OK);
        gtk_dialog_set_alternative_button_order (GTK_DIALOG (fc),
                                                GTK_RESPONSE_OK,
                                                GTK_RESPONSE_CANCEL,
                                                -1);

	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (fc), FALSE);
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (fc), TRUE);
	file = g_file_new_for_uri (ev_window->priv->uri);
	base_name = g_file_get_basename (file);
	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (fc), base_name);

	default_uri = ev_application_get_filechooser_uri (EV_APP, GTK_FILE_CHOOSER_ACTION_SAVE);
	if (default_uri) {
		gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (fc), default_uri);
	} else {
		const gchar *folder;

		folder = g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS);
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (fc),
						     folder ? folder : g_get_home_dir ());
	}

	g_object_unref (file);
	g_free (base_name);

	g_signal_connect (fc, "response",
			  G_CALLBACK (file_save_dialog_response_cb),
			  ev_window);

	gtk_widget_show (fc);
}

static void
ev_window_load_print_settings_from_metadata (EvWindow *window)
{
	gchar *uri = window->priv->uri;
	gint   i;
	
	/* Load print setting that are specific to the document */
	for (i = 0; i < G_N_ELEMENTS (document_print_settings); i++) {
		GValue   value = { 0, };
		gboolean success;

		success = ev_metadata_manager_get (uri, document_print_settings[i], &value, TRUE);
		gtk_print_settings_set (window->priv->print_settings,
					document_print_settings[i],
					success ? g_value_get_string (&value) : NULL);
		if (success)
			g_value_unset (&value);
	}
}

static void
ev_window_save_print_settings (EvWindow *window)
{
	gchar *uri = window->priv->uri;
	gint   i;
	
	/* Save print settings that are specific to the document */
	for (i = 0; i < G_N_ELEMENTS (document_print_settings); i++) {
		const gchar *value;

		value = gtk_print_settings_get (window->priv->print_settings,
						document_print_settings[i]);
		ev_metadata_manager_set_string (uri, document_print_settings[i], value);
	}
}

static void
ev_window_save_print_page_setup (EvWindow *window)
{
	gchar        *uri = window->priv->uri;
	GtkPageSetup *page_setup = window->priv->print_page_setup;

	/* Save page setup options that are specific to the document */
	ev_metadata_manager_set_int (uri, "page-setup-orientation",
				     gtk_page_setup_get_orientation (page_setup));
	ev_metadata_manager_set_double (uri, "page-setup-margin-top",
					gtk_page_setup_get_top_margin (page_setup, GTK_UNIT_MM));
	ev_metadata_manager_set_double (uri, "page-setup-margin-bottom",
					gtk_page_setup_get_bottom_margin (page_setup, GTK_UNIT_MM));
	ev_metadata_manager_set_double (uri, "page-setup-margin-left",
					gtk_page_setup_get_left_margin (page_setup, GTK_UNIT_MM));
	ev_metadata_manager_set_double (uri, "page-setup-margin-right",
					gtk_page_setup_get_right_margin (page_setup, GTK_UNIT_MM));
}

static void
ev_window_load_print_page_setup_from_metadata (EvWindow *window)
{
	gchar        *uri = window->priv->uri;
	GtkPageSetup *page_setup = window->priv->print_page_setup;
	GtkPaperSize *paper_size;
	GValue        value = { 0, };

	paper_size = gtk_page_setup_get_paper_size (page_setup);
	
	/* Load page setup options that are specific to the document */
	if (ev_metadata_manager_get (uri, "page-setup-orientation", &value, TRUE)) {
		gtk_page_setup_set_orientation (page_setup, g_value_get_int (&value));
		g_value_unset (&value);
	} else {
		gtk_page_setup_set_orientation (page_setup, GTK_PAGE_ORIENTATION_PORTRAIT);
	}
	
	if (ev_metadata_manager_get (uri, "page-setup-margin-top", &value, TRUE)) {
		gtk_page_setup_set_top_margin (page_setup, g_value_get_double (&value), GTK_UNIT_MM);
		g_value_unset (&value);
	} else {
		gtk_page_setup_set_top_margin (page_setup,
					       gtk_paper_size_get_default_top_margin (paper_size, GTK_UNIT_MM),
					       GTK_UNIT_MM);
	}

	if (ev_metadata_manager_get (uri, "page-setup-margin-bottom", &value, TRUE)) {
		gtk_page_setup_set_bottom_margin (page_setup, g_value_get_double (&value), GTK_UNIT_MM);
		g_value_unset (&value);
	} else {
		gtk_page_setup_set_bottom_margin (page_setup,
						  gtk_paper_size_get_default_bottom_margin (paper_size, GTK_UNIT_MM),
						  GTK_UNIT_MM);
	}

	if (ev_metadata_manager_get (uri, "page-setup-margin-left", &value, TRUE)) {
		gtk_page_setup_set_left_margin (page_setup, g_value_get_double (&value), GTK_UNIT_MM);
		g_value_unset (&value);
	} else {
		gtk_page_setup_set_left_margin (page_setup,
						gtk_paper_size_get_default_left_margin (paper_size, GTK_UNIT_MM),
						GTK_UNIT_MM);
	}	

	if (ev_metadata_manager_get (uri, "page-setup-margin-right", &value, TRUE)) {
		gtk_page_setup_set_right_margin (page_setup, g_value_get_double (&value), GTK_UNIT_MM);
		g_value_unset (&value);
	} else {
		gtk_page_setup_set_right_margin (page_setup,
						 gtk_paper_size_get_default_right_margin (paper_size, GTK_UNIT_MM),
						 GTK_UNIT_MM);
	}	
}

static void
ev_window_print_page_setup_done_cb (GtkPageSetup *page_setup,
				    EvWindow     *window)
{
	/* Dialog was canceled */
	if (!page_setup)
		return;

	if (window->priv->print_page_setup != page_setup) {
		if (window->priv->print_page_setup)
			g_object_unref (window->priv->print_page_setup);
		window->priv->print_page_setup = g_object_ref (page_setup);
	}
	
	ev_application_set_page_setup (EV_APP, page_setup);
	ev_window_save_print_page_setup (window);
}

static void
ev_window_cmd_file_print_setup (GtkAction *action, EvWindow *ev_window)
{
	if (!ev_window->priv->print_page_setup) {
		ev_window->priv->print_page_setup = gtk_page_setup_copy (
			ev_application_get_page_setup (EV_APP));
		ev_window_load_print_page_setup_from_metadata (ev_window);
	}
	
	gtk_print_run_page_setup_dialog_async (
		GTK_WINDOW (ev_window),
		ev_window->priv->print_page_setup,
		ev_window->priv->print_settings,
		(GtkPageSetupDoneFunc) ev_window_print_page_setup_done_cb,
		ev_window);
}

static void
ev_window_print_cancel (EvWindow *ev_window)
{
	EvPrintOperation *op;
	
	if (!ev_window->priv->print_queue)
		return;

	while ((op = g_queue_peek_tail (ev_window->priv->print_queue))) {
		ev_print_operation_cancel (op);
	}
}

static void
ev_window_print_update_pending_jobs_message (EvWindow *ev_window,
					     gint      n_jobs)
{
	gchar *text = NULL;
	
	if (!EV_IS_PROGRESS_MESSAGE_AREA (ev_window->priv->message_area) ||
	    !ev_window->priv->print_queue)
		return;

	if (n_jobs == 0) {
		ev_window_set_message_area (ev_window, NULL);
		return;
	}
	
	if (n_jobs > 1) {
		text = g_strdup_printf (ngettext ("%d pending job in queue",
						  "%d pending jobs in queue",
						  n_jobs - 1), n_jobs - 1);
	}

	ev_message_area_set_secondary_text (EV_MESSAGE_AREA (ev_window->priv->message_area),
					    text);
	g_free (text);
}

static gboolean
destroy_window (GtkWidget *window)
{
	gtk_widget_destroy (window);
	
	return FALSE;
}

static void
ev_window_print_operation_done (EvPrintOperation       *op,
				GtkPrintOperationResult result,
				EvWindow               *ev_window)
{
	gint n_jobs;
	
	switch (result) {
	case GTK_PRINT_OPERATION_RESULT_APPLY: {
		GtkPrintSettings *print_settings;
		
		print_settings = ev_print_operation_get_print_settings (op);
		if (ev_window->priv->print_settings != print_settings) {
			if (ev_window->priv->print_settings)
				g_object_unref (ev_window->priv->print_settings);
			ev_window->priv->print_settings = g_object_ref (print_settings);
		}
		
		ev_application_set_print_settings (EV_APP, print_settings);
		ev_window_save_print_settings (ev_window);
	}

		break;
	case GTK_PRINT_OPERATION_RESULT_ERROR: {
		GtkWidget *dialog;
		GError    *error = NULL;


		ev_print_operation_get_error (op, &error);
		
		/* The message area is already used by
		 * the printing progress, so it's better to
		 * use a popup dialog in this case
		 */
		dialog = gtk_message_dialog_new (GTK_WINDOW (ev_window),
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
						 "%s", _("Failed to print document"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  "%s", error->message);
		g_signal_connect (dialog, "response",
				  G_CALLBACK (gtk_widget_destroy),
				  NULL);
		gtk_widget_show (dialog);
		
		g_error_free (error);
	}
		break;
	case GTK_PRINT_OPERATION_RESULT_CANCEL:
	default:
		break;
	}

	g_queue_remove (ev_window->priv->print_queue, op);
	g_object_unref (op);
	n_jobs = g_queue_get_length (ev_window->priv->print_queue);
	ev_window_print_update_pending_jobs_message (ev_window, n_jobs);

	if (n_jobs == 0 && ev_window->priv->close_after_print)
		g_idle_add ((GSourceFunc)destroy_window,
			    ev_window);
}

static void
ev_window_print_progress_response_cb (EvProgressMessageArea *area,
				      gint                   response,
				      EvWindow              *ev_window)
{
	if (response == GTK_RESPONSE_CANCEL) {
		EvPrintOperation *op;

		op = g_queue_peek_tail (ev_window->priv->print_queue);
		ev_print_operation_cancel (op);
	} else {
		gtk_widget_hide (GTK_WIDGET (area));
	}
}

static void
ev_window_print_operation_status_changed (EvPrintOperation *op,
					  EvWindow         *ev_window)
{
	const gchar *status;
	gdouble      fraction;

	status = ev_print_operation_get_status (op);
	fraction = ev_print_operation_get_progress (op);
	
	if (!ev_window->priv->message_area) {
		GtkWidget   *area;
		const gchar *job_name;
		gchar       *text;

		job_name = ev_print_operation_get_job_name (op);
		text = g_strdup_printf (_("Printing job “%s”"), job_name);

		area = ev_progress_message_area_new (GTK_STOCK_PRINT,
						     text,
						     GTK_STOCK_CLOSE,
						     GTK_RESPONSE_CLOSE,
						     GTK_STOCK_CANCEL,
						     GTK_RESPONSE_CANCEL,
						     NULL);
		ev_window_print_update_pending_jobs_message (ev_window, 1);
		g_signal_connect (area, "response",
				  G_CALLBACK (ev_window_print_progress_response_cb),
				  ev_window);
		gtk_widget_show (area);
		ev_window_set_message_area (ev_window, area);
		g_free (text);
	}

	ev_progress_message_area_set_status (EV_PROGRESS_MESSAGE_AREA (ev_window->priv->message_area),
					     status);
	ev_progress_message_area_set_fraction (EV_PROGRESS_MESSAGE_AREA (ev_window->priv->message_area),
					       fraction);
}

static void
ev_window_print_operation_begin_print (EvPrintOperation *op,
				       EvWindow         *ev_window)
{
	GtkPrintSettings *print_settings;
	
	if (!ev_window->priv->print_queue)
		ev_window->priv->print_queue = g_queue_new ();

	g_queue_push_head (ev_window->priv->print_queue, op);
	ev_window_print_update_pending_jobs_message (ev_window,
						     g_queue_get_length (ev_window->priv->print_queue));
	
	if (ev_window->priv->print_settings)
		g_object_unref (ev_window->priv->print_settings);
	print_settings = ev_print_operation_get_print_settings (op);
	ev_window->priv->print_settings = g_object_ref (print_settings);
}

void
ev_window_print_range (EvWindow *ev_window,
		       gint      first_page,
		       gint      last_page)
{
	EvPrintOperation *op;
	EvPageCache      *page_cache;
	gint              current_page;
	gint              document_last_page;

	g_return_if_fail (EV_IS_WINDOW (ev_window));
	g_return_if_fail (ev_window->priv->document != NULL);

	if (!ev_window->priv->print_queue)
		ev_window->priv->print_queue = g_queue_new ();

	op = ev_print_operation_new (ev_window->priv->document);
	if (!op) {
		g_warning ("%s", "Printing is not supported for document\n");
		return;
	}

	g_signal_connect (op, "begin_print",
			  G_CALLBACK (ev_window_print_operation_begin_print),
			  (gpointer)ev_window);
	g_signal_connect (op, "status_changed",
			  G_CALLBACK (ev_window_print_operation_status_changed),
			  (gpointer)ev_window);
	g_signal_connect (op, "done",
			  G_CALLBACK (ev_window_print_operation_done),
			  (gpointer)ev_window);

	page_cache = ev_page_cache_get (ev_window->priv->document);
	current_page = ev_page_cache_get_current_page (page_cache);
	document_last_page = ev_page_cache_get_n_pages (page_cache);

	if (!ev_window->priv->print_settings) {
		ev_window->priv->print_settings = gtk_print_settings_copy (
			ev_application_get_print_settings (EV_APP));
		ev_window_load_print_settings_from_metadata (ev_window);
	}

	if (!ev_window->priv->print_page_setup) {
		ev_window->priv->print_page_setup = gtk_page_setup_copy (
			ev_application_get_page_setup (EV_APP));
		ev_window_load_print_page_setup_from_metadata (ev_window);
	}

	if (first_page != 1 || last_page != document_last_page) {
		GtkPageRange range;

		/* Ranges in GtkPrint are 0 - N */
		range.start = first_page - 1;
		range.end = last_page - 1;
		
		gtk_print_settings_set_print_pages (ev_window->priv->print_settings,
						    GTK_PRINT_PAGES_RANGES);
		gtk_print_settings_set_page_ranges (ev_window->priv->print_settings,
						    &range, 1);
	}

	ev_print_operation_set_job_name (op, gtk_window_get_title (GTK_WINDOW (ev_window)));
	ev_print_operation_set_current_page (op, current_page);
	ev_print_operation_set_print_settings (op, ev_window->priv->print_settings);
	ev_print_operation_set_default_page_setup (op, ev_window->priv->print_page_setup);

	ev_print_operation_run (op, GTK_WINDOW (ev_window));
}

static void
ev_window_print (EvWindow *window)
{
	EvPageCache *page_cache;
	gint         last_page;

	page_cache = ev_page_cache_get (window->priv->document);
	last_page = ev_page_cache_get_n_pages (page_cache);

	ev_window_print_range (window, 1, last_page);
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
						   ev_window->priv->uri,
					           ev_window->priv->document);
		g_object_add_weak_pointer (G_OBJECT (ev_window->priv->properties),
					   (gpointer) &(ev_window->priv->properties));
		gtk_window_set_transient_for (GTK_WINDOW (ev_window->priv->properties),
					      GTK_WINDOW (ev_window));
	}

	ev_document_fc_mutex_lock ();
	gtk_widget_show (ev_window->priv->properties);
	ev_document_fc_mutex_unlock ();
}

static void
print_jobs_confirmation_dialog_response (GtkDialog *dialog,
					 gint       response,
					 EvWindow  *ev_window)
{
	gtk_widget_destroy (GTK_WIDGET (dialog));	
	
	switch (response) {
	case GTK_RESPONSE_YES:
		if (!ev_window->priv->print_queue ||
		    g_queue_is_empty (ev_window->priv->print_queue))
			gtk_widget_destroy (GTK_WIDGET (ev_window));
		else
			ev_window->priv->close_after_print = TRUE;
		break;
	case GTK_RESPONSE_NO:
		ev_window->priv->close_after_print = TRUE;
		if (ev_window->priv->print_queue &&
		    !g_queue_is_empty (ev_window->priv->print_queue)) {
			gtk_widget_set_sensitive (GTK_WIDGET (ev_window), FALSE);
			ev_window_print_cancel (ev_window);
		} else {
			gtk_widget_destroy (GTK_WIDGET (ev_window));
		}
		break;
	case GTK_RESPONSE_CANCEL:
	default:
		ev_window->priv->close_after_print = FALSE;
	}
}

static void
ev_window_cmd_file_close_window (GtkAction *action, EvWindow *ev_window)
{
	GtkWidget *dialog;
	gchar     *text, *markup;
	gint       n_print_jobs;

	n_print_jobs = ev_window->priv->print_queue ?
		g_queue_get_length (ev_window->priv->print_queue) : 0;
	
	if (n_print_jobs == 0) {
		gtk_widget_destroy (GTK_WIDGET (ev_window));
		return;
	}

	dialog = gtk_message_dialog_new (GTK_WINDOW (ev_window),
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_NONE,
					 NULL);
	if (n_print_jobs == 1) {
		EvPrintOperation *op;
		const gchar      *job_name;

		op = g_queue_peek_tail (ev_window->priv->print_queue);
		job_name = ev_print_operation_get_job_name (op);

		text = g_strdup_printf (_("Wait until print job “%s” finishes before closing?"),
					job_name);
	} else {
		text = g_strdup_printf (_("There are %d print jobs active. "
					  "Wait until print finishes before closing?"),
					n_print_jobs);
	}

	markup = g_strdup_printf ("<b>%s</b>", text);
	g_free (text);

	gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), markup);
	g_free (markup);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s",
						  _("If you close the window, pending print "
						    "jobs will not be printed."));
	
	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				_("Cancel _print and Close"),
				GTK_RESPONSE_NO,
				GTK_STOCK_CANCEL,
				GTK_RESPONSE_CANCEL,
				_("Close _after Printing"),
				GTK_RESPONSE_YES,
				NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_YES);
        gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                                 GTK_RESPONSE_YES,
                                                 GTK_RESPONSE_NO,
                                                 GTK_RESPONSE_CANCEL,
                                                 -1);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (print_jobs_confirmation_dialog_response),
			  ev_window);
	gtk_widget_show (dialog);
}

static void
ev_window_cmd_focus_page_selector (GtkAction *act, EvWindow *window)
{
	GtkAction *action;
	
	update_chrome_flag (window, EV_CHROME_RAISE_TOOLBAR, TRUE);
	ev_window_set_action_sensitive (window, "ViewToolbar", FALSE);
	update_chrome_visibility (window);
	
	action = gtk_action_group_get_action (window->priv->action_group,
				     	      PAGE_SELECTOR_ACTION);
	ev_page_action_grab_focus (EV_PAGE_ACTION (action));
}

static void
ev_window_cmd_scroll_forward (GtkAction *action, EvWindow *window)
{
	ev_view_scroll (EV_VIEW (window->priv->view), GTK_SCROLL_PAGE_FORWARD, FALSE);
}

static void
ev_window_cmd_scroll_backward (GtkAction *action, EvWindow *window)
{
	ev_view_scroll (EV_VIEW (window->priv->view), GTK_SCROLL_PAGE_BACKWARD, FALSE);
}

static void
ev_window_cmd_continuous (GtkAction *action, EvWindow *ev_window)
{
	gboolean continuous;

	ev_window_stop_presentation (ev_window, TRUE);
	continuous = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	g_object_set (G_OBJECT (ev_window->priv->view),
		      "continuous", continuous,
		      NULL);
	ev_window_update_actions (ev_window);
}

static void
ev_window_cmd_dual (GtkAction *action, EvWindow *ev_window)
{
	gboolean dual_page;

	ev_window_stop_presentation (ev_window, TRUE);
	dual_page = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	g_object_set (G_OBJECT (ev_window->priv->view),
		      "dual-page", dual_page,
		      NULL);
	ev_window_update_actions (ev_window);
}

static void
ev_window_cmd_view_best_fit (GtkAction *action, EvWindow *ev_window)
{
	ev_window_stop_presentation (ev_window, TRUE);

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
		ev_view_set_sizing_mode (EV_VIEW (ev_window->priv->view), EV_SIZING_BEST_FIT);
	} else {
		ev_view_set_sizing_mode (EV_VIEW (ev_window->priv->view), EV_SIZING_FREE);
	}
	ev_window_update_actions (ev_window);
}

static void
ev_window_cmd_view_page_width (GtkAction *action, EvWindow *ev_window)
{
	ev_window_stop_presentation (ev_window, TRUE);

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
		ev_view_set_sizing_mode (EV_VIEW (ev_window->priv->view), EV_SIZING_FIT_WIDTH);
	} else {
		ev_view_set_sizing_mode (EV_VIEW (ev_window->priv->view), EV_SIZING_FREE);
	}
	ev_window_update_actions (ev_window);
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

	if (ev_window->priv->document == NULL || !EV_IS_DOCUMENT_FIND (ev_window->priv->document)) {
		g_error ("Find action should be insensitive since document doesn't support find");
		return;
	} 

	update_chrome_flag (ev_window, EV_CHROME_FINDBAR, TRUE);
	update_chrome_visibility (ev_window);
	gtk_widget_grab_focus (ev_window->priv->find_bar);
}

static void
ev_window_cmd_edit_find_next (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

	update_chrome_flag (ev_window, EV_CHROME_FINDBAR, TRUE);
	update_chrome_visibility (ev_window);
	gtk_widget_grab_focus (ev_window->priv->find_bar);
	ev_view_find_next (EV_VIEW (ev_window->priv->view));
}

static void
ev_window_cmd_edit_find_previous (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

	update_chrome_flag (ev_window, EV_CHROME_FINDBAR, TRUE);
	update_chrome_visibility (ev_window);
	gtk_widget_grab_focus (ev_window->priv->find_bar);
	ev_view_find_previous (EV_VIEW (ev_window->priv->view));
}

static void
ev_window_cmd_edit_copy (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

	ev_view_copy (EV_VIEW (ev_window->priv->view));
}

static void
ev_window_sidebar_position_change_cb (GObject *object, GParamSpec *pspec,
				      EvWindow *ev_window)
{
	if (!ev_window_is_empty (ev_window))
		ev_metadata_manager_set_int (ev_window->priv->uri, "sidebar_size",
					     gtk_paned_get_position (GTK_PANED (object)));
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
fullscreen_toolbar_setup_item_properties (GtkUIManager *ui_manager)
{
	GtkWidget *item;

	item = gtk_ui_manager_get_widget (ui_manager, "/FullscreenToolbar/GoPreviousPage");
	g_object_set (item, "is-important", FALSE, NULL);

	item = gtk_ui_manager_get_widget (ui_manager, "/FullscreenToolbar/GoNextPage");
	g_object_set (item, "is-important", FALSE, NULL);

	item = gtk_ui_manager_get_widget (ui_manager, "/FullscreenToolbar/StartPresentation");
	g_object_set (item, "is-important", TRUE, NULL);
	
	item = gtk_ui_manager_get_widget (ui_manager, "/FullscreenToolbar/LeaveFullscreen");
	g_object_set (item, "is-important", TRUE, NULL);
}

static void
fullscreen_toolbar_remove_shadow (GtkWidget *toolbar)
{
	static gboolean done = FALSE;

	if (!done) {
		gtk_rc_parse_string (
			"\n"
			"   style \"fullscreen-toolbar-style\"\n"
			"   {\n"
			"      GtkToolbar::shadow-type=GTK_SHADOW_NONE\n"
			"   }\n"
			"\n"
			"    widget \"*.fullscreen-toolbar\" style \"fullscreen-toolbar-style\"\n"
			"\n");
		done = TRUE;
	}
	
	gtk_widget_set_name (toolbar, "fullscreen-toolbar");
}

static void
ev_window_run_fullscreen (EvWindow *window)
{
	EvView  *view = EV_VIEW (window->priv->view);
	gboolean fullscreen_window = TRUE;

	if (ev_view_get_fullscreen (view))
		return;
	
	if (!window->priv->fullscreen_toolbar) {
		window->priv->fullscreen_toolbar =
			gtk_ui_manager_get_widget (window->priv->ui_manager,
						   "/FullscreenToolbar");

		gtk_toolbar_set_style (GTK_TOOLBAR (window->priv->fullscreen_toolbar),
				       GTK_TOOLBAR_BOTH_HORIZ);
		fullscreen_toolbar_remove_shadow (window->priv->fullscreen_toolbar);
		fullscreen_toolbar_setup_item_properties (window->priv->ui_manager);

		gtk_box_pack_start (GTK_BOX (window->priv->main_box),
				    window->priv->fullscreen_toolbar,
				    FALSE, FALSE, 0);
		gtk_box_reorder_child (GTK_BOX (window->priv->main_box),
				       window->priv->fullscreen_toolbar, 1);
	}

	if (ev_view_get_presentation (view)) {
		ev_window_stop_presentation (window, FALSE);
		fullscreen_window = FALSE;
	}

	g_object_set (G_OBJECT (window->priv->scrolled_window),
		      "shadow-type", GTK_SHADOW_NONE,
		      NULL);

	ev_view_set_fullscreen (view, TRUE);
	ev_window_update_fullscreen_action (window);

	/* If the user doesn't have the main toolbar he/she won't probably want
	 * the toolbar in fullscreen mode. See bug #483048
	 */
	update_chrome_flag (window, EV_CHROME_FULLSCREEN_TOOLBAR,
			    (window->priv->chrome & EV_CHROME_TOOLBAR) != 0);
	update_chrome_visibility (window);

	if (fullscreen_window)
		gtk_window_fullscreen (GTK_WINDOW (window));
	gtk_widget_grab_focus (window->priv->view);

	if (!ev_window_is_empty (window))
		ev_metadata_manager_set_boolean (window->priv->uri, "fullscreen", TRUE);
}

static void
ev_window_stop_fullscreen (EvWindow *window,
			   gboolean  unfullscreen_window)
{
	EvView *view = EV_VIEW (window->priv->view);

	if (!ev_view_get_fullscreen (view))
		return;

	g_object_set (G_OBJECT (window->priv->scrolled_window),
		      "shadow-type", GTK_SHADOW_IN,
		      NULL);

	ev_view_set_fullscreen (view, FALSE);
	ev_window_update_fullscreen_action (window);
	update_chrome_flag (window, EV_CHROME_FULLSCREEN_TOOLBAR, FALSE);
	update_chrome_visibility (window);
	if (unfullscreen_window)
		gtk_window_unfullscreen (GTK_WINDOW (window));

	if (!ev_window_is_empty (window))
		ev_metadata_manager_set_boolean (window->priv->uri, "fullscreen", FALSE);
}

static void
ev_window_cmd_view_fullscreen (GtkAction *action, EvWindow *window)
{
	gboolean fullscreen;

	fullscreen = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	if (fullscreen) {
		ev_window_run_fullscreen (window);
	} else {
		ev_window_stop_fullscreen (window, TRUE);
	}
}

static gboolean
presentation_timeout_cb (EvWindow *window)
{
	EvView *view = EV_VIEW (window->priv->view);

	if (!view || !ev_view_get_presentation (EV_VIEW (view)))
		return FALSE;

	ev_view_hide_cursor (EV_VIEW (window->priv->view));
	window->priv->presentation_timeout_id = 0;

	return FALSE;
}

static void
presentation_set_timeout (EvWindow *window)
{
	if (window->priv->presentation_timeout_id > 0) {
		g_source_remove (window->priv->presentation_timeout_id);
	}

	window->priv->presentation_timeout_id =
		g_timeout_add_seconds (PRESENTATION_TIMEOUT,
				       (GSourceFunc)presentation_timeout_cb, window);

	ev_view_show_cursor (EV_VIEW (window->priv->view));
}

static void
presentation_clear_timeout (EvWindow *window)
{
	if (window->priv->presentation_timeout_id > 0) {
		g_source_remove (window->priv->presentation_timeout_id);
	}
	
	window->priv->presentation_timeout_id = 0;

	ev_view_show_cursor (EV_VIEW (window->priv->view));
}

static gboolean
presentation_motion_notify_cb (GtkWidget *widget,
			       GdkEventMotion *event,
			       gpointer user_data)
{
	EvWindow *window = EV_WINDOW (user_data);

	presentation_set_timeout (window);

	return FALSE;
}

static gboolean
presentation_leave_notify_cb (GtkWidget *widget,
			      GdkEventCrossing *event,
			      gpointer user_data)
{
	EvWindow *window = EV_WINDOW (user_data);

	presentation_clear_timeout (window);

	return FALSE;
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
	EvView  *view = EV_VIEW (window->priv->view);
	gboolean fullscreen_window = TRUE;

	if (ev_view_get_presentation (view))
		return;

	if (ev_view_get_fullscreen (view)) {
		ev_window_stop_fullscreen (window, FALSE);
		fullscreen_window = FALSE;
	}
	
	g_object_set (G_OBJECT (window->priv->scrolled_window),
		      "shadow-type", GTK_SHADOW_NONE,
		      NULL);

	ev_view_set_presentation (view, TRUE);
	ev_window_update_presentation_action (window);

	update_chrome_visibility (window);
	
	gtk_widget_grab_focus (window->priv->view);
	if (fullscreen_window)
		gtk_window_fullscreen (GTK_WINDOW (window));

	g_signal_connect (window->priv->view,
			  "motion-notify-event",
			  G_CALLBACK (presentation_motion_notify_cb),
			  window);
	g_signal_connect (window->priv->view,
			  "leave-notify-event",
			  G_CALLBACK (presentation_leave_notify_cb),
			  window);
	presentation_set_timeout (window);

	ev_application_screensaver_disable (EV_APP);
	
	if (!ev_window_is_empty (window))
		ev_metadata_manager_set_boolean (window->priv->uri, "presentation", TRUE);
}

static void
ev_window_stop_presentation (EvWindow *window,
			     gboolean  unfullscreen_window)
{
	EvView *view = EV_VIEW (window->priv->view);
	
	if (!ev_view_get_presentation (view))
		return;

	g_object_set (G_OBJECT (window->priv->scrolled_window),
		      "shadow-type", GTK_SHADOW_IN,
		      NULL);

	ev_view_set_presentation (EV_VIEW (window->priv->view), FALSE);
	ev_window_update_presentation_action (window);
	update_chrome_visibility (window);
	if (unfullscreen_window)
		gtk_window_unfullscreen (GTK_WINDOW (window));

	g_signal_handlers_disconnect_by_func (window->priv->view,
					      (gpointer) presentation_motion_notify_cb,
					      window);
	g_signal_handlers_disconnect_by_func (window->priv->view,
					      (gpointer) presentation_leave_notify_cb,
					      window);
	presentation_clear_timeout (window);

	ev_application_screensaver_enable (EV_APP);

	if (!ev_window_is_empty (window))
		ev_metadata_manager_set_boolean (window->priv->uri, "presentation", FALSE);
}

static void
ev_window_cmd_view_presentation (GtkAction *action, EvWindow *window)
{
	gboolean presentation;

	presentation = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	if (presentation) {
		ev_window_run_presentation (window);
	}
}

static void
ev_window_setup_gtk_settings (EvWindow *window)
{
	GtkSettings *settings;
	GdkScreen   *screen;
	gchar       *menubar_accel_accel;

	screen = gtk_window_get_screen (GTK_WINDOW (window));
	settings = gtk_settings_get_for_screen (screen);

	g_object_get (settings,
		      "gtk-menu-bar-accel", &menubar_accel_accel,
		      NULL);
	if (menubar_accel_accel != NULL && menubar_accel_accel[0] != '\0') {
		gtk_accelerator_parse (menubar_accel_accel,
				       &window->priv->menubar_accel_keyval,
				       &window->priv->menubar_accel_modifier);
		if (window->priv->menubar_accel_keyval == 0) {
			g_warning ("Failed to parse menu bar accelerator '%s'\n",
				   menubar_accel_accel);
		}
	} else {
		window->priv->menubar_accel_keyval = 0;
		window->priv->menubar_accel_modifier = 0;
	}

	g_free (menubar_accel_accel);
}

static void
ev_window_screen_changed (GtkWidget *widget,
			  GdkScreen *old_screen)
{
	EvWindow *window = EV_WINDOW (widget);
	EvWindowPrivate *priv = window->priv;
	GdkScreen *screen;

	screen = gtk_widget_get_screen (widget);
	if (screen == old_screen)
		return;

	ev_window_setup_gtk_settings (window);
	ev_view_set_screen_dpi (EV_VIEW (priv->view),
				get_screen_dpi (GTK_WINDOW (window)));
	
	if (GTK_WIDGET_CLASS (ev_window_parent_class)->screen_changed) {
		GTK_WIDGET_CLASS (ev_window_parent_class)->screen_changed (widget, old_screen);
	}
}

static gboolean
ev_window_state_event (GtkWidget           *widget,
		       GdkEventWindowState *event)
{
	EvWindow *window = EV_WINDOW (widget);
	EvView   *view = EV_VIEW (window->priv->view);

	if (GTK_WIDGET_CLASS (ev_window_parent_class)->window_state_event) {
		GTK_WIDGET_CLASS (ev_window_parent_class)->window_state_event (widget, event);
	}

	if ((event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) == 0)
		return FALSE;

	if (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) {
		if (ev_view_get_fullscreen (view) || ev_view_get_presentation (view))
			return FALSE;
		
		ev_window_run_fullscreen (window);
	} else {
		if (ev_view_get_fullscreen (view))
			ev_window_stop_fullscreen (window, FALSE);
		else if (ev_view_get_presentation (view))
			ev_window_stop_presentation (window, FALSE);
	}

	return FALSE;
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
	ev_window_update_actions (window);
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
ev_window_cmd_edit_toolbar_cb (GtkDialog *dialog, gint response, gpointer data)
{
	EvWindow *ev_window = EV_WINDOW (data);
        egg_editable_toolbar_set_edit_mode
			(EGG_EDITABLE_TOOLBAR (ev_window->priv->toolbar), FALSE);
	ev_application_save_toolbars_model (EV_APP);
        gtk_widget_destroy (GTK_WIDGET (dialog));
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
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 500, 400);
	  
	editor = egg_toolbar_editor_new (ev_window->priv->ui_manager,
					 ev_application_get_toolbars_model (EV_APP));
	gtk_container_set_border_width (GTK_CONTAINER (editor), 5);
	gtk_box_set_spacing (GTK_BOX (EGG_TOOLBAR_EDITOR (editor)), 5);
             
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), editor);

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

	ev_view_previous_page (EV_VIEW (ev_window->priv->view));
}

static void
ev_window_cmd_go_next_page (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

	ev_view_next_page (EV_VIEW (ev_window->priv->view));
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
	ev_window_reload_document (ev_window, NULL);
}

static void
ev_window_cmd_view_autoscroll (GtkAction *action, EvWindow *ev_window)
{
	ev_view_autoscroll_start (EV_VIEW (ev_window->priv->view));
}

static void
ev_window_cmd_help_contents (GtkAction *action, EvWindow *ev_window)
{
	GError  *error = NULL;

	gtk_show_uri (gtk_window_get_screen (GTK_WINDOW (ev_window)),
		      "ghelp:evince",
		      GDK_CURRENT_TIME,
		      &error);
	if (error) {
		ev_window_error_message (ev_window, error, 
					 "%s", _("There was an error displaying help"));
		g_error_free (error);
	}
}

static void
ev_window_cmd_leave_fullscreen (GtkAction *action, EvWindow *window)
{
	ev_window_stop_fullscreen (window, TRUE);
}

static void
ev_window_cmd_start_presentation (GtkAction *action, EvWindow *window)
{
	ev_window_run_presentation (window);
}

static void
ev_window_cmd_escape (GtkAction *action, EvWindow *window)
{
	GtkWidget *widget;

	ev_view_autoscroll_stop (EV_VIEW (window->priv->view));
	
	widget = gtk_window_get_focus (GTK_WINDOW (window));
	if (widget && gtk_widget_get_ancestor (widget, EGG_TYPE_FIND_BAR)) {
		update_chrome_flag (window, EV_CHROME_FINDBAR, FALSE);
		update_chrome_visibility (window);
		gtk_widget_grab_focus (window->priv->view);
	} else {
		gboolean fullscreen;
		gboolean presentation;

		g_object_get (window->priv->view,
			      "fullscreen", &fullscreen,
			      "presentation", &presentation,
			      NULL);

		if (fullscreen) {
			ev_window_stop_fullscreen (window, TRUE);
		} else if (presentation) {
			ev_window_stop_presentation (window, TRUE);
			gtk_widget_grab_focus (window->priv->view);
		} else {
			gtk_widget_grab_focus (window->priv->view);
		}

		if (fullscreen && presentation)
			g_warning ("Both fullscreen and presentation set somehow");
	}
}

static void
save_sizing_mode (EvWindow *window)
{
	EvSizingMode mode;
	GEnumValue *enum_value;

	mode = ev_view_get_sizing_mode (EV_VIEW (window->priv->view));
	enum_value = g_enum_get_value (g_type_class_peek (EV_TYPE_SIZING_MODE), mode);

	if (!ev_window_is_empty (window))
		ev_metadata_manager_set_string (window->priv->uri, "sizing_mode",
						enum_value->value_nick);
}

static void
ev_window_set_view_size (EvWindow *window)
{
	gint width, height;
	GtkRequisition vsb_requisition;
	GtkRequisition hsb_requisition;
	gint scrollbar_spacing;
	GtkWidget *scrolled_window = window->priv->scrolled_window;

	if (!window->priv->view)
		return;

	/* Calculate the width available for the content */
	width  = scrolled_window->allocation.width;
	height = scrolled_window->allocation.height;

	if (gtk_scrolled_window_get_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window)) == GTK_SHADOW_IN) {
		width -=  2 * window->priv->view->style->xthickness;
		height -= 2 * window->priv->view->style->ythickness;
	}

	gtk_widget_size_request (GTK_SCROLLED_WINDOW (scrolled_window)->vscrollbar,
				 &vsb_requisition);
	gtk_widget_size_request (GTK_SCROLLED_WINDOW (scrolled_window)->hscrollbar,
				 &hsb_requisition);
	gtk_widget_style_get (scrolled_window,
			      "scrollbar_spacing",
			      &scrollbar_spacing,
			      NULL);

	ev_view_set_zoom_for_size (EV_VIEW (window->priv->view),
				   MAX (1, width),
				   MAX (1, height),
				   vsb_requisition.width + scrollbar_spacing,
				   hsb_requisition.height + scrollbar_spacing);
}

static void     
ev_window_sizing_mode_changed_cb (EvView *view, GParamSpec *pspec,
		 		  EvWindow   *ev_window)
{
	EvSizingMode sizing_mode;

	g_object_get (ev_window->priv->view,
		      "sizing-mode", &sizing_mode,
		      NULL);

	g_object_set (ev_window->priv->scrolled_window,
		      "hscrollbar-policy",
		      sizing_mode == EV_SIZING_FREE ?
		      GTK_POLICY_AUTOMATIC : GTK_POLICY_NEVER,
		      "vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		      NULL);

	update_sizing_buttons (ev_window);
	save_sizing_mode (ev_window);
}

static void     
ev_window_zoom_changed_cb (EvView *view, GParamSpec *pspec, EvWindow *ev_window)
{
        ev_window_update_actions (ev_window);

	if (ev_view_get_sizing_mode (view) == EV_SIZING_FREE && !ev_window_is_empty (ev_window)) {
		gdouble zoom;

		zoom = ev_view_get_zoom (view);
		zoom *= 72.0 / get_screen_dpi (GTK_WINDOW(ev_window));
		ev_metadata_manager_set_double (ev_window->priv->uri, "zoom", zoom);
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

	if (!ev_window_is_empty (ev_window))
		ev_metadata_manager_set_boolean (ev_window->priv->uri, "continuous",
					         ev_view_get_continuous (EV_VIEW (ev_window->priv->view)));
}

static void     
ev_window_rotation_changed_cb (EvView *view, GParamSpec *pspec, EvWindow *window)
{
	int rotation;

	rotation = ev_view_get_rotation (EV_VIEW (window->priv->view));

	if (!ev_window_is_empty (window))
		ev_metadata_manager_set_int (window->priv->uri, "rotation",
					     rotation);

	ev_sidebar_thumbnails_refresh (EV_SIDEBAR_THUMBNAILS (window->priv->sidebar_thumbs),
				       rotation);
	ev_window_refresh_window_thumbnail (window, rotation);
}

static void
ev_window_has_selection_changed_cb (EvView *view, GParamSpec *pspec, EvWindow *window)
{
        ev_window_update_actions (window);
}

static void     
ev_window_dual_mode_changed_cb (EvView *view, GParamSpec *pspec, EvWindow *ev_window)
{
	ev_window_update_dual_page_action (ev_window);

	if (!ev_window_is_empty (ev_window))
		ev_metadata_manager_set_boolean (ev_window->priv->uri, "dual-page",
					         ev_view_get_dual_page (EV_VIEW (ev_window->priv->view)));
}

static char *
build_comments_string (void)
{
#ifdef ENABLE_PDF
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

	return g_strdup_printf (_("Document Viewer.\n"
				  "Using poppler %s (%s)"),
				version, backend_name);
#else
	return g_strdup_printf (_("Document Viewer"));
#endif
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
		"Carlos Garcia Campos <carlosgc@gnome.org>",
		"Wouter Bolsterlee <wbolster@gnome.org>",
                "Christian Persch <chpe" "\100" "gnome.org>",
		NULL
	};

	const char *documenters[] = {
		"Nickolay V. Shmyrev <nshmyrev@yandex.ru>",
		NULL
	};

	const char *license[] = {
		N_("Evince is free software; you can redistribute it and/or modify "
		   "it under the terms of the GNU General Public License as published by "
		   "the Free Software Foundation; either version 2 of the License, or "
		   "(at your option) any later version.\n"),
		N_("Evince is distributed in the hope that it will be useful, "
		   "but WITHOUT ANY WARRANTY; without even the implied warranty of "
		   "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
		   "GNU General Public License for more details.\n"),
		N_("You should have received a copy of the GNU General Public License "
		   "along with Evince; if not, write to the Free Software Foundation, Inc., "
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
		_("© 1996–2009 The Evince authors"),
		"license", license_trans,
		"website", "http://www.gnome.org/projects/evince",
		"comments", comments,
		"authors", authors,
		"documenters", documenters,
		"translator-credits", _("translator-credits"),
		"logo-icon-name", "evince",
		"wrap-license", TRUE,
		NULL);

	g_free (comments);
	g_free (license_trans);
}

static void
ev_window_view_toolbar_cb (GtkAction *action, EvWindow *ev_window)
{
	gboolean active;
	
	active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	update_chrome_flag (ev_window, EV_CHROME_TOOLBAR, active);
	update_chrome_visibility (ev_window);
	ev_metadata_manager_set_boolean (NULL, "show_toolbar", active);
}

static void
ev_window_view_sidebar_cb (GtkAction *action, EvWindow *ev_window)
{
	if (ev_view_get_presentation (EV_VIEW (ev_window->priv->view)))
		return;
	    
	update_chrome_flag (ev_window, EV_CHROME_SIDEBAR,
			    gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
	update_chrome_visibility (ev_window);
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
	} else if (current_page == ev_window->priv->sidebar_attachments) {
		id = ATTACHMENTS_SIDEBAR_ID;
	} else if (current_page == ev_window->priv->sidebar_layers) {
		id = LAYERS_SIDEBAR_ID;
	} else {
		g_assert_not_reached();
	}

	g_object_unref (current_page);

	if (!ev_window_is_empty (ev_window))
		ev_metadata_manager_set_string (ev_window->priv->uri, "sidebar_page", id);
}

static void
ev_window_sidebar_visibility_changed_cb (EvSidebar  *ev_sidebar,
					 GParamSpec *pspec,
					 EvWindow   *ev_window)
{
	EvView *view = EV_VIEW (ev_window->priv->view);
	GtkAction *action;

	action = gtk_action_group_get_action (ev_window->priv->action_group, "ViewSidebar");

	if (!ev_view_get_presentation (view)) {
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
					      GTK_WIDGET_VISIBLE (ev_sidebar));

		ev_metadata_manager_set_boolean (ev_window->priv->uri, "sidebar_visibility",
					         GTK_WIDGET_VISIBLE (ev_sidebar));
	}
}

static void
view_menu_link_popup (EvWindow *ev_window,
		      EvLink   *link)
{
	gboolean   show_external = FALSE;
	gboolean   show_internal = FALSE;
	GtkAction *action;
	
	if (ev_window->priv->link)
		g_object_unref (ev_window->priv->link);
	
	if (link)
		ev_window->priv->link = g_object_ref (link);
	else	
		ev_window->priv->link = NULL;

	if (ev_window->priv->link) {
		EvLinkAction *ev_action;

		ev_action = ev_link_get_action (link);
		if (ev_action) {
			switch (ev_link_action_get_action_type (ev_action)) {
		                case EV_LINK_ACTION_TYPE_GOTO_DEST:
		                case EV_LINK_ACTION_TYPE_GOTO_REMOTE:
					show_internal = TRUE;
					break;
		                case EV_LINK_ACTION_TYPE_EXTERNAL_URI:
		                case EV_LINK_ACTION_TYPE_LAUNCH:
					show_external = TRUE;
					break;
		                default:
					break;
			}
		}
	}
	
	action = gtk_action_group_get_action (ev_window->priv->view_popup_action_group,
					      "OpenLink");
	gtk_action_set_visible (action, show_external);

	action = gtk_action_group_get_action (ev_window->priv->view_popup_action_group,
					      "CopyLinkAddress");
	gtk_action_set_visible (action, show_external);

	action = gtk_action_group_get_action (ev_window->priv->view_popup_action_group,
					      "GoLink");
	gtk_action_set_visible (action, show_internal);

	action = gtk_action_group_get_action (ev_window->priv->view_popup_action_group,
					      "OpenLinkNewWindow");
	gtk_action_set_visible (action, show_internal);
}

static void
view_menu_image_popup (EvWindow  *ev_window,
		       EvImage   *image)
{
	GtkAction *action;
	gboolean   show_image = FALSE;
	
	if (ev_window->priv->image)
		g_object_unref (ev_window->priv->image);
	
	if (image)
		ev_window->priv->image = g_object_ref (image);
	else	
		ev_window->priv->image = NULL;

	show_image = (ev_window->priv->image != NULL);
	
	action = gtk_action_group_get_action (ev_window->priv->view_popup_action_group,
					      "SaveImageAs");
	gtk_action_set_visible (action, show_image);

	action = gtk_action_group_get_action (ev_window->priv->view_popup_action_group,
					      "CopyImage");
	gtk_action_set_visible (action, show_image);
}

static gboolean
view_menu_popup_cb (EvView   *view,
		    GObject  *object,
		    EvWindow *ev_window)
{
	if (ev_view_get_presentation (EV_VIEW (ev_window->priv->view)))
		return FALSE;

	view_menu_link_popup (ev_window,
			      EV_IS_LINK (object) ? EV_LINK (object) : NULL);
	view_menu_image_popup (ev_window,
			       EV_IS_IMAGE (object) ? EV_IMAGE (object) : NULL);
	
	gtk_menu_popup (GTK_MENU (ev_window->priv->view_popup),
			NULL, NULL, NULL, NULL,
			3, gtk_get_current_event_time ());
	return TRUE;
}

static gboolean
attachment_bar_menu_popup_cb (EvSidebarAttachments *attachbar,
			      GList           *attach_list,
			      EvWindow        *ev_window)
{
	GtkWidget *popup;

	g_assert (attach_list != NULL);

	if (ev_window->priv->attach_list) {
		g_list_foreach (ev_window->priv->attach_list,
				(GFunc) g_object_unref, NULL);
		g_list_free (ev_window->priv->attach_list);
	}
	
	ev_window->priv->attach_list = attach_list;
	
	popup = ev_window->priv->attachment_popup;

	gtk_menu_popup (GTK_MENU (popup), NULL, NULL,
			NULL, NULL,
			3, gtk_get_current_event_time ());

	return TRUE;
}

static void
ev_window_update_find_status_message (EvWindow *ev_window)
{
	gchar *message;

	if (!ev_window->priv->find_job)
		return;
	
	if (ev_job_is_finished (ev_window->priv->find_job)) {
		gint n_results;

		n_results = ev_job_find_get_n_results (EV_JOB_FIND (ev_window->priv->find_job),
						       ev_page_cache_get_current_page (ev_window->priv->page_cache));
		/* TRANS: Sometimes this could be better translated as
		                      "%d hit(s) on this page".  Therefore this string
				      contains plural cases. */
		message = g_strdup_printf (ngettext ("%d found on this page",
						     "%d found on this page",
						     n_results),
					   n_results);
	} else {
		gdouble percent;

		percent = ev_job_find_get_progress (EV_JOB_FIND (ev_window->priv->find_job));
		message = g_strdup_printf (_("%3d%% remaining to search"),
					   (gint) ((1.0 - percent) * 100));
	}
	
	egg_find_bar_set_status_text (EGG_FIND_BAR (ev_window->priv->find_bar), message);
	g_free (message);
}

static void
ev_window_find_job_finished_cb (EvJobFind *job,
				EvWindow  *ev_window)
{
	ev_window_update_find_status_message (ev_window);
}

static void
ev_window_find_job_updated_cb (EvJobFind *job,
			       gint       page,
			       EvWindow  *ev_window)
{
	ev_window_update_actions (ev_window);
	
	ev_view_find_changed (EV_VIEW (ev_window->priv->view),
			      ev_job_find_get_results (job),
			      page);
	ev_window_update_find_status_message (ev_window);
}

static void
ev_window_clear_find_job (EvWindow *ev_window)
{
	if (ev_window->priv->find_job != NULL) {
		if (!ev_job_is_finished (ev_window->priv->find_job))
			ev_job_cancel (ev_window->priv->find_job);

		g_signal_handlers_disconnect_by_func (ev_window->priv->find_job,
						      ev_window_find_job_finished_cb,
						      ev_window);
		g_signal_handlers_disconnect_by_func (ev_window->priv->find_job,
						      ev_window_find_job_updated_cb,
						      ev_window);
		g_object_unref (ev_window->priv->find_job);
		ev_window->priv->find_job = NULL;
	}
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
	ev_view_find_cancel (EV_VIEW (ev_window->priv->view));
	ev_window_clear_find_job (ev_window);
	update_chrome_flag (ev_window, EV_CHROME_FINDBAR, FALSE);
	update_chrome_visibility (ev_window);
}

static void
find_bar_search_changed_cb (EggFindBar *find_bar,
			    GParamSpec *param,
			    EvWindow   *ev_window)
{
	gboolean case_sensitive;
	const char *search_string;

	if (!ev_window->priv->document || !EV_IS_DOCUMENT_FIND (ev_window->priv->document))
		return;
	
	/* Either the string or case sensitivity could have changed. */
	case_sensitive = egg_find_bar_get_case_sensitive (find_bar);
	search_string = egg_find_bar_get_search_string (find_bar);

	ev_view_find_search_changed (EV_VIEW (ev_window->priv->view));

	ev_window_clear_find_job (ev_window);

	if (search_string && search_string[0]) {
		ev_window->priv->find_job = ev_job_find_new (ev_window->priv->document,
							     ev_page_cache_get_current_page (ev_window->priv->page_cache),
							     ev_page_cache_get_n_pages (ev_window->priv->page_cache),
							     search_string,
							     case_sensitive);
		g_signal_connect (ev_window->priv->find_job, "finished",
				  G_CALLBACK (ev_window_find_job_finished_cb),
				  ev_window);
		g_signal_connect (ev_window->priv->find_job, "updated",
				  G_CALLBACK (ev_window_find_job_updated_cb),
				  ev_window);
		ev_job_scheduler_push_job (ev_window->priv->find_job, EV_JOB_PRIORITY_NONE);
	} else {
		ev_window_update_actions (ev_window);
		egg_find_bar_set_status_text (EGG_FIND_BAR (ev_window->priv->find_bar),
					      NULL);
		gtk_widget_queue_draw (GTK_WIDGET (ev_window->priv->view));
	}
}

static void
find_bar_visibility_changed_cb (EggFindBar *find_bar,
				GParamSpec *param,
				EvWindow   *ev_window)
{
	gboolean visible;

	visible = GTK_WIDGET_VISIBLE (find_bar);

	if (ev_window->priv->document &&
	    EV_IS_DOCUMENT_FIND (ev_window->priv->document)) {
		ev_view_find_set_highlight_search (EV_VIEW (ev_window->priv->view), visible);
		ev_view_find_search_changed (EV_VIEW (ev_window->priv->view));
		ev_window_update_actions (ev_window);

		if (visible)
			find_bar_search_changed_cb (find_bar, NULL, ev_window);
		else
			egg_find_bar_set_status_text (EGG_FIND_BAR (ev_window->priv->find_bar), NULL);
	}
}

static void
find_bar_scroll (EggFindBar   *find_bar,
		 GtkScrollType scroll,
		 EvWindow     *ev_window)
{
	ev_view_scroll (EV_VIEW (ev_window->priv->view), scroll, FALSE);
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
		ev_view_set_zoom (EV_VIEW (ev_window->priv->view),
				  zoom * get_screen_dpi (GTK_WINDOW (ev_window)) / 72.0,
				  FALSE);
	}
}

static void
ev_window_drag_data_received (GtkWidget        *widget,
			      GdkDragContext   *context,
			      gint              x,
			      gint              y,
			      GtkSelectionData *selection_data,
			      guint             info,
			      guint             time)

{
	EvWindow  *window = EV_WINDOW (widget);
	gchar    **uris;
	gint       i = 0;
	GSList    *uri_list = NULL;
	GtkWidget *source;

	source = gtk_drag_get_source_widget (context);
	if (source && widget == gtk_widget_get_toplevel (source)) {
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}

	uris = gtk_selection_data_get_uris (selection_data);
	if (!uris) {
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}

	for (i = 0; uris[i]; i++) {
		uri_list = g_slist_prepend (uri_list, (gpointer) uris[i]);
	}

	ev_application_open_uri_list (EV_APP, uri_list,
				      gtk_window_get_screen (GTK_WINDOW (window)),
				      0);
	gtk_drag_finish (context, TRUE, FALSE, time);

	g_strfreev (uris);
	g_slist_free (uri_list);
}

static void
ev_window_finalize (GObject *object)
{
	GList *windows = ev_application_get_windows (EV_APP);

	if (windows == NULL) {
		ev_application_shutdown (EV_APP);
	} else {
		g_list_free (windows);
	}
	
	G_OBJECT_CLASS (ev_window_parent_class)->finalize (object);
}

static void
ev_window_dispose (GObject *object)
{
	EvWindow *window = EV_WINDOW (object);
	EvWindowPrivate *priv = window->priv;
	GObject *mpkeys = ev_application_get_media_keys (EV_APP);

	if (mpkeys) {
		g_signal_handlers_disconnect_by_func (mpkeys,
						      ev_window_media_player_key_pressed,
						      window);
	}
	
	if (priv->setup_document_idle > 0) {
		g_source_remove (priv->setup_document_idle);
		priv->setup_document_idle = 0;
	}
	
	if (priv->monitor) {
		g_object_unref (priv->monitor);
		priv->monitor = NULL;
	}
	
	if (priv->title) {
		ev_window_title_free (priv->title);
		priv->title = NULL;
	}

	if (priv->ui_manager) {
		g_object_unref (priv->ui_manager);
		priv->ui_manager = NULL;
	}

	if (priv->action_group) {
		g_object_unref (priv->action_group);
		priv->action_group = NULL;
	}

	if (priv->view_popup_action_group) {
		g_object_unref (priv->view_popup_action_group);
		priv->view_popup_action_group = NULL;
	}

	if (priv->attachment_popup_action_group) {
		g_object_unref (priv->attachment_popup_action_group);
		priv->attachment_popup_action_group = NULL;
	}

	if (priv->recent_action_group) {
		g_object_unref (priv->recent_action_group);
		priv->recent_action_group = NULL;
	}

	if (priv->recent_manager) {
		g_signal_handlers_disconnect_by_func (priv->recent_manager,
						      ev_window_setup_recent,
						      window);
		priv->recent_manager = NULL;
	}

	priv->recent_ui_id = 0;

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

	if (priv->password_view) {
		g_object_unref (priv->password_view);
		priv->password_view = NULL;
	}

	if (priv->load_job) {
		ev_window_clear_load_job (window);
	}

	if (priv->reload_job) {
		ev_window_clear_reload_job (window);
	}

	if (priv->save_job) {
		ev_window_clear_save_job (window);
	}

	if (priv->thumbnail_job) {
		ev_window_clear_thumbnail_job (window);
	}

	if (priv->find_job) {
		ev_window_clear_find_job (window);
	}
	
	if (priv->local_uri) {
		ev_window_clear_local_uri (window);
		priv->local_uri = NULL;
	}

	ev_window_clear_progress_idle (window);
	if (priv->progress_cancellable) {
		g_object_unref (priv->progress_cancellable);
		priv->progress_cancellable = NULL;
	}
	
	ev_window_close_dialogs (window);

	if (window->priv->print_settings) {
		g_object_unref (window->priv->print_settings);
		window->priv->print_settings = NULL;
	}

	if (window->priv->print_page_setup) {
		g_object_unref (window->priv->print_page_setup);
		window->priv->print_page_setup = NULL;
	}

	if (priv->link) {
		g_object_unref (priv->link);
		priv->link = NULL;
	}

	if (priv->image) {
		g_object_unref (priv->image);
		priv->image = NULL;
	}

	if (priv->attach_list) {
		g_list_foreach (priv->attach_list,
				(GFunc) g_object_unref,
				NULL);
		g_list_free (priv->attach_list);
		priv->attach_list = NULL;
	}

	if (priv->find_bar) {
		g_signal_handlers_disconnect_by_func
			(window->priv->find_bar,
			 G_CALLBACK (find_bar_close_cb),
			 window);
		priv->find_bar = NULL;
	}

	if (priv->uri) {
		/* Delete the uri if it's a temp symlink (open a copy) */
		ev_window_clear_temp_symlink (window);
		g_free (priv->uri);
		priv->uri = NULL;
	}

	if (priv->search_string) {
		g_free (priv->search_string);
		priv->search_string = NULL;
	}
	
	if (priv->dest) {
		g_object_unref (priv->dest);
		priv->dest = NULL;
	}

	if (priv->history) {
		g_object_unref (priv->history);
		priv->history = NULL;
	}

	if (priv->presentation_timeout_id > 0) {
		g_source_remove (priv->presentation_timeout_id);
		priv->presentation_timeout_id = 0;
	}

	if (priv->print_queue) {
		g_queue_free (priv->print_queue);
		priv->print_queue = NULL;
	}

	G_OBJECT_CLASS (ev_window_parent_class)->dispose (object);
}

static void
menubar_deactivate_cb (GtkWidget *menubar,
		       EvWindow  *window)
{
	g_signal_handlers_disconnect_by_func (menubar,
					      G_CALLBACK (menubar_deactivate_cb),
					      window);

	gtk_menu_shell_deselect (GTK_MENU_SHELL (menubar));

	update_chrome_visibility (window);
}

static gboolean
ev_window_key_press_event (GtkWidget   *widget,
			   GdkEventKey *event)
{
	EvWindow        *ev_window = EV_WINDOW (widget);
	EvWindowPrivate *priv = ev_window->priv;
	gboolean         handled = FALSE;

	/* Propagate the event to the view first
	 * It's needed to be able to type in
	 * annot popups windows
	 */
	if (priv->view) {
		g_object_ref (priv->view);
		if (GTK_WIDGET_IS_SENSITIVE (priv->view))
			handled = gtk_widget_event (priv->view, (GdkEvent*) event);
		g_object_unref (priv->view);
	}

	if (!handled && !ev_view_get_presentation (EV_VIEW (priv->view))) {
		guint modifier = event->state & gtk_accelerator_get_default_mod_mask ();

		if (priv->menubar_accel_keyval != 0 &&
		    event->keyval == priv->menubar_accel_keyval &&
		    modifier == priv->menubar_accel_modifier) {
			if (!GTK_WIDGET_VISIBLE (priv->menubar)) {
				g_signal_connect (priv->menubar, "deactivate",
						  G_CALLBACK (menubar_deactivate_cb),
						  ev_window);

				gtk_widget_show (priv->menubar);
				gtk_menu_shell_select_first (GTK_MENU_SHELL (priv->menubar),
							     FALSE);

				handled = TRUE;
			}
		}
	}

	if (!handled)
		handled = GTK_WIDGET_CLASS (ev_window_parent_class)->key_press_event (widget, event);

	return handled;
}

static void
ev_window_class_init (EvWindowClass *ev_window_class)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (ev_window_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (ev_window_class);

	g_object_class->dispose = ev_window_dispose;
	g_object_class->finalize = ev_window_finalize;

	widget_class->key_press_event = ev_window_key_press_event;
	widget_class->screen_changed = ev_window_screen_changed;
	widget_class->window_state_event = ev_window_state_event;
	widget_class->drag_data_received = ev_window_drag_data_received;

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
	{ "FileOpenCopy", NULL, N_("Op_en a Copy"), "<control>N",
	  N_("Open a copy of the current document in a new window"),
	  G_CALLBACK (ev_window_cmd_file_open_copy) },
       	{ "FileSaveAs", GTK_STOCK_SAVE_AS, N_("_Save a Copy..."), "<control>S",
	  N_("Save a copy of the current document"),
	  G_CALLBACK (ev_window_cmd_save_as) },
	{ "FilePageSetup", GTK_STOCK_PAGE_SETUP, N_("Page Set_up..."), NULL,
	  N_("Setup the page settings for printing"),
	  G_CALLBACK (ev_window_cmd_file_print_setup) },
	{ "FilePrint", GTK_STOCK_PRINT, N_("_Print..."), "<control>P",
	  N_("Print this document"),
	  G_CALLBACK (ev_window_cmd_file_print) },
	{ "FileProperties", GTK_STOCK_PROPERTIES, N_("P_roperties"), "<alt>Return", NULL,
	  G_CALLBACK (ev_window_cmd_file_properties) },			      
	{ "FileCloseWindow", GTK_STOCK_CLOSE, NULL, "<control>W", NULL,
	  G_CALLBACK (ev_window_cmd_file_close_window) },

        /* Edit menu */
        { "EditCopy", GTK_STOCK_COPY, NULL, "<control>C", NULL,
          G_CALLBACK (ev_window_cmd_edit_copy) },
 	{ "EditSelectAll", GTK_STOCK_SELECT_ALL, N_("Select _All"), "<control>A", NULL,
	  G_CALLBACK (ev_window_cmd_edit_select_all) },
        { "EditFind", GTK_STOCK_FIND, N_("_Find..."), "<control>F",
          N_("Find a word or phrase in the document"),
          G_CALLBACK (ev_window_cmd_edit_find) },
	{ "EditFindNext", NULL, N_("Find Ne_xt"), "<control>G", NULL,
	  G_CALLBACK (ev_window_cmd_edit_find_next) },
	{ "EditFindPrevious", NULL, N_("Find Pre_vious"), "<shift><control>G", NULL,
	  G_CALLBACK (ev_window_cmd_edit_find_previous) },
        { "EditToolbar", NULL, N_("T_oolbar"), NULL, NULL,
          G_CALLBACK (ev_window_cmd_edit_toolbar) },
	{ "EditRotateLeft", EV_STOCK_ROTATE_LEFT, N_("Rotate _Left"), "<control>Left", NULL,
	  G_CALLBACK (ev_window_cmd_edit_rotate_left) },
	{ "EditRotateRight", EV_STOCK_ROTATE_RIGHT, N_("Rotate _Right"), "<control>Right", NULL,
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

	{ "ViewAutoscroll", GTK_STOCK_MEDIA_PLAY, N_("Auto_scroll"), NULL, NULL,
	  G_CALLBACK (ev_window_cmd_view_autoscroll) },

        /* Go menu */
        { "GoPreviousPage", GTK_STOCK_GO_UP, N_("_Previous Page"), "<control>Page_Up",
          N_("Go to the previous page"),
          G_CALLBACK (ev_window_cmd_go_previous_page) },
        { "GoNextPage", GTK_STOCK_GO_DOWN, N_("_Next Page"), "<control>Page_Down",
          N_("Go to the next page"),
          G_CALLBACK (ev_window_cmd_go_next_page) },
        { "GoFirstPage", GTK_STOCK_GOTO_TOP, N_("_First Page"), "<control>Home",
          N_("Go to the first page"),
          G_CALLBACK (ev_window_cmd_go_first_page) },
        { "GoLastPage", GTK_STOCK_GOTO_BOTTOM, N_("_Last Page"), "<control>End",
          N_("Go to the last page"),
          G_CALLBACK (ev_window_cmd_go_last_page) },

	/* Help menu */
	{ "HelpContents", GTK_STOCK_HELP, N_("_Contents"), "F1", NULL,
	  G_CALLBACK (ev_window_cmd_help_contents) },

	{ "HelpAbout", GTK_STOCK_ABOUT, N_("_About"), NULL, NULL,
	  G_CALLBACK (ev_window_cmd_help_about) },

	/* Toolbar-only */
	{ "LeaveFullscreen", GTK_STOCK_LEAVE_FULLSCREEN, N_("Leave Fullscreen"), NULL,
	  N_("Leave fullscreen mode"),
	  G_CALLBACK (ev_window_cmd_leave_fullscreen) },
	{ "StartPresentation", EV_STOCK_RUN_PRESENTATION, N_("Start Presentation"), NULL,
	  N_("Start a presentation"),
	  G_CALLBACK (ev_window_cmd_start_presentation) },

	/* Accellerators */
	{ "Escape", NULL, "", "Escape", "",
	  G_CALLBACK (ev_window_cmd_escape) },
        { "Slash", GTK_STOCK_FIND, NULL, "slash", NULL,
          G_CALLBACK (ev_window_cmd_edit_find) },
        { "F3", NULL, "", "F3", NULL,
          G_CALLBACK (ev_window_cmd_edit_find_next) },
        { "PageDown", NULL, "", "Page_Down", NULL,
          G_CALLBACK (ev_window_cmd_scroll_forward) },
        { "PageUp", NULL, "", "Page_Up", NULL,
          G_CALLBACK (ev_window_cmd_scroll_backward) },
        { "Space", NULL, "", "space", NULL,
          G_CALLBACK (ev_window_cmd_scroll_forward) },
        { "ShiftSpace", NULL, "", "<shift>space", NULL,
          G_CALLBACK (ev_window_cmd_scroll_backward) },
        { "BackSpace", NULL, "", "BackSpace", NULL,
          G_CALLBACK (ev_window_cmd_scroll_backward) },
        { "ShiftBackSpace", NULL, "", "<shift>BackSpace", NULL,
          G_CALLBACK (ev_window_cmd_scroll_forward) },
        { "Return", NULL, "", "Return", NULL,
          G_CALLBACK (ev_window_cmd_scroll_forward) },
        { "ShiftReturn", NULL, "", "<shift>Return", NULL,
          G_CALLBACK (ev_window_cmd_scroll_backward) },
        { "Plus", GTK_STOCK_ZOOM_IN, NULL, "plus", NULL,
          G_CALLBACK (ev_window_cmd_view_zoom_in) },
        { "CtrlEqual", GTK_STOCK_ZOOM_IN, NULL, "<control>equal", NULL,
          G_CALLBACK (ev_window_cmd_view_zoom_in) },
        { "Equal", GTK_STOCK_ZOOM_IN, NULL, "equal", NULL,
          G_CALLBACK (ev_window_cmd_view_zoom_in) },
        { "Minus", GTK_STOCK_ZOOM_OUT, NULL, "minus", NULL,
          G_CALLBACK (ev_window_cmd_view_zoom_out) },
        { "FocusPageSelector", NULL, "", "<control>l", NULL,
          G_CALLBACK (ev_window_cmd_focus_page_selector) },
        { "GoBackwardFast", NULL, "", "<shift>Page_Up", NULL,
          G_CALLBACK (ev_window_cmd_go_backward) },
        { "GoForwardFast", NULL, "", "<shift>Page_Down", NULL,
          G_CALLBACK (ev_window_cmd_go_forward) },
        { "KpPlus", GTK_STOCK_ZOOM_IN, NULL, "KP_Add", NULL,
          G_CALLBACK (ev_window_cmd_view_zoom_in) },
        { "KpMinus", GTK_STOCK_ZOOM_OUT, NULL, "KP_Subtract", NULL,
          G_CALLBACK (ev_window_cmd_view_zoom_out) },
        { "CtrlKpPlus", GTK_STOCK_ZOOM_IN, NULL, "<control>KP_Add", NULL,
          G_CALLBACK (ev_window_cmd_view_zoom_in) },
        { "CtrlKpMinus", GTK_STOCK_ZOOM_OUT, NULL, "<control>KP_Subtract", NULL,
          G_CALLBACK (ev_window_cmd_view_zoom_out) },
	{ "CtrlInsert", GTK_STOCK_COPY, NULL, "<control>Insert", NULL,
	  G_CALLBACK (ev_window_cmd_edit_copy) },
};

/* Toggle items */
static const GtkToggleActionEntry toggle_entries[] = {
	/* View Menu */
	{ "ViewToolbar", NULL, N_("_Toolbar"), NULL,
	  N_("Show or hide the toolbar"),
	  G_CALLBACK (ev_window_view_toolbar_cb), TRUE },
        { "ViewSidebar", GTK_STOCK_INDEX, N_("Side _Pane"), "F9",
	  N_("Show or hide the side pane"),
	  G_CALLBACK (ev_window_view_sidebar_cb), TRUE },
        { "ViewContinuous", EV_STOCK_VIEW_CONTINUOUS, N_("_Continuous"), NULL,
	  N_("Show the entire document"),
	  G_CALLBACK (ev_window_cmd_continuous), TRUE },
        { "ViewDual", EV_STOCK_VIEW_DUAL, N_("_Dual"), NULL,
	  N_("Show two pages at once"),
	  G_CALLBACK (ev_window_cmd_dual), FALSE },
        { "ViewFullscreen", GTK_STOCK_FULLSCREEN, N_("_Fullscreen"), "F11",
          N_("Expand the window to fill the screen"),
          G_CALLBACK (ev_window_cmd_view_fullscreen) },
        { "ViewPresentation", EV_STOCK_RUN_PRESENTATION, N_("Pre_sentation"), "F5",
          N_("Run document as a presentation"),
          G_CALLBACK (ev_window_cmd_view_presentation) },
        { "ViewBestFit", EV_STOCK_ZOOM_PAGE, N_("_Best Fit"), NULL,
          N_("Make the current document fill the window"),
          G_CALLBACK (ev_window_cmd_view_best_fit) },
        { "ViewPageWidth", EV_STOCK_ZOOM_WIDTH, N_("Fit Page _Width"), NULL,
          N_("Make the current document fill the window width"),
          G_CALLBACK (ev_window_cmd_view_page_width) },
};

/* Popups specific items */
static const GtkActionEntry view_popup_entries [] = {
	/* Links */
	{ "OpenLink", GTK_STOCK_OPEN, N_("_Open Link"), NULL,
	  NULL, G_CALLBACK (ev_view_popup_cmd_open_link) },
	{ "GoLink", GTK_STOCK_GO_FORWARD, N_("_Go To"), NULL,
	  NULL, G_CALLBACK (ev_view_popup_cmd_open_link) },
	{ "OpenLinkNewWindow", NULL, N_("Open in New _Window"), NULL,
	  NULL, G_CALLBACK (ev_view_popup_cmd_open_link_new_window) },
	{ "CopyLinkAddress", NULL, N_("_Copy Link Address"), NULL,
	  NULL, G_CALLBACK (ev_view_popup_cmd_copy_link_address) },
	{ "SaveImageAs", NULL, N_("_Save Image As..."), NULL,
	  NULL, G_CALLBACK (ev_view_popup_cmd_save_image_as) },
	{ "CopyImage", NULL, N_("Copy _Image"), NULL,
	  NULL, G_CALLBACK (ev_view_popup_cmd_copy_image) },
};

static const GtkActionEntry attachment_popup_entries [] = {
	{ "OpenAttachment", GTK_STOCK_OPEN, N_("_Open..."), NULL,
	  NULL, G_CALLBACK (ev_attachment_popup_cmd_open_attachment) },
	{ "SaveAttachmentAs", GTK_STOCK_SAVE_AS, N_("_Save a Copy..."), NULL,
	  NULL, G_CALLBACK (ev_attachment_popup_cmd_save_attachment_as) },
};

static void
sidebar_links_link_activated_cb (EvSidebarLinks *sidebar_links, EvLink *link, EvWindow *window)
{
	ev_view_handle_link (EV_VIEW (window->priv->view), link);
}

static void
activate_link_cb (EvPageAction *page_action, EvLink *link, EvWindow *window)
{
	ev_view_handle_link (EV_VIEW (window->priv->view), link);
	gtk_widget_grab_focus (window->priv->view);
}

static void
navigation_action_activate_link_cb (EvNavigationAction *action, EvLink *link, EvWindow *window)
{
	
	ev_view_handle_link (EV_VIEW (window->priv->view), link);
	gtk_widget_grab_focus (window->priv->view);
}

static void
sidebar_layers_visibility_changed (EvSidebarLayers *layers,
				   EvWindow        *window)
{
	ev_view_reload (EV_VIEW (window->priv->view));
}

static void
register_custom_actions (EvWindow *window, GtkActionGroup *group)
{
	GtkAction *action;

	action = g_object_new (EV_TYPE_PAGE_ACTION,
			       "name", PAGE_SELECTOR_ACTION,
			       "label", _("Page"),
			       "tooltip", _("Select Page"),
			       "icon_name", "text-x-generic",
			       "visible_overflown", FALSE,
			       NULL);
	g_signal_connect (action, "activate_link",
			  G_CALLBACK (activate_link_cb), window);
	gtk_action_group_add_action (group, action);
	g_object_unref (action);

	action = g_object_new (EPHY_TYPE_ZOOM_ACTION,
			       "name", ZOOM_CONTROL_ACTION,
			       "label", _("Zoom"),
			       "stock_id", EV_STOCK_ZOOM,
			       "tooltip", _("Adjust the zoom level"),
			       "zoom", 1.0,
			       NULL);
	g_signal_connect (action, "zoom_to_level",
			  G_CALLBACK (zoom_control_changed_cb), window);
	gtk_action_group_add_action (group, action);
	g_object_unref (action);

	action = g_object_new (EV_TYPE_NAVIGATION_ACTION,
			       "name", NAVIGATION_ACTION,
			       "label", _("Navigation"),
			       "is_important", TRUE,
			       "short_label", _("Back"),
			       "stock_id", GTK_STOCK_GO_DOWN,
			       /*translators: this is the history action*/
			       "tooltip", _("Move across visited pages"),
			       NULL);
	g_signal_connect (action, "activate_link",
			  G_CALLBACK (navigation_action_activate_link_cb), window);
	gtk_action_group_add_action (group, action);
	g_object_unref (action);

	action = g_object_new (EV_TYPE_OPEN_RECENT_ACTION,
			       "name", "FileOpenRecent",
			       "label", _("_Open..."),
			       "tooltip", _("Open an existing document"),
			       "stock_id", GTK_STOCK_OPEN,
			       NULL);
	g_signal_connect (action, "activate",
			  G_CALLBACK (ev_window_cmd_file_open), window);
	g_signal_connect (action, "item_activated",
			  G_CALLBACK (ev_window_open_recent_action_item_activated),
			  window);
	gtk_action_group_add_action (group, action);
	g_object_unref (action);
}

static void
set_action_properties (GtkActionGroup *action_group)
{
	GtkAction *action;

	action = gtk_action_group_get_action (action_group, "GoPreviousPage");
	g_object_set (action, "is-important", TRUE, NULL);
	/*translators: this is the label for toolbar button*/
	g_object_set (action, "short_label", _("Previous"), NULL);

	action = gtk_action_group_get_action (action_group, "GoNextPage");
	g_object_set (action, "is-important", TRUE, NULL);
	/*translators: this is the label for toolbar button*/
	g_object_set (action, "short_label", _("Next"), NULL);

	action = gtk_action_group_get_action (action_group, "ViewZoomIn");
	/*translators: this is the label for toolbar button*/
	g_object_set (action, "short_label", _("Zoom In"), NULL);

	action = gtk_action_group_get_action (action_group, "ViewZoomOut");
	/*translators: this is the label for toolbar button*/
	g_object_set (action, "short_label", _("Zoom Out"), NULL);

	action = gtk_action_group_get_action (action_group, "ViewBestFit");
	/*translators: this is the label for toolbar button*/
	g_object_set (action, "short_label", _("Best Fit"), NULL);

	action = gtk_action_group_get_action (action_group, "ViewPageWidth");
	/*translators: this is the label for toolbar button*/
	g_object_set (action, "short_label", _("Fit Width"), NULL);

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

static gboolean
view_actions_focus_in_cb (GtkWidget *widget, GdkEventFocus *event, EvWindow *window)
{
#ifdef ENABLE_DBUS
	GObject *keys;

	keys = ev_application_get_media_keys (EV_APP);
	ev_media_player_keys_focused (EV_MEDIA_PLAYER_KEYS (keys));
#endif /* ENABLE_DBUS */

	update_chrome_flag (window, EV_CHROME_RAISE_TOOLBAR, FALSE);
	ev_window_set_action_sensitive (window, "ViewToolbar", TRUE);

	ev_window_set_view_accels_sensitivity (window, TRUE);

	update_chrome_visibility (window);

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
	if (!(event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN)) {
		gboolean maximized;

		maximized = event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED;
		if (!ev_window_is_empty (window))
			ev_metadata_manager_set_boolean (window->priv->uri, "window_maximized", maximized);
	}

	return FALSE;
}

static gboolean
window_configure_event_cb (EvWindow *window, GdkEventConfigure *event, gpointer dummy)
{
	char *uri = window->priv->uri;
	GdkWindowState state;
	int x, y, width, height, document_width, document_height;

	state = gdk_window_get_state (GTK_WIDGET (window)->window);

	if (!(state & GDK_WINDOW_STATE_FULLSCREEN)) {
		gtk_window_get_position (GTK_WINDOW (window), &x, &y);
		gtk_window_get_size (GTK_WINDOW (window), &width, &height);

		if (!ev_window_is_empty (window) && window->priv->page_cache) {
			ev_page_cache_get_max_width (window->priv->page_cache, 
						     0, 1.0,
						     &document_width);
			ev_page_cache_get_max_height (window->priv->page_cache, 
						      0, 1.0,
						      &document_height);			
			ev_metadata_manager_set_double (uri, "window_width_ratio", 
							(double)width / document_width);
			ev_metadata_manager_set_double (uri, "window_height_ratio", 
							(double)height / document_height);
			
			ev_metadata_manager_set_int (uri, "window_x", x);
			ev_metadata_manager_set_int (uri, "window_y", y);
			ev_metadata_manager_set_int (uri, "window_width", width);
			ev_metadata_manager_set_int (uri, "window_height", height);
		}
	}

	return FALSE;
}

static void
launch_action (EvWindow *window, EvLinkAction *action)
{
	const char *filename = ev_link_action_get_filename (action);
	GAppInfo *app_info;
	GFile *file;
	GList file_list = {NULL};
	GAppLaunchContext *context;
	GError *error = NULL;

	if (filename == NULL)
		return;

	if (g_path_is_absolute (filename)) {
		file = g_file_new_for_path (filename);
	} else {
		GFile *base_file;
		gchar *dir;

		dir = g_path_get_dirname (window->priv->uri);
		base_file = g_file_new_for_uri (dir);
		g_free (dir);
		
		file = g_file_resolve_relative_path (base_file, filename);
		g_object_unref (base_file);
	}

	app_info = g_file_query_default_handler (file, NULL, &error);
	if (!app_info) {
		ev_window_error_message (window, error,
					 "%s",
					 _("Unable to launch external application."));
		g_object_unref (file);
		g_error_free (error);

		return;
	}

	context = G_APP_LAUNCH_CONTEXT (gdk_app_launch_context_new ());
	gdk_app_launch_context_set_screen (GDK_APP_LAUNCH_CONTEXT (context),
					   gtk_window_get_screen (GTK_WINDOW (window)));
	gdk_app_launch_context_set_timestamp (GDK_APP_LAUNCH_CONTEXT (context), GDK_CURRENT_TIME);
	
	file_list.data = file;
	if (!g_app_info_launch (app_info, &file_list, context, &error)) {
		ev_window_error_message (window, error,
					 "%s",
					 _("Unable to launch external application."));
		g_error_free (error);
	}
	
	g_object_unref (app_info);
	g_object_unref (file);
        /* FIXMEchpe: unref launch context? */

	/* According to the PDF spec filename can be an executable. I'm not sure
	   allowing to launch executables is a good idea though. -- marco */
}

static void
launch_external_uri (EvWindow *window, EvLinkAction *action)
{
	const gchar *uri = ev_link_action_get_uri (action);
	GError *error = NULL;
	gboolean ret;
	GAppLaunchContext *context;

	context = G_APP_LAUNCH_CONTEXT (gdk_app_launch_context_new ());
	gdk_app_launch_context_set_screen (GDK_APP_LAUNCH_CONTEXT (context),
					   gtk_window_get_screen (GTK_WINDOW (window)));
	gdk_app_launch_context_set_timestamp (GDK_APP_LAUNCH_CONTEXT (context),
					      GDK_CURRENT_TIME);

	if (!g_strstr_len (uri, strlen (uri), "://") &&
	    !g_str_has_prefix (uri, "mailto:")) {
		gchar *http;
		
		/* Not a valid uri, assuming it's http */
		http = g_strdup_printf ("http://%s", uri);
		ret = g_app_info_launch_default_for_uri (http, context, &error);
		g_free (http);
	} else {
		ret = g_app_info_launch_default_for_uri (uri, context, &error);
	}
	
  	if (ret == FALSE) {
		ev_window_error_message (window, error, 
					 "%s", _("Unable to open external link"));
		g_error_free (error);
	}

        /* FIXMEchpe: unref launch context? */
}

static void
open_remote_link (EvWindow *window, EvLinkAction *action)
{
	gchar *uri;
	gchar *dir;

	dir = g_path_get_dirname (window->priv->uri);
	
	uri = g_build_filename (dir, ev_link_action_get_filename (action),
				NULL);
	g_free (dir);

	ev_application_open_uri_at_dest (EV_APP, uri,
					 gtk_window_get_screen (GTK_WINDOW (window)),
					 ev_link_action_get_dest (action),
					 0,
					 NULL, 
					 GDK_CURRENT_TIME);

	g_free (uri);
}

static void
do_action_named (EvWindow *window, EvLinkAction *action)
{
	const gchar *name = ev_link_action_get_name (action);

	if (g_ascii_strcasecmp (name, "FirstPage") == 0) {
		ev_window_cmd_go_first_page (NULL, window);
	} else if (g_ascii_strcasecmp (name, "PrevPage") == 0) {
		ev_window_cmd_go_previous_page (NULL, window);
	} else if (g_ascii_strcasecmp (name, "NextPage") == 0) {
		ev_window_cmd_go_next_page (NULL, window);
	} else if (g_ascii_strcasecmp (name, "LastPage") == 0) {
		ev_window_cmd_go_last_page (NULL, window);
	} else if (g_ascii_strcasecmp (name, "GoToPage") == 0) {
		ev_window_cmd_focus_page_selector (NULL, window);
	} else if (g_ascii_strcasecmp (name, "Find") == 0) {
		ev_window_cmd_edit_find (NULL, window);
	} else if (g_ascii_strcasecmp (name, "Close") == 0) {
		ev_window_cmd_file_close_window (NULL, window);
	} else if (g_ascii_strcasecmp (name, "Print") == 0) {
		ev_window_cmd_file_print (NULL, window);
	} else {
		g_warning ("Unimplemented named action: %s, please post a "
		           "bug report in Evince bugzilla "
		           "(http://bugzilla.gnome.org) with a testcase.",
			   name);
	}
}

static void
view_external_link_cb (EvView *view, EvLinkAction *action, EvWindow *window)
{
	switch (ev_link_action_get_action_type (action)) {
	        case EV_LINK_ACTION_TYPE_GOTO_DEST: {
			EvLinkDest *dest;
			
			dest = ev_link_action_get_dest (action);
			if (!dest)
				return;

			ev_window_cmd_file_open_copy_at_dest (window, dest);
		}
			break;
	        case EV_LINK_ACTION_TYPE_EXTERNAL_URI:
			launch_external_uri (window, action);
			break;
	        case EV_LINK_ACTION_TYPE_LAUNCH:
			launch_action (window, action);
			break;
	        case EV_LINK_ACTION_TYPE_GOTO_REMOTE:
			open_remote_link (window, action);
			break;
	        case EV_LINK_ACTION_TYPE_NAMED:
			do_action_named (window, action);
			break;
	        default:
			g_assert_not_reached ();
	}
}

static void
ev_view_popup_cmd_open_link (GtkAction *action, EvWindow *window)
{
	ev_view_handle_link (EV_VIEW (window->priv->view), window->priv->link);
}

static void
ev_view_popup_cmd_open_link_new_window (GtkAction *action, EvWindow *window)
{
	EvLinkAction *ev_action = NULL;
	EvLinkDest   *dest;

	ev_action = ev_link_get_action (window->priv->link);
	if (!ev_action)
		return;

	dest = ev_link_action_get_dest (ev_action);
	if (!dest)
		return;

	ev_window_cmd_file_open_copy_at_dest (window, dest);
}

static void
ev_view_popup_cmd_copy_link_address (GtkAction *action, EvWindow *window)
{
	EvLinkAction *ev_action;

	ev_action = ev_link_get_action (window->priv->link);
	if (!ev_action)
		return;

	ev_view_copy_link_address (EV_VIEW (window->priv->view),
				   ev_action);
}


static void
image_save_dialog_response_cb (GtkWidget *fc,
			       gint       response_id,
			       EvWindow  *ev_window)
{
	GFile           *target_file;
	gboolean         is_native;
	GError          *error = NULL;
	GdkPixbuf       *pixbuf;
	gchar           *uri;
	gchar 	       **extensions;
	gchar           *filename;
	gchar           *file_format;
	GdkPixbufFormat *format;
	GtkFileFilter   *filter;
	
	if (response_id != GTK_RESPONSE_OK) {
		gtk_widget_destroy (fc);
		return;
	}

	uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (fc));
	filter = gtk_file_chooser_get_filter (GTK_FILE_CHOOSER (fc));
	format = g_object_get_data (G_OBJECT (filter), "pixbuf-format");
	
	if (format == NULL) {
		format = get_gdk_pixbuf_format_by_extension (uri);
	}

	if (format == NULL && g_strrstr (uri, ".") == NULL) {
		/* no extension found and no extension provided within uri */
		format = get_gdk_pixbuf_format_by_extension (".png");
		if (format == NULL) {
			/* no .png support, try .jpeg */
			format = get_gdk_pixbuf_format_by_extension (".jpeg");
		}
	}

	if (format == NULL) {
		ev_window_error_message (ev_window, NULL, 
					 "%s",
					 _("Couldn't find appropriate format to save image"));
		g_free (uri);
		gtk_widget_destroy (fc);

		return;
	}

	extensions = gdk_pixbuf_format_get_extensions (format);
	if (!g_str_has_suffix (uri, extensions[0])) {
		gchar *uri_extension;
		
		uri_extension = g_strconcat (uri, ".", extensions[0], NULL);
		target_file = g_file_new_for_uri (uri_extension);
		g_free (uri_extension);
	} else {
		target_file = g_file_new_for_uri (uri);
	}
	g_strfreev (extensions);
	g_free (uri);
	
	is_native = g_file_is_native (target_file);
	if (is_native) {
		filename = g_file_get_path (target_file);
	} else {
		filename = ev_tmp_filename ("saveimage");
	}

	ev_document_doc_mutex_lock ();
	pixbuf = ev_document_images_get_image (EV_DOCUMENT_IMAGES (ev_window->priv->document),
					       ev_window->priv->image);
	ev_document_doc_mutex_unlock ();

	file_format = gdk_pixbuf_format_get_name (format);
	gdk_pixbuf_save (pixbuf, filename, file_format, &error, NULL);
	g_free (file_format);
	g_object_unref (pixbuf);
	
	if (error) {
		ev_window_error_message (ev_window, error, 
					 "%s", _("The image could not be saved."));
		g_error_free (error);
		g_free (filename);
		g_object_unref (target_file);
		gtk_widget_destroy (fc);

		return;
	}

	if (!is_native) {
		GFile *source_file;
		
		source_file = g_file_new_for_path (filename);
		
		ev_window_save_remote (ev_window, EV_SAVE_IMAGE,
				       source_file, target_file);
		g_object_unref (source_file);
	}
	
	g_free (filename);
	g_object_unref (target_file);
	gtk_widget_destroy (fc);
}

static void
ev_view_popup_cmd_save_image_as (GtkAction *action, EvWindow *window)
{
	GtkWidget *fc;

	if (!window->priv->image)
		return;

	fc = gtk_file_chooser_dialog_new (_("Save Image"),
					  GTK_WINDOW (window),
					  GTK_FILE_CHOOSER_ACTION_SAVE,
					  GTK_STOCK_CANCEL,
					  GTK_RESPONSE_CANCEL,
					  GTK_STOCK_SAVE, GTK_RESPONSE_OK,
					  NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (fc), GTK_RESPONSE_OK);
        gtk_dialog_set_alternative_button_order (GTK_DIALOG (fc),
                                                 GTK_RESPONSE_OK,
                                                 GTK_RESPONSE_CANCEL,
                                                 -1);

	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (fc), FALSE);
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (fc), TRUE);
	
	file_chooser_dialog_add_writable_pixbuf_formats	(GTK_FILE_CHOOSER (fc));
	
	g_signal_connect (fc, "response",
			  G_CALLBACK (image_save_dialog_response_cb),
			  window);

	gtk_widget_show (fc);
}

static void
ev_view_popup_cmd_copy_image (GtkAction *action, EvWindow *window)
{
	GtkClipboard *clipboard;
	GdkPixbuf    *pixbuf;

	if (!window->priv->image)
		return;
	
	clipboard = gtk_widget_get_clipboard (GTK_WIDGET (window),
					      GDK_SELECTION_CLIPBOARD);
	ev_document_doc_mutex_lock ();
	pixbuf = ev_document_images_get_image (EV_DOCUMENT_IMAGES (window->priv->document),
					       window->priv->image);
	ev_document_doc_mutex_unlock ();
	
	gtk_clipboard_set_image (clipboard, pixbuf);
	g_object_unref (pixbuf);
}

static void
ev_attachment_popup_cmd_open_attachment (GtkAction *action, EvWindow *window)
{
	GList     *l;
	GdkScreen *screen;
	
	if (!window->priv->attach_list)
		return;

	screen = gtk_window_get_screen (GTK_WINDOW (window));

	for (l = window->priv->attach_list; l && l->data; l = g_list_next (l)) {
		EvAttachment *attachment;
		GError       *error = NULL;
		
		attachment = (EvAttachment *) l->data;
		
		ev_attachment_open (attachment, screen, GDK_CURRENT_TIME, &error);

		if (error) {
			ev_window_error_message (window, error, 
						 "%s", _("Unable to open attachment"));
			g_error_free (error);
		}
	}
}

static void
attachment_save_dialog_response_cb (GtkWidget *fc,
				    gint       response_id,
				    EvWindow  *ev_window)
{
	GFile                *target_file;
	gchar                *uri;
	GList                *l;
	GtkFileChooserAction  fc_action;
	gboolean              is_dir;
	gboolean              is_native;
	
	if (response_id != GTK_RESPONSE_OK) {
		gtk_widget_destroy (fc);
		return;
	}

	uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (fc));
	target_file = g_file_new_for_uri (uri);
	g_object_get (G_OBJECT (fc), "action", &fc_action, NULL);
	is_dir = (fc_action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
	is_native = g_file_is_native (target_file);
	
	for (l = ev_window->priv->attach_list; l && l->data; l = g_list_next (l)) {
		EvAttachment *attachment;
		GFile        *save_to;
		GError       *error = NULL;
		
		attachment = (EvAttachment *) l->data;

		if (is_native) {
			if (is_dir) {
				save_to = g_file_get_child (target_file,
							    ev_attachment_get_name (attachment));
			} else {
				save_to = g_object_ref (target_file);
			}
		} else {
			save_to = ev_tmp_file_get ("saveattachment");
		}

		ev_attachment_save (attachment, save_to, &error);
		
		if (error) {
			ev_window_error_message (ev_window, error, 
						 "%s", _("The attachment could not be saved."));
			g_error_free (error);
			g_object_unref (save_to);

			continue;
		}

		if (!is_native) {
			GFile *dest_file;

			if (is_dir) {
				dest_file = g_file_get_child (target_file,
							      ev_attachment_get_name (attachment));
			} else {
				dest_file = g_object_ref (target_file);
			}

			ev_window_save_remote (ev_window, EV_SAVE_ATTACHMENT,
					       save_to, dest_file);

			g_object_unref (dest_file);
		}

		g_object_unref (save_to);
	}

	g_free (uri);
	g_object_unref (target_file);

	gtk_widget_destroy (fc);
}

static void
ev_attachment_popup_cmd_save_attachment_as (GtkAction *action, EvWindow *window)
{
	GtkWidget    *fc;
	EvAttachment *attachment = NULL;

	if (!window->priv->attach_list)
		return;

	if (g_list_length (window->priv->attach_list) == 1)
		attachment = (EvAttachment *) window->priv->attach_list->data;
	
	fc = gtk_file_chooser_dialog_new (
		_("Save Attachment"),
		GTK_WINDOW (window),
		attachment ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
		GTK_STOCK_CANCEL,
		GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_OK,
		NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (fc), GTK_RESPONSE_OK);
        gtk_dialog_set_alternative_button_order (GTK_DIALOG (fc),
                                                 GTK_RESPONSE_OK,
                                                 GTK_RESPONSE_CANCEL,
                                                 -1);

	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (fc), TRUE);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (fc), FALSE);

	if (attachment)
		gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (fc),
						   ev_attachment_get_name (attachment));

	g_signal_connect (fc, "response",
			  G_CALLBACK (attachment_save_dialog_response_cb),
			  window);

	gtk_widget_show (fc);
}

static void
ev_window_media_player_key_pressed (EvWindow    *window,
				    const gchar *key,
				    gpointer     user_data)
{
	if (!gtk_window_is_active (GTK_WINDOW (window))) 
		return;
	
	/* Note how Previous/Next only go to the
	 * next/previous page despite their icon telling you
	 * they should go to the beginning/end.
	 *
	 * There's very few keyboards with FFW/RWD though,
	 * so we stick the most useful keybinding on the most
	 * often seen keys
	 */
	if (strcmp (key, "Play") == 0) {
		ev_window_run_presentation (window);
	} else if (strcmp (key, "Previous") == 0) {
		ev_window_cmd_go_previous_page (NULL, window);
	} else if (strcmp (key, "Next") == 0) {
		ev_window_cmd_go_next_page (NULL, window);
	} else if (strcmp (key, "FastForward") == 0) {
		ev_window_cmd_go_last_page (NULL, window);
	} else if (strcmp (key, "Rewind") == 0) {
		ev_window_cmd_go_first_page (NULL, window);
	}
}

static void
ev_window_init (EvWindow *ev_window)
{
	GtkActionGroup *action_group;
	GtkAccelGroup *accel_group;
	GError *error = NULL;
	GtkWidget *sidebar_widget;
	GObject *mpkeys;
	gchar *ui_path;

	g_signal_connect (ev_window, "configure_event",
			  G_CALLBACK (window_configure_event_cb), NULL);
	g_signal_connect (ev_window, "window_state_event",
			  G_CALLBACK (window_state_event_cb), NULL);

	ev_window->priv = EV_WINDOW_GET_PRIVATE (ev_window);

	ev_window->priv->page_mode = PAGE_MODE_DOCUMENT;
	ev_window->priv->title = ev_window_title_new (ev_window);

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

	action_group = gtk_action_group_new ("ViewPopupActions");
	ev_window->priv->view_popup_action_group = action_group;
	gtk_action_group_set_translation_domain (action_group, NULL);
	gtk_action_group_add_actions (action_group, view_popup_entries,
				      G_N_ELEMENTS (view_popup_entries),
				      ev_window);
	gtk_ui_manager_insert_action_group (ev_window->priv->ui_manager,
					    action_group, 0);

	action_group = gtk_action_group_new ("AttachmentPopupActions");
	ev_window->priv->attachment_popup_action_group = action_group;
	gtk_action_group_set_translation_domain (action_group, NULL);
	gtk_action_group_add_actions (action_group, attachment_popup_entries,
				      G_N_ELEMENTS (attachment_popup_entries),
				      ev_window);
	gtk_ui_manager_insert_action_group (ev_window->priv->ui_manager,
					    action_group, 0);

	ui_path = g_build_filename (ev_application_get_data_dir (EV_APP),
				    "evince-ui.xml", NULL);
	if (!gtk_ui_manager_add_ui_from_file (
		ev_window->priv->ui_manager, ui_path, &error))
	{
		g_warning ("building menus failed: %s", error->message);
		g_error_free (error);
	}
	g_free (ui_path);
	
	ev_window->priv->recent_manager = gtk_recent_manager_get_default ();
	ev_window->priv->recent_action_group = NULL;
	ev_window->priv->recent_ui_id = 0;
	g_signal_connect_swapped (ev_window->priv->recent_manager,
				  "changed",
				  G_CALLBACK (ev_window_setup_recent),
				  ev_window);

	ev_window->priv->menubar =
		 gtk_ui_manager_get_widget (ev_window->priv->ui_manager,
					    "/MainMenu");
	gtk_box_pack_start (GTK_BOX (ev_window->priv->main_box),
			    ev_window->priv->menubar,
			    FALSE, FALSE, 0);

	ev_window->priv->toolbar = GTK_WIDGET 
	  (g_object_new (EGG_TYPE_EDITABLE_TOOLBAR,
			 "ui-manager", ev_window->priv->ui_manager,
			 "popup-path", "/ToolbarPopup",
			 "model", ev_application_get_toolbars_model (EV_APP),
			 NULL));

	egg_editable_toolbar_show (EGG_EDITABLE_TOOLBAR (ev_window->priv->toolbar),
				   "DefaultToolBar");
	gtk_box_pack_start (GTK_BOX (ev_window->priv->main_box),
			    ev_window->priv->toolbar,
			    FALSE, FALSE, 0);
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

	sidebar_widget = ev_sidebar_attachments_new ();
	ev_window->priv->sidebar_attachments = sidebar_widget;
	g_signal_connect_object (sidebar_widget,
				 "popup",
				 G_CALLBACK (attachment_bar_menu_popup_cb),
				 ev_window, 0);
	gtk_widget_show (sidebar_widget);
	ev_sidebar_add_page (EV_SIDEBAR (ev_window->priv->sidebar),
			     sidebar_widget);

	sidebar_widget = ev_sidebar_layers_new ();
	ev_window->priv->sidebar_layers = sidebar_widget;
	g_signal_connect (sidebar_widget,
			  "layers_visibility_changed",
			  G_CALLBACK (sidebar_layers_visibility_changed),
			  ev_window);
	gtk_widget_show (sidebar_widget);
	ev_sidebar_add_page (EV_SIDEBAR (ev_window->priv->sidebar),
			     sidebar_widget);

	ev_window->priv->view_box = gtk_vbox_new (FALSE, 0);
	ev_window->priv->scrolled_window =
		GTK_WIDGET (g_object_new (GTK_TYPE_SCROLLED_WINDOW,
					  "shadow-type", GTK_SHADOW_IN,
					  NULL));
	gtk_box_pack_start (GTK_BOX (ev_window->priv->view_box),
			    ev_window->priv->scrolled_window,
			    TRUE, TRUE, 0);
	gtk_widget_show (ev_window->priv->scrolled_window);

	gtk_paned_add2 (GTK_PANED (ev_window->priv->hpaned),
			ev_window->priv->view_box);
	gtk_widget_show (ev_window->priv->view_box);

	ev_window->priv->view = ev_view_new ();
	ev_view_set_screen_dpi (EV_VIEW (ev_window->priv->view),
				get_screen_dpi (GTK_WINDOW (ev_window)));
	ev_window->priv->password_view = ev_password_view_new (GTK_WINDOW (ev_window));
	g_signal_connect_swapped (ev_window->priv->password_view,
				  "unlock",
				  G_CALLBACK (ev_window_password_view_unlock),
				  ev_window);
	g_signal_connect_object (ev_window->priv->view, "focus_in_event",
			         G_CALLBACK (view_actions_focus_in_cb),
				 ev_window, 0);
	g_signal_connect_object (ev_window->priv->view, "focus_out_event",
			         G_CALLBACK (view_actions_focus_out_cb),
			         ev_window, 0);
	g_signal_connect_object (ev_window->priv->view, "external-link",
			         G_CALLBACK (view_external_link_cb),
			         ev_window, 0);
	g_signal_connect_object (ev_window->priv->view, "handle-link",
			         G_CALLBACK (view_handle_link_cb),
			         ev_window, 0);
	g_signal_connect_swapped (ev_window->priv->view, "zoom_invalid",
				 G_CALLBACK (ev_window_set_view_size),
				 ev_window);
	g_signal_connect_object (ev_window->priv->view,
			         "popup",
				 G_CALLBACK (view_menu_popup_cb),
				 ev_window, 0);
	gtk_widget_show (ev_window->priv->view);
	gtk_widget_show (ev_window->priv->password_view);

	/* Find Bar */
	ev_window->priv->find_bar = egg_find_bar_new ();
	gtk_box_pack_end (GTK_BOX (ev_window->priv->main_box),
			  ev_window->priv->find_bar,
			  FALSE, TRUE, 0);

	/* We own a ref on these widgets, as we can swap them in and out */
	g_object_ref (ev_window->priv->view);
	g_object_ref (ev_window->priv->password_view);

	gtk_container_add (GTK_CONTAINER (ev_window->priv->scrolled_window),
			   ev_window->priv->view);

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
	g_signal_connect (ev_window->priv->view,
			  "notify::has-selection",
			  G_CALLBACK (ev_window_has_selection_changed_cb),
			  ev_window);

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
			  G_CALLBACK (find_bar_visibility_changed_cb),
			  ev_window);
	g_signal_connect (ev_window->priv->find_bar,
			  "scroll",
			  G_CALLBACK (find_bar_scroll),
			  ev_window);

	/* Popups */
	ev_window->priv->view_popup = gtk_ui_manager_get_widget (ev_window->priv->ui_manager,
								 "/DocumentPopup");
	ev_window->priv->link = NULL;

	ev_window->priv->attachment_popup = gtk_ui_manager_get_widget (ev_window->priv->ui_manager,
								       "/AttachmentPopup");
	ev_window->priv->attach_list = NULL;

	/* Media player keys */
	mpkeys = ev_application_get_media_keys (EV_APP);
	if (mpkeys) {
		g_signal_connect_swapped (mpkeys, "key_pressed",
					  G_CALLBACK (ev_window_media_player_key_pressed),
					  ev_window);
	}

	/* Give focus to the document view */
	gtk_widget_grab_focus (ev_window->priv->view);

	/* Set it user interface params */
	ev_window_setup_recent (ev_window);

	ev_window_setup_gtk_settings (ev_window);

	setup_chrome_from_metadata (ev_window);
	set_chrome_actions (ev_window);
	update_chrome_visibility (ev_window);

	gtk_window_set_default_size (GTK_WINDOW (ev_window), 600, 600);

	setup_view_from_metadata (ev_window);
	setup_sidebar_from_metadata (ev_window);

        ev_window_sizing_mode_changed_cb (EV_VIEW (ev_window->priv->view), NULL, ev_window);
	ev_window_setup_action_sensitivity (ev_window);

	/* Drag and Drop */
	gtk_drag_dest_set (GTK_WIDGET (ev_window),
			   GTK_DEST_DEFAULT_ALL,
			   NULL, 0,
			   GDK_ACTION_COPY);
	gtk_drag_dest_add_uri_targets (GTK_WIDGET (ev_window));
}

/**
 * ev_window_new:
 *
 * Creates a #GtkWidget that represents the window.
 *
 * Returns: the #GtkWidget that represents the window.
 */
GtkWidget *
ev_window_new (void)
{
	GtkWidget *ev_window;

	ev_window = GTK_WIDGET (g_object_new (EV_TYPE_WINDOW,
					      "type", GTK_WINDOW_TOPLEVEL,
					      NULL));

	return ev_window;
}
