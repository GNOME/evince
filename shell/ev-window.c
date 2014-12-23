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

#include "eggfindbar.h"
#include "ev-find-sidebar.h"

#include "ev-application.h"
#include "ev-document-factory.h"
#include "ev-document-find.h"
#include "ev-document-fonts.h"
#include "ev-document-images.h"
#include "ev-document-links.h"
#include "ev-document-annotations.h"
#include "ev-document-misc.h"
#include "ev-file-exporter.h"
#include "ev-file-helpers.h"
#include "ev-file-monitor.h"
#include "ev-history.h"
#include "ev-image.h"
#include "ev-job-scheduler.h"
#include "ev-jobs.h"
#include "ev-loading-message.h"
#include "ev-message-area.h"
#include "ev-metadata.h"
#include "ev-page-action-widget.h"
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
#include "ev-bookmark-action.h"
#include "ev-zoom-action.h"
#include "ev-toolbar.h"
#include "ev-bookmarks.h"
#include "ev-recent-view.h"

#ifdef ENABLE_DBUS
#include "ev-gdbus-generated.h"
#include "ev-media-player-keys.h"
#endif /* ENABLE_DBUS */

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#include <X11/extensions/XInput2.h>
#endif

typedef enum {
	PAGE_MODE_DOCUMENT,
	PAGE_MODE_PASSWORD
} EvWindowPageMode;

typedef enum {
        EV_CHROME_TOOLBAR            = 1 << 0,
        EV_CHROME_FINDBAR            = 1 << 1,
        EV_CHROME_RAISE_TOOLBAR      = 1 << 2,
        EV_CHROME_FULLSCREEN_TOOLBAR = 1 << 3,
        EV_CHROME_SIDEBAR            = 1 << 4,
        EV_CHROME_NORMAL             = EV_CHROME_TOOLBAR | EV_CHROME_SIDEBAR
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
	GtkWidget *toolbar;
	GtkWidget *hpaned;
	GtkWidget *view_box;
	GtkWidget *sidebar;
	GtkWidget *find_bar;
	GtkWidget *scrolled_window;
	GtkWidget *view;
	GtkWidget *loading_message;
	GtkWidget *presentation_view;
	GtkWidget *message_area;
	GtkWidget *password_view;
	GtkWidget *sidebar_thumbs;
	GtkWidget *sidebar_links;
	GtkWidget *find_sidebar;
	GtkWidget *sidebar_attachments;
	GtkWidget *sidebar_layers;
	GtkWidget *sidebar_annots;
	GtkWidget *sidebar_bookmarks;

	/* Settings */
	GSettings *settings;
	GSettings *default_settings;
	GSettings *lockdown_settings;

	/* Progress Messages */
	guint progress_idle;
	GCancellable *progress_cancellable;

	/* Fullscreen */
	GtkWidget *fs_overlay;
	GtkWidget *fs_eventbox;
	GtkWidget *fs_revealer;
	GtkWidget *fs_toolbar;
	gboolean   fs_pointer_on_toolbar;
	guint      fs_timeout_id;

	/* Loading message */
	guint loading_message_timeout;

	/* Dialogs */
	GtkWidget *properties;
	GtkWidget *print_dialog;

	GtkRecentManager *recent_manager;

	/* Popup view */
	GMenuModel   *view_popup_menu;
	GtkWidget    *view_popup;
	EvLink       *link;
	EvImage      *image;
	EvAnnotation *annot;

	/* Popup attachment */
	GMenuModel   *attachment_popup_menu;
	GtkWidget    *attachment_popup;
	GList        *attach_list;

	/* For bookshelf view of recent items*/
	EvRecentView *recent_view;

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
	GMenu *bookmarks_menu;

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

	/* Caret navigation */
	GtkWidget *ask_caret_navigation_check;

	/* Send to */
	gboolean has_mailto_handler;
};

#define EV_WINDOW_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_WINDOW, EvWindowPrivate))

#define EV_WINDOW_IS_PRESENTATION(w) (w->priv->presentation_view != NULL)

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
#define GS_PAGE_CACHE_SIZE       "page-cache-size"
#define GS_AUTO_RELOAD           "auto-reload"
#define GS_LAST_DOCUMENT_DIRECTORY "document-directory"
#define GS_LAST_PICTURES_DIRECTORY "pictures-directory"
#define GS_ALLOW_LINKS_CHANGE_ZOOM "allow-links-change-zoom"

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

#define TOOLBAR_RESOURCE_PATH "/org/gnome/evince/shell/ui/toolbar.xml"

#define FULLSCREEN_POPUP_TIMEOUT 2
#define FULLSCREEN_TRANSITION_DURATION 1000 /* in milliseconds */

#define FIND_PAGE_RATE_REFRESH	100

static const gchar *document_print_settings[] = {
	GTK_PRINT_SETTINGS_COLLATE,
	GTK_PRINT_SETTINGS_REVERSE,
	GTK_PRINT_SETTINGS_NUMBER_UP,
	GTK_PRINT_SETTINGS_SCALE,
	GTK_PRINT_SETTINGS_PRINT_PAGES,
	GTK_PRINT_SETTINGS_PAGE_RANGES,
	GTK_PRINT_SETTINGS_PAGE_SET,
	GTK_PRINT_SETTINGS_OUTPUT_URI
};

static void	ev_window_update_actions_sensitivity    (EvWindow         *ev_window);
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
static void     ev_window_run_presentation              (EvWindow         *window);
static void     ev_window_stop_presentation             (EvWindow         *window,
							 gboolean          unfullscreen_window);
static void     ev_window_popup_cmd_open_link           (GSimpleAction    *action,
							 GVariant         *parameter,
							 gpointer          user_data);
static void     ev_window_popup_cmd_open_link_new_window(GSimpleAction    *action,
							 GVariant         *parameter,
							 gpointer          user_data);
static void     ev_window_popup_cmd_copy_link_address   (GSimpleAction    *action,
							 GVariant         *parameter,
							 gpointer          user_data);
static void     ev_window_popup_cmd_save_image_as       (GSimpleAction    *action,
							 GVariant         *parameter,
							 gpointer          user_data);
static void     ev_window_popup_cmd_copy_image          (GSimpleAction    *action,
							 GVariant         *parameter,
							 gpointer          user_data);
static void     ev_window_popup_cmd_annot_properties    (GSimpleAction    *action,
							 GVariant         *parameter,
							 gpointer          user_data);
static void     ev_window_popup_cmd_remove_annotation   (GSimpleAction    *action,
							 GVariant         *parameter,
							 gpointer          user_data);
static void	ev_window_popup_cmd_open_attachment     (GSimpleAction    *action,
							 GVariant         *parameter,
							 gpointer          user_data);
static void	ev_window_popup_cmd_save_attachment_as  (GSimpleAction    *action,
							 GVariant         *parameter,
							 gpointer          user_data);
static void	view_handle_link_cb 			(EvView           *view, 
							 EvLink           *link, 
							 EvWindow         *window);
static void     activate_link_cb                        (GObject          *object,
							 EvLink           *link,
							 EvWindow         *window);
static void     ev_window_update_find_status_message    (EvWindow         *ev_window);
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
#ifdef ENABLE_DBUS
static void	ev_window_emit_closed			(EvWindow         *window);
static void 	ev_window_emit_doc_loaded		(EvWindow	  *window);
#endif
static void     ev_window_setup_bookmarks               (EvWindow         *window);

static void     ev_window_show_find_bar                 (EvWindow         *ev_window,
							 gboolean          restart);
static void     ev_window_close_find_bar                (EvWindow         *ev_window);
static void     ev_window_clear_find_job                (EvWindow         *ev_window);
static void     ev_window_destroy_recent_view           (EvWindow         *ev_window);
static void     recent_view_item_activated_cb           (EvRecentView     *recent_view,
                                                         const char       *uri,
                                                         EvWindow         *ev_window);

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
ev_window_is_recent_view (EvWindow *ev_window)
{
	return ev_toolbar_get_mode (EV_TOOLBAR (ev_window->priv->toolbar)) == EV_TOOLBAR_MODE_RECENT_VIEW;
}

static void
ev_window_set_action_enabled (EvWindow   *ev_window,
			      const char *name,
			      gboolean    enabled)
{
	GAction *action;

	action = g_action_map_lookup_action (G_ACTION_MAP (ev_window), name);
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);
}

static void
ev_window_update_actions_sensitivity (EvWindow *ev_window)
{
	EvDocument *document = ev_window->priv->document;
	EvView     *view = EV_VIEW (ev_window->priv->view);
	const EvDocumentInfo *info = NULL;
	gboolean has_document = FALSE;
	gboolean ok_to_print = TRUE;
	gboolean ok_to_copy = TRUE;
	gboolean has_properties = TRUE;
	gboolean override_restrictions = TRUE;
	gboolean can_get_text = FALSE;
	gboolean can_find = FALSE;
	gboolean can_find_in_page = FALSE;
	gboolean presentation_mode;
	gboolean recent_view_mode;
	gboolean dual_mode = FALSE;
	gboolean has_pages = FALSE;
	int      n_pages = 0, page = -1;

	if (document) {
		has_document = TRUE;
		info = ev_document_get_info (document);
		page = ev_document_model_get_page (ev_window->priv->model);
		n_pages = ev_document_get_n_pages (ev_window->priv->document);
		has_pages = n_pages > 0;
		dual_mode = ev_document_model_get_dual_page (ev_window->priv->model);
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

	/* Get modes */
	presentation_mode = EV_WINDOW_IS_PRESENTATION (ev_window);
	recent_view_mode = ev_window_is_recent_view (ev_window);

	/* File menu */
	ev_window_set_action_enabled (ev_window, "open-copy", has_document);
	ev_window_set_action_enabled (ev_window, "save-copy", has_document &&
				      ok_to_copy && !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "print", has_pages &&
				      ok_to_print && !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "show-properties",
				      has_document && has_properties &&
				      !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "open-containing-folder",
				      has_document && !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "send-to", has_document &&
				      ev_window->priv->has_mailto_handler &&
				      !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "fullscreen",
				      has_document && !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "presentation",
				      has_document && !recent_view_mode);

        /* Edit menu */
	ev_window_set_action_enabled (ev_window, "select-all", has_pages &&
				      can_get_text && !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "find", can_find &&
				      !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "toggle-find", can_find &&
				      !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "rotate-left", has_pages &&
				      !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "rotate-right", has_pages &&
				      !recent_view_mode);

        /* View menu */
	ev_window_set_action_enabled (ev_window, "continuous", has_pages &&
				      !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "dual-page", has_pages &&
				      !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "dual-odd-left", has_pages &&
				      !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "reload", has_pages &&
				      !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "auto-scroll", has_pages &&
				      !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "inverted-colors",
				      has_pages && !recent_view_mode);

	/* Bookmarks menu */
	ev_window_set_action_enabled (ev_window, "add-bookmark",
				      has_pages && ev_window->priv->bookmarks &&
				      !recent_view_mode);

	/* Other actions that must be disabled in recent view, in
	 * case they have a shortcut or gesture associated
	 */
	ev_window_set_action_enabled (ev_window, "save-settings", !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "show-side-pane", !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "goto-bookmark", !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "scroll-forward", !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "scroll-backwards", !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "sizing-mode", !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "zoom", !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "escape", !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "open-menu", !recent_view_mode);

	/* Same for popups specific actions */
	ev_window_set_action_enabled (ev_window, "open-link", !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "open-link-new-window", !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "go-to-link", !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "copy-link-address", !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "save-image", !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "copy-image", !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "open-attachment", !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "save-attachment", !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "annot-properties", !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "remove-annot", !recent_view_mode);

	can_find_in_page = (ev_window->priv->find_job &&
			    ev_job_find_has_results (EV_JOB_FIND (ev_window->priv->find_job)));

	ev_window_set_action_enabled (ev_window, "copy",
					has_pages &&
					ev_view_get_has_selection (view) &&
					!recent_view_mode);
	ev_window_set_action_enabled (ev_window, "find-next",
				      has_pages && can_find_in_page &&
				      !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "find-previous",
				      has_pages && can_find_in_page &&
				      !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "dual-odd-left", dual_mode &&
				      !recent_view_mode);

	ev_window_set_action_enabled (ev_window, "zoom-in",
				      has_pages &&
				      ev_view_can_zoom_in (view) &&
				      !presentation_mode &&
				      !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "zoom-out",
				      has_pages &&
				      ev_view_can_zoom_out (view) &&
				      !presentation_mode &&
				      !recent_view_mode);

        /* Go menu */
	if (has_pages) {
		ev_window_set_action_enabled (ev_window, "go-previous-page", page > 0);
		ev_window_set_action_enabled (ev_window, "go-next-page", page < n_pages - 1);
		ev_window_set_action_enabled (ev_window, "go-first-page", page > 0);
		ev_window_set_action_enabled (ev_window, "go-last-page", page < n_pages - 1);
		ev_window_set_action_enabled (ev_window, "select-page", TRUE);
	} else {
  		ev_window_set_action_enabled (ev_window, "go-first-page", FALSE);
		ev_window_set_action_enabled (ev_window, "go-previous-page", FALSE);
		ev_window_set_action_enabled (ev_window, "go-next-page", FALSE);
		ev_window_set_action_enabled (ev_window, "go-last-page", FALSE);
		ev_window_set_action_enabled (ev_window, "select-page", FALSE);
	}

	ev_window_set_action_enabled (ev_window, "go-back-history",
				      !ev_history_is_frozen (ev_window->priv->history) &&
				      ev_history_can_go_back (ev_window->priv->history) &&
				      !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "go-forward-history",
				      !ev_history_is_frozen (ev_window->priv->history) &&
				      ev_history_can_go_forward (ev_window->priv->history) &&
				      !recent_view_mode);

	ev_window_set_action_enabled (ev_window, "caret-navigation",
				      has_pages &&
				      ev_view_supports_caret_navigation (view) &&
				      !presentation_mode &&
				      !recent_view_mode);
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
	gboolean toolbar, findbar, sidebar;
	gboolean presentation;

	presentation = EV_WINDOW_IS_PRESENTATION (window);

	toolbar = ((priv->chrome & EV_CHROME_TOOLBAR) != 0  || 
		   (priv->chrome & EV_CHROME_RAISE_TOOLBAR) != 0) && !presentation;
	findbar = (priv->chrome & EV_CHROME_FINDBAR) != 0;
	sidebar = (priv->chrome & EV_CHROME_SIDEBAR) != 0 && priv->document && !presentation;

	set_widget_visibility (priv->toolbar, toolbar);
	set_widget_visibility (priv->find_bar, findbar);
	set_widget_visibility (priv->sidebar, sidebar);
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
	GAction     *action;
	const gchar *mode = NULL;

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "sizing-mode");

	switch (ev_document_model_get_sizing_mode (window->priv->model)) {
	case EV_SIZING_FIT_PAGE:
		mode = "fit-page";
		break;
	case EV_SIZING_FIT_WIDTH:
		mode = "fit-width";
		break;
	case EV_SIZING_AUTOMATIC:
		mode = "automatic";
		break;
	case EV_SIZING_FREE:
		mode = "free";
		break;
	}

	g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_string (mode));
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

