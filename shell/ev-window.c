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

#include "ev-find-sidebar.h"
#include "ev-annotations-toolbar.h"
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
#include "ev-zoom-action.h"
#include "ev-toolbar.h"
#include "ev-bookmarks.h"
#include "ev-recent-view.h"
#include "ev-search-box.h"

#ifdef ENABLE_DBUS
#include "ev-gdbus-generated.h"
#include "ev-media-player-keys.h"
#endif /* ENABLE_DBUS */

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#include <X11/extensions/XInput2.h>
#endif

#define MOUSE_BACK_BUTTON 8
#define MOUSE_FORWARD_BUTTON 9

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

typedef enum {
	EV_WINDOW_ACTION_RELOAD,
	EV_WINDOW_ACTION_CLOSE
} EvWindowAction;

typedef struct {
	/* UI */
	EvChrome chrome;

	GtkWidget *main_box;
	GtkWidget *toolbar;
	GtkWidget *hpaned;
	GtkWidget *view_box;
	GtkWidget *sidebar;
	GtkWidget *search_box;
	GtkWidget *search_bar;
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
	GtkWidget *annots_toolbar;

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
	char *display_name;
	char *edit_name;
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
	EvJob            *save_job;
	gboolean          close_after_save;

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
} EvWindowPrivate;

#define GET_PRIVATE(o) ev_window_get_instance_private (o)

#define EV_WINDOW_IS_PRESENTATION(priv) (priv->presentation_view != NULL)

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
#define LINKS_SIDEBAR_ICON EV_STOCK_OUTLINE
#define THUMBNAILS_SIDEBAR_ICON "view-grid-symbolic"
#define ATTACHMENTS_SIDEBAR_ICON "mail-attachment-symbolic"
#define LAYERS_SIDEBAR_ICON "view-paged-symbolic"
#define ANNOTS_SIDEBAR_ICON "accessories-text-editor-symbolic"
#define BOOKMARKS_SIDEBAR_ICON "user-bookmarks-symbolic"

#define EV_PRINT_SETTINGS_FILE  "print-settings"
#define EV_PRINT_SETTINGS_GROUP "Print Settings"
#define EV_PAGE_SETUP_GROUP     "Page Setup"

#define EV_TOOLBARS_FILENAME "evince-toolbar.xml"

#define TOOLBAR_RESOURCE_PATH "/org/gnome/evince/shell/ui/toolbar.xml"

#define FULLSCREEN_POPUP_TIMEOUT 2
#define FULLSCREEN_TRANSITION_DURATION 1000 /* in milliseconds */

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
static void     ev_window_save_job_cb                   (EvJob            *save,
							 EvWindow         *window);
static void     ev_window_sizing_mode_changed_cb        (EvDocumentModel  *model,
							 GParamSpec       *pspec,
							 EvWindow         *ev_window);
static void     ev_window_zoom_changed_cb 	        (EvDocumentModel  *model,
							 GParamSpec       *pspec,
							 EvWindow         *ev_window);
static void     ev_window_add_recent                    (EvWindow         *window,
							 const char       *uri);
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
static void     ev_window_popup_cmd_annotate_selected_text (GSimpleAction    *action,
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
static void     ev_window_destroy_recent_view           (EvWindow         *ev_window);
static void     recent_view_item_activated_cb           (EvRecentView     *recent_view,
                                                         const char       *uri,
                                                         EvWindow         *ev_window);
static void     ev_window_fullscreen_show_toolbar       (EvWindow         *ev_window);
static void     ev_window_begin_add_annot               (EvWindow         *ev_window,
							 EvAnnotationType  annot_type);
static void	ev_window_cancel_add_annot		(EvWindow *window);

static gchar *nautilus_sendto = NULL;

G_DEFINE_TYPE_WITH_PRIVATE (EvWindow, ev_window, GTK_TYPE_APPLICATION_WINDOW)

static gboolean
ev_window_is_recent_view (EvWindow *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	return ev_toolbar_get_mode (EV_TOOLBAR (priv->toolbar)) == EV_TOOLBAR_MODE_RECENT_VIEW;
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
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	EvDocument *document = priv->document;
	EvView     *view = EV_VIEW (priv->view);
	const EvDocumentInfo *info = NULL;
	gboolean has_document = FALSE;
	gboolean ok_to_print = TRUE;
	gboolean ok_to_copy = TRUE;
	gboolean has_properties = TRUE;
	gboolean override_restrictions = TRUE;
	gboolean can_get_text = FALSE;
	gboolean can_find = FALSE;
	gboolean can_find_in_page;
	gboolean can_annotate = FALSE;
	gboolean presentation_mode;
	gboolean recent_view_mode;
	gboolean dual_mode = FALSE;
	gboolean has_pages = FALSE;
	int      n_pages = 0, page = -1;

	if (document) {
		has_document = TRUE;
		info = ev_document_get_info (document);
		page = ev_document_model_get_page (priv->model);
		n_pages = ev_document_get_n_pages (priv->document);
		has_pages = n_pages > 0;
		dual_mode = ev_document_model_get_dual_page (priv->model);
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

	if (has_document && EV_IS_DOCUMENT_ANNOTATIONS (document)) {
		can_annotate = ev_document_annotations_can_add_annotation (EV_DOCUMENT_ANNOTATIONS (document));
	}

	if (has_document && priv->settings) {
		override_restrictions =
			g_settings_get_boolean (priv->settings,
						GS_OVERRIDE_RESTRICTIONS);
	}

	if (!override_restrictions && info && info->fields_mask & EV_DOCUMENT_INFO_PERMISSIONS) {
		ok_to_print = (info->permissions & EV_DOCUMENT_PERMISSIONS_OK_TO_PRINT);
		ok_to_copy = (info->permissions & EV_DOCUMENT_PERMISSIONS_OK_TO_COPY);
	}

	if (has_document && !ev_print_operation_exists_for_document(document))
		ok_to_print = FALSE;

	if (has_document && priv->lockdown_settings &&
	    g_settings_get_boolean (priv->lockdown_settings, GS_LOCKDOWN_SAVE)) {
		ok_to_copy = FALSE;
	}

	if (has_document && priv->lockdown_settings &&
	    g_settings_get_boolean (priv->lockdown_settings, GS_LOCKDOWN_PRINT)) {
		ok_to_print = FALSE;
	}

	/* Get modes */
	presentation_mode = EV_WINDOW_IS_PRESENTATION (priv);
	recent_view_mode = ev_window_is_recent_view (ev_window);

	/* File menu */
	ev_window_set_action_enabled (ev_window, "open-copy", has_document);
	ev_window_set_action_enabled (ev_window, "save-as", has_document &&
				      ok_to_copy && !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "print", has_pages &&
				      ok_to_print && !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "show-properties",
				      has_document && has_properties &&
				      !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "open-containing-folder",
				      has_document && !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "send-to", has_document &&
				      priv->has_mailto_handler &&
	                              nautilus_sendto &&
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
	ev_window_set_action_enabled (ev_window, "toggle-edit-annots", can_annotate &&
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
	ev_window_set_action_enabled (ev_window, "rtl", has_pages &&
				      !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "reload", has_pages &&
				      !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "auto-scroll", has_pages &&
				      !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "inverted-colors",
				      has_pages && !recent_view_mode);
#if WITH_GSPELL
	ev_window_set_action_enabled (ev_window, "enable-spellchecking", TRUE);
#else
	ev_window_set_action_enabled (ev_window, "enable-spellchecking", FALSE);
#endif

	/* Bookmarks menu */
	ev_window_set_action_enabled (ev_window, "add-bookmark",
				      has_pages && priv->bookmarks &&
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

	/* Don't enable popup actions here because the page can change while a
	 * popup is visible due to kinetic scrolling. The 'popup' functions
	 * will enable appropriate actions when the popup is shown. */
	if (recent_view_mode) {
		ev_window_set_action_enabled (ev_window, "annotate-selected-text", FALSE);
		ev_window_set_action_enabled (ev_window, "open-link", FALSE);
		ev_window_set_action_enabled (ev_window, "open-link-new-window", FALSE);
		ev_window_set_action_enabled (ev_window, "go-to-link", FALSE);
		ev_window_set_action_enabled (ev_window, "copy-link-address", FALSE);
		ev_window_set_action_enabled (ev_window, "save-image", FALSE);
		ev_window_set_action_enabled (ev_window, "copy-image", FALSE);
		ev_window_set_action_enabled (ev_window, "open-attachment", FALSE);
		ev_window_set_action_enabled (ev_window, "save-attachment", FALSE);
		ev_window_set_action_enabled (ev_window, "annot-properties", FALSE);
		ev_window_set_action_enabled (ev_window, "remove-annot", FALSE);
	}

	can_find_in_page = ev_search_box_has_results (EV_SEARCH_BOX (priv->search_box));

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
				      has_pages && !recent_view_mode);

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
				      !ev_history_is_frozen (priv->history) &&
				      ev_history_can_go_back (priv->history) &&
				      !recent_view_mode);
	ev_window_set_action_enabled (ev_window, "go-forward-history",
				      !ev_history_is_frozen (priv->history) &&
				      ev_history_can_go_forward (priv->history) &&
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
	EvWindowPrivate *priv = GET_PRIVATE (window);
	gboolean toolbar, sidebar;
	gboolean presentation;

	presentation = EV_WINDOW_IS_PRESENTATION (priv);

	toolbar = ((priv->chrome & EV_CHROME_TOOLBAR) != 0  || 
		   (priv->chrome & EV_CHROME_RAISE_TOOLBAR) != 0) && !presentation;
	sidebar = (priv->chrome & EV_CHROME_SIDEBAR) != 0 && priv->document && !presentation;

	set_widget_visibility (priv->toolbar, toolbar);
	set_widget_visibility (priv->sidebar, sidebar);

	if (toolbar && ev_document_model_get_fullscreen (priv->model))
		ev_window_fullscreen_show_toolbar (window);
}

static void
update_chrome_flag (EvWindow *window, EvChrome flag, gboolean active)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);

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
	EvWindowPrivate *priv = GET_PRIVATE (window);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "sizing-mode");

	switch (ev_document_model_get_sizing_mode (priv->model)) {
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
	EvWindowPrivate *priv;

	g_return_val_if_fail (EV_IS_WINDOW (ev_window), FALSE);

	priv = GET_PRIVATE (EV_WINDOW (ev_window));

	return (priv->document == NULL) &&
		(priv->load_job == NULL);
}

static void
ev_window_set_message_area (EvWindow  *window,
			    GtkWidget *area)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);

	if (priv->message_area == area)
		return;

	if (priv->message_area)
		gtk_widget_destroy (priv->message_area);
	priv->message_area = area;

	if (!area)
		return;

	gtk_box_pack_start (GTK_BOX (priv->main_box),
			    priv->message_area,
			    FALSE, FALSE, 0);
        /* Pack the message area right after the search bar */
        gtk_box_reorder_child (GTK_BOX (priv->main_box),
                               priv->message_area, 2);
	g_object_add_weak_pointer (G_OBJECT (priv->message_area),
				   (gpointer) &(priv->message_area));
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
	EvWindowPrivate *priv = GET_PRIVATE (window);

	if (priv->message_area)
		return;

	va_start (args, format);
	msg = g_strdup_vprintf (format, args);
	va_end (args);
	
	area = ev_message_area_new (GTK_MESSAGE_ERROR,
				    msg,
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
	EvWindowPrivate *priv = GET_PRIVATE (window);

	if (priv->message_area)
		return;

	va_start (args, format);
	msg = g_strdup_vprintf (format, args);
	va_end (args);

	area = ev_message_area_new (GTK_MESSAGE_WARNING,
				    msg,
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
	EvWindowPrivate *priv = GET_PRIVATE (window);

	priv->loading_message_timeout = 0;
	gtk_widget_show (priv->loading_message);

	return FALSE;
}

static void
ev_window_show_loading_message (EvWindow *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);

	if (priv->loading_message_timeout)
		return;
	priv->loading_message_timeout =
		g_timeout_add_full (G_PRIORITY_LOW, 0.5, (GSourceFunc)show_loading_message_cb, window, NULL);
}

static void
ev_window_hide_loading_message (EvWindow *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);

	if (priv->loading_message_timeout) {
		g_source_remove (priv->loading_message_timeout);
		priv->loading_message_timeout = 0;
	}

	gtk_widget_hide (priv->loading_message);
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
	EvWindowPrivate *priv = GET_PRIVATE (window);

	if (EV_IS_DOCUMENT_LINKS (priv->document) &&
	    ev_document_links_has_document_links (EV_DOCUMENT_LINKS (priv->document))) {
		LinkTitleData data;
		GtkTreeModel *model;

		data.link = link;
		data.link_title = NULL;

		g_object_get (G_OBJECT (priv->sidebar_links),
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
	EvWindowPrivate *priv = GET_PRIVATE (window);
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
			page_label = ev_document_links_get_dest_page_label (EV_DOCUMENT_LINKS (priv->document), dest);
			if (!page_label)
				return;

			title = g_strdup_printf (_("Page %s"), page_label);
			g_free (page_label);

			new_link = ev_link_new (title, action);
			g_free (title);
		}
	}
	ev_history_add_link (priv->history, new_link ? new_link : link);
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
	EvWindowPrivate *priv = GET_PRIVATE (window);

	ev_sidebar_layers_update_layers_state (EV_SIDEBAR_LAYERS (priv->sidebar_layers));
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
	EvWindowPrivate *priv = GET_PRIVATE (window);

	if (!priv->metadata)
		return;

	position = g_variant_new ("(uu)", page, offset);
	caret_position = g_variant_print (position, FALSE);
	g_variant_unref (position);

	ev_metadata_set_string (priv->metadata, "caret-position", caret_position);
	g_free (caret_position);
}

static void
ev_window_page_changed_cb (EvWindow        *ev_window,
			   gint             old_page,
			   gint             new_page,
			   EvDocumentModel *model)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	ev_window_update_actions_sensitivity (ev_window);

	if (priv->metadata && !ev_window_is_empty (ev_window))
		ev_metadata_set_int (priv->metadata, "page", new_page);
}

