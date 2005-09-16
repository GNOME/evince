/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2005 Red Hat, Inc
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

#include "ev-window-title.h"

#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs-utils.h>

struct _EvWindowTitle
{
	EvWindow *window;
	EvDocument *document;
	EvWindowTitleType type;
	char *title;
};

EvWindowTitle *
ev_window_title_new (EvWindow *window)
{
	EvWindowTitle *window_title;

	window_title = g_new0 (EvWindowTitle, 1);
	window_title->window = window;
	window_title->type = EV_WINDOW_TITLE_DOCUMENT;

	return window_title;
}

static void
ev_window_title_update (EvWindowTitle *window_title)
{
	GtkWindow *window = GTK_WINDOW (window_title->window);
	char *password_title;

	switch (window_title->type) {
	case EV_WINDOW_TITLE_DOCUMENT:
		gtk_window_set_title (window, window_title->title);
		break;
	case EV_WINDOW_TITLE_PASSWORD:
		password_title = g_strdup_printf (_("%s - Password Required"),
						  window_title->title);
		gtk_window_set_title (window, password_title);
		g_free (password_title);
		break;
	}
}

void
ev_window_title_set_type (EvWindowTitle *window_title, EvWindowTitleType type)
{
	window_title->type = type;

	ev_window_title_update (window_title);
}

void
ev_window_title_set_document (EvWindowTitle *window_title,
			      EvDocument    *document,
			      const char    *uri)
{
	EvPageCache *page_cache;
	const char *title;

	window_title->document = document;

	g_free (window_title->title);

	if (document == NULL) {
		window_title->title = NULL;
		return;
	}

	page_cache = ev_page_cache_get (document);
	g_return_if_fail (page_cache != NULL);

	title = ev_page_cache_get_title (page_cache);

	/* Make sure we get a valid title back */
	if (title && title[0] != '\000' && g_utf8_validate (title, -1, NULL)) {
		window_title->title = g_strdup (title);
	}

	if (window_title->title) {
		char *p;

		for (p = window_title->title; *p; ++p) {
			/* an '\n' byte is always ASCII, no need for UTF-8 special casing */
			if (*p == '\n')
				*p = ' ';
		}
	}

	if (window_title->title == NULL && uri) {
		char *display_name;

		display_name = gnome_vfs_format_uri_for_display (uri);
		window_title->title = g_path_get_basename (display_name);
		g_free (display_name);
	}

	if (window_title->title == NULL) {
		window_title->title = g_strdup (_("Document Viewer"));
	}

	ev_window_title_update (window_title);
}

void
ev_window_title_free (EvWindowTitle *window_title)
{
	g_free (window_title->title);
	g_free (window_title);
}
