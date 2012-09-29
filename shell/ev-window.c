/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2009 Juanjo Marín <juanj.marin@juntadeandalucia.es>
 *  Copyright (C) 2008 Carlos Garcia Campos
 *  Copyright (C) 2004 Martin Kretzschmar
 *  Copyright (C) 2004 Red Hat, Inc.
 *  Copyright (C) 2000, 2001, 2002, 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005, 2009, 2012 Christian Persch
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

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
#include "ev-document-annotations.h"
#include "ev-document-type-builtins.h"
#include "ev-document-misc.h"
#include "ev-file-exporter.h"
#include "ev-file-helpers.h"
#include "ev-file-monitor.h"
#include "ev-history.h"
#include "ev-image.h"
#include "ev-job-scheduler.h"
#include "ev-jobs.h"
#include "ev-message-area.h"
#include "ev-metadata.h"
#include "ev-navigation-action.h"
#include "ev-open-recent-action.h"
#include "ev-page-action.h"
#include "ev-password-view.h"
#include "ev-properties-dialog.h"
#include "ev-sidebar-annotations.h"
#include "ev-sidebar-attachments.h"
#include "ev-sidebar-bookmarks.h"
#include "ev-sidebar.h"
#include "ev-sidebar-links.h"
#include "ev-sidebar-page.h"
#include "ev-sidebar-thumbnails.h"
#include "ev-sidebar-layers.h"
#include "ev-stock-icons.h"
#include "ev-utils.h"
#include "ev-keyring.h"
#include "ev-view.h"
#include "ev-view-presentation.h"
#include "ev-view-type-builtins.h"
#include "ev-window.h"
#include "ev-window-title.h"
#include "ev-print-operation.h"
#include "ev-progress-message-area.h"
#include "ev-annotation-properties-dialog.h"
#include "ev-bookmarks.h"
#include "ev-bookmark-action.h"

#ifdef ENABLE_DBUS
#include "ev-gdbus-generated.h"
#include "ev-media-player-keys.h"
#endif /* ENABLE_DBUS */

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
	GtkWidget *presentation_view;
	GtkWidget *message_area;
	GtkWidget *password_view;
	GtkWidget *sidebar_thumbs;
	GtkWidget *sidebar_links;
	GtkWidget *sidebar_attachments;
	GtkWidget *sidebar_layers;
	GtkWidget *sidebar_annots;
	GtkWidget *sidebar_bookmarks;

	/* Settings */
	GSettings *settings;
	GSettings *default_settings;
	GSettings *lockdown_settings;

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
	GtkActionGroup   *bookmarks_action_group;
	guint             bookmarks_ui_id;
	GtkUIManager     *ui_manager;

	/* Fullscreen mode */
	GtkWidget *fullscreen_toolbar;

	/* Popup view */
	GtkWidget    *view_popup;
	EvLink       *link;
	EvImage      *image;
	EvAnnotation *annot;

	/* Popup attachment */
	GtkWidget    *attachment_popup;
	GList        *attach_list;

	/* Document */
	EvDocumentModel *model;
	char *uri;
	glong uri_mtime;
	char *local_uri;
	gboolean in_reload;
	EvFileMonitor *monitor;
	guint setup_document_idle;
	
	EvDocument *document;
	EvHistory *history;
	EvWindowPageMode page_mode;
	EvWindowTitle *title;
	EvMetadata *metadata;
	EvBookmarks *bookmarks;

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

#ifdef ENABLE_DBUS
	/* DBus */
	EvEvinceWindow *skeleton;
	gchar          *dbus_object_path;
#endif

        guint presentation_mode_inhibit_id;
};

#define EV_WINDOW_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_WINDOW, EvWindowPrivate))

#define EV_WINDOW_IS_PRESENTATION(w) (w->priv->presentation_view != NULL)

#define PAGE_SELECTOR_ACTION	"PageSelector"
#define ZOOM_CONTROL_ACTION	"ViewZoom"
#define NAVIGATION_ACTION	"Navigation"

#define GS_LOCKDOWN_SCHEMA_NAME  "org.gnome.desktop.lockdown"
#define GS_LOCKDOWN_SAVE         "disable-save-to-disk"
#define GS_LOCKDOWN_PRINT        "disable-printing"
#define GS_LOCKDOWN_PRINT_SETUP  "disable-print-setup"

#ifdef ENABLE_DBUS
#define EV_WINDOW_DBUS_OBJECT_PATH "/org/gnome/evince/Window/%d"
#define EV_WINDOW_DBUS_INTERFACE   "org.gnome.evince.Window"
#endif

#define GS_SCHEMA_NAME           "org.gnome.Evince"
#define GS_OVERRIDE_RESTRICTIONS "override-restrictions"
#define GS_AUTO_RELOAD           "auto-reload"
#define GS_LAST_DOCUMENT_DIRECTORY "document-directory"
#define GS_LAST_PICTURES_DIRECTORY "pictures-directory"

#define SIDEBAR_DEFAULT_SIZE    132
#define LINKS_SIDEBAR_ID "links"
#define THUMBNAILS_SIDEBAR_ID "thumbnails"
#define ATTACHMENTS_SIDEBAR_ID "attachments"
#define LAYERS_SIDEBAR_ID "layers"
#define ANNOTS_SIDEBAR_ID "annotations"
#define BOOKMARKS_SIDEBAR_ID "bookmarks"

#define EV_PRINT_SETTINGS_FILE  "print-settings"
#define EV_PRINT_SETTINGS_GROUP "Print Settings"
#define EV_PAGE_SETUP_GROUP     "Page Setup"

#define EV_TOOLBARS_FILENAME "evince-toolbar.xml"

#define MIN_SCALE 0.05409
#define PAGE_CACHE_SIZE 52428800 /* 50MB */

#define MAX_RECENT_ITEM_LEN (40)

#define EV_HELP         "help:evince"
#define EV_HELP_TOOLBAR "help:evince/toolbar"

