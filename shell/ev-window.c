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
#include "ev-document-thumbnails.h"
#include "ev-document-links.h"
#include "ev-document-find.h"
#include "ev-document-security.h"
#include "eggfindbar.h"

#include "pdf-document.h"
#include "pixbuf-document.h"
#include "ps-document.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomeprintui/gnome-print-dialog.h>

#include <gconf/gconf-client.h>

#include <string.h>

#include "ev-application.h"
#include "ev-stock-icons.h"

typedef enum {
	EV_SIZING_BEST_FIT,
	EV_SIZING_FIT_WIDTH,
	EV_SIZING_FREE,
} EvSizingMode;

typedef enum {
	PAGE_MODE_SINGLE_PAGE,
	PAGE_MODE_CONTINUOUS_PAGE,
	PAGE_MODE_PASSWORD,
} EvWindowPageMode;

typedef enum {
	EV_CHROME_MENUBAR	= 1 << 0,
	EV_CHROME_TOOLBAR	= 1 << 1,
	EV_CHROME_SIDEBAR	= 1 << 2,
	EV_CHROME_FINDBAR	= 1 << 3,
	EV_CHROME_STATUSBAR	= 1 << 4,
	EV_CHROME_NORMAL	= EV_CHROME_MENUBAR | EV_CHROME_TOOLBAR | EV_CHROME_SIDEBAR | EV_CHROME_STATUSBAR
} EvChrome;

struct _EvWindowPrivate {
	GtkWidget *main_box;
	GtkWidget *menubar;
	GtkWidget *toolbar_dock;
	GtkWidget *toolbar;
	GtkWidget *hpaned;
	GtkWidget *sidebar;
	GtkWidget *thumbs_sidebar;
	GtkWidget *find_bar;
	GtkWidget *scrolled_window;
	GtkWidget *view;
	GtkWidget *page_view;
	GtkWidget *password_view;
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	GtkWidget *statusbar;
	guint help_message_cid;
	guint view_message_cid;
	GtkWidget *fullscreen_toolbar;
	GtkWidget *fullscreen_popup;
	char *uri;

	EvDocument *document;

	EvWindowPageMode page_mode;
	/* These members are used temporarily when in PAGE_MODE_PASSWORD */
	EvDocument *password_document;
	GtkWidget *password_dialog;
	char *password_uri;

	EvChrome chrome;
	gboolean fullscreen_mode;
	EvSizingMode sizing_mode;
	GSource *fullscreen_timeout_source;
};

static GtkTargetEntry ev_drop_types[] = {
	{ "text/uri-list", 0, 0 }
};


#define EV_WINDOW_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_WINDOW, EvWindowPrivate))

#define PAGE_SELECTOR_ACTION	"PageSelector"

#define GCONF_CHROME_TOOLBAR	"/apps/evince/show_toolbar"
#define GCONF_CHROME_SIDEBAR	"/apps/evince/show_sidebar"
#define GCONF_CHROME_STATUSBAR	"/apps/evince/show_statusbar"

#define GCONF_SIDEBAR_SIZE      "/apps/evince/sidebar_size"
#define SIDEBAR_DEFAULT_SIZE    132

static void     ev_window_update_fullscreen_popup (EvWindow         *window);
static void     ev_window_sidebar_visibility_changed_cb (EvSidebar *ev_sidebar, GParamSpec *pspec,
							 EvWindow   *ev_window);
static void     ev_window_set_page_mode           (EvWindow         *window,
						   EvWindowPageMode  page_mode);
static gboolean start_loading_document            (EvWindow         *ev_window,
						   EvDocument       *document,
						   const char       *uri);
static void     ev_window_set_sizing_mode         (EvWindow         *ev_window,
						   EvSizingMode      sizing_mode);

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
	EvDocument *document;
	EvWindowPageMode page_mode;
	EvView *view;

	document = ev_window->priv->document;
	page_mode = ev_window->priv->page_mode;

	view = EV_VIEW (ev_window->priv->view);

	/* File menu */
	/* "FileOpen": always sensitive */
	set_action_sensitive (ev_window, "FileSaveAs", document!=NULL);
	set_action_sensitive (ev_window, "FilePrint", document!=NULL);
	/* "FileCloseWindow": always sensitive */

        /* Edit menu */
	set_action_sensitive (ev_window, "EditCopy", document!=NULL);
	set_action_sensitive (ev_window, "EditSelectAll", document!=NULL);

	if (document)
		set_action_sensitive (ev_window, "EditFind", EV_IS_DOCUMENT_FIND (document));
	else
		set_action_sensitive (ev_window, "EditFind", FALSE);

        /* View menu */
	set_action_sensitive (ev_window, "ViewZoomIn", document!=NULL);
	set_action_sensitive (ev_window, "ViewZoomOut", document!=NULL);
	set_action_sensitive (ev_window, "ViewNormalSize", document!=NULL);
	set_action_sensitive (ev_window, "ViewBestFit", document!=NULL);
	set_action_sensitive (ev_window, "ViewPageWidth", document!=NULL);
	set_action_sensitive (ev_window, "ViewReload", document!=NULL);

        /* Go menu */
	if (document) {
		int n_pages;
		int page;

		page = ev_view_get_page (EV_VIEW (ev_window->priv->view));
		n_pages = ev_document_get_n_pages (document);

		set_action_sensitive (ev_window, "GoPreviousPage", page > 1);
		set_action_sensitive (ev_window, "GoNextPage", page < n_pages);
		set_action_sensitive (ev_window, "GoFirstPage", page > 1);
		set_action_sensitive (ev_window, "GoLastPage", page < n_pages);
	} else {
  		set_action_sensitive (ev_window, "GoFirstPage", FALSE);
		set_action_sensitive (ev_window, "GoPreviousPage", FALSE);
		set_action_sensitive (ev_window, "GoNextPage", FALSE);
		set_action_sensitive (ev_window, "GoLastPage", FALSE);
	}

	/* Page View radio group */
	if (document) {
		set_action_sensitive (ev_window, "SinglePage", page_mode != PAGE_MODE_PASSWORD);
		set_action_sensitive (ev_window, "ContinuousPage", page_mode != PAGE_MODE_PASSWORD);
	} else {
		set_action_sensitive (ev_window, "SinglePage", FALSE);
		set_action_sensitive (ev_window, "ContinuousPage", FALSE);
	}
	/* Help menu */
	/* "HelpContents": always sensitive */
	/* "HelpAbout": always sensitive */

	/* Toolbar-specific actions: */
	set_action_sensitive (ev_window, PAGE_SELECTOR_ACTION, document!=NULL);
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
	gboolean menubar, toolbar, sidebar, findbar, statusbar, fullscreen_toolbar;

	menubar = (priv->chrome & EV_CHROME_MENUBAR) != 0 && !priv->fullscreen_mode;
	toolbar = (priv->chrome & EV_CHROME_TOOLBAR) != 0 && !priv->fullscreen_mode;
	sidebar = (priv->chrome & EV_CHROME_SIDEBAR) != 0 && !priv->fullscreen_mode;
	fullscreen_toolbar = (priv->chrome & EV_CHROME_TOOLBAR) != 0;
	statusbar = (priv->chrome & EV_CHROME_STATUSBAR) != 0 && !priv->fullscreen_mode;
	findbar = (priv->chrome & EV_CHROME_FINDBAR) != 0;

	set_widget_visibility (priv->menubar, menubar);
	set_widget_visibility (priv->toolbar_dock, toolbar);
	set_widget_visibility (priv->sidebar, sidebar);
	set_widget_visibility (priv->find_bar, findbar);
	set_widget_visibility (priv->statusbar, statusbar);
	set_widget_visibility (priv->fullscreen_toolbar, fullscreen_toolbar);

	if (priv->fullscreen_popup != NULL) {
		set_widget_visibility (priv->fullscreen_popup, priv->fullscreen_mode);
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
ev_window_cmd_view_best_fit (GtkAction *action, EvWindow *ev_window)
{
	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
		ev_window_set_sizing_mode (ev_window, EV_SIZING_BEST_FIT);
	} else {
		ev_window_set_sizing_mode (ev_window, EV_SIZING_FREE);
	}
}

