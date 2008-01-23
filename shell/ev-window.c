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
#include "ev-window-title.h"
#include "ev-navigation-action.h"
#include "ev-page-action.h"
#include "ev-open-recent-action.h"
#include "ev-sidebar.h"
#include "ev-sidebar-links.h"
#include "ev-sidebar-attachments.h"
#include "ev-sidebar-thumbnails.h"
#include "ev-view.h"
#include "ev-password.h"
#include "ev-password-view.h"
#include "ev-properties-dialog.h"
#include "ev-file-exporter.h"
#include "ev-document-thumbnails.h"
#include "ev-document-links.h"
#include "ev-document-fonts.h"
#include "ev-document-find.h"
#include "ev-document-images.h"
#include "ev-document-security.h"
#include "ev-document-factory.h"
#include "ev-job-queue.h"
#include "ev-jobs.h"
#include "ev-sidebar-page.h"
#include "eggfindbar.h"
#include "egg-toolbar-editor.h"
#include "egg-editable-toolbar.h"
#include "egg-toolbars-model.h"
#include "ephy-zoom.h"
#include "ephy-zoom-action.h"
#include "ev-application.h"
#include "ev-stock-icons.h"
#include "ev-metadata-manager.h"
#include "ev-file-helpers.h"
#include "ev-utils.h"
#include "ev-history.h"
#include "ev-image.h"
#include "ev-message-area.h"

#ifdef WITH_GNOME_PRINT
#include "ev-print-job.h"
#include <libgnomeprintui/gnome-print-dialog.h>
#endif

#ifdef WITH_GTK_PRINT
#include <gtk/gtkprintunixdialog.h>
#endif

#ifdef ENABLE_PDF
#include <poppler.h>
#endif

#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <gconf/gconf-client.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if !GLIB_CHECK_VERSION (2, 13, 3)
char *xdg_user_dir_lookup (char *type);
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
	EV_CHROME_PREVIEW_TOOLBAR       = 1 << 6,
	EV_CHROME_NORMAL	= EV_CHROME_MENUBAR | EV_CHROME_TOOLBAR | EV_CHROME_SIDEBAR
} EvChrome;

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

	/* Dialogs */
	GtkWidget *properties;
#ifdef WITH_PRINT
	GtkWidget *print_dialog;
#endif
	GtkWidget *password_dialog;

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

	/* Preview mode */
	GtkWidget *preview_toolbar;
	gchar     *print_settings_file;

	/* Popup view */
	GtkWidget *view_popup;
	EvLink    *link;
	EvImage   *image;

	/* Popup attachment */
	GtkWidget    *attachment_popup;
	GList        *attach_list;

	/* Document */
	char *uri;
	char *local_uri;
	EvLinkDest *dest;
	gboolean unlink_temp_file;
	gboolean in_reload;
	
	EvDocument *document;
	EvHistory *history;
	EvPageCache *page_cache;
	EvWindowPageMode page_mode;
	EvWindowTitle *title;

	EvJob *load_job;
	EvJob *thumbnail_job;
	EvJob *save_job;
#ifdef WITH_GNOME_PRINT
	GnomePrintJob *print_job;
#endif

#ifdef WITH_GTK_PRINT
	EvJob            *print_job;
	gboolean          print_preview;
	GtkPrintJob      *gtk_print_job;
	GtkPrinter       *printer;
	GtkPrintSettings *print_settings;
	GtkPageSetup     *print_page_setup;
#endif
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

#ifdef WITH_GTK_PRINT
static const gchar *document_print_settings[] = {
	GTK_PRINT_SETTINGS_N_COPIES,
	GTK_PRINT_SETTINGS_COLLATE,
	GTK_PRINT_SETTINGS_REVERSE,
	GTK_PRINT_SETTINGS_NUMBER_UP,
	GTK_PRINT_SETTINGS_SCALE,
	GTK_PRINT_SETTINGS_PRINT_PAGES,
	GTK_PRINT_SETTINGS_PAGE_RANGES,
	GTK_PRINT_SETTINGS_PAGE_SET
};
#endif /* WITH_GTK_PRINT */

static void	ev_window_update_actions	 	(EvWindow         *ev_window);
static void     ev_window_sidebar_visibility_changed_cb (EvSidebar        *ev_sidebar,
							 GParamSpec       *pspec,
							 EvWindow         *ev_window);
static void     ev_window_set_page_mode                 (EvWindow         *window,
							 EvWindowPageMode  page_mode);
static void	ev_window_load_job_cb  			(EvJobLoad        *job,
							 gpointer          data);
static void     ev_window_set_icon_from_thumbnail       (EvJobThumbnail   *job,
							 EvWindow         *ev_window);
#ifdef WITH_GTK_PRINT
static void     ev_window_print_job_cb                  (EvJobPrint       *job,
							 EvWindow         *window);
#endif
static void     ev_window_save_job_cb                   (EvJobSave        *save,
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
static void     ev_window_stop_fullscreen               (EvWindow         *window);
static void     ev_window_cmd_view_fullscreen           (GtkAction        *action,
							 EvWindow         *window);
static void     ev_window_run_presentation              (EvWindow         *window);
static void     ev_window_stop_presentation             (EvWindow         *window);
static void     ev_window_cmd_view_presentation         (GtkAction        *action,
							 EvWindow         *window);
static void     ev_window_run_preview                   (EvWindow         *window);
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
static void     ev_window_cmd_edit_find                 (GtkAction        *action,
							 EvWindow         *ev_window);
static void     find_bar_search_changed_cb              (EggFindBar       *find_bar,
							 GParamSpec       *param,
							 EvWindow         *ev_window);

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
	gboolean override_restrictions = FALSE;
	gboolean can_get_text = FALSE;
	gboolean has_pages = FALSE;
	gboolean can_find = FALSE;

	GConfClient *client;

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

	client = gconf_client_get_default ();
	override_restrictions = gconf_client_get_bool (client, 
						       GCONF_OVERRIDE_RESTRICTIONS, 
						       NULL);
	if (!override_restrictions && info && info->fields_mask & EV_DOCUMENT_INFO_PERMISSIONS) {
		ok_to_print = (info->permissions & EV_DOCUMENT_PERMISSIONS_OK_TO_PRINT);
		ok_to_copy = (info->permissions & EV_DOCUMENT_PERMISSIONS_OK_TO_COPY);
	}

	if (has_document && !EV_IS_FILE_EXPORTER(document))
		ok_to_print = FALSE;

	
	if (gconf_client_get_bool (client, GCONF_LOCKDOWN_SAVE, NULL)) {
		ok_to_copy = FALSE;
	}

	if (gconf_client_get_bool (client, GCONF_LOCKDOWN_PRINT, NULL)) {
		ok_to_print = FALSE;
	}
#ifndef WITH_PRINT
	ok_to_print = FALSE;
#endif
	g_object_unref (client);


	/* File menu */
	ev_window_set_action_sensitive (ev_window, "FileOpenCopy", has_document);
	ev_window_set_action_sensitive (ev_window, "FileSaveAs", has_document && ok_to_copy);

#ifdef WITH_GTK_PRINT
	ev_window_set_action_sensitive (ev_window, "FilePrintSetup", has_pages && ok_to_print);
#endif

#ifdef WITH_GNOME_PRINT
	ev_window_set_action_sensitive (ev_window, "FilePrintSetup", FALSE);
#endif
	
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
	ev_window_set_action_sensitive (ev_window, "PreviewPrint", has_pages && ok_to_print);

        ev_window_update_actions (ev_window);
}