#define TOOLBAR_RESOURCE_PATH "/org/gnome/evince/shell/ui/toolbar.xml"

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
static void     ev_window_view_toolbar_cb               (GtkAction        *action,
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
static void     ev_window_sizing_mode_changed_cb        (EvDocumentModel  *model,
							 GParamSpec       *pspec,
							 EvWindow         *ev_window);
static void     ev_window_zoom_changed_cb 	        (EvDocumentModel  *model,
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
static void     ev_view_popup_cmd_annot_properties      (GtkAction        *action,
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
static void     view_external_link_cb                   (EvWindow         *window,
							 EvLinkAction     *action);
static void     ev_window_load_file_remote              (EvWindow         *ev_window,
							 GFile            *source_file);
static void     ev_window_media_player_key_pressed      (EvWindow         *window,
							 const gchar      *key,
							 gpointer          user_data);
static void     ev_window_update_max_min_scale          (EvWindow         *window);
#ifdef ENABLE_DBUS
static void	ev_window_emit_closed			(EvWindow         *window);
static void 	ev_window_emit_doc_loaded		(EvWindow	  *window);
#endif
static void     ev_window_setup_bookmarks               (EvWindow         *window);

static gchar *nautilus_sendto = NULL;

G_DEFINE_TYPE (EvWindow, ev_window, GTK_TYPE_APPLICATION_WINDOW)

static gdouble
get_screen_dpi (EvWindow *window)
{
	GdkScreen *screen;

	screen = gtk_window_get_screen (GTK_WINDOW (window));
	return ev_document_misc_get_screen_dpi (screen);
}

static gboolean
ev_window_is_editing_toolbar (EvWindow *ev_window)
{
	return egg_editable_toolbar_get_edit_mode (EGG_EDITABLE_TOOLBAR (ev_window->priv->toolbar));
}

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

	if (document) {
		has_document = TRUE;
		has_pages = ev_document_get_n_pages (document) > 0;
		info = ev_document_get_info (document);
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

	if (has_document && ev_window->priv->settings) {
		override_restrictions =
			g_settings_get_boolean (ev_window->priv->settings,
						GS_OVERRIDE_RESTRICTIONS);
	}

	if (!override_restrictions && info && info->fields_mask & EV_DOCUMENT_INFO_PERMISSIONS) {
		ok_to_print = (info->permissions & EV_DOCUMENT_PERMISSIONS_OK_TO_PRINT);
		ok_to_copy = (info->permissions & EV_DOCUMENT_PERMISSIONS_OK_TO_COPY);
	}

	if (has_document && !ev_print_operation_exists_for_document(document))
		ok_to_print = FALSE;

	if (has_document && ev_window->priv->lockdown_settings &&
	    g_settings_get_boolean (ev_window->priv->lockdown_settings, GS_LOCKDOWN_SAVE)) {
		ok_to_copy = FALSE;
	}

	if (has_document && ev_window->priv->lockdown_settings &&
	    g_settings_get_boolean (ev_window->priv->lockdown_settings, GS_LOCKDOWN_PRINT)) {
		ok_to_print = FALSE;
	}

	/* File menu */
	ev_window_set_action_sensitive (ev_window, "FileOpenCopy", has_document);
	ev_window_set_action_sensitive (ev_window, "FileSaveAs", has_document && ok_to_copy);
	ev_window_set_action_sensitive (ev_window, "FilePrint", has_pages && ok_to_print);
	ev_window_set_action_sensitive (ev_window, "FileProperties", has_document && has_properties);
	ev_window_set_action_sensitive (ev_window, "FileOpenContainingFolder", has_document);
	ev_window_set_action_sensitive (ev_window, "FileSendTo", has_document);

        /* Edit menu */
	ev_window_set_action_sensitive (ev_window, "EditSelectAll", has_pages && can_get_text);
	ev_window_set_action_sensitive (ev_window, "EditFind", can_find);
	ev_window_set_action_sensitive (ev_window, "Slash", can_find);
	ev_window_set_action_sensitive (ev_window, "EditRotateLeft", has_pages);
	ev_window_set_action_sensitive (ev_window, "EditRotateRight", has_pages);

        /* View menu */
	ev_window_set_action_sensitive (ev_window, "ViewToolbar", !ev_window_is_editing_toolbar (ev_window));
	ev_window_set_action_sensitive (ev_window, "ViewContinuous", has_pages);
	ev_window_set_action_sensitive (ev_window, "ViewDual", has_pages);
	ev_window_set_action_sensitive (ev_window, "ViewDualOddLeft", has_pages);
	ev_window_set_action_sensitive (ev_window, "ViewBestFit", has_pages);
	ev_window_set_action_sensitive (ev_window, "ViewPageWidth", has_pages);
	ev_window_set_action_sensitive (ev_window, "ViewReload", has_pages);
	ev_window_set_action_sensitive (ev_window, "ViewAutoscroll", has_pages);
	ev_window_set_action_sensitive (ev_window, "ViewInvertedColors", has_pages);

	/* Bookmarks menu */
	ev_window_set_action_sensitive (ev_window, "BookmarksAdd",
					has_pages && ev_window->priv->bookmarks);

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
	EvSizingMode sizing_mode;

	if (ev_window->priv->document) {
		page = ev_document_model_get_page (ev_window->priv->model);
		n_pages = ev_document_get_n_pages (ev_window->priv->document);
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

	presentation_mode = EV_WINDOW_IS_PRESENTATION (ev_window);
	
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
		ev_window_set_action_sensitive (ev_window, "GoToPage", TRUE);
	} else {
  		ev_window_set_action_sensitive (ev_window, "GoFirstPage", FALSE);
		ev_window_set_action_sensitive (ev_window, "GoPreviousPage", FALSE);
		ev_window_set_action_sensitive (ev_window, "GoNextPage", FALSE);
		ev_window_set_action_sensitive (ev_window, "GoLastPage", FALSE);
		ev_window_set_action_sensitive (ev_window, "GoToPage", FALSE);
	}

	sizing_mode = ev_document_model_get_sizing_mode (ev_window->priv->model);
	if (has_pages && sizing_mode != EV_SIZING_FIT_WIDTH && sizing_mode != EV_SIZING_BEST_FIT) {
		GtkAction *action;
		float      zoom;
		float      real_zoom;

		action = gtk_action_group_get_action (ev_window->priv->action_group,
						      ZOOM_CONTROL_ACTION);

		real_zoom = ev_document_model_get_scale (ev_window->priv->model);
		real_zoom *= 72.0 / get_screen_dpi (ev_window);
		zoom = ephy_zoom_get_nearest_zoom_level (real_zoom);

		ephy_zoom_action_set_zoom_level (EPHY_ZOOM_ACTION (action), zoom);
	}
}

static const gchar *view_accels[] = {
	"PageDown",
	"PageUp",
	"Space",
	"ShiftSpace",
	"BackSpace",
	"ShiftBackSpace",
	"Return",
	"ShiftReturn",
	"Plus",
	"Minus",
	"KpPlus",
	"KpMinus",
	"Equal",
	"n",
	"p",
	"BestFit",
	"PageWidth"
};

static void
ev_window_set_view_accels_sensitivity (EvWindow *window, gboolean sensitive)
{
	gboolean can_find;
	gint     i;

	if (!window->priv->action_group)
		return;

	for (i = 0; i < G_N_ELEMENTS (view_accels); i++)
		ev_window_set_action_sensitive (window, view_accels[i], sensitive);

	can_find = window->priv->document && EV_IS_DOCUMENT_FIND (window->priv->document);
	ev_window_set_action_sensitive (window, "Slash", sensitive && can_find);
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

	presentation = EV_WINDOW_IS_PRESENTATION (window);
	fullscreen = ev_document_model_get_fullscreen (priv->model);
	fullscreen_mode = fullscreen || presentation;

	menubar = (priv->chrome & EV_CHROME_MENUBAR) != 0 && !fullscreen_mode;
	toolbar = ((priv->chrome & EV_CHROME_TOOLBAR) != 0  || 
		   (priv->chrome & EV_CHROME_RAISE_TOOLBAR) != 0) && !fullscreen_mode;
	fullscreen_toolbar = ((priv->chrome & EV_CHROME_FULLSCREEN_TOOLBAR) != 0 || 
			      (priv->chrome & EV_CHROME_RAISE_TOOLBAR) != 0) && fullscreen;
	findbar = (priv->chrome & EV_CHROME_FINDBAR) != 0;
	sidebar = (priv->chrome & EV_CHROME_SIDEBAR) != 0 && priv->document && !presentation;

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

	switch (ev_document_model_get_sizing_mode (window->priv->model)) {
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

static void
update_chrome_actions (EvWindow *window)
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

typedef struct _PageTitleData {
	const gchar *page_label;
	gchar       *page_title;
} PageTitleData;

static gboolean
ev_window_find_page_title (GtkTreeModel  *tree_model,
			   GtkTreePath   *path,
			   GtkTreeIter   *iter,
			   PageTitleData *data)
{
	gchar *page_string;
	
	gtk_tree_model_get (tree_model, iter,
			    EV_DOCUMENT_LINKS_COLUMN_PAGE_LABEL, &page_string, 
			    -1);
	
	if (!page_string)
		return FALSE;
	
	if (!strcmp (page_string, data->page_label)) {
		gtk_tree_model_get (tree_model, iter,
				    EV_DOCUMENT_LINKS_COLUMN_MARKUP, &data->page_title, 
				    -1);
		g_free (page_string);
		return TRUE;
	}
	
	g_free (page_string);
	return FALSE;
}

static gchar *
ev_window_get_page_title (EvWindow    *window,
			  const gchar *page_label)
{
	if (EV_IS_DOCUMENT_LINKS (window->priv->document) &&
	    ev_document_links_has_document_links (EV_DOCUMENT_LINKS (window->priv->document))) {
		PageTitleData data;
		GtkTreeModel *model;

		data.page_label = page_label;
		data.page_title = NULL;

		g_object_get (G_OBJECT (window->priv->sidebar_links),
			      "model", &model,
			      NULL);
		if (model) {
			gtk_tree_model_foreach (model,
						(GtkTreeModelForeachFunc)ev_window_find_page_title,
						&data);

			g_object_unref (model);
		}

		return data.page_title;
	}

	return NULL;
}

static void
ev_window_add_history (EvWindow *window, gint page, EvLink *link)
{
	gchar *page_label = NULL;
	gchar *page_title;
	gchar *link_title;
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
		page_label = ev_document_links_get_dest_page_label (EV_DOCUMENT_LINKS (window->priv->document), dest);
	} else {
		dest = ev_link_dest_new_page (page);
		action = ev_link_action_new_dest (dest);
		page_label = ev_document_get_page_label (window->priv->document, page);
	}

	if (!page_label)
		return;

	page_title = ev_window_get_page_title (window, page_label);
	if (page_title) {
		link_title = g_strdup_printf (_("Page %s — %s"), page_label, page_title);
		g_free (page_title);
	} else {
		link_title = g_strdup_printf (_("Page %s"), page_label);
	}

	real_link = ev_link_new (link_title, action);
	
	ev_history_add_link (window->priv->history, real_link);

	g_free (link_title);
	g_free (page_label);
	g_object_unref (real_link);
}

static void
view_handle_link_cb (EvView *view, EvLink *link, EvWindow *window)
{
	int current_page = ev_document_model_get_page (window->priv->model);
	
	ev_window_add_history (window, 0, link);
	ev_window_add_history (window, current_page, NULL);
}

static void
view_selection_changed_cb (EvView   *view,
			   EvWindow *window)
{
	ev_window_set_action_sensitive (window, "EditCopy",
					ev_view_get_has_selection (view));
}

static void
view_layers_changed_cb (EvView   *view,
			EvWindow *window)
{
	ev_sidebar_layers_update_layers_state (EV_SIDEBAR_LAYERS (window->priv->sidebar_layers));
}

static void
ev_window_page_changed_cb (EvWindow        *ev_window,
			   gint             old_page,
			   gint             new_page,
			   EvDocumentModel *model)
{
	ev_window_update_actions (ev_window);

	ev_window_update_find_status_message (ev_window);

	if (abs (new_page - old_page) > 1) {
		ev_window_add_history (ev_window, new_page, NULL);
		ev_window_add_history (ev_window, old_page, NULL);
	}

	if (ev_window->priv->metadata && !ev_window_is_empty (ev_window))
		ev_metadata_set_int (ev_window->priv->metadata, "page", new_page);
}

static const gchar *
ev_window_sidebar_get_current_page_id (EvWindow *ev_window)
{
	GtkWidget   *current_page;
	const gchar *id;

	g_object_get (ev_window->priv->sidebar,
		      "current_page", &current_page,
		      NULL);

	if (current_page == ev_window->priv->sidebar_links) {
		id = LINKS_SIDEBAR_ID;
	} else if (current_page == ev_window->priv->sidebar_thumbs) {
		id = THUMBNAILS_SIDEBAR_ID;
	} else if (current_page == ev_window->priv->sidebar_attachments) {
		id = ATTACHMENTS_SIDEBAR_ID;
	} else if (current_page == ev_window->priv->sidebar_layers) {
		id = LAYERS_SIDEBAR_ID;
	} else if (current_page == ev_window->priv->sidebar_annots) {
		id = ANNOTS_SIDEBAR_ID;
	} else if (current_page == ev_window->priv->sidebar_bookmarks) {
		id = BOOKMARKS_SIDEBAR_ID;
	} else {
		g_assert_not_reached();
	}

	g_object_unref (current_page);

	return id;
}

static void
ev_window_sidebar_set_current_page (EvWindow    *window,
				    const gchar *page_id)
{
	EvDocument *document = window->priv->document;
	EvSidebar  *sidebar = EV_SIDEBAR (window->priv->sidebar);
	GtkWidget  *links = window->priv->sidebar_links;
	GtkWidget  *thumbs = window->priv->sidebar_thumbs;
	GtkWidget  *attachments = window->priv->sidebar_attachments;
	GtkWidget  *annots = window->priv->sidebar_annots;
	GtkWidget  *layers = window->priv->sidebar_layers;
	GtkWidget  *bookmarks = window->priv->sidebar_bookmarks;

	if (strcmp (page_id, LINKS_SIDEBAR_ID) == 0 &&
	    ev_sidebar_page_support_document (EV_SIDEBAR_PAGE (links), document)) {
		ev_sidebar_set_page (sidebar, links);
	} else if (strcmp (page_id, THUMBNAILS_SIDEBAR_ID) == 0 &&
		   ev_sidebar_page_support_document (EV_SIDEBAR_PAGE (thumbs), document)) {
		ev_sidebar_set_page (sidebar, thumbs);
	} else if (strcmp (page_id, ATTACHMENTS_SIDEBAR_ID) == 0 &&
		   ev_sidebar_page_support_document (EV_SIDEBAR_PAGE (attachments), document)) {
		ev_sidebar_set_page (sidebar, attachments);
	} else if (strcmp (page_id, LAYERS_SIDEBAR_ID) == 0 &&
		   ev_sidebar_page_support_document (EV_SIDEBAR_PAGE (layers), document)) {
		ev_sidebar_set_page (sidebar, layers);
	} else if (strcmp (page_id, ANNOTS_SIDEBAR_ID) == 0 &&
		   ev_sidebar_page_support_document (EV_SIDEBAR_PAGE (annots), document)) {
		ev_sidebar_set_page (sidebar, annots);
	} else if (strcmp (page_id, BOOKMARKS_SIDEBAR_ID) == 0 &&
		   ev_sidebar_page_support_document (EV_SIDEBAR_PAGE (bookmarks), document)) {
		ev_sidebar_set_page (sidebar, bookmarks);
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
ev_window_init_metadata_with_default_values (EvWindow *window)
{
	GSettings  *settings = window->priv->default_settings;
	EvMetadata *metadata = window->priv->metadata;

	/* Chrome */
	if (!ev_metadata_has_key (metadata, "show_toolbar")) {
		ev_metadata_set_boolean (metadata, "show_toolbar",
					 g_settings_get_boolean (settings, "show-toolbar"));
	}
	if (!ev_metadata_has_key (metadata, "sidebar_visibility")) {
		ev_metadata_set_boolean (metadata, "sidebar_visibility",
					 g_settings_get_boolean (settings, "show-sidebar"));
	}

	/* Sidebar */
	if (!ev_metadata_has_key (metadata, "sidebar_size")) {
		ev_metadata_set_int (metadata, "sidebar_size",
				     g_settings_get_int (settings, "sidebar-size"));
	}
	if (!ev_metadata_has_key (metadata, "sidebar_page")) {
		gchar *sidebar_page_id = g_settings_get_string (settings, "sidebar-page");

		ev_metadata_set_string (metadata, "sidebar_page", sidebar_page_id);
		g_free (sidebar_page_id);
	}

	/* Document model */
	if (!ev_metadata_has_key (metadata, "continuous")) {
		ev_metadata_set_boolean (metadata, "continuous",
					 g_settings_get_boolean (settings, "continuous"));
	}
	if (!ev_metadata_has_key (metadata, "dual-page")) {
		ev_metadata_set_boolean (metadata, "dual-page",
					 g_settings_get_boolean (settings, "dual-page"));
	}
	if (!ev_metadata_has_key (metadata, "dual-page-odd-left")) {
		ev_metadata_set_boolean (metadata, "dual-page-odd-left",
					 g_settings_get_boolean (settings, "dual-page-odd-left"));
	}
	if (!ev_metadata_has_key (metadata, "inverted-colors")) {
		ev_metadata_set_boolean (metadata, "inverted-colors",
					 g_settings_get_boolean (settings, "inverted-colors"));
	}
	if (!ev_metadata_has_key (metadata, "sizing_mode")) {
		EvSizingMode mode = g_settings_get_enum (settings, "sizing-mode");
		GEnumValue *enum_value = g_enum_get_value (g_type_class_peek (EV_TYPE_SIZING_MODE), mode);

		ev_metadata_set_string (metadata, "sizing_mode", enum_value->value_nick);
	}

	if (!ev_metadata_has_key (metadata, "zoom")) {
		ev_metadata_set_double (metadata, "zoom",
					g_settings_get_double (settings, "zoom"));
	}

	if (!ev_metadata_has_key (metadata, "fullscreen")) {
		ev_metadata_set_boolean (metadata, "fullscreen",
					 g_settings_get_boolean (settings, "fullscreen"));
	}
}

static void
setup_chrome_from_metadata (EvWindow *window)
{
	gboolean show_toolbar;
	gboolean show_sidebar;

	if (!window->priv->metadata)
		return;

	if (ev_metadata_get_boolean (window->priv->metadata, "show_toolbar", &show_toolbar))
		update_chrome_flag (window, EV_CHROME_TOOLBAR, show_toolbar);
	if (ev_metadata_get_boolean (window->priv->metadata, "sidebar_visibility", &show_sidebar))
		update_chrome_flag (window, EV_CHROME_SIDEBAR, show_sidebar);
	update_chrome_visibility (window);
}

static void
setup_sidebar_from_metadata (EvWindow *window)
{
	gchar *page_id;
	gint   sidebar_size;

	if (!window->priv->metadata)
		return;

	if (ev_metadata_get_int (window->priv->metadata, "sidebar_size", &sidebar_size))
		gtk_paned_set_position (GTK_PANED (window->priv->hpaned), sidebar_size);

	if (ev_metadata_get_string (window->priv->metadata, "sidebar_page", &page_id))
		ev_window_sidebar_set_current_page (window, page_id);
}

static void
setup_model_from_metadata (EvWindow *window)
{
	gint     page;
	gchar   *sizing_mode;
	gdouble  zoom;
	gint     rotation;
	gboolean inverted_colors = FALSE;
	gboolean continuous = FALSE;
	gboolean dual_page = FALSE;
	gboolean dual_page_odd_left = FALSE;
	gboolean fullscreen = FALSE;

	if (!window->priv->metadata)
		return;

	/* Current page */
	if (!window->priv->dest &&
	    ev_metadata_get_int (window->priv->metadata, "page", &page)) {
		ev_document_model_set_page (window->priv->model, page);
	}

	/* Sizing mode */
	if (ev_metadata_get_string (window->priv->metadata, "sizing_mode", &sizing_mode)) {
		GEnumValue *enum_value;

		enum_value = g_enum_get_value_by_nick
			(g_type_class_peek (EV_TYPE_SIZING_MODE), sizing_mode);
		ev_document_model_set_sizing_mode (window->priv->model, enum_value->value);
	}

	/* Zoom */
	if (ev_document_model_get_sizing_mode (window->priv->model) == EV_SIZING_FREE) {
		if (ev_metadata_get_double (window->priv->metadata, "zoom", &zoom)) {
			zoom *= get_screen_dpi (window) / 72.0;
			ev_document_model_set_scale (window->priv->model, zoom);
		}
	}

	/* Rotation */
	if (ev_metadata_get_int (window->priv->metadata, "rotation", &rotation)) {
		switch (rotation) {
		case 90:
			rotation = 90;
			break;
		case 180:
			rotation = 180;
			break;
		case 270:
			rotation = 270;
			break;
		default:
			rotation = 0;
			break;
		}
		ev_document_model_set_rotation (window->priv->model, rotation);
	}

	/* Inverted Colors */
	if (ev_metadata_get_boolean (window->priv->metadata, "inverted-colors", &inverted_colors)) {
		ev_document_model_set_inverted_colors (window->priv->model, inverted_colors);
	}

	/* Continuous */
	if (ev_metadata_get_boolean (window->priv->metadata, "continuous", &continuous)) {
		ev_document_model_set_continuous (window->priv->model, continuous);
	}

	/* Dual page */
	if (ev_metadata_get_boolean (window->priv->metadata, "dual-page", &dual_page)) {
		ev_document_model_set_dual_page (window->priv->model, dual_page);
	}

	/* Dual page odd pages left */
	if (ev_metadata_get_boolean (window->priv->metadata, "dual-page-odd-left", &dual_page_odd_left)) {
		ev_document_model_set_dual_page_odd_pages_left (window->priv->model, dual_page_odd_left);
	}

	/* Fullscreen */
	if (ev_metadata_get_boolean (window->priv->metadata, "fullscreen", &fullscreen)) {
		if (fullscreen)
			ev_window_run_fullscreen (window);
	}
}

static void
setup_document_from_metadata (EvWindow *window)
{
	gint    page, n_pages;
	gint    width;
	gint    height;
	gdouble width_ratio;
	gdouble height_ratio;

	if (!window->priv->metadata)
		return;

	setup_sidebar_from_metadata (window);

	/* Make sure to not open a document on the last page,
	 * since closing it on the last page most likely means the
	 * user was finished reading the document. In that case, reopening should
	 * show the first page. */
	page = ev_document_model_get_page (window->priv->model);
	n_pages = ev_document_get_n_pages (window->priv->document);
	if (page == n_pages - 1)
		ev_document_model_set_page (window->priv->model, 0);

	if (ev_metadata_get_int (window->priv->metadata, "window_width", &width) &&
	    ev_metadata_get_int (window->priv->metadata, "window_height", &height))
		return; /* size was already set in setup_size_from_metadata */

	g_settings_get (window->priv->default_settings, "window-ratio", "(dd)", &width_ratio, &height_ratio);
	if (width_ratio > 0. && height_ratio > 0.) {
		gdouble    document_width;
		gdouble    document_height;
		GdkScreen *screen;
		gint       request_width;
		gint       request_height;

		ev_document_get_max_page_size (window->priv->document,
					       &document_width, &document_height);

		request_width = (gint)(width_ratio * document_width + 0.5);
		request_height = (gint)(height_ratio * document_height + 0.5);

		screen = gtk_window_get_screen (GTK_WINDOW (window));
		if (screen) {
			request_width = MIN (request_width, gdk_screen_get_width (screen));
			request_height = MIN (request_height, gdk_screen_get_height (screen));
		}

		if (request_width > 0 && request_height > 0) {
			gtk_window_resize (GTK_WINDOW (window),
					   request_width,
					   request_height);
		}
	}
}

static void
setup_size_from_metadata (EvWindow *window)
{
	gint     width;
	gint     height;
	gboolean maximized;
	gint     x;
	gint     y;

	if (!window->priv->metadata)
		return;

	if (ev_metadata_get_boolean (window->priv->metadata, "window_maximized", &maximized)) {
		if (maximized) {
			gtk_window_maximize (GTK_WINDOW (window));
			return;
		} else {
			gtk_window_unmaximize (GTK_WINDOW (window));
		}
	}

	if (ev_metadata_get_int (window->priv->metadata, "window_x", &x) &&
	    ev_metadata_get_int (window->priv->metadata, "window_y", &y)) {
		gtk_window_move (GTK_WINDOW (window), x, y);
	}

        if (ev_metadata_get_int (window->priv->metadata, "window_width", &width) &&
	    ev_metadata_get_int (window->priv->metadata, "window_height", &height)) {
		gtk_window_resize (GTK_WINDOW (window), width, height);
	}
}

static void
setup_view_from_metadata (EvWindow *window)
{
	gboolean presentation;

	if (!window->priv->metadata)
		return;

	/* Presentation */
	if (ev_metadata_get_boolean (window->priv->metadata, "presentation", &presentation)) {
		if (presentation)
			ev_window_run_presentation (window);
	}
}

static void
ev_window_setup_default (EvWindow *ev_window)
{
	EvDocumentModel *model = ev_window->priv->model;
	GSettings       *settings = ev_window->priv->default_settings;

	/* Chrome */
	update_chrome_flag (ev_window, EV_CHROME_TOOLBAR,
			    g_settings_get_boolean (settings, "show-toolbar"));
	update_chrome_flag (ev_window, EV_CHROME_SIDEBAR,
			    g_settings_get_boolean (settings, "show-sidebar"));
	update_chrome_visibility (ev_window);

	/* Sidebar */
	gtk_paned_set_position (GTK_PANED (ev_window->priv->hpaned),
				g_settings_get_int (settings, "sidebar-size"));

	/* Document model */
	ev_document_model_set_continuous (model, g_settings_get_boolean (settings, "continuous"));
	ev_document_model_set_dual_page (model, g_settings_get_boolean (settings, "dual-page"));
	ev_document_model_set_dual_page_odd_pages_left (model, g_settings_get_boolean (settings, "dual-page-odd-left"));
	ev_document_model_set_inverted_colors (model, g_settings_get_boolean (settings, "inverted-colors"));
	ev_document_model_set_sizing_mode (model, g_settings_get_enum (settings, "sizing-mode"));
	if (ev_document_model_get_sizing_mode (model) == EV_SIZING_FREE)
		ev_document_model_set_scale (model, g_settings_get_double (settings, "zoom"));
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
		if (ev_document_model_get_inverted_colors (ev_window->priv->model))
			ev_document_misc_invert_pixbuf (job->thumbnail);
		gtk_window_set_icon (GTK_WINDOW (ev_window),
				     job->thumbnail);
	}

	ev_window_clear_thumbnail_job (ev_window);
}

static void
ev_window_refresh_window_thumbnail (EvWindow *ev_window)
{
	gdouble page_width;
	gdouble scale;
	gint rotation;
	EvDocument *document = ev_window->priv->document;

	if (!document || ev_document_get_n_pages (document) <= 0 ||
	    !ev_document_check_dimensions (document)) {
		return;
	}

	ev_window_clear_thumbnail_job (ev_window);

	ev_document_get_page_size (document, 0, &page_width, NULL);
	scale = 128. / page_width;
	rotation = ev_document_model_get_rotation (ev_window->priv->model);

	ev_window->priv->thumbnail_job = ev_job_thumbnail_new (document, 0, rotation, scale);
	g_signal_connect (ev_window->priv->thumbnail_job, "finished",
			  G_CALLBACK (ev_window_set_icon_from_thumbnail),
			  ev_window);
	ev_job_scheduler_push_job (ev_window->priv->thumbnail_job, EV_JOB_PRIORITY_NONE);
}

static void
override_restrictions_changed (GSettings *settings,
			       gchar     *key,
			       EvWindow  *ev_window)
{
	ev_window_setup_action_sensitivity (ev_window);
}

#ifdef HAVE_DESKTOP_SCHEMAS
static void
lockdown_changed (GSettings   *lockdown,
		  const gchar *key,
		  EvWindow    *ev_window)
{
	ev_window_setup_action_sensitivity (ev_window);
}
#endif

static GSettings *
ev_window_ensure_settings (EvWindow *ev_window)
{
        EvWindowPrivate *priv = ev_window->priv;

        if (priv->settings != NULL)
                return priv->settings;

        priv->settings = g_settings_new (GS_SCHEMA_NAME);
        g_signal_connect (priv->settings,
                          "changed::"GS_OVERRIDE_RESTRICTIONS,
                          G_CALLBACK (override_restrictions_changed),
                          ev_window);
        return priv->settings;
}

static gboolean
ev_window_setup_document (EvWindow *ev_window)
{
	const EvDocumentInfo *info;
	EvDocument *document = ev_window->priv->document;
	GtkAction *action;

	ev_window->priv->setup_document_idle = 0;

	ev_window_refresh_window_thumbnail (ev_window);

	ev_window_set_page_mode (ev_window, PAGE_MODE_DOCUMENT);
	ev_window_title_set_document (ev_window->priv->title, document);
	ev_window_title_set_uri (ev_window->priv->title, ev_window->priv->uri);

        ev_window_ensure_settings (ev_window);

#ifdef HAVE_DESKTOP_SCHEMAS
	if (!ev_window->priv->lockdown_settings) {
		ev_window->priv->lockdown_settings = g_settings_new (GS_LOCKDOWN_SCHEMA_NAME);
		g_signal_connect (ev_window->priv->lockdown_settings,
				  "changed",
				  G_CALLBACK (lockdown_changed),
				  ev_window);
	}
#endif

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
	
	info = ev_document_get_info (document);
	update_document_mode (ev_window, info->mode);

	if (EV_IS_DOCUMENT_FIND (document)) {
		EvFindOptions options;

		options = ev_document_find_get_supported_options (EV_DOCUMENT_FIND (document));
		egg_find_bar_enable_case_sensitive (EGG_FIND_BAR (ev_window->priv->find_bar),
						    options & EV_FIND_CASE_SENSITIVE);
		egg_find_bar_enable_whole_words_only (EGG_FIND_BAR (ev_window->priv->find_bar),
						      options & EV_FIND_WHOLE_WORDS_ONLY);
	}

	if (EV_WINDOW_IS_PRESENTATION (ev_window))
		gtk_widget_grab_focus (ev_window->priv->presentation_view);
	else
		gtk_widget_grab_focus (ev_window->priv->view);

	return FALSE;
}

static void
ev_window_set_document (EvWindow *ev_window, EvDocument *document)
{
	if (ev_window->priv->document == document)
		return;

	if (ev_window->priv->document)
		g_object_unref (ev_window->priv->document);
	ev_window->priv->document = g_object_ref (document);

	ev_window_update_max_min_scale (ev_window);

	ev_window_set_message_area (ev_window, NULL);

	if (ev_document_get_n_pages (document) <= 0) {
		ev_window_warning_message (ev_window, "%s",
					   _("The document contains no pages"));
	} else if (!ev_document_check_dimensions (document)) {
		ev_window_warning_message (ev_window, "%s",
					   _("The document contains only empty pages"));
	}

	if (EV_WINDOW_IS_PRESENTATION (ev_window)) {
		gint current_page;

		current_page = ev_view_presentation_get_current_page (
			EV_VIEW_PRESENTATION (ev_window->priv->presentation_view));
		gtk_widget_destroy (ev_window->priv->presentation_view);
		ev_window->priv->presentation_view = NULL;

		/* Update the model with the current presentation page */
		ev_document_model_set_page (ev_window->priv->model, current_page);
		ev_window_run_presentation (ev_window);
	}

	if (ev_window->priv->setup_document_idle > 0)
		g_source_remove (ev_window->priv->setup_document_idle);

	ev_window->priv->setup_document_idle = g_idle_add ((GSourceFunc)ev_window_setup_document, ev_window);
}

static void
ev_window_document_changed (EvWindow *ev_window,
			    gpointer  user_data)
{
	if (ev_window->priv->settings &&
	    g_settings_get_boolean (ev_window->priv->settings, GS_AUTO_RELOAD))
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
		ev_document_model_set_document (ev_window->priv->model, document);

#ifdef ENABLE_DBUS
		ev_window_emit_doc_loaded (ev_window);
#endif
		setup_chrome_from_metadata (ev_window);
		update_chrome_actions (ev_window);
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

		if (ev_window->priv->search_string && EV_IS_DOCUMENT_FIND (document) &&
		    ev_window->priv->window_mode != EV_WINDOW_MODE_PRESENTATION) {
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

	if (ev_job_is_failed (job)) {
		ev_window_clear_reload_job (ev_window);
		ev_window->priv->in_reload = FALSE;
		if (ev_window->priv->dest) {
			g_object_unref (ev_window->priv->dest);
			ev_window->priv->dest = NULL;
		}

		return;
	}

	ev_document_model_set_document (ev_window->priv->model,
					job->document);
	if (ev_window->priv->dest) {
		ev_window_handle_link (ev_window, ev_window->priv->dest);
		/* Already unrefed by ev_link_action
		 * FIXME: link action should inc dest ref counting
		 * or not unref it at all
		 */
		ev_window->priv->dest = NULL;
	}

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
		char *base_name, *template;
                GFile *tmp_file;
                GError *err = NULL;

		/* We'd like to keep extension of source uri since
		 * it helps to resolve some mime types, say cbz.
                 */
		base_name = g_file_get_basename (source_file);
                template = g_strdup_printf ("document.XXXXXX-%s", base_name);
                g_free (base_name);

                tmp_file = ev_mkstemp_file (template, &err);
		g_free (template);
                if (tmp_file == NULL) {
                        ev_window_error_message (ev_window, err,
                                                 "%s", _("Failed to load remote file."));
                        g_error_free (err);
                        return;
                }

		ev_window->priv->local_uri = g_file_get_uri (tmp_file);
		g_object_unref (tmp_file);

		ev_job_load_set_uri (EV_JOB_LOAD (ev_window->priv->load_job),
				     ev_window->priv->local_uri);
	}

	ev_window_reset_progress_cancellable (ev_window);
	
	target_file = g_file_new_for_uri (ev_window->priv->local_uri);
	g_file_copy_async (source_file, target_file,
			   G_FILE_COPY_OVERWRITE,
			   G_PRIORITY_DEFAULT,
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

	ev_window->priv->window_mode = mode;

	if (ev_window->priv->uri)
		g_free (ev_window->priv->uri);
	ev_window->priv->uri = g_strdup (uri);

	if (ev_window->priv->metadata)
		g_object_unref (ev_window->priv->metadata);
	if (ev_window->priv->bookmarks)
		g_object_unref (ev_window->priv->bookmarks);

	source_file = g_file_new_for_uri (uri);
	if (ev_is_metadata_supported_for_file (source_file)) {
		ev_window->priv->metadata = ev_metadata_new (source_file);
		ev_window_init_metadata_with_default_values (ev_window);
	} else {
		ev_window->priv->metadata = NULL;
	}

	if (ev_window->priv->metadata) {
		ev_window->priv->bookmarks = ev_bookmarks_new (ev_window->priv->metadata);
		ev_sidebar_bookmarks_set_bookmarks (EV_SIDEBAR_BOOKMARKS (ev_window->priv->sidebar_bookmarks),
						    ev_window->priv->bookmarks);
		g_signal_connect_swapped (ev_window->priv->bookmarks, "changed",
					  G_CALLBACK (ev_window_setup_bookmarks),
					  ev_window);
	} else {
		ev_window->priv->bookmarks = NULL;
	}

	if (ev_window->priv->search_string)
		g_free (ev_window->priv->search_string);
	ev_window->priv->search_string = search_string ?
		g_strdup (search_string) : NULL;

	if (ev_window->priv->dest)
		g_object_unref (ev_window->priv->dest);
	ev_window->priv->dest = dest ? g_object_ref (dest) : NULL;

	setup_size_from_metadata (ev_window);
	setup_model_from_metadata (ev_window);
	ev_window_setup_bookmarks (ev_window);

	ev_window->priv->load_job = ev_job_load_new (uri);
	g_signal_connect (ev_window->priv->load_job,
			  "finished",
			  G_CALLBACK (ev_window_load_job_cb),
			  ev_window);

	if (!g_file_is_native (source_file) && !ev_window->priv->local_uri) {
		ev_window_load_file_remote (ev_window, source_file);
	} else {
		ev_view_set_loading (EV_VIEW (ev_window->priv->view), TRUE);
		g_object_unref (source_file);
		ev_job_scheduler_push_job (ev_window->priv->load_job, EV_JOB_PRIORITY_NONE);
	}
}

void
ev_window_open_document (EvWindow       *ev_window,
			 EvDocument     *document,
			 EvLinkDest     *dest,
			 EvWindowRunMode mode,
			 const gchar    *search_string)
{
	if (document == ev_window->priv->document)
		return;

	ev_window_close_dialogs (ev_window);
	ev_window_clear_load_job (ev_window);
	ev_window_clear_local_uri (ev_window);

	if (ev_window->priv->monitor) {
		g_object_unref (ev_window->priv->monitor);
		ev_window->priv->monitor = NULL;
	}

	if (ev_window->priv->uri)
		g_free (ev_window->priv->uri);
	ev_window->priv->uri = g_strdup (ev_document_get_uri (document));

	setup_size_from_metadata (ev_window);
	setup_model_from_metadata (ev_window);

	ev_document_model_set_document (ev_window->priv->model, document);

	setup_document_from_metadata (ev_window);
	setup_view_from_metadata (ev_window);

	if (dest) {
		EvLink *link;
		EvLinkAction *link_action;

		link_action = ev_link_action_new_dest (dest);
		link = ev_link_new (NULL, link_action);
		ev_view_handle_link (EV_VIEW (ev_window->priv->view), link);
		/* FIXME: link action should inc dest ref counting
		 * or not unref it at all
		 */
		g_object_ref (dest);
		g_object_unref (link);
	}

	switch (mode) {
	case EV_WINDOW_MODE_FULLSCREEN:
		ev_window_run_fullscreen (ev_window);
		break;
	case EV_WINDOW_MODE_PRESENTATION:
		ev_window_run_presentation (ev_window);
		break;
	default:
		break;
	}

	if (search_string && EV_IS_DOCUMENT_FIND (document) &&
	    mode != EV_WINDOW_MODE_PRESENTATION) {
		ev_window_cmd_edit_find (NULL, ev_window);
		egg_find_bar_set_search_string (EGG_FIND_BAR (ev_window->priv->find_bar),
						search_string);
	}

	/* Create a monitor for the document */
	ev_window->priv->monitor = ev_file_monitor_new (ev_window->priv->uri);
	g_signal_connect_swapped (ev_window->priv->monitor, "changed",
				  G_CALLBACK (ev_window_document_changed),
				  ev_window);
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
	ev_window_clear_reload_job (ev_window);
	ev_window->priv->in_reload = TRUE;

	if (ev_window->priv->dest)
		g_object_unref (ev_window->priv->dest);
	ev_window->priv->dest = dest ? g_object_ref (dest) : NULL;

	if (ev_window->priv->local_uri) {
		ev_window_reload_remote (ev_window);
	} else {
		ev_window_reload_local (ev_window);
	}
}

static const gchar *
get_settings_key_for_directory (GUserDirectory directory)
{
        switch (directory) {
                case G_USER_DIRECTORY_PICTURES:
                        return GS_LAST_PICTURES_DIRECTORY;
                case G_USER_DIRECTORY_DOCUMENTS:
                default:
                        return GS_LAST_DOCUMENT_DIRECTORY;
        }
}

static void
ev_window_file_chooser_restore_folder (EvWindow       *window,
                                       GtkFileChooser *file_chooser,
                                       const gchar    *uri,
                                       GUserDirectory  directory)
{
        const gchar *folder_uri, *dir;
        gchar *parent_uri = NULL;

        g_settings_get (ev_window_ensure_settings (window),
                        get_settings_key_for_directory (directory),
                        "m&s", &folder_uri);
        if (folder_uri == NULL && uri != NULL) {
                GFile *file, *parent;

                file = g_file_new_for_uri (uri);
                parent = g_file_get_parent (file);
                g_object_unref (file);
                if (parent) {
                        folder_uri = parent_uri = g_file_get_uri (parent);
                        g_object_unref (parent);
                }
        }

        if (folder_uri) {
                gtk_file_chooser_set_current_folder_uri (file_chooser, folder_uri);
        } else {
                dir = g_get_user_special_dir (directory);
                gtk_file_chooser_set_current_folder (file_chooser,
                                                     dir ? dir : g_get_home_dir ());
        }

        g_free (parent_uri);
}

static void
ev_window_file_chooser_save_folder (EvWindow       *window,
                                    GtkFileChooser *file_chooser,
                                    GUserDirectory  directory)
{
        gchar *uri, *folder;

        folder = gtk_file_chooser_get_current_folder (file_chooser);
        if (g_strcmp0 (folder, g_get_user_special_dir (directory)) == 0) {
                /* Store 'nothing' if the folder is the default one */
                uri = NULL;
        } else {
                uri = gtk_file_chooser_get_current_folder_uri (file_chooser);
        }
        g_free (folder);

        g_settings_set (ev_window_ensure_settings (window),
                        get_settings_key_for_directory (directory),
                        "ms", uri);
        g_free (uri);
}

static void
file_open_dialog_response_cb (GtkWidget *chooser,
			      gint       response_id,
			      EvWindow  *ev_window)
{
	if (response_id == GTK_RESPONSE_OK) {
		GSList *uris;

                ev_window_file_chooser_save_folder (ev_window, GTK_FILE_CHOOSER (chooser),
                                                    G_USER_DIRECTORY_DOCUMENTS);

		uris = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (chooser));

		ev_application_open_uri_list (EV_APP, uris,
					      gtk_window_get_screen (GTK_WINDOW (ev_window)),
					      gtk_get_current_event_time ());

		g_slist_foreach (uris, (GFunc)g_free, NULL);
		g_slist_free (uris);
	}

	gtk_widget_destroy (chooser);
}

static void
ev_window_cmd_file_open (GtkAction *action, EvWindow *window)
{
	GtkWidget   *chooser;

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

        ev_window_file_chooser_restore_folder (window, GTK_FILE_CHOOSER (chooser),
                                               NULL, G_USER_DIRECTORY_DOCUMENTS);

	g_signal_connect (chooser, "response",
			  G_CALLBACK (file_open_dialog_response_cb),
			  window);

	gtk_widget_show (chooser);
}

static void
ev_window_open_copy_at_dest (EvWindow   *window,
			     EvLinkDest *dest)
{
	EvWindow *new_window = EV_WINDOW (ev_window_new ());

	if (window->priv->metadata)
		new_window->priv->metadata = g_object_ref (window->priv->metadata);
	ev_window_open_document (new_window,
				 window->priv->document,
				 dest, 0, NULL);
	gtk_window_present (GTK_WINDOW (new_window));
}

static void
ev_window_cmd_file_open_copy (GtkAction *action, EvWindow *window)
{
	ev_window_open_copy_at_dest (window, NULL);
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
					 NULL, 0, NULL, gtk_get_current_event_time ());
}

static void
ev_window_open_recent_action_item_activated (EvOpenRecentAction *action,
					     const gchar        *uri,
					     EvWindow           *window)
{
	ev_application_open_uri_at_dest (EV_APP, uri,
					 gtk_window_get_screen (GTK_WINDOW (window)),
					 NULL, 0, NULL, gtk_get_current_event_time ());
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
ev_window_recent_action_connect_proxy_cb (GtkActionGroup *action_group,
                                          GtkAction *action,
                                          GtkWidget *proxy,
                                          gpointer data)
{
        GtkLabel *label;

        if (!GTK_IS_MENU_ITEM (proxy))
                return;

        label = GTK_LABEL (gtk_bin_get_child (GTK_BIN (proxy)));

        gtk_label_set_ellipsize (label, PANGO_ELLIPSIZE_MIDDLE);
        gtk_label_set_max_width_chars (label, MAX_RECENT_ITEM_LEN);
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
        g_signal_connect (ev_window->priv->recent_action_group, "connect-proxy",
                          G_CALLBACK (ev_window_recent_action_connect_proxy_cb), NULL);

	gtk_ui_manager_insert_action_group (ev_window->priv->ui_manager,
					    ev_window->priv->recent_action_group, -1);

	items = gtk_recent_manager_get_items (ev_window->priv->recent_manager);
	items = g_list_sort (items, (GCompareFunc) compare_recent_items);

	for (l = items; l && l->data; l = g_list_next (l)) {
		GtkRecentInfo *info;
		GtkAction     *action;
		gchar         *action_name;
		gchar         *label;
                const gchar   *mime_type;
                gchar         *content_type;
                GIcon         *icon = NULL;

		info = (GtkRecentInfo *) l->data;

		if (!gtk_recent_info_has_application (info, evince))
			continue;

		action_name = g_strdup_printf ("RecentFile%u", i++);
		label = ev_window_get_recent_file_label (
			n_items + 1, gtk_recent_info_get_display_name (info));

                mime_type = gtk_recent_info_get_mime_type (info);
                content_type = g_content_type_from_mime_type (mime_type);
                if (content_type != NULL) {
                        icon = g_content_type_get_icon (content_type);
                        g_free (content_type);
                }

		action = g_object_new (GTK_TYPE_ACTION,
				       "name", action_name,
				       "label", label,
                                       "gicon", icon,
                                       "always-show-image", TRUE,
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
                if (icon != NULL)
                        g_object_unref (icon);

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
	} else {
		ev_window_add_recent (window, EV_JOB_SAVE (job)->uri);
	}

	ev_window_clear_save_job (window);
}

static void
file_save_dialog_response_cb (GtkWidget *fc,
			      gint       response_id,
			      EvWindow  *ev_window)
{
	gchar *uri;

	if (response_id != GTK_RESPONSE_OK) {
		gtk_widget_destroy (fc);
		return;
	}

        ev_window_file_chooser_save_folder (ev_window, GTK_FILE_CHOOSER (fc),
                                            G_USER_DIRECTORY_DOCUMENTS);

	uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (fc));

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
	g_object_unref (file);
	g_free (base_name);

        ev_window_file_chooser_restore_folder (ev_window, GTK_FILE_CHOOSER (fc),
                                               ev_window->priv->uri,
                                               G_USER_DIRECTORY_DOCUMENTS);

	g_signal_connect (fc, "response",
			  G_CALLBACK (file_save_dialog_response_cb),
			  ev_window);

	gtk_widget_show (fc);
}

static void
ev_window_cmd_send_to (GtkAction *action,
		       EvWindow  *ev_window)
{
	GAppInfo   *app_info;
	gchar      *command;
	const char *uri;
	char       *unescaped_uri;
	GError     *error = NULL;

	uri = ev_window->priv->local_uri ? ev_window->priv->local_uri : ev_window->priv->uri;
	unescaped_uri = g_uri_unescape_string (uri, NULL);
	command = g_strdup_printf ("%s \"%s\"", nautilus_sendto, unescaped_uri);
	g_free (unescaped_uri);
	app_info = g_app_info_create_from_commandline (command, NULL, 0, &error);
	if (app_info) {
		GdkAppLaunchContext *context;
		GdkScreen           *screen;

		screen = gtk_window_get_screen (GTK_WINDOW (ev_window));
		context = gdk_display_get_app_launch_context (gdk_screen_get_display (screen));
		gdk_app_launch_context_set_screen (context, screen);
		gdk_app_launch_context_set_timestamp (context, gtk_get_current_event_time ());
		g_app_info_launch (app_info, NULL, G_APP_LAUNCH_CONTEXT (context), &error);
		g_object_unref (context);

		g_object_unref (app_info);
	}
	g_free (command);

	if (error) {
		ev_window_error_message (ev_window, error, "%s",
					 _("Could not send current document"));
		g_error_free (error);
	}
}

static void
ev_window_cmd_open_containing_folder (GtkAction *action, EvWindow *ev_window)
{
	GtkWidget *ev_window_widget;
	GFile *file;
	GFile *parent;

	ev_window_widget = GTK_WIDGET (ev_window);

	file = g_file_new_for_uri (ev_window->priv->uri);
	parent = g_file_get_parent (file);

	if (parent) {
		char *parent_uri;

		parent_uri = g_file_get_uri (parent);
		if (parent_uri) {
			GdkScreen *screen;
			guint32 timestamp;
			GError *error;

			screen = gtk_widget_get_screen (ev_window_widget);
			timestamp = gtk_get_current_event_time ();

			error = NULL;
			if (!gtk_show_uri (screen, parent_uri, timestamp, &error)) {
				ev_window_error_message (ev_window, error, _("Could not open the containing folder"));
				g_error_free (error);
			}

			g_free (parent_uri);
		}
	}

	if (file)
		g_object_unref (file);

	if (parent)
		g_object_unref (parent);
	
}

static GKeyFile *
get_print_settings_file (void)
{
	GKeyFile *print_settings_file;
	gchar    *filename;
        GError *error = NULL;

	print_settings_file = g_key_file_new ();

	filename = g_build_filename (ev_application_get_dot_dir (EV_APP, FALSE),
                                     EV_PRINT_SETTINGS_FILE, NULL);
        if (!g_key_file_load_from_file (print_settings_file,
                                        filename,
                                        G_KEY_FILE_KEEP_COMMENTS |
                                        G_KEY_FILE_KEEP_TRANSLATIONS,
                                        &error)) {

                /* Don't warn if the file simply doesn't exist */
                if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
			g_warning ("%s", error->message);

                g_error_free (error);
	}

	g_free (filename);

	return print_settings_file;
}

static void
save_print_setting_file (GKeyFile *key_file)
{
	gchar  *filename;
	gchar  *data;
	gsize  data_length;
	GError *error = NULL;

	filename = g_build_filename (ev_application_get_dot_dir (EV_APP, TRUE),
				     EV_PRINT_SETTINGS_FILE, NULL);
	data = g_key_file_to_data (key_file, &data_length, NULL);
	g_file_set_contents (filename, data, data_length, &error);
	if (error) {
		g_warning ("Failed to save print settings: %s", error->message);
		g_error_free (error);
	}
	g_free (data);
	g_free (filename);
}

static void
ev_window_save_print_settings (EvWindow         *window,
			       GtkPrintSettings *print_settings)
{
	GKeyFile *key_file;
	gint      i;

	key_file = get_print_settings_file ();
	gtk_print_settings_to_key_file (print_settings, key_file, EV_PRINT_SETTINGS_GROUP);

	/* Save print settings that are specific to the document */
	for (i = 0; i < G_N_ELEMENTS (document_print_settings); i++) {
		/* Remove it from global settings */
		g_key_file_remove_key (key_file, EV_PRINT_SETTINGS_GROUP,
				       document_print_settings[i], NULL);

		if (window->priv->metadata) {
			const gchar *value;

			value = gtk_print_settings_get (print_settings,
							document_print_settings[i]);
			ev_metadata_set_string (window->priv->metadata,
						document_print_settings[i], value);
		}
	}

	save_print_setting_file (key_file);
	g_key_file_free (key_file);
}

static void
ev_window_save_print_page_setup (EvWindow     *window,
				 GtkPageSetup *page_setup)
{
	GKeyFile *key_file;

	key_file = get_print_settings_file ();
	gtk_page_setup_to_key_file (page_setup, key_file, EV_PAGE_SETUP_GROUP);

	/* Do not save document settings in global file */
	g_key_file_remove_key (key_file, EV_PAGE_SETUP_GROUP,
			       "page-setup-orientation", NULL);
	g_key_file_remove_key (key_file, EV_PAGE_SETUP_GROUP,
			       "page-setup-margin-top", NULL);
	g_key_file_remove_key (key_file, EV_PAGE_SETUP_GROUP,
			       "page-setup-margin-bottom", NULL);
	g_key_file_remove_key (key_file, EV_PAGE_SETUP_GROUP,
			       "page-setup-margin-left", NULL);
	g_key_file_remove_key (key_file, EV_PAGE_SETUP_GROUP,
			       "page-setup-margin-right", NULL);

	save_print_setting_file (key_file);
	g_key_file_free (key_file);

	if (!window->priv->metadata)
		return;

	/* Save page setup options that are specific to the document */
	ev_metadata_set_int (window->priv->metadata, "page-setup-orientation",
			     gtk_page_setup_get_orientation (page_setup));
	ev_metadata_set_double (window->priv->metadata, "page-setup-margin-top",
				gtk_page_setup_get_top_margin (page_setup, GTK_UNIT_MM));
	ev_metadata_set_double (window->priv->metadata, "page-setup-margin-bottom",
				gtk_page_setup_get_bottom_margin (page_setup, GTK_UNIT_MM));
	ev_metadata_set_double (window->priv->metadata, "page-setup-margin-left",
				gtk_page_setup_get_left_margin (page_setup, GTK_UNIT_MM));
	ev_metadata_set_double (window->priv->metadata, "page-setup-margin-right",
				gtk_page_setup_get_right_margin (page_setup, GTK_UNIT_MM));
}

static void
ev_window_load_print_settings_from_metadata (EvWindow         *window,
					     GtkPrintSettings *print_settings)
{
	gint i;

	if (!window->priv->metadata)
		return;

	/* Load print setting that are specific to the document */
	for (i = 0; i < G_N_ELEMENTS (document_print_settings); i++) {
		gchar *value = NULL;

		ev_metadata_get_string (window->priv->metadata,
					document_print_settings[i], &value);
		gtk_print_settings_set (print_settings,
					document_print_settings[i], value);
	}
}

static void
ev_window_load_print_page_setup_from_metadata (EvWindow     *window,
					       GtkPageSetup *page_setup)
{
	gint          int_value;
	gdouble       double_value;
	GtkPaperSize *paper_size = gtk_page_setup_get_paper_size (page_setup);

	/* Load page setup options that are specific to the document */
	if (window->priv->metadata &&
	    ev_metadata_get_int (window->priv->metadata, "page-setup-orientation", &int_value)) {
		gtk_page_setup_set_orientation (page_setup, int_value);
	} else {
		gtk_page_setup_set_orientation (page_setup, GTK_PAGE_ORIENTATION_PORTRAIT);
	}

	if (window->priv->metadata &&
	    ev_metadata_get_double (window->priv->metadata, "page-setup-margin-top", &double_value)) {
		gtk_page_setup_set_top_margin (page_setup, double_value, GTK_UNIT_MM);
	} else {
		gtk_page_setup_set_top_margin (page_setup,
					       gtk_paper_size_get_default_top_margin (paper_size, GTK_UNIT_MM),
					       GTK_UNIT_MM);
	}

	if (window->priv->metadata &&
	    ev_metadata_get_double (window->priv->metadata, "page-setup-margin-bottom", &double_value)) {
		gtk_page_setup_set_bottom_margin (page_setup, double_value, GTK_UNIT_MM);
	} else {
		gtk_page_setup_set_bottom_margin (page_setup,
						  gtk_paper_size_get_default_bottom_margin (paper_size, GTK_UNIT_MM),
						  GTK_UNIT_MM);
	}

	if (window->priv->metadata &&
	    ev_metadata_get_double (window->priv->metadata, "page-setup-margin-left", &double_value)) {
		gtk_page_setup_set_left_margin (page_setup, double_value, GTK_UNIT_MM);
	} else {
		gtk_page_setup_set_left_margin (page_setup,
						gtk_paper_size_get_default_left_margin (paper_size, GTK_UNIT_MM),
						GTK_UNIT_MM);
	}

	if (window->priv->metadata &&
	    ev_metadata_get_double (window->priv->metadata, "page-setup-margin-right", &double_value)) {
		gtk_page_setup_set_right_margin (page_setup, double_value, GTK_UNIT_MM);
	} else {
		gtk_page_setup_set_right_margin (page_setup,
						 gtk_paper_size_get_default_right_margin (paper_size, GTK_UNIT_MM),
						 GTK_UNIT_MM);
	}
}

static GtkPrintSettings *
get_print_settings (GKeyFile *key_file)
{
	GtkPrintSettings *print_settings;

	print_settings = g_key_file_has_group (key_file, EV_PRINT_SETTINGS_GROUP) ?
		gtk_print_settings_new_from_key_file (key_file, EV_PRINT_SETTINGS_GROUP, NULL) :
		gtk_print_settings_new ();

	return print_settings ? print_settings : gtk_print_settings_new ();
}

static GtkPageSetup *
get_print_page_setup (GKeyFile *key_file)
{
	GtkPageSetup *page_setup;

	page_setup = g_key_file_has_group (key_file, EV_PAGE_SETUP_GROUP) ?
		gtk_page_setup_new_from_key_file (key_file, EV_PAGE_SETUP_GROUP, NULL) :
		gtk_page_setup_new ();

	return page_setup ? page_setup : gtk_page_setup_new ();
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
		ev_window_save_print_settings (ev_window, print_settings);

		if (ev_print_operation_get_embed_page_setup (op)) {
			GtkPageSetup *page_setup;

			page_setup = ev_print_operation_get_default_page_setup (op);
			ev_window_save_print_page_setup (ev_window, page_setup);
		}
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
	if (!ev_window->priv->print_queue)
		ev_window->priv->print_queue = g_queue_new ();

	g_queue_push_head (ev_window->priv->print_queue, op);
	ev_window_print_update_pending_jobs_message (ev_window,
						     g_queue_get_length (ev_window->priv->print_queue));
}

void
ev_window_print_range (EvWindow *ev_window,
		       gint      first_page,
		       gint      last_page)
{
	EvPrintOperation *op;
	GKeyFile         *print_settings_file;
	GtkPrintSettings *print_settings;
	GtkPageSetup     *print_page_setup;
	gint              current_page;
	gint              document_last_page;
	gboolean          embed_page_setup;

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

	current_page = ev_document_model_get_page (ev_window->priv->model);
	document_last_page = ev_document_get_n_pages (ev_window->priv->document);

	print_settings_file = get_print_settings_file ();

	print_settings = get_print_settings (print_settings_file);
	ev_window_load_print_settings_from_metadata (ev_window, print_settings);

	print_page_setup = get_print_page_setup (print_settings_file);
	ev_window_load_print_page_setup_from_metadata (ev_window, print_page_setup);

	if (first_page != 1 || last_page != document_last_page) {
		GtkPageRange range;

		/* Ranges in GtkPrint are 0 - N */
		range.start = first_page - 1;
		range.end = last_page - 1;

		gtk_print_settings_set_print_pages (print_settings,
						    GTK_PRINT_PAGES_RANGES);
		gtk_print_settings_set_page_ranges (print_settings,
						    &range, 1);
	}

	ev_print_operation_set_job_name (op, gtk_window_get_title (GTK_WINDOW (ev_window)));
	ev_print_operation_set_current_page (op, current_page);
	ev_print_operation_set_print_settings (op, print_settings);
	ev_print_operation_set_default_page_setup (op, print_page_setup);
	embed_page_setup = ev_window->priv->lockdown_settings ?
		!g_settings_get_boolean (ev_window->priv->lockdown_settings,
					 GS_LOCKDOWN_PRINT_SETUP) :
		TRUE;
	ev_print_operation_set_embed_page_setup (op, embed_page_setup);

	g_object_unref (print_settings);
	g_object_unref (print_page_setup);
	g_key_file_free (print_settings_file);

	ev_print_operation_run (op, GTK_WINDOW (ev_window));
}

static void
ev_window_print (EvWindow *window)
{
	ev_window_print_range (window, 1,
			       ev_document_get_n_pages (window->priv->document));
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
document_modified_confirmation_dialog_response (GtkDialog *dialog,
						gint       response,
						EvWindow  *ev_window)
{
	gtk_widget_destroy (GTK_WIDGET (dialog));

	switch (response) {
	case GTK_RESPONSE_YES:
		ev_window_cmd_save_as (NULL, ev_window);
		break;
	case GTK_RESPONSE_NO:
		gtk_widget_destroy (GTK_WIDGET (ev_window));
		break;
	case GTK_RESPONSE_CANCEL:
	default:
		break;
	}
}

static gboolean
ev_window_check_document_modified (EvWindow *ev_window)
{
	EvDocument  *document = ev_window->priv->document;
	GtkWidget   *dialog;
	gchar       *text, *markup;
	const gchar *secondary_text;

	if (!document)
		return FALSE;

	if (EV_IS_DOCUMENT_FORMS (document) &&
	    ev_document_forms_document_is_modified (EV_DOCUMENT_FORMS (document))) {
		secondary_text = _("Document contains form fields that have been filled out. "
				   "If you don't save a copy, changes will be permanently lost.");
	} else if (EV_IS_DOCUMENT_ANNOTATIONS (document) &&
		   ev_document_annotations_document_is_modified (EV_DOCUMENT_ANNOTATIONS (document))) {
		secondary_text = _("Document contains new or modified annotations. "
				   "If you don't save a copy, changes will be permanently lost.");
	} else {
		return FALSE;
	}


	text = g_markup_printf_escaped (_("Save a copy of document “%s” before closing?"),
					gtk_window_get_title (GTK_WINDOW (ev_window)));

	dialog = gtk_message_dialog_new (GTK_WINDOW (ev_window),
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_NONE,
					 NULL);

	markup = g_strdup_printf ("<b>%s</b>", text);
	g_free (text);

	gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), markup);
	g_free (markup);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  "%s", secondary_text);

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				_("Close _without Saving"),
				GTK_RESPONSE_NO,
				GTK_STOCK_CANCEL,
				GTK_RESPONSE_CANCEL,
				_("Save a _Copy"),
				GTK_RESPONSE_YES,
				NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_YES);
        gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                                 GTK_RESPONSE_YES,
                                                 GTK_RESPONSE_NO,
                                                 GTK_RESPONSE_CANCEL,
                                                 -1);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (document_modified_confirmation_dialog_response),
			  ev_window);
	gtk_widget_show (dialog);

	return TRUE;
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

static gboolean
ev_window_check_print_queue (EvWindow *ev_window)
{
	GtkWidget *dialog;
	gchar     *text, *markup;
	gint       n_print_jobs;

	n_print_jobs = ev_window->priv->print_queue ?
		g_queue_get_length (ev_window->priv->print_queue) : 0;

	if (n_print_jobs == 0)
		return FALSE;

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
		/* TRANS: the singular form is not really used as n_print_jobs > 1
 			  but some languages distinguish between different plurals forms,
			  so the ngettext is needed. */
		text = g_strdup_printf (ngettext("There is %d print job active. "
						 "Wait until print finishes before closing?",
						 "There are %d print jobs active. "
						 "Wait until print finishes before closing?",
						 n_print_jobs),
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

	return TRUE;
}

static gboolean
ev_window_close (EvWindow *ev_window)
{
	if (EV_WINDOW_IS_PRESENTATION (ev_window)) {
		gint current_page;

		/* Save current page */
		current_page = ev_view_presentation_get_current_page (
			EV_VIEW_PRESENTATION (ev_window->priv->presentation_view));
		ev_document_model_set_page (ev_window->priv->model, current_page);
	}

	if (ev_window_check_document_modified (ev_window))
		return FALSE;

	if (ev_window_check_print_queue (ev_window))
		return FALSE;

	return TRUE;
}

static void
ev_window_cmd_file_close_window (GtkAction *action, EvWindow *ev_window)
{
	if (ev_window_close (ev_window))
		gtk_widget_destroy (GTK_WIDGET (ev_window));
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
	ev_document_model_set_continuous (ev_window->priv->model, continuous);
}

static void
ev_window_cmd_dual (GtkAction *action, EvWindow *ev_window)
{
	gboolean dual_page;

	ev_window_stop_presentation (ev_window, TRUE);
	dual_page = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	ev_document_model_set_dual_page (ev_window->priv->model, dual_page);
}

static void
ev_window_cmd_dual_odd_pages_left (GtkAction *action, EvWindow *ev_window)
{
	gboolean dual_page_odd_left;

	ev_window_stop_presentation (ev_window, TRUE);
	dual_page_odd_left = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	ev_document_model_set_dual_page_odd_pages_left (ev_window->priv->model,
							dual_page_odd_left);
}

static void
ev_window_cmd_view_best_fit (GtkAction *action, EvWindow *ev_window)
{
	ev_window_stop_presentation (ev_window, TRUE);

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
		ev_document_model_set_sizing_mode (ev_window->priv->model, EV_SIZING_BEST_FIT);
	} else {
		ev_document_model_set_sizing_mode (ev_window->priv->model, EV_SIZING_FREE);
	}
	ev_window_update_actions (ev_window);
}

static void
ev_window_cmd_best_fit (GtkAction *action, EvWindow *ev_window)
{
	ev_document_model_set_sizing_mode (ev_window->priv->model, EV_SIZING_BEST_FIT);
}

static void
ev_window_cmd_view_page_width (GtkAction *action, EvWindow *ev_window)
{
	ev_window_stop_presentation (ev_window, TRUE);

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
		ev_document_model_set_sizing_mode (ev_window->priv->model, EV_SIZING_FIT_WIDTH);
	} else {
		ev_document_model_set_sizing_mode (ev_window->priv->model, EV_SIZING_FREE);
	}
	ev_window_update_actions (ev_window);
}

