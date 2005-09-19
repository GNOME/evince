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
#include "ev-document-factory.h"

#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs-utils.h>

typedef struct
{
	EvBackend backend;
	const char *ext;
} BadExtensionEntry;

struct _EvWindowTitle
{
	EvWindow *window;
	EvDocument *document;
	EvWindowTitleType type;
	char *title;
};

static const BadExtensionEntry bad_extensions[] = {
	{ EV_BACKEND_PS, ".dvi" },
	{ EV_BACKEND_PDF, ".doc" }
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

static char *
get_filename_from_uri (const char *uri)
{
	char *filename;
	char *display_name;

	display_name = gnome_vfs_format_uri_for_display (uri);
	filename = g_path_get_basename (display_name);
	g_free (display_name);

	return filename;
}

void
ev_window_title_set_document (EvWindowTitle *window_title,
			      EvDocument    *document,
			      const char    *uri)
{
	EvPageCache *page_cache;
	const char *title;
	int i;

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

		/* Some docs report titles with confusing extensions (ex. .doc for pdf).
	           Let's show the filename in this case */
		for (i = 0; i < G_N_ELEMENTS (bad_extensions); i++) {
			if (bad_extensions[i].backend == ev_document_factory_get_backend (document) &&
			    g_str_has_suffix (window_title->title, bad_extensions[i].ext)) {
				char *new_title;
				char *filename = get_filename_from_uri (uri);

				new_title = g_strdup_printf ("%s (%s)", window_title->title, filename);
				g_free (window_title->title);
				window_title->title = new_title;

				g_free (filename);
			}
		}

		for (p = window_title->title; *p; ++p) {
			/* an '\n' byte is always ASCII, no need for UTF-8 special casing */
			if (*p == '\n')
				*p = ' ';
		}
	}

	if (window_title->title == NULL && uri) {
		window_title->title = get_filename_from_uri (uri);
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