static void
ev_window_update_actions (EvWindow *ev_window)
{
	EvView *view = EV_VIEW (ev_window->priv->view);
	int n_pages = 0, page = -1;
	gboolean has_pages = FALSE;
	gboolean presentation_mode;

	if (ev_window->priv->document && ev_window->priv->page_cache) {
		page = ev_page_cache_get_current_page (ev_window->priv->page_cache);
		n_pages = ev_page_cache_get_n_pages (ev_window->priv->page_cache);
		has_pages = n_pages > 0;
	}

	ev_window_set_action_sensitive (ev_window, "EditCopy",
					has_pages &&
					ev_view_get_has_selection (view));
	ev_window_set_action_sensitive (ev_window, "EditFindNext",
					ev_view_can_find_next (view));
	ev_window_set_action_sensitive (ev_window, "EditFindPrevious",
					ev_view_can_find_previous (view));

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
	gboolean preview_toolbar;

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
	preview_toolbar = (priv->chrome& EV_CHROME_PREVIEW_TOOLBAR);

	set_widget_visibility (priv->menubar, menubar);	
	set_widget_visibility (priv->toolbar, toolbar);
	set_widget_visibility (priv->find_bar, findbar);
	set_widget_visibility (priv->sidebar, sidebar);
	
	if (priv->preview_toolbar)
		set_widget_visibility (priv->preview_toolbar, preview_toolbar);

	ev_window_set_action_sensitive (window, "EditToolbar", toolbar);
	gtk_widget_set_sensitive (priv->menubar, menubar);

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
ev_window_error_message_response_cb (EvMessageArea *area,
				     gint           response_id,
				     EvWindow      *window)
{
	ev_window_set_message_area (window, NULL);
}

static void
ev_window_error_message (GtkWindow *window, const gchar *msg, GError *error)
{
	GtkWidget *area;

	if (EV_WINDOW (window)->priv->message_area)
		return;

	area = ev_message_area_new (GTK_MESSAGE_ERROR,
				    msg,
				    GTK_STOCK_CLOSE,
				    GTK_RESPONSE_CANCEL,
				    NULL);
	if (error)
		ev_message_area_set_secondary_text (EV_MESSAGE_AREA (area), error->message);
	g_signal_connect (area, "response",
			  G_CALLBACK (ev_window_error_message_response_cb),
			  window);
	gtk_widget_show (area);
	ev_window_set_message_area (EV_WINDOW (window), area);
}

static void
find_changed_cb (EvDocument *document, int page, EvWindow *ev_window)
{
	ev_window_update_actions (ev_window);
}

static void
page_changed_cb (EvPageCache *page_cache,
		 gint         page,
		 EvWindow    *ev_window)
{
	ev_window_update_actions (ev_window);
	
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
setup_document_from_metadata (EvWindow *window)
{
	char *uri = window->priv->uri;
	GValue page = { 0, };
	gint n_pages;
	gint new_page;

	/* View the previously shown page, but make sure to not open a document on
	 * the last page, since closing it on the last page most likely means the
	 * user was finished reading the document. In that case, reopening should
	 * show the first page. */
	if (uri && ev_metadata_manager_get (uri, "page", &page, TRUE)) {
		n_pages = ev_page_cache_get_n_pages (window->priv->page_cache);
		new_page = CLAMP (g_value_get_int (&page), 0, n_pages - 1);
		if (!window->priv->in_reload && new_page == n_pages - 1)
			new_page = 0;
		ev_page_cache_set_current_page (window->priv->page_cache,
						new_page);
		g_value_unset (&page);
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
setup_sidebar_from_metadata (EvWindow *window, EvDocument *document)
{
	char *uri = window->priv->uri;
	GtkWidget *sidebar = window->priv->sidebar;
	GtkWidget *links = window->priv->sidebar_links;
	GtkWidget *thumbs = window->priv->sidebar_thumbs;
	GtkWidget *attachments = window->priv->sidebar_attachments;
	GValue sidebar_size = { 0, };
	GValue sidebar_page = { 0, };
	GValue sidebar_visibility = { 0, };

	if (ev_metadata_manager_get (uri, "sidebar_size", &sidebar_size, FALSE)) {
		gtk_paned_set_position (GTK_PANED (window->priv->hpaned),
					g_value_get_int (&sidebar_size));
		g_value_unset(&sidebar_size);
	}
	
	if (document && ev_metadata_manager_get (uri, "sidebar_page", &sidebar_page, FALSE)) {
		const char *page_id = g_value_get_string (&sidebar_page);
		
		if (strcmp (page_id, LINKS_SIDEBAR_ID) == 0 && ev_sidebar_page_support_document (EV_SIDEBAR_PAGE (links), document)) {
			ev_sidebar_set_page (EV_SIDEBAR (sidebar), links);
		} else if (strcmp (page_id, THUMBNAILS_SIDEBAR_ID) && ev_sidebar_page_support_document (EV_SIDEBAR_PAGE (thumbs), document)) {
			ev_sidebar_set_page (EV_SIDEBAR (sidebar), thumbs);
		} else if (strcmp (page_id, ATTACHMENTS_SIDEBAR_ID) && ev_sidebar_page_support_document (EV_SIDEBAR_PAGE (attachments), document)) {
			ev_sidebar_set_page (EV_SIDEBAR (sidebar), attachments);
		}
		g_value_unset (&sidebar_page);
	} else if (document && ev_sidebar_page_support_document (EV_SIDEBAR_PAGE (links), document)) {
		ev_sidebar_set_page (EV_SIDEBAR (sidebar), links);
	}

	if (ev_metadata_manager_get (uri, "sidebar_visibility", &sidebar_visibility, FALSE)) {
		update_chrome_flag (window, EV_CHROME_SIDEBAR, g_value_get_boolean (&sidebar_visibility));
		g_value_unset (&sidebar_visibility);
		update_chrome_visibility (window);
	}
}

static void
setup_size_from_metadata (EvWindow *window)
{
	char *uri = window->priv->uri;
	GValue width = { 0, };
	GValue height = { 0, };
	GValue width_ratio = { 0, };
	GValue height_ratio = { 0, };
	GValue maximized = { 0, };
	GValue x = { 0, };
	GValue y = { 0, };

	if (ev_metadata_manager_get (uri, "window_maximized", &maximized, FALSE)) {
		if (g_value_get_boolean (&maximized)) {
			gtk_window_maximize (GTK_WINDOW (window));
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
		return;
	}

        if (window->priv->page_cache &&
    	    ev_metadata_manager_get (uri, "window_width_ratio", &width_ratio, FALSE) &&
	    ev_metadata_manager_get (uri, "window_height_ratio", &height_ratio, FALSE)) {
		
		gint document_width;
		gint document_height;
		
		GdkScreen *screen;
		
		gint request_width;
		gint request_height;

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
			        
		gtk_window_resize (GTK_WINDOW (window),
				   request_width,
				   request_height);
	    	g_value_unset (&width_ratio);
		g_value_unset (&height_ratio);
	}
}

static void
setup_view_from_metadata (EvWindow *window)
{
	EvView *view = EV_VIEW (window->priv->view);
	char *uri = window->priv->uri;
	GEnumValue *enum_value;
	GValue sizing_mode = { 0, };
	GValue zoom = { 0, };
	GValue continuous = { 0, };
	GValue dual_page = { 0, };
	GValue presentation = { 0, };
	GValue fullscreen = { 0, };
	GValue rotation = { 0, };
	GValue maximized = { 0, };

	/* Maximized */
	if (ev_metadata_manager_get (uri, "window_maximized", &maximized, FALSE)) {
		if (g_value_get_boolean (&maximized)) {
			gtk_window_maximize (GTK_WINDOW (window));
		} else {
			gtk_window_unmaximize (GTK_WINDOW (window));
		}
		g_value_unset (&maximized);
	}

	/* Sizing mode */
	if (ev_metadata_manager_get (uri, "sizing_mode", &sizing_mode, FALSE)) {
		enum_value = g_enum_get_value_by_nick
			(EV_SIZING_MODE_CLASS, g_value_get_string (&sizing_mode));
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
		ev_job_queue_remove_job (ev_window->priv->thumbnail_job);

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
	
	EvRenderContext *rc;
	gint page_width, page_height;
	gdouble scale;
	EvDocument *document = ev_window->priv->document;
	
	if (!EV_IS_DOCUMENT_THUMBNAILS (document)) {
		return;
	}
	
	ev_window_clear_thumbnail_job (ev_window);
	
	ev_page_cache_get_size (ev_window->priv->page_cache,
				0, 0, 1.0,
				&page_width, &page_height);
	scale = (gdouble)128 / (gdouble)page_width;
	
	rc = ev_render_context_new (rotation, 0, scale);
	
	ev_window->priv->thumbnail_job = ev_job_thumbnail_new (document, rc);
	g_signal_connect (ev_window->priv->thumbnail_job, "finished",
			  G_CALLBACK (ev_window_set_icon_from_thumbnail),
			  ev_window);
	ev_job_queue_add_job (EV_JOB (ev_window->priv->thumbnail_job), EV_JOB_PRIORITY_LOW);
	g_object_unref (rc);
}

static gboolean
ev_window_setup_document (EvWindow *ev_window)
{
	const EvDocumentInfo *info;
	EvDocument *document = ev_window->priv->document;
	EvSidebar *sidebar = EV_SIDEBAR (ev_window->priv->sidebar);
	GtkAction *action;
	
	if (EV_IS_DOCUMENT_FIND (document)) {
		g_signal_connect_object (G_OBJECT (document),
				         "find_changed",
				         G_CALLBACK (find_changed_cb),	
				         ev_window, 0);
	}
	
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
					           ev_window->priv->document);
	}
	
	info = ev_page_cache_get_info (ev_window->priv->page_cache);
	update_document_mode (ev_window, info->mode);

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

	setup_size_from_metadata (ev_window);
	setup_sidebar_from_metadata (ev_window, document);
	setup_document_from_metadata (ev_window);

	if (ev_page_cache_get_n_pages (ev_window->priv->page_cache) > 0) {
		ev_view_set_document (view, document);
	}

	g_idle_add ((GSourceFunc)ev_window_setup_document, ev_window);
}

static void
password_dialog_response (GtkWidget *password_dialog,
			  gint       response_id,
			  EvWindow  *ev_window)
{
	char *password;
	
	if (response_id == GTK_RESPONSE_OK) {

		password = ev_password_dialog_get_password (EV_PASSWORD_DIALOG (password_dialog));
		if (password) {
			ev_document_doc_mutex_lock ();
			ev_document_security_set_password (EV_DOCUMENT_SECURITY (ev_window->priv->load_job->document),
							   password);
			ev_document_doc_mutex_unlock ();
		}
		g_free (password);

		ev_password_dialog_save_password (EV_PASSWORD_DIALOG (password_dialog));

		ev_window_title_set_type (ev_window->priv->title, EV_WINDOW_TITLE_DOCUMENT);
		ev_job_queue_add_job (ev_window->priv->load_job, EV_JOB_PRIORITY_HIGH);
		
		gtk_widget_destroy (password_dialog);
			
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
	g_assert (ev_window->priv->load_job);

	gtk_widget_set_sensitive (ev_window->priv->password_view, FALSE);

	ev_window_title_set_uri (ev_window->priv->title, ev_window->priv->uri);
	ev_window_title_set_type (ev_window->priv->title, EV_WINDOW_TITLE_PASSWORD);

	if (ev_window->priv->password_dialog == NULL) {
		ev_window->priv->password_dialog =
 			g_object_new (EV_TYPE_PASSWORD_DIALOG, "uri", ev_window->priv->uri, NULL);
		gtk_window_set_transient_for (GTK_WINDOW (ev_window->priv->password_dialog), GTK_WINDOW (ev_window));

		g_object_add_weak_pointer (G_OBJECT (ev_window->priv->password_dialog),
					   (gpointer) &(ev_window->priv->password_dialog));
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
ev_window_clear_load_job (EvWindow *ev_window)
{
	if (ev_window->priv->load_job != NULL) {
		
		if (!ev_window->priv->load_job->finished)
			ev_job_queue_remove_job (ev_window->priv->load_job);
		
		g_signal_handlers_disconnect_by_func (ev_window->priv->load_job, ev_window_load_job_cb, ev_window);
		g_object_unref (ev_window->priv->load_job);
		ev_window->priv->load_job = NULL;
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
ev_window_clear_print_settings_file (EvWindow *ev_window)
{
	if (ev_window->priv->print_settings_file) {
		g_unlink (ev_window->priv->print_settings_file);
		g_free (ev_window->priv->print_settings_file);
		ev_window->priv->print_settings_file = NULL;
	}
}

static void
ev_window_clear_temp_file (EvWindow *ev_window)
{
	GnomeVFSURI *uri;
	gchar       *filename;
	const gchar *tempdir;

	if (!ev_window->priv->uri)
		return;

	uri = gnome_vfs_uri_new (ev_window->priv->uri);
	if (!gnome_vfs_uri_is_local (uri)) {
		gnome_vfs_uri_unref (uri);
		return;
	}
	gnome_vfs_uri_unref (uri);

	filename = g_filename_from_uri (ev_window->priv->uri, NULL, NULL);
	if (!filename)
		return;

	tempdir = g_get_tmp_dir ();
	if (g_ascii_strncasecmp (filename, tempdir, strlen (tempdir)) == 0) {
		g_unlink (filename);
	}

	g_free (filename);
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

	g_assert (job->uri);
	
	ev_view_set_loading (EV_VIEW (ev_window->priv->view), FALSE);

	/* Success! */
	if (job->error == NULL) {
		ev_window_set_document (ev_window, document);
		
		if (job->mode != EV_WINDOW_MODE_PREVIEW) {
			setup_view_from_metadata (ev_window);
		}

		if (!ev_window->priv->unlink_temp_file) {
			ev_window_add_recent (ev_window, ev_window->priv->uri);
		}

		if (job->dest) {
			EvLink *link;
			EvLinkAction *link_action;
	
			link_action = ev_link_action_new_dest (g_object_ref (job->dest));
			link = ev_link_new (NULL, link_action);
			ev_view_handle_link (EV_VIEW (ev_window->priv->view), link);
		    	g_object_unref (link);
		}

		switch (job->mode) {
		        case EV_WINDOW_MODE_FULLSCREEN:
				ev_window_run_fullscreen (ev_window);
				break;
		        case EV_WINDOW_MODE_PRESENTATION:
				ev_window_run_presentation (ev_window);
				break;
		        case EV_WINDOW_MODE_PREVIEW:
				ev_window_run_preview (ev_window);
				break;
		        default:
				break;
		}

		/* Restart the search after reloading */
		if (ev_window->priv->in_reload) {
			GtkWidget *widget;
			
			widget = gtk_window_get_focus (GTK_WINDOW (ev_window));
			if (widget && gtk_widget_get_ancestor (widget, EGG_TYPE_FIND_BAR)) {
				find_bar_search_changed_cb (EGG_FIND_BAR (ev_window->priv->find_bar),
							    NULL, ev_window);
			}
		} else if (job->search_string && EV_IS_DOCUMENT_FIND (document)) {
			ev_window_cmd_edit_find (NULL, ev_window);
			egg_find_bar_set_search_string (EGG_FIND_BAR (ev_window->priv->find_bar),
							job->search_string);
		}

		ev_window_clear_load_job (ev_window);
		ev_window->priv->in_reload = FALSE;
		return;
	}

	if (job->error->domain == EV_DOCUMENT_ERROR &&
	    job->error->code == EV_DOCUMENT_ERROR_ENCRYPTED) {
		gchar *base_name, *file_name;

		setup_view_from_metadata (ev_window);

		file_name = gnome_vfs_format_uri_for_display (job->uri);
		base_name = g_path_get_basename (file_name);
		ev_password_view_set_file_name (EV_PASSWORD_VIEW (ev_window->priv->password_view),
						base_name);
		g_free (file_name);
		g_free (base_name);
		ev_window_set_page_mode (ev_window, PAGE_MODE_PASSWORD);
		
		ev_window_popup_password_dialog (ev_window);
	} else {
		ev_window_error_message (GTK_WINDOW (ev_window), 
					 _("Unable to open document"),
					 job->error);
		ev_window_clear_load_job (ev_window);
		ev_window->priv->in_reload = FALSE;
	}	

	return;
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
	if (ev_window->priv->password_dialog)
		gtk_widget_destroy (ev_window->priv->password_dialog);
	ev_window->priv->password_dialog = NULL;
	
#ifdef WITH_PRINT
	if (ev_window->priv->print_dialog)
		gtk_widget_destroy (ev_window->priv->print_dialog);
	ev_window->priv->print_dialog = NULL;
#endif

#ifdef WITH_GNOME_PRINT
	if (ev_window->priv->print_job)
		g_object_unref (ev_window->priv->print_job);
	ev_window->priv->print_job = NULL;
#endif
	
	if (ev_window->priv->properties)
		gtk_widget_destroy (ev_window->priv->properties);
	ev_window->priv->properties = NULL;
}

static gint
open_xfer_update_progress_callback (GnomeVFSAsyncHandle      *handle,
				    GnomeVFSXferProgressInfo *info,
				    EvWindow                 *ev_window)
{
	switch (info->status) {
	        case GNOME_VFS_XFER_PROGRESS_STATUS_OK:
			if (info->phase == GNOME_VFS_XFER_PHASE_COMPLETED) {
				ev_job_queue_add_job (ev_window->priv->load_job, EV_JOB_PRIORITY_HIGH);
			}

			return 1;
	        case GNOME_VFS_XFER_PROGRESS_STATUS_VFSERROR:
	        case GNOME_VFS_XFER_PROGRESS_STATUS_OVERWRITE:
	        case GNOME_VFS_XFER_PROGRESS_STATUS_DUPLICATE:
			return 1;
	        default:
			g_assert_not_reached ();
	}

	return 0;
}

void
ev_window_open_uri (EvWindow       *ev_window,
		    const char     *uri,
		    EvLinkDest     *dest,
		    EvWindowRunMode mode,
		    const gchar    *search_string, 
		    gboolean        unlink_temp_file,
		    const gchar    *print_settings)
{
	GnomeVFSURI *source_uri;
	GnomeVFSURI *target_uri;

	if (ev_window->priv->uri &&
	    g_ascii_strcasecmp (ev_window->priv->uri, uri) == 0) {
		ev_window->priv->in_reload = TRUE;
	}
	
	ev_window_close_dialogs (ev_window);
	ev_window_clear_load_job (ev_window);
	ev_window_clear_local_uri (ev_window);
	ev_window_clear_print_settings_file (ev_window);
	ev_view_set_loading (EV_VIEW (ev_window->priv->view), TRUE);

	ev_window->priv->unlink_temp_file = unlink_temp_file;

	if (mode == EV_WINDOW_MODE_PREVIEW) {
		ev_window->priv->print_settings_file = print_settings ? 
			g_strdup (print_settings) : NULL;
	}

	if (ev_window->priv->uri)
		g_free (ev_window->priv->uri);
	ev_window->priv->uri = g_strdup (uri);

	setup_size_from_metadata (ev_window);
	
	ev_window->priv->load_job = ev_job_load_new (uri, dest, mode, search_string);
	g_signal_connect (ev_window->priv->load_job,
			  "finished",
			  G_CALLBACK (ev_window_load_job_cb),
			  ev_window);

	source_uri = gnome_vfs_uri_new (uri);
	if (!gnome_vfs_uri_is_local (source_uri) && !ev_window->priv->local_uri) {
		GnomeVFSAsyncHandle *handle;
		GList               *slist = NULL;
		GList               *tlist = NULL;
		char                *tmp_name;
		char                *base_name;

		/* We'd like to keep extension of source uri since
		 * it helps to resolve some mime types, say cbz */

		tmp_name = ev_tmp_filename (NULL);
		base_name = gnome_vfs_uri_extract_short_name (source_uri);
		ev_window->priv->local_uri = g_strconcat ("file:", tmp_name, "-", base_name, NULL);
		ev_job_load_set_uri (EV_JOB_LOAD (ev_window->priv->load_job),
				     ev_window->priv->local_uri);
		g_free (base_name);
		g_free (tmp_name);
		
		target_uri = gnome_vfs_uri_new (ev_window->priv->local_uri);
		
		slist = g_list_prepend (slist, source_uri);
		tlist = g_list_prepend (tlist, target_uri);
		gnome_vfs_async_xfer (&handle, slist, tlist,
				      GNOME_VFS_XFER_DEFAULT | GNOME_VFS_XFER_FOLLOW_LINKS,
				      GNOME_VFS_XFER_ERROR_MODE_ABORT,
				      GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE,
				      GNOME_VFS_PRIORITY_DEFAULT,
				      (GnomeVFSAsyncXferProgressCallback)
				      open_xfer_update_progress_callback,
				      ev_window,
				      NULL, NULL); 
		
		g_list_free (slist);
		g_list_free (tlist);
		gnome_vfs_uri_unref (target_uri);
		gnome_vfs_uri_unref (source_uri);
		
		return;
	}

	gnome_vfs_uri_unref (source_uri);
	ev_job_queue_add_job (ev_window->priv->load_job, EV_JOB_PRIORITY_HIGH);
}

static void
file_open_dialog_response_cb (GtkWidget *chooser,
			      gint       response_id,
			      EvWindow  *ev_window)
{
	gchar *uri;

	if (response_id == GTK_RESPONSE_OK) {
		GSList *uris;

		uris = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (chooser));

		ev_application_open_uri_list (EV_APP, uris,
					      gtk_window_get_screen (GTK_WINDOW (ev_window)),
					      GDK_CURRENT_TIME);
	
		g_slist_foreach (uris, (GFunc)g_free, NULL);	
		g_slist_free (uris);
	}

	uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (chooser));
	ev_application_set_chooser_uri (EV_APP, uri);
	g_free (uri);

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

	ev_document_factory_add_filters (chooser, NULL);
	gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (chooser), TRUE);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), FALSE);
	if (ev_application_get_chooser_uri (EV_APP) != NULL) {
		gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (chooser),
					  ev_application_get_chooser_uri (EV_APP));
	} else if (window->priv->uri != NULL) {
		gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (chooser),
					  window->priv->uri);
	} else {
#if GLIB_CHECK_VERSION (2, 13, 3)
		const gchar *folder;

		folder = g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS);
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser),
						     folder ? folder : g_get_home_dir ());
#else
		char *folder;
		
		folder = xdg_user_dir_lookup ("DOCUMENTS");
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser),
						     folder);
		free (folder);