static void
ev_window_cmd_page_width (GtkAction *action, EvWindow *ev_window)
{
	ev_document_model_set_sizing_mode (ev_window->priv->model, EV_SIZING_FIT_WIDTH);
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
	if (ev_window->priv->document == NULL || !EV_IS_DOCUMENT_FIND (ev_window->priv->document)) {
		g_error ("Find action should be insensitive since document doesn't support find");
		return;
	}

	if (EV_WINDOW_IS_PRESENTATION (ev_window))
		return;

	update_chrome_flag (ev_window, EV_CHROME_FINDBAR, TRUE);
	update_chrome_visibility (ev_window);
	gtk_widget_grab_focus (ev_window->priv->find_bar);
}

static void
ev_window_cmd_edit_find_next (GtkAction *action, EvWindow *ev_window)
{
	if (EV_WINDOW_IS_PRESENTATION (ev_window))
		return;

	update_chrome_flag (ev_window, EV_CHROME_FINDBAR, TRUE);
	update_chrome_visibility (ev_window);
	gtk_widget_grab_focus (ev_window->priv->find_bar);
	ev_view_find_next (EV_VIEW (ev_window->priv->view));
}

static void
ev_window_cmd_edit_find_previous (GtkAction *action, EvWindow *ev_window)
{
	if (EV_WINDOW_IS_PRESENTATION (ev_window))
		return;

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
ev_window_sidebar_position_change_cb (GObject    *object,
				      GParamSpec *pspec,
				      EvWindow   *ev_window)
{
	if (ev_window->priv->metadata && !ev_window_is_empty (ev_window))
		ev_metadata_set_int (ev_window->priv->metadata, "sidebar_size",
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
				      ev_document_model_get_fullscreen (window->priv->model));
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
ev_window_run_fullscreen (EvWindow *window)
{
	gboolean fullscreen_window = TRUE;

	if (ev_document_model_get_fullscreen (window->priv->model))
		return;
	
	if (!window->priv->fullscreen_toolbar) {
		window->priv->fullscreen_toolbar =
			gtk_ui_manager_get_widget (window->priv->ui_manager,
						   "/FullscreenToolbar");

		gtk_widget_set_name (window->priv->fullscreen_toolbar,
				     "ev-fullscreen-toolbar");
		gtk_toolbar_set_style (GTK_TOOLBAR (window->priv->fullscreen_toolbar),
				       GTK_TOOLBAR_BOTH_HORIZ);
		fullscreen_toolbar_setup_item_properties (window->priv->ui_manager);

		gtk_box_pack_start (GTK_BOX (window->priv->main_box),
				    window->priv->fullscreen_toolbar,
				    FALSE, FALSE, 0);
		gtk_box_reorder_child (GTK_BOX (window->priv->main_box),
				       window->priv->fullscreen_toolbar, 1);
	}

	if (EV_WINDOW_IS_PRESENTATION (window)) {
		ev_window_stop_presentation (window, FALSE);
		fullscreen_window = FALSE;
	}

	g_object_set (G_OBJECT (window->priv->scrolled_window),
		      "shadow-type", GTK_SHADOW_NONE,
		      NULL);

	ev_document_model_set_fullscreen (window->priv->model, TRUE);
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

	if (window->priv->metadata && !ev_window_is_empty (window))
		ev_metadata_set_boolean (window->priv->metadata, "fullscreen", TRUE);
}

static void
ev_window_stop_fullscreen (EvWindow *window,
			   gboolean  unfullscreen_window)
{
	if (!ev_document_model_get_fullscreen (window->priv->model))
		return;

	g_object_set (G_OBJECT (window->priv->scrolled_window),
		      "shadow-type", GTK_SHADOW_IN,
		      NULL);

	ev_document_model_set_fullscreen (window->priv->model, FALSE);
	ev_window_update_fullscreen_action (window);
	update_chrome_flag (window, EV_CHROME_FULLSCREEN_TOOLBAR, FALSE);
	update_chrome_visibility (window);
	if (unfullscreen_window)
		gtk_window_unfullscreen (GTK_WINDOW (window));

	if (window->priv->metadata && !ev_window_is_empty (window))
		ev_metadata_set_boolean (window->priv->metadata, "fullscreen", FALSE);
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

static void
ev_window_inhibit_screensaver (EvWindow *window)
{
        EvWindowPrivate *priv = window->priv;

        if (priv->presentation_mode_inhibit_id != 0)
                return;

        priv->presentation_mode_inhibit_id =
                gtk_application_inhibit (GTK_APPLICATION (g_application_get_default ()),
                                         GTK_WINDOW (window),
                                         GTK_APPLICATION_INHIBIT_IDLE,
                                         _("Running in presentation mode"));
}


static void
ev_window_uninhibit_screensaver (EvWindow *window)
{
        EvWindowPrivate *priv = window->priv;

        if (priv->presentation_mode_inhibit_id == 0)
                return;

        gtk_application_uninhibit (GTK_APPLICATION (g_application_get_default ()),
                                   priv->presentation_mode_inhibit_id);
        priv->presentation_mode_inhibit_id = 0;
}

static void
ev_window_update_presentation_action (EvWindow *window)
{
	GtkAction *action;

	action = gtk_action_group_get_action (window->priv->action_group, "ViewPresentation");
	g_signal_handlers_block_by_func
		(action, G_CALLBACK (ev_window_cmd_view_presentation), window);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      EV_WINDOW_IS_PRESENTATION (window));
	g_signal_handlers_unblock_by_func
		(action, G_CALLBACK (ev_window_cmd_view_presentation), window);
}

static void
ev_window_view_presentation_finished (EvWindow *window)
{
	ev_window_stop_presentation (window, TRUE);
}

static gboolean
ev_window_view_presentation_focus_in (EvWindow *window)
{
        ev_window_inhibit_screensaver (window);

	return FALSE;
}

static gboolean
ev_window_view_presentation_focus_out (EvWindow *window)
{
        ev_window_uninhibit_screensaver (window);

	return FALSE;
}

static void
ev_window_run_presentation (EvWindow *window)
{
	gboolean fullscreen_window = TRUE;
	guint    current_page;
	guint    rotation;
	gboolean inverted_colors;

	if (EV_WINDOW_IS_PRESENTATION (window))
		return;

	if (ev_document_model_get_fullscreen (window->priv->model)) {
		ev_window_stop_fullscreen (window, FALSE);
		fullscreen_window = FALSE;
	}

	current_page = ev_document_model_get_page (window->priv->model);
	rotation = ev_document_model_get_rotation (window->priv->model);
	inverted_colors = ev_document_model_get_inverted_colors (window->priv->model);
	window->priv->presentation_view = ev_view_presentation_new (window->priv->document,
								    current_page,
								    rotation,
								    inverted_colors);
	g_signal_connect_swapped (window->priv->presentation_view, "finished",
				  G_CALLBACK (ev_window_view_presentation_finished),
				  window);
	g_signal_connect_swapped (window->priv->presentation_view, "external-link",
				  G_CALLBACK (view_external_link_cb),
				  window);
	g_signal_connect_swapped (window->priv->presentation_view, "focus-in-event",
				  G_CALLBACK (ev_window_view_presentation_focus_in),
				  window);
	g_signal_connect_swapped (window->priv->presentation_view, "focus-out-event",
				  G_CALLBACK (ev_window_view_presentation_focus_out),
				  window);

	gtk_box_pack_start (GTK_BOX (window->priv->main_box),
			    window->priv->presentation_view,
			    TRUE, TRUE, 0);

	gtk_widget_hide (window->priv->hpaned);
	ev_window_update_presentation_action (window);
	update_chrome_visibility (window);

	gtk_widget_grab_focus (window->priv->presentation_view);
	if (fullscreen_window)
		gtk_window_fullscreen (GTK_WINDOW (window));

	gtk_widget_show (window->priv->presentation_view);

        ev_window_inhibit_screensaver (window);

	if (window->priv->metadata && !ev_window_is_empty (window))
		ev_metadata_set_boolean (window->priv->metadata, "presentation", TRUE);
}

static void
ev_window_stop_presentation (EvWindow *window,
			     gboolean  unfullscreen_window)
{
	guint current_page;
	guint rotation;

	if (!EV_WINDOW_IS_PRESENTATION (window))
		return;

	current_page = ev_view_presentation_get_current_page (EV_VIEW_PRESENTATION (window->priv->presentation_view));
	ev_document_model_set_page (window->priv->model, current_page);
	rotation = ev_view_presentation_get_rotation (EV_VIEW_PRESENTATION (window->priv->presentation_view));
	ev_document_model_set_rotation (window->priv->model, rotation);

	gtk_container_remove (GTK_CONTAINER (window->priv->main_box),
			      window->priv->presentation_view);
	window->priv->presentation_view = NULL;

	gtk_widget_show (window->priv->hpaned);
	ev_window_update_presentation_action (window);
	update_chrome_visibility (window);
	if (unfullscreen_window)
		gtk_window_unfullscreen (GTK_WINDOW (window));

	gtk_widget_grab_focus (window->priv->view);

        ev_window_uninhibit_screensaver (window);

	if (window->priv->metadata && !ev_window_is_empty (window))
		ev_metadata_set_boolean (window->priv->metadata, "presentation", FALSE);
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
ev_window_update_max_min_scale (EvWindow *window)
{
	gdouble    dpi;
	GtkAction *action;
	gdouble    min_width, min_height;
	gdouble    width, height;
	gdouble    max_scale;
	gint       rotation = ev_document_model_get_rotation (window->priv->model);

	if (!window->priv->document)
		return;

	dpi = get_screen_dpi (window) / 72.0;

	ev_document_get_min_page_size (window->priv->document, &min_width, &min_height);
	width = (rotation == 0 || rotation == 180) ? min_width : min_height;
	height = (rotation == 0 || rotation == 180) ? min_height : min_width;
	max_scale = sqrt (PAGE_CACHE_SIZE / (width * dpi * 4 * height * dpi));

	action = gtk_action_group_get_action (window->priv->action_group,
					      ZOOM_CONTROL_ACTION);
	ephy_zoom_action_set_max_zoom_level (EPHY_ZOOM_ACTION (action), max_scale * dpi);

	ev_document_model_set_min_scale (window->priv->model, MIN_SCALE * dpi);
	ev_document_model_set_max_scale (window->priv->model, max_scale * dpi);
}

static void
ev_window_screen_changed (GtkWidget *widget,
			  GdkScreen *old_screen)
{
	EvWindow *window = EV_WINDOW (widget);
	GdkScreen *screen;

	screen = gtk_widget_get_screen (widget);
	if (screen == old_screen)
		return;

	ev_window_setup_gtk_settings (window);
	ev_window_update_max_min_scale (window);

	if (GTK_WIDGET_CLASS (ev_window_parent_class)->screen_changed) {
		GTK_WIDGET_CLASS (ev_window_parent_class)->screen_changed (widget, old_screen);
	}
}

static gboolean
ev_window_state_event (GtkWidget           *widget,
		       GdkEventWindowState *event)
{
	EvWindow *window = EV_WINDOW (widget);

	if (GTK_WIDGET_CLASS (ev_window_parent_class)->window_state_event) {
		GTK_WIDGET_CLASS (ev_window_parent_class)->window_state_event (widget, event);
	}

	if ((event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) == 0)
		return FALSE;

	if (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) {
		if (ev_document_model_get_fullscreen (window->priv->model) || EV_WINDOW_IS_PRESENTATION (window))
			return FALSE;
		
		ev_window_run_fullscreen (window);
	} else {
		if (ev_document_model_get_fullscreen (window->priv->model))
			ev_window_stop_fullscreen (window, FALSE);
		else if (EV_WINDOW_IS_PRESENTATION (window))
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
	gint rotation;

	if (EV_WINDOW_IS_PRESENTATION (ev_window)) {
		rotation = ev_view_presentation_get_rotation (EV_VIEW_PRESENTATION (ev_window->priv->presentation_view));
		ev_view_presentation_set_rotation (EV_VIEW_PRESENTATION (ev_window->priv->presentation_view),
						   rotation - 90);
	} else {
		rotation = ev_document_model_get_rotation (ev_window->priv->model);

		ev_document_model_set_rotation (ev_window->priv->model, rotation - 90);
	}
}

static void
ev_window_cmd_edit_rotate_right (GtkAction *action, EvWindow *ev_window)
{
	gint rotation;

	if (EV_WINDOW_IS_PRESENTATION (ev_window)) {
		rotation = ev_view_presentation_get_rotation (EV_VIEW_PRESENTATION (ev_window->priv->presentation_view));
		ev_view_presentation_set_rotation (EV_VIEW_PRESENTATION (ev_window->priv->presentation_view),
						   rotation + 90);
	} else {
		rotation = ev_document_model_get_rotation (ev_window->priv->model);

		ev_document_model_set_rotation (ev_window->priv->model, rotation + 90);
	}
}

static void
ev_window_cmd_view_inverted_colors (GtkAction *action, EvWindow *ev_window)
{
	gboolean inverted_colors = ev_document_model_get_inverted_colors (ev_window->priv->model);

	ev_document_model_set_inverted_colors (ev_window->priv->model, !inverted_colors);
}

static void
ev_window_cmd_edit_toolbar_cb (GtkDialog *dialog,
			       gint       response,
			       EvWindow  *ev_window)
{
	EggEditableToolbar *toolbar;
	gchar              *toolbars_file;

	if (response == GTK_RESPONSE_HELP) {
		GError *error = NULL;

		gtk_show_uri (gtk_window_get_screen (GTK_WINDOW (dialog)),
			      EV_HELP_TOOLBAR,
			      gtk_get_current_event_time (),
			      &error);

		if (error) {
			ev_window_error_message (ev_window, error,
						 "%s", _("There was an error displaying help"));
			g_error_free (error);
		}

		return;
	}

	toolbar = EGG_EDITABLE_TOOLBAR (ev_window->priv->toolbar);
        egg_editable_toolbar_set_edit_mode (toolbar, FALSE);
	ev_window_set_action_sensitive (ev_window, "ViewToolbar", TRUE);

	toolbars_file = g_build_filename (ev_application_get_dot_dir (EV_APP, TRUE),
					  "evince_toolbar.xml", NULL);
	egg_toolbars_model_save_toolbars (egg_editable_toolbar_get_model (toolbar),
					  toolbars_file, "1.0");
	g_free (toolbars_file);

        gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
ev_window_cmd_edit_toolbar (GtkAction *action, EvWindow *ev_window)
{
	GtkWidget          *dialog;
	GtkWidget          *editor;
	GtkWidget          *content_area;
	EggEditableToolbar *toolbar;

	dialog = gtk_dialog_new_with_buttons (_("Toolbar Editor"),
					      GTK_WINDOW (ev_window),
				              GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_CLOSE,
					      GTK_RESPONSE_CLOSE,
					      GTK_STOCK_HELP,
					      GTK_RESPONSE_HELP,
					      NULL);
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)), 5);
	gtk_box_set_spacing (GTK_BOX (content_area), 2);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 500, 400);

	toolbar = EGG_EDITABLE_TOOLBAR (ev_window->priv->toolbar);
	editor = egg_toolbar_editor_new (ev_window->priv->ui_manager,
					 egg_editable_toolbar_get_model (toolbar));

	gtk_container_set_border_width (GTK_CONTAINER (editor), 5);
	gtk_box_set_spacing (GTK_BOX (EGG_TOOLBAR_EDITOR (editor)), 5);

        gtk_box_pack_start (GTK_BOX (content_area), editor, TRUE, TRUE, 0);

	egg_editable_toolbar_set_edit_mode (toolbar, TRUE);
	ev_window_set_action_sensitive (ev_window, "ViewToolbar", FALSE);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (ev_window_cmd_edit_toolbar_cb),
			  ev_window);
	gtk_widget_show_all (dialog);
}