static const gchar *
ev_window_sidebar_get_current_page_id (EvWindow *ev_window)
{
	GtkWidget   *current_page;
	const gchar *id;
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	g_object_get (priv->sidebar,
		      "current_page", &current_page,
		      NULL);

	if (current_page == priv->sidebar_links) {
		id = LINKS_SIDEBAR_ID;
	} else if (current_page == priv->sidebar_thumbs) {
		id = THUMBNAILS_SIDEBAR_ID;
	} else if (current_page == priv->sidebar_attachments) {
		id = ATTACHMENTS_SIDEBAR_ID;
	} else if (current_page == priv->sidebar_layers) {
		id = LAYERS_SIDEBAR_ID;
	} else if (current_page == priv->sidebar_annots) {
		id = ANNOTS_SIDEBAR_ID;
	} else if (current_page == priv->sidebar_bookmarks) {
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
	EvWindowPrivate *priv = GET_PRIVATE (window);
	EvDocument *document = priv->document;
	EvSidebar  *sidebar = EV_SIDEBAR (priv->sidebar);
	GtkWidget  *links = priv->sidebar_links;
	GtkWidget  *thumbs = priv->sidebar_thumbs;
	GtkWidget  *attachments = priv->sidebar_attachments;
	GtkWidget  *annots = priv->sidebar_annots;
	GtkWidget  *layers = priv->sidebar_layers;
	GtkWidget  *bookmarks = priv->sidebar_bookmarks;

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
	EvWindowPrivate *priv = GET_PRIVATE (window);
	GSettings  *settings = priv->default_settings;
	EvMetadata *metadata = priv->metadata;

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
	if (!ev_metadata_has_key (metadata, "rtl")) {
		ev_metadata_set_boolean (metadata, "rtl",
					 gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL ? TRUE : FALSE);
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
	EvWindowPrivate *priv = GET_PRIVATE (window);

	if (!priv->metadata)
		return;

	if (ev_metadata_get_boolean (priv->metadata, "show_toolbar", &show_toolbar))
		update_chrome_flag (window, EV_CHROME_TOOLBAR, show_toolbar);
	if (ev_metadata_get_boolean (priv->metadata, "sidebar_visibility", &show_sidebar))
		update_chrome_flag (window, EV_CHROME_SIDEBAR, show_sidebar);
	update_chrome_visibility (window);
}

static void
setup_sidebar_from_metadata (EvWindow *window)
{
	gchar *page_id;
	gint   sidebar_size;
	EvWindowPrivate *priv = GET_PRIVATE (window);

	if (!priv->metadata)
		return;

	if (ev_metadata_get_int (priv->metadata, "sidebar_size", &sidebar_size))
		gtk_paned_set_position (GTK_PANED (priv->hpaned), sidebar_size);

	if (ev_metadata_get_string (priv->metadata, "sidebar_page", &page_id))
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
	gboolean rtl = FALSE;
	gboolean fullscreen = FALSE;
	EvWindowPrivate *priv = GET_PRIVATE (window);

	if (!priv->metadata)
		return;

	/* Current page */
	if (!priv->dest &&
	    ev_metadata_get_int (priv->metadata, "page", &page)) {
		ev_document_model_set_page (priv->model, page);
	}

	/* Sizing mode */
	if (ev_metadata_get_string (priv->metadata, "sizing_mode", &sizing_mode)) {
		GEnumValue *enum_value;

		enum_value = g_enum_get_value_by_nick
			(g_type_class_peek (EV_TYPE_SIZING_MODE), sizing_mode);
		ev_document_model_set_sizing_mode (priv->model, enum_value->value);
	}

	/* Zoom */
	if (ev_document_model_get_sizing_mode (priv->model) == EV_SIZING_FREE) {
		if (ev_metadata_get_double (priv->metadata, "zoom", &zoom)) {
			zoom *= ev_document_misc_get_widget_dpi  (GTK_WIDGET (window)) / 72.0;
			ev_document_model_set_scale (priv->model, zoom);
		}
	}

	/* Rotation */
	if (ev_metadata_get_int (priv->metadata, "rotation", &rotation)) {
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
		ev_document_model_set_rotation (priv->model, rotation);
	}

	/* Inverted Colors */
	if (ev_metadata_get_boolean (priv->metadata, "inverted-colors", &inverted_colors)) {
		ev_document_model_set_inverted_colors (priv->model, inverted_colors);
	}

	/* Continuous */
	if (ev_metadata_get_boolean (priv->metadata, "continuous", &continuous)) {
		ev_document_model_set_continuous (priv->model, continuous);
	}

	/* Dual page */
	if (ev_metadata_get_boolean (priv->metadata, "dual-page", &dual_page)) {
		ev_document_model_set_dual_page (priv->model, dual_page);
	}

	/* Dual page odd pages left */
	if (ev_metadata_get_boolean (priv->metadata, "dual-page-odd-left", &dual_page_odd_left)) {
		ev_document_model_set_dual_page_odd_pages_left (priv->model, dual_page_odd_left);
	}

	/* Right to left document */
	if (ev_metadata_get_boolean (priv->metadata, "rtl", &rtl)) {
		ev_document_model_set_rtl (priv->model, rtl);
	}

	/* Fullscreen */
	if (ev_metadata_get_boolean (priv->metadata, "fullscreen", &fullscreen)) {
		if (fullscreen)
			ev_window_run_fullscreen (window);
	}
}

static void
monitor_get_dimesions (EvWindow *ev_window,
                       gint     *width,
                       gint     *height)
{
	GdkDisplay  *display;
	GdkWindow   *gdk_window;
	GdkMonitor  *monitor;
	GdkRectangle geometry;

	*width = 0;
	*height = 0;

	display = gtk_widget_get_display (GTK_WIDGET (ev_window));
	gdk_window = gtk_widget_get_window (GTK_WIDGET (ev_window));

	if (gdk_window) {
		monitor = gdk_display_get_monitor_at_window (display,
							     gdk_window);
		gdk_monitor_get_workarea (monitor, &geometry);
		*width = geometry.width;
		*height = geometry.height;
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
	EvWindowPrivate *priv = GET_PRIVATE (window);

	if (!priv->metadata)
		return;

	setup_sidebar_from_metadata (window);

	/* Make sure to not open a document on the last page,
	 * since closing it on the last page most likely means the
	 * user was finished reading the document. In that case, reopening should
	 * show the first page. */
	page = ev_document_model_get_page (priv->model);
	n_pages = ev_document_get_n_pages (priv->document);
	if (page == n_pages - 1)
		ev_document_model_set_page (priv->model, 0);

	if (ev_metadata_get_int (priv->metadata, "window_width", &width) &&
	    ev_metadata_get_int (priv->metadata, "window_height", &height))
		return; /* size was already set in setup_size_from_metadata */

	/* Following code is intended to be executed first time a document is opened
	 * in Evince, that's why is located *after* the previous return that exits
	 * when evince metadata for window_width{height} already exists. */
	if (n_pages == 1)
		ev_document_model_set_dual_page (priv->model, FALSE);
	else if (n_pages == 2)
		ev_document_model_set_dual_page_odd_pages_left (priv->model, TRUE);

	g_settings_get (priv->default_settings, "window-ratio", "(dd)", &width_ratio, &height_ratio);
	if (width_ratio > 0. && height_ratio > 0.) {
		gdouble    document_width;
		gdouble    document_height;
		gint       request_width;
		gint       request_height;
		gint       monitor_width;
		gint       monitor_height;

		ev_document_get_max_page_size (priv->document,
					       &document_width, &document_height);

		request_width = (gint)(width_ratio * document_width + 0.5);
		request_height = (gint)(height_ratio * document_height + 0.5);

		monitor_get_dimesions (window, &monitor_width, &monitor_height);
		if (monitor_width > 0 && monitor_height > 0) {
			request_width = MIN (request_width, monitor_width);
			request_height = MIN (request_height, monitor_height);
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
	EvWindowPrivate *priv = GET_PRIVATE (window);

	if (!priv->metadata)
		return;

	if (ev_metadata_get_int (priv->metadata, "window_x", &x) &&
	    ev_metadata_get_int (priv->metadata, "window_y", &y)) {
		gtk_window_move (GTK_WINDOW (window), x, y);
	}

        if (ev_metadata_get_int (priv->metadata, "window_width", &width) &&
	    ev_metadata_get_int (priv->metadata, "window_height", &height)) {
		gtk_window_resize (GTK_WINDOW (window), width, height);
	}

	if (ev_metadata_get_boolean (priv->metadata, "window_maximized", &maximized)) {
		if (maximized) {
			gtk_window_maximize (GTK_WINDOW (window));
		} else {
			gtk_window_unmaximize (GTK_WINDOW (window));
		}
	}
}

static void
setup_view_from_metadata (EvWindow *window)
{
	gboolean presentation;
	EvWindowPrivate *priv = GET_PRIVATE (window);

	if (!priv->metadata)
		return;

	/* Presentation */
	if (ev_metadata_get_boolean (priv->metadata, "presentation", &presentation)) {
		if (presentation)
			ev_window_run_presentation (window);
	}

	/* Caret navigation mode */
	if (ev_view_supports_caret_navigation (EV_VIEW (priv->view))) {
		gboolean caret_navigation;
		gchar   *caret_position;

		if (ev_metadata_get_string (priv->metadata, "caret-position", &caret_position)) {
			GVariant *position;

			position = g_variant_parse (G_VARIANT_TYPE ("(uu)"), caret_position, NULL, NULL, NULL);
			if (position) {
				guint page, offset;

				g_variant_get (position, "(uu)", &page, &offset);
				g_variant_unref (position);

				ev_view_set_caret_cursor_position (EV_VIEW (priv->view),
								   page, offset);
			}
		}

		if (ev_metadata_get_boolean (priv->metadata, "caret-navigation", &caret_navigation))
			ev_view_set_caret_navigation_enabled (EV_VIEW (priv->view), caret_navigation);
	}
}

static void
page_cache_size_changed (GSettings *settings,
			 gchar     *key,
			 EvWindow  *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	guint page_cache_mb;

	page_cache_mb = g_settings_get_uint (settings, GS_PAGE_CACHE_SIZE);
	ev_view_set_page_cache_size (EV_VIEW (priv->view),
				     page_cache_mb * 1024 * 1024);
}

static void
allow_links_change_zoom_changed (GSettings *settings,
			 gchar     *key,
			 EvWindow  *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	gboolean allow_links_change_zoom = g_settings_get_boolean (settings, GS_ALLOW_LINKS_CHANGE_ZOOM);

	ev_view_set_allow_links_change_zoom (EV_VIEW (priv->view), allow_links_change_zoom);
}

static void
ev_window_setup_default (EvWindow *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	EvDocumentModel *model = priv->model;
	GSettings       *settings = priv->default_settings;

	/* Chrome */
	update_chrome_flag (ev_window, EV_CHROME_SIDEBAR,
			    g_settings_get_boolean (settings, "show-sidebar"));
	update_chrome_visibility (ev_window);

	/* Sidebar */
	gtk_paned_set_position (GTK_PANED (priv->hpaned),
				g_settings_get_int (settings, "sidebar-size"));

	/* Document model */
	ev_document_model_set_continuous (model, g_settings_get_boolean (settings, "continuous"));
	ev_document_model_set_dual_page (model, g_settings_get_boolean (settings, "dual-page"));
	ev_document_model_set_dual_page_odd_pages_left (model, g_settings_get_boolean (settings, "dual-page-odd-left"));
	ev_document_model_set_rtl (model, gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL ? TRUE : FALSE);
	ev_document_model_set_inverted_colors (model, g_settings_get_boolean (settings, "inverted-colors"));
	ev_document_model_set_sizing_mode (model, g_settings_get_enum (settings, "sizing-mode"));
	if (ev_document_model_get_sizing_mode (model) == EV_SIZING_FREE)
		ev_document_model_set_scale (model, g_settings_get_double (settings, "zoom"));

	g_simple_action_set_state (
		G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (ev_window),
		                                             "enable-spellchecking")),
		g_variant_new_boolean (
#ifdef WITH_GSPELL
		g_settings_get_boolean (settings, "enable-spellchecking")
#else
		FALSE
#endif
		)
	);
	ev_view_set_enable_spellchecking (EV_VIEW (priv->view),
		g_settings_get_boolean (settings, "enable-spellchecking"));
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
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

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
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	EvDocument *document = priv->document;

	priv->setup_document_idle = 0;

	ev_window_set_page_mode (ev_window, PAGE_MODE_DOCUMENT);
	ev_window_title_set_document (priv->title, document);
	ev_window_title_set_filename (priv->title,
				      priv->display_name);

        ev_window_ensure_settings (ev_window);

#ifdef HAVE_DESKTOP_SCHEMAS
	if (!priv->lockdown_settings) {
		priv->lockdown_settings = g_settings_new (GS_LOCKDOWN_SCHEMA_NAME);
		g_signal_connect (priv->lockdown_settings,
				  "changed",
				  G_CALLBACK (lockdown_changed),
				  ev_window);
	}
#endif

	ev_window_update_actions_sensitivity (ev_window);

	if (priv->properties) {
		ev_properties_dialog_set_document (EV_PROPERTIES_DIALOG (priv->properties),
						   priv->uri,
					           priv->document);
	}

	info = ev_document_get_info (document);
	update_document_mode (ev_window, info->mode);

	if (priv->search_string && EV_IS_DOCUMENT_FIND (document) &&
	    !EV_WINDOW_IS_PRESENTATION (priv)) {
		GtkSearchEntry *entry;

		ev_window_show_find_bar (ev_window, FALSE);
		entry = ev_search_box_get_entry (EV_SEARCH_BOX (priv->search_box));
		gtk_entry_set_text (GTK_ENTRY (entry), priv->search_string);
	}

	g_clear_pointer (&priv->search_string, g_free);

	if (EV_WINDOW_IS_PRESENTATION (priv))
		gtk_widget_grab_focus (priv->presentation_view);
	else if (!gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (priv->search_bar)))
		gtk_widget_grab_focus (priv->view);

	return FALSE;
}

static void
ev_window_set_document_metadata (EvWindow *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);
	const EvDocumentInfo *info;

	if (!priv->metadata)
		return;

	info = ev_document_get_info (priv->document);
	if (info->fields_mask & EV_DOCUMENT_INFO_TITLE && info->title && info->title[0] != '\0')
		ev_metadata_set_string (priv->metadata, "title", info->title);
	else
		ev_metadata_set_string (priv->metadata, "title", "");

	if (info->fields_mask & EV_DOCUMENT_INFO_AUTHOR && info->author && info->author[0] != '\0')
		ev_metadata_set_string (priv->metadata, "author", info->author);
	else
		ev_metadata_set_string (priv->metadata, "author", "");
}

static void
ev_window_set_document (EvWindow *ev_window, EvDocument *document)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	if (priv->document == document)
		return;

	if (priv->document)
		g_object_unref (priv->document);
	priv->document = g_object_ref (document);

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

	ev_toolbar_set_mode (EV_TOOLBAR (priv->toolbar),
			     EV_TOOLBAR_MODE_NORMAL);
	ev_window_title_set_type (priv->title, EV_WINDOW_TITLE_DOCUMENT);
	ev_window_update_actions_sensitivity (ev_window);

	if (EV_WINDOW_IS_PRESENTATION (priv)) {
		gint current_page;

		current_page = ev_view_presentation_get_current_page (
			EV_VIEW_PRESENTATION (priv->presentation_view));
		gtk_widget_destroy (priv->presentation_view);
		priv->presentation_view = NULL;

		/* Update the model with the current presentation page */
		ev_document_model_set_page (priv->model, current_page);
		ev_window_run_presentation (ev_window);
	}

	if (priv->setup_document_idle > 0)
		g_source_remove (priv->setup_document_idle);

	priv->setup_document_idle = g_idle_add ((GSourceFunc)ev_window_setup_document, ev_window);
}

static void
ev_window_file_changed (EvWindow *ev_window,
			gpointer  user_data)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	if (priv->settings &&
	    g_settings_get_boolean (priv->settings, GS_AUTO_RELOAD))
		ev_window_reload_document (ev_window, NULL);
}

static void
ev_window_password_view_unlock (EvWindow *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	const gchar *password;

	g_assert (priv->load_job);

	password = ev_password_view_get_password (EV_PASSWORD_VIEW (priv->password_view));
	ev_job_load_set_password (EV_JOB_LOAD (priv->load_job), password);
	ev_job_scheduler_push_job (priv->load_job, EV_JOB_PRIORITY_NONE);
}

static void
ev_window_clear_load_job (EvWindow *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	if (priv->load_job != NULL) {
		if (!ev_job_is_finished (priv->load_job))
			ev_job_cancel (priv->load_job);

		g_signal_handlers_disconnect_by_func (priv->load_job, ev_window_load_job_cb, ev_window);
		g_object_unref (priv->load_job);
		priv->load_job = NULL;
	}
}

static void
ev_window_clear_reload_job (EvWindow *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	if (priv->reload_job != NULL) {
		if (!ev_job_is_finished (priv->reload_job))
			ev_job_cancel (priv->reload_job);
		
		g_signal_handlers_disconnect_by_func (priv->reload_job, ev_window_reload_job_cb, ev_window);
		g_object_unref (priv->reload_job);
		priv->reload_job = NULL;
	}
}

static void
ev_window_clear_local_uri (EvWindow *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	if (priv->local_uri) {
		ev_tmp_uri_unlink (priv->local_uri);
		g_free (priv->local_uri);
		priv->local_uri = NULL;
	}
}

static void
ev_window_handle_link (EvWindow *ev_window,
		       EvLinkDest *dest)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	if (dest) {
		EvLink *link;
		EvLinkAction *link_action;

		link_action = ev_link_action_new_dest (dest);
		link = ev_link_new (NULL, link_action);
		ev_view_handle_link (EV_VIEW (priv->view), link);
		g_object_unref (link_action);
		g_object_unref (link);
	}
}