#endif
	}
	
	g_signal_connect (chooser, "response",
			  G_CALLBACK (file_open_dialog_response_cb),
			  window);

	gtk_widget_show (chooser);
}

static gchar *
ev_window_create_tmp_symlink (const gchar *filename, GError **error)
{
	gchar *tmp_filename = NULL;
	gchar *name;
	gint   res;
	guint  i = 0;

	name = g_path_get_basename (filename);
	
	do {
		gchar *basename;

		if (tmp_filename)
			g_free (tmp_filename);

		basename = g_strdup_printf ("%s-%d", name, i++);
		tmp_filename = g_build_filename (ev_tmp_dir (),
						 basename, NULL);
		
		g_free (basename);
	} while ((res = symlink (filename, tmp_filename)) != 0 && errno == EEXIST);

	g_free (name);
	
	if (res != 0 && errno != EEXIST) {
		if (error) {
			*error = g_error_new (G_FILE_ERROR,
					      g_file_error_from_errno (errno),
					      _("Couldn't create symlink “%s”: %s"),
					      tmp_filename, strerror (errno));
		}

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
		ev_window_error_message (GTK_WINDOW (window),
					 _("Cannot open a copy."),
					 error);

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
					 TRUE,
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
					 NULL, 0, NULL, FALSE, NULL, 
					 GDK_CURRENT_TIME);
}