static void
ev_window_cmd_edit_save_settings (GtkAction *action, EvWindow *ev_window)
{
	EvWindowPrivate *priv = ev_window->priv;
	EvDocumentModel *model = priv->model;
	GSettings       *settings = priv->default_settings;
	EvSizingMode     sizing_mode;

	g_settings_set_boolean (settings, "continuous",
				ev_document_model_get_continuous (model));
	g_settings_set_boolean (settings, "dual-page",
				ev_document_model_get_dual_page (model));
	g_settings_set_boolean (settings, "dual-page-odd-left",
				ev_document_model_get_dual_page_odd_pages_left (model));
	g_settings_set_boolean (settings, "fullscreen",
				ev_document_model_get_fullscreen (model));
	g_settings_set_boolean (settings, "inverted-colors",
				ev_document_model_get_inverted_colors (model));
	sizing_mode = ev_document_model_get_sizing_mode (model);
	g_settings_set_enum (settings, "sizing-mode", sizing_mode);
	if (sizing_mode == EV_SIZING_FREE) {
		gdouble zoom = ev_document_model_get_scale (model);

		zoom *= 72.0 / get_screen_dpi (ev_window);
		g_settings_set_double (settings, "zoom", zoom);
	}
	g_settings_set_boolean (settings, "show-toolbar",
				gtk_widget_get_visible (priv->toolbar));
	g_settings_set_boolean (settings, "show-sidebar",
				gtk_widget_get_visible (priv->sidebar));
	g_settings_set_int (settings, "sidebar-size",
			    gtk_paned_get_position (GTK_PANED (priv->hpaned)));
	g_settings_set_string (settings, "sidebar-page",
			       ev_window_sidebar_get_current_page_id (ev_window));
	g_settings_apply (settings);
}