static void
ev_window_cmd_view_page_width (GtkAction *action, EvWindow *ev_window)
{
	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
		ev_window_set_sizing_mode (ev_window, EV_SIZING_FIT_WIDTH);
	} else {
		ev_window_set_sizing_mode (ev_window, EV_SIZING_FREE);
	}
}

static void
update_sizing_buttons (EvWindow *window)
{
	GtkActionGroup *action_group = window->priv->action_group;
	GtkAction *action;
	gboolean best_fit, page_width;

	switch (window->priv->sizing_mode) {
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
}

void
ev_window_open_page (EvWindow *ev_window, int page)
{
	ev_view_set_page (EV_VIEW (ev_window->priv->view), page);
}

void
ev_window_open_link (EvWindow *ev_window, EvLink *link)
{
	ev_view_go_to_link (EV_VIEW (ev_window->priv->view), link);
}

gboolean
ev_window_is_empty (const EvWindow *ev_window)
{
	g_return_val_if_fail (EV_IS_WINDOW (ev_window), FALSE);

	return ev_window->priv->document == NULL;
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
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

/* Would be nice to have this in gdk-pixbuf */
static gboolean
mime_type_supported_by_gdk_pixbuf (const gchar *mime_type)
{
	GSList *formats, *list;
	gboolean retval = FALSE;

	formats = gdk_pixbuf_get_formats ();

	list = formats;
	while (list) {
		GdkPixbufFormat *format = list->data;
		int i;
		gchar **mime_types;

		if (gdk_pixbuf_format_is_disabled (format))
			continue;

		mime_types = gdk_pixbuf_format_get_mime_types (format);

		for (i = 0; mime_types[i] != NULL; i++) {
			if (strcmp (mime_types[i], mime_type) == 0) {
				retval = TRUE;
				break;
			}
		}

		if (retval)
			break;

		list = list->next;
	}

	g_slist_free (formats);

	return retval;
}

static void
update_window_title (EvDocument *document, GParamSpec *pspec, EvWindow *ev_window)
{
	char *title = NULL;
	char *doc_title = NULL;
	gboolean password_needed;

	password_needed = (ev_window->priv->password_document != NULL);

	if (document) {
		doc_title = ev_document_get_title (document);

		/* Make sure we get a valid title back */
		if (doc_title) {
			if (doc_title[0] == '\000' ||
			    !g_utf8_validate (doc_title, -1, NULL)) {
				g_free (doc_title);
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
		char *basename;

		basename = g_path_get_basename (ev_window->priv->uri);
		doc_title = gnome_vfs_unescape_string_for_display (basename);
		g_free (basename);
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
update_total_pages (EvWindow *ev_window)
{
	GtkAction *action;
	int pages;

	pages = ev_document_get_n_pages (ev_window->priv->document);
	action = gtk_action_group_get_action
		(ev_window->priv->action_group, PAGE_SELECTOR_ACTION);
	ev_page_action_set_total_pages (EV_PAGE_ACTION (action), pages);
}

/* This function assumes that ev_window just had ev_window->document set.
 */
static gboolean
document_supports_sidebar (EvDocument *document)
{
	return (EV_IS_DOCUMENT_THUMBNAILS (document) && EV_IS_DOCUMENT_LINKS (document));
}

static void
hide_sidebar_and_actions (EvWindow *ev_window)
{
	GtkAction *action;
	/* Alsthough we update the hiddenness of the sidebar, we don't want to
	 * store the value */
	g_signal_handlers_disconnect_by_func (ev_window->priv->sidebar,
					      ev_window_sidebar_visibility_changed_cb,
					      ev_window);
	gtk_widget_hide (ev_window->priv->sidebar);
	action = gtk_action_group_get_action (ev_window->priv->action_group, "ViewSidebar");
	gtk_action_set_sensitive (action, FALSE);

}

static void
ev_window_setup_document (EvWindow *ev_window)
{
	EvDocument *document;
	EvView *view = EV_VIEW (ev_window->priv->view);
	EvSidebar *sidebar = EV_SIDEBAR (ev_window->priv->sidebar);

	document = ev_window->priv->document;

	g_signal_connect_object (G_OBJECT (document),
				 "notify::title",
				 G_CALLBACK (update_window_title),
				 ev_window, 0);

	ev_window_set_page_mode (ev_window, PAGE_MODE_SINGLE_PAGE);

	if (document_supports_sidebar (document)) 
		ev_sidebar_set_document (sidebar, document);
	else
		hide_sidebar_and_actions (ev_window);
	ev_view_set_document (view, document);

	update_window_title (document, NULL, ev_window);
	update_total_pages (ev_window);
	update_action_sensitivity (ev_window);
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

		password = ev_password_dialog_get_password (password_dialog);
		if (password)
			ev_document_security_set_password (EV_DOCUMENT_SECURITY (ev_window->priv->password_document),
							   password);
		g_free (password);

		document = ev_window->priv->password_document;
		uri = ev_window->priv->password_uri;

		ev_window->priv->password_document = NULL;
		ev_window->priv->password_uri = NULL;

		if (start_loading_document (ev_window, document, uri)) {
			gtk_widget_destroy (password_dialog);
		}

		g_object_unref (document);
		g_free (uri);

		return;
	}

	gtk_widget_set_sensitive (ev_window->priv->password_view, TRUE);
	gtk_widget_destroy (password_dialog);
}

/* Called either by start_loading_document or by the "unlock" callback on the
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
		gchar *basename, *file_name;

		basename = g_path_get_basename (ev_window->priv->password_uri);
		file_name = gnome_vfs_unescape_string_for_display (basename);
		ev_window->priv->password_dialog =
			ev_password_dialog_new (GTK_WIDGET (ev_window), file_name);
		g_object_add_weak_pointer (G_OBJECT (ev_window->priv->password_dialog),
					   (gpointer *) &(ev_window->priv->password_dialog));
		g_signal_connect (ev_window->priv->password_dialog,
				  "response",
				  G_CALLBACK (password_dialog_response),
				  ev_window);
		g_free (basename);
		g_free (file_name);
		gtk_widget_show (ev_window->priv->password_dialog);
	} else {
		ev_password_dialog_set_bad_pass (ev_window->priv->password_dialog);
	}
}

/* This wil try to load the document.  It might be called multiple times on the
 * same document by the password dialog.
 *
 * Since the flow of the error dialog is very confusing, we assume that both
 * document and uri will go away after this function is called, and thus we need
 * to ref/dup them.  Additionally, it needs to clear
 * ev_window->priv->password_{uri,document}, and thus people who call this
 * function should _not_ necessarily expect those to exist after being
 * called. */
static gboolean
start_loading_document (EvWindow   *ev_window,
			EvDocument *document,
			const char *uri)
{
	gboolean result;
	GError *error = NULL;

	g_assert (document);
	g_assert (document != ev_window->priv->document);
	g_assert (uri);
	if (ev_window->priv->password_document) {
		g_object_unref (ev_window->priv->password_document);
		ev_window->priv->password_document = NULL;
	}
	if (ev_window->priv->password_uri) {
		g_free (ev_window->priv->password_uri);
		ev_window->priv->password_uri = NULL;
	}

	result = ev_document_load (document, uri, &error);

	/* Success! */
	if (result) {
		if (ev_window->priv->document)
			g_object_unref (ev_window->priv->document);
		ev_window->priv->document = g_object_ref (document);
		ev_window_setup_document (ev_window);

		return TRUE;
	}

	/* unable to load the document */
	g_assert (error != NULL);

	if (error->domain == EV_DOCUMENT_ERROR &&
	    error->code == EV_DOCUMENT_ERROR_ENCRYPTED) {
		char *file_name;

		ev_window->priv->password_document = g_object_ref (document);
		ev_window->priv->password_uri = g_strdup (uri);

		file_name = g_path_get_basename (uri);
		ev_password_view_set_file_name (EV_PASSWORD_VIEW (ev_window->priv->password_view),
						file_name);
		g_free (file_name);
		ev_window_set_page_mode (ev_window, PAGE_MODE_PASSWORD);

		ev_window_popup_password_dialog (ev_window);
	} else {
		unable_to_load (ev_window, error->message);
	}
	g_error_free (error);

	return FALSE;
}

static gboolean
is_file_supported (const gchar *mime_type)
{
	static char *supported_types [] = {
		"application/pdf",
		"application/postscript",
		"application/x-gzpostscript",
		"image/x-eps",
		NULL
	};
	gint   i;
	
	g_return_val_if_fail (mime_type != NULL, FALSE);

	if (mime_type_supported_by_gdk_pixbuf (mime_type))
		return TRUE;
	
	for (i = 0; supported_types[i] != NULL; i++) {
		if (g_ascii_strcasecmp (mime_type, supported_types[i]) == 0)
			return TRUE;
	}
	
	return FALSE;
}

void
ev_window_open (EvWindow *ev_window, const char *uri)
{
	EvDocument *document = NULL;
	char *mime_type;

	g_free (ev_window->priv->uri);
	ev_window->priv->uri = g_strdup (uri);

	mime_type = gnome_vfs_get_mime_type (uri);

	if (mime_type == NULL)
		document = NULL;
	else if (!strcmp (mime_type, "application/pdf"))
		document = g_object_new (PDF_TYPE_DOCUMENT, NULL);
	else if (!strcmp (mime_type, "application/postscript") ||
		 !strcmp (mime_type, "application/x-gzpostscript") ||
		 !strcmp (mime_type, "image/x-eps"))
		document = g_object_new (PS_TYPE_DOCUMENT, NULL);
	else if (mime_type_supported_by_gdk_pixbuf (mime_type))
		document = g_object_new (PIXBUF_TYPE_DOCUMENT, NULL);

	if (document) {
		start_loading_document (ev_window, document, uri);
		/* See the comment on start_loading_document on ref counting.
		 * As the password dialog flow is confusing, we're very explicit
		 * on ref counting. */
		g_object_unref (document);
	} else {
		char *error_message;

		error_message = g_strdup_printf (_("Unhandled MIME type: '%s'"),
						 mime_type?mime_type:"<Unknown MIME Type>");
		unable_to_load (ev_window, error_message);
		g_free (error_message);
	}

	g_free (mime_type);
}

static void
ev_window_open_uri_list (EvWindow *ev_window, GList *uri_list)
{
	GList *list;
	gchar *uri, *mime_type;
	
	g_return_if_fail (uri_list != NULL);
	
	list = uri_list;
	while (list) {
		uri = gnome_vfs_uri_to_string (list->data, GNOME_VFS_URI_HIDE_NONE);
		mime_type = gnome_vfs_get_mime_type (uri);
		
		if (is_file_supported (mime_type)) {
			if (ev_window_is_empty (EV_WINDOW (ev_window))) {
				ev_window_open (ev_window, uri);
				
				gtk_widget_show (GTK_WIDGET (ev_window));
			} else {
				EvWindow *new_window;
				
				new_window = ev_application_new_window (EV_APP);
				ev_window_open (new_window, uri);
				
				gtk_widget_show (GTK_WIDGET (new_window));
			}
		}

		g_free (mime_type);
		g_free (uri);

		list = g_list_next (list);
	}
}

static void
ev_window_cmd_file_open (GtkAction *action, EvWindow *ev_window)
{
	ev_application_open (EV_APP, NULL);
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
ev_window_cmd_save_as (GtkAction *action, EvWindow *ev_window)
{
	GtkWidget *fc;
	GtkFileFilter *pdf_filter, *all_filter;
	gchar *uri = NULL;

	fc = gtk_file_chooser_dialog_new (
		_("Save a Copy"),
		NULL, GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_OK,
		NULL);
	gtk_window_set_modal (GTK_WINDOW (fc), TRUE);

	pdf_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (pdf_filter, _("PDF Documents"));
	gtk_file_filter_add_mime_type (pdf_filter, "application/pdf");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (fc), pdf_filter);

	all_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (all_filter, _("All Files"));
	gtk_file_filter_add_pattern (all_filter, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (fc), all_filter);
	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (fc), pdf_filter);

	gtk_dialog_set_default_response (GTK_DIALOG (fc), GTK_RESPONSE_OK);

	gtk_widget_show (fc);

	while (gtk_dialog_run (GTK_DIALOG (fc)) == GTK_RESPONSE_OK) {
		uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (fc));

/* FIXME
		if (g_file_test (uri, G_FILE_TEST_EXISTS) &&
		    !overwrite_existing_file (GTK_WINDOW (fc), uri))
				continue;
*/

		if (ev_document_save (ev_window->priv->document, uri, NULL))
			break;
		else
			save_error_dialog (GTK_WINDOW (fc), uri);
	}
	gtk_widget_destroy (fc);
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
	} else 	if (transport) {
		if (!strcmp ((const gchar *)transport, "CUPS"))
			return TRUE;
	}

	return FALSE;
}

static void
ev_window_print (EvWindow *ev_window)
{
	GnomePrintConfig *config;
	GnomePrintJob *job;
	GtkWidget *print_dialog;
	EvPrintJob *print_job = NULL;

        g_return_if_fail (EV_IS_WINDOW (ev_window));
	g_return_if_fail (ev_window->priv->document != NULL);

	config = gnome_print_config_default ();
	job = gnome_print_job_new (config);

	print_dialog = gnome_print_dialog_new (job, _("Print"),
					       (GNOME_PRINT_DIALOG_RANGE |
						GNOME_PRINT_DIALOG_COPIES));
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
		if (!using_postscript_printer (config)) {
			GtkWidget *dialog;

			dialog = gtk_message_dialog_new (
				GTK_WINDOW (print_dialog), GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
				_("Printing is not supported on this printer."));
			gtk_message_dialog_format_secondary_text (
				GTK_MESSAGE_DIALOG (dialog),
				_("You were trying to print to a printer using the \"%s\" driver. This program requires a PostScript printer driver."),
				gnome_print_config_get (
					config, "Settings.Engine.Backend.Driver"));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);

			continue;
		}

		print_job = g_object_new (EV_TYPE_PRINT_JOB,
					  "gnome_print_job", job,
					  "document", ev_window->priv->document,
					  "print_dialog", print_dialog,
					  NULL);
		break;
	}

	gtk_widget_destroy (print_dialog);

	if (print_job != NULL) {
		ev_print_job_print (print_job, GTK_WINDOW (ev_window));
		g_object_unref (print_job);
	}
}