G_GNUC_PRINTF (3, 4) static void
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

G_GNUC_PRINTF (2, 3) static void
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

static gboolean
show_loading_message_cb (EvWindow *window)
{
	window->priv->loading_message_timeout = 0;
	gtk_widget_show (window->priv->loading_message);

	return FALSE;
}

static void
ev_window_show_loading_message (EvWindow *window)
{
	if (window->priv->loading_message_timeout)
		return;
	window->priv->loading_message_timeout =
		g_timeout_add_full (G_PRIORITY_LOW, 0.5, (GSourceFunc)show_loading_message_cb, window, NULL);
}

static void
ev_window_hide_loading_message (EvWindow *window)
{
	if (window->priv->loading_message_timeout) {
		g_source_remove (window->priv->loading_message_timeout);
		window->priv->loading_message_timeout = 0;
	}

	gtk_widget_hide (window->priv->loading_message);
}

typedef struct _LinkTitleData {
	EvLink      *link;
	const gchar *link_title;
} LinkTitleData;

static gboolean
find_link_cb (GtkTreeModel  *tree_model,
	      GtkTreePath   *path,
	      GtkTreeIter   *iter,
	      LinkTitleData *data)
{
	EvLink *link;
	gboolean retval = FALSE;

	gtk_tree_model_get (tree_model, iter,
			    EV_DOCUMENT_LINKS_COLUMN_LINK, &link,
			    -1);
	if (!link)
		return retval;

	if (ev_link_action_equal (ev_link_get_action (data->link), ev_link_get_action (link))) {
		data->link_title = ev_link_get_title (link);
		retval = TRUE;
	}

	g_object_unref (link);

	return retval;
}

static const gchar *
ev_window_find_title_for_link (EvWindow *window,
			       EvLink   *link)
{
	if (EV_IS_DOCUMENT_LINKS (window->priv->document) &&
	    ev_document_links_has_document_links (EV_DOCUMENT_LINKS (window->priv->document))) {
		LinkTitleData data;
		GtkTreeModel *model;

		data.link = link;
		data.link_title = NULL;

		g_object_get (G_OBJECT (window->priv->sidebar_links),
			      "model", &model,
			      NULL);
		if (model) {
			gtk_tree_model_foreach (model,
						(GtkTreeModelForeachFunc)find_link_cb,
						&data);

			g_object_unref (model);
		}

		return data.link_title;
	}

	return NULL;
}

static void
view_handle_link_cb (EvView *view, EvLink *link, EvWindow *window)
{
	EvLink *new_link = NULL;

	if (!ev_link_get_title (link)) {
		const gchar *link_title;

		link_title = ev_window_find_title_for_link (window, link);
		if (link_title) {
			new_link = ev_link_new (link_title, ev_link_get_action (link));
		} else {
			EvLinkAction *action;
			EvLinkDest   *dest;
			gchar        *page_label;
			gchar        *title;

			action = ev_link_get_action (link);
			dest = ev_link_action_get_dest (action);
			page_label = ev_document_links_get_dest_page_label (EV_DOCUMENT_LINKS (window->priv->document), dest);
			if (!page_label)
				return;

			title = g_strdup_printf (_("Page %s"), page_label);
			g_free (page_label);

			new_link = ev_link_new (title, action);
			g_free (title);
		}
	}
	ev_history_add_link (window->priv->history, new_link ? new_link : link);
	if (new_link)
		g_object_unref (new_link);
}

static void
view_selection_changed_cb (EvView   *view,
			   EvWindow *window)
{
	ev_window_set_action_enabled (window, "copy",
					ev_view_get_has_selection (view));
}

static void
view_layers_changed_cb (EvView   *view,
			EvWindow *window)
{
	ev_sidebar_layers_update_layers_state (EV_SIDEBAR_LAYERS (window->priv->sidebar_layers));
}

static void
view_is_loading_changed_cb (EvView     *view,
			    GParamSpec *spec,
			    EvWindow   *window)
{
	if (ev_view_is_loading (view))
		ev_window_show_loading_message (window);
	else
		ev_window_hide_loading_message (window);
}

static void
view_caret_cursor_moved_cb (EvView   *view,
			    guint     page,
			    guint     offset,
			    EvWindow *window)
{
	GVariant *position;
	gchar    *caret_position;

	if (!window->priv->metadata)
		return;

	position = g_variant_new ("(uu)", page, offset);
	caret_position = g_variant_print (position, FALSE);
	g_variant_unref (position);

	ev_metadata_set_string (window->priv->metadata, "caret-position", caret_position);
	g_free (caret_position);
}

static void
ev_window_page_changed_cb (EvWindow        *ev_window,
			   gint             old_page,
			   gint             new_page,
			   EvDocumentModel *model)
{
	ev_window_update_actions_sensitivity (ev_window);

	ev_window_update_find_status_message (ev_window);

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
	} else {
		/* setup thumbnails by default */
		ev_sidebar_set_page (sidebar, thumbs);
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

	if (n_pages == 1)
		ev_document_model_set_dual_page (window->priv->model, FALSE);

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

	/* Caret navigation mode */
	if (ev_view_supports_caret_navigation (EV_VIEW (window->priv->view))) {
		gboolean caret_navigation;
		gchar   *caret_position;

		if (ev_metadata_get_string (window->priv->metadata, "caret-position", &caret_position)) {
			GVariant *position;

			position = g_variant_parse (G_VARIANT_TYPE ("(uu)"), caret_position, NULL, NULL, NULL);
			if (position) {
				guint page, offset;

				g_variant_get (position, "(uu)", &page, &offset);
				g_variant_unref (position);

				ev_view_set_caret_cursor_position (EV_VIEW (window->priv->view),
								   page, offset);
			}
		}

		if (ev_metadata_get_boolean (window->priv->metadata, "caret-navigation", &caret_navigation))
			ev_view_set_caret_navigation_enabled (EV_VIEW (window->priv->view), caret_navigation);
	}
}

static void
page_cache_size_changed (GSettings *settings,
			 gchar     *key,
			 EvWindow  *ev_window)
{
	guint page_cache_mb;

	page_cache_mb = g_settings_get_uint (settings, GS_PAGE_CACHE_SIZE);
	ev_view_set_page_cache_size (EV_VIEW (ev_window->priv->view),
				     page_cache_mb * 1024 * 1024);
}

static void
allow_links_change_zoom_changed (GSettings *settings,
			 gchar     *key,
			 EvWindow  *ev_window)
{
	gboolean allow_links_change_zoom = g_settings_get_boolean (settings, GS_ALLOW_LINKS_CHANGE_ZOOM);

	ev_view_set_allow_links_change_zoom (EV_VIEW (ev_window->priv->view), allow_links_change_zoom);
}

static void
ev_window_setup_default (EvWindow *ev_window)
{
	EvDocumentModel *model = ev_window->priv->model;
	GSettings       *settings = ev_window->priv->default_settings;

	/* Chrome */
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
	gdouble page_width, page_height;
	gint width, height;
	gint rotation;
	EvDocument *document = ev_window->priv->document;

	if (!document || ev_document_get_n_pages (document) <= 0 ||
	    !ev_document_check_dimensions (document)) {
		return;
	}

	ev_window_clear_thumbnail_job (ev_window);

	ev_document_get_page_size (document, 0, &page_width, &page_height);
	width = 128;
	height = (int)(width * page_height / page_width + 0.5);
	rotation = ev_document_model_get_rotation (ev_window->priv->model);

	ev_window->priv->thumbnail_job = ev_job_thumbnail_new_with_target_size (document, 0, rotation,
										width, height);
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
	ev_window_update_actions_sensitivity (ev_window);
}

#ifdef HAVE_DESKTOP_SCHEMAS
static void
lockdown_changed (GSettings   *lockdown,
		  const gchar *key,
		  EvWindow    *ev_window)
{
	ev_window_update_actions_sensitivity (ev_window);
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
        g_signal_connect (priv->settings,
			  "changed::"GS_PAGE_CACHE_SIZE,
			  G_CALLBACK (page_cache_size_changed),
			  ev_window);
        g_signal_connect (priv->settings,
			  "changed::"GS_ALLOW_LINKS_CHANGE_ZOOM,
			  G_CALLBACK (allow_links_change_zoom_changed),
			  ev_window);

        return priv->settings;
}