static void
ev_window_cmd_view_zoom_in (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

	ev_document_model_set_sizing_mode (ev_window->priv->model, EV_SIZING_FREE);
	ev_view_zoom_in (EV_VIEW (ev_window->priv->view));
}

static void
ev_window_cmd_view_zoom_out (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

	ev_document_model_set_sizing_mode (ev_window->priv->model, EV_SIZING_FREE);
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

	ev_document_model_set_page (ev_window->priv->model, 0);
}

static void
ev_window_cmd_go_last_page (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

	ev_document_model_set_page (ev_window->priv->model,
				    ev_document_get_n_pages (ev_window->priv->document) - 1);
}

static void
ev_window_cmd_go_forward (GtkAction *action, EvWindow *ev_window)
{
	int n_pages, current_page;
	
        g_return_if_fail (EV_IS_WINDOW (ev_window));

	n_pages = ev_document_get_n_pages (ev_window->priv->document);
	current_page = ev_document_model_get_page (ev_window->priv->model);
	
	if (current_page + 10 < n_pages) {
		ev_document_model_set_page (ev_window->priv->model, current_page + 10);
	}
}

static void
ev_window_cmd_go_backward (GtkAction *action, EvWindow *ev_window)
{
	int current_page;
	
        g_return_if_fail (EV_IS_WINDOW (ev_window));

	current_page = ev_document_model_get_page (ev_window->priv->model);
	
	if (current_page - 10 >= 0) {
		ev_document_model_set_page (ev_window->priv->model, current_page - 10);
	}
}