/* This callback will executed when load job will be finished.
 *
 * Since the flow of the error dialog is very confusing, we assume that both
 * document and uri will go away after this function is called, and thus we need
 * to ref/dup them.  Additionally, it needs to clear
 * priv->password_{uri,document}, and thus people who call this
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
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	g_assert (job_load->uri);

	ev_window_hide_loading_message (ev_window);

	/* Success! */
	if (!ev_job_is_failed (job)) {
		ev_document_model_set_document (priv->model, document);

#ifdef ENABLE_DBUS
		ev_window_emit_doc_loaded (ev_window);
#endif
		setup_chrome_from_metadata (ev_window);
		setup_document_from_metadata (ev_window);
		setup_view_from_metadata (ev_window);

		ev_window_add_recent (ev_window, priv->uri);

		ev_window_title_set_type (priv->title,
					  EV_WINDOW_TITLE_DOCUMENT);
		if (job_load->password) {
			GPasswordSave flags;

			flags = ev_password_view_get_password_save_flags (
				EV_PASSWORD_VIEW (priv->password_view));
			ev_keyring_save_password (priv->uri,
						  job_load->password,
						  flags);
		}

		ev_window_handle_link (ev_window, priv->dest);
		g_clear_object (&priv->dest);

		switch (priv->window_mode) {
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
		priv->monitor = ev_file_monitor_new (priv->uri);
		g_signal_connect_swapped (priv->monitor, "changed",
					  G_CALLBACK (ev_window_file_changed),
					  ev_window);
		
		ev_window_clear_load_job (ev_window);
		return;
	}

	if (g_error_matches (job->error, EV_DOCUMENT_ERROR, EV_DOCUMENT_ERROR_ENCRYPTED) &&
	    EV_IS_DOCUMENT_SECURITY (document)) {
		gchar *password;
		
		setup_view_from_metadata (ev_window);
		
		/* First look whether password is in keyring */
		password = ev_keyring_lookup_password (priv->uri);
		if (password) {
			if (job_load->password && strcmp (password, job_load->password) == 0) {
				/* Password in keyring is wrong */
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
		ev_window_title_set_filename (priv->title,
					      priv->display_name);
		ev_window_title_set_type (priv->title,
					  EV_WINDOW_TITLE_PASSWORD);

		ev_password_view_set_filename (EV_PASSWORD_VIEW (priv->password_view),
					       priv->display_name);

		ev_window_set_page_mode (ev_window, PAGE_MODE_PASSWORD);

		ev_job_load_set_password (job_load, NULL);
		ev_password_view_ask_password (EV_PASSWORD_VIEW (priv->password_view));
	} else {
		ev_toolbar_set_mode (EV_TOOLBAR (priv->toolbar),
				     EV_TOOLBAR_MODE_RECENT_VIEW);
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
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	if (ev_job_is_failed (job)) {
		ev_window_clear_reload_job (ev_window);
		priv->in_reload = FALSE;
		if (priv->dest) {
			g_object_unref (priv->dest);
			priv->dest = NULL;
		}

		return;
	}

	ev_document_model_set_document (priv->model,
					job->document);
	if (priv->dest) {
		ev_window_handle_link (ev_window, priv->dest);
		g_clear_object (&priv->dest);
	}

	/* Restart the search after reloading */
	if (gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (priv->search_bar)))
		ev_search_box_restart (EV_SEARCH_BOX (priv->search_box));

	ev_window_clear_reload_job (ev_window);
	priv->in_reload = FALSE;
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
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	return priv->uri;
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
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	if (priv->print_dialog)
		gtk_widget_destroy (priv->print_dialog);
	priv->print_dialog = NULL;
	
	if (priv->properties)
		gtk_widget_destroy (priv->properties);
	priv->properties = NULL;
}

static void
ev_window_clear_progress_idle (EvWindow *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	if (priv->progress_idle > 0)
		g_source_remove (priv->progress_idle);
	priv->progress_idle = 0;
}

static void
reset_progress_idle (EvWindow *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	priv->progress_idle = 0;
}

static void
ev_window_show_progress_message (EvWindow   *ev_window,
				 guint       interval,
				 GSourceFunc function)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	if (priv->progress_idle > 0)
		g_source_remove (priv->progress_idle);
	priv->progress_idle =
		g_timeout_add_seconds_full (G_PRIORITY_DEFAULT,
					    interval, function,
					    ev_window,
					    (GDestroyNotify)reset_progress_idle);
}

static void
ev_window_reset_progress_cancellable (EvWindow *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	if (priv->progress_cancellable)
		g_cancellable_reset (priv->progress_cancellable);
	else
		priv->progress_cancellable = g_cancellable_new ();
}

static void
ev_window_progress_response_cb (EvProgressMessageArea *area,
				gint                   response,
				EvWindow              *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	if (response == GTK_RESPONSE_CANCEL)
		g_cancellable_cancel (priv->progress_cancellable);
	ev_window_set_message_area (ev_window, NULL);
}

static gboolean
show_loading_progress (EvWindow *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	GtkWidget *area;
	gchar     *text;
	gchar 	  *display_name;

	if (priv->message_area)
		return FALSE;

	text = g_uri_unescape_string (priv->uri, NULL);
	display_name = g_markup_escape_text (text, -1);
	g_free (text);
	text = g_strdup_printf (_("Loading document from “%s”"),
				display_name);

	area = ev_progress_message_area_new ("document-open-symbolic",
					     text,
					     _("C_ancel"),
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
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	gchar *text;
	gchar *display_name;

	ev_window_hide_loading_message (ev_window);
	priv->in_reload = FALSE;

	text = g_uri_unescape_string (priv->local_uri, NULL);
	display_name = g_markup_escape_text (text, -1);
	g_free (text);
	ev_window_error_message (ev_window, error,
				 _("Unable to open document “%s”."),
				 display_name);
	g_free (display_name);
	g_free (priv->local_uri);
	priv->local_uri = NULL;
	priv->uri_mtime = 0;
}

static void
set_uri_mtime (GFile        *source,
	       GAsyncResult *async_result,
	       EvWindow     *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	GFileInfo *info;
	GError *error = NULL;

	info = g_file_query_info_finish (source, async_result, &error);

	if (error) {
		priv->uri_mtime = 0;
		g_error_free (error);
	} else {
		GTimeVal mtime;

		g_file_info_get_modification_time (info, &mtime);
		priv->uri_mtime = mtime.tv_sec;
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
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	GError *error = NULL;

	ev_window_clear_progress_idle (ev_window);
	ev_window_set_message_area (ev_window, NULL);

	g_file_copy_finish (source, async_result, &error);
	if (!error) {
		ev_job_scheduler_push_job (priv->load_job, EV_JOB_PRIORITY_NONE);
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
		g_free (priv->uri);
		priv->uri = NULL;
		g_clear_pointer (&priv->display_name, g_free);
		g_clear_pointer (&priv->edit_name, g_free);
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
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	gchar *status;
	gdouble fraction;
	
	if (!priv->message_area)
		return;

	if (total_bytes <= 0)
		return;

	fraction = n_bytes / (gdouble)total_bytes;
	status = g_strdup_printf (_("Downloading document (%d%%)"),
				  (gint)(fraction * 100));
	
	ev_progress_message_area_set_status (EV_PROGRESS_MESSAGE_AREA (priv->message_area),
					     status);
	ev_progress_message_area_set_fraction (EV_PROGRESS_MESSAGE_AREA (priv->message_area),
					       fraction);

	g_free (status);
}

static void
ev_window_load_file_remote (EvWindow *ev_window,
			    GFile    *source_file)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	GFile *target_file;

	if (!priv->local_uri) {
		char *base_name, *template;
                GFile *tmp_file;
                GError *err = NULL;

		/* We'd like to keep extension of source uri since
		 * it helps to resolve some mime types, say cbz.
                 */
		base_name = priv->edit_name;
                template = g_strdup_printf ("document.XXXXXX-%s", base_name);

                tmp_file = ev_mkstemp_file (template, &err);
		g_free (template);
                if (tmp_file == NULL) {
                        ev_window_error_message (ev_window, err,
                                                 "%s", _("Failed to load remote file."));
                        g_error_free (err);
                        return;
                }

		priv->local_uri = g_file_get_uri (tmp_file);
		g_object_unref (tmp_file);

		ev_job_load_set_uri (EV_JOB_LOAD (priv->load_job),
				     priv->local_uri);
	}

	ev_window_reset_progress_cancellable (ev_window);

	target_file = g_file_new_for_uri (priv->local_uri);
	g_file_copy_async (source_file, target_file,
			   G_FILE_COPY_OVERWRITE,
			   G_PRIORITY_DEFAULT,
			   priv->progress_cancellable,
			   (GFileProgressCallback)window_open_file_copy_progress_cb,
			   ev_window, 
			   (GAsyncReadyCallback)window_open_file_copy_ready_cb,
			   ev_window);
	g_object_unref (target_file);

	ev_window_show_progress_message (ev_window, 1,
					 (GSourceFunc)show_loading_progress);
}

static void
set_filenames (EvWindow *ev_window, GFile *f)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	GFileInfo       *info;
	GError          *error = NULL;

	g_clear_pointer (&priv->display_name, g_free);
	g_clear_pointer (&priv->edit_name, g_free);

	info = g_file_query_info (f,
				  G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
				  G_FILE_ATTRIBUTE_STANDARD_EDIT_NAME,
			          G_FILE_QUERY_INFO_NONE, NULL, &error);
	if (info) {
		priv->display_name = g_strdup (g_file_info_get_display_name (info));
		priv->edit_name = g_strdup (g_file_info_get_edit_name (info));
		g_object_unref (info);
	} else {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	if (!priv->display_name)
		priv->display_name = g_file_get_basename (f);
	if (!priv->edit_name)
		priv->edit_name = g_file_get_basename (f);
}

void
ev_window_open_uri (EvWindow       *ev_window,
		    const char     *uri,
		    EvLinkDest     *dest,
		    EvWindowRunMode mode,
		    const gchar    *search_string)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	GFile *source_file;

	priv->in_reload = FALSE;

	g_clear_pointer (&priv->search_string, g_free);
	priv->search_string = search_string ?
		g_strdup (search_string) : NULL;

	if (priv->uri &&
	    g_ascii_strcasecmp (priv->uri, uri) == 0) {
		ev_window_reload_document (ev_window, dest);
		return;
	}

	if (priv->monitor) {
		g_object_unref (priv->monitor);
		priv->monitor = NULL;
	}
	
	ev_window_close_dialogs (ev_window);
	ev_window_clear_load_job (ev_window);
	ev_window_clear_local_uri (ev_window);

	priv->window_mode = mode;

	if (priv->uri)
		g_free (priv->uri);
	priv->uri = g_strdup (uri);

	if (priv->metadata)
		g_object_unref (priv->metadata);
	if (priv->bookmarks)
		g_object_unref (priv->bookmarks);

	source_file = g_file_new_for_uri (uri);
	if (ev_is_metadata_supported_for_file (source_file)) {
		priv->metadata = ev_metadata_new (source_file);
		ev_window_init_metadata_with_default_values (ev_window);
	} else {
		priv->metadata = NULL;
	}

	if (priv->metadata) {
		priv->bookmarks = ev_bookmarks_new (priv->metadata);
		ev_sidebar_bookmarks_set_bookmarks (EV_SIDEBAR_BOOKMARKS (priv->sidebar_bookmarks),
						    priv->bookmarks);
		g_signal_connect_swapped (priv->bookmarks, "changed",
					  G_CALLBACK (ev_window_setup_bookmarks),
					  ev_window);
		ev_window_setup_bookmarks (ev_window);
	} else {
		priv->bookmarks = NULL;
	}

	if (priv->dest)
		g_object_unref (priv->dest);
	priv->dest = dest ? g_object_ref (dest) : NULL;

	set_filenames (ev_window, source_file);
	setup_size_from_metadata (ev_window);
	setup_model_from_metadata (ev_window);

	priv->load_job = ev_job_load_new (uri);
	g_signal_connect (priv->load_job,
			  "finished",
			  G_CALLBACK (ev_window_load_job_cb),
			  ev_window);

	if (!g_file_is_native (source_file) && !priv->local_uri) {
		ev_window_load_file_remote (ev_window, source_file);
	} else {
		ev_window_show_loading_message (ev_window);
		g_object_unref (source_file);
		ev_job_scheduler_push_job (priv->load_job, EV_JOB_PRIORITY_NONE);
	}
}

void
ev_window_open_document (EvWindow       *ev_window,
			 EvDocument     *document,
			 EvLinkDest     *dest,
			 EvWindowRunMode mode,
			 const gchar    *search_string)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	if (document == priv->document)
		return;

	ev_window_close_dialogs (ev_window);
	ev_window_clear_load_job (ev_window);
	ev_window_clear_local_uri (ev_window);

	if (priv->monitor) {
		g_object_unref (priv->monitor);
		priv->monitor = NULL;
	}

	if (priv->uri)
		g_free (priv->uri);
	priv->uri = g_strdup (ev_document_get_uri (document));

	setup_size_from_metadata (ev_window);
	setup_model_from_metadata (ev_window);

	ev_document_model_set_document (priv->model, document);

	setup_document_from_metadata (ev_window);
	setup_view_from_metadata (ev_window);

	if (dest) {
		EvLink *link;
		EvLinkAction *link_action;

		link_action = ev_link_action_new_dest (dest);
		link = ev_link_new (NULL, link_action);
		ev_view_handle_link (EV_VIEW (priv->view), link);
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
		GtkSearchEntry *entry;

		ev_window_show_find_bar (ev_window, FALSE);
		entry = ev_search_box_get_entry (EV_SEARCH_BOX (priv->search_box));
		gtk_entry_set_text (GTK_ENTRY (entry), search_string);
	}

	/* Create a monitor for the document */
	priv->monitor = ev_file_monitor_new (priv->uri);
	g_signal_connect_swapped (priv->monitor, "changed",
				  G_CALLBACK (ev_window_file_changed),
				  ev_window);
}

void
ev_window_open_recent_view (EvWindow *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	if (priv->recent_view)
		return;

	gtk_widget_hide (priv->hpaned);

	priv->recent_view = EV_RECENT_VIEW (ev_recent_view_new ());
	g_signal_connect_object (priv->recent_view,
				 "item-activated",
				 G_CALLBACK (recent_view_item_activated_cb),
				 ev_window, 0);
	gtk_box_pack_start (GTK_BOX (priv->main_box),
			    GTK_WIDGET (priv->recent_view),
			    TRUE, TRUE, 0);

	gtk_widget_show (GTK_WIDGET (priv->recent_view));
	ev_toolbar_set_mode (EV_TOOLBAR (priv->toolbar),
			     EV_TOOLBAR_MODE_RECENT_VIEW);
	ev_window_title_set_type (priv->title, EV_WINDOW_TITLE_RECENT);

	ev_window_update_actions_sensitivity (ev_window);
}

static void
ev_window_destroy_recent_view (EvWindow *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	if (!priv->recent_view)
		return;

	gtk_widget_destroy (GTK_WIDGET (priv->recent_view));
	priv->recent_view = NULL;
	gtk_widget_show (priv->hpaned);
}

static void
ev_window_reload_local (EvWindow *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	const gchar *uri;

	uri = priv->local_uri ? priv->local_uri : priv->uri;
	priv->reload_job = ev_job_load_new (uri);
	g_signal_connect (priv->reload_job, "finished",
			  G_CALLBACK (ev_window_reload_job_cb),
			  ev_window);
	ev_job_scheduler_push_job (priv->reload_job, EV_JOB_PRIORITY_NONE);
}

static gboolean
show_reloading_progress (EvWindow *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	GtkWidget *area;
	gchar     *text;

	if (priv->message_area)
		return FALSE;
	
	text = g_strdup_printf (_("Reloading document from %s"),
				priv->uri);
	area = ev_progress_message_area_new ("view-refresh-symbolic",
					     text,
					     _("C_ancel"),
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
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	gchar *status;
	gdouble fraction;
	
	if (!priv->message_area)
		return;

	if (total_bytes <= 0)
		return;

	fraction = n_bytes / (gdouble)total_bytes;
	status = g_strdup_printf (_("Downloading document (%d%%)"),
				  (gint)(fraction * 100));
	
	ev_progress_message_area_set_status (EV_PROGRESS_MESSAGE_AREA (priv->message_area),
					     status);
	ev_progress_message_area_set_fraction (EV_PROGRESS_MESSAGE_AREA (priv->message_area),
					       fraction);

	g_free (status);
}

static void
query_remote_uri_mtime_cb (GFile        *remote,
			   GAsyncResult *async_result,
			   EvWindow     *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
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
	if (priv->uri_mtime != mtime.tv_sec) {
		GFile *target_file;

		/* Remote file has changed */
		priv->uri_mtime = mtime.tv_sec;

		ev_window_reset_progress_cancellable (ev_window);
		
		target_file = g_file_new_for_uri (priv->local_uri);
		g_file_copy_async (remote, target_file,
				   G_FILE_COPY_OVERWRITE,
				   G_PRIORITY_DEFAULT,
				   priv->progress_cancellable,
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
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	GFile *remote;

	remote = g_file_new_for_uri (priv->uri);
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
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	ev_window_clear_reload_job (ev_window);
	priv->in_reload = TRUE;

	if (priv->dest)
		g_object_unref (priv->dest);
	priv->dest = dest ? g_object_ref (dest) : NULL;

	if (priv->local_uri) {
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
ev_window_cmd_new_window (GSimpleAction *action,
			  GVariant      *parameter,
			  gpointer       user_data)
{

	EvWindow  *window = user_data;
	GdkScreen *screen;
	guint32    timestamp;

	screen = gtk_window_get_screen (GTK_WINDOW (window));
        timestamp = gtk_get_current_event_time ();

	ev_application_new_window (EV_APP, screen, timestamp);
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
					       _("_Cancel"),
					       GTK_RESPONSE_CANCEL,
					       _("_Open"), GTK_RESPONSE_OK,
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
	EvWindowPrivate *priv = GET_PRIVATE (window);
	EvWindow *new_window = EV_WINDOW (ev_window_new ());
	EvWindowPrivate *new_priv = GET_PRIVATE (new_window);

	if (priv->metadata)
		new_priv->metadata = g_object_ref (priv->metadata);
	new_priv->display_name = g_strdup (priv->display_name);
	new_priv->edit_name = g_strdup (priv->edit_name);
	ev_window_open_document (new_window,
				 priv->document,
				 dest, 0, NULL);
	new_priv->chrome = priv->chrome;

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
ev_window_add_recent (EvWindow *window, const char *uri)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);

	gtk_recent_manager_add_item (priv->recent_manager, uri);
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
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	priv->progress_idle = 0;

	if (priv->message_area)
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
	area = ev_progress_message_area_new ("document-save-symbolic",
					     text,
					     _("C_ancel"),
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
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	
	if (!priv->message_area)
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

	ev_progress_message_area_set_status (EV_PROGRESS_MESSAGE_AREA (priv->message_area),
					     status);
	ev_progress_message_area_set_fraction (EV_PROGRESS_MESSAGE_AREA (priv->message_area),
					       fraction);

	g_free (status);
}

static void
ev_window_save_remote (EvWindow  *ev_window,
		       EvSaveType save_type,
		       GFile     *src,
		       GFile     *dst)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	ev_window_reset_progress_cancellable (ev_window);
	g_object_set_data (G_OBJECT (dst), "ev-window", ev_window);
	g_object_set_data (G_OBJECT (dst), "save-type", GINT_TO_POINTER (save_type));
	g_file_copy_async (src, dst,
			   G_FILE_COPY_OVERWRITE,
			   G_PRIORITY_DEFAULT,
			   priv->progress_cancellable,
			   (GFileProgressCallback)window_save_file_copy_progress_cb,
			   dst,
			   (GAsyncReadyCallback)window_save_file_copy_ready_cb,
			   dst);
	priv->progress_idle =
		g_timeout_add_seconds_full (G_PRIORITY_DEFAULT,
					    1,
					    (GSourceFunc)show_saving_progress,
					    dst,
					    NULL);
}

static void
ev_window_clear_save_job (EvWindow *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	if (priv->save_job != NULL) {
		if (!ev_job_is_finished (priv->save_job))
			ev_job_cancel (priv->save_job);
		
		g_signal_handlers_disconnect_by_func (priv->save_job,
						      ev_window_save_job_cb,
						      ev_window);
		g_object_unref (priv->save_job);
		priv->save_job = NULL;
	}
}

static gboolean
destroy_window (GtkWidget *window)
{
	gtk_widget_destroy (window);

	return FALSE;
}

static void
ev_window_save_job_cb (EvJob     *job,
		       EvWindow  *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);
	if (ev_job_is_failed (job)) {
		priv->close_after_save = FALSE;
		ev_window_error_message (window, job->error,
					 _("The file could not be saved as “%s”."),
					 EV_JOB_SAVE (job)->uri);
	} else {
		ev_window_add_recent (window, EV_JOB_SAVE (job)->uri);
	}

	ev_window_clear_save_job (window);

	if (priv->close_after_save)
		g_idle_add ((GSourceFunc)destroy_window, window);
}

static void
file_save_dialog_response_cb (GtkWidget *fc,
			      gint       response_id,
			      EvWindow  *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	gchar *uri;

	if (response_id != GTK_RESPONSE_OK) {
		priv->close_after_save = FALSE;
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
	priv->save_job = ev_job_save_new (priv->document,
						     uri, priv->uri);
	g_signal_connect (priv->save_job, "finished",
			  G_CALLBACK (ev_window_save_job_cb),
			  ev_window);
	/* The priority doesn't matter for this job */
	ev_job_scheduler_push_job (priv->save_job, EV_JOB_PRIORITY_NONE);

	g_free (uri);
	gtk_widget_destroy (fc);
}

static void
ev_window_save_as (EvWindow *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	GtkWidget *fc;
	gchar *base_name, *dir_name, *var_tmp_dir, *tmp_dir;
	GFile *file, *parent;
	const gchar *default_dir, *dest_dir, *documents_dir;

	fc = gtk_file_chooser_dialog_new (
		_("Save As…"),
		GTK_WINDOW (ev_window), GTK_FILE_CHOOSER_ACTION_SAVE,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_Save"), GTK_RESPONSE_OK,
		NULL);

	ev_document_factory_add_filters (fc, priv->document);
	gtk_dialog_set_default_response (GTK_DIALOG (fc), GTK_RESPONSE_OK);

	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (fc), FALSE);
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (fc), TRUE);

	file = g_file_new_for_uri (priv->uri);
	base_name = priv->edit_name;
	parent = g_file_get_parent (file);
	dir_name = g_file_get_path (parent);
	g_object_unref (parent);

	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (fc), base_name);

	documents_dir = g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS);
	default_dir = g_file_test (documents_dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR) ?
	              documents_dir : g_get_home_dir ();

	tmp_dir = g_build_filename ("tmp", NULL);
	var_tmp_dir = g_build_filename ("var", "tmp", NULL);
	dest_dir = dir_name && !g_str_has_prefix (dir_name, g_get_tmp_dir ()) &&
			    !g_str_has_prefix (dir_name, tmp_dir) &&
	                    !g_str_has_prefix (dir_name, var_tmp_dir) ?
	                    dir_name : default_dir;

	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (fc),
					     dest_dir);

	g_object_unref (file);
	g_free (tmp_dir);
	g_free (var_tmp_dir);
	g_free (dir_name);

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

	ev_window_save_as (window);
}

static void
ev_window_cmd_send_to (GSimpleAction *action,
		       GVariant      *parameter,
		       gpointer       user_data)
{
	EvWindow   *ev_window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	GAppInfo   *app_info;
	gchar      *command;
	const char *uri;
	char       *unescaped_uri;
	GError     *error = NULL;

	uri = priv->local_uri ? priv->local_uri : priv->uri;
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
	EvWindow *window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (window);

	guint32 timestamp;
	GAppInfo *app = NULL;
	GdkAppLaunchContext *context;
	GdkDisplay *display;
	GdkScreen *screen;
	GFile *file;
	GList list;
	GError *error = NULL;

	app =  g_app_info_get_default_for_type ("inode/directory", FALSE);

	if (app == NULL) {
		return;
	}

	file = g_file_new_for_uri (priv->uri);
	list.next = list.prev = NULL;
	list.data = file;

	display = gtk_widget_get_display (GTK_WIDGET (window));
	screen = gtk_widget_get_screen (GTK_WIDGET (window));
	timestamp = gtk_get_current_event_time ();

	context = gdk_display_get_app_launch_context (display);
	gdk_app_launch_context_set_screen (context, screen);
	gdk_app_launch_context_set_timestamp (context, timestamp);

	g_app_info_launch (app, &list,
                           G_APP_LAUNCH_CONTEXT (context), &error);

	if (error != NULL) {
		gchar *uri;

		uri = g_file_get_uri (file);
		g_warning ("Could not show containing folder for \"%s\": %s",
			   uri, error->message);

		g_error_free (error);
		g_free (uri);
	}

	g_object_unref (context);
	g_object_unref (app);
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
	EvWindowPrivate *priv = GET_PRIVATE (window);
	GKeyFile *key_file;
	gint      i;

	key_file = get_print_settings_file ();
	gtk_print_settings_to_key_file (print_settings, key_file, EV_PRINT_SETTINGS_GROUP);

	/* Always Remove n_copies from global settings */
	g_key_file_remove_key (key_file, EV_PRINT_SETTINGS_GROUP, GTK_PRINT_SETTINGS_N_COPIES, NULL);

	/* Save print settings that are specific to the document */
	for (i = 0; i < G_N_ELEMENTS (document_print_settings); i++) {
		/* Remove it from global settings */
		g_key_file_remove_key (key_file, EV_PRINT_SETTINGS_GROUP,
				       document_print_settings[i], NULL);

		if (priv->metadata) {
			const gchar *value;

			value = gtk_print_settings_get (print_settings,
							document_print_settings[i]);
			ev_metadata_set_string (priv->metadata,
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
	EvWindowPrivate *priv = GET_PRIVATE (window);
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

	if (!priv->metadata)
		return;

	/* Save page setup options that are specific to the document */
	ev_metadata_set_int (priv->metadata, "page-setup-orientation",
			     gtk_page_setup_get_orientation (page_setup));
	ev_metadata_set_double (priv->metadata, "page-setup-margin-top",
				gtk_page_setup_get_top_margin (page_setup, GTK_UNIT_MM));
	ev_metadata_set_double (priv->metadata, "page-setup-margin-bottom",
				gtk_page_setup_get_bottom_margin (page_setup, GTK_UNIT_MM));
	ev_metadata_set_double (priv->metadata, "page-setup-margin-left",
				gtk_page_setup_get_left_margin (page_setup, GTK_UNIT_MM));
	ev_metadata_set_double (priv->metadata, "page-setup-margin-right",
				gtk_page_setup_get_right_margin (page_setup, GTK_UNIT_MM));
}

static void
ev_window_load_print_settings_from_metadata (EvWindow         *window,
					     GtkPrintSettings *print_settings)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);
	gint i;

	if (!priv->metadata)
		return;

	/* Load print setting that are specific to the document */
	for (i = 0; i < G_N_ELEMENTS (document_print_settings); i++) {
		gchar *value = NULL;

		ev_metadata_get_string (priv->metadata,
					document_print_settings[i], &value);
		gtk_print_settings_set (print_settings,
					document_print_settings[i], value);
	}
}

static void
ev_window_load_print_page_setup_from_metadata (EvWindow     *window,
					       GtkPageSetup *page_setup)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);
	gint          int_value;
	gdouble       double_value;
	GtkPaperSize *paper_size = gtk_page_setup_get_paper_size (page_setup);

	/* Load page setup options that are specific to the document */
	if (priv->metadata &&
	    ev_metadata_get_int (priv->metadata, "page-setup-orientation", &int_value)) {
		gtk_page_setup_set_orientation (page_setup, int_value);
	} else {
		gtk_page_setup_set_orientation (page_setup, GTK_PAGE_ORIENTATION_PORTRAIT);
	}

	if (priv->metadata &&
	    ev_metadata_get_double (priv->metadata, "page-setup-margin-top", &double_value)) {
		gtk_page_setup_set_top_margin (page_setup, double_value, GTK_UNIT_MM);
	} else {
		gtk_page_setup_set_top_margin (page_setup,
					       gtk_paper_size_get_default_top_margin (paper_size, GTK_UNIT_MM),
					       GTK_UNIT_MM);
	}

	if (priv->metadata &&
	    ev_metadata_get_double (priv->metadata, "page-setup-margin-bottom", &double_value)) {
		gtk_page_setup_set_bottom_margin (page_setup, double_value, GTK_UNIT_MM);
	} else {
		gtk_page_setup_set_bottom_margin (page_setup,
						  gtk_paper_size_get_default_bottom_margin (paper_size, GTK_UNIT_MM),
						  GTK_UNIT_MM);
	}

	if (priv->metadata &&
	    ev_metadata_get_double (priv->metadata, "page-setup-margin-left", &double_value)) {
		gtk_page_setup_set_left_margin (page_setup, double_value, GTK_UNIT_MM);
	} else {
		gtk_page_setup_set_left_margin (page_setup,
						gtk_paper_size_get_default_left_margin (paper_size, GTK_UNIT_MM),
						GTK_UNIT_MM);
	}

	if (priv->metadata &&
	    ev_metadata_get_double (priv->metadata, "page-setup-margin-right", &double_value)) {
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
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	EvPrintOperation *op;

	if (!priv->print_queue)
		return;

	while ((op = g_queue_peek_tail (priv->print_queue))) {
		ev_print_operation_cancel (op);
	}
}