static void
ev_window_cmd_file_print (GtkAction *action, EvWindow *ev_window)
{
	ev_window_print (ev_window);
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

		egg_find_bar_grab_focus (EGG_FIND_BAR (ev_window->priv->find_bar));
	}
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
	GdkRectangle screen_rect;
	gboolean toolbar;

	g_return_if_fail (popup != NULL);

	toolbar = (window->priv->chrome & EV_CHROME_TOOLBAR) != 0;
	popup_width = popup->requisition.width;
	popup_height = popup->requisition.height;

	/* FIXME multihead */
	gdk_screen_get_monitor_geometry (gdk_screen_get_default (),
			gdk_screen_get_monitor_at_window
                        (gdk_screen_get_default (),
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
	GConfClient *client;
	int sidebar_size;

	sidebar_size = gtk_paned_get_position (GTK_PANED (object));

	client = gconf_client_get_default ();
	gconf_client_set_int (client, GCONF_SIDEBAR_SIZE, sidebar_size, NULL);
	g_object_unref (client);
}

static void
destroy_fullscreen_popup (EvWindow *window)
{
	if (window->priv->fullscreen_popup != NULL)
	{
		/* FIXME multihead */
		g_signal_handlers_disconnect_by_func
			(gdk_screen_get_default (),
			 G_CALLBACK (screen_size_changed_cb), window);

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
	window->priv->fullscreen_timeout_source = NULL;

	return FALSE;
}

static void
fullscreen_set_timeout (EvWindow *window)
{
	GSource *source;

	if (window->priv->fullscreen_timeout_source != NULL)
		g_source_destroy (window->priv->fullscreen_timeout_source);

	source = g_timeout_source_new (1000);
	g_source_set_callback (source, fullscreen_timeout_cb, window, NULL);
	g_source_attach (source, NULL);
	window->priv->fullscreen_timeout_source = source;
}

static void
fullscreen_clear_timeout (EvWindow *window)
{
	if (window->priv->fullscreen_timeout_source != NULL)
		g_source_destroy (window->priv->fullscreen_timeout_source);

	window->priv->fullscreen_timeout_source = NULL;
	ev_view_show_cursor (EV_VIEW (window->priv->view));
}

static gboolean
fullscreen_motion_notify_cb (GtkWidget *widget,
			     GdkEventMotion *event,
			     gpointer user_data)
{
	EvWindow *window = EV_WINDOW (user_data);

	if (!GTK_WIDGET_VISIBLE (window->priv->fullscreen_popup)) {
		g_object_set (window->priv->fullscreen_popup, "visible", TRUE, NULL);
		ev_view_show_cursor (EV_VIEW (window->priv->view));
	}

	fullscreen_set_timeout (window);

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

	/* FIXME multihead */
	g_signal_connect (gdk_screen_get_default (), "size-changed",
			  G_CALLBACK (screen_size_changed_cb), window);
	g_signal_connect (popup, "size_request",
			  G_CALLBACK (fullscreen_popup_size_request_cb), window);

	return popup;
}

static void
ev_window_fullscreen (EvWindow *window)
{
	window->priv->fullscreen_mode = TRUE;

	if (window->priv->fullscreen_popup == NULL)
		window->priv->fullscreen_popup
			= ev_window_create_fullscreen_popup (window);
	update_chrome_visibility (window);

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

	ev_window_update_fullscreen_popup (window);
}

static void
ev_window_unfullscreen (EvWindow *window)
{
	window->priv->fullscreen_mode = FALSE;

	g_object_set (G_OBJECT (window->priv->scrolled_window),
		      "shadow-type", GTK_SHADOW_IN,
		      NULL);

	fullscreen_clear_timeout (window);

	g_signal_handlers_disconnect_by_func (window->priv->view,
					      (gpointer) fullscreen_motion_notify_cb,
					      window);

//	destroy_fullscreen_popup (window);

	update_chrome_visibility (window);
}

static void
ev_window_cmd_view_fullscreen (GtkAction *action, EvWindow *ev_window)
{
	gboolean fullscreen;

        g_return_if_fail (EV_IS_WINDOW (ev_window));

	fullscreen = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

	if (fullscreen) {
		gtk_window_fullscreen (GTK_WINDOW (ev_window));
	} else {
		gtk_window_unfullscreen (GTK_WINDOW (ev_window));
	}
}

static gboolean
ev_window_state_event_cb (GtkWidget *widget, GdkEventWindowState *event, EvWindow *window)
{
	if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
	{
		GtkActionGroup *action_group;
		GtkAction *action;
		gboolean fullscreen;

		fullscreen = event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN;

		if (fullscreen)
		{
			ev_window_fullscreen (window);
		}
		else
		{
			ev_window_unfullscreen (window);
		}

		action_group = window->priv->action_group;

		action = gtk_action_group_get_action (action_group, "ViewFullscreen");
		g_signal_handlers_block_by_func
			(action, G_CALLBACK (ev_window_cmd_view_fullscreen), window);
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), fullscreen);
		g_signal_handlers_unblock_by_func
			(action, G_CALLBACK (ev_window_cmd_view_fullscreen), window);

	}

	return FALSE;
}