static void
ev_window_cmd_bookmark_activate (GtkAction *action,
				 EvWindow  *window)
{
	guint page = ev_bookmark_action_get_page (EV_BOOKMARK_ACTION (action));

	ev_document_model_set_page (window->priv->model, page);
}

static gint
compare_bookmarks (EvBookmark *a,
		   EvBookmark *b)
{
	return strcmp (a->title, b->title);
}

static void
ev_window_setup_bookmarks (EvWindow *window)
{
	GList *items, *l;

	if (!window->priv->bookmarks)
		return;

	if (window->priv->bookmarks_ui_id > 0) {
		gtk_ui_manager_remove_ui (window->priv->ui_manager,
					  window->priv->bookmarks_ui_id);
		gtk_ui_manager_ensure_update (window->priv->ui_manager);
	}
	window->priv->bookmarks_ui_id = gtk_ui_manager_new_merge_id (window->priv->ui_manager);

	if (window->priv->bookmarks_action_group) {
		gtk_ui_manager_remove_action_group (window->priv->ui_manager,
						    window->priv->bookmarks_action_group);
		g_object_unref (window->priv->bookmarks_action_group);
	}
	window->priv->bookmarks_action_group = gtk_action_group_new ("BookmarksActions");
	gtk_ui_manager_insert_action_group (window->priv->ui_manager,
					    window->priv->bookmarks_action_group, -1);

	items = ev_bookmarks_get_bookmarks (window->priv->bookmarks);
	items = g_list_sort (items, (GCompareFunc)compare_bookmarks);

	for (l = items; l && l->data; l = g_list_next (l)) {
		EvBookmark *bm = (EvBookmark *)l->data;
		GtkAction  *action;

		action = ev_bookmark_action_new (bm);
		g_signal_connect (action, "activate",
				  G_CALLBACK (ev_window_cmd_bookmark_activate),
				  window);
		gtk_action_group_add_action (window->priv->bookmarks_action_group,
					     action);

		gtk_ui_manager_add_ui (window->priv->ui_manager,
				       window->priv->bookmarks_ui_id,
				       "/MainMenu/BookmarksMenu/BookmarksItems",
				       gtk_action_get_label (action),
				       gtk_action_get_name (action),
				       GTK_UI_MANAGER_MENUITEM,
				       FALSE);

		g_object_unref (action);
	}

	g_list_free (items);
}

static void
ev_window_cmd_bookmarks_add (GtkAction *action,
			     EvWindow  *window)
{
	EvBookmark bm;
	gchar     *page_label;
	gchar     *page_title;

	bm.page = ev_document_model_get_page (window->priv->model);
	page_label = ev_document_get_page_label (window->priv->document, bm.page);
	page_title = ev_window_get_page_title (window, page_label);
	bm.title = page_title ? page_title : g_strdup_printf (_("Page %s"), page_label);
	g_free (page_label);

	/* EvBookmarks takes ownership of bookmark */
	ev_bookmarks_add (window->priv->bookmarks, &bm);
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
		      EV_HELP,
		      gtk_get_current_event_time (),
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

		fullscreen = ev_document_model_get_fullscreen (window->priv->model);

		if (fullscreen) {
			ev_window_stop_fullscreen (window, TRUE);
		} else if (EV_WINDOW_IS_PRESENTATION (window)) {
			ev_window_stop_presentation (window, TRUE);
			gtk_widget_grab_focus (window->priv->view);
		} else {
			gtk_widget_grab_focus (window->priv->view);
		}

		if (fullscreen && EV_WINDOW_IS_PRESENTATION (window))
			g_warning ("Both fullscreen and presentation set somehow");
	}
}

static void
save_sizing_mode (EvWindow *window)
{
	EvSizingMode mode;
	GEnumValue *enum_value;

	if (!window->priv->metadata || ev_window_is_empty (window))
		return;

	mode = ev_document_model_get_sizing_mode (window->priv->model);
	enum_value = g_enum_get_value (g_type_class_peek (EV_TYPE_SIZING_MODE), mode);
	ev_metadata_set_string (window->priv->metadata, "sizing_mode",
				enum_value->value_nick);
}

static void
ev_window_document_changed_cb (EvDocumentModel *model,
			       GParamSpec      *pspec,
			       EvWindow        *ev_window)
{
	ev_window_set_document (ev_window,
				ev_document_model_get_document (model));
}

static void
ev_window_sizing_mode_changed_cb (EvDocumentModel *model,
				  GParamSpec      *pspec,
		 		  EvWindow        *ev_window)
{
	EvSizingMode sizing_mode = ev_document_model_get_sizing_mode (model);

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
ev_window_zoom_changed_cb (EvDocumentModel *model, GParamSpec *pspec, EvWindow *ev_window)
{
        ev_window_update_actions (ev_window);

	if (!ev_window->priv->metadata)
		return;

	if (ev_document_model_get_sizing_mode (model) == EV_SIZING_FREE && !ev_window_is_empty (ev_window)) {
		gdouble zoom;

		zoom = ev_document_model_get_scale (model);
		zoom *= 72.0 / get_screen_dpi (ev_window);
		ev_metadata_set_double (ev_window->priv->metadata, "zoom", zoom);
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
				      ev_document_model_get_continuous (window->priv->model));
	g_signal_handlers_unblock_by_func
		(action, G_CALLBACK (ev_window_cmd_continuous), window);
}

static void
ev_window_continuous_changed_cb (EvDocumentModel *model,
				 GParamSpec      *pspec,
				 EvWindow        *ev_window)
{
	ev_window_update_continuous_action (ev_window);

	if (ev_window->priv->metadata && !ev_window_is_empty (ev_window))
		ev_metadata_set_boolean (ev_window->priv->metadata, "continuous",
					 ev_document_model_get_continuous (model));
}

static void
ev_window_rotation_changed_cb (EvDocumentModel *model,
			       GParamSpec      *pspec,
			       EvWindow        *window)
{
	gint rotation = ev_document_model_get_rotation (model);

	if (window->priv->metadata && !ev_window_is_empty (window))
		ev_metadata_set_int (window->priv->metadata, "rotation",
				     rotation);

	ev_window_update_max_min_scale (window);
	ev_window_refresh_window_thumbnail (window);
}

static void
ev_window_update_inverted_colors_action (EvWindow *window)
{
	GtkAction *action;

	action = gtk_action_group_get_action (window->priv->action_group, "ViewInvertedColors");
	g_signal_handlers_block_by_func
		(action, G_CALLBACK (ev_window_cmd_view_inverted_colors), window);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      ev_document_model_get_inverted_colors (window->priv->model));
	g_signal_handlers_unblock_by_func
		(action, G_CALLBACK (ev_window_cmd_view_inverted_colors), window);
}

static void
ev_window_inverted_colors_changed_cb (EvDocumentModel *model,
			              GParamSpec      *pspec,
			              EvWindow        *window)
{
	gboolean inverted_colors = ev_document_model_get_inverted_colors (model);

	ev_window_update_inverted_colors_action (window);

	if (window->priv->metadata && !ev_window_is_empty (window))
		ev_metadata_set_boolean (window->priv->metadata, "inverted-colors",
					 inverted_colors);

	ev_window_refresh_window_thumbnail (window);
}

static void
ev_window_update_dual_page_action (EvWindow *window)
{
	GtkAction *action;

	action = gtk_action_group_get_action (window->priv->action_group, "ViewDual");
	g_signal_handlers_block_by_func
		(action, G_CALLBACK (ev_window_cmd_dual), window);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      ev_document_model_get_dual_page (window->priv->model));
	g_signal_handlers_unblock_by_func
		(action, G_CALLBACK (ev_window_cmd_dual), window);
}

static void
ev_window_dual_mode_changed_cb (EvDocumentModel *model,
				GParamSpec      *pspec,
				EvWindow        *ev_window)
{
	ev_window_update_dual_page_action (ev_window);

	if (ev_window->priv->metadata && !ev_window_is_empty (ev_window))
		ev_metadata_set_boolean (ev_window->priv->metadata, "dual-page",
					 ev_document_model_get_dual_page (model));
}

static void
ev_window_update_dual_page_odd_pages_left_action (EvWindow *window)
{
	GtkAction *action;

	action = gtk_action_group_get_action (window->priv->action_group, "ViewDualOddLeft");
	g_signal_handlers_block_by_func
		(action, G_CALLBACK (ev_window_cmd_dual_odd_pages_left), window);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      ev_document_model_get_dual_page_odd_pages_left (window->priv->model));
	g_signal_handlers_unblock_by_func
		(action, G_CALLBACK (ev_window_cmd_dual_odd_pages_left), window);
}

static void
ev_window_dual_mode_odd_pages_left_changed_cb (EvDocumentModel *model,
					       GParamSpec      *pspec,
					       EvWindow        *ev_window)
{
	ev_window_update_dual_page_odd_pages_left_action (ev_window);

	if (ev_window->priv->metadata && !ev_window_is_empty (ev_window))
		ev_metadata_set_boolean (ev_window->priv->metadata, "dual-page-odd-left",
					 ev_document_model_get_dual_page_odd_pages_left (model));
}

static char *
build_comments_string (EvDocument *document)
{
	gchar *comments = NULL;
	EvDocumentBackendInfo info;

	if (document && ev_document_get_backend_info (document, &info)) {
		comments = g_strdup_printf (
			_("Document Viewer\nUsing %s (%s)"),
			info.name, info.version);
	} else {
		comments = g_strdup_printf (
			_("Document Viewer"));
	}

	return comments;
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
		"Phil Bull <philbull@gmail.com>",
		"Tiffany Antpolski <tiffany.antopolski@gmail.com>",
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
		   "51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA\n")
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

	comments = build_comments_string (ev_window->priv->document);

	gtk_show_about_dialog (
		GTK_WINDOW (ev_window),
		"name", _("Evince"),
		"version", VERSION,
		"copyright",
		_("© 1996–2012 The Evince authors"),
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
	if (ev_window->priv->metadata)
		ev_metadata_set_boolean (ev_window->priv->metadata, "show_toolbar", active);
}

static void
ev_window_view_sidebar_cb (GtkAction *action, EvWindow *ev_window)
{
	if (EV_WINDOW_IS_PRESENTATION (ev_window))
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
	if (ev_window->priv->metadata && !ev_window_is_empty (ev_window)) {
		ev_metadata_set_string (ev_window->priv->metadata,
					"sidebar_page",
					ev_window_sidebar_get_current_page_id (ev_window));
	}
}

static void
ev_window_sidebar_visibility_changed_cb (EvSidebar  *ev_sidebar,
					 GParamSpec *pspec,
					 EvWindow   *ev_window)
{
	GtkAction *action;

	action = gtk_action_group_get_action (ev_window->priv->action_group, "ViewSidebar");

	if (!EV_WINDOW_IS_PRESENTATION (ev_window)) {
		gboolean visible = gtk_widget_get_visible (GTK_WIDGET (ev_sidebar));

		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), visible);

		if (ev_window->priv->metadata)
			ev_metadata_set_boolean (ev_window->priv->metadata, "sidebar_visibility",
						 visible);
		if (!visible)
			gtk_widget_grab_focus (ev_window->priv->view);
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

static void
view_menu_annot_popup (EvWindow     *ev_window,
		       EvAnnotation *annot)
{
	GtkAction *action;
	gboolean   show_annot = FALSE;

	if (ev_window->priv->annot)
		g_object_unref (ev_window->priv->annot);
	ev_window->priv->annot = (annot) ? g_object_ref (annot) : NULL;

	action = gtk_action_group_get_action (ev_window->priv->view_popup_action_group,
					      "AnnotProperties");
	gtk_action_set_visible (action, (annot != NULL && EV_IS_ANNOTATION_MARKUP (annot)));

	if (annot && EV_IS_ANNOTATION_ATTACHMENT (annot)) {
		EvAttachment *attachment;

		attachment = ev_annotation_attachment_get_attachment (EV_ANNOTATION_ATTACHMENT (annot));
		if (attachment) {
			show_annot = TRUE;
			if (ev_window->priv->attach_list) {
				g_list_foreach (ev_window->priv->attach_list,
						(GFunc) g_object_unref, NULL);
				g_list_free (ev_window->priv->attach_list);
				ev_window->priv->attach_list = NULL;
			}
			ev_window->priv->attach_list =
				g_list_prepend (ev_window->priv->attach_list,
						g_object_ref (attachment));
		}
	}

	action = gtk_action_group_get_action (ev_window->priv->attachment_popup_action_group,
					      "OpenAttachment");
	gtk_action_set_visible (action, show_annot);

	action = gtk_action_group_get_action (ev_window->priv->attachment_popup_action_group,
					      "SaveAttachmentAs");
	gtk_action_set_visible (action, show_annot);
}