static void
ev_window_print_update_pending_jobs_message (EvWindow *ev_window,
					     gint      n_jobs)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	gchar *text = NULL;

	if (!EV_IS_PROGRESS_MESSAGE_AREA (priv->message_area) ||
	    !priv->print_queue)
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

	ev_message_area_set_secondary_text (EV_MESSAGE_AREA (priv->message_area),
					    text);
	g_free (text);
}

static void
ev_window_print_operation_done (EvPrintOperation       *op,
				GtkPrintOperationResult result,
				EvWindow               *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
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

	g_queue_remove (priv->print_queue, op);
	g_object_unref (op);
	n_jobs = g_queue_get_length (priv->print_queue);
	ev_window_print_update_pending_jobs_message (ev_window, n_jobs);

	if (n_jobs == 0 && priv->close_after_print)
		g_idle_add ((GSourceFunc)destroy_window,
			    ev_window);
}

static void
ev_window_print_progress_response_cb (EvProgressMessageArea *area,
				      gint                   response,
				      EvWindow              *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	if (response == GTK_RESPONSE_CANCEL) {
		EvPrintOperation *op;

		op = g_queue_peek_tail (priv->print_queue);
		ev_print_operation_cancel (op);
	} else {
		gtk_widget_hide (GTK_WIDGET (area));
	}
}

static void
ev_window_print_operation_status_changed (EvPrintOperation *op,
					  EvWindow         *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	const gchar *status;
	gdouble      fraction;

	status = ev_print_operation_get_status (op);
	fraction = ev_print_operation_get_progress (op);
	
	if (!priv->message_area) {
		GtkWidget   *area;
		const gchar *job_name;
		gchar       *text;

		job_name = ev_print_operation_get_job_name (op);
		text = g_strdup_printf (_("Printing job “%s”"), job_name);

		area = ev_progress_message_area_new ("document-print-symbolic",
						     text,
						     _("C_ancel"),
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

	ev_progress_message_area_set_status (EV_PROGRESS_MESSAGE_AREA (priv->message_area),
					     status);
	ev_progress_message_area_set_fraction (EV_PROGRESS_MESSAGE_AREA (priv->message_area),
					       fraction);
}

static void
ev_window_print_operation_begin_print (EvPrintOperation *op,
				       EvWindow         *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	if (!priv->print_queue)
		priv->print_queue = g_queue_new ();

	g_queue_push_head (priv->print_queue, op);
	ev_window_print_update_pending_jobs_message (ev_window,
						     g_queue_get_length (priv->print_queue));
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
	gchar            *dot;
	EvWindowPrivate *priv;

	g_return_if_fail (EV_IS_WINDOW (ev_window));
	priv = GET_PRIVATE (ev_window);
	g_return_if_fail (priv->document != NULL);

	if (!priv->print_queue)
		priv->print_queue = g_queue_new ();

	op = ev_print_operation_new (priv->document);
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

	current_page = ev_document_model_get_page (priv->model);
	document_last_page = ev_document_get_n_pages (priv->document);

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

	output_basename = g_strdup (priv->edit_name);
	dot = g_strrstr (output_basename, ".");
	if (dot)
		dot[0] = '\0';

	/* Set output basename for printing to file */
	gtk_print_settings_set (print_settings,
			        GTK_PRINT_SETTINGS_OUTPUT_BASENAME,
			        output_basename);
	g_free (output_basename);

	ev_print_operation_set_job_name (op, gtk_window_get_title (GTK_WINDOW (ev_window)));
	ev_print_operation_set_current_page (op, current_page);
	ev_print_operation_set_print_settings (op, print_settings);
	ev_print_operation_set_default_page_setup (op, print_page_setup);
	embed_page_setup = priv->lockdown_settings ?
		!g_settings_get_boolean (priv->lockdown_settings,
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
	EvWindowPrivate *priv = GET_PRIVATE (window);

	ev_window_print_range (window, 1,
			       ev_document_get_n_pages (priv->document));
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
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	if (priv->properties == NULL) {
		priv->properties = ev_properties_dialog_new ();
		ev_properties_dialog_set_document (EV_PROPERTIES_DIALOG (priv->properties),
						   priv->uri,
					           priv->document);
		g_object_add_weak_pointer (G_OBJECT (priv->properties),
					   (gpointer) &(priv->properties));
		gtk_window_set_transient_for (GTK_WINDOW (priv->properties),
					      GTK_WINDOW (ev_window));
	}

	ev_document_fc_mutex_lock ();
	gtk_widget_show (priv->properties);
	ev_document_fc_mutex_unlock ();
}

static void
document_modified_reload_dialog_response (GtkDialog *dialog,
					  gint	     response,
					  EvWindow  *ev_window)
{
	gtk_widget_destroy (GTK_WIDGET (dialog));

	if (response == GTK_RESPONSE_YES)
	        ev_window_reload_document (ev_window, NULL);
}

static void
document_modified_confirmation_dialog_response (GtkDialog *dialog,
						gint       response,
						EvWindow  *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	gtk_widget_destroy (GTK_WIDGET (dialog));

	switch (response) {
	case GTK_RESPONSE_YES:
		priv->close_after_save = TRUE;
		ev_window_save_as (ev_window);
		break;
	case GTK_RESPONSE_NO:
		gtk_widget_destroy (GTK_WIDGET (ev_window));
		break;
	case GTK_RESPONSE_CANCEL:
	default:
		priv->close_after_save = FALSE;
	}
}

static gboolean
ev_window_check_document_modified (EvWindow      *ev_window,
				   EvWindowAction command)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	EvDocument  *document = priv->document;
	GtkWidget   *dialog;
	gchar       *text, *markup;
	const gchar *secondary_text, *secondary_text_command;

	if (!document)
		return FALSE;

	if (EV_IS_DOCUMENT_FORMS (document) &&
	    ev_document_forms_document_is_modified (EV_DOCUMENT_FORMS (document))) {
		secondary_text = _("Document contains form fields that have been filled out. ");
	} else if (EV_IS_DOCUMENT_ANNOTATIONS (document) &&
		   ev_document_annotations_document_is_modified (EV_DOCUMENT_ANNOTATIONS (document))) {
		secondary_text = _("Document contains new or modified annotations. ");
	} else {
		return FALSE;
	}

	dialog = gtk_message_dialog_new (GTK_WINDOW (ev_window),
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_NONE,
					 NULL);

	if (command == EV_WINDOW_ACTION_RELOAD) {
		text = g_markup_printf_escaped (_("Reload document “%s”?"),
						gtk_window_get_title (GTK_WINDOW (ev_window)));
		secondary_text_command = _("If you reload the document, changes will be permanently lost.");
		gtk_dialog_add_buttons (GTK_DIALOG (dialog),
					_("_No"),
					GTK_RESPONSE_NO,
					_("_Reload"),
					GTK_RESPONSE_YES,
					NULL);
		g_signal_connect (dialog, "response",
				  G_CALLBACK (document_modified_reload_dialog_response),
				  ev_window);
	} else {
		text = g_markup_printf_escaped (_("Save a copy of document “%s” before closing?"),
                                                gtk_window_get_title (GTK_WINDOW (ev_window)));
		secondary_text_command = _("If you don’t save a copy, changes will be permanently lost.");
		gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				_("Close _without Saving"),
				GTK_RESPONSE_NO,
				_("C_ancel"),
				GTK_RESPONSE_CANCEL,
				_("Save a _Copy"),
				GTK_RESPONSE_YES,
				NULL);
		g_signal_connect (dialog, "response",
			  G_CALLBACK (document_modified_confirmation_dialog_response),
			  ev_window);

	}
	markup = g_strdup_printf ("<b>%s</b>", text);
	g_free (text);

	gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), markup);
	g_free (markup);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  "%s %s", secondary_text, secondary_text_command);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_YES);

	gtk_widget_show (dialog);

	return TRUE;
}

static void
print_jobs_confirmation_dialog_response (GtkDialog *dialog,
					 gint       response,
					 EvWindow  *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	gtk_widget_destroy (GTK_WIDGET (dialog));

	switch (response) {
	case GTK_RESPONSE_YES:
		if (!priv->print_queue ||
		    g_queue_is_empty (priv->print_queue))
			gtk_widget_destroy (GTK_WIDGET (ev_window));
		else
			priv->close_after_print = TRUE;
		break;
	case GTK_RESPONSE_NO:
		priv->close_after_print = TRUE;
		if (priv->print_queue &&
		    !g_queue_is_empty (priv->print_queue)) {
			gtk_widget_set_sensitive (GTK_WIDGET (ev_window), FALSE);
			ev_window_print_cancel (ev_window);
		} else {
			gtk_widget_destroy (GTK_WIDGET (ev_window));
		}
		break;
	case GTK_RESPONSE_CANCEL:
	default:
		priv->close_after_print = FALSE;
	}
}

static gboolean
ev_window_check_print_queue (EvWindow *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	GtkWidget *dialog;
	gchar     *text, *markup;
	gint       n_print_jobs;

	n_print_jobs = priv->print_queue ?
		g_queue_get_length (priv->print_queue) : 0;

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

		op = g_queue_peek_tail (priv->print_queue);
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
				_("_Cancel"),
				GTK_RESPONSE_CANCEL,
				_("Close _after Printing"),
				GTK_RESPONSE_YES,
				NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_YES);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (print_jobs_confirmation_dialog_response),
			  ev_window);
	gtk_widget_show (dialog);

	return TRUE;
}

static gboolean
ev_window_close (EvWindow *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	if (EV_WINDOW_IS_PRESENTATION (priv)) {
		gint current_page;

		/* Save current page */
		current_page = ev_view_presentation_get_current_page (
			EV_VIEW_PRESENTATION (priv->presentation_view));
		ev_document_model_set_page (priv->model, current_page);
	}

	if (ev_window_check_document_modified (ev_window, EV_WINDOW_ACTION_CLOSE))
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

/**
 * ev_window_show_help:
 * @window: the #EvWindow
 * @topic: (allow-none): the help topic, or %NULL to show the index
 *
 * Launches the help viewer on @screen to show the evince help.
 * If @topic is %NULL, shows the help index; otherwise the topic.
 */
static void
ev_window_show_help (EvWindow   *window,
                     const char *topic)
{
        char *escaped_topic, *uri;

        if (topic != NULL) {
                escaped_topic = g_uri_escape_string (topic, NULL, TRUE);
                uri = g_strdup_printf ("help:evince/%s", escaped_topic);
                g_free (escaped_topic);
        } else {
                uri = g_strdup ("help:evince");
        }

        gtk_show_uri_on_window (GTK_WINDOW (window), uri,
				gtk_get_current_event_time (), NULL);
        g_free (uri);
}

static void
ev_window_cmd_help (GSimpleAction *action,
		    GVariant      *parameter,
		    gpointer       user_data)
{
	EvWindow *ev_window = user_data;

        ev_window_show_help (ev_window, NULL);
}

static void
ev_window_cmd_about (GSimpleAction *action,
		     GVariant      *parameter,
		     gpointer       user_data)
{
	EvWindow *ev_window = user_data;

        const char *authors[] = {
                "Martin Kretzschmar <m_kretzschmar@gmx.net>",
                "Jonathan Blandford <jrb@gnome.org>",
                "Marco Pesenti Gritti <marco@gnome.org>",
                "Nickolay V. Shmyrev <nshmyrev@yandex.ru>",
                "Bryan Clark <clarkbw@gnome.org>",
                "Carlos Garcia Campos <carlosgc@gnome.org>",
                "Wouter Bolsterlee <wbolster@gnome.org>",
                "Christian Persch <chpe" "\100" "src.gnome.org>",
                "Germán Poo-Caamaño <gpoo" "\100" "gnome.org>",
                NULL
        };
        const char *documenters[] = {
                "Nickolay V. Shmyrev <nshmyrev@yandex.ru>",
                "Phil Bull <philbull@gmail.com>",
                "Tiffany Antpolski <tiffany.antopolski@gmail.com>",
                NULL
        };
#ifdef ENABLE_NLS
        const char **p;

        for (p = authors; *p; ++p)
                *p = _(*p);

        for (p = documenters; *p; ++p)
                *p = _(*p);
#endif

        gtk_show_about_dialog (GTK_WINDOW (ev_window),
                               "name", _("Evince"),
                               "version", VERSION,
                               "copyright", _("© 1996–2017 The Evince authors"),
                               "license-type", GTK_LICENSE_GPL_2_0,
                               "website", "https://wiki.gnome.org/Apps/Evince",
                               "comments", _("Document Viewer"),
                               "authors", authors,
                               "documenters", documenters,
                               "translator-credits", _("translator-credits"),
                               "logo-icon-name", "org.gnome.Evince",
                               NULL);
}

static void
ev_window_cmd_focus_page_selector (GSimpleAction *action,
				   GVariant      *parameter,
				   gpointer       user_data)
{
	EvWindow *window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (window);
	GtkWidget *page_selector;
	EvToolbar *toolbar;

	update_chrome_flag (window, EV_CHROME_RAISE_TOOLBAR, TRUE);
	update_chrome_visibility (window);

	toolbar = priv->fs_toolbar ? EV_TOOLBAR (priv->fs_toolbar) : EV_TOOLBAR (priv->toolbar);
	page_selector = ev_toolbar_get_page_selector (toolbar);
	ev_page_action_widget_grab_focus (EV_PAGE_ACTION_WIDGET (page_selector));
}

static void
ev_window_cmd_scroll_forward (GSimpleAction *action,
			      GVariant      *parameter,
			      gpointer       user_data)
{
	EvWindow *window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (window);

	g_signal_emit_by_name (priv->view, "scroll", GTK_SCROLL_PAGE_FORWARD, GTK_ORIENTATION_VERTICAL);
}

static void
ev_window_cmd_scroll_backwards (GSimpleAction *action,
				GVariant      *parameter,
				gpointer       user_data)
{
	EvWindow *window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (window);

	g_signal_emit_by_name (priv->view, "scroll", GTK_SCROLL_PAGE_BACKWARD, GTK_ORIENTATION_VERTICAL);
}

static void
ev_window_cmd_continuous (GSimpleAction *action,
			  GVariant      *state,
			  gpointer       user_data)
{
	EvWindow *window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (window);

	ev_window_stop_presentation (window, TRUE);
	ev_document_model_set_continuous (priv->model, g_variant_get_boolean (state));
	g_simple_action_set_state (action, state);
}

static void
ev_window_cmd_dual (GSimpleAction *action,
		    GVariant      *state,
		    gpointer       user_data)
{
	EvWindow *window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (window);

	ev_window_stop_presentation (window, TRUE);
	ev_document_model_set_dual_page (priv->model, g_variant_get_boolean (state));
	g_simple_action_set_state (action, state);
}

static void
ev_window_cmd_dual_odd_pages_left (GSimpleAction *action,
				   GVariant      *state,
				   gpointer       user_data)
{
	EvWindow *window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (window);

	ev_document_model_set_dual_page_odd_pages_left (priv->model,
							g_variant_get_boolean (state));
	g_simple_action_set_state (action, state);
}

static void
ev_window_cmd_rtl (GSimpleAction *action,
                   GVariant      *state,
                   gpointer       user_data)
{
	EvWindow *window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (window);

	ev_document_model_set_rtl (priv->model,
	                           g_variant_get_boolean (state));
	g_simple_action_set_state (action, state);
}

static void
ev_window_change_sizing_mode_action_state (GSimpleAction *action,
					   GVariant      *state,
					   gpointer       user_data)
{
	EvWindow *window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (window);
	const gchar *mode;

	mode = g_variant_get_string (state, NULL);

	if (g_str_equal (mode, "fit-page"))
		ev_document_model_set_sizing_mode (priv->model, EV_SIZING_FIT_PAGE);
	else if (g_str_equal (mode, "fit-width"))
		ev_document_model_set_sizing_mode (priv->model, EV_SIZING_FIT_WIDTH);
	else if (g_str_equal (mode, "automatic"))
		ev_document_model_set_sizing_mode (priv->model, EV_SIZING_AUTOMATIC);
	else if (g_str_equal (mode, "free"))
		ev_document_model_set_sizing_mode (priv->model, EV_SIZING_FREE);
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
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	gdouble zoom = g_variant_get_double (parameter);

	ev_document_model_set_sizing_mode (priv->model, EV_SIZING_FREE);
	ev_document_model_set_scale (priv->model,
				     zoom * ev_document_misc_get_widget_dpi (GTK_WIDGET (ev_window)) / 72.0);
}

static void
ev_window_cmd_set_default_zoom (GSimpleAction *action,
				GVariant      *parameter,
				gpointer       user_data)
{
	EvWindow *ev_window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	ev_document_model_set_sizing_mode (priv->model, EV_SIZING_FREE);
	ev_document_model_set_scale (priv->model,
				     1. * ev_document_misc_get_widget_dpi (GTK_WIDGET (ev_window)) / 72.0);
}

static void
ev_window_cmd_edit_select_all (GSimpleAction *action,
			       GVariant      *parameter,
			       gpointer       user_data)
{
	EvWindow *ev_window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	ev_view_select_all (EV_VIEW (priv->view));
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
	EvView *view;
	gchar *selected_text = NULL;
	EvWindow *ev_window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	view = EV_VIEW (priv->view);
        selected_text = ev_view_get_selected_text (view);

        if (selected_text != NULL) {
		GtkSearchEntry *entry = ev_search_box_get_entry (EV_SEARCH_BOX (priv->search_box));
		gtk_entry_set_text (GTK_ENTRY (entry), selected_text);
		g_free (selected_text);
	}

	ev_window_show_find_bar (ev_window, TRUE);
}

static void
ev_window_find_restart (EvWindow *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	gint page;

	page = ev_document_model_get_page (priv->model);
	ev_view_find_restart (EV_VIEW (priv->view), page);
	ev_find_sidebar_restart (EV_FIND_SIDEBAR (priv->find_sidebar), page);
}

static void
ev_window_find_previous (EvWindow *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	ev_view_find_previous (EV_VIEW (priv->view));
	ev_find_sidebar_previous (EV_FIND_SIDEBAR (priv->find_sidebar));
}

static void
ev_window_find_next (EvWindow *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	ev_view_find_next (EV_VIEW (priv->view));
	ev_find_sidebar_next (EV_FIND_SIDEBAR (priv->find_sidebar));
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
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	gboolean search_mode_enabled;

	if (EV_WINDOW_IS_PRESENTATION (priv))
		return;

	search_mode_enabled = gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (priv->search_bar));
	ev_window_show_find_bar (ev_window, FALSE);

	/* Use idle to make sure view allocation happens before find */
	if (!search_mode_enabled)
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
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	gboolean search_mode_enabled;

	if (EV_WINDOW_IS_PRESENTATION (priv))
		return;

	search_mode_enabled = gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (priv->search_bar));
	ev_window_show_find_bar (ev_window, FALSE);

	/* Use idle to make sure view allocation happens before find */
	if (!search_mode_enabled)
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
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	ev_view_copy (EV_VIEW (priv->view));
}