static gboolean
ev_window_setup_document (EvWindow *ev_window)
{
	const EvDocumentInfo *info;
	EvDocument *document = ev_window->priv->document;

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

	ev_window_update_actions_sensitivity (ev_window);

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

		if (ev_window->priv->search_string &&
		    !EV_WINDOW_IS_PRESENTATION (ev_window)) {
			ev_window_show_find_bar (ev_window, FALSE);
			egg_find_bar_set_search_string (EGG_FIND_BAR (ev_window->priv->find_bar), ev_window->priv->search_string);
		}

		g_clear_pointer (&ev_window->priv->search_string, g_free);
	}

	if (EV_WINDOW_IS_PRESENTATION (ev_window))
		gtk_widget_grab_focus (ev_window->priv->presentation_view);
	else if (!gtk_widget_get_visible (ev_window->priv->find_bar))
		gtk_widget_grab_focus (ev_window->priv->view);

	return FALSE;
}

static void
ev_window_set_document_metadata (EvWindow *window)
{
	const EvDocumentInfo *info;

	if (!window->priv->metadata)
		return;

	info = ev_document_get_info (window->priv->document);
	if (info->fields_mask & EV_DOCUMENT_INFO_TITLE && info->title && info->title[0] != '\0')
		ev_metadata_set_string (window->priv->metadata, "title", info->title);
	else
		ev_metadata_set_string (window->priv->metadata, "title", "");

	if (info->fields_mask & EV_DOCUMENT_INFO_AUTHOR && info->author && info->author[0] != '\0')
		ev_metadata_set_string (window->priv->metadata, "author", info->author);
	else
		ev_metadata_set_string (window->priv->metadata, "author", "");
}