static void
ev_window_open_recent_action_item_activated (EvOpenRecentAction *action,
					     const gchar        *uri,
					     EvWindow           *window)
{
	ev_application_open_uri_at_dest (EV_APP, uri,
					 gtk_window_get_screen (GTK_WINDOW (window)),
					 NULL, 0, NULL, FALSE, NULL, 
					 GDK_CURRENT_TIME);
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
		
		g_signal_connect (G_OBJECT (action), "activate",
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

static gint
save_xfer_update_progress_callback (GnomeVFSAsyncHandle      *handle,
				    GnomeVFSXferProgressInfo *info,
				    GnomeVFSURI              *tmp_uri)
{
	switch (info->status) {
	        case GNOME_VFS_XFER_PROGRESS_STATUS_OK:
			if (info->phase == GNOME_VFS_XFER_PHASE_COMPLETED) {
				gchar *uri;

				uri = gnome_vfs_uri_to_string (tmp_uri, 0);
				ev_tmp_uri_unlink (uri);
				g_free (uri);
				gnome_vfs_uri_unref (tmp_uri);
			}
			return 1;
	        case GNOME_VFS_XFER_PROGRESS_STATUS_VFSERROR:
			if (info->vfs_status != GNOME_VFS_OK) {
				GtkWidget *dialog;
				gchar     *uri;

				dialog = gtk_message_dialog_new (NULL,
								 GTK_DIALOG_DESTROY_WITH_PARENT,
								 GTK_MESSAGE_ERROR,
								 GTK_BUTTONS_CLOSE,
								 _("The file could not be saved as “%s”."),
								 info->target_name);
				gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
									  gnome_vfs_result_to_string (info->vfs_status));

				g_signal_connect (dialog, "response",
						  G_CALLBACK (gtk_widget_destroy),
						  NULL);
				gtk_widget_show (dialog);

				uri = gnome_vfs_uri_to_string (tmp_uri, 0);
				ev_tmp_uri_unlink (uri);
				g_free (uri);
				gnome_vfs_uri_unref (tmp_uri);
			}
			return 1;
	        case GNOME_VFS_XFER_PROGRESS_STATUS_OVERWRITE:
	        case GNOME_VFS_XFER_PROGRESS_STATUS_DUPLICATE:
			return 1;
	        default:
			g_assert_not_reached ();
	}

	return 0;
}

static void
ev_window_save_remote (EvWindow    *ev_window,
		       GnomeVFSURI *src,
		       GnomeVFSURI *dst)
{
	GnomeVFSAsyncHandle *handle;
	GList               *slist = NULL;
	GList               *tlist = NULL;
	
	slist = g_list_prepend (slist, src);
	tlist = g_list_prepend (tlist, dst);
	gnome_vfs_async_xfer (&handle, slist, tlist,
			      GNOME_VFS_XFER_DEFAULT | GNOME_VFS_XFER_FOLLOW_LINKS,
			      GNOME_VFS_XFER_ERROR_MODE_ABORT,
			      GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE,
			      GNOME_VFS_PRIORITY_DEFAULT,
			      (GnomeVFSAsyncXferProgressCallback)
			      save_xfer_update_progress_callback,
			      gnome_vfs_uri_ref (src),
			      NULL, NULL);
	g_list_free (slist);
	g_list_free (tlist);
}

static void
ev_window_clear_save_job (EvWindow *ev_window)
{
	if (ev_window->priv->save_job != NULL) {
		if (!ev_window->priv->save_job->finished)
			ev_job_queue_remove_job (ev_window->priv->save_job);
		
		g_signal_handlers_disconnect_by_func (ev_window->priv->save_job,
						      ev_window_save_job_cb,
						      ev_window);
		g_object_unref (ev_window->priv->save_job);
		ev_window->priv->save_job = NULL;
	}
}

static void
ev_window_save_job_cb (EvJobSave *job,
		       EvWindow  *window)
{
	if (job->error) {
		gchar *msg;
		
		msg = g_strdup_printf (_("The file could not be saved as “%s”."), job->uri);
		ev_window_error_message (GTK_WINDOW (window), msg, job->error);
		g_free (msg);
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
	
	uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (fc));
	
	ev_window_clear_save_job (ev_window);
	ev_window->priv->save_job = ev_job_save_new (ev_window->priv->document,
						     uri, ev_window->priv->uri);
	g_signal_connect (ev_window->priv->save_job, "finished",
			  G_CALLBACK (ev_window_save_job_cb),
			  ev_window);
	/* The priority doesn't matter for this job */
	ev_job_queue_add_job (ev_window->priv->save_job, EV_JOB_PRIORITY_LOW);

	g_free (uri);

	gtk_widget_destroy (fc);
}

static void
ev_window_cmd_save_as (GtkAction *action, EvWindow *ev_window)
{
	GtkWidget *fc;
	gchar *base_name;
	gchar *file_name;
#if GLIB_CHECK_VERSION (2, 13, 3)
	const gchar *folder;
#else
	gchar *folder;
#endif

	fc = gtk_file_chooser_dialog_new (
		_("Save a Copy"),
		GTK_WINDOW (ev_window), GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_OK,
		NULL);

	ev_document_factory_add_filters (fc, ev_window->priv->document);
	gtk_dialog_set_default_response (GTK_DIALOG (fc), GTK_RESPONSE_OK);

	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (fc), FALSE);
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (fc), TRUE);	
	file_name = gnome_vfs_format_uri_for_display (ev_window->priv->uri);
	base_name = g_path_get_basename (file_name);
	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (fc), base_name);
	
#if GLIB_CHECK_VERSION (2, 13, 3)
	folder = g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (fc),
					     folder ? folder : g_get_home_dir ());
#else
	folder = xdg_user_dir_lookup ("DOCUMENTS");
        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (fc), folder);
	free (folder);
#endif
	
	g_free (file_name);
	g_free (base_name);
        
	g_signal_connect (fc, "response",
			  G_CALLBACK (file_save_dialog_response_cb),
			  ev_window);

	gtk_widget_show (fc);
}

#ifdef WITH_GTK_PRINT
static void
ev_window_print_page_setup_done_cb (GtkPageSetup *page_setup,
				    EvWindow     *window)
{
	/* Dialog was canceled */
	if (!page_setup)
		return;

	if (window->priv->print_page_setup)
		g_object_unref (window->priv->print_page_setup);
	window->priv->print_page_setup = g_object_ref (page_setup);
}
#endif /* WITH_GTK_PRINT */

static void
ev_window_cmd_file_print_setup (GtkAction *action, EvWindow *ev_window)
{
#ifdef WITH_GTK_PRINT
	gtk_print_run_page_setup_dialog_async (
		GTK_WINDOW (ev_window),
		ev_window->priv->print_page_setup,
		ev_window->priv->print_settings,
		(GtkPageSetupDoneFunc) ev_window_print_page_setup_done_cb,
		ev_window);
#endif /* WITH_GTK_PRINT */
}