static void
ev_window_sidebar_position_change_cb (GObject    *object,
				      GParamSpec *pspec,
				      EvWindow   *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	if (priv->metadata && !ev_window_is_empty (ev_window))
		ev_metadata_set_int (priv->metadata, "sidebar_size",
				     gtk_paned_get_position (GTK_PANED (object)));
}

static void
ev_window_update_links_model (EvWindow *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);
	GtkTreeModel *model;
	GtkWidget *page_selector;

	g_object_get (priv->sidebar_links,
		      "model", &model,
		      NULL);

	if (!model)
		return;

	page_selector = ev_toolbar_get_page_selector (EV_TOOLBAR (priv->toolbar));
	ev_page_action_widget_update_links_model (EV_PAGE_ACTION_WIDGET (page_selector), model);
	if (priv->fs_toolbar) {
		page_selector = ev_toolbar_get_page_selector (EV_TOOLBAR (priv->fs_toolbar));
		ev_page_action_widget_update_links_model (EV_PAGE_ACTION_WIDGET (page_selector), model);
	}
	g_object_unref (model);
}

static void
ev_window_update_fullscreen_action (EvWindow *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);
	GAction *action;
	gboolean fullscreen;

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "fullscreen");
	fullscreen = ev_document_model_get_fullscreen (priv->model);
	g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (fullscreen));
}

static void
ev_window_fullscreen_hide_toolbar (EvWindow *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);

	if (!ev_toolbar_has_visible_popups (EV_TOOLBAR (priv->fs_toolbar)))
		gtk_revealer_set_reveal_child (GTK_REVEALER (priv->fs_revealer), FALSE);
}

static gboolean
fullscreen_toolbar_timeout_cb (EvWindow *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);

	ev_window_fullscreen_hide_toolbar (window);

	if (!gtk_revealer_get_reveal_child (GTK_REVEALER (priv->fs_revealer))) {
		priv->fs_timeout_id = 0;
		return FALSE;
	}

	return TRUE;
}

static void
ev_window_remove_fullscreen_timeout (EvWindow *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);

	if (priv->fs_timeout_id)
		g_source_remove (priv->fs_timeout_id);
	priv->fs_timeout_id = 0;
}

static void
ev_window_add_fullscreen_timeout (EvWindow *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);

	ev_window_remove_fullscreen_timeout (window);
	priv->fs_timeout_id =
		g_timeout_add_seconds (FULLSCREEN_POPUP_TIMEOUT,
				       (GSourceFunc)fullscreen_toolbar_timeout_cb, window);
}

static void
ev_window_fullscreen_show_toolbar (EvWindow *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);

	ev_window_remove_fullscreen_timeout (window);
	if (gtk_revealer_get_reveal_child (GTK_REVEALER (priv->fs_revealer)))
		return;

	gtk_revealer_set_reveal_child (GTK_REVEALER (priv->fs_revealer), TRUE);
	if (!priv->fs_pointer_on_toolbar)
		ev_window_add_fullscreen_timeout (window);
}

static gboolean
ev_window_fullscreen_toolbar_enter_notify (GtkWidget *widget,
					   GdkEvent  *event,
					   EvWindow  *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);

	priv->fs_pointer_on_toolbar = TRUE;
	ev_window_fullscreen_show_toolbar (window);

	return FALSE;
}

static gboolean
ev_window_fullscreen_toolbar_leave_notify (GtkWidget *widget,
					   GdkEvent  *event,
					   EvWindow  *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);

	priv->fs_pointer_on_toolbar = FALSE;
	ev_window_add_fullscreen_timeout (window);

	return FALSE;
}

static void
ev_window_run_fullscreen (EvWindow *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);
	gboolean fullscreen_window = TRUE;

	if (ev_document_model_get_fullscreen (priv->model))
		return;

	if (EV_WINDOW_IS_PRESENTATION (priv)) {
		ev_window_stop_presentation (window, FALSE);
		fullscreen_window = FALSE;
	}

	priv->fs_overlay = gtk_overlay_new ();
	priv->fs_eventbox = gtk_event_box_new ();
	priv->fs_revealer = gtk_revealer_new ();
	g_signal_connect (priv->fs_eventbox, "enter-notify-event",
			  G_CALLBACK (ev_window_fullscreen_toolbar_enter_notify),
			  window);
	g_signal_connect (priv->fs_eventbox, "leave-notify-event",
			  G_CALLBACK (ev_window_fullscreen_toolbar_leave_notify),
			  window);

	gtk_widget_set_size_request (priv->fs_eventbox, -1, 1);
	gtk_widget_set_valign (priv->fs_eventbox, GTK_ALIGN_START);
	gtk_revealer_set_transition_duration (GTK_REVEALER (priv->fs_revealer), FULLSCREEN_TRANSITION_DURATION);

	g_object_ref (priv->main_box);
	gtk_container_remove (GTK_CONTAINER (window), priv->main_box);
	gtk_container_add (GTK_CONTAINER (priv->fs_overlay),
			   priv->main_box);
	g_object_unref (priv->main_box);

	priv->fs_toolbar = ev_toolbar_new (window);
	ev_toolbar_set_mode (EV_TOOLBAR (priv->fs_toolbar),
		             EV_TOOLBAR_MODE_FULLSCREEN);

	ev_window_update_links_model (window);
	g_signal_connect (ev_toolbar_get_page_selector (EV_TOOLBAR (priv->fs_toolbar)),
			  "activate-link",
			  G_CALLBACK (activate_link_cb),
			  window);
	gtk_container_add (GTK_CONTAINER (priv->fs_revealer),
			   priv->fs_toolbar);
	gtk_widget_show (priv->fs_toolbar);

	gtk_container_add (GTK_CONTAINER (priv->fs_eventbox),
			   priv->fs_revealer);
	gtk_widget_show (priv->fs_revealer);
	gtk_overlay_add_overlay (GTK_OVERLAY (priv->fs_overlay),
				 priv->fs_eventbox);
	gtk_widget_show (priv->fs_eventbox);

	gtk_container_add (GTK_CONTAINER (window), priv->fs_overlay);
	gtk_widget_show (priv->fs_overlay);

	ev_document_model_set_fullscreen (priv->model, TRUE);
	ev_window_update_fullscreen_action (window);

	ev_window_fullscreen_show_toolbar (window);

	if (fullscreen_window)
		gtk_window_fullscreen (GTK_WINDOW (window));
	gtk_widget_grab_focus (priv->view);

	if (priv->metadata && !ev_window_is_empty (window))
		ev_metadata_set_boolean (priv->metadata, "fullscreen", TRUE);
}

static void
ev_window_stop_fullscreen (EvWindow *window,
			   gboolean  unfullscreen_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);

	if (!ev_document_model_get_fullscreen (priv->model))
		return;

	gtk_container_remove (GTK_CONTAINER (priv->fs_revealer),
			      priv->fs_toolbar);
	priv->fs_toolbar = NULL;
	gtk_container_remove (GTK_CONTAINER (priv->fs_eventbox),
			      priv->fs_revealer);
	gtk_container_remove (GTK_CONTAINER (priv->fs_overlay),
			      priv->fs_eventbox);

	g_object_ref (priv->main_box);
	gtk_container_remove (GTK_CONTAINER (priv->fs_overlay),
			      priv->main_box);
	gtk_container_remove (GTK_CONTAINER (window), priv->fs_overlay);
	priv->fs_overlay = NULL;
	gtk_container_add (GTK_CONTAINER (window), priv->main_box);
	g_object_unref (priv->main_box);

	ev_window_remove_fullscreen_timeout (window);

	ev_document_model_set_fullscreen (priv->model, FALSE);
	ev_window_update_fullscreen_action (window);

	if (unfullscreen_window)
		gtk_window_unfullscreen (GTK_WINDOW (window));

	if (priv->metadata && !ev_window_is_empty (window))
		ev_metadata_set_boolean (priv->metadata, "fullscreen", FALSE);
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
	EvWindowPrivate *priv = GET_PRIVATE (window);

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
	EvWindowPrivate *priv = GET_PRIVATE (window);

        if (priv->presentation_mode_inhibit_id == 0)
                return;

        gtk_application_uninhibit (GTK_APPLICATION (g_application_get_default ()),
                                   priv->presentation_mode_inhibit_id);
        priv->presentation_mode_inhibit_id = 0;
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
	EvWindowPrivate *priv = GET_PRIVATE (window);
	gboolean fullscreen_window = TRUE;
	guint    current_page;
	guint    rotation;
	gboolean inverted_colors;

	if (EV_WINDOW_IS_PRESENTATION (priv))
		return;

	ev_window_close_find_bar (window);

	if (ev_document_model_get_fullscreen (priv->model)) {
		ev_window_stop_fullscreen (window, FALSE);
		fullscreen_window = FALSE;
	}

	current_page = ev_document_model_get_page (priv->model);
	rotation = ev_document_model_get_rotation (priv->model);
	inverted_colors = ev_document_model_get_inverted_colors (priv->model);
	priv->presentation_view = ev_view_presentation_new (priv->document,
								    current_page,
								    rotation,
								    inverted_colors);
	g_signal_connect_swapped (priv->presentation_view, "finished",
				  G_CALLBACK (ev_window_view_presentation_finished),
				  window);
	g_signal_connect_swapped (priv->presentation_view, "external-link",
				  G_CALLBACK (view_external_link_cb),
				  window);
	g_signal_connect_swapped (priv->presentation_view, "focus-in-event",
				  G_CALLBACK (ev_window_view_presentation_focus_in),
				  window);
	g_signal_connect_swapped (priv->presentation_view, "focus-out-event",
				  G_CALLBACK (ev_window_view_presentation_focus_out),
				  window);

	gtk_box_pack_start (GTK_BOX (priv->main_box),
			    priv->presentation_view,
			    TRUE, TRUE, 0);

	gtk_widget_hide (priv->hpaned);
	update_chrome_visibility (window);

	gtk_widget_grab_focus (priv->presentation_view);
	if (fullscreen_window)
		gtk_window_fullscreen (GTK_WINDOW (window));

	gtk_widget_show (priv->presentation_view);

        ev_window_inhibit_screensaver (window);

	if (priv->metadata && !ev_window_is_empty (window))
		ev_metadata_set_boolean (priv->metadata, "presentation", TRUE);
}

static void
ev_window_stop_presentation (EvWindow *window,
			     gboolean  unfullscreen_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);
	guint current_page;
	guint rotation;

	if (!EV_WINDOW_IS_PRESENTATION (priv))
		return;

	current_page = ev_view_presentation_get_current_page (EV_VIEW_PRESENTATION (priv->presentation_view));
	ev_document_model_set_page (priv->model, current_page);
	rotation = ev_view_presentation_get_rotation (EV_VIEW_PRESENTATION (priv->presentation_view));
	ev_document_model_set_rotation (priv->model, rotation);

	gtk_container_remove (GTK_CONTAINER (priv->main_box),
			      priv->presentation_view);
	priv->presentation_view = NULL;

	gtk_widget_show (priv->hpaned);
	update_chrome_visibility (window);
	if (unfullscreen_window)
		gtk_window_unfullscreen (GTK_WINDOW (window));

	gtk_widget_grab_focus (priv->view);

        ev_window_uninhibit_screensaver (window);

	if (priv->metadata && !ev_window_is_empty (window))
		ev_metadata_set_boolean (priv->metadata, "presentation", FALSE);
}

static void
ev_window_cmd_view_presentation (GSimpleAction *action,
				 GVariant      *state,
				 gpointer       user_data)
{
	EvWindow *window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (window);

	if (!EV_WINDOW_IS_PRESENTATION (priv))
		ev_window_run_presentation (window);
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
	EvWindowPrivate *priv = GET_PRIVATE (window);

	if (GTK_WIDGET_CLASS (ev_window_parent_class)->window_state_event) {
		GTK_WIDGET_CLASS (ev_window_parent_class)->window_state_event (widget, event);
	}

	if ((event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) == 0)
		return FALSE;

	if (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) {
		if (ev_document_model_get_fullscreen (priv->model) || EV_WINDOW_IS_PRESENTATION (priv))
			return FALSE;

		ev_window_run_fullscreen (window);
	} else {
		if (ev_document_model_get_fullscreen (priv->model))
			ev_window_stop_fullscreen (window, FALSE);
		else if (EV_WINDOW_IS_PRESENTATION (priv))
			ev_window_stop_presentation (window, FALSE);
	}

	return FALSE;
}

static void
ev_window_set_page_mode (EvWindow         *window,
			 EvWindowPageMode  page_mode)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);
	GtkWidget *child = NULL;
	GtkWidget *real_child;

	if (priv->page_mode == page_mode)
		return;

	priv->page_mode = page_mode;

	switch (page_mode) {
	        case PAGE_MODE_DOCUMENT:
			child = priv->view;
			break;
	        case PAGE_MODE_PASSWORD:
			child = priv->password_view;
			break;
	        default:
			g_assert_not_reached ();
	}

	real_child = gtk_bin_get_child (GTK_BIN (priv->scrolled_window));
	if (child != real_child) {
		gtk_container_remove (GTK_CONTAINER (priv->scrolled_window),
				      real_child);
		gtk_container_add (GTK_CONTAINER (priv->scrolled_window),
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
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	gint rotation;

	if (EV_WINDOW_IS_PRESENTATION (priv)) {
		rotation = ev_view_presentation_get_rotation (EV_VIEW_PRESENTATION (priv->presentation_view));
		ev_view_presentation_set_rotation (EV_VIEW_PRESENTATION (priv->presentation_view),
						   rotation - 90);
	} else {
		rotation = ev_document_model_get_rotation (priv->model);

		ev_document_model_set_rotation (priv->model, rotation - 90);
	}
}

static void
ev_window_cmd_edit_rotate_right (GSimpleAction *action,
				 GVariant      *parameter,
				 gpointer       user_data)
{
	EvWindow *ev_window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	gint rotation;

	if (EV_WINDOW_IS_PRESENTATION (priv)) {
		rotation = ev_view_presentation_get_rotation (EV_VIEW_PRESENTATION (priv->presentation_view));
		ev_view_presentation_set_rotation (EV_VIEW_PRESENTATION (priv->presentation_view),
						   rotation + 90);
	} else {
		rotation = ev_document_model_get_rotation (priv->model);

		ev_document_model_set_rotation (priv->model, rotation + 90);
	}
}

static void
ev_window_cmd_view_inverted_colors (GSimpleAction *action,
				    GVariant      *state,
				    gpointer       user_data)
{
	EvWindow *ev_window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	ev_document_model_set_inverted_colors (priv->model,
					       g_variant_get_boolean (state));
	g_simple_action_set_state (action, state);
}

static void
ev_window_cmd_view_enable_spellchecking (GSimpleAction *action,
				    GVariant      *state,
				    gpointer       user_data)
{
	EvWindow *ev_window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	ev_view_set_enable_spellchecking (EV_VIEW (priv->view),
	g_variant_get_boolean (state));
	g_simple_action_set_state (action, state);
}

static void
ev_window_cmd_edit_save_settings (GSimpleAction *action,
				  GVariant      *state,
				  gpointer       user_data)
{
	EvWindow        *ev_window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	EvView          *ev_view = EV_VIEW (priv->view);
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

		zoom *= 72.0 / ev_document_misc_get_widget_dpi (GTK_WIDGET (ev_window));
		g_settings_set_double (settings, "zoom", zoom);
	}
	g_settings_set_boolean (settings, "show-sidebar",
				gtk_widget_get_visible (priv->sidebar));
	g_settings_set_int (settings, "sidebar-size",
			    gtk_paned_get_position (GTK_PANED (priv->hpaned)));
	g_settings_set_string (settings, "sidebar-page",
			       ev_window_sidebar_get_current_page_id (ev_window));
	g_settings_set_boolean (settings, "enable-spellchecking",
				ev_view_get_enable_spellchecking (ev_view));
	g_settings_apply (settings);
}

static void
ev_window_cmd_view_zoom_in (GSimpleAction *action,
			    GVariant      *parameter,
			    gpointer       user_data)
{
	EvWindow *ev_window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	ev_document_model_set_sizing_mode (priv->model, EV_SIZING_FREE);
	ev_view_zoom_in (EV_VIEW (priv->view));
}

static void
ev_window_cmd_view_zoom_out (GSimpleAction *action,
			     GVariant      *parameter,
			     gpointer       user_data)
{
	EvWindow *ev_window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	ev_document_model_set_sizing_mode (priv->model, EV_SIZING_FREE);
	ev_view_zoom_out (EV_VIEW (priv->view));
}

static void
ev_window_cmd_go_back_history (GSimpleAction *action,
			       GVariant      *parameter,
			       gpointer       user_data)
{
	EvWindow *ev_window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	ev_history_go_back (priv->history);
}

static void
ev_window_cmd_go_forward_history (GSimpleAction *action,
				  GVariant      *parameter,
				  gpointer       user_data)
{
	EvWindow *ev_window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	ev_history_go_forward (priv->history);
}

static void
ev_window_cmd_go_previous_page (GSimpleAction *action,
				GVariant      *parameter,
				gpointer       user_data)
{
	EvWindow *window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (window);

	ev_view_previous_page (EV_VIEW (priv->view));
}

static void
ev_window_cmd_go_next_page (GSimpleAction *action,
					GVariant      *parameter,
					gpointer       user_data)
{
	EvWindow *window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (window);

	ev_view_next_page (EV_VIEW (priv->view));
}

static void
ev_window_cmd_go_first_page (GSimpleAction *action,
					 GVariant      *parameter,
					 gpointer       user_data)
{
	EvWindow *window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (window);

	ev_document_model_set_page (priv->model, 0);
}

static void
ev_window_cmd_go_last_page (GSimpleAction *action,
			    GVariant      *parameter,
			    gpointer       user_data)
{
	EvWindow *window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (window);

	ev_document_model_set_page (priv->model,
				    ev_document_get_n_pages (priv->document) - 1);
}

static void
ev_window_cmd_go_forward (GSimpleAction *action,
			  GVariant      *parameter,
			  gpointer       user_data)
{
	EvWindow *ev_window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	int n_pages, current_page;

	n_pages = ev_document_get_n_pages (priv->document);
	current_page = ev_document_model_get_page (priv->model);

	if (current_page + 10 < n_pages) {
		ev_document_model_set_page (priv->model, current_page + 10);
	}
}

static void
ev_window_cmd_go_backwards (GSimpleAction *action,
			    GVariant      *parameter,
			    gpointer       user_data)
{
	EvWindow *ev_window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	int current_page;

	current_page = ev_document_model_get_page (priv->model);

	if (current_page - 10 >= 0) {
		ev_document_model_set_page (priv->model, current_page - 10);
	}
}

static gint
compare_bookmarks (EvBookmark *a,
		   EvBookmark *b)
{
	if (a->page < b->page)
		return -1;
	if (a->page > b->page)
		return 1;
	return 0;
}