static void
ev_window_set_document (EvWindow *ev_window, EvDocument *document)
{
	if (ev_window->priv->document == document)
		return;

	if (ev_window->priv->document)
		g_object_unref (ev_window->priv->document);
	ev_window->priv->document = g_object_ref (document);

	ev_window_set_message_area (ev_window, NULL);

	ev_window_set_document_metadata (ev_window);

	if (ev_document_get_n_pages (document) <= 0) {
		ev_window_warning_message (ev_window, "%s",
					   _("The document contains no pages"));
	} else if (!ev_document_check_dimensions (document)) {
		ev_window_warning_message (ev_window, "%s",
					   _("The document contains only empty pages"));
	}

	ev_window_destroy_recent_view (ev_window);

	ev_toolbar_set_mode (EV_TOOLBAR (ev_window->priv->toolbar),
			     EV_TOOLBAR_MODE_NORMAL);
	ev_window_title_set_type (ev_window->priv->title, EV_WINDOW_TITLE_DOCUMENT);
	ev_window_update_actions_sensitivity (ev_window);

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
ev_window_file_changed (EvWindow *ev_window,
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
		g_object_unref (link_action);
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
	gchar     *text;
	gchar 	  *display_name;

	g_assert (job_load->uri);

	ev_window_hide_loading_message (ev_window);

	/* Success! */
	if (!ev_job_is_failed (job)) {
		ev_document_model_set_document (ev_window->priv->model, document);

#ifdef ENABLE_DBUS
		ev_window_emit_doc_loaded (ev_window);
#endif
		setup_chrome_from_metadata (ev_window);
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
		g_clear_object (&ev_window->priv->dest);

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

		/* Create a monitor for the document */
		ev_window->priv->monitor = ev_file_monitor_new (ev_window->priv->uri);
		g_signal_connect_swapped (ev_window->priv->monitor, "changed",
					  G_CALLBACK (ev_window_file_changed),
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
		text = g_uri_unescape_string (job_load->uri, NULL);
		display_name = g_markup_escape_text (text, -1);
		g_free (text);
		ev_window_error_message (ev_window, job->error, 
					 _("Unable to open document “%s”."),
					 display_name);
		g_free (display_name);
		ev_window_clear_load_job (ev_window);
	}	
}

static void
ev_window_reload_job_cb (EvJob    *job,
			 EvWindow *ev_window)
{
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
		g_clear_object (&ev_window->priv->dest);
	}

	/* Restart the search after reloading */
	if (gtk_widget_is_visible (ev_window->priv->find_bar)) {
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
	gchar *text;
	gchar *display_name;

	ev_window_hide_loading_message (ev_window);
	ev_window->priv->in_reload = FALSE;

	text = g_uri_unescape_string (ev_window->priv->local_uri, NULL);
	display_name = g_markup_escape_text (text, -1);
	g_free (text);
	ev_window_error_message (ev_window, error, 
				 _("Unable to open document “%s”."),
				 display_name);
	g_free (display_name);
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

		ev_window_hide_loading_message (ev_window);
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

	g_clear_pointer (&ev_window->priv->search_string, g_free);
	ev_window->priv->search_string = search_string ?
		g_strdup (search_string) : NULL;

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
		ev_window_setup_bookmarks (ev_window);
	} else {
		ev_window->priv->bookmarks = NULL;
	}

	if (ev_window->priv->dest)
		g_object_unref (ev_window->priv->dest);
	ev_window->priv->dest = dest ? g_object_ref (dest) : NULL;

	setup_size_from_metadata (ev_window);
	setup_model_from_metadata (ev_window);

	ev_window->priv->load_job = ev_job_load_new (uri);
	g_signal_connect (ev_window->priv->load_job,
			  "finished",
			  G_CALLBACK (ev_window_load_job_cb),
			  ev_window);

	if (!g_file_is_native (source_file) && !ev_window->priv->local_uri) {
		ev_window_load_file_remote (ev_window, source_file);
	} else {
		ev_window_show_loading_message (ev_window);
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
		g_object_unref (link_action);
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
		ev_window_show_find_bar (ev_window, FALSE);
		egg_find_bar_set_search_string (EGG_FIND_BAR (ev_window->priv->find_bar),
						search_string);
	}

	/* Create a monitor for the document */
	ev_window->priv->monitor = ev_file_monitor_new (ev_window->priv->uri);
	g_signal_connect_swapped (ev_window->priv->monitor, "changed",
				  G_CALLBACK (ev_window_file_changed),
				  ev_window);
}

void
ev_window_open_recent_view (EvWindow *ev_window)
{
	if (ev_window->priv->recent_view)
		return;

	gtk_widget_hide (ev_window->priv->hpaned);
	gtk_widget_hide (ev_window->priv->find_bar);

	ev_window->priv->recent_view = EV_RECENT_VIEW (ev_recent_view_new ());
	g_signal_connect_object (ev_window->priv->recent_view,
				 "item-activated",
				 G_CALLBACK (recent_view_item_activated_cb),
				 ev_window, 0);
	gtk_box_pack_start (GTK_BOX (ev_window->priv->main_box),
			    GTK_WIDGET (ev_window->priv->recent_view),
			    TRUE, TRUE, 0);

	gtk_widget_show (GTK_WIDGET (ev_window->priv->recent_view));
	ev_toolbar_set_mode (EV_TOOLBAR (ev_window->priv->toolbar),
			     EV_TOOLBAR_MODE_RECENT_VIEW);
	ev_window_title_set_type (ev_window->priv->title, EV_WINDOW_TITLE_RECENT);

	ev_window_update_actions_sensitivity (ev_window);
}

static void
ev_window_destroy_recent_view (EvWindow *ev_window)
{
	if (!ev_window->priv->recent_view)
		return;

	gtk_widget_destroy (GTK_WIDGET (ev_window->priv->recent_view));
	ev_window->priv->recent_view = NULL;
	gtk_widget_show (ev_window->priv->hpaned);
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
        const gchar *dir;
        gchar *folder_uri;

        g_settings_get (ev_window_ensure_settings (window),
                        get_settings_key_for_directory (directory),
                        "ms", &folder_uri);
        if (folder_uri == NULL && uri != NULL) {
                GFile *file, *parent;

                file = g_file_new_for_uri (uri);
                parent = g_file_get_parent (file);
                g_object_unref (file);
                if (parent) {
                        folder_uri = g_file_get_uri (parent);
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

        g_free (folder_uri);
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
ev_window_cmd_file_open (GSimpleAction *action,
			 GVariant      *parameter,
			 gpointer       user_data)
{
	EvWindow  *window = user_data;
	GtkWidget *chooser;

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
ev_window_cmd_file_open_copy (GSimpleAction *action,
			      GVariant      *parameter,
			      gpointer       user_data)
{
	EvWindow *window = user_data;

	ev_window_open_copy_at_dest (window, NULL);
}

static void
ev_window_add_recent (EvWindow *window, const char *filename)
{
	gtk_recent_manager_add_item (window->priv->recent_manager, filename);
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
ev_window_save_a_copy (EvWindow *ev_window)
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
ev_window_cmd_save_as (GSimpleAction *action,
		       GVariant      *parameter,
		       gpointer       user_data)
{
	EvWindow *window = user_data;

	ev_window_save_a_copy (window);
}

static void
ev_window_cmd_send_to (GSimpleAction *action,
		       GVariant      *parameter,
		       gpointer       user_data)
{
	EvWindow   *ev_window = user_data;
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
ev_window_cmd_open_containing_folder (GSimpleAction *action,
				      GVariant      *parameter,
				      gpointer       user_data)
{
	EvWindow *ev_window = user_data;
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
	gchar            *output_basename;
	gchar            *unescaped_basename;
	const gchar      *document_uri;
	gchar            *dot;

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

	document_uri = ev_document_get_uri (ev_window->priv->document);
	output_basename = g_path_get_basename (document_uri);
	dot = g_strrstr (output_basename, ".");
	if (dot)
		dot[0] = '\0';

	unescaped_basename = g_uri_unescape_string (output_basename, NULL);
	/* Set output basename for printing to file */
	gtk_print_settings_set (print_settings,
			        GTK_PRINT_SETTINGS_OUTPUT_BASENAME,
			        unescaped_basename);
	g_free (unescaped_basename);
	g_free (output_basename);

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
ev_window_cmd_file_print (GSimpleAction *action,
			  GVariant      *state,
			  gpointer       user_data)
{
	EvWindow *ev_window = user_data;

	ev_window_print (ev_window);
}

static void
ev_window_cmd_file_properties (GSimpleAction *action,
			       GVariant      *state,
			       gpointer       user_data)
{
	EvWindow *ev_window = user_data;

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
		ev_window_save_a_copy (ev_window);
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
ev_window_cmd_file_close_window (GSimpleAction *action,
				 GVariant      *parameter,
				 gpointer       user_data)
{
	EvWindow *ev_window = user_data;

	if (ev_window_close (ev_window))
		gtk_widget_destroy (GTK_WIDGET (ev_window));
}

static void
ev_window_cmd_focus_page_selector (GSimpleAction *action,
				   GVariant      *parameter,
				   gpointer       user_data)
{
	EvWindow *window = user_data;
	GtkWidget *page_selector;
	EvToolbar *toolbar;

	update_chrome_flag (window, EV_CHROME_RAISE_TOOLBAR, TRUE);
	update_chrome_visibility (window);

	toolbar = window->priv->fs_toolbar ? EV_TOOLBAR (window->priv->fs_toolbar) : EV_TOOLBAR (window->priv->toolbar);
	page_selector = ev_toolbar_get_page_selector (toolbar);
	ev_page_action_widget_grab_focus (EV_PAGE_ACTION_WIDGET (page_selector));
}

static void
ev_window_cmd_scroll_forward (GSimpleAction *action,
			      GVariant      *parameter,
			      gpointer       user_data)
{
	EvWindow *window = user_data;

	g_signal_emit_by_name (window->priv->view, "scroll", GTK_SCROLL_PAGE_FORWARD, GTK_ORIENTATION_VERTICAL);
}

static void
ev_window_cmd_scroll_backwards (GSimpleAction *action,
				GVariant      *parameter,
				gpointer       user_data)
{
	EvWindow *window = user_data;

	g_signal_emit_by_name (window->priv->view, "scroll", GTK_SCROLL_PAGE_BACKWARD, GTK_ORIENTATION_VERTICAL);
}

static void
ev_window_cmd_continuous (GSimpleAction *action,
			  GVariant      *state,
			  gpointer       user_data)
{
	EvWindow *window = user_data;

	ev_window_stop_presentation (window, TRUE);
	ev_document_model_set_continuous (window->priv->model, g_variant_get_boolean (state));
	g_simple_action_set_state (action, state);
}

static void
ev_window_cmd_dual (GSimpleAction *action,
		    GVariant      *state,
		    gpointer       user_data)
{
	EvWindow *window = user_data;

	ev_window_stop_presentation (window, TRUE);
	ev_document_model_set_dual_page (window->priv->model, g_variant_get_boolean (state));
	g_simple_action_set_state (action, state);
}

static void
ev_window_cmd_dual_odd_pages_left (GSimpleAction *action,
				   GVariant      *state,
				   gpointer       user_data)
{
	EvWindow *window = user_data;

	ev_document_model_set_dual_page_odd_pages_left (window->priv->model,
							g_variant_get_boolean (state));
	g_simple_action_set_state (action, state);
}

static void
ev_window_change_sizing_mode_action_state (GSimpleAction *action,
					   GVariant      *state,
					   gpointer       user_data)
{
	EvWindow *window = user_data;
	const gchar *mode;

	mode = g_variant_get_string (state, NULL);

	if (g_str_equal (mode, "fit-page"))
		ev_document_model_set_sizing_mode (window->priv->model, EV_SIZING_FIT_PAGE);
	else if (g_str_equal (mode, "fit-width"))
		ev_document_model_set_sizing_mode (window->priv->model, EV_SIZING_FIT_WIDTH);
	else if (g_str_equal (mode, "automatic"))
		ev_document_model_set_sizing_mode (window->priv->model, EV_SIZING_AUTOMATIC);
	else if (g_str_equal (mode, "free"))
		ev_document_model_set_sizing_mode (window->priv->model, EV_SIZING_FREE);
	else
		g_assert_not_reached ();

	g_simple_action_set_state (action, state);
}

static void
ev_window_cmd_view_zoom (GSimpleAction *action,
			 GVariant      *parameter,
			 gpointer       user_data)
{
	EvWindow *ev_window = user_data;
	gdouble zoom = g_variant_get_double (parameter);

	ev_document_model_set_sizing_mode (ev_window->priv->model, EV_SIZING_FREE);
	ev_document_model_set_scale (ev_window->priv->model,
				     zoom * get_screen_dpi (ev_window) / 72.0);
}

static void
ev_window_cmd_edit_select_all (GSimpleAction *action,
			       GVariant      *parameter,
			       gpointer       user_data)
{
	EvWindow *ev_window = user_data;

	ev_view_select_all (EV_VIEW (ev_window->priv->view));
}

static void
ev_window_cmd_toggle_find (GSimpleAction *action,
			   GVariant      *state,
			   gpointer       user_data)
{
	EvWindow *ev_window = user_data;

	if (g_variant_get_boolean (state))
		ev_window_show_find_bar (ev_window, TRUE);
	else
		ev_window_close_find_bar (ev_window);

	g_simple_action_set_state (action, state);
}

static void
ev_window_cmd_find (GSimpleAction *action,
		    GVariant      *parameter,
		    gpointer       user_data)
{
	EvWindow *ev_window = user_data;

	ev_window_show_find_bar (ev_window, TRUE);
}

static void
ev_window_find_restart (EvWindow *ev_window)
{
	gint page;

	page = ev_document_model_get_page (ev_window->priv->model);
	ev_view_find_restart (EV_VIEW (ev_window->priv->view), page);
	ev_find_sidebar_restart (EV_FIND_SIDEBAR (ev_window->priv->find_sidebar), page);
}

static void
ev_window_find_previous (EvWindow *ev_window)
{
	ev_view_find_previous (EV_VIEW (ev_window->priv->view));
	ev_find_sidebar_previous (EV_FIND_SIDEBAR (ev_window->priv->find_sidebar));
}

static void
ev_window_find_next (EvWindow *ev_window)
{
	ev_view_find_next (EV_VIEW (ev_window->priv->view));
	ev_find_sidebar_next (EV_FIND_SIDEBAR (ev_window->priv->find_sidebar));
}

static gboolean
find_next_idle_cb (EvWindow *ev_window)
{
	ev_window_find_next (ev_window);
	return FALSE;
}

static void
ev_window_cmd_edit_find_next (GSimpleAction *action,
			      GVariant      *parameter,
			      gpointer       user_data)
{
	EvWindow *ev_window = user_data;
	gboolean find_bar_hidden;

	if (EV_WINDOW_IS_PRESENTATION (ev_window))
		return;

	find_bar_hidden = !gtk_widget_get_visible (ev_window->priv->find_bar);
	ev_window_show_find_bar (ev_window, FALSE);

	/* Use idle to make sure view allocation happens before find */
	if (find_bar_hidden)
		g_idle_add ((GSourceFunc)find_next_idle_cb, ev_window);
	else
		ev_window_find_next (ev_window);
}

static gboolean
find_previous_idle_cb (EvWindow *ev_window)
{
	ev_window_find_previous (ev_window);
	return FALSE;
}

static void
ev_window_cmd_edit_find_previous (GSimpleAction *action,
				  GVariant      *parameter,
				  gpointer       user_data)
{
	EvWindow *ev_window = user_data;
	gboolean find_bar_hidden;

	if (EV_WINDOW_IS_PRESENTATION (ev_window))
		return;

	find_bar_hidden = !gtk_widget_get_visible (ev_window->priv->find_bar);
	ev_window_show_find_bar (ev_window, FALSE);

	/* Use idle to make sure view allocation happens before find */
	if (find_bar_hidden)
		g_idle_add ((GSourceFunc)find_previous_idle_cb, ev_window);
	else
		ev_window_find_previous (ev_window);
}

static void
ev_window_cmd_edit_copy (GSimpleAction *action,
			 GVariant      *parameter,
			 gpointer       user_data)
{
	EvWindow *ev_window = user_data;

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
ev_window_update_links_model (EvWindow *window)
{
	GtkTreeModel *model;
	GtkWidget *page_selector;

	g_object_get (window->priv->sidebar_links,
		      "model", &model,
		      NULL);

	if (!model)
		return;

	page_selector = ev_toolbar_get_page_selector (EV_TOOLBAR (window->priv->toolbar));
	ev_page_action_widget_update_links_model (EV_PAGE_ACTION_WIDGET (page_selector), model);
	if (window->priv->fs_toolbar) {
		page_selector = ev_toolbar_get_page_selector (EV_TOOLBAR (window->priv->fs_toolbar));
		ev_page_action_widget_update_links_model (EV_PAGE_ACTION_WIDGET (page_selector), model);
	}
	g_object_unref (model);
}

static void
ev_window_update_fullscreen_action (EvWindow *window)
{
	GAction *action;
	gboolean fullscreen;

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "fullscreen");
	fullscreen = ev_document_model_get_fullscreen (window->priv->model);
	g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (fullscreen));
}

static void
ev_window_fullscreen_hide_toolbar (EvWindow *window)
{
	if (!ev_toolbar_has_visible_popups (EV_TOOLBAR (window->priv->fs_toolbar)))
		gtk_revealer_set_reveal_child (GTK_REVEALER (window->priv->fs_revealer), FALSE);
}

static gboolean
fullscreen_toolbar_timeout_cb (EvWindow *window)
{
	ev_window_fullscreen_hide_toolbar (window);

	if (!gtk_revealer_get_reveal_child (GTK_REVEALER (window->priv->fs_revealer))) {
		window->priv->fs_timeout_id = 0;
		return FALSE;
	}

	return TRUE;
}

static void
ev_window_remove_fullscreen_timeout (EvWindow *window)
{
	if (window->priv->fs_timeout_id)
		g_source_remove (window->priv->fs_timeout_id);
	window->priv->fs_timeout_id = 0;
}

static void
ev_window_add_fullscreen_timeout (EvWindow *window)
{
	window->priv->fs_timeout_id =
		g_timeout_add_seconds (FULLSCREEN_POPUP_TIMEOUT,
				       (GSourceFunc)fullscreen_toolbar_timeout_cb, window);
}

static void
ev_window_fullscreen_show_toolbar (EvWindow *window)
{
	ev_window_remove_fullscreen_timeout (window);
	if (gtk_revealer_get_reveal_child (GTK_REVEALER (window->priv->fs_revealer)))
		return;

	gtk_revealer_set_reveal_child (GTK_REVEALER (window->priv->fs_revealer), TRUE);
	if (!window->priv->fs_pointer_on_toolbar)
		ev_window_add_fullscreen_timeout (window);
}

static gboolean
ev_window_fullscreen_toolbar_enter_notify (GtkWidget *widget,
					   GdkEvent  *event,
					   EvWindow  *window)
{
	window->priv->fs_pointer_on_toolbar = TRUE;
	ev_window_fullscreen_show_toolbar (window);

	return FALSE;
}

static gboolean
ev_window_fullscreen_toolbar_leave_notify (GtkWidget *widget,
					   GdkEvent  *event,
					   EvWindow  *window)
{
	window->priv->fs_pointer_on_toolbar = FALSE;
	ev_window_add_fullscreen_timeout (window);

	return FALSE;
}

static void
ev_window_run_fullscreen (EvWindow *window)
{
	gboolean fullscreen_window = TRUE;

	if (ev_document_model_get_fullscreen (window->priv->model))
		return;

	if (EV_WINDOW_IS_PRESENTATION (window)) {
		ev_window_stop_presentation (window, FALSE);
		fullscreen_window = FALSE;
	}

	window->priv->fs_overlay = gtk_overlay_new ();
	window->priv->fs_eventbox = gtk_event_box_new ();
	window->priv->fs_revealer = gtk_revealer_new ();
	g_signal_connect (window->priv->fs_eventbox, "enter-notify-event",
			  G_CALLBACK (ev_window_fullscreen_toolbar_enter_notify),
			  window);
	g_signal_connect (window->priv->fs_eventbox, "leave-notify-event",
			  G_CALLBACK (ev_window_fullscreen_toolbar_leave_notify),
			  window);

	gtk_widget_set_size_request (window->priv->fs_eventbox, -1, 1);
	gtk_widget_set_valign (window->priv->fs_eventbox, GTK_ALIGN_START);
	gtk_revealer_set_transition_type (GTK_REVEALER (window->priv->fs_revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
	gtk_revealer_set_transition_duration (GTK_REVEALER (window->priv->fs_revealer), FULLSCREEN_TRANSITION_DURATION);

	g_object_ref (window->priv->main_box);
	gtk_container_remove (GTK_CONTAINER (window), window->priv->main_box);
	gtk_container_add (GTK_CONTAINER (window->priv->fs_overlay),
			   window->priv->main_box);
	g_object_unref (window->priv->main_box);

	window->priv->fs_toolbar = ev_toolbar_new (window);
	ev_toolbar_set_mode (EV_TOOLBAR (window->priv->fs_toolbar),
		             EV_TOOLBAR_MODE_FULLSCREEN);

	ev_window_update_links_model (window);
	g_signal_connect (ev_toolbar_get_page_selector (EV_TOOLBAR (window->priv->fs_toolbar)),
			  "activate-link",
			  G_CALLBACK (activate_link_cb),
			  window);
	gtk_container_add (GTK_CONTAINER (window->priv->fs_revealer),
			   window->priv->fs_toolbar);
	gtk_widget_show (window->priv->fs_toolbar);

	gtk_container_add (GTK_CONTAINER (window->priv->fs_eventbox),
			   window->priv->fs_revealer);
	gtk_widget_show (window->priv->fs_revealer);
	gtk_overlay_add_overlay (GTK_OVERLAY (window->priv->fs_overlay),
				 window->priv->fs_eventbox);
	gtk_widget_show (window->priv->fs_eventbox);

	gtk_container_add (GTK_CONTAINER (window), window->priv->fs_overlay);
	gtk_widget_show (window->priv->fs_overlay);

	ev_document_model_set_fullscreen (window->priv->model, TRUE);
	ev_window_update_fullscreen_action (window);

	ev_window_fullscreen_show_toolbar (window);

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

	gtk_container_remove (GTK_CONTAINER (window->priv->fs_revealer),
			      window->priv->fs_toolbar);
	window->priv->fs_toolbar = NULL;
	gtk_container_remove (GTK_CONTAINER (window->priv->fs_eventbox),
			      window->priv->fs_revealer);
	gtk_container_remove (GTK_CONTAINER (window->priv->fs_overlay),
			      window->priv->fs_eventbox);

	g_object_ref (window->priv->main_box);
	gtk_container_remove (GTK_CONTAINER (window->priv->fs_overlay),
			      window->priv->main_box);
	gtk_container_remove (GTK_CONTAINER (window), window->priv->fs_overlay);
	window->priv->fs_overlay = NULL;
	gtk_container_add (GTK_CONTAINER (window), window->priv->main_box);
	g_object_unref (window->priv->main_box);

	ev_window_remove_fullscreen_timeout (window);

	ev_document_model_set_fullscreen (window->priv->model, FALSE);
	ev_window_update_fullscreen_action (window);

	if (unfullscreen_window)
		gtk_window_unfullscreen (GTK_WINDOW (window));

	if (window->priv->metadata && !ev_window_is_empty (window))
		ev_metadata_set_boolean (window->priv->metadata, "fullscreen", FALSE);
}

static void
ev_window_cmd_view_fullscreen (GSimpleAction *action,
			       GVariant      *state,
			       gpointer       user_data)
{
	EvWindow *window = user_data;

	if (g_variant_get_boolean (state)) {
		ev_window_run_fullscreen (window);
	} else {
		ev_window_stop_fullscreen (window, TRUE);
	}

	g_simple_action_set_state (action, state);
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
	GAction *action;

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "presentation");
	g_simple_action_set_state (G_SIMPLE_ACTION (action),
				   g_variant_new_boolean (EV_WINDOW_IS_PRESENTATION (window)));
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
ev_window_cmd_view_presentation (GSimpleAction *action,
				 GVariant      *state,
				 gpointer       user_data)
{
	EvWindow *window = user_data;

	if (g_variant_get_boolean (state)) {
		ev_window_run_presentation (window);
	}
	/* We don't exit presentation when action is toggled because it conflicts with some
	 * remote controls. The behaviour is also consistent with libreoffice and other
	 * presentation tools. See https://bugzilla.gnome.org/show_bug.cgi?id=556162
	 */
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
	ev_window_update_actions_sensitivity (window);
}

static void
ev_window_cmd_edit_rotate_left (GSimpleAction *action,
				GVariant      *parameter,
				gpointer       user_data)
{
	EvWindow *ev_window = user_data;
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
ev_window_cmd_edit_rotate_right (GSimpleAction *action,
				 GVariant      *parameter,
				 gpointer       user_data)
{
	EvWindow *ev_window = user_data;
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
ev_window_cmd_view_inverted_colors (GSimpleAction *action,
				    GVariant      *state,
				    gpointer       user_data)
{
	EvWindow *ev_window = user_data;

	ev_document_model_set_inverted_colors (ev_window->priv->model,
					       g_variant_get_boolean (state));
	g_simple_action_set_state (action, state);
}

static void
ev_window_cmd_edit_save_settings (GSimpleAction *action,
				  GVariant      *state,
				  gpointer       user_data)
{
	EvWindow        *ev_window = user_data;
	EvWindowPrivate *priv = ev_window->priv;
	EvDocumentModel *model = priv->model;
	GSettings       *settings = priv->default_settings;
	EvSizingMode     sizing_mode;
	EvView          *view = EV_VIEW (ev_window->priv->view);

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
	g_settings_set_boolean (settings, "show-sidebar",
				gtk_widget_get_visible (priv->sidebar));
	g_settings_set_int (settings, "sidebar-size",
			    gtk_paned_get_position (GTK_PANED (priv->hpaned)));
	g_settings_set_string (settings, "sidebar-page",
			       ev_window_sidebar_get_current_page_id (ev_window));
	g_settings_apply (settings);
}

static void
ev_window_cmd_view_zoom_in (GSimpleAction *action,
			    GVariant      *parameter,
			    gpointer       user_data)
{
	EvWindow *ev_window = user_data;

	ev_document_model_set_sizing_mode (ev_window->priv->model, EV_SIZING_FREE);
	ev_view_zoom_in (EV_VIEW (ev_window->priv->view));
}

static void
ev_window_cmd_view_zoom_out (GSimpleAction *action,
			     GVariant      *parameter,
			     gpointer       user_data)
{
	EvWindow *ev_window = user_data;

	ev_document_model_set_sizing_mode (ev_window->priv->model, EV_SIZING_FREE);
	ev_view_zoom_out (EV_VIEW (ev_window->priv->view));
}

static void
ev_window_cmd_go_back_history (GSimpleAction *action,
			       GVariant      *parameter,
			       gpointer       user_data)
{
	EvWindow *ev_window = user_data;

	ev_history_go_back (ev_window->priv->history);
}

static void
ev_window_cmd_go_forward_history (GSimpleAction *action,
				  GVariant      *parameter,
				  gpointer       user_data)
{
	EvWindow *ev_window = user_data;

	ev_history_go_forward (ev_window->priv->history);
}

static void
ev_window_cmd_go_previous_page (GSimpleAction *action,
				GVariant      *parameter,
				gpointer       user_data)
{
	EvWindow *window = user_data;

	ev_view_previous_page (EV_VIEW (window->priv->view));
}

static void
ev_window_cmd_go_next_page (GSimpleAction *action,
					GVariant      *parameter,
					gpointer       user_data)
{
	EvWindow *window = user_data;

	ev_view_next_page (EV_VIEW (window->priv->view));
}

static void
ev_window_cmd_go_first_page (GSimpleAction *action,
					 GVariant      *parameter,
					 gpointer       user_data)
{
	EvWindow *window = user_data;

	ev_document_model_set_page (window->priv->model, 0);
}

static void
ev_window_cmd_go_last_page (GSimpleAction *action,
			    GVariant      *parameter,
			    gpointer       user_data)
{
	EvWindow *window = user_data;

	ev_document_model_set_page (window->priv->model,
				    ev_document_get_n_pages (window->priv->document) - 1);
}

static void
ev_window_cmd_go_forward (GSimpleAction *action,
			  GVariant      *parameter,
			  gpointer       user_data)
{
	EvWindow *ev_window = user_data;
	int n_pages, current_page;

	n_pages = ev_document_get_n_pages (ev_window->priv->document);
	current_page = ev_document_model_get_page (ev_window->priv->model);
	
	if (current_page + 10 < n_pages) {
		ev_document_model_set_page (ev_window->priv->model, current_page + 10);
	}
}

static void
ev_window_cmd_go_backwards (GSimpleAction *action,
			    GVariant      *parameter,
			    gpointer       user_data)
{
	EvWindow *ev_window = user_data;
	int current_page;

	current_page = ev_document_model_get_page (ev_window->priv->model);
	
	if (current_page - 10 >= 0) {
		ev_document_model_set_page (ev_window->priv->model, current_page - 10);
	}
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
	GList *items, *it;

	g_menu_remove_all (window->priv->bookmarks_menu);

	items = g_list_sort (ev_bookmarks_get_bookmarks (window->priv->bookmarks),
			     (GCompareFunc) compare_bookmarks);

	for (it = items; it; it = it->next) {
		EvBookmark *bookmark = it->data;
		GMenuItem *item;

		item = g_menu_item_new (bookmark->title, NULL);
		g_menu_item_set_action_and_target (item, "win.goto-bookmark", "u", bookmark->page);
		g_menu_append_item (window->priv->bookmarks_menu, item);

		g_object_unref (item);
	}

	g_list_free (items);
}

static void
ev_window_cmd_bookmarks_add (GSimpleAction *action,
			     GVariant      *parameter,
			     gpointer       user_data)
{
	EvWindow *window = user_data;
	EvBookmark bm;
	gchar     *page_label;

	bm.page = ev_document_model_get_page (window->priv->model);
	page_label = ev_document_get_page_label (window->priv->document, bm.page);
	bm.title = g_strdup_printf (_("Page %s"), page_label);
	g_free (page_label);

	/* EvBookmarks takes ownership of bookmark */
	ev_bookmarks_add (window->priv->bookmarks, &bm);
}

static void
ev_window_activate_goto_bookmark_action (GSimpleAction *action,
					 GVariant      *parameter,
					 gpointer       user_data)
{
	EvWindow *window = user_data;

	ev_document_model_set_page (window->priv->model, g_variant_get_uint32 (parameter));
}

static void
ev_window_cmd_view_reload (GSimpleAction *action,
			   GVariant      *parameter,
			   gpointer       user_data)
{
	EvWindow *ev_window = user_data;

	ev_window_reload_document (ev_window, NULL);
}

static void
ev_window_cmd_view_autoscroll (GSimpleAction *action,
			       GVariant      *parameter,
			       gpointer       user_data)
{
	EvWindow *ev_window = user_data;

	ev_view_autoscroll_start (EV_VIEW (ev_window->priv->view));
}

static void
ev_window_cmd_escape (GSimpleAction *action,
		      GVariant      *parameter,
		      gpointer       user_data)
{
	EvWindow *window = user_data;
	ev_view_autoscroll_stop (EV_VIEW (window->priv->view));

	if (gtk_widget_get_visible (window->priv->find_bar))
		ev_window_close_find_bar (window);
	else if (ev_document_model_get_fullscreen (window->priv->model))
		ev_window_stop_fullscreen (window, TRUE);
	else if (EV_WINDOW_IS_PRESENTATION (window))
		ev_window_stop_presentation (window, TRUE);
	else
		gtk_widget_grab_focus (window->priv->view);
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
        ev_window_update_actions_sensitivity (ev_window);

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
ev_window_continuous_changed_cb (EvDocumentModel *model,
				 GParamSpec      *pspec,
				 EvWindow        *ev_window)
{
	gboolean continuous;
	GAction *action;

	continuous = ev_document_model_get_continuous (model);

	action = g_action_map_lookup_action (G_ACTION_MAP (ev_window), "continuous");
	g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (continuous));

	if (ev_window->priv->metadata && !ev_window_is_empty (ev_window))
		ev_metadata_set_boolean (ev_window->priv->metadata, "continuous", continuous);
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

	ev_window_refresh_window_thumbnail (window);
}

static void
ev_window_inverted_colors_changed_cb (EvDocumentModel *model,
			              GParamSpec      *pspec,
			              EvWindow        *window)
{
	gboolean inverted_colors = ev_document_model_get_inverted_colors (model);
	GAction *action;

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "inverted-colors");
	g_simple_action_set_state (G_SIMPLE_ACTION (action),
				   g_variant_new_boolean (inverted_colors));

	if (window->priv->metadata && !ev_window_is_empty (window))
		ev_metadata_set_boolean (window->priv->metadata, "inverted-colors",
					 inverted_colors);

	ev_window_refresh_window_thumbnail (window);
}

static void
ev_window_dual_mode_changed_cb (EvDocumentModel *model,
				GParamSpec      *pspec,
				EvWindow        *ev_window)
{
	gboolean dual_page;
	GAction *action;

	dual_page = ev_document_model_get_dual_page (model);

	action = g_action_map_lookup_action (G_ACTION_MAP (ev_window), "dual-page");
	g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (dual_page));

	if (ev_window->priv->metadata && !ev_window_is_empty (ev_window))
		ev_metadata_set_boolean (ev_window->priv->metadata, "dual-page", dual_page);
}

static void
ev_window_dual_mode_odd_pages_left_changed_cb (EvDocumentModel *model,
					       GParamSpec      *pspec,
					       EvWindow        *ev_window)
{
	if (ev_window->priv->metadata && !ev_window_is_empty (ev_window))
		ev_metadata_set_boolean (ev_window->priv->metadata, "dual-page-odd-left",
					 ev_document_model_get_dual_page_odd_pages_left (model));
}

static void
ev_window_cmd_action_menu (GSimpleAction *action,
			   GVariant      *parameter,
			   gpointer       user_data)
{
	EvWindow  *ev_window = user_data;
	EvToolbar *toolbar;

	toolbar = ev_window->priv->fs_toolbar ? EV_TOOLBAR (ev_window->priv->fs_toolbar) : EV_TOOLBAR (ev_window->priv->toolbar);
	ev_toolbar_action_menu_popup (toolbar);
}

static void
ev_window_view_cmd_toggle_sidebar (GSimpleAction *action,
				   GVariant      *state,
				   gpointer       user_data)
{
	EvWindow *ev_window = user_data;
	gboolean show_side_pane;

	if (EV_WINDOW_IS_PRESENTATION (ev_window))
		return;

	show_side_pane = g_variant_get_boolean (state);
	g_simple_action_set_state (action, g_variant_new_boolean (show_side_pane));

	update_chrome_flag (ev_window, EV_CHROME_SIDEBAR, show_side_pane);
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
	if (!EV_WINDOW_IS_PRESENTATION (ev_window)) {
		gboolean visible = gtk_widget_get_visible (GTK_WIDGET (ev_sidebar));

		g_action_group_change_action_state (G_ACTION_GROUP (ev_window), "show-side-pane",
						    g_variant_new_boolean (visible));

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
	gboolean  show_external = FALSE;
	gboolean  show_internal = FALSE;
	GAction  *action;

	g_clear_object (&ev_window->priv->link);
	if (link) {
		EvLinkAction *ev_action;

		ev_window->priv->link = g_object_ref (link);

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

	action = g_action_map_lookup_action (G_ACTION_MAP (ev_window), "open-link");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), show_external);

	action = g_action_map_lookup_action (G_ACTION_MAP (ev_window), "copy-link-address");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), show_external);

	action = g_action_map_lookup_action (G_ACTION_MAP (ev_window), "go-to-link");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), show_internal);

	action = g_action_map_lookup_action (G_ACTION_MAP (ev_window), "open-link-new-window");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), show_internal);
}

static void
view_menu_image_popup (EvWindow  *ev_window,
		       EvImage   *image)
{
	GAction *action;
	gboolean show_image = FALSE;

	g_clear_object (&ev_window->priv->image);
	if (image) {
		ev_window->priv->image = g_object_ref (image);
		show_image = TRUE;
	}

	action = g_action_map_lookup_action (G_ACTION_MAP (ev_window), "save-image");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), show_image);

	action = g_action_map_lookup_action (G_ACTION_MAP (ev_window), "copy-image");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), show_image);
}