static gboolean
ev_window_focus_in_event (GtkWidget *widget, GdkEventFocus *event)
{
	EvWindow *window = EV_WINDOW (widget);
	EvWindowPrivate *priv = window->priv;

	if (priv->fullscreen_mode)
	{
		gtk_widget_show (priv->fullscreen_popup);
	}

	return GTK_WIDGET_CLASS (ev_window_parent_class)->focus_in_event (widget, event);
}

static gboolean
ev_window_focus_out_event (GtkWidget *widget, GdkEventFocus *event)
{
	EvWindow *window = EV_WINDOW (widget);
	EvWindowPrivate *priv = window->priv;

	if (priv->fullscreen_mode)
	{
		gtk_widget_hide (priv->fullscreen_popup);
	}

	return GTK_WIDGET_CLASS (ev_window_parent_class)->focus_out_event (widget, event);
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
	case PAGE_MODE_SINGLE_PAGE:
		child = window->priv->view;
		break;
	case PAGE_MODE_PASSWORD:
		child = window->priv->password_view;
		break;
	case PAGE_MODE_CONTINUOUS_PAGE:
		child = window->priv->page_view;
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
ev_window_cmd_view_zoom_in (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

	ev_window_set_sizing_mode (ev_window, EV_SIZING_FREE);

	ev_view_zoom_in (EV_VIEW (ev_window->priv->view));
}

static void
ev_window_cmd_view_zoom_out (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

	ev_window_set_sizing_mode (ev_window, EV_SIZING_FREE);

	ev_view_zoom_out (EV_VIEW (ev_window->priv->view));
}

static void
ev_window_cmd_view_normal_size (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

	ev_view_set_size (EV_VIEW (ev_window->priv->view), -1, -1);
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
ev_window_cmd_view_reload (GtkAction *action, EvWindow *ev_window)
{
	char *uri;
	int page;

	g_return_if_fail (EV_IS_WINDOW (ev_window));

	page = ev_document_get_page (ev_window->priv->document);
	uri = g_strdup (ev_window->priv->uri);

	ev_window_open (ev_window, uri);
	ev_window_open_page (ev_window, page);

	g_free (uri);
}

static void
ev_window_cmd_help_contents (GtkAction *action, EvWindow *ev_window)
{
        g_return_if_fail (EV_IS_WINDOW (ev_window));

        /* FIXME */
}

static void
ev_window_cmd_leave_fullscreen (GtkAction *action, EvWindow *window)
{
	gtk_window_unfullscreen (GTK_WINDOW (window));
}

static void
update_view_size (EvWindow *window)
{
	int width, height;
	GtkRequisition vsb_requisition;
	int scrollbar_spacing;

	width = window->priv->scrolled_window->allocation.width;
	height = window->priv->scrolled_window->allocation.height;

	/* the scrolled window has a GTK_SHADOW_IN */
	width -= 2 * window->priv->view->style->xthickness;
	height -= 2 * window->priv->view->style->ythickness;

	if (window->priv->sizing_mode == EV_SIZING_BEST_FIT) {
		ev_view_set_size (EV_VIEW (window->priv->view),
				  MAX (1, width), MAX (1, height));
	} else if (window->priv->sizing_mode == EV_SIZING_FIT_WIDTH) {
		gtk_widget_size_request (GTK_SCROLLED_WINDOW (window->priv->scrolled_window)->vscrollbar,
					 &vsb_requisition);
		gtk_widget_style_get (window->priv->scrolled_window,
				      "scrollbar_spacing", &scrollbar_spacing,
				      NULL);
		ev_view_set_size (EV_VIEW (window->priv->view),
				  width - vsb_requisition.width - scrollbar_spacing, -1);
	}
}

static void
size_allocate_cb (GtkWidget     *scrolled_window,
		  GtkAllocation *allocation,
		  EvWindow      *window)
{
	update_view_size (window);
}

static void
ev_window_set_sizing_mode (EvWindow     *ev_window,
			   EvSizingMode  sizing_mode)
{
	GtkWidget *scrolled_window;

	if (ev_window->priv->sizing_mode == sizing_mode)
		return;

	scrolled_window = ev_window->priv->scrolled_window;
	ev_window->priv->sizing_mode = sizing_mode;

	g_signal_handlers_disconnect_by_func (scrolled_window, size_allocate_cb, ev_window);

	update_view_size (ev_window);

	switch (sizing_mode) {
	case EV_SIZING_BEST_FIT:
		g_object_set (G_OBJECT (scrolled_window),
			      "hscrollbar-policy", GTK_POLICY_NEVER,
			      "vscrollbar-policy", GTK_POLICY_NEVER,
			      NULL);
		g_signal_connect (scrolled_window, "size-allocate",
				  G_CALLBACK (size_allocate_cb),
				  ev_window);
		break;
	case EV_SIZING_FIT_WIDTH:
		g_object_set (G_OBJECT (scrolled_window),
			      "hscrollbar-policy", GTK_POLICY_NEVER,
			      "vscrollbar-policy", GTK_POLICY_AUTOMATIC,
			      NULL);
		g_signal_connect (scrolled_window, "size-allocate",
				  G_CALLBACK (size_allocate_cb),
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
	update_chrome_flag (ev_window, EV_CHROME_TOOLBAR,
			    GCONF_CHROME_TOOLBAR,
			    gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
}

static void
ev_window_view_statusbar_cb (GtkAction *action, EvWindow *ev_window)
{
	update_chrome_flag (ev_window, EV_CHROME_STATUSBAR,
			    GCONF_CHROME_STATUSBAR,
			    gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
}

static void
ev_window_view_sidebar_cb (GtkAction *action, EvWindow *ev_window)
{
	update_chrome_flag (ev_window, EV_CHROME_SIDEBAR,
			    GCONF_CHROME_SIDEBAR,
			    gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
}

static void
ev_window_sidebar_visibility_changed_cb (EvSidebar *ev_sidebar, GParamSpec *pspec,
					 EvWindow   *ev_window)
{
	GtkAction *action;
	gboolean visible;

	visible = GTK_WIDGET_VISIBLE (ev_sidebar);

	/* In fullscreen mode the sidebar is not visible,
	 * but we don't want to update the chrome
	 */
	if (ev_window->priv->fullscreen_mode)
		return;
	
	action = gtk_action_group_get_action (ev_window->priv->action_group, "ViewSidebar");
	
	g_signal_handlers_block_by_func
		(action, G_CALLBACK (ev_window_view_sidebar_cb), ev_window);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), visible);
	g_signal_handlers_unblock_by_func
		(action, G_CALLBACK (ev_window_view_sidebar_cb), ev_window);

	update_chrome_flag (ev_window, EV_CHROME_SIDEBAR,
			    GCONF_CHROME_SIDEBAR, visible);
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
update_current_page (EvWindow *ev_window,
		     EvView   *view)
{
	int page;
	GtkAction *action;
	EvSidebarThumbnails *thumbs;

	thumbs = EV_SIDEBAR_THUMBNAILS (ev_window->priv->thumbs_sidebar);
	ev_sidebar_thumbnails_select_page (thumbs, ev_view_get_page (view));

	action = gtk_action_group_get_action
		(ev_window->priv->action_group, PAGE_SELECTOR_ACTION);

	page = ev_view_get_page (EV_VIEW (ev_window->priv->view));
	ev_page_action_set_current_page (EV_PAGE_ACTION (action), page);
}

static void
view_page_changed_cb (EvView   *view,
		      EvWindow *ev_window)
{
	update_current_page (ev_window, view);
	update_action_sensitivity (ev_window);
}

static void
view_status_changed_cb (EvView     *view,
			GParamSpec *pspec,
			EvWindow   *ev_window)
{
	const char *message;

	gtk_statusbar_pop (GTK_STATUSBAR (ev_window->priv->statusbar),
			   ev_window->priv->view_message_cid);

	message = ev_view_get_status (view);
	if (message) {
		gtk_statusbar_push (GTK_STATUSBAR (ev_window->priv->statusbar),
				    ev_window->priv->view_message_cid, message);
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
ev_window_page_mode_cb (GtkRadioAction *action,
			GtkRadioAction *activated_action,
			EvWindow       *window)
{
	int mode;

	mode = gtk_radio_action_get_current_value (action);

	g_assert (mode == PAGE_MODE_CONTINUOUS_PAGE ||
		  mode == PAGE_MODE_SINGLE_PAGE);

	ev_window_set_page_mode (window, (EvWindowPageMode) mode);
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
		if (visible && search_string) {
			ev_document_find_begin (EV_DOCUMENT_FIND (ev_window->priv->document), search_string, case_sensitive);
		} else {
			ev_document_find_cancel (EV_DOCUMENT_FIND (ev_window->priv->document));
			egg_find_bar_set_status_text (EGG_FIND_BAR (ev_window->priv->find_bar),
						      NULL);
			gtk_widget_queue_draw (GTK_WIDGET (ev_window->priv->view));
		}
	}
}

static void
ev_window_dispose (GObject *object)
{
	EvWindow *window = EV_WINDOW (object);
	EvWindowPrivate *priv = window->priv;

	if (priv->ui_manager) {
		g_object_unref (priv->ui_manager);
		priv->ui_manager = NULL;
	}

	if (priv->action_group) {
		g_object_unref (priv->action_group);
		priv->action_group = NULL;
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

	if (priv->password_document) {
		g_object_unref (priv->password_document);
		priv->password_document = NULL;
	}
	
	if (priv->password_uri) {
		g_free (priv->password_uri);
		priv->password_uri = NULL;
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

	widget_class->focus_in_event = ev_window_focus_in_event;
	widget_class->focus_out_event = ev_window_focus_out_event;

	g_type_class_add_private (g_object_class, sizeof (EvWindowPrivate));
}

/* Normal items */
static GtkActionEntry entries[] = {
	{ "File", NULL, N_("_File") },
        { "Edit", NULL, N_("_Edit") },
	{ "View", NULL, N_("_View") },
        { "Go", NULL, N_("_Go") },
	{ "Help", NULL, N_("_Help") },

	/* File menu */
	{ "FileOpen", GTK_STOCK_OPEN, NULL, "<control>O",
	  N_("Open an existing document"),
	  G_CALLBACK (ev_window_cmd_file_open) },
       	{ "FileSaveAs", GTK_STOCK_SAVE_AS, N_("_Save a Copy..."), NULL,
	  N_("Save the current document with a new filename"),
	  G_CALLBACK (ev_window_cmd_save_as) },
	{ "FilePrint", GTK_STOCK_PRINT, N_("Print..."), "<control>P",
	  N_("Print this document"),
	  G_CALLBACK (ev_window_cmd_file_print) },
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

        /* View menu */
        { "ViewZoomIn", GTK_STOCK_ZOOM_IN, NULL, "<control>plus",
          N_("Enlarge the document"),
          G_CALLBACK (ev_window_cmd_view_zoom_in) },
        { "ViewZoomOut", GTK_STOCK_ZOOM_OUT, NULL, "<control>minus",
          N_("Shrink the document"),
          G_CALLBACK (ev_window_cmd_view_zoom_out) },
        { "ViewNormalSize", GTK_STOCK_ZOOM_100, NULL, "<control>0",
          N_("Reset the zoom level to the default value"),
          G_CALLBACK (ev_window_cmd_view_normal_size) },
        { "ViewReload", GTK_STOCK_REFRESH, N_("_Reload"), "<control>R",
          N_("Reload the document"),
          G_CALLBACK (ev_window_cmd_view_reload) },

        /* Go menu */
        { "GoPreviousPage", GTK_STOCK_GO_BACK, N_("_Previous Page"), "Page_Up",
          N_("Go to the previous page"),
          G_CALLBACK (ev_window_cmd_go_previous_page) },
        { "GoNextPage", GTK_STOCK_GO_FORWARD, N_("_Next Page"), "Page_Down",
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

	/* Toolbar-only */
	{ "LeaveFullscreen", EV_STOCK_LEAVE_FULLSCREEN, N_("Leave Fullscreen"), "Escape",
	  N_("Leave fullscreen mode"),
	  G_CALLBACK (ev_window_cmd_leave_fullscreen) }
};

/* Toggle items */
static GtkToggleActionEntry toggle_entries[] = {
	/* View Menu */
	{ "ViewToolbar", NULL, N_("_Toolbar"), "<shift><control>T",
	  N_("Show or hide the toolbar"),
	  G_CALLBACK (ev_window_view_toolbar_cb), TRUE },
	{ "ViewStatusbar", NULL, N_("_Statusbar"), NULL,
	  N_("Show or hide the statusbar"),
	  G_CALLBACK (ev_window_view_statusbar_cb), TRUE },
        { "ViewSidebar", NULL, N_("Side _pane"), "F9",
	  N_("Show or hide the side pane"),
	  G_CALLBACK (ev_window_view_sidebar_cb), TRUE },
        { "ViewFullscreen", NULL, N_("_Fullscreen"), "F11",
          N_("Expand the window to fill the screen"),
          G_CALLBACK (ev_window_cmd_view_fullscreen) },
        { "ViewBestFit", EV_STOCK_ZOOM_PAGE, N_("_Best Fit"), NULL,
          N_("Make the current document fill the window"),
          G_CALLBACK (ev_window_cmd_view_best_fit) },
        { "ViewPageWidth", EV_STOCK_ZOOM_WIDTH, N_("Fit Page _Width"), NULL,
          N_("Make the current document fill the window width"),
          G_CALLBACK (ev_window_cmd_view_page_width) },
};

static GtkRadioActionEntry page_view_entries[] = {
	{ "SinglePage", GTK_STOCK_DND, N_("Single"), NULL,
	  N_("Show the document one page at a time"),
	  PAGE_MODE_SINGLE_PAGE },
	{ "ContinuousPage", GTK_STOCK_DND_MULTIPLE, N_("Multi"), NULL,
	  N_("Show the full document at once"),
	  PAGE_MODE_CONTINUOUS_PAGE }
};

static void
goto_page_cb (GtkAction *action, int page_number, EvWindow *ev_window)
{
	EvView *view = EV_VIEW (ev_window->priv->view);

	if (ev_view_get_page (view) != page_number) {
		ev_view_set_page (view, page_number);
	}
}

static void
drag_data_received_cb (GtkWidget *widget, GdkDragContext *context,
		       gint x, gint y, GtkSelectionData *selection_data,
		       guint info, guint time, gpointer gdata)
{
	GList    *uri_list = NULL;

	uri_list = gnome_vfs_uri_list_parse ((gchar *) selection_data->data);

	if (uri_list) {
		ev_window_open_uri_list (EV_WINDOW (widget), uri_list);
		
		gnome_vfs_uri_list_free (uri_list);

		gtk_drag_finish (context, TRUE, FALSE, time);
	}
}

static void
register_custom_actions (EvWindow *window, GtkActionGroup *group)
{
	GtkAction *action;

	action = g_object_new (EV_TYPE_PAGE_ACTION,
			       "name", PAGE_SELECTOR_ACTION,
			       "label", _("Page"),
			       "tooltip", _("Select Page"),
			       NULL);
	g_signal_connect (action, "goto_page",
			  G_CALLBACK (goto_page_cb), window);
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
	/*translators: this is the label for toolbar button*/
	g_object_set (action, "is-important", TRUE, NULL);
	g_object_set (action, "short_label", _("Next"), NULL);
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

	action= gtk_action_group_get_action (action_group, "ViewSidebar");
	g_signal_handlers_block_by_func
		(action, G_CALLBACK (ev_window_view_sidebar_cb), window);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      (priv->chrome & EV_CHROME_SIDEBAR) != 0);
	g_signal_handlers_unblock_by_func
		(action, G_CALLBACK (ev_window_view_sidebar_cb), window);

	action= gtk_action_group_get_action (action_group, "ViewStatusbar");
	g_signal_handlers_block_by_func
		(action, G_CALLBACK (ev_window_view_statusbar_cb), window);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      (priv->chrome & EV_CHROME_STATUSBAR) != 0);
	g_signal_handlers_unblock_by_func
		(action, G_CALLBACK (ev_window_view_statusbar_cb), window);
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

	value = gconf_client_get (client, GCONF_CHROME_SIDEBAR, NULL);
	if (value != NULL) {
		if (value->type == GCONF_VALUE_BOOL && !gconf_value_get_bool (value)) {
			chrome &= ~EV_CHROME_SIDEBAR;
		}
		gconf_value_free (value);
	}

	value = gconf_client_get (client, GCONF_CHROME_STATUSBAR, NULL);
	if (value != NULL) {
		if (value->type == GCONF_VALUE_BOOL && !gconf_value_get_bool (value)) {
			chrome &= ~EV_CHROME_STATUSBAR;
		}
		gconf_value_free (value);
	}

	g_object_unref (client);

	return chrome;
}

static void
ev_window_init (EvWindow *ev_window)
{
	GtkActionGroup *action_group;
	GtkAccelGroup *accel_group;
	GError *error = NULL;
	GtkWidget *sidebar_widget, *toolbar_dock;
	GConfValue *value;
	GConfClient *client;
	int sidebar_size;

	ev_window->priv = EV_WINDOW_GET_PRIVATE (ev_window);

	ev_window->priv->page_mode = PAGE_MODE_SINGLE_PAGE;
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
	gtk_action_group_add_radio_actions (action_group, page_view_entries,
					    G_N_ELEMENTS (page_view_entries),
					    ev_window->priv->page_mode,
					    G_CALLBACK (ev_window_page_mode_cb),
					    ev_window);
	set_action_properties (action_group);
	register_custom_actions (ev_window, action_group);

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

	ev_window->priv->toolbar =
		gtk_ui_manager_get_widget (ev_window->priv->ui_manager,
					   "/ToolBar");
	gtk_box_pack_start (GTK_BOX (toolbar_dock), ev_window->priv->toolbar,
			    TRUE, TRUE, 0);
	gtk_widget_show (ev_window->priv->toolbar);

	ev_window->priv->fullscreen_toolbar =
		gtk_ui_manager_get_widget (ev_window->priv->ui_manager, "/LeaveFullscreenToolbar");
	gtk_toolbar_set_show_arrow (GTK_TOOLBAR (ev_window->priv->fullscreen_toolbar), TRUE);
	gtk_toolbar_set_style (GTK_TOOLBAR (ev_window->priv->fullscreen_toolbar), GTK_TOOLBAR_BOTH_HORIZ);

	/* Add the main area */
	ev_window->priv->hpaned = gtk_hpaned_new ();
	g_signal_connect (ev_window->priv->hpaned,
			  "notify::position",
			  G_CALLBACK (ev_window_sidebar_position_change_cb),
			  ev_window);
	
	sidebar_size = SIDEBAR_DEFAULT_SIZE;
	client = gconf_client_get_default ();
	value = gconf_client_get (client, GCONF_SIDEBAR_SIZE, NULL);
	if (value != NULL) {
		if (value->type == GCONF_VALUE_INT) {
			sidebar_size = gconf_value_get_int (value);
		}
		gconf_value_free (value);
	}
	g_object_unref (client);
	gtk_paned_set_position (GTK_PANED (ev_window->priv->hpaned), sidebar_size);
	gtk_box_pack_start (GTK_BOX (ev_window->priv->main_box), ev_window->priv->hpaned,
			    TRUE, TRUE, 0);
	gtk_widget_show (ev_window->priv->hpaned);
	
	ev_window->priv->sidebar = ev_sidebar_new ();
	gtk_paned_pack1 (GTK_PANED (ev_window->priv->hpaned),
			 ev_window->priv->sidebar, FALSE, FALSE);
	gtk_widget_show (ev_window->priv->sidebar);

	/* Stub sidebar, for now */
	sidebar_widget = ev_sidebar_links_new ();
	gtk_widget_show (sidebar_widget);
	ev_sidebar_add_page (EV_SIDEBAR (ev_window->priv->sidebar),
			     "index",
			     _("Index"),
			     sidebar_widget);

	ev_window->priv->thumbs_sidebar = ev_sidebar_thumbnails_new ();
	gtk_widget_show (ev_window->priv->thumbs_sidebar);
	ev_sidebar_add_page (EV_SIDEBAR (ev_window->priv->sidebar),
			     "thumbnails",
			     _("Thumbnails"),
			     ev_window->priv->thumbs_sidebar);

	ev_window->priv->scrolled_window =
		GTK_WIDGET (g_object_new (GTK_TYPE_SCROLLED_WINDOW,
					  "shadow-type", GTK_SHADOW_IN,
					  NULL));
	gtk_widget_show (ev_window->priv->scrolled_window);

	gtk_paned_add2 (GTK_PANED (ev_window->priv->hpaned),
			ev_window->priv->scrolled_window);

	ev_window->priv->view = ev_view_new ();
	//ev_window->priv->page_view = ev_page_view_new ();
	ev_window->priv->password_view = ev_password_view_new ();
	g_signal_connect_swapped (ev_window->priv->password_view,
				  "unlock",
				  G_CALLBACK (ev_window_popup_password_dialog),
				  ev_window);
	gtk_widget_show (ev_window->priv->view);
	//gtk_widget_show (ev_window->priv->page_view);
	gtk_widget_show (ev_window->priv->password_view);

	/* We own a ref on these widgets, as we can swap them in and out */
	g_object_ref (ev_window->priv->view);
	//g_object_ref (ev_window->priv->page_view);
	g_object_ref (ev_window->priv->password_view);

	gtk_container_add (GTK_CONTAINER (ev_window->priv->scrolled_window),
			   ev_window->priv->view);
	g_signal_connect (ev_window->priv->view,
			  "page-changed",
			  G_CALLBACK (view_page_changed_cb),
			  ev_window);
	g_signal_connect (ev_window->priv->view,
			  "notify::find-status",
			  G_CALLBACK (view_find_status_changed_cb),
			  ev_window);
	g_signal_connect (ev_window->priv->view,
			  "notify::status",
			  G_CALLBACK (view_status_changed_cb),
			  ev_window);

	ev_window->priv->statusbar = gtk_statusbar_new ();
	gtk_box_pack_end (GTK_BOX (ev_window->priv->main_box),
			  ev_window->priv->statusbar,
			  FALSE, TRUE, 0);
	ev_window->priv->help_message_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR (ev_window->priv->statusbar), "help_message");
	ev_window->priv->view_message_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR (ev_window->priv->statusbar), "view_message");

	ev_window->priv->find_bar = egg_find_bar_new ();
	gtk_box_pack_end (GTK_BOX (ev_window->priv->main_box),
			  ev_window->priv->find_bar,
			  FALSE, TRUE, 0);

	ev_window->priv->chrome = load_chrome ();
	set_chrome_actions (ev_window);
	update_chrome_visibility (ev_window);

	/* Connect sidebar signals */
	g_signal_connect (ev_window->priv->sidebar,
			  "notify::visible",
			  G_CALLBACK (ev_window_sidebar_visibility_changed_cb),
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

	g_signal_connect (ev_window, "window-state-event",
			  G_CALLBACK (ev_window_state_event_cb),
			  ev_window);

	/* Give focus to the scrolled window */
	gtk_widget_grab_focus (ev_window->priv->scrolled_window);

	/* Drag and Drop */
	gtk_drag_dest_unset (GTK_WIDGET (ev_window));
	gtk_drag_dest_set (GTK_WIDGET (ev_window), GTK_DEST_DEFAULT_ALL, ev_drop_types,
			   sizeof (ev_drop_types) / sizeof (ev_drop_types[0]),
			   GDK_ACTION_COPY);
	g_signal_connect (G_OBJECT (ev_window), "drag_data_received",
			  G_CALLBACK (drag_data_received_cb), NULL);

	/* Set it to something random to force a change */
	ev_window->priv->sizing_mode = EV_SIZING_FREE;
	ev_window_set_sizing_mode (ev_window,  EV_SIZING_FIT_WIDTH);
	update_action_sensitivity (ev_window);
}