static gboolean
view_menu_popup_cb (EvView   *view,
		    GList    *items,
		    EvWindow *ev_window)
{
	GList   *l;
	gboolean has_link = FALSE;
	gboolean has_image = FALSE;
	gboolean has_annot = FALSE;

	for (l = items; l; l = g_list_next (l)) {
		if (EV_IS_LINK (l->data)) {
			view_menu_link_popup (ev_window, EV_LINK (l->data));
			has_link = TRUE;
		} else if (EV_IS_IMAGE (l->data)) {
			view_menu_image_popup (ev_window, EV_IMAGE (l->data));
			has_image = TRUE;
		} else if (EV_IS_ANNOTATION (l->data)) {
			view_menu_annot_popup (ev_window, EV_ANNOTATION (l->data));
			has_annot = TRUE;
		}
	}

	if (!has_link)
		view_menu_link_popup (ev_window, NULL);
	if (!has_image)
		view_menu_image_popup (ev_window, NULL);
	if (!has_annot)
		view_menu_annot_popup (ev_window, NULL);

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
		EvJobFind *job_find = EV_JOB_FIND (ev_window->priv->find_job);

		if (ev_job_find_has_results (job_find)) {
			gint n_results;

			n_results = ev_job_find_get_n_results (job_find,
							       ev_document_model_get_page (ev_window->priv->model));
			/* TRANS: Sometimes this could be better translated as
			   "%d hit(s) on this page".  Therefore this string
			   contains plural cases. */
			message = g_strdup_printf (ngettext ("%d found on this page",
							     "%d found on this page",
							     n_results),
						   n_results);
		} else {
			message = g_strdup (_("Not found"));
		}
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
ev_window_search_start (EvWindow *ev_window)
{
	EggFindBar *find_bar = EGG_FIND_BAR (ev_window->priv->find_bar);
	const char *search_string;

	if (!ev_window->priv->document || !EV_IS_DOCUMENT_FIND (ev_window->priv->document))
		return;

	search_string = egg_find_bar_get_search_string (find_bar);

	ev_window_clear_find_job (ev_window);
	if (search_string && search_string[0]) {
		EvFindOptions options = EV_FIND_DEFAULT;

		ev_window->priv->find_job = ev_job_find_new (ev_window->priv->document,
							     ev_document_model_get_page (ev_window->priv->model),
							     ev_document_get_n_pages (ev_window->priv->document),
							     search_string,
							     FALSE);

		if (egg_find_bar_get_case_sensitive (find_bar))
			options |= EV_FIND_CASE_SENSITIVE;
		if (egg_find_bar_get_whole_words_only (find_bar))
			options |= EV_FIND_WHOLE_WORDS_ONLY;
		ev_job_find_set_options (EV_JOB_FIND (ev_window->priv->find_job), options);

		ev_view_find_started (EV_VIEW (ev_window->priv->view), EV_JOB_FIND (ev_window->priv->find_job));

		g_signal_connect (ev_window->priv->find_job, "finished",
				  G_CALLBACK (ev_window_find_job_finished_cb),
				  ev_window);
		g_signal_connect (ev_window->priv->find_job, "updated",
				  G_CALLBACK (ev_window_find_job_updated_cb),
				  ev_window);
		ev_job_scheduler_push_job (ev_window->priv->find_job, EV_JOB_PRIORITY_NONE);
	} else {
		ev_window_update_actions (ev_window);
		egg_find_bar_set_status_text (find_bar, NULL);
		gtk_widget_queue_draw (GTK_WIDGET (ev_window->priv->view));
	}
}

static void
find_bar_search_changed_cb (EggFindBar *find_bar,
			    GParamSpec *param,
			    EvWindow   *ev_window)
{
	/* Either the string or case sensitivity could have changed. */
	ev_view_find_search_changed (EV_VIEW (ev_window->priv->view));
	ev_window_search_start (ev_window);
}

static void
find_bar_visibility_changed_cb (EggFindBar *find_bar,
				GParamSpec *param,
				EvWindow   *ev_window)
{
	gboolean visible;

	visible = gtk_widget_get_visible (GTK_WIDGET (find_bar));

	if (ev_window->priv->document &&
	    EV_IS_DOCUMENT_FIND (ev_window->priv->document)) {
		ev_view_find_set_highlight_search (EV_VIEW (ev_window->priv->view), visible);
		ev_window_update_actions (ev_window);

		if (visible)
			ev_window_search_start (ev_window);
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

	if (zoom == EPHY_ZOOM_BEST_FIT) {
		mode = EV_SIZING_BEST_FIT;
	} else if (zoom == EPHY_ZOOM_FIT_WIDTH) {
		mode = EV_SIZING_FIT_WIDTH;
	} else {
		mode = EV_SIZING_FREE;
	}

	ev_document_model_set_sizing_mode (ev_window->priv->model, mode);

	if (mode == EV_SIZING_FREE) {
		ev_document_model_set_scale (ev_window->priv->model,
					     zoom * get_screen_dpi (ev_window) / 72.0);
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

#ifdef ENABLE_DBUS
	if (priv->skeleton != NULL) {
                ev_window_emit_closed (window);

                g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (priv->skeleton));
                g_object_unref (priv->skeleton);
                priv->skeleton = NULL;
                g_free (priv->dbus_object_path);
                priv->dbus_object_path = NULL;
	}
#endif /* ENABLE_DBUS */

	if (priv->bookmarks) {
		g_object_unref (priv->bookmarks);
		priv->bookmarks = NULL;
	}

	if (priv->metadata) {
		g_object_unref (priv->metadata);
		priv->metadata = NULL;
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

	if (priv->bookmarks_action_group) {
		g_object_unref (priv->bookmarks_action_group);
		priv->bookmarks_action_group = NULL;
	}

	if (priv->recent_manager) {
		g_signal_handlers_disconnect_by_func (priv->recent_manager,
						      ev_window_setup_recent,
						      window);
		priv->recent_manager = NULL;
	}

	if (priv->settings) {
		g_object_unref (priv->settings);
		priv->settings = NULL;
	}

	if (priv->default_settings) {
		g_settings_apply (priv->default_settings);
		g_object_unref (priv->default_settings);
		priv->default_settings = NULL;
	}

	if (priv->lockdown_settings) {
		g_object_unref (priv->lockdown_settings);
		priv->lockdown_settings = NULL;
	}

	priv->recent_ui_id = 0;

	if (priv->model) {
		g_signal_handlers_disconnect_by_func (priv->model,
						      ev_window_page_changed_cb,
						      window);
		g_object_unref (priv->model);
		priv->model = NULL;
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

	if (priv->link) {
		g_object_unref (priv->link);
		priv->link = NULL;
	}

	if (priv->image) {
		g_object_unref (priv->image);
		priv->image = NULL;
	}

	if (priv->annot) {
		g_object_unref (priv->annot);
		priv->annot = NULL;
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
		if (gtk_widget_is_sensitive (priv->view))
			handled = gtk_widget_event (priv->view, (GdkEvent*) event);
		g_object_unref (priv->view);
	}

	if (!handled && !EV_WINDOW_IS_PRESENTATION (ev_window)) {
		guint modifier = event->state & gtk_accelerator_get_default_mod_mask ();

		if (priv->menubar_accel_keyval != 0 &&
		    event->keyval == priv->menubar_accel_keyval &&
		    modifier == priv->menubar_accel_modifier) {
			if (!gtk_widget_get_visible (priv->menubar)) {
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

static gboolean
ev_window_delete_event (GtkWidget   *widget,
			GdkEventAny *event)
{
	return !ev_window_close (EV_WINDOW (widget));
}

static void
ev_window_class_init (EvWindowClass *ev_window_class)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (ev_window_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (ev_window_class);

	g_object_class->dispose = ev_window_dispose;

	widget_class->delete_event = ev_window_delete_event;
	widget_class->key_press_event = ev_window_key_press_event;
	widget_class->screen_changed = ev_window_screen_changed;
	widget_class->window_state_event = ev_window_state_event;
	widget_class->drag_data_received = ev_window_drag_data_received;

	nautilus_sendto = g_find_program_in_path ("nautilus-sendto");

	g_type_class_add_private (g_object_class, sizeof (EvWindowPrivate));
}

/* Normal items */
static const GtkActionEntry entries[] = {
	{ "File", NULL, N_("_File") },
        { "Edit", NULL, N_("_Edit") },
	{ "View", NULL, N_("_View") },
        { "Go", NULL, N_("_Go") },
	{ "Bookmarks", NULL, N_("_Bookmarks") },
	{ "Help", NULL, N_("_Help") },

	/* File menu */
	{ "FileOpen", GTK_STOCK_OPEN, N_("_Open…"), "<control>O",
	  N_("Open an existing document"),
	  G_CALLBACK (ev_window_cmd_file_open) },
	{ "FileOpenCopy", NULL, N_("Op_en a Copy"), "<control>N",
	  N_("Open a copy of the current document in a new window"),
	  G_CALLBACK (ev_window_cmd_file_open_copy) },
       	{ "FileSaveAs", GTK_STOCK_SAVE_AS, N_("_Save a Copy…"), "<control>S",
	  N_("Save a copy of the current document"),
	  G_CALLBACK (ev_window_cmd_save_as) },
	{ "FileSendTo", EV_STOCK_SEND_TO, N_("Send _To..."), NULL,
	  N_("Send current document by mail, instant message..."),
	  G_CALLBACK (ev_window_cmd_send_to) },
	{ "FileOpenContainingFolder", GTK_STOCK_DIRECTORY, N_("Open Containing _Folder"), NULL,
	  N_("Show the folder which contains this file in the file manager"),
	  G_CALLBACK (ev_window_cmd_open_containing_folder) },
	{ "FilePrint", GTK_STOCK_PRINT, N_("_Print…"), "<control>P",
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
        { "EditFind", GTK_STOCK_FIND, N_("_Find…"), "<control>F",
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
	{ "EditSaveSettings", NULL, N_("Save Current Settings as _Default"), "<control>T", NULL,
	  G_CALLBACK (ev_window_cmd_edit_save_settings) },


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
        { "GoToPage", GTK_STOCK_GOTO_TOP, N_("Go to Pa_ge"),"<control>L",
          N_("Go to Page"),
          G_CALLBACK (ev_window_cmd_focus_page_selector) },

	/* Bookmarks menu */
	{ "BookmarksAdd", GTK_STOCK_ADD, N_("_Add Bookmark"), "<control>D",
	  N_("Add a bookmark for the current page"),
	  G_CALLBACK (ev_window_cmd_bookmarks_add) },

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
	{ "p", GTK_STOCK_GO_UP, "", "p", NULL,
	  G_CALLBACK (ev_window_cmd_go_previous_page) },
	{ "n", GTK_STOCK_GO_DOWN, "", "n", NULL,
	  G_CALLBACK (ev_window_cmd_go_next_page) },
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
	{ "BestFit", EV_STOCK_ZOOM_PAGE, NULL, "f", NULL,
	  G_CALLBACK (ev_window_cmd_best_fit) },
	{ "PageWidth", EV_STOCK_ZOOM_WIDTH, NULL, "w", NULL,
	  G_CALLBACK (ev_window_cmd_page_width) },
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
        { "ViewDual", EV_STOCK_VIEW_DUAL, N_("_Dual (Even pages left)"), NULL,
	  N_("Show two pages at once with even pages on the left"),
	  G_CALLBACK (ev_window_cmd_dual), FALSE },
	{ "ViewDualOddLeft", EV_STOCK_VIEW_DUAL, N_("Dual (_Odd pages left)"), NULL,
	  N_("Show two pages at once with odd pages on the left"),
	  G_CALLBACK (ev_window_cmd_dual_odd_pages_left), FALSE },
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
	{ "ViewInvertedColors", EV_STOCK_INVERTED_COLORS, N_("_Inverted Colors"), "<control>I",
	  N_("Show page contents with the colors inverted"),
	  G_CALLBACK (ev_window_cmd_view_inverted_colors) },

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
	{ "SaveImageAs", NULL, N_("_Save Image As…"), NULL,
	  NULL, G_CALLBACK (ev_view_popup_cmd_save_image_as) },
	{ "CopyImage", NULL, N_("Copy _Image"), NULL,
	  NULL, G_CALLBACK (ev_view_popup_cmd_copy_image) },
	{ "AnnotProperties", NULL, N_("Annotation Properties…"), NULL,
	  NULL, G_CALLBACK (ev_view_popup_cmd_annot_properties) }
};

static const GtkActionEntry attachment_popup_entries [] = {
	{ "OpenAttachment", GTK_STOCK_OPEN, N_("_Open Attachment"), NULL,
	  NULL, G_CALLBACK (ev_attachment_popup_cmd_open_attachment) },
	{ "SaveAttachmentAs", GTK_STOCK_SAVE_AS, N_("_Save Attachment As…"), NULL,
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
sidebar_annots_annot_activated_cb (EvSidebarAnnotations *sidebar_annots,
				   EvMapping            *annot_mapping,
				   EvWindow             *window)
{
	ev_view_focus_annotation (EV_VIEW (window->priv->view), annot_mapping);
}

static void
sidebar_annots_begin_annot_add (EvSidebarAnnotations *sidebar_annots,
				EvAnnotationType      annot_type,
				EvWindow             *window)
{
	ev_view_begin_add_annotation (EV_VIEW (window->priv->view), annot_type);
}

static void
view_annot_added (EvView       *view,
		  EvAnnotation *annot,
		  EvWindow     *window)
{
	ev_sidebar_annotations_annot_added (EV_SIDEBAR_ANNOTATIONS (window->priv->sidebar_annots),
					    annot);
}

static void
sidebar_annots_annot_add_cancelled (EvSidebarAnnotations *sidebar_annots,
				    EvWindow             *window)
{
	ev_view_cancel_add_annotation (EV_VIEW (window->priv->view));
}

static void
sidebar_bookmarks_add_bookmark (EvSidebarBookmarks *sidebar_bookmarks,
				EvWindow           *window)
{
	ev_window_cmd_bookmarks_add (NULL, window);
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
	ev_page_action_set_model (EV_PAGE_ACTION (action),
				  window->priv->model);
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
			       "stock_id", GTK_STOCK_GO_BACK,
			       /*translators: this is the history action*/
			       "tooltip", _("Move across visited pages"),
			       NULL);
	g_signal_connect (action, "activate_link",
			  G_CALLBACK (navigation_action_activate_link_cb), window);
	gtk_action_group_add_action (group, action);
	g_object_unref (action);

	action = g_object_new (EV_TYPE_OPEN_RECENT_ACTION,
			       "name", "FileOpenRecent",
			       "label", _("_Open…"),
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

	action = gtk_action_group_get_action (action_group, "FileOpenContainingFolder");
	/*translators: this is the label for toolbar button*/
	g_object_set (action, "short_label", _("Open Folder"), NULL);

	action = gtk_action_group_get_action (action_group, "FileSendTo");
	/*translators: this is the label for toolbar button*/
	g_object_set (action, "short_label", _("Send To"), NULL);
	gtk_action_set_visible (action, nautilus_sendto != NULL);

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
	ev_page_action_set_links_model (EV_PAGE_ACTION (action), model);
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
	ev_window_set_action_sensitive (window, "ViewToolbar",
					!ev_window_is_editing_toolbar (window));

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
		if (window->priv->metadata && !ev_window_is_empty (window))
			ev_metadata_set_boolean (window->priv->metadata, "window_maximized", maximized);
	}

	return FALSE;
}

static gboolean
window_configure_event_cb (EvWindow *window, GdkEventConfigure *event, gpointer dummy)
{
	GdkWindowState state;
	gdouble document_width, document_height;

	if (!window->priv->metadata)
		return FALSE;

	state = gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET (window)));

	if (!(state & GDK_WINDOW_STATE_FULLSCREEN)) {
		if (window->priv->document) {
			ev_document_get_max_page_size (window->priv->document,
						       &document_width, &document_height);
			g_settings_set (window->priv->default_settings, "window-ratio", "(dd)",
					(double)event->width / document_width,
					(double)event->height / document_height);

			ev_metadata_set_int (window->priv->metadata, "window_x", event->x);
			ev_metadata_set_int (window->priv->metadata, "window_y", event->y);
			ev_metadata_set_int (window->priv->metadata, "window_width", event->width);
			ev_metadata_set_int (window->priv->metadata, "window_height", event->height);
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
	GdkAppLaunchContext *context;
	GdkScreen *screen;
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

	screen = gtk_window_get_screen (GTK_WINDOW (window));
	context = gdk_display_get_app_launch_context (gdk_screen_get_display (screen));
	gdk_app_launch_context_set_screen (context, screen);
	gdk_app_launch_context_set_timestamp (context, gtk_get_current_event_time ());

	file_list.data = file;
	if (!g_app_info_launch (app_info, &file_list, G_APP_LAUNCH_CONTEXT (context), &error)) {
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
	GdkAppLaunchContext *context;
	GdkScreen *screen;

	screen = gtk_window_get_screen (GTK_WINDOW (window));
	context = gdk_display_get_app_launch_context (gdk_screen_get_display (screen));
	gdk_app_launch_context_set_screen (context, screen);
	gdk_app_launch_context_set_timestamp (context, gtk_get_current_event_time ());

	if (!g_strstr_len (uri, strlen (uri), "://") &&
	    !g_str_has_prefix (uri, "mailto:")) {
		gchar *new_uri;

		/* Not a valid uri, assume http if it starts with www */
		if (g_str_has_prefix (uri, "www.")) {
			new_uri = g_strdup_printf ("http://%s", uri);
		} else {
			GFile *file, *parent;

			file = g_file_new_for_uri (window->priv->uri);
			parent = g_file_get_parent (file);
			g_object_unref (file);
			if (parent) {
				gchar *parent_uri = g_file_get_uri (parent);

				new_uri = g_build_filename (parent_uri, uri, NULL);
				g_free (parent_uri);
				g_object_unref (parent);
			} else {
				new_uri = g_strdup_printf ("file:///%s", uri);
			}
		}
		ret = g_app_info_launch_default_for_uri (new_uri, G_APP_LAUNCH_CONTEXT (context), &error);
		g_free (new_uri);
	} else {
		ret = g_app_info_launch_default_for_uri (uri, G_APP_LAUNCH_CONTEXT (context), &error);
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
					 gtk_get_current_event_time ());

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
view_external_link_cb (EvWindow *window, EvLinkAction *action)
{
	switch (ev_link_action_get_action_type (action)) {
	        case EV_LINK_ACTION_TYPE_GOTO_DEST: {
			EvLinkDest *dest;
			
			dest = ev_link_action_get_dest (action);
			if (!dest)
				return;

			ev_window_open_copy_at_dest (window, dest);
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

	ev_window_open_copy_at_dest (window, dest);
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

static GFile *
create_file_from_uri_for_format (const gchar     *uri,
				 GdkPixbufFormat *format)
{
	GFile  *target_file;
	gchar **extensions;
	gchar  *uri_extension;
	gint    i;

	extensions = gdk_pixbuf_format_get_extensions (format);
	for (i = 0; extensions[i]; i++) {
		if (g_str_has_suffix (uri, extensions[i])) {
			g_strfreev (extensions);
			return g_file_new_for_uri (uri);
		}
	}

	uri_extension = g_strconcat (uri, ".", extensions[0], NULL);
	target_file = g_file_new_for_uri (uri_extension);
	g_free (uri_extension);
	g_strfreev (extensions);

	return target_file;
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
	gchar           *filename;
	gchar           *file_format;
	GdkPixbufFormat *format;
	GtkFileFilter   *filter;
	
	if (response_id != GTK_RESPONSE_OK) {
		gtk_widget_destroy (fc);
		return;
	}

	ev_window_file_chooser_save_folder (ev_window, GTK_FILE_CHOOSER (fc),
                                            G_USER_DIRECTORY_PICTURES);

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

	target_file = create_file_from_uri_for_format (uri, format);
	g_free (uri);

	is_native = g_file_is_native (target_file);
	if (is_native) {
		filename = g_file_get_path (target_file);
	} else {
                /* Create a temporary local file to save to */
                if (ev_mkstemp ("saveimage.XXXXXX", &filename, &error) == -1)
                        goto has_error;
	}

	ev_document_doc_mutex_lock ();
	pixbuf = ev_document_images_get_image (EV_DOCUMENT_IMAGES (ev_window->priv->document),
					       ev_window->priv->image);
	ev_document_doc_mutex_unlock ();

	file_format = gdk_pixbuf_format_get_name (format);
	gdk_pixbuf_save (pixbuf, filename, file_format, &error, NULL);
	g_free (file_format);
	g_object_unref (pixbuf);
	
    has_error:
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
	
        ev_window_file_chooser_restore_folder (window, GTK_FILE_CHOOSER (fc), NULL,
                                               G_USER_DIRECTORY_PICTURES);

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
ev_view_popup_cmd_annot_properties (GtkAction *action,
				    EvWindow  *window)
{
	const gchar                  *author;
	GdkRGBA                       rgba;
	gdouble                       opacity;
	gboolean                      popup_is_open;
	EvAnnotationPropertiesDialog *dialog;
	EvAnnotation                 *annot = window->priv->annot;
	EvAnnotationsSaveMask         mask = EV_ANNOTATIONS_SAVE_NONE;

	if (!annot)
		return;

	dialog = EV_ANNOTATION_PROPERTIES_DIALOG (ev_annotation_properties_dialog_new_with_annotation (window->priv->annot));
	if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_APPLY) {
		gtk_widget_destroy (GTK_WIDGET (dialog));

		return;
	}

	/* Set annotations changes */
	author = ev_annotation_properties_dialog_get_author (dialog);
	if (ev_annotation_markup_set_label (EV_ANNOTATION_MARKUP (annot), author))
		mask |= EV_ANNOTATIONS_SAVE_LABEL;

	ev_annotation_properties_dialog_get_rgba (dialog, &rgba);
	if (ev_annotation_set_rgba (annot, &rgba))
		mask |= EV_ANNOTATIONS_SAVE_COLOR;

	opacity = ev_annotation_properties_dialog_get_opacity (dialog);
	if (ev_annotation_markup_set_opacity (EV_ANNOTATION_MARKUP (annot), opacity))
		mask |= EV_ANNOTATIONS_SAVE_OPACITY;

	popup_is_open = ev_annotation_properties_dialog_get_popup_is_open (dialog);
	if (ev_annotation_markup_set_popup_is_open (EV_ANNOTATION_MARKUP (annot), popup_is_open))
		mask |= EV_ANNOTATIONS_SAVE_POPUP_IS_OPEN;

	if (EV_IS_ANNOTATION_TEXT (annot)) {
		EvAnnotationTextIcon icon;

		icon = ev_annotation_properties_dialog_get_text_icon (dialog);
		if (ev_annotation_text_set_icon (EV_ANNOTATION_TEXT (annot), icon))
			mask |= EV_ANNOTATIONS_SAVE_TEXT_ICON;
	}

	if (mask != EV_ANNOTATIONS_SAVE_NONE) {
		ev_document_doc_mutex_lock ();
		ev_document_annotations_save_annotation (EV_DOCUMENT_ANNOTATIONS (window->priv->document),
							 window->priv->annot,
							 mask);
		ev_document_doc_mutex_unlock ();

		/* FIXME: update annot region only */
		ev_view_reload (EV_VIEW (window->priv->view));
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
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
		
		ev_attachment_open (attachment, screen, gtk_get_current_event_time (), &error);

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

	ev_window_file_chooser_save_folder (ev_window, GTK_FILE_CHOOSER (fc),
                                            G_USER_DIRECTORY_DOCUMENTS);

	uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (fc));
	target_file = g_file_new_for_uri (uri);
	g_object_get (G_OBJECT (fc), "action", &fc_action, NULL);
	is_dir = (fc_action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
	is_native = g_file_is_native (target_file);
	
	for (l = ev_window->priv->attach_list; l && l->data; l = g_list_next (l)) {
		EvAttachment *attachment;
		GFile        *save_to = NULL;
		GError       *error = NULL;
		
		attachment = (EvAttachment *) l->data;

		if (is_native) {
			if (is_dir) {
				save_to = g_file_get_child (target_file,
                                    /* FIXMEchpe: file name encoding! */
							    ev_attachment_get_name (attachment));
			} else {
				save_to = g_object_ref (target_file);
			}
		} else {
			save_to = ev_mkstemp_file ("saveattachment.XXXXXX", &error);
		}

                if (save_to)
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

        ev_window_file_chooser_restore_folder (window, GTK_FILE_CHOOSER (fc), NULL,
                                               G_USER_DIRECTORY_DOCUMENTS);

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
		if (EV_WINDOW_IS_PRESENTATION (window))
			ev_view_presentation_previous_page (EV_VIEW_PRESENTATION (window->priv->presentation_view));
		else
			ev_window_cmd_go_previous_page (NULL, window);
	} else if (strcmp (key, "Next") == 0) {
		if (EV_WINDOW_IS_PRESENTATION (window))
			ev_view_presentation_next_page (EV_VIEW_PRESENTATION (window->priv->presentation_view));
		else
			ev_window_cmd_go_next_page (NULL, window);
	} else if (strcmp (key, "FastForward") == 0) {
		ev_window_cmd_go_last_page (NULL, window);
	} else if (strcmp (key, "Rewind") == 0) {
		ev_window_cmd_go_first_page (NULL, window);
	}
}

static EggToolbarsModel *
get_toolbars_model (void)
{
	EggToolbarsModel *toolbars_model;
	gchar            *toolbars_file;
	gint              i;

	toolbars_model = egg_toolbars_model_new ();

	toolbars_file = g_build_filename (ev_application_get_dot_dir (EV_APP, FALSE),
					  "evince_toolbar.xml", NULL);
	egg_toolbars_model_load_names_from_resource (toolbars_model, TOOLBAR_RESOURCE_PATH);

	if (!egg_toolbars_model_load_toolbars (toolbars_model, toolbars_file)) {
		egg_toolbars_model_load_toolbars_from_resource (toolbars_model, TOOLBAR_RESOURCE_PATH);
                goto skip_conversion;
	}

	/* Open item doesn't exist anymore,
	 * convert it to OpenRecent for compatibility
	 */
	for (i = 0; i < egg_toolbars_model_n_items (toolbars_model, 0); i++) {
		const gchar *item;

		item = egg_toolbars_model_item_nth (toolbars_model, 0, i);
		if (g_ascii_strcasecmp (item, "FileOpen") == 0) {
			egg_toolbars_model_remove_item (toolbars_model, 0, i);
			egg_toolbars_model_add_item (toolbars_model, 0, i,
						     "FileOpenRecent");
			egg_toolbars_model_save_toolbars (toolbars_model, toolbars_file, "1.0");
			break;
		}
	}

    skip_conversion:
	g_free (toolbars_file);

	egg_toolbars_model_set_flags (toolbars_model, 0, EGG_TB_MODEL_NOT_REMOVABLE);

	return toolbars_model;
}

#ifdef ENABLE_DBUS
static void
ev_window_sync_source (EvWindow     *window,
		       EvSourceLink *link)
{
	guint32		 timestamp;
	gchar		*uri_input;
	GFile		*input_gfile;

        if (window->priv->skeleton == NULL)
		return;

	timestamp = gtk_get_current_event_time ();
	if (g_path_is_absolute (link->filename)) {
		input_gfile = g_file_new_for_path (link->filename);
	} else {
		GFile *gfile, *parent_gfile;

		gfile = g_file_new_for_uri (window->priv->uri);
		parent_gfile = g_file_get_parent (gfile);

		/* parent_gfile should never be NULL */
		if (parent_gfile == NULL) {
			g_printerr ("Document URI is '/'\n");
			return;
		}

		input_gfile = g_file_get_child (parent_gfile, link->filename);
		g_object_unref (parent_gfile);
		g_object_unref (gfile);
	}

	uri_input = g_file_get_uri (input_gfile);
	g_object_unref (input_gfile);

        ev_evince_window_emit_sync_source (window->priv->skeleton,
                                           uri_input,
                                           g_variant_new ("(ii)", link->line, link->col),
                                           timestamp);
	g_free (uri_input);
}

static void
ev_window_emit_closed (EvWindow *window)
{
	if (window->priv->skeleton == NULL)
		return;

        ev_evince_window_emit_closed (window->priv->skeleton);

	/* If this is the last window call g_dbus_connection_flush_sync()
	 * to make sure the signal is emitted.
	 */
	if (ev_application_get_n_windows (EV_APP) == 1)
		g_dbus_connection_flush_sync (g_application_get_dbus_connection (g_application_get_default ()), NULL, NULL);
}

static void
ev_window_emit_doc_loaded (EvWindow *window)
{
        if (window->priv->skeleton == NULL)
                return;

        ev_evince_window_emit_document_loaded (window->priv->skeleton, window->priv->uri);
}

static gboolean
handle_sync_view_cb (EvEvinceWindow        *object,
		     GDBusMethodInvocation *invocation,
		     const gchar           *source_file,
		     GVariant              *source_point,
		     guint                  timestamp,
		     EvWindow              *window)
{
	if (window->priv->document && ev_document_has_synctex (window->priv->document)) {
		EvSourceLink link;

		link.filename = (char *) source_file;
		g_variant_get (source_point, "(ii)", &link.line, &link.col);
		ev_view_highlight_forward_search (EV_VIEW (window->priv->view), &link);
		gtk_window_present_with_time (GTK_WINDOW (window), timestamp);
	}

	ev_evince_window_complete_sync_view (object, invocation);

	return TRUE;
}
#endif /* ENABLE_DBUS */

static gboolean
_gtk_css_provider_load_from_resource (GtkCssProvider *provider,
				      const char     *resource_path,
				      GError        **error)
{
	GBytes  *data;
	gboolean retval;

	data = g_resources_lookup_data (resource_path, 0, error);
	if (!data)
		return FALSE;

	retval = gtk_css_provider_load_from_data (provider,
						  g_bytes_get_data (data, NULL),
						  g_bytes_get_size (data),
						  error);
	g_bytes_unref (data);

	return retval;
}

static void
ev_window_init (EvWindow *ev_window)
{
	GtkActionGroup *action_group;
	GtkAccelGroup *accel_group;
	GtkCssProvider *css_provider;
	GError *error = NULL;
	GtkWidget *sidebar_widget;
	GtkWidget *menuitem;
	EggToolbarsModel *toolbars_model;
	GObject *mpkeys;
#ifdef ENABLE_DBUS
	GDBusConnection *connection;
	static gint window_id = 0;
#endif

	g_signal_connect (ev_window, "configure_event",
			  G_CALLBACK (window_configure_event_cb), NULL);
	g_signal_connect (ev_window, "window_state_event",
			  G_CALLBACK (window_state_event_cb), NULL);

	ev_window->priv = EV_WINDOW_GET_PRIVATE (ev_window);

#ifdef ENABLE_DBUS
	connection = g_application_get_dbus_connection (g_application_get_default ());
        if (connection) {
                EvEvinceWindow *skeleton;

		ev_window->priv->dbus_object_path = g_strdup_printf (EV_WINDOW_DBUS_OBJECT_PATH, window_id++);

                skeleton = ev_evince_window_skeleton_new ();
                if (g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (skeleton),
                                                      connection,
                                                      ev_window->priv->dbus_object_path,
                                                      &error)) {
                        ev_window->priv->skeleton = skeleton;
			g_signal_connect (skeleton, "handle-sync-view",
					  G_CALLBACK (handle_sync_view_cb),
					  ev_window);
                } else {
                        g_printerr ("Failed to register bus object %s: %s\n",
				    ev_window->priv->dbus_object_path, error->message);
                        g_error_free (error);
			g_free (ev_window->priv->dbus_object_path);
			ev_window->priv->dbus_object_path = NULL;
			error = NULL;

                        g_object_unref (skeleton);
                        ev_window->priv->skeleton = NULL;
                }
        }
#endif /* ENABLE_DBUS */

	ev_window->priv->model = ev_document_model_new ();

	ev_window->priv->page_mode = PAGE_MODE_DOCUMENT;
	ev_window->priv->chrome = EV_CHROME_NORMAL;
        ev_window->priv->presentation_mode_inhibit_id = 0;
	ev_window->priv->title = ev_window_title_new (ev_window);

	ev_window->priv->main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
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

        gtk_ui_manager_add_ui_from_resource (ev_window->priv->ui_manager,
                                             "/org/gnome/evince/shell/ui/evince.xml",
                                             &error);
        g_assert_no_error (error);

	css_provider = gtk_css_provider_new ();
	_gtk_css_provider_load_from_resource (css_provider,
					      "/org/gnome/evince/shell/ui/evince.css",
					      &error);
	g_assert_no_error (error);
	gtk_style_context_add_provider_for_screen (gtk_widget_get_screen (GTK_WIDGET (ev_window)),
					GTK_STYLE_PROVIDER (css_provider),
					GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref (css_provider);

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
	menuitem = gtk_ui_manager_get_widget (ev_window->priv->ui_manager,
					      "/MainMenu/EditMenu/EditRotateLeftMenu");
	gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (menuitem), TRUE);
	menuitem = gtk_ui_manager_get_widget (ev_window->priv->ui_manager,
					      "/MainMenu/EditMenu/EditRotateRightMenu");
	gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (menuitem), TRUE);

	toolbars_model = get_toolbars_model ();
	ev_window->priv->toolbar = GTK_WIDGET
		(g_object_new (EGG_TYPE_EDITABLE_TOOLBAR,
			       "ui-manager", ev_window->priv->ui_manager,
			       "popup-path", "/ToolbarPopup",
			       "model", toolbars_model,
			       NULL));
	g_object_unref (toolbars_model);

	gtk_style_context_add_class
		(gtk_widget_get_style_context (GTK_WIDGET (ev_window->priv->toolbar)),
		 GTK_STYLE_CLASS_PRIMARY_TOOLBAR);

	egg_editable_toolbar_show (EGG_EDITABLE_TOOLBAR (ev_window->priv->toolbar),
				   "DefaultToolBar");
	gtk_box_pack_start (GTK_BOX (ev_window->priv->main_box),
			    ev_window->priv->toolbar,
			    FALSE, FALSE, 0);
	gtk_widget_show (ev_window->priv->toolbar);

	/* Add the main area */
	ev_window->priv->hpaned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	g_signal_connect (ev_window->priv->hpaned,
			  "notify::position",
			  G_CALLBACK (ev_window_sidebar_position_change_cb),
			  ev_window);
	
	gtk_paned_set_position (GTK_PANED (ev_window->priv->hpaned), SIDEBAR_DEFAULT_SIZE);
	gtk_box_pack_start (GTK_BOX (ev_window->priv->main_box), ev_window->priv->hpaned,
			    TRUE, TRUE, 0);
	gtk_widget_show (ev_window->priv->hpaned);
	
	ev_window->priv->sidebar = ev_sidebar_new ();
	ev_sidebar_set_model (EV_SIDEBAR (ev_window->priv->sidebar),
			      ev_window->priv->model);
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

	sidebar_widget = ev_sidebar_annotations_new ();
	ev_window->priv->sidebar_annots = sidebar_widget;
	g_signal_connect (sidebar_widget,
			  "annot_activated",
			  G_CALLBACK (sidebar_annots_annot_activated_cb),
			  ev_window);
	g_signal_connect (sidebar_widget,
			  "begin_annot_add",
			  G_CALLBACK (sidebar_annots_begin_annot_add),
			  ev_window);
	g_signal_connect (sidebar_widget,
			  "annot_add_cancelled",
			  G_CALLBACK (sidebar_annots_annot_add_cancelled),
			  ev_window);
	gtk_widget_show (sidebar_widget);
	ev_sidebar_add_page (EV_SIDEBAR (ev_window->priv->sidebar),
			     sidebar_widget);

	sidebar_widget = ev_sidebar_bookmarks_new ();
	ev_window->priv->sidebar_bookmarks = sidebar_widget;
	g_signal_connect (sidebar_widget,
			  "add-bookmark",
			  G_CALLBACK (sidebar_bookmarks_add_bookmark),
			  ev_window);
	gtk_widget_show (sidebar_widget);
	ev_sidebar_add_page (EV_SIDEBAR (ev_window->priv->sidebar),
			     sidebar_widget);

	ev_window->priv->view_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
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
	ev_view_set_page_cache_size (EV_VIEW (ev_window->priv->view), PAGE_CACHE_SIZE);
	ev_view_set_model (EV_VIEW (ev_window->priv->view), ev_window->priv->model);

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
	g_signal_connect_swapped (ev_window->priv->view, "external-link",
				  G_CALLBACK (view_external_link_cb),
				  ev_window);
	g_signal_connect_object (ev_window->priv->view, "handle-link",
			         G_CALLBACK (view_handle_link_cb),
			         ev_window, 0);
	g_signal_connect_object (ev_window->priv->view, "popup",
				 G_CALLBACK (view_menu_popup_cb),
				 ev_window, 0);
	g_signal_connect_object (ev_window->priv->view, "selection-changed",
				 G_CALLBACK (view_selection_changed_cb),
				 ev_window, 0);
	g_signal_connect_object (ev_window->priv->view, "annot-added",
				 G_CALLBACK (view_annot_added),
				 ev_window, 0);
	g_signal_connect_object (ev_window->priv->view, "layers-changed",
				 G_CALLBACK (view_layers_changed_cb),
				 ev_window, 0);
#ifdef ENABLE_DBUS
	g_signal_connect_swapped (ev_window->priv->view, "sync-source",
				  G_CALLBACK (ev_window_sync_source),
				  ev_window);
#endif
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

	/* Connect to model signals */
	g_signal_connect_swapped (ev_window->priv->model,
				  "page-changed",
				  G_CALLBACK (ev_window_page_changed_cb),
				  ev_window);
	g_signal_connect (ev_window->priv->model,
			  "notify::document",
			  G_CALLBACK (ev_window_document_changed_cb),
			  ev_window);
	g_signal_connect (ev_window->priv->model,
			  "notify::scale",
			  G_CALLBACK (ev_window_zoom_changed_cb),
			  ev_window);
	g_signal_connect (ev_window->priv->model,
			  "notify::sizing-mode",
			  G_CALLBACK (ev_window_sizing_mode_changed_cb),
			  ev_window);
	g_signal_connect (ev_window->priv->model,
			  "notify::rotation",
			  G_CALLBACK (ev_window_rotation_changed_cb),
			  ev_window);
	g_signal_connect (ev_window->priv->model,
			  "notify::continuous",
			  G_CALLBACK (ev_window_continuous_changed_cb),
			  ev_window);
	g_signal_connect (ev_window->priv->model,
			  "notify::dual-page",
			  G_CALLBACK (ev_window_dual_mode_changed_cb),
			  ev_window);
	g_signal_connect (ev_window->priv->model,
			  "notify::dual-odd-left",
			  G_CALLBACK (ev_window_dual_mode_odd_pages_left_changed_cb),
			  ev_window);
	g_signal_connect (ev_window->priv->model,
			  "notify::inverted-colors",
			  G_CALLBACK (ev_window_inverted_colors_changed_cb),
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
			  "notify::whole-words-only",
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

	ev_window->priv->default_settings = g_settings_new (GS_SCHEMA_NAME".Default");
	g_settings_delay (ev_window->priv->default_settings);
	ev_window_setup_default (ev_window);
	update_chrome_actions (ev_window);

	/* Set it user interface params */
	ev_window_setup_recent (ev_window);

	ev_window_setup_gtk_settings (ev_window);

	gtk_window_set_default_size (GTK_WINDOW (ev_window), 600, 600);

        ev_window_sizing_mode_changed_cb (ev_window->priv->model, NULL, ev_window);
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
                                              "application", g_application_get_default (),
					      NULL));

	return ev_window;
}

const gchar *
ev_window_get_dbus_object_path (EvWindow *ev_window)
{
#ifdef ENABLE_DBUS
	return ev_window->priv->dbus_object_path;
#else
	return NULL;
#endif
}