static void
view_menu_annot_popup (EvWindow     *ev_window,
		       EvAnnotation *annot)
{
	GAction *action;
	gboolean show_annot_props = FALSE;
	gboolean show_attachment = FALSE;
	gboolean can_remove_annots = FALSE;

	g_clear_object (&ev_window->priv->annot);
	if (annot) {
		ev_window->priv->annot = g_object_ref (annot);

		show_annot_props = EV_IS_ANNOTATION_MARKUP (annot);

		if (EV_IS_ANNOTATION_ATTACHMENT (annot)) {
			EvAttachment *attachment;

			attachment = ev_annotation_attachment_get_attachment (EV_ANNOTATION_ATTACHMENT (annot));
			if (attachment) {
				show_attachment = TRUE;

				g_list_free_full (ev_window->priv->attach_list,
						  g_object_unref);
				ev_window->priv->attach_list =
					g_list_prepend (ev_window->priv->attach_list,
							g_object_ref (attachment));
			}
		}
	}

	if (EV_IS_DOCUMENT_ANNOTATIONS (ev_window->priv->document))
		can_remove_annots = ev_document_annotations_can_remove_annotation (EV_DOCUMENT_ANNOTATIONS (ev_window->priv->document));

	action = g_action_map_lookup_action (G_ACTION_MAP (ev_window), "annot-properties");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), show_annot_props);

	action = g_action_map_lookup_action (G_ACTION_MAP (ev_window), "remove-annot");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), annot != NULL && can_remove_annots);

	action = g_action_map_lookup_action (G_ACTION_MAP (ev_window), "open-attachment");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), show_attachment);

	action = g_action_map_lookup_action (G_ACTION_MAP (ev_window), "save-attachment");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), show_attachment);
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

	if (!ev_window->priv->view_popup) {
		ev_window->priv->view_popup = gtk_menu_new_from_model (ev_window->priv->view_popup_menu);
		gtk_menu_attach_to_widget (GTK_MENU (ev_window->priv->view_popup),
					   GTK_WIDGET (ev_window), NULL);
	}

	gtk_menu_popup (GTK_MENU (ev_window->priv->view_popup),
			NULL, NULL, NULL, NULL,
			3, gtk_get_current_event_time ());
	return TRUE;
}