static void
ev_window_setup_bookmarks (EvWindow *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);
	GList *items, *it;

	g_menu_remove_all (priv->bookmarks_menu);

	items = g_list_sort (ev_bookmarks_get_bookmarks (priv->bookmarks),
			     (GCompareFunc) compare_bookmarks);

	for (it = items; it; it = it->next) {
		EvBookmark *bookmark = it->data;
		GMenuItem *item;

		item = g_menu_item_new (bookmark->title, NULL);
		g_menu_item_set_action_and_target (item, "win.goto-bookmark", "u", bookmark->page);
		g_menu_append_item (priv->bookmarks_menu, item);

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
	EvWindowPrivate *priv = GET_PRIVATE (window);
	EvBookmark bm;
	gchar     *page_label;

	bm.page = ev_document_model_get_page (priv->model);
	page_label = ev_document_get_page_label (priv->document, bm.page);
	bm.title = g_strdup_printf (_("Page %s"), page_label);
	g_free (page_label);

	/* EvBookmarks takes ownership of bookmark */
	ev_bookmarks_add (priv->bookmarks, &bm);
}

static void
ev_window_cmd_bookmarks_delete (GSimpleAction *action,
				GVariant      *parameter,
				gpointer       user_data)
{
	EvWindow *window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (window);
	EvBookmark bm;

	bm.page = ev_document_model_get_page (priv->model);
	bm.title = NULL;

	ev_bookmarks_delete (priv->bookmarks, &bm);
}

static void
ev_window_activate_goto_bookmark_action (GSimpleAction *action,
					 GVariant      *parameter,
					 gpointer       user_data)
{
	EvWindow *window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (window);

	ev_document_model_set_page (priv->model, g_variant_get_uint32 (parameter));
}

static void
ev_window_cmd_view_reload (GSimpleAction *action,
			   GVariant      *parameter,
			   gpointer       user_data)
{
	EvWindow *ev_window = user_data;

	if (ev_window_check_document_modified (ev_window, EV_WINDOW_ACTION_RELOAD))
		return;

	ev_window_reload_document (ev_window, NULL);
}

static void
ev_window_cmd_view_autoscroll (GSimpleAction *action,
			       GVariant      *parameter,
			       gpointer       user_data)
{
	EvWindow *ev_window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	ev_view_autoscroll_start (EV_VIEW (priv->view));
}

static void
ev_window_cmd_escape (GSimpleAction *action,
		      GVariant      *parameter,
		      gpointer       user_data)
{
	EvWindow *window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (window);

	ev_view_autoscroll_stop (EV_VIEW (priv->view));

	if (gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (priv->search_bar)))
		ev_window_close_find_bar (window);
	else if (ev_document_model_get_fullscreen (priv->model))
		ev_window_stop_fullscreen (window, TRUE);
	else if (EV_WINDOW_IS_PRESENTATION (priv))
		ev_window_stop_presentation (window, TRUE);
	else {
		/* Cancel any annotation in progress and untoggle the
		 * toolbar button. */
		ev_window_cancel_add_annot (window);
		ev_annotations_toolbar_add_annot_finished (EV_ANNOTATIONS_TOOLBAR (priv->annots_toolbar));
		gtk_widget_grab_focus (priv->view);
	}
}

static void
save_sizing_mode (EvWindow *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);
	EvSizingMode mode;
	GEnumValue *enum_value;

	if (!priv->metadata || ev_window_is_empty (window))
		return;

	mode = ev_document_model_get_sizing_mode (priv->model);
	enum_value = g_enum_get_value (g_type_class_peek (EV_TYPE_SIZING_MODE), mode);
	ev_metadata_set_string (priv->metadata, "sizing_mode",
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
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	EvSizingMode sizing_mode = ev_document_model_get_sizing_mode (model);

	g_object_set (priv->scrolled_window,
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
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

        ev_window_update_actions_sensitivity (ev_window);

	if (!priv->metadata)
		return;

	if (ev_document_model_get_sizing_mode (model) == EV_SIZING_FREE && !ev_window_is_empty (ev_window)) {
		gdouble zoom;

		zoom = ev_document_model_get_scale (model);
		zoom *= 72.0 / ev_document_misc_get_widget_dpi (GTK_WIDGET (ev_window));
		ev_metadata_set_double (priv->metadata, "zoom", zoom);
	}
}

static void
ev_window_continuous_changed_cb (EvDocumentModel *model,
				 GParamSpec      *pspec,
				 EvWindow        *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	gboolean continuous;
	GAction *action;

	continuous = ev_document_model_get_continuous (model);

	action = g_action_map_lookup_action (G_ACTION_MAP (ev_window), "continuous");
	g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (continuous));

	if (priv->metadata && !ev_window_is_empty (ev_window))
		ev_metadata_set_boolean (priv->metadata, "continuous", continuous);
}

static void
ev_window_rotation_changed_cb (EvDocumentModel *model,
			       GParamSpec      *pspec,
			       EvWindow        *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);
	gint rotation = ev_document_model_get_rotation (model);

	if (priv->metadata && !ev_window_is_empty (window))
		ev_metadata_set_int (priv->metadata, "rotation",
				     rotation);
}

static void
ev_window_inverted_colors_changed_cb (EvDocumentModel *model,
			              GParamSpec      *pspec,
			              EvWindow        *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);
	gboolean inverted_colors = ev_document_model_get_inverted_colors (model);
	GAction *action;

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "inverted-colors");
	g_simple_action_set_state (G_SIMPLE_ACTION (action),
				   g_variant_new_boolean (inverted_colors));

	if (priv->metadata && !ev_window_is_empty (window))
		ev_metadata_set_boolean (priv->metadata, "inverted-colors",
					 inverted_colors);
}

static void
ev_window_dual_mode_changed_cb (EvDocumentModel *model,
				GParamSpec      *pspec,
				EvWindow        *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	gboolean dual_page;
	GAction *action;

	dual_page = ev_document_model_get_dual_page (model);

	action = g_action_map_lookup_action (G_ACTION_MAP (ev_window), "dual-page");
	g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (dual_page));

	if (priv->metadata && !ev_window_is_empty (ev_window))
		ev_metadata_set_boolean (priv->metadata, "dual-page", dual_page);
}

static void
ev_window_dual_mode_odd_pages_left_changed_cb (EvDocumentModel *model,
					       GParamSpec      *pspec,
					       EvWindow        *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	gboolean odd_left;
	GAction *action;

	odd_left = ev_document_model_get_dual_page_odd_pages_left (model);

	action = g_action_map_lookup_action (G_ACTION_MAP (ev_window), "dual-odd-left");
	g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (odd_left));

	if (priv->metadata && !ev_window_is_empty (ev_window))
		ev_metadata_set_boolean (priv->metadata, "dual-page-odd-left",
					 odd_left);
}

static void
ev_window_direction_changed_cb (EvDocumentModel *model,
                          GParamSpec      *pspec,
                          EvWindow        *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	gboolean rtl;
	GAction *action;

	rtl = ev_document_model_get_rtl (model);

	action = g_action_map_lookup_action (G_ACTION_MAP (ev_window), "rtl");
	g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (rtl));

	if (priv->metadata && !ev_window_is_empty (ev_window))
		ev_metadata_set_boolean (priv->metadata, "rtl",
					 rtl);
}

static void
ev_window_cmd_action_menu (GSimpleAction *action,
			   GVariant      *parameter,
			   gpointer       user_data)
{
	EvWindow  *ev_window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	EvToolbar *toolbar;

	toolbar = priv->fs_toolbar ? EV_TOOLBAR (priv->fs_toolbar) : EV_TOOLBAR (priv->toolbar);
	ev_toolbar_action_menu_popup (toolbar);
}

static void
ev_window_view_cmd_toggle_sidebar (GSimpleAction *action,
				   GVariant      *state,
				   gpointer       user_data)
{
	EvWindow *ev_window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	gboolean show_side_pane;

	if (EV_WINDOW_IS_PRESENTATION (priv))
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
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	if (priv->metadata && !ev_window_is_empty (ev_window)) {
		ev_metadata_set_string (priv->metadata,
					"sidebar_page",
					ev_window_sidebar_get_current_page_id (ev_window));
	}
}

static void
ev_window_sidebar_visibility_changed_cb (EvSidebar  *ev_sidebar,
					 GParamSpec *pspec,
					 EvWindow   *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	if (!EV_WINDOW_IS_PRESENTATION (priv)) {
		gboolean visible = gtk_widget_get_visible (GTK_WIDGET (ev_sidebar));

		g_action_group_change_action_state (G_ACTION_GROUP (ev_window), "show-side-pane",
						    g_variant_new_boolean (visible));

		if (priv->metadata)
			ev_metadata_set_boolean (priv->metadata, "sidebar_visibility",
						 visible);
		if (!visible)
			gtk_widget_grab_focus (priv->view);
	}
}

static void
view_menu_link_popup (EvWindow *ev_window,
		      EvLink   *link)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	gboolean  show_external = FALSE;
	gboolean  show_internal = FALSE;

	g_clear_object (&priv->link);
	if (link) {
		EvLinkAction *ev_action;

		priv->link = g_object_ref (link);

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

	ev_window_set_action_enabled (ev_window, "open-link", show_external);
	ev_window_set_action_enabled (ev_window, "copy-link-address", show_external);
	ev_window_set_action_enabled (ev_window, "go-to-link", show_internal);

	ev_window_set_action_enabled (ev_window, "open-link-new-window", show_internal);
}

static void
view_menu_image_popup (EvWindow  *ev_window,
		       EvImage   *image)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	gboolean show_image = FALSE;

	g_clear_object (&priv->image);
	if (image) {
		priv->image = g_object_ref (image);
		show_image = TRUE;
	}

	ev_window_set_action_enabled (ev_window, "save-image", show_image);
	ev_window_set_action_enabled (ev_window, "copy-image", show_image);
}

static void
view_menu_annot_popup (EvWindow     *ev_window,
		       EvAnnotation *annot)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	gboolean show_annot_props = FALSE;
	gboolean show_attachment = FALSE;
	gboolean can_remove_annots = FALSE;

	g_clear_object (&priv->annot);
	if (annot) {
		priv->annot = g_object_ref (annot);

		show_annot_props = EV_IS_ANNOTATION_MARKUP (annot);

		if (EV_IS_ANNOTATION_ATTACHMENT (annot)) {
			EvAttachment *attachment;

			attachment = ev_annotation_attachment_get_attachment (EV_ANNOTATION_ATTACHMENT (annot));
			if (attachment) {
				show_attachment = TRUE;

				g_list_free_full (priv->attach_list,
						  g_object_unref);
				priv->attach_list = NULL;
				priv->attach_list =
					g_list_prepend (priv->attach_list,
							g_object_ref (attachment));
			}
		}
	}

	if (EV_IS_DOCUMENT_ANNOTATIONS (priv->document))
		can_remove_annots = ev_document_annotations_can_remove_annotation (EV_DOCUMENT_ANNOTATIONS (priv->document));

	ev_window_set_action_enabled (ev_window, "annot-properties", show_annot_props);
	ev_window_set_action_enabled (ev_window, "remove-annot", annot != NULL && can_remove_annots);
	ev_window_set_action_enabled (ev_window, "open-attachment", show_attachment);
	ev_window_set_action_enabled (ev_window, "save-attachment", show_attachment);
}

static gboolean
view_menu_popup_cb (EvView   *view,
		    GList    *items,
		    EvWindow *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	EvDocument *document = priv->document;
	GList   *l;
	gboolean has_link = FALSE;
	gboolean has_image = FALSE;
	gboolean has_annot = FALSE;
	gboolean can_annotate;

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

	can_annotate = EV_IS_DOCUMENT_ANNOTATIONS (document) &&
		ev_document_annotations_can_add_annotation (EV_DOCUMENT_ANNOTATIONS (document)) &&
		!has_annot && ev_view_get_has_selection (view);

	ev_window_set_action_enabled (ev_window, "annotate-selected-text", can_annotate);

	if (!priv->view_popup) {
		priv->view_popup = gtk_menu_new_from_model (priv->view_popup_menu);
		gtk_menu_attach_to_widget (GTK_MENU (priv->view_popup),
					   GTK_WIDGET (ev_window), NULL);
	}

	gtk_menu_popup_at_pointer (GTK_MENU (priv->view_popup), NULL);
	return TRUE;
}

static gboolean
attachment_bar_menu_popup_cb (EvSidebarAttachments *attachbar,
			      GList                *attach_list,
			      EvWindow             *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	g_assert (attach_list != NULL);

	ev_window_set_action_enabled (ev_window, "open-attachment", TRUE);

	ev_window_set_action_enabled (ev_window, "save-attachment", TRUE);

	g_list_free_full (priv->attach_list, g_object_unref);
	priv->attach_list = attach_list;

	if (!priv->attachment_popup) {
		priv->attachment_popup = gtk_menu_new_from_model (priv->attachment_popup_menu);
		gtk_menu_attach_to_widget (GTK_MENU (priv->attachment_popup),
					   GTK_WIDGET (ev_window), NULL);
	}

	gtk_menu_popup_at_pointer (GTK_MENU (priv->attachment_popup), NULL);

	return TRUE;
}

static gboolean
save_attachment_to_target_file (EvAttachment *attachment,
                                GFile        *target_file,
                                gboolean      is_dir,
                                gboolean      is_native,
                                EvWindow     *ev_window)
{
	GFile  *save_to = NULL;
	GError *error = NULL;

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

		return FALSE;
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
	return TRUE;
}

static gboolean
attachment_bar_save_attachment_cb (EvSidebarAttachments  *attachbar,
                                   EvAttachment          *attachment,
                                   const char            *uri,
                                   EvWindow              *ev_window)
{
	GFile    *target_file;
	gboolean  is_native;
	gboolean  success;

	target_file = g_file_new_for_uri (uri);
	is_native = g_file_is_native (target_file);

	success = save_attachment_to_target_file (attachment,
	                                          target_file,
	                                          FALSE,
	                                          is_native,
	                                          ev_window);

	g_object_unref (target_file);
	return success;
}

static void
find_sidebar_result_activated_cb (EvFindSidebar *find_sidebar,
				  gint           page,
				  gint           result,
				  EvWindow      *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);

	ev_view_find_set_result (EV_VIEW (priv->view), page, result);
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
search_entry_stop_search_cb (GtkSearchEntry *entry,
			     EvWindow       *ev_window)
{
	ev_window_close_find_bar (ev_window);
}

static void
search_started_cb (EvSearchBox *search_box,
		   EvJobFind   *job,
		   EvWindow    *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	if (!priv->document || !EV_IS_DOCUMENT_FIND (priv->document))
		return;

	ev_view_find_search_changed (EV_VIEW (priv->view));
	ev_view_find_started (EV_VIEW (priv->view), job);
	ev_find_sidebar_start (EV_FIND_SIDEBAR (priv->find_sidebar), job);
}

static void
search_updated_cb (EvSearchBox *search_box,
		   gint         page,
		   EvWindow    *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	ev_window_update_actions_sensitivity (ev_window);
	ev_find_sidebar_update (EV_FIND_SIDEBAR (priv->find_sidebar));
}

static void
search_cleared_cb (EvSearchBox *search_box,
		   EvWindow    *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	ev_window_update_actions_sensitivity (ev_window);
	ev_find_sidebar_clear (EV_FIND_SIDEBAR (priv->find_sidebar));

	ev_view_find_search_changed (EV_VIEW (priv->view));
	gtk_widget_queue_draw (GTK_WIDGET (priv->view));
}

static void
search_previous_cb (EvSearchBox *search_box,
		    EvWindow    *ev_window)
{
	ev_window_find_previous (ev_window);
}

static void
search_next_cb (EvSearchBox *search_box,
		EvWindow    *ev_window)
{
	ev_window_find_next (ev_window);
}

static void
search_bar_search_mode_enabled_changed (GtkSearchBar *search_bar,
					GParamSpec   *param,
					EvWindow     *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	gboolean enabled = gtk_search_bar_get_search_mode (search_bar);

	ev_view_find_set_highlight_search (EV_VIEW (priv->view), enabled);
	ev_window_update_actions_sensitivity (ev_window);

	if (!enabled) {
		/* Handle the case of search bar close button clicked */
		ev_window_close_find_bar (ev_window);
	}
}

void
ev_window_handle_annot_popup (EvWindow     *ev_window,
			      EvAnnotation *annot)
{
	view_menu_annot_popup (ev_window, annot);
}

static void
ev_window_show_find_bar (EvWindow *ev_window,
			 gboolean  restart)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	if (gtk_widget_get_visible (priv->find_sidebar)) {
		gtk_widget_grab_focus (priv->search_box);
		return;
	}

	if (priv->document == NULL || !EV_IS_DOCUMENT_FIND (priv->document)) {
		g_error ("Find action should be insensitive since document doesn't support find");
		return;
	}

	if (EV_WINDOW_IS_PRESENTATION (priv))
		return;

	ev_history_freeze (priv->history);

	g_object_ref (priv->sidebar);
	gtk_container_remove (GTK_CONTAINER (priv->hpaned), priv->sidebar);
	gtk_paned_pack1 (GTK_PANED (priv->hpaned),
			 priv->find_sidebar, FALSE, FALSE);
	gtk_widget_show (priv->find_sidebar);


	gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (priv->search_bar), TRUE);
	gtk_widget_grab_focus (priv->search_box);
	g_action_group_change_action_state (G_ACTION_GROUP (ev_window), "toggle-find", g_variant_new_boolean (TRUE));

	if (restart) {
		GtkSearchEntry *entry = ev_search_box_get_entry (EV_SEARCH_BOX (priv->search_box));
		const char     *search_string = gtk_entry_get_text (GTK_ENTRY (entry));

		if (search_string && search_string[0])
			ev_window_find_restart (ev_window);
	}
}

static void
ev_window_close_find_bar (EvWindow *ev_window)
{
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	if (!gtk_widget_get_visible (priv->find_sidebar))
		return;

	g_object_ref (priv->find_sidebar);
	gtk_container_remove (GTK_CONTAINER (priv->hpaned),
			      priv->find_sidebar);
	gtk_paned_pack1 (GTK_PANED (priv->hpaned),
			 priv->sidebar, FALSE, FALSE);
	gtk_widget_hide (priv->find_sidebar);

	gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (priv->search_bar), FALSE);
	gtk_widget_grab_focus (priv->view);
	g_action_group_change_action_state (G_ACTION_GROUP (ev_window), "toggle-find", g_variant_new_boolean (FALSE));

	ev_history_thaw (priv->history);
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
	EvWindowPrivate *priv = GET_PRIVATE (window);
	GAction *action;

	if (priv->metadata)
		ev_metadata_set_boolean (priv->metadata, "caret-navigation", enabled);

	ev_view_set_caret_navigation_enabled (EV_VIEW (priv->view), enabled);

	action = g_action_map_lookup_action (G_ACTION_MAP (window), "caret-navigation");
	g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (enabled));
}

static void
ev_window_caret_navigation_message_area_response_cb (EvMessageArea *area,
						     gint           response_id,
						     EvWindow      *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);

	/* Turn the caret navigation mode on */
	if (response_id == GTK_RESPONSE_YES)
		ev_window_set_caret_navigation_enabled (window, TRUE);

	/* Turn the confirmation dialog off if the user has requested not to show it again */
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->ask_caret_navigation_check))) {
		g_settings_set_boolean (ev_window_ensure_settings (window), "show-caret-navigation-message", FALSE);
		g_settings_apply (priv->settings);
	}

	priv->ask_caret_navigation_check = NULL;
	ev_window_set_message_area (window, NULL);
	gtk_widget_grab_focus (priv->view);
}