#ifdef WITH_GTK_PRINT
static void
ev_window_clear_print_job (EvWindow *window)
{
	if (window->priv->print_job) {
		if (!window->priv->print_job->finished)
			ev_job_queue_remove_job (window->priv->print_job);

		g_signal_handlers_disconnect_by_func (window->priv->print_job,
						      ev_window_print_job_cb,
						      window);
		g_object_unref (window->priv->print_job);
		window->priv->print_job = NULL;
	}

	if (window->priv->gtk_print_job) {
		g_object_unref (window->priv->gtk_print_job);
		window->priv->gtk_print_job = NULL;
	}
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
ev_window_print_finished (GtkPrintJob *print_job,
			  EvWindow    *window,
			  GError      *error)
{
	ev_window_clear_print_job (window);
	
	if (error) {
		GtkWidget *dialog;
		
		dialog = gtk_message_dialog_new (GTK_WINDOW (window),
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK,
						 _("Failed to print document"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  error->message);

		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	} else {
		/* If printed successfully, save print settings */
		ev_application_set_print_settings (EV_APP,
						   window->priv->print_settings);
		ev_window_save_print_settings (window);
	}
}

static void
ev_window_print_send (EvWindow    *window,
		      const gchar *filename)
{
	GtkPrintSettings *settings;
	EvFileExporterCapabilities capabilities;
	
	/* Some printers take into account some print settings,
	 * and others don't. However we have exported the document
	 * to a ps or pdf file according to such print settings. So,
	 * we want to send the exported file to printer with those
	 * settings set to default values. 
	 */
	settings = gtk_print_settings_copy (window->priv->print_settings);
	capabilities = ev_file_exporter_get_capabilities (EV_FILE_EXPORTER (window->priv->document));

	gtk_print_settings_set_page_ranges (settings, NULL, 0);
	gtk_print_settings_set_print_pages (settings, GTK_PRINT_PAGES_ALL);
	if (capabilities & EV_FILE_EXPORTER_CAN_COPIES)
		gtk_print_settings_set_n_copies (settings, 1);
	if (capabilities & EV_FILE_EXPORTER_CAN_PAGE_SET)
		gtk_print_settings_set_page_set (settings, GTK_PAGE_SET_ALL);
	if (capabilities & EV_FILE_EXPORTER_CAN_SCALE)
		gtk_print_settings_set_scale (settings, 1.0);
	if (capabilities & EV_FILE_EXPORTER_CAN_COLLATE)
		gtk_print_settings_set_collate (settings, FALSE);
	if (capabilities & EV_FILE_EXPORTER_CAN_REVERSE)
		gtk_print_settings_set_reverse (settings, FALSE);
	if (capabilities & EV_FILE_EXPORTER_CAN_NUMBER_UP) {
		gtk_print_settings_set_number_up (settings, 1);
		gtk_print_settings_set_int (settings, "cups-"GTK_PRINT_SETTINGS_NUMBER_UP, 1);
	}
	
	if (window->priv->print_preview) {
		gchar *uri;
		gchar *print_settings_file = NULL;

		ev_application_set_print_settings (EV_APP,
						   window->priv->print_settings);
		
#if GTK_CHECK_VERSION (2, 11, 0)
		print_settings_file = ev_tmp_filename ("print-settings");
		gtk_print_settings_to_file (settings, print_settings_file, NULL);
#endif
		uri = g_filename_to_uri (filename, NULL, NULL);
		ev_application_open_uri_at_dest (EV_APP,
						 uri, 
						 gtk_window_get_screen (GTK_WINDOW (window)),
						 NULL,
						 EV_WINDOW_MODE_PREVIEW,
						 NULL, 
						 TRUE,
						 print_settings_file,
						 GDK_CURRENT_TIME);
		g_free (print_settings_file);
		g_free (uri);
	} else {
		GtkPrintJob *job;
		GError      *error = NULL;
	
		job = gtk_print_job_new ("evince-print",
					 window->priv->printer,
					 settings,
					 window->priv->print_page_setup);

		if (window->priv->gtk_print_job)
			g_object_unref (window->priv->gtk_print_job);
		window->priv->gtk_print_job = job;

		if (gtk_print_job_set_source_file (job, filename, &error)) {
			gtk_print_job_send (job,
					    (GtkPrintJobCompleteFunc)ev_window_print_finished,
					    window, NULL);
		} else {
			ev_window_clear_print_job (window);
			g_warning (error->message);
			g_error_free (error);
		}
	}

	g_object_unref (settings);
}

static void
ev_window_print_job_cb (EvJobPrint *job,
			EvWindow   *window)
{
	if (job->error) {
		g_warning (job->error->message);
		ev_window_clear_print_job (window);
		return;
	}

	g_assert (job->temp_file != NULL);

	ev_window_print_send (window, job->temp_file);
}

static gboolean
ev_window_print_dialog_response_cb (GtkDialog *dialog,
				    gint       response,
				    EvWindow  *window)
{
	EvPrintRange  *ranges = NULL;
	EvPrintPageSet page_set;
	gint           n_ranges = 0;
	gint           copies;
	gint           pages_per_sheet;
	gboolean       collate;
	gboolean       reverse;
	gdouble        scale;
	gint           current_page;
	gdouble        width;
	gdouble        height;
	GtkPrintPages  print_pages;
	const gchar   *file_format;
	
	if (response == GTK_RESPONSE_CANCEL) {
		gtk_widget_destroy (GTK_WIDGET (dialog));
		window->priv->print_dialog = NULL;

		return FALSE;
	}

	window->priv->print_preview = (response == GTK_RESPONSE_APPLY);

	if (window->priv->printer)
		g_object_unref (window->priv->printer);
	if (window->priv->print_settings)
		g_object_unref (window->priv->print_settings);
	if (window->priv->print_page_setup)
		g_object_unref (window->priv->print_page_setup);
	
	window->priv->printer = g_object_ref (
		gtk_print_unix_dialog_get_selected_printer (GTK_PRINT_UNIX_DIALOG (dialog)));
	window->priv->print_settings = g_object_ref (
		gtk_print_unix_dialog_get_settings (GTK_PRINT_UNIX_DIALOG (dialog)));
	window->priv->print_page_setup = g_object_ref (
		gtk_print_unix_dialog_get_page_setup (GTK_PRINT_UNIX_DIALOG (dialog)));

	file_format = gtk_print_settings_get (window->priv->print_settings,
					      GTK_PRINT_SETTINGS_OUTPUT_FILE_FORMAT);
	
	if (!gtk_printer_accepts_ps (window->priv->printer)) {
		GtkWidget *msgdialog;

		msgdialog = gtk_message_dialog_new (GTK_WINDOW (dialog),
						    GTK_DIALOG_MODAL,
						    GTK_MESSAGE_ERROR,
						    GTK_BUTTONS_OK,
						    _("Printing is not supported on this printer."));
		
		gtk_dialog_run (GTK_DIALOG (msgdialog));
		gtk_widget_destroy (msgdialog);

		return FALSE;
	}

	ev_window_clear_print_job (window);
	
	current_page = gtk_print_unix_dialog_get_current_page (GTK_PRINT_UNIX_DIALOG (dialog));
	print_pages = gtk_print_settings_get_print_pages (window->priv->print_settings);
	
	switch (print_pages) {
	case GTK_PRINT_PAGES_CURRENT:
		ranges = g_new0 (EvPrintRange, 1);
		
		ranges->start = current_page;
		ranges->end = current_page;
		n_ranges = 1;
				
		break;
	case GTK_PRINT_PAGES_RANGES: {
		GtkPageRange *page_range;
		
		page_range = gtk_print_settings_get_page_ranges (window->priv->print_settings,
								 &n_ranges);
		if (n_ranges > 0)
			ranges = g_memdup (page_range, n_ranges * sizeof (GtkPageRange));
	}
		break;
	case GTK_PRINT_PAGES_ALL: {
		gint n_pages;

		n_pages = ev_page_cache_get_n_pages (ev_page_cache_get (window->priv->document));
		
		ranges = g_new0 (EvPrintRange, 1);

		ranges->start = 0;
		ranges->end = n_pages - 1;
		n_ranges = 1;
	}
		break;
	}

	page_set = (EvPrintPageSet)gtk_print_settings_get_page_set (window->priv->print_settings);

	scale = gtk_print_settings_get_scale (window->priv->print_settings) * 0.01;
	
	width = gtk_page_setup_get_paper_width (window->priv->print_page_setup,
						GTK_UNIT_POINTS);
	height = gtk_page_setup_get_paper_height (window->priv->print_page_setup,
						  GTK_UNIT_POINTS);
	
	if (scale != 1.0) {
		width *= scale;
		height *= scale;
	}

	pages_per_sheet = gtk_print_settings_get_number_up (window->priv->print_settings);
	
	copies = gtk_print_settings_get_n_copies (window->priv->print_settings);
	collate = gtk_print_settings_get_collate (window->priv->print_settings);
	reverse = gtk_print_settings_get_reverse (window->priv->print_settings);
	
	window->priv->print_job = ev_job_print_new (window->priv->document,
						    file_format ? file_format : "ps",
						    width, height,
						    ranges, n_ranges,
						    page_set,
						    pages_per_sheet,
						    copies, collate,
						    reverse);
	
	g_signal_connect (window->priv->print_job, "finished",
			  G_CALLBACK (ev_window_print_job_cb),
			  window);
	/* The priority doesn't matter for this job */
	ev_job_queue_add_job (window->priv->print_job, EV_JOB_PRIORITY_LOW);
	
	gtk_widget_destroy (GTK_WIDGET (dialog));
	window->priv->print_dialog = NULL;

	return TRUE;
}

void
ev_window_print_range (EvWindow *ev_window, int first_page, int last_page)
{
	GtkWidget           *dialog;
	EvPageCache         *page_cache;
	gint                 current_page;
	gint                 document_last_page;
	GtkPrintCapabilities capabilities;

	g_return_if_fail (EV_IS_WINDOW (ev_window));
	g_return_if_fail (ev_window->priv->document != NULL);

	if (ev_window->priv->print_dialog) {
		gtk_window_present (GTK_WINDOW (ev_window->priv->print_dialog));
		return;
	}
	
	page_cache = ev_page_cache_get (ev_window->priv->document);
	current_page = ev_page_cache_get_current_page (page_cache);
	document_last_page = ev_page_cache_get_n_pages (page_cache);

	if (!ev_window->priv->print_settings) {
		ev_window->priv->print_settings = gtk_print_settings_copy (
			ev_application_get_print_settings (EV_APP));
		ev_window_load_print_settings_from_metadata (ev_window);
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

	dialog = gtk_print_unix_dialog_new (_("Print"), GTK_WINDOW (ev_window));
	ev_window->priv->print_dialog = dialog;
	
	capabilities = GTK_PRINT_CAPABILITY_PREVIEW |
		ev_file_exporter_get_capabilities (EV_FILE_EXPORTER (ev_window->priv->document));
	gtk_print_unix_dialog_set_manual_capabilities (GTK_PRINT_UNIX_DIALOG (dialog),
						       capabilities);

	gtk_print_unix_dialog_set_current_page (GTK_PRINT_UNIX_DIALOG (dialog),
						current_page);
	
	gtk_print_unix_dialog_set_settings (GTK_PRINT_UNIX_DIALOG (dialog),
					    ev_window->priv->print_settings);
	
	if (ev_window->priv->print_page_setup)
		gtk_print_unix_dialog_set_page_setup (GTK_PRINT_UNIX_DIALOG (dialog),
						      ev_window->priv->print_page_setup);
	
	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (ev_window_print_dialog_response_cb),
			  ev_window);

	gtk_widget_show (dialog);
}
#endif /* WITH_GTK_PRINT */

#ifdef WITH_GNOME_PRINT
static gboolean
ev_window_print_dialog_response_cb (GtkDialog *print_dialog,
				    gint       response,
				    EvWindow  *ev_window)
{
	EvPrintJob *print_job;
	GnomePrintConfig *config;
    
	if (response != GNOME_PRINT_DIALOG_RESPONSE_PRINT) {
		gtk_widget_destroy (GTK_WIDGET (print_dialog));
		ev_window->priv->print_dialog = NULL;
		g_object_unref (ev_window->priv->print_job);
		ev_window->priv->print_job = NULL;
		
		return FALSE;
	}

	config = gnome_print_dialog_get_config (GNOME_PRINT_DIALOG (print_dialog));

	/* FIXME: Change this when we have the first backend
	 * that can print more than postscript
	 */
	if (using_pdf_printer (config)) {
		GtkWidget *dialog;
		
		dialog = gtk_message_dialog_new (GTK_WINDOW (print_dialog), GTK_DIALOG_MODAL,
						 GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
						 _("Generating PDF is not supported"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		
		return FALSE;
	} else if (!using_postscript_printer (config)) {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (GTK_WINDOW (print_dialog), GTK_DIALOG_MODAL,
						 GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
						 _("Printing is not supported on this printer."));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  _("You were trying to print to a printer using the “%s” driver. "
							    "This program requires a PostScript printer driver."),
							  gnome_print_config_get (config, (guchar *)"Settings.Engine.Backend.Driver"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		
		return FALSE;
	}

	save_print_config_to_file (config);
    
	print_job = g_object_new (EV_TYPE_PRINT_JOB,
				  "gnome_print_job", ev_window->priv->print_job,
				  "document", ev_window->priv->document,
				  "print_dialog", print_dialog,
				  NULL);

	if (print_job != NULL) {
		ev_print_job_print (print_job, GTK_WINDOW (ev_window));
		g_object_unref (print_job);
	}

	g_object_unref (config);

	gtk_widget_destroy (GTK_WIDGET (print_dialog));
	ev_window->priv->print_dialog = NULL;
	g_object_unref (ev_window->priv->print_job);
	ev_window->priv->print_job = NULL;

	return FALSE;
}

void
ev_window_print_range (EvWindow *ev_window, int first_page, int last_page)
{
	GnomePrintConfig *config;
	gchar *pages_label;

        g_return_if_fail (EV_IS_WINDOW (ev_window));
	g_return_if_fail (ev_window->priv->document != NULL);

	config = load_print_config_from_file ();

	if (ev_window->priv->print_job == NULL)
		ev_window->priv->print_job = gnome_print_job_new (config);
	
	if (ev_window->priv->print_dialog == NULL) {
		ev_window->priv->print_dialog =
			gnome_print_dialog_new (ev_window->priv->print_job,
						(guchar *) _("Print"),
						(GNOME_PRINT_DIALOG_RANGE |
						 GNOME_PRINT_DIALOG_COPIES));
	}
	
	gtk_window_set_transient_for (GTK_WINDOW (ev_window->priv->print_dialog),
				      GTK_WINDOW (ev_window));								
	g_object_unref (config);								

	pages_label = g_strconcat (_("Pages"), " ", NULL);
	gnome_print_dialog_construct_range_page (GNOME_PRINT_DIALOG (ev_window->priv->print_dialog),
						 GNOME_PRINT_RANGE_ALL |
						 GNOME_PRINT_RANGE_RANGE,
						 first_page, last_page,
						 NULL, (const guchar *)pages_label);
	g_free (pages_label);
						 
	gtk_dialog_set_response_sensitive (GTK_DIALOG (ev_window->priv->print_dialog),
					   GNOME_PRINT_DIALOG_RESPONSE_PREVIEW,
					   FALSE);

	g_signal_connect (G_OBJECT (ev_window->priv->print_dialog), "response",
			  G_CALLBACK (ev_window_print_dialog_response_cb),
			  ev_window);
	gtk_widget_show (ev_window->priv->print_dialog);
}
#endif /* WITH_GNOME_PRINT */

static void
ev_window_print (EvWindow *window)
{
	EvPageCache *page_cache;
	gint         last_page;

	page_cache = ev_page_cache_get (window->priv->document);
	last_page = ev_page_cache_get_n_pages (page_cache);

#ifdef WITH_PRINT
	ev_window_print_range (window, 1, last_page);
#endif
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
					   (gpointer) &(ev_window->priv->properties));
		gtk_window_set_transient_for (GTK_WINDOW (ev_window->priv->properties),
					      GTK_WINDOW (ev_window));
	}

	ev_document_fc_mutex_lock ();
	gtk_widget_show (ev_window->priv->properties);
	ev_document_fc_mutex_unlock ();
}
					
static void
ev_window_cmd_file_close_window (GtkAction *action, EvWindow *ev_window)
{
	g_return_if_fail (EV_IS_WINDOW (ev_window));

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
	ev_view_scroll (EV_VIEW (window->priv->view), EV_SCROLL_PAGE_FORWARD, FALSE);
}

static void
ev_window_cmd_scroll_backward (GtkAction *action, EvWindow *window)
{
	ev_view_scroll (EV_VIEW (window->priv->view), EV_SCROLL_PAGE_BACKWARD, FALSE);
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
	ev_window_update_actions (ev_window);
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
	ev_window_update_actions (ev_window);
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
	ev_window_update_actions (ev_window);
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

	g_object_set (G_OBJECT (window->priv->scrolled_window),
		      "shadow-type", GTK_SHADOW_NONE,
		      NULL);

	ev_view_set_fullscreen (EV_VIEW (window->priv->view), TRUE);
	ev_window_update_fullscreen_action (window);

	/* If the user doesn't have the main toolbar he/she won't probably want
	 * the toolbar in fullscreen mode. See bug #483048
	 */
	update_chrome_flag (window, EV_CHROME_FULLSCREEN_TOOLBAR,
			    (window->priv->chrome & EV_CHROME_TOOLBAR) != 0);
	update_chrome_visibility (window);
	
	gtk_window_fullscreen (GTK_WINDOW (window));
	gtk_widget_grab_focus (window->priv->view);

	if (!ev_window_is_empty (window))
		ev_metadata_manager_set_boolean (window->priv->uri, "fullscreen", TRUE);
}

static void
ev_window_stop_fullscreen (EvWindow *window)
{
	EvView *view = EV_VIEW (window->priv->view);

	if (!ev_view_get_fullscreen (EV_VIEW (view)))
		return;

	g_object_set (G_OBJECT (window->priv->scrolled_window),
		      "shadow-type", GTK_SHADOW_IN,
		      NULL);

	ev_view_set_fullscreen (view, FALSE);
	ev_window_update_fullscreen_action (window);
	update_chrome_flag (window, EV_CHROME_FULLSCREEN_TOOLBAR, FALSE);
	update_chrome_visibility (window);
	gtk_window_unfullscreen (GTK_WINDOW (window));

	if (!ev_window_is_empty (window))
		ev_metadata_manager_set_boolean (window->priv->uri, "fullscreen", FALSE);
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

#if GLIB_CHECK_VERSION (2, 13, 0)
	window->priv->presentation_timeout_id =
		g_timeout_add_seconds (PRESENTATION_TIMEOUT,
				       (GSourceFunc)presentation_timeout_cb, window);
#else
	window->priv->presentation_timeout_id = 
	    g_timeout_add (PRESENTATION_TIMEOUT * 1000,
			   (GSourceFunc)presentation_timeout_cb, window);
#endif	

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
	g_object_set (G_OBJECT (window->priv->scrolled_window),
		      "shadow-type", GTK_SHADOW_NONE,
		      NULL);

	ev_view_set_presentation (EV_VIEW (window->priv->view), TRUE);
	ev_window_update_presentation_action (window);

	update_chrome_visibility (window);
	
	gtk_widget_grab_focus (window->priv->view);
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
ev_window_stop_presentation (EvWindow *window)
{
	if (!ev_view_get_presentation (EV_VIEW (window->priv->view)))
		return;

	g_object_set (G_OBJECT (window->priv->scrolled_window),
		      "shadow-type", GTK_SHADOW_IN,
		      NULL);

	ev_view_set_presentation (EV_VIEW (window->priv->view), FALSE);
	ev_window_update_presentation_action (window);
	update_chrome_visibility (window);
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

	g_return_if_fail (EV_IS_WINDOW (window));
	ev_window_stop_fullscreen (window);

	presentation = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	if (presentation) {
		ev_window_run_presentation (window);
	} else {
		ev_window_stop_presentation (window);
	}
}

static void
ev_window_run_preview (EvWindow *window)
{
	GtkAction *action;
	
	if (!window->priv->preview_toolbar) {
		window->priv->preview_toolbar =
			gtk_ui_manager_get_widget (window->priv->ui_manager,
						   "/PreviewToolbar");

		gtk_box_pack_start (GTK_BOX (window->priv->main_box),
				    window->priv->preview_toolbar,
				    FALSE, FALSE, 0);
		gtk_box_reorder_child (GTK_BOX (window->priv->main_box),
				       window->priv->preview_toolbar, 1);
	}
	
	ev_view_set_continuous (EV_VIEW (window->priv->view), FALSE); 
	
	update_chrome_flag (window, EV_CHROME_TOOLBAR, FALSE);
	update_chrome_flag (window, EV_CHROME_MENUBAR, FALSE);
	update_chrome_flag (window, EV_CHROME_SIDEBAR, FALSE);

	update_chrome_flag (window, EV_CHROME_PREVIEW_TOOLBAR, TRUE);
	
	action = gtk_action_group_get_action (window->priv->action_group,
					      "PreviewPrint");
#if GTK_CHECK_VERSION (2, 11, 0)
	gtk_action_set_visible (action, TRUE);
#else
	gtk_action_set_visible (action, FALSE);
#endif

	update_chrome_visibility (window);
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

	ev_view_set_screen_dpi (EV_VIEW (priv->view),
				get_screen_dpi (GTK_WINDOW (window)));
	
	if (GTK_WIDGET_CLASS (ev_window_parent_class)->screen_changed) {
		GTK_WIDGET_CLASS (ev_window_parent_class)->screen_changed (widget, old_screen);
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
	gchar *uri;

	uri = g_strdup (ev_window->priv->uri);
	ev_window_open_uri (ev_window, uri, NULL, 0, NULL, FALSE, NULL);
	g_free (uri);
}

static void
ev_window_cmd_view_autoscroll (GtkAction *action, EvWindow *ev_window)
{
	ev_view_autoscroll (EV_VIEW (ev_window->priv->view));
}

static void
ev_window_cmd_help_contents (GtkAction *action, EvWindow *ev_window)
{
	GError *error = NULL;
	GdkScreen *screen;
	char *command;
	const char *lang;
	char *uri = NULL;

	int i;

	const char * const * langs = g_get_language_names ();

	for (i = 0; langs[i]; i++) {
		lang = langs[i];
		if (strchr (lang, '.')) {
			continue;
		}

		uri = g_build_filename(GNOMEDATADIR,
				       "/gnome/help/" PACKAGE,
				       lang,
				       "/evince.xml",
				       NULL);
					
		if (g_file_test (uri, G_FILE_TEST_EXISTS)) {
			break;
		}
		g_free (uri);
		uri = NULL;
	}

	if (uri == NULL) {
		g_warning ("Cannot find help");
		return;
	}
	
	command = g_strconcat ("gnome-help ghelp://", uri,  NULL);
	g_free (uri);
	
	screen = gtk_widget_get_screen (GTK_WIDGET (ev_window));
	gdk_spawn_command_line_on_screen (screen, command, &error);
	if (error != NULL) {
		g_warning (error->message);
		g_error_free (error);
	}
	g_free (command);
}

static void
ev_window_cmd_leave_fullscreen (GtkAction *action, EvWindow *window)
{
	ev_window_stop_fullscreen (window);
}

static void
ev_window_cmd_start_presentation (GtkAction *action, EvWindow *window)
{
	ev_window_stop_fullscreen (window);
	ev_window_run_presentation (window);
}

#ifdef WITH_GTK_PRINT
static void ev_window_do_preview_print (EvWindow *window);

static gboolean
ev_window_enumerate_printer_cb (GtkPrinter *printer,
				EvWindow   *window)
{
	EvWindowPrivate *priv = window->priv;
	const gchar *printer_name;

	printer_name = gtk_print_settings_get_printer (priv->print_settings);
	if ((printer_name
	     && strcmp (printer_name, gtk_printer_get_name (printer)) == 0) ||
	    (!printer_name && gtk_printer_is_default (printer))) {
		if (priv->printer)
			g_object_unref (priv->printer);
		priv->printer = g_object_ref (printer);

		/* Now that we have the printer, we'll start the print */
		ev_window_do_preview_print (window);

		return TRUE; /* we're done */
	}

	return FALSE; /* continue the enumeration */
}

static void
ev_window_preview_print_finished (GtkPrintJob *print_job,
				  EvWindow    *window,
				  GError      *error)
{
	if (error) {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (GTK_WINDOW (window),
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK,
						 _("Failed to print document"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  error->message);

		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}

	g_object_unref (print_job);
	gtk_widget_destroy (GTK_WIDGET (window));
}

static void
ev_window_do_preview_print (EvWindow *window)
{
	EvWindowPrivate  *priv = window->priv;
	GtkPageSetup     *page_setup;
	GtkPrintJob      *job;
	gchar            *filename;
	GError           *error = NULL;

	g_assert (priv->print_settings != NULL);
	g_assert (priv->printer != NULL);

	page_setup = gtk_page_setup_new ();

	job = gtk_print_job_new ("evince-print",
				 priv->printer,
				 priv->print_settings,
				 page_setup);

	g_object_unref (priv->print_settings);
	priv->print_settings = NULL;
	g_object_unref (priv->printer);
	priv->printer = NULL;
	g_object_unref (page_setup);

	filename = g_filename_from_uri (priv->local_uri ?
					priv->local_uri : priv->uri,
					NULL, NULL);

	if (gtk_print_job_set_source_file (job, filename, &error)) {
		gtk_print_job_send (job,
				    (GtkPrintJobCompleteFunc)ev_window_preview_print_finished,
				    window, NULL);
	} else {
		g_warning (error->message);
		g_error_free (error);
	}

	g_free (filename);

	gtk_widget_hide (GTK_WIDGET (window));
}

#endif /* WITH_GTK_PRINT */

static void
ev_window_cmd_preview_print (GtkAction *action, EvWindow *window)
{
#ifdef WITH_GTK_PRINT
	EvWindowPrivate *priv = window->priv;
	GtkPrintSettings *print_settings = NULL;
#if GTK_CHECK_VERSION (2, 11, 0)
	const gchar      *print_settings_file = priv->print_settings_file;

	if (print_settings_file) {
		if (g_file_test (print_settings_file, G_FILE_TEST_IS_REGULAR)) {
			GError *error = NULL;

			print_settings = gtk_print_settings_new_from_file (print_settings_file,
									   &error);
			
			if (error) {
				g_warning (error->message);
				g_error_free (error);
				print_settings = NULL;
			}
		}
	}
#endif /* GTK 2.11.0 */
	
	if (!print_settings)
		print_settings = gtk_print_settings_new ();

	if (priv->print_settings)
		g_object_unref (priv->print_settings);
	priv->print_settings = print_settings;

	gtk_enumerate_printers ((GtkPrinterFunc) ev_window_enumerate_printer_cb,
				window, NULL, FALSE);
#endif /* WITH_GTK_PRINT */
}

static void
ev_window_cmd_escape (GtkAction *action, EvWindow *window)
{
	GtkWidget *widget;

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
			ev_window_stop_fullscreen (window);
		} else if (presentation) {
			ev_window_stop_presentation (window);
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
	enum_value = g_enum_get_value (EV_SIZING_MODE_CLASS, mode);

	if (!ev_window_is_empty (window))
		ev_metadata_manager_set_string (window->priv->uri, "sizing_mode",
						enum_value->value_nick);
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

	g_signal_handlers_disconnect_by_func (ev_window->priv->view, ev_view_update_view_size, scrolled_window);

	if (sizing_mode != EV_SIZING_FREE)
	    	ev_view_update_view_size (EV_VIEW (ev_window->priv->view),
					  GTK_SCROLLED_WINDOW (scrolled_window));

	switch (sizing_mode) {
	        case EV_SIZING_BEST_FIT:
			g_object_set (G_OBJECT (scrolled_window),
				      "hscrollbar-policy", GTK_POLICY_NEVER,
				      "vscrollbar-policy", GTK_POLICY_AUTOMATIC,
				      NULL);
			g_signal_connect (ev_window->priv->view, "zoom_invalid",
					  G_CALLBACK (ev_view_update_view_size),
					  scrolled_window);
			break;
	        case EV_SIZING_FIT_WIDTH:
			g_object_set (G_OBJECT (scrolled_window),
				      "hscrollbar-policy", GTK_POLICY_NEVER,
				      "vscrollbar-policy", GTK_POLICY_AUTOMATIC,
				      NULL);
			g_signal_connect (ev_window->priv->view, "zoom_invalid",
					  G_CALLBACK (ev_view_update_view_size),
					  scrolled_window);
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
		_("\xc2\xa9 1996-2007 The Evince authors"),
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

	/* Either the string or case sensitivity could have changed. */

	case_sensitive = egg_find_bar_get_case_sensitive (find_bar);
	search_string = egg_find_bar_get_search_string (find_bar);

	ev_view_search_changed (EV_VIEW(ev_window->priv->view));

	if (ev_window->priv->document &&
	    EV_IS_DOCUMENT_FIND (ev_window->priv->document)) {
		if (search_string && search_string[0]) {
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

		        ev_window_update_actions (ev_window);
			egg_find_bar_set_status_text (EGG_FIND_BAR (ev_window->priv->find_bar),
						      NULL);
			gtk_widget_queue_draw (GTK_WIDGET (ev_window->priv->view));
		}
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
		ev_view_set_highlight_search (EV_VIEW(ev_window->priv->view), visible);
		ev_view_search_changed (EV_VIEW(ev_window->priv->view));
		ev_window_update_actions (ev_window);

		if (!visible)
			egg_find_bar_set_status_text (EGG_FIND_BAR (ev_window->priv->find_bar), NULL);
	}
}

static void
find_bar_scroll(EggFindBar *find_bar, GtkScrollType scroll, EvWindow* ev_window)
{
	ev_view_scroll(EV_VIEW(ev_window->priv->view), scroll, FALSE);
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

	if (priv->save_job) {
		ev_window_clear_save_job (window);
	}

	if (priv->thumbnail_job) {
		ev_window_clear_thumbnail_job (window);
	}
	
	if (priv->local_uri) {
		ev_window_clear_local_uri (window);
		priv->local_uri = NULL;
	}
	
	ev_window_close_dialogs (window);

#ifdef WITH_GTK_PRINT
	ev_window_clear_print_job (window);

	if (window->priv->gtk_print_job) {
		g_object_unref (window->priv->gtk_print_job);
		window->priv->gtk_print_job = NULL;
	}
	
	if (window->priv->printer) {
		g_object_unref (window->priv->printer);
		window->priv->printer = NULL;
	}

	if (window->priv->print_settings) {
		g_object_unref (window->priv->print_settings);
		window->priv->print_settings = NULL;
	}

	if (window->priv->print_page_setup) {
		g_object_unref (window->priv->print_page_setup);
		window->priv->print_page_setup = NULL;
	}
#endif

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
		if (priv->unlink_temp_file)
			ev_window_clear_temp_file (window);
		g_free (priv->uri);
		priv->uri = NULL;
	}

	if (priv->dest) {
		g_object_unref (priv->dest);
		priv->dest = NULL;
	}

	if (priv->history) {
		g_object_unref (priv->history);
		priv->history = NULL;
	}

	if (priv->print_settings_file) {
		ev_window_clear_print_settings_file (window);
		priv->print_settings_file = NULL;
	}

	if (priv->presentation_timeout_id > 0) {
		g_source_remove (priv->presentation_timeout_id);
		priv->presentation_timeout_id = 0;
	}

	G_OBJECT_CLASS (ev_window_parent_class)->dispose (object);
}

static void
ev_window_class_init (EvWindowClass *ev_window_class)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (ev_window_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (ev_window_class);

	g_object_class->dispose = ev_window_dispose;
	g_object_class->finalize = ev_window_finalize;

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
	{ "FileOpenCopy", NULL, N_("Op_en a Copy"), NULL,
	  N_("Open a copy of the current document in a new window"),
	  G_CALLBACK (ev_window_cmd_file_open_copy) },
       	{ "FileSaveAs", GTK_STOCK_SAVE_AS, N_("_Save a Copy..."), "<control>S",
	  N_("Save a copy of the current document"),
	  G_CALLBACK (ev_window_cmd_save_as) },
	{ "FilePrintSetup", NULL, N_("Print Set_up..."), NULL,
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
	{ "EditRotateLeft", EV_STOCK_ROTATE_LEFT, N_("Rotate _Left"), NULL, NULL,
	  G_CALLBACK (ev_window_cmd_edit_rotate_left) },
	{ "EditRotateRight", EV_STOCK_ROTATE_RIGHT, N_("Rotate _Right"), NULL, NULL,
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
	{ "PreviewPrint", GTK_STOCK_PRINT, N_("Print"), NULL,
	  N_("Print this document"),
	  G_CALLBACK (ev_window_cmd_preview_print) },

	/* Accellerators */
	{ "Escape", NULL, "", "Escape", "",
	  G_CALLBACK (ev_window_cmd_escape) },
        { "Slash", GTK_STOCK_FIND, NULL, "slash", NULL,
          G_CALLBACK (ev_window_cmd_edit_find) },
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
	char *uri = NULL;

	if (filename  && g_path_is_absolute (filename)) {
		uri = gnome_vfs_get_uri_from_local_path (filename);
	} else {
		GnomeVFSURI *base_uri, *resolved_uri;

		base_uri = gnome_vfs_uri_new (window->priv->uri);
		if (base_uri && filename) {
			resolved_uri = gnome_vfs_uri_resolve_relative (base_uri, filename);	
			if (resolved_uri) {
				uri = gnome_vfs_uri_to_string (resolved_uri, GNOME_VFS_URI_HIDE_NONE);
				gnome_vfs_uri_unref (resolved_uri);
			}
			gnome_vfs_uri_unref (base_uri);
		}
	}

	if (uri) {
		gnome_vfs_url_show (uri);
	} else {
		gnome_vfs_url_show (filename);
	}

	g_free (uri);

	/* According to the PDF spec filename can be an executable. I'm not sure
	   allowing to launch executables is a good idea though. -- marco */
}

static void
launch_external_uri (EvWindow *window, EvLinkAction *action)
{
	const gchar *uri = ev_link_action_get_uri (action);
	GnomeVFSResult result = gnome_vfs_url_show (uri);
	GtkWidget *dialog;
	gchar* message = NULL;

	switch(result) {
		case GNOME_VFS_OK:
			break;
		case GNOME_VFS_ERROR_BAD_PARAMETERS:
			message = _("Invalid URI: “%s”");
			break;
		case GNOME_VFS_ERROR_NOT_SUPPORTED:
			message = _("Unsupported URI: “%s”");
			break;
		default:
			message = _("Unknown error");
	}
	if(message) {
		dialog = gtk_message_dialog_new (GTK_WINDOW (window),
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
						 _("Unable to open external link"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  message, uri);
		g_signal_connect (dialog, "response",
				  G_CALLBACK (gtk_widget_destroy),
				  NULL);
		gtk_widget_show (dialog);

	}
	return;
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
					 FALSE,
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
	GtkClipboard *clipboard;
	EvLinkAction *ev_action;
	const gchar *uri;

	ev_action = ev_link_get_action (window->priv->link);
	if (!ev_action)
		return;

	uri = ev_link_action_get_uri (ev_action);

	clipboard = gtk_widget_get_clipboard (GTK_WIDGET (window),
					      GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text (clipboard, uri, -1);
}


static void
image_save_dialog_response_cb (GtkWidget *fc,
			       gint       response_id,
			       EvWindow  *ev_window)
{
	GnomeVFSURI     *target_uri;
	gboolean         is_local;
	GError          *error = NULL;
	GdkPixbuf       *pixbuf;
	gchar           *uri;
	gchar           *uri_extension;
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

	if (format == NULL) {
		ev_window_error_message (GTK_WINDOW (ev_window),
					 _("Couldn't find appropriate format to save image"),
					 NULL);
		g_free (uri);
		gtk_widget_destroy (fc);

		return;
	}

	extensions = gdk_pixbuf_format_get_extensions (format);
	uri_extension = g_strconcat (uri, ".", extensions[0], NULL);
	g_strfreev(extensions);
	file_format = gdk_pixbuf_format_get_name (format);
	
	target_uri = gnome_vfs_uri_new (uri_extension);
	is_local = gnome_vfs_uri_is_local (target_uri);
	
	if (is_local) {
		filename = g_filename_from_uri (uri_extension, NULL, NULL);
	} else {
		filename = ev_tmp_filename ("saveimage");
	}
	
	g_free (uri);
	g_free (uri_extension);

	ev_document_doc_mutex_lock ();
	pixbuf = ev_document_images_get_image (EV_DOCUMENT_IMAGES (ev_window->priv->document),
					       ev_window->priv->image);
	ev_document_doc_mutex_unlock ();
	
	gdk_pixbuf_save (pixbuf, filename, file_format, &error, NULL);
	g_object_unref (pixbuf);
	
	if (error) {
		ev_window_error_message (GTK_WINDOW (ev_window),
					 _("The image could not be saved."),
					 error);
		g_error_free (error);
		g_free (filename);
		gnome_vfs_uri_unref (target_uri);
		gtk_widget_destroy (fc);

		return;
	}

	if (!is_local) {
		GnomeVFSURI *source_uri;
		gchar       *local_uri;

		local_uri = g_filename_to_uri (filename, NULL, NULL);
		source_uri = gnome_vfs_uri_new (local_uri);
		g_free (local_uri);
		ev_window_save_remote (ev_window, source_uri, target_uri);
		gnome_vfs_uri_unref (source_uri);
	}
	
	g_free (filename);
	gnome_vfs_uri_unref (target_uri);
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
	GList *l;
	
	if (!window->priv->attach_list)
		return;

	for (l = window->priv->attach_list; l && l->data; l = g_list_next (l)) {
		EvAttachment *attachment;
		GError       *error = NULL;
		
		attachment = (EvAttachment *) l->data;
		
		ev_attachment_open (attachment, &error);

		if (error) {
			ev_window_error_message (GTK_WINDOW (window),
						 _("Unable to open attachment"),
						 error);
			g_error_free (error);
		}
	}
}

static void
attachment_save_dialog_response_cb (GtkWidget *fc,
				    gint       response_id,
				    EvWindow  *ev_window)
{
	GnomeVFSURI          *target_uri;
	gchar                *uri;
	GList                *l;
	GtkFileChooserAction  fc_action;
	gboolean              is_dir;
	gboolean              is_local;
	
	if (response_id != GTK_RESPONSE_OK) {
		gtk_widget_destroy (fc);
		return;
	}

	uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (fc));
	target_uri = gnome_vfs_uri_new (uri);
	g_object_get (G_OBJECT (fc), "action", &fc_action, NULL);
	is_dir = (fc_action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
	is_local = gnome_vfs_uri_is_local (target_uri);
	
	for (l = ev_window->priv->attach_list; l && l->data; l = g_list_next (l)) {
		EvAttachment *attachment;
		gchar        *filename;
		GError       *error = NULL;
		
		attachment = (EvAttachment *) l->data;

		if (is_local) {
			if (is_dir) {
				filename = g_strjoin ("/", uri,
						      ev_attachment_get_name (attachment),
						      NULL);
			} else {
				filename = g_strdup (uri);
			}
		} else {
			filename = ev_tmp_filename ("saveattachment");
		}

		ev_attachment_save (attachment, filename, &error);
		
		if (error) {
			ev_window_error_message (GTK_WINDOW (ev_window),
						 _("The attachment could not be saved."),
						 error);
			g_error_free (error);
			g_free (filename);

			continue;
		}

		if (!is_local) {
			GnomeVFSURI *src_uri;
			GnomeVFSURI *dest_uri;
			gchar       *local_uri;

			if (is_dir) {
				const gchar *name = ev_attachment_get_name (attachment);

				dest_uri = gnome_vfs_uri_append_file_name (target_uri,
									   name);
			} else {
				dest_uri = gnome_vfs_uri_ref (target_uri);
			}
			
			local_uri = g_filename_to_uri (filename, NULL, NULL);
			src_uri = gnome_vfs_uri_new (local_uri);
			g_free (local_uri);
			ev_window_save_remote (ev_window, src_uri, dest_uri);
			gnome_vfs_uri_unref (src_uri);
			gnome_vfs_uri_unref (dest_uri);
		}

		g_free (filename);
	}

	g_free (uri);
	gnome_vfs_uri_unref (target_uri);

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
ev_window_init (EvWindow *ev_window)
{
	GtkActionGroup *action_group;
	GtkAccelGroup *accel_group;
	GError *error = NULL;
	GtkWidget *sidebar_widget;

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

	if (!gtk_ui_manager_add_ui_from_file (ev_window->priv->ui_manager,
					      DATADIR"/evince-ui.xml",
					      &error)) {
		g_warning ("building menus failed: %s", error->message);
		g_error_free (error);
	}
	
#if GTK_CHECK_VERSION(2,11,4)
	ev_window->priv->recent_manager = gtk_recent_manager_get_default ();
#else
	/* It's fine to just use the one of the default screen here */
	ev_window->priv->recent_manager = gtk_recent_manager_get_for_screen (
		gdk_screen_get_default ());
#endif
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
	g_signal_connect_object (ev_window->priv->view, "external-link",
			         G_CALLBACK (view_external_link_cb),
			         ev_window, 0);
	g_signal_connect_object (ev_window->priv->view, "handle-link",
			         G_CALLBACK (view_handle_link_cb),
			         ev_window, 0);

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

	/* Give focus to the document view */
	gtk_widget_grab_focus (ev_window->priv->view);

	/* Set it user interface params */
	ev_window_setup_recent (ev_window);

	setup_chrome_from_metadata (ev_window);
	set_chrome_actions (ev_window);
	update_chrome_visibility (ev_window);

	gtk_window_set_default_size (GTK_WINDOW (ev_window), 600, 600);

	setup_view_from_metadata (ev_window);
	setup_sidebar_from_metadata (ev_window, NULL);

        ev_window_sizing_mode_changed_cb (EV_VIEW (ev_window->priv->view), NULL, ev_window);
	ev_window_setup_action_sensitivity (ev_window);
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