static gboolean
attachment_bar_menu_popup_cb (EvSidebarAttachments *attachbar,
			      GList                *attach_list,
			      EvWindow             *ev_window)
{
	GAction *action;

	g_assert (attach_list != NULL);

	action = g_action_map_lookup_action (G_ACTION_MAP (ev_window), "open-attachment");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);

	action = g_action_map_lookup_action (G_ACTION_MAP (ev_window), "save-attachment");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);

	g_list_free_full (ev_window->priv->attach_list, g_object_unref);
	ev_window->priv->attach_list = attach_list;

	if (!ev_window->priv->attachment_popup) {
		ev_window->priv->attachment_popup = gtk_menu_new_from_model (ev_window->priv->attachment_popup_menu);
		gtk_menu_attach_to_widget (GTK_MENU (ev_window->priv->attachment_popup),
					   GTK_WIDGET (ev_window), NULL);
	}

	gtk_menu_popup (GTK_MENU (ev_window->priv->attachment_popup),
			NULL, NULL, NULL, NULL,
			3, gtk_get_current_event_time ());

	return TRUE;
}

static void
find_sidebar_result_activated_cb (EvFindSidebar *find_sidebar,
				  gint           page,
				  gint           result,
				  EvWindow      *window)
{
	ev_view_find_set_result (EV_VIEW (window->priv->view), page, result);
}

static void
recent_view_item_activated_cb (EvRecentView *recent_view,
                               const char   *uri,
                               EvWindow     *ev_window)
{
	ev_application_open_uri_at_dest (EV_APP, uri,
					 gtk_window_get_screen (GTK_WINDOW (ev_window)),
					 NULL, 0, NULL, gtk_get_current_event_time ());
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
	ev_window_clear_find_job (ev_window);
}

/**
  * find_bar_check_refresh_rate:
  *
  * Check whether the current page should trigger an status update in the
  * find bar given its document size and the rate page.
  *
  * For documents with less pages than page_rate, it will return TRUE for
  * every page.  For documents with more pages, it will return TRUE every
  * ((total_pages / page rate) + 1).
  *
  * This slow down the update rate in the GUI, making the search more
  * responsive.
  */
static inline gboolean
find_check_refresh_rate (EvJobFind *job, gint page_rate)
{
	return ((job->current_page % (gint)((job->n_pages / page_rate) + 1)) == 0);
}

static void
ev_window_find_job_updated_cb (EvJobFind *job,
			       gint       page,
			       EvWindow  *ev_window)
{
	/* Adjust the status update when searching for a term according
	 * to the document size in pages.  For documents smaller (or equal)
	 * than 100 pages, it will be updated in every page.  A value of
	 * 100 is enough to update the find bar every 1%.
	 */
	if (find_check_refresh_rate (job, FIND_PAGE_RATE_REFRESH)) {
		ev_window_update_actions_sensitivity (ev_window);
		ev_window_update_find_status_message (ev_window);
		ev_find_sidebar_update (EV_FIND_SIDEBAR (ev_window->priv->find_sidebar));
	}
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
	ev_window_find_previous (ev_window);
}

static void
find_bar_next_cb (EggFindBar *find_bar,
		  EvWindow   *ev_window)
{
	ev_window_find_next (ev_window);
}

static void
find_bar_close_cb (EggFindBar *find_bar,
		   EvWindow   *ev_window)
{
	ev_window_close_find_bar (ev_window);
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
		ev_find_sidebar_start (EV_FIND_SIDEBAR (ev_window->priv->find_sidebar),
				       EV_JOB_FIND (ev_window->priv->find_job));

		g_signal_connect (ev_window->priv->find_job, "finished",
				  G_CALLBACK (ev_window_find_job_finished_cb),
				  ev_window);
		g_signal_connect (ev_window->priv->find_job, "updated",
				  G_CALLBACK (ev_window_find_job_updated_cb),
				  ev_window);
		ev_job_scheduler_push_job (ev_window->priv->find_job, EV_JOB_PRIORITY_NONE);
	} else {
		ev_window_update_actions_sensitivity (ev_window);
		egg_find_bar_set_status_text (find_bar, NULL);
		ev_find_sidebar_clear (EV_FIND_SIDEBAR (ev_window->priv->find_sidebar));
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
		ev_window_update_actions_sensitivity (ev_window);

		if (!visible)
			egg_find_bar_set_status_text (EGG_FIND_BAR (ev_window->priv->find_bar), NULL);
	}
}

static void
ev_window_show_find_bar (EvWindow *ev_window,
			 gboolean  restart)
{
	if (gtk_widget_get_visible (ev_window->priv->find_bar)) {
		gtk_widget_grab_focus (ev_window->priv->find_bar);
		return;
	}

	if (ev_window->priv->document == NULL || !EV_IS_DOCUMENT_FIND (ev_window->priv->document)) {
		g_error ("Find action should be insensitive since document doesn't support find");
		return;
	}

	if (EV_WINDOW_IS_PRESENTATION (ev_window))
		return;

	ev_history_freeze (ev_window->priv->history);

	g_object_ref (ev_window->priv->sidebar);
	gtk_container_remove (GTK_CONTAINER (ev_window->priv->hpaned), ev_window->priv->sidebar);
	gtk_paned_pack1 (GTK_PANED (ev_window->priv->hpaned),
			 ev_window->priv->find_sidebar, FALSE, FALSE);
	gtk_widget_show (ev_window->priv->find_sidebar);

	update_chrome_flag (ev_window, EV_CHROME_FINDBAR, TRUE);
	update_chrome_visibility (ev_window);
	gtk_widget_grab_focus (ev_window->priv->find_bar);
	g_action_group_change_action_state (G_ACTION_GROUP (ev_window), "toggle-find", g_variant_new_boolean (TRUE));

	if (restart && ev_window->priv->find_job)
		ev_window_find_restart (ev_window);
}

static void
ev_window_close_find_bar (EvWindow *ev_window)
{
	if (!gtk_widget_get_visible (ev_window->priv->find_bar))
		return;

	g_object_ref (ev_window->priv->find_sidebar);
	gtk_container_remove (GTK_CONTAINER (ev_window->priv->hpaned),
			      ev_window->priv->find_sidebar);
	gtk_paned_pack1 (GTK_PANED (ev_window->priv->hpaned),
			 ev_window->priv->sidebar, FALSE, FALSE);

	update_chrome_flag (ev_window, EV_CHROME_FINDBAR, FALSE);
	update_chrome_visibility (ev_window);
	gtk_widget_grab_focus (ev_window->priv->view);
	g_action_group_change_action_state (G_ACTION_GROUP (ev_window), "toggle-find", g_variant_new_boolean (FALSE));

	ev_history_thaw (ev_window->priv->history);
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
ev_window_set_caret_navigation_enabled (EvWindow *window,
					gboolean enabled)
{
	GAction *action;

	if (window->priv->metadata)
		ev_metadata_set_boolean (window->priv->metadata, "caret-navigation", enabled);

	ev_view_set_caret_navigation_enabled (EV_VIEW (window->priv->view), enabled);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "caret-navigation");
	g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (enabled));
}