static void
ev_window_cmd_view_toggle_caret_navigation (GSimpleAction *action,
					    GVariant      *state,
					    gpointer       user_data)
{
	EvWindow  *window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (window);
	GtkWidget *message_area;
	GtkWidget *box;
	GtkWidget *hbox;
	gboolean   enabled;

	/* Don't ask for user confirmation to turn the caret navigation off when it is active,
	 * or to turn it on when the confirmation dialog is not to be shown per settings */
	enabled = ev_view_is_caret_navigation_enabled (EV_VIEW (priv->view));
	if (enabled || !g_settings_get_boolean (ev_window_ensure_settings (window), "show-caret-navigation-message")) {
		ev_window_set_caret_navigation_enabled (window, !enabled);
		return;
	}

	/* Ask for user confirmation to turn the caret navigation mode on */
	if (priv->message_area)
		return;

	message_area = ev_message_area_new (GTK_MESSAGE_QUESTION,
					    _("Enable caret navigation?"),
					    _("_Enable"), GTK_RESPONSE_YES,
					    NULL);
	ev_message_area_set_secondary_text (EV_MESSAGE_AREA (message_area),
					    _("Pressing F7 turns the caret navigation on or off. "
					      "This feature places a moveable cursor in text pages, "
					      "allowing you to move around and select text with your keyboard. "
					      "Do you want to enable the caret navigation?"));

	priv->ask_caret_navigation_check = gtk_check_button_new_with_label (_("Don’t show this message again"));
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_box_pack_start (GTK_BOX (hbox), priv->ask_caret_navigation_check,
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
ev_window_cmd_add_highlight_annotation (GSimpleAction *action,
                                        GVariant      *state,
                                        gpointer       user_data)
{
	EvWindow *ev_window = user_data;

	ev_window_begin_add_annot (ev_window, EV_ANNOTATION_TYPE_TEXT_MARKUP);
}

static void
ev_window_cmd_add_annotation (GSimpleAction *action,
			      GVariant      *state,
			      gpointer       user_data)
{
	EvWindow *ev_window = user_data;

	ev_window_begin_add_annot (ev_window, EV_ANNOTATION_TYPE_TEXT);
}

static void
ev_window_cmd_toggle_edit_annots (GSimpleAction *action,
				  GVariant      *state,
				  gpointer       user_data)
{
	EvWindow *ev_window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	if (g_variant_get_boolean (state))
		gtk_widget_show (priv->annots_toolbar);
	else
		gtk_widget_hide (priv->annots_toolbar);

	g_simple_action_set_state (action, state);
}

static void
ev_window_dispose (GObject *object)
{
	EvWindow *window = EV_WINDOW (object);
	EvWindowPrivate *priv = GET_PRIVATE (window);
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
		g_list_free_full (priv->attach_list, g_object_unref);
		priv->attach_list = NULL;
	}

	if (priv->uri) {
		g_free (priv->uri);
		priv->uri = NULL;
	}

	g_clear_pointer (&priv->display_name, g_free);
	g_clear_pointer (&priv->edit_name, g_free);

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
ev_window_button_press_event (GtkWidget      *widget,
                              GdkEventButton *event)
{
        EvWindow *window = EV_WINDOW (widget);
	EvWindowPrivate *priv = GET_PRIVATE (window);

        switch (event->button) {
        case MOUSE_BACK_BUTTON:
                ev_history_go_back (priv->history);
                return TRUE;
        case MOUSE_FORWARD_BUTTON:
                ev_history_go_forward (priv->history);
                return TRUE;
        default:
                break;
        }

        return FALSE;
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
	widget_class->button_press_event = ev_window_button_press_event;

	nautilus_sendto = g_find_program_in_path ("nautilus-sendto");
}

static const GActionEntry actions[] = {
	{ "new", ev_window_cmd_new_window },
	{ "open", ev_window_cmd_file_open },
	{ "open-copy", ev_window_cmd_file_open_copy },
	{ "save-as", ev_window_cmd_save_as },
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
	{ "rtl", NULL, NULL, "false", ev_window_cmd_rtl },
	{ "show-side-pane", NULL, NULL, "false", ev_window_view_cmd_toggle_sidebar },
	{ "inverted-colors", NULL, NULL, "false", ev_window_cmd_view_inverted_colors },
	{ "enable-spellchecking", NULL, NULL, "false", ev_window_cmd_view_enable_spellchecking },
	{ "fullscreen", NULL, NULL, "false", ev_window_cmd_view_fullscreen },
	{ "presentation", ev_window_cmd_view_presentation },
	{ "rotate-left", ev_window_cmd_edit_rotate_left },
	{ "rotate-right", ev_window_cmd_edit_rotate_right },
	{ "zoom-in", ev_window_cmd_view_zoom_in },
	{ "zoom-out", ev_window_cmd_view_zoom_out },
	{ "reload", ev_window_cmd_view_reload },
	{ "auto-scroll", ev_window_cmd_view_autoscroll },
	{ "add-bookmark", ev_window_cmd_bookmarks_add },
	{ "delete-bookmark", ev_window_cmd_bookmarks_delete },
	{ "goto-bookmark", ev_window_activate_goto_bookmark_action, "u" },
	{ "close", ev_window_cmd_file_close_window },
	{ "scroll-forward", ev_window_cmd_scroll_forward },
	{ "scroll-backwards", ev_window_cmd_scroll_backwards },
	{ "sizing-mode", NULL, "s", "'free'", ev_window_change_sizing_mode_action_state },
	{ "zoom", ev_window_cmd_view_zoom, "d" },
	{ "default-zoom", ev_window_cmd_set_default_zoom },
	{ "escape", ev_window_cmd_escape },
	{ "open-menu", ev_window_cmd_action_menu },
	{ "caret-navigation", NULL, NULL, "false", ev_window_cmd_view_toggle_caret_navigation },
	{ "add-annotation", NULL, NULL, "false", ev_window_cmd_add_annotation },
	{ "highlight-annotation", NULL, NULL, "false", ev_window_cmd_add_highlight_annotation },
	{ "toggle-edit-annots", NULL, NULL, "false", ev_window_cmd_toggle_edit_annots },
	{ "about", ev_window_cmd_about },
	{ "help", ev_window_cmd_help },
	/* Popups specific items */
	{ "annotate-selected-text", ev_window_popup_cmd_annotate_selected_text },
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
	EvWindowPrivate *priv = GET_PRIVATE (window);

	ev_view_handle_link (EV_VIEW (priv->view), link);
}

static void
activate_link_cb (GObject *object, EvLink *link, EvWindow *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);

	ev_view_handle_link (EV_VIEW (priv->view), link);
	gtk_widget_grab_focus (priv->view);
}

static void
history_changed_cb (EvHistory *history,
                    EvWindow  *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);

	ev_window_set_action_enabled (window, "go-back-history",
				      ev_history_can_go_back (priv->history));
	ev_window_set_action_enabled (window, "go-forward-history",
				      ev_history_can_go_forward (priv->history));
}

static void
sidebar_layers_visibility_changed (EvSidebarLayers *layers,
				   EvWindow        *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);

	ev_view_reload (EV_VIEW (priv->view));
}

static void
sidebar_annots_annot_activated_cb (EvSidebarAnnotations *sidebar_annots,
				   EvMapping            *annot_mapping,
				   EvWindow             *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);

	ev_view_focus_annotation (EV_VIEW (priv->view), annot_mapping);
}

static void
ev_window_begin_add_annot (EvWindow        *window,
			   EvAnnotationType annot_type)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);

	if (annot_type == EV_ANNOTATION_TYPE_TEXT_MARKUP &&
	    ev_view_get_has_selection (EV_VIEW (priv->view))) {
		ev_view_add_text_markup_annotation_for_selected_text (EV_VIEW (priv->view));
		return;
	}

	ev_view_begin_add_annotation (EV_VIEW (priv->view), annot_type);
}

static void
view_annot_added (EvView       *view,
		  EvAnnotation *annot,
		  EvWindow     *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);

	ev_sidebar_annotations_annot_added (EV_SIDEBAR_ANNOTATIONS (priv->sidebar_annots),
					    annot);
	ev_annotations_toolbar_add_annot_finished (EV_ANNOTATIONS_TOOLBAR (priv->annots_toolbar));
}

static void
view_annot_removed (EvView       *view,
		    EvAnnotation *annot,
		    EvWindow     *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);

	ev_sidebar_annotations_annot_removed (EV_SIDEBAR_ANNOTATIONS (priv->sidebar_annots));
}

static void
ev_window_cancel_add_annot(EvWindow *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);

	ev_view_cancel_add_annotation (EV_VIEW (priv->view));
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
	EvWindowPrivate *priv = GET_PRIVATE (window);

	if (!(event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN)) {
		gboolean maximized;

		maximized = event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED;
		if (priv->metadata && !ev_window_is_empty (window))
			ev_metadata_set_boolean (priv->metadata, "window_maximized", maximized);
	}

	return FALSE;
}

static gboolean
window_configure_event_cb (EvWindow *window, GdkEventConfigure *event, gpointer dummy)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);
	GdkWindowState state;
	gdouble document_width, document_height;
	gint window_x, window_y, window_width, window_height;

	if (!priv->metadata)
		return FALSE;

	state = gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET (window)));

	if (!(state & GDK_WINDOW_STATE_FULLSCREEN) &&
	    !(state & GDK_WINDOW_STATE_MAXIMIZED)) {
		if (priv->document) {
			ev_document_get_max_page_size (priv->document,
						       &document_width, &document_height);
			gtk_window_get_size (GTK_WINDOW (window), &window_width, &window_height);
			gtk_window_get_position (GTK_WINDOW (window), &window_x, &window_y);
			g_settings_set (priv->default_settings, "window-ratio", "(dd)",
					(double)window_width / document_width,
					(double)window_height / document_height);

			ev_metadata_set_int (priv->metadata, "window_x", window_x);
			ev_metadata_set_int (priv->metadata, "window_y", window_y);
			ev_metadata_set_int (priv->metadata, "window_width",window_width);
			ev_metadata_set_int (priv->metadata, "window_height", window_height);
		}
	}

	return FALSE;
}

static void
launch_action (EvWindow *window, EvLinkAction *action)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);
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

		dir = g_path_get_dirname (priv->uri);
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
	EvWindowPrivate *priv = GET_PRIVATE (window);
	const gchar *uri = ev_link_action_get_uri (action);
	GError *error = NULL;
	gboolean ret;
	GdkAppLaunchContext *context;
	GdkScreen *screen;
	GFile *file;
	gchar *uri_scheme;

	screen = gtk_window_get_screen (GTK_WINDOW (window));
	context = gdk_display_get_app_launch_context (gdk_screen_get_display (screen));
	gdk_app_launch_context_set_screen (context, screen);
	gdk_app_launch_context_set_timestamp (context, gtk_get_current_event_time ());
	file = g_file_new_for_uri (uri);
	uri_scheme = g_file_get_uri_scheme (file);
	g_object_unref (file);

	if (uri_scheme == NULL) {
		gchar *new_uri;

		/* Not a valid uri, assume http if it starts with www */
		if (g_str_has_prefix (uri, "www.")) {
			new_uri = g_strdup_printf ("http://%s", uri);
		} else {
			GFile *parent;

			file = g_file_new_for_uri (priv->uri);
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

        g_object_unref (context);
}

static void
open_remote_link (EvWindow *window, EvLinkAction *action)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);
	gchar *uri;
	gchar *dir;

	dir = g_path_get_dirname (priv->uri);
	
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
	} else if (g_ascii_strcasecmp (name, "SaveAs") == 0) {
		g_action_group_activate_action (G_ACTION_GROUP (window), "save-as", NULL);
	} else {
		g_warning ("Unimplemented named action: %s, please post a "
		           "bug report in Evince Gitlab "
		           "(https://gitlab.gnome.org/GNOME/evince/issues) "
			   "with a testcase.",
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
	EvWindowPrivate *priv = GET_PRIVATE (window);

	ev_view_handle_link (EV_VIEW (priv->view), priv->link);
}

static void
ev_window_popup_cmd_annotate_selected_text (GSimpleAction *action,
					    GVariant      *parameter,
					    gpointer       user_data)
{
	EvWindow *ev_window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
	EvView *view = EV_VIEW (priv->view);
	ev_view_add_text_markup_annotation_for_selected_text (view);
}

static void
ev_window_popup_cmd_open_link_new_window (GSimpleAction *action,
					  GVariant      *parameter,
					  gpointer       user_data)
{
	EvLinkAction *ev_action = NULL;
	EvLinkDest   *dest;
	EvWindow     *window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (window);

	ev_action = ev_link_get_action (priv->link);
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
	EvWindowPrivate *priv = GET_PRIVATE (window);

	ev_action = ev_link_get_action (priv->link);
	if (!ev_action)
		return;

	ev_view_copy_link_address (EV_VIEW (priv->view),
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
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
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
					 _("Couldn’t find appropriate format to save image"));
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
	pixbuf = ev_document_images_get_image (EV_DOCUMENT_IMAGES (priv->document),
					       priv->image);
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
	EvWindowPrivate *priv = GET_PRIVATE (window);

	if (!priv->image)
		return;

	fc = gtk_file_chooser_dialog_new (_("Save Image"),
					  GTK_WINDOW (window),
					  GTK_FILE_CHOOSER_ACTION_SAVE,
					  _("_Cancel"),
					  GTK_RESPONSE_CANCEL,
					  _("_Save"), GTK_RESPONSE_OK,
					  NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (fc), GTK_RESPONSE_OK);

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
	EvWindowPrivate *priv = GET_PRIVATE (window);

	if (!priv->image)
		return;

	clipboard = gtk_widget_get_clipboard (GTK_WIDGET (window),
					      GDK_SELECTION_CLIPBOARD);
	ev_document_doc_mutex_lock ();
	pixbuf = ev_document_images_get_image (EV_DOCUMENT_IMAGES (priv->document),
					       priv->image);
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
	EvWindowPrivate              *priv = GET_PRIVATE (window);
	const gchar                  *author;
	GdkRGBA                       rgba;
	gdouble                       opacity;
	gboolean                      popup_is_open;
	EvAnnotationPropertiesDialog *dialog;
	EvAnnotation                 *annot = priv->annot;
	EvAnnotationsSaveMask         mask = EV_ANNOTATIONS_SAVE_NONE;

	if (!annot)
		return;

	dialog = EV_ANNOTATION_PROPERTIES_DIALOG (ev_annotation_properties_dialog_new_with_annotation (priv->annot));
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

	if (EV_IS_ANNOTATION_TEXT_MARKUP (annot)) {
		EvAnnotationTextMarkupType markup_type;

		markup_type = ev_annotation_properties_dialog_get_text_markup_type (dialog);
		if (ev_annotation_text_markup_set_markup_type (EV_ANNOTATION_TEXT_MARKUP (annot), markup_type))
			mask |= EV_ANNOTATIONS_SAVE_TEXT_MARKUP_TYPE;
	}

	if (mask != EV_ANNOTATIONS_SAVE_NONE) {
		ev_document_doc_mutex_lock ();
		ev_document_annotations_save_annotation (EV_DOCUMENT_ANNOTATIONS (priv->document),
							 priv->annot,
							 mask);
		ev_document_doc_mutex_unlock ();

		/* FIXME: update annot region only */
		ev_view_reload (EV_VIEW (priv->view));
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
ev_window_popup_cmd_remove_annotation (GSimpleAction *action,
				       GVariant      *parameter,
				       gpointer       user_data)
{
	EvWindow *window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (window);

	ev_view_remove_annotation (EV_VIEW (priv->view),
				   priv->annot);
}

static void
ev_window_popup_cmd_open_attachment (GSimpleAction *action,
				     GVariant      *parameter,
				     gpointer       user_data)
{
	GList     *l;
	GdkScreen *screen;
	EvWindow  *window = user_data;
	EvWindowPrivate *priv = GET_PRIVATE (window);

	if (!priv->attach_list)
		return;

	screen = gtk_window_get_screen (GTK_WINDOW (window));

	for (l = priv->attach_list; l && l->data; l = g_list_next (l)) {
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
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);
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

	for (l = priv->attach_list; l && l->data; l = g_list_next (l)) {
		EvAttachment *attachment;

		attachment = (EvAttachment *) l->data;

		save_attachment_to_target_file (attachment,
		                                target_file,
		                                is_dir,
		                                is_native,
		                                ev_window);
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
	EvWindowPrivate *priv = GET_PRIVATE (window);

	if (!priv->attach_list)
		return;

	if (g_list_length (priv->attach_list) == 1)
		attachment = (EvAttachment *) priv->attach_list->data;
	
	fc = gtk_file_chooser_dialog_new (
		_("Save Attachment"),
		GTK_WINDOW (window),
		attachment ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
		_("_Cancel"),
		GTK_RESPONSE_CANCEL,
		_("_Save"), GTK_RESPONSE_OK,
		NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (fc), GTK_RESPONSE_OK);

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
	EvWindowPrivate *priv = GET_PRIVATE (window);

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
		if (EV_WINDOW_IS_PRESENTATION (priv))
			ev_view_presentation_previous_page (EV_VIEW_PRESENTATION (priv->presentation_view));
		else
			g_action_group_activate_action (G_ACTION_GROUP (window), "go-previous-page", NULL);
	} else if (strcmp (key, "Next") == 0) {
		if (EV_WINDOW_IS_PRESENTATION (priv))
			ev_view_presentation_next_page (EV_VIEW_PRESENTATION (priv->presentation_view));
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
	EvWindowPrivate *priv = GET_PRIVATE (window);
	guint32		 timestamp;
	gchar		*uri_input;
	GFile		*input_gfile;

        if (priv->skeleton == NULL)
		return;

	timestamp = gtk_get_current_event_time ();
	if (g_path_is_absolute (link->filename)) {
		input_gfile = g_file_new_for_path (link->filename);
	} else {
		GFile *gfile, *parent_gfile;

		gfile = g_file_new_for_uri (priv->uri);
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

        ev_evince_window_emit_sync_source (priv->skeleton,
                                           uri_input,
                                           g_variant_new ("(ii)", link->line, link->col),
                                           timestamp);
	g_free (uri_input);
}

static void
ev_window_emit_closed (EvWindow *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);

	if (priv->skeleton == NULL)
		return;

        ev_evince_window_emit_closed (priv->skeleton);

	/* If this is the last window call g_dbus_connection_flush_sync()
	 * to make sure the signal is emitted.
	 */
	if (ev_application_get_n_windows (EV_APP) == 1)
		g_dbus_connection_flush_sync (g_application_get_dbus_connection (g_application_get_default ()), NULL, NULL);
}

static void
ev_window_emit_doc_loaded (EvWindow *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);

        if (priv->skeleton == NULL)
                return;

        ev_evince_window_emit_document_loaded (priv->skeleton, priv->uri);
}

static gboolean
handle_sync_view_cb (EvEvinceWindow        *object,
		     GDBusMethodInvocation *invocation,
		     const gchar           *source_file,
		     GVariant              *source_point,
		     guint                  timestamp,
		     EvWindow              *window)
{
	EvWindowPrivate *priv = GET_PRIVATE (window);

	if (priv->document && ev_document_has_synctex (priv->document)) {
		EvSourceLink link;

		link.filename = (char *) source_file;
		g_variant_get (source_point, "(ii)", &link.line, &link.col);
		ev_view_highlight_forward_search (EV_VIEW (priv->view), &link);
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
ev_window_init_css (void)
{
	static gsize initialization_value = 0;

	if (g_once_init_enter (&initialization_value)) {
		GtkCssProvider *css_provider;
		GError *error = NULL;

		css_provider = gtk_css_provider_new ();
		_gtk_css_provider_load_from_resource (css_provider,
						      "/org/gnome/evince/ui/evince.css",
						      &error);
		g_assert_no_error (error);
		gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
						GTK_STYLE_PROVIDER (css_provider),
						GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		g_object_unref (css_provider);

		g_once_init_leave (&initialization_value, 1);
	}
}

static void
ev_window_init (EvWindow *ev_window)
{
	GtkBuilder *builder;
	GError *error = NULL;
	GtkWidget *sidebar_widget;
	GtkWidget *overlay;
	GtkWidget *searchbar_revealer;
	GObject *mpkeys;
	guint page_cache_mb;
	gboolean allow_links_change_zoom;
	GtkEntry *search_entry;
	EvWindowPrivate *priv;
#ifdef ENABLE_DBUS
	GDBusConnection *connection;
	static gint window_id = 0;
#endif
	GAppInfo *app_info;

	g_signal_connect (ev_window, "configure_event",
			  G_CALLBACK (window_configure_event_cb), NULL);
	g_signal_connect (ev_window, "window_state_event",
			  G_CALLBACK (window_state_event_cb), NULL);

	priv = GET_PRIVATE (ev_window);

#ifdef ENABLE_DBUS
	connection = g_application_get_dbus_connection (g_application_get_default ());
        if (connection) {
                EvEvinceWindow *skeleton;

		priv->dbus_object_path = g_strdup_printf (EV_WINDOW_DBUS_OBJECT_PATH, window_id++);

                skeleton = ev_evince_window_skeleton_new ();
                if (g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (skeleton),
                                                      connection,
                                                      priv->dbus_object_path,
                                                      &error)) {
                        priv->skeleton = skeleton;
			g_signal_connect (skeleton, "handle-sync-view",
					  G_CALLBACK (handle_sync_view_cb),
					  ev_window);
                } else {
                        g_printerr ("Failed to register bus object %s: %s\n",
				    priv->dbus_object_path, error->message);
                        g_error_free (error);
			g_free (priv->dbus_object_path);
			priv->dbus_object_path = NULL;
			error = NULL;

                        g_object_unref (skeleton);
                        priv->skeleton = NULL;
                }
        }
#endif /* ENABLE_DBUS */

	priv->model = ev_document_model_new ();

	priv->page_mode = PAGE_MODE_DOCUMENT;
	priv->chrome = EV_CHROME_NORMAL;
        priv->presentation_mode_inhibit_id = 0;

	priv->history = ev_history_new (priv->model);
	g_signal_connect (priv->history, "activate-link",
			  G_CALLBACK (activate_link_cb),
			  ev_window);
        g_signal_connect (priv->history, "changed",
                          G_CALLBACK (history_changed_cb),
                          ev_window);

	priv->bookmarks_menu = g_menu_new ();

	app_info = g_app_info_get_default_for_uri_scheme ("mailto");
	priv->has_mailto_handler = app_info != NULL;
	g_clear_object (&app_info);

	priv->main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (ev_window), priv->main_box);
	gtk_widget_show (priv->main_box);

	g_action_map_add_action_entries (G_ACTION_MAP (ev_window),
					 actions, G_N_ELEMENTS (actions),
					 ev_window);

	ev_window_init_css ();

	priv->recent_manager = gtk_recent_manager_get_default ();

	priv->toolbar = ev_toolbar_new (ev_window);
	gtk_widget_set_no_show_all (priv->toolbar, TRUE);
	gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (priv->toolbar), TRUE);
	gtk_window_set_titlebar (GTK_WINDOW (ev_window), priv->toolbar);
	gtk_widget_show (priv->toolbar);

	/* Window title */
	priv->title = ev_window_title_new (ev_window);

	g_signal_connect (ev_toolbar_get_page_selector (EV_TOOLBAR (priv->toolbar)),
			  "activate-link",
			  G_CALLBACK (activate_link_cb),
			  ev_window);

        /* Annotations toolbar */
	priv->annots_toolbar = ev_annotations_toolbar_new ();
	g_signal_connect_swapped (priv->annots_toolbar,
				  "begin-add-annot",
				  G_CALLBACK (ev_window_begin_add_annot),
				  ev_window);
	g_signal_connect_swapped (priv->annots_toolbar,
				  "cancel-add-annot",
				  G_CALLBACK (ev_window_cancel_add_annot),
				  ev_window);
	gtk_box_pack_start (GTK_BOX (priv->main_box),
			    priv->annots_toolbar, FALSE, TRUE, 0);

	/* Search Bar */
	priv->search_bar = gtk_search_bar_new ();
	gtk_search_bar_set_show_close_button (GTK_SEARCH_BAR (priv->search_bar), TRUE);

	priv->search_box = ev_search_box_new (priv->model);
	search_entry = GTK_ENTRY (ev_search_box_get_entry (EV_SEARCH_BOX (priv->search_box)));
	gtk_entry_set_width_chars (search_entry, 32);
	gtk_entry_set_max_length (search_entry, 512);
	gtk_container_add (GTK_CONTAINER (priv->search_bar),
			   priv->search_box);
	gtk_widget_show (priv->search_box);

	/* Wrap search bar in a revealer.
	 * Workaround for the gtk+ bug: https://bugzilla.gnome.org/show_bug.cgi?id=724096
	 */
	searchbar_revealer = gtk_revealer_new ();
	g_object_bind_property (G_OBJECT (searchbar_revealer), "reveal-child",
				G_OBJECT (priv->search_bar), "search-mode-enabled",
				G_BINDING_BIDIRECTIONAL);
	gtk_container_add (GTK_CONTAINER (searchbar_revealer), priv->search_bar);
	gtk_widget_show (GTK_WIDGET (searchbar_revealer));

	/* We don't use gtk_search_bar_connect_entry, because it clears the entry when the
	 * search is closed, but we want to keep the current search.
	 */
	gtk_box_pack_start (GTK_BOX (priv->main_box),
			    searchbar_revealer, FALSE, TRUE, 0);
	gtk_widget_show (priv->search_bar);

	/* Add the main area */
	priv->hpaned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	g_signal_connect (priv->hpaned,
			  "notify::position",
			  G_CALLBACK (ev_window_sidebar_position_change_cb),
			  ev_window);
	
	gtk_paned_set_position (GTK_PANED (priv->hpaned), SIDEBAR_DEFAULT_SIZE);
	gtk_box_pack_start (GTK_BOX (priv->main_box), priv->hpaned,
			    TRUE, TRUE, 0);
	gtk_widget_show (priv->hpaned);
	
	priv->sidebar = ev_sidebar_new ();
	ev_sidebar_set_model (EV_SIDEBAR (priv->sidebar),
			      priv->model);
	gtk_paned_pack1 (GTK_PANED (priv->hpaned),
			 priv->sidebar, FALSE, FALSE);
	gtk_widget_show (priv->sidebar);

	/* Stub sidebar, for now */

	sidebar_widget = ev_sidebar_thumbnails_new ();
	priv->sidebar_thumbs = sidebar_widget;
	g_signal_connect (sidebar_widget,
			  "notify::main-widget",
			  G_CALLBACK (sidebar_page_main_widget_update_cb),
			  ev_window);
	sidebar_page_main_widget_update_cb (G_OBJECT (sidebar_widget), NULL, ev_window);
	gtk_widget_show (sidebar_widget);
	ev_sidebar_add_page (EV_SIDEBAR (priv->sidebar),
			      sidebar_widget,
			      THUMBNAILS_SIDEBAR_ID, _("Thumbnails"),
			      THUMBNAILS_SIDEBAR_ICON);

	sidebar_widget = ev_sidebar_links_new ();
	priv->sidebar_links = sidebar_widget;
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
	ev_sidebar_add_page (EV_SIDEBAR (priv->sidebar),
			      sidebar_widget,
			      LINKS_SIDEBAR_ID, _("Outline"),
			      LINKS_SIDEBAR_ICON);

	sidebar_widget = ev_sidebar_annotations_new ();
	priv->sidebar_annots = sidebar_widget;
	g_signal_connect (sidebar_widget,
			  "annot_activated",
			  G_CALLBACK (sidebar_annots_annot_activated_cb),
			  ev_window);
	gtk_widget_show (sidebar_widget);
	ev_sidebar_add_page (EV_SIDEBAR (priv->sidebar),
			      sidebar_widget,
			      ANNOTS_SIDEBAR_ID, _("Annotations"),
			      ANNOTS_SIDEBAR_ICON);

	sidebar_widget = ev_sidebar_bookmarks_new ();
	priv->sidebar_bookmarks = sidebar_widget;
	gtk_widget_show (sidebar_widget);
	ev_sidebar_add_page (EV_SIDEBAR (priv->sidebar),
			      sidebar_widget,
			      BOOKMARKS_SIDEBAR_ID, _("Bookmarks"),
			      BOOKMARKS_SIDEBAR_ICON);

	sidebar_widget = ev_sidebar_attachments_new ();
	priv->sidebar_attachments = sidebar_widget;
	g_signal_connect_object (sidebar_widget,
				 "popup",
				 G_CALLBACK (attachment_bar_menu_popup_cb),
				 ev_window, 0);
	g_signal_connect_object (sidebar_widget,
				 "save-attachment",
				 G_CALLBACK (attachment_bar_save_attachment_cb),
				 ev_window, 0);
	gtk_widget_show (sidebar_widget);
	ev_sidebar_add_page (EV_SIDEBAR (priv->sidebar),
			      sidebar_widget,
			      ATTACHMENTS_SIDEBAR_ID, _("Attachments"),
			      ATTACHMENTS_SIDEBAR_ICON);

	sidebar_widget = ev_sidebar_layers_new ();
	priv->sidebar_layers = sidebar_widget;
	g_signal_connect (sidebar_widget,
			  "layers_visibility_changed",
			  G_CALLBACK (sidebar_layers_visibility_changed),
			  ev_window);
	gtk_widget_show (sidebar_widget);
	ev_sidebar_add_page (EV_SIDEBAR (priv->sidebar),
			      sidebar_widget,
			      LAYERS_SIDEBAR_ID, _("Layers"),
			      LAYERS_SIDEBAR_ICON);

	priv->view_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	overlay = gtk_overlay_new ();
	priv->scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (overlay), priv->scrolled_window);
	gtk_widget_show (priv->scrolled_window);

	priv->loading_message = ev_loading_message_new ();
	gtk_widget_set_name (priv->loading_message, "ev-loading-message");
	gtk_widget_set_halign (priv->loading_message, GTK_ALIGN_END);
	gtk_widget_set_valign (priv->loading_message, GTK_ALIGN_START);
	gtk_widget_set_no_show_all (priv->loading_message, TRUE);
	gtk_overlay_add_overlay (GTK_OVERLAY (overlay), priv->loading_message);

	gtk_box_pack_start (GTK_BOX (priv->view_box),
			    overlay,
			    TRUE, TRUE, 0);
	gtk_widget_show (overlay);

	gtk_paned_add2 (GTK_PANED (priv->hpaned),
			priv->view_box);
	gtk_widget_show (priv->view_box);

	priv->view = ev_view_new ();
	page_cache_mb = g_settings_get_uint (ev_window_ensure_settings (ev_window),
					     GS_PAGE_CACHE_SIZE);
	ev_view_set_page_cache_size (EV_VIEW (priv->view),
				     page_cache_mb * 1024 * 1024);
	allow_links_change_zoom = g_settings_get_boolean (ev_window_ensure_settings (ev_window),
				     GS_ALLOW_LINKS_CHANGE_ZOOM);
	ev_view_set_allow_links_change_zoom (EV_VIEW (priv->view),
				     allow_links_change_zoom);
	ev_view_set_model (EV_VIEW (priv->view), priv->model);

	priv->password_view = ev_password_view_new (GTK_WINDOW (ev_window));
	g_signal_connect_swapped (priv->password_view,
				  "unlock",
				  G_CALLBACK (ev_window_password_view_unlock),
				  ev_window);
	g_signal_connect_object (priv->view, "focus_in_event",
			         G_CALLBACK (view_actions_focus_in_cb),
				 ev_window, 0);
	g_signal_connect_swapped (priv->view, "external-link",
				  G_CALLBACK (view_external_link_cb),
				  ev_window);
	g_signal_connect_object (priv->view, "handle-link",
			         G_CALLBACK (view_handle_link_cb),
			         ev_window, 0);
	g_signal_connect_object (priv->view, "popup",
				 G_CALLBACK (view_menu_popup_cb),
				 ev_window, 0);
	g_signal_connect_object (priv->view, "selection-changed",
				 G_CALLBACK (view_selection_changed_cb),
				 ev_window, 0);
	g_signal_connect_object (priv->view, "annot-added",
				 G_CALLBACK (view_annot_added),
				 ev_window, 0);
	g_signal_connect_object (priv->view, "annot-removed",
				 G_CALLBACK (view_annot_removed),
				 ev_window, 0);
	g_signal_connect_object (priv->view, "layers-changed",
				 G_CALLBACK (view_layers_changed_cb),
				 ev_window, 0);
	g_signal_connect_object (priv->view, "notify::is-loading",
				 G_CALLBACK (view_is_loading_changed_cb),
				 ev_window, 0);
	g_signal_connect_object (priv->view, "cursor-moved",
				 G_CALLBACK (view_caret_cursor_moved_cb),
				 ev_window, 0);
#ifdef ENABLE_DBUS
	g_signal_connect_swapped (priv->view, "sync-source",
				  G_CALLBACK (ev_window_sync_source),
				  ev_window);
#endif
	gtk_widget_show (priv->view);
	gtk_widget_show (priv->password_view);

	/* Find results sidebar */
	priv->find_sidebar = ev_find_sidebar_new ();
	g_signal_connect (priv->find_sidebar,
			  "result-activated",
			  G_CALLBACK (find_sidebar_result_activated_cb),
			  ev_window);

	/* We own a ref on these widgets, as we can swap them in and out */
	g_object_ref (priv->view);
	g_object_ref (priv->password_view);

	gtk_container_add (GTK_CONTAINER (priv->scrolled_window),
			   priv->view);

	/* Connect to model signals */
	g_signal_connect_swapped (priv->model,
				  "page-changed",
				  G_CALLBACK (ev_window_page_changed_cb),
				  ev_window);
	g_signal_connect (priv->model,
			  "notify::document",
			  G_CALLBACK (ev_window_document_changed_cb),
			  ev_window);
	g_signal_connect (priv->model,
			  "notify::scale",
			  G_CALLBACK (ev_window_zoom_changed_cb),
			  ev_window);
	g_signal_connect (priv->model,
			  "notify::sizing-mode",
			  G_CALLBACK (ev_window_sizing_mode_changed_cb),
			  ev_window);
	g_signal_connect (priv->model,
			  "notify::rotation",
			  G_CALLBACK (ev_window_rotation_changed_cb),
			  ev_window);
	g_signal_connect (priv->model,
			  "notify::continuous",
			  G_CALLBACK (ev_window_continuous_changed_cb),
			  ev_window);
	g_signal_connect (priv->model,
			  "notify::dual-page",
			  G_CALLBACK (ev_window_dual_mode_changed_cb),
			  ev_window);
	g_signal_connect (priv->model,
			  "notify::dual-odd-left",
			  G_CALLBACK (ev_window_dual_mode_odd_pages_left_changed_cb),
			  ev_window);
	g_signal_connect (priv->model,
			  "notify::rtl",
			  G_CALLBACK (ev_window_direction_changed_cb),
			  ev_window);
	g_signal_connect (priv->model,
			  "notify::inverted-colors",
			  G_CALLBACK (ev_window_inverted_colors_changed_cb),
			  ev_window);

     	/* Connect sidebar signals */
	g_signal_connect (priv->sidebar,
			  "notify::visible",
			  G_CALLBACK (ev_window_sidebar_visibility_changed_cb),
			  ev_window);
	g_signal_connect (priv->sidebar,
			  "notify::current-page",
			  G_CALLBACK (ev_window_sidebar_current_page_changed_cb),
			  ev_window);

	/* Connect to find bar signals */
	g_signal_connect (priv->search_box,
			  "started",
			  G_CALLBACK (search_started_cb),
			  ev_window);
	g_signal_connect (priv->search_box,
			  "updated",
			  G_CALLBACK (search_updated_cb),
			  ev_window);
	g_signal_connect (priv->search_box,
			  "cleared",
			  G_CALLBACK (search_cleared_cb),
			  ev_window);
	g_signal_connect (priv->search_box,
			  "previous",
			  G_CALLBACK (search_previous_cb),
			  ev_window);
	g_signal_connect (priv->search_box,
			  "next",
			  G_CALLBACK (search_next_cb),
			  ev_window);
	g_signal_connect (search_entry,
			  "stop-search",
			  G_CALLBACK (search_entry_stop_search_cb),
			  ev_window);
	g_signal_connect (priv->search_bar,
			  "notify::search-mode-enabled",
			  G_CALLBACK (search_bar_search_mode_enabled_changed),
			  ev_window);

	/* Popups */
	builder = gtk_builder_new_from_resource ("/org/gnome/evince/gtk/menus.ui");
	priv->view_popup_menu = g_object_ref (G_MENU_MODEL (gtk_builder_get_object (builder, "view-popup-menu")));
	priv->attachment_popup_menu = g_object_ref (G_MENU_MODEL (gtk_builder_get_object (builder, "attachments-popup")));
	g_object_unref (builder);

	/* Media player keys */
	mpkeys = ev_application_get_media_keys (EV_APP);
	if (mpkeys) {
		g_signal_connect_swapped (mpkeys, "key_pressed",
					  G_CALLBACK (ev_window_media_player_key_pressed),
					  ev_window);
	}

	/* Give focus to the document view */
	gtk_widget_grab_focus (priv->view);

	priv->default_settings = g_settings_new (GS_SCHEMA_NAME".Default");
	g_settings_delay (priv->default_settings);
	ev_window_setup_default (ev_window);

	gtk_window_set_default_size (GTK_WINDOW (ev_window), 600, 600);

        ev_window_sizing_mode_changed_cb (priv->model, NULL, ev_window);
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
	EvWindowPrivate *priv = GET_PRIVATE (ev_window);

	return priv->dbus_object_path;
#else
	return NULL;
#endif
}

GMenuModel *
ev_window_get_bookmarks_menu (EvWindow *ev_window)
{
	EvWindowPrivate *priv;

	g_return_val_if_fail (EV_WINDOW (ev_window), NULL);

	priv = GET_PRIVATE (ev_window);

	return G_MENU_MODEL (priv->bookmarks_menu);
}

EvHistory *
ev_window_get_history (EvWindow *ev_window)
{
	EvWindowPrivate *priv;

	g_return_val_if_fail (EV_WINDOW (ev_window), NULL);

	priv = GET_PRIVATE (ev_window);

	return priv->history;
}

EvDocumentModel *
ev_window_get_document_model (EvWindow *ev_window)
{
	EvWindowPrivate *priv;

	g_return_val_if_fail (EV_WINDOW (ev_window), NULL);

	priv = GET_PRIVATE (ev_window);

	return priv->model;
}

GtkWidget *
ev_window_get_toolbar (EvWindow *ev_window)
{
	EvWindowPrivate *priv;

	g_return_val_if_fail (EV_WINDOW (ev_window), NULL);

	priv = GET_PRIVATE (ev_window);

	return priv->toolbar;
}

void
ev_window_focus_view (EvWindow *ev_window)
{
	EvWindowPrivate *priv;

	g_return_if_fail (EV_WINDOW (ev_window));

	priv = GET_PRIVATE (ev_window);

	gtk_widget_grab_focus (priv->view);
}

EvMetadata *
ev_window_get_metadata (EvWindow *ev_window)
{
	EvWindowPrivate *priv;

	g_return_val_if_fail (EV_WINDOW (ev_window), NULL);

	priv = GET_PRIVATE (ev_window);

	return priv->metadata;
}

gint
ev_window_get_metadata_sidebar_size (EvWindow *ev_window)
{
	EvWindowPrivate *priv;
	gint sidebar_size;

	g_return_val_if_fail (EV_WINDOW (ev_window), 0);

	priv = GET_PRIVATE (ev_window);

	if (!priv->metadata)
		return 0;

	if (ev_metadata_get_int (priv->metadata, "sidebar_size", &sidebar_size))
		return sidebar_size;

	return 0;
}