static void
ev_window_caret_navigation_message_area_response_cb (EvMessageArea *area,
						     gint           response_id,
						     EvWindow      *window)
{
	/* Turn the caret navigation mode on */
	if (response_id == GTK_RESPONSE_YES)
		ev_window_set_caret_navigation_enabled (window, TRUE);

	/* Turn the confirmation dialog off if the user has requested not to show it again */
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (window->priv->ask_caret_navigation_check))) {
		g_settings_set_boolean (ev_window_ensure_settings (window), "show-caret-navigation-message", FALSE);
		g_settings_apply (window->priv->settings);
	}

	window->priv->ask_caret_navigation_check = NULL;
	ev_window_set_message_area (window, NULL);
	gtk_widget_grab_focus (window->priv->view);
}

static void
ev_window_cmd_view_toggle_caret_navigation (GSimpleAction *action,
					    GVariant      *state,
					    gpointer       user_data)
{
	EvWindow  *window = user_data;
	GtkWidget *message_area;
	GtkWidget *box;
	GtkWidget *hbox;
	gboolean   enabled;

	/* Don't ask for user confirmation to turn the caret navigation off when it is active,
	 * or to turn it on when the confirmation dialog is not to be shown per settings */
	enabled = ev_view_is_caret_navigation_enabled (EV_VIEW (window->priv->view));
	if (enabled || !g_settings_get_boolean (ev_window_ensure_settings (window), "show-caret-navigation-message")) {
		ev_window_set_caret_navigation_enabled (window, !enabled);
		return;
	}

	/* Ask for user confirmation to turn the caret navigation mode on */
	if (window->priv->message_area)
		return;

	message_area = ev_message_area_new (GTK_MESSAGE_QUESTION,
					    _("Enable caret navigation?"),
					    GTK_STOCK_NO,  GTK_RESPONSE_NO,
					    _("_Enable"), GTK_RESPONSE_YES,
					    NULL);
	ev_message_area_set_secondary_text (EV_MESSAGE_AREA (message_area),
					    _("Pressing F7 turns the caret navigation on or off. "
					      "This feature places a moveable cursor in text pages, "
					      "allowing you to move around and select text with your keyboard. "
					      "Do you want to enable the caret navigation?"));

	window->priv->ask_caret_navigation_check = gtk_check_button_new_with_label (_("Don't show this message again"));
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_box_pack_start (GTK_BOX (hbox), window->priv->ask_caret_navigation_check,
			    TRUE, TRUE, 0);
	gtk_widget_show_all (hbox);

	box = _ev_message_area_get_main_box (EV_MESSAGE_AREA (message_area));
	gtk_box_pack_start (GTK_BOX (box), hbox, TRUE, TRUE, 0);

	g_signal_connect (message_area, "response",
			  G_CALLBACK (ev_window_caret_navigation_message_area_response_cb),
			  window);

	gtk_widget_show (message_area);
	ev_window_set_message_area (window, message_area);
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

	if (priv->loading_message_timeout) {
		g_source_remove (priv->loading_message_timeout);
		priv->loading_message_timeout = 0;
	}

	ev_window_remove_fullscreen_timeout (window);

	if (priv->monitor) {
		g_object_unref (priv->monitor);
		priv->monitor = NULL;
	}
	
	if (priv->title) {
		ev_window_title_free (priv->title);
		priv->title = NULL;
	}

	g_clear_object (&priv->bookmarks_menu);
	g_clear_object (&priv->view_popup_menu);
	g_clear_object (&priv->attachment_popup_menu);

	if (priv->recent_manager) {
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

/*
 * GtkWindow catches keybindings for the menu items _before_ passing them to
 * the focused widget. This is unfortunate and means that pressing Ctrl+a,
 * Ctrl+left or Ctrl+right in the search bar ends up selecting text in the EvView
 * or rotating it.
 * Here we override GtkWindow's handler to do the same things that it
 * does, but in the opposite order and then we chain up to the grand
 * parent handler, skipping gtk_window_key_press_event.
 */
static gboolean
ev_window_key_press_event (GtkWidget   *widget,
			   GdkEventKey *event)
{
	static gpointer grand_parent_class = NULL;
	GtkWindow *window = GTK_WINDOW (widget);

	if (grand_parent_class == NULL)
                grand_parent_class = g_type_class_peek_parent (ev_window_parent_class);

        /* Handle focus widget key events */
        if (gtk_window_propagate_key_event (window, event))
		return TRUE;

	/* Handle mnemonics and accelerators */
	if (gtk_window_activate_key (window, event))
		return TRUE;

        /* Chain up, invokes binding set on window */
	return GTK_WIDGET_CLASS (grand_parent_class)->key_press_event (widget, event);
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
	widget_class->window_state_event = ev_window_state_event;
	widget_class->drag_data_received = ev_window_drag_data_received;

	nautilus_sendto = g_find_program_in_path ("nautilus-sendto");

	g_type_class_add_private (g_object_class, sizeof (EvWindowPrivate));
}

static const GActionEntry actions[] = {
	{ "open", ev_window_cmd_file_open },
	{ "open-copy", ev_window_cmd_file_open_copy },
	{ "save-copy", ev_window_cmd_save_as },
	{ "send-to", ev_window_cmd_send_to },
	{ "open-containing-folder", ev_window_cmd_open_containing_folder },
	{ "print", ev_window_cmd_file_print },
	{ "show-properties", ev_window_cmd_file_properties },
	{ "copy", ev_window_cmd_edit_copy },
	{ "select-all", ev_window_cmd_edit_select_all },
	{ "save-settings", ev_window_cmd_edit_save_settings },
	{ "go-previous-page", ev_window_cmd_go_previous_page },
	{ "go-next-page", ev_window_cmd_go_next_page },
	{ "go-first-page", ev_window_cmd_go_first_page },
	{ "go-last-page", ev_window_cmd_go_last_page },
	{ "go-forward", ev_window_cmd_go_forward },
	{ "go-backwards", ev_window_cmd_go_backwards },
	{ "go-back-history", ev_window_cmd_go_back_history },
	{ "go-forward-history", ev_window_cmd_go_forward_history },
	{ "find", ev_window_cmd_find },
	{ "toggle-find", NULL, NULL, "false", ev_window_cmd_toggle_find },
	{ "find-next", ev_window_cmd_edit_find_next },
	{ "find-previous", ev_window_cmd_edit_find_previous },
	{ "select-page", ev_window_cmd_focus_page_selector },
	{ "continuous", NULL, NULL, "true", ev_window_cmd_continuous },
	{ "dual-page", NULL, NULL, "false", ev_window_cmd_dual },
	{ "dual-odd-left", NULL, NULL, "false", ev_window_cmd_dual_odd_pages_left },
	{ "show-side-pane", NULL, NULL, "false", ev_window_view_cmd_toggle_sidebar },
	{ "inverted-colors", NULL, NULL, "false", ev_window_cmd_view_inverted_colors },
	{ "fullscreen", NULL, NULL, "false", ev_window_cmd_view_fullscreen },
	{ "presentation", NULL, NULL, "false", ev_window_cmd_view_presentation },
	{ "rotate-left", ev_window_cmd_edit_rotate_left },
	{ "rotate-right", ev_window_cmd_edit_rotate_right },
	{ "zoom-in", ev_window_cmd_view_zoom_in },
	{ "zoom-out", ev_window_cmd_view_zoom_out },
	{ "reload", ev_window_cmd_view_reload },
	{ "auto-scroll", ev_window_cmd_view_autoscroll },
	{ "add-bookmark", ev_window_cmd_bookmarks_add },
	{ "goto-bookmark", ev_window_activate_goto_bookmark_action, "u" },
	{ "close", ev_window_cmd_file_close_window },
	{ "scroll-forward", ev_window_cmd_scroll_forward },
	{ "scroll-backwards", ev_window_cmd_scroll_backwards },
	{ "sizing-mode", NULL, "s", "'free'", ev_window_change_sizing_mode_action_state },
	{ "zoom", ev_window_cmd_view_zoom, "d" },
	{ "escape", ev_window_cmd_escape },
	{ "open-menu", ev_window_cmd_action_menu },
	{ "caret-navigation", NULL, NULL, "false", ev_window_cmd_view_toggle_caret_navigation },
	/* Popups specific items */
	{ "open-link", ev_window_popup_cmd_open_link },
	{ "open-link-new-window", ev_window_popup_cmd_open_link_new_window },
	{ "go-to-link", ev_window_popup_cmd_open_link },
	{ "copy-link-address", ev_window_popup_cmd_copy_link_address },
	{ "save-image", ev_window_popup_cmd_save_image_as },
	{ "copy-image", ev_window_popup_cmd_copy_image },
	{ "open-attachment", ev_window_popup_cmd_open_attachment },
	{ "save-attachment", ev_window_popup_cmd_save_attachment_as },
	{ "annot-properties", ev_window_popup_cmd_annot_properties },
	{ "remove-annot", ev_window_popup_cmd_remove_annotation }
};

static void
sidebar_links_link_activated_cb (EvSidebarLinks *sidebar_links, EvLink *link, EvWindow *window)
{
	ev_view_handle_link (EV_VIEW (window->priv->view), link);
}

static void
activate_link_cb (GObject *object, EvLink *link, EvWindow *window)
{
	ev_view_handle_link (EV_VIEW (window->priv->view), link);
	gtk_widget_grab_focus (window->priv->view);
}

static void
history_changed_cb (EvHistory *history,
                    EvWindow  *window)
{
	ev_window_set_action_enabled (window, "go-back-history",
				      ev_history_can_go_back (window->priv->history));
	ev_window_set_action_enabled (window, "go-forward-history",
				      ev_history_can_go_forward (window->priv->history));
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
view_annot_removed (EvView       *view,
		    EvAnnotation *annot,
		    EvWindow     *window)
{
	ev_sidebar_annotations_annot_removed (EV_SIDEBAR_ANNOTATIONS (window->priv->sidebar_annots));
}

static void
sidebar_annots_annot_add_cancelled (EvSidebarAnnotations *sidebar_annots,
				    EvWindow             *window)
{
	ev_view_cancel_add_annotation (EV_VIEW (window->priv->view));
}

static void
sidebar_widget_model_set (EvSidebarLinks *ev_sidebar_links,
			  GParamSpec     *pspec,
			  EvWindow       *ev_window)
{
	ev_window_update_links_model (ev_window);
}

static gboolean
view_actions_focus_in_cb (GtkWidget *widget, GdkEventFocus *event, EvWindow *window)
{
#ifdef ENABLE_DBUS
	GObject *keys;

	keys = ev_application_get_media_keys (EV_APP);
	if (keys)
		ev_media_player_keys_focused (EV_MEDIA_PLAYER_KEYS (keys));
#endif /* ENABLE_DBUS */

	update_chrome_flag (window, EV_CHROME_RAISE_TOOLBAR, FALSE);
	update_chrome_visibility (window);

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
		g_action_group_activate_action (G_ACTION_GROUP (window), "go-first-page", NULL);
	} else if (g_ascii_strcasecmp (name, "PrevPage") == 0) {
		g_action_group_activate_action (G_ACTION_GROUP (window), "go-previous-page", NULL);
	} else if (g_ascii_strcasecmp (name, "NextPage") == 0) {
		g_action_group_activate_action (G_ACTION_GROUP (window), "go-next-page", NULL);
	} else if (g_ascii_strcasecmp (name, "LastPage") == 0) {
		g_action_group_activate_action (G_ACTION_GROUP (window), "go-last-page", NULL);
	} else if (g_ascii_strcasecmp (name, "GoToPage") == 0) {
		g_action_group_activate_action (G_ACTION_GROUP (window), "select-page", NULL);
	} else if (g_ascii_strcasecmp (name, "Find") == 0) {
		g_action_group_activate_action (G_ACTION_GROUP (window), "find", NULL);
	} else if (g_ascii_strcasecmp (name, "Close") == 0) {
		g_action_group_activate_action (G_ACTION_GROUP (window), "close", NULL);
	} else if (g_ascii_strcasecmp (name, "Print") == 0) {
		g_action_group_activate_action (G_ACTION_GROUP (window), "print", NULL);
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
ev_window_popup_cmd_open_link (GSimpleAction *action,
			       GVariant      *parameter,
			       gpointer       user_data)
{
	EvWindow *window = user_data;

	ev_view_handle_link (EV_VIEW (window->priv->view), window->priv->link);
}

static void
ev_window_popup_cmd_open_link_new_window (GSimpleAction *action,
					  GVariant      *parameter,
					  gpointer       user_data)
{
	EvLinkAction *ev_action = NULL;
	EvLinkDest   *dest;
	EvWindow     *window = user_data;

	ev_action = ev_link_get_action (window->priv->link);
	if (!ev_action)
		return;

	dest = ev_link_action_get_dest (ev_action);
	if (!dest)
		return;

	ev_window_open_copy_at_dest (window, dest);
}

static void
ev_window_popup_cmd_copy_link_address (GSimpleAction *action,
				       GVariant      *parameter,
				       gpointer       user_data)
{
	EvLinkAction *ev_action;
	EvWindow     *window = user_data;

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
ev_window_popup_cmd_save_image_as (GSimpleAction *action,
				   GVariant      *parameter,
				   gpointer       user_data)
{
	GtkWidget *fc;
	EvWindow  *window = user_data;

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
ev_window_popup_cmd_copy_image (GSimpleAction *action,
				GVariant      *parameter,
				gpointer       user_data)
{
	GtkClipboard *clipboard;
	GdkPixbuf    *pixbuf;
	EvWindow     *window = user_data;

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
ev_window_popup_cmd_annot_properties (GSimpleAction *action,
				      GVariant      *parameter,
				      gpointer       user_data)
{
	EvWindow                     *window = user_data;
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
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));
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
ev_window_popup_cmd_remove_annotation (GSimpleAction *action,
				       GVariant      *parameter,
				       gpointer       user_data)
{
	EvWindow *window = user_data;

	ev_view_remove_annotation (EV_VIEW (window->priv->view),
				   window->priv->annot);
}

static void
ev_window_popup_cmd_open_attachment (GSimpleAction *action,
				     GVariant      *parameter,
				     gpointer       user_data)
{
	GList     *l;
	GdkScreen *screen;
	EvWindow  *window = user_data;

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
ev_window_popup_cmd_save_attachment_as (GSimpleAction *action,
					GVariant      *parameter,
					gpointer       user_data)
{
	GtkWidget    *fc;
	EvAttachment *attachment = NULL;
	EvWindow     *window = user_data;

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
			g_action_group_activate_action (G_ACTION_GROUP (window), "go-previous-page", NULL);
	} else if (strcmp (key, "Next") == 0) {
		if (EV_WINDOW_IS_PRESENTATION (window))
			ev_view_presentation_next_page (EV_VIEW_PRESENTATION (window->priv->presentation_view));
		else
			g_action_group_activate_action (G_ACTION_GROUP (window), "go-next-page", NULL);
	} else if (strcmp (key, "FastForward") == 0) {
		g_action_group_activate_action (G_ACTION_GROUP (window), "go-last-page", NULL);
	} else if (strcmp (key, "Rewind") == 0) {
		g_action_group_activate_action (G_ACTION_GROUP (window), "go-first-page", NULL);
	}
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
	GtkBuilder *builder;
	GtkCssProvider *css_provider;
	GError *error = NULL;
	GtkWidget *sidebar_widget;
	GtkWidget *overlay;
	GObject *mpkeys;
	guint page_cache_mb;
	gboolean allow_links_change_zoom;
#ifdef ENABLE_DBUS
	GDBusConnection *connection;
	static gint window_id = 0;
#endif
	GAppInfo *app_info;

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

	ev_window->priv->history = ev_history_new (ev_window->priv->model);
	g_signal_connect (ev_window->priv->history, "activate-link",
			  G_CALLBACK (activate_link_cb),
			  ev_window);
        g_signal_connect (ev_window->priv->history, "changed",
                          G_CALLBACK (history_changed_cb),
                          ev_window);

	ev_window->priv->bookmarks_menu = g_menu_new ();

	app_info = g_app_info_get_default_for_uri_scheme ("mailto");
	ev_window->priv->has_mailto_handler = app_info != NULL;
	g_clear_object (&app_info);

	ev_window->priv->main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (ev_window), ev_window->priv->main_box);
	gtk_widget_show (ev_window->priv->main_box);

	g_action_map_add_action_entries (G_ACTION_MAP (ev_window),
					 actions, G_N_ELEMENTS (actions),
					 ev_window);

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

	ev_window->priv->toolbar = ev_toolbar_new (ev_window);
	gtk_widget_set_no_show_all (ev_window->priv->toolbar, TRUE);
	gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (ev_window->priv->toolbar), TRUE);
	gtk_window_set_titlebar (GTK_WINDOW (ev_window), ev_window->priv->toolbar);
	gtk_widget_show (ev_window->priv->toolbar);

	/* Window title */
	ev_window->priv->title = ev_window_title_new (ev_window);

	g_signal_connect (ev_toolbar_get_page_selector (EV_TOOLBAR (ev_window->priv->toolbar)),
			  "activate-link",
			  G_CALLBACK (activate_link_cb),
			  ev_window);

	/* Find Bar */
	ev_window->priv->find_bar = egg_find_bar_new ();
	gtk_style_context_add_class (gtk_widget_get_style_context (ev_window->priv->find_bar),
				     GTK_STYLE_CLASS_PRIMARY_TOOLBAR);
	gtk_box_pack_start (GTK_BOX (ev_window->priv->main_box),
			    ev_window->priv->find_bar,
			    FALSE, TRUE, 0);

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
	gtk_widget_show (sidebar_widget);
	ev_sidebar_add_page (EV_SIDEBAR (ev_window->priv->sidebar),
			     sidebar_widget);

	ev_window->priv->view_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	overlay = gtk_overlay_new ();
	ev_window->priv->scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (overlay), ev_window->priv->scrolled_window);
	gtk_widget_show (ev_window->priv->scrolled_window);

	ev_window->priv->loading_message = ev_loading_message_new ();
	gtk_widget_set_name (ev_window->priv->loading_message, "ev-loading-message");
	gtk_widget_set_halign (ev_window->priv->loading_message, GTK_ALIGN_END);
	gtk_widget_set_valign (ev_window->priv->loading_message, GTK_ALIGN_START);
	gtk_widget_set_no_show_all (ev_window->priv->loading_message, TRUE);
	gtk_overlay_add_overlay (GTK_OVERLAY (overlay), ev_window->priv->loading_message);

	gtk_box_pack_start (GTK_BOX (ev_window->priv->view_box),
			    overlay,
			    TRUE, TRUE, 0);
	gtk_widget_show (overlay);

	gtk_paned_add2 (GTK_PANED (ev_window->priv->hpaned),
			ev_window->priv->view_box);
	gtk_widget_show (ev_window->priv->view_box);

	ev_window->priv->view = ev_view_new ();
	page_cache_mb = g_settings_get_uint (ev_window_ensure_settings (ev_window),
					     GS_PAGE_CACHE_SIZE);
	ev_view_set_page_cache_size (EV_VIEW (ev_window->priv->view),
				     page_cache_mb * 1024 * 1024);
	allow_links_change_zoom = g_settings_get_boolean (ev_window_ensure_settings (ev_window),
				     GS_ALLOW_LINKS_CHANGE_ZOOM);
	ev_view_set_allow_links_change_zoom (EV_VIEW (ev_window->priv->view),
				     allow_links_change_zoom);
	ev_view_set_model (EV_VIEW (ev_window->priv->view), ev_window->priv->model);

	ev_window->priv->password_view = ev_password_view_new (GTK_WINDOW (ev_window));
	g_signal_connect_swapped (ev_window->priv->password_view,
				  "unlock",
				  G_CALLBACK (ev_window_password_view_unlock),
				  ev_window);
	g_signal_connect_object (ev_window->priv->view, "focus_in_event",
			         G_CALLBACK (view_actions_focus_in_cb),
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
	g_signal_connect_object (ev_window->priv->view, "annot-removed",
				 G_CALLBACK (view_annot_removed),
				 ev_window, 0);
	g_signal_connect_object (ev_window->priv->view, "layers-changed",
				 G_CALLBACK (view_layers_changed_cb),
				 ev_window, 0);
	g_signal_connect_object (ev_window->priv->view, "notify::is-loading",
				 G_CALLBACK (view_is_loading_changed_cb),
				 ev_window, 0);
	g_signal_connect_object (ev_window->priv->view, "cursor-moved",
				 G_CALLBACK (view_caret_cursor_moved_cb),
				 ev_window, 0);
#ifdef ENABLE_DBUS
	g_signal_connect_swapped (ev_window->priv->view, "sync-source",
				  G_CALLBACK (ev_window_sync_source),
				  ev_window);
#endif
	gtk_widget_show (ev_window->priv->view);
	gtk_widget_show (ev_window->priv->password_view);

	/* Find results sidebar */
	ev_window->priv->find_sidebar = ev_find_sidebar_new ();
	g_signal_connect (ev_window->priv->find_sidebar,
			  "result-activated",
			  G_CALLBACK (find_sidebar_result_activated_cb),
			  ev_window);

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

	/* Popups */
	builder = gtk_builder_new_from_resource ("/org/gnome/evince/shell/ui/menus.ui");
	ev_window->priv->view_popup_menu = g_object_ref (G_MENU_MODEL (gtk_builder_get_object (builder, "view-popup-menu")));
	ev_window->priv->attachment_popup_menu = g_object_ref (G_MENU_MODEL (gtk_builder_get_object (builder, "attachments-popup")));
	g_object_unref (builder);

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

	gtk_window_set_default_size (GTK_WINDOW (ev_window), 600, 600);
        gtk_window_set_hide_titlebar_when_maximized (GTK_WINDOW (ev_window), TRUE);

        ev_window_sizing_mode_changed_cb (ev_window->priv->model, NULL, ev_window);
	ev_window_update_actions_sensitivity (ev_window);

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
					      "show-menubar", FALSE,
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

GMenuModel *
ev_window_get_bookmarks_menu (EvWindow *ev_window)
{
	g_return_val_if_fail (EV_WINDOW (ev_window), NULL);

	return G_MENU_MODEL (ev_window->priv->bookmarks_menu);
}

EvHistory *
ev_window_get_history (EvWindow *ev_window)
{
	g_return_val_if_fail (EV_WINDOW (ev_window), NULL);

	return ev_window->priv->history;
}

EvDocumentModel *
ev_window_get_document_model (EvWindow *ev_window)
{
	g_return_val_if_fail (EV_WINDOW (ev_window), NULL);

	return ev_window->priv->model;
}

GtkWidget *
ev_window_get_toolbar (EvWindow *ev_window)
{
	g_return_val_if_fail (EV_WINDOW (ev_window), NULL);

	return ev_window->priv->toolbar;
}

void
ev_window_focus_view (EvWindow *ev_window)
{
	g_return_if_fail (EV_WINDOW (ev_window));

	gtk_widget_grab_focus (ev_window->priv->view);
}
