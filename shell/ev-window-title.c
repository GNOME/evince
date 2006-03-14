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
	EvWindowTitleType type;
	EvDocument *document;
	char *uri;
};

static const BadExtensionEntry bad_extensions[] = {
	{ EV_BACKEND_PS, ".dvi" },
	{ EV_BACKEND_PDF, ".doc" },
	{ EV_BACKEND_PDF, ".indd" },
	{ EV_BACKEND_PDF, ".rtf" }
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

/* Some docs report titles with confusing extensions (ex. .doc for pdf).
   Let's show the filename in this case */
static void
ev_window_title_sanitize_extension (EvWindowTitle *window_title, char **title) {
	EvBackend backend;
	int i;

	backend = ev_document_factory_get_backend (window_title->document);
	for (i = 0; i < G_N_ELEMENTS (bad_extensions); i++) {
		if (bad_extensions[i].backend == backend &&
		    g_str_has_suffix (*title, bad_extensions[i].ext)) {
			char *new_title;
			char *filename = get_filename_from_uri (window_title->uri);

			new_title = g_strdup_printf ("%s (%s)", *title, filename);
			g_free (*title);
			*title = new_title;

			g_free (filename);
		}
	}
}

static void
ev_window_title_update (EvWindowTitle *window_title)
{
	GtkWindow *window = GTK_WINDOW (window_title->window);
	char *title = NULL, *password_title, *p;
	EvPageCache *page_cache;

	if (window_title->document != NULL) {
		char *doc_title;

		page_cache = ev_page_cache_get (window_title->document);
		g_return_if_fail (page_cache != NULL);
		doc_title = (char *)ev_page_cache_get_title (page_cache);

		/* Make sure we get a valid title back */
		if (doc_title != NULL) {
			doc_title = g_strstrip (doc_title);

			if (doc_title[0] != '\0' &&
			    g_utf8_validate (doc_title, -1, NULL)) {
				title = g_strdup (doc_title);
			}
		}
	}

	if (title && window_title->uri) {
		ev_window_title_sanitize_extension (window_title, &title);
	} else {
		if (window_title->uri) {
			title = get_filename_from_uri (window_title->uri);
		} else {
			title = g_strdup (_("Document Viewer"));
		}
	}

	for (p = title; *p; ++p) {
		/* an '\n' byte is always ASCII, no need for UTF-8 special casing */
		if (*p == '\n')	*p = ' ';
	}

	switch (window_title->type) {
	case EV_WINDOW_TITLE_DOCUMENT:
		gtk_window_set_title (window, title);
		break;
	case EV_WINDOW_TITLE_PASSWORD:
		password_title = g_strdup_printf (_("%s - Password Required"), title);
		gtk_window_set_title (window, password_title);
		g_free (password_title);
		break;
	}

	g_free (title);
}

void
ev_window_title_set_type (EvWindowTitle *window_title, EvWindowTitleType type)
{
	window_title->type = type;

	ev_window_title_update (window_title);
}

void
ev_window_title_set_document (EvWindowTitle *window_title,
			      EvDocument    *document)
{
	window_title->document = document;

	ev_window_title_update (window_title);
}

void
ev_window_title_set_uri (EvWindowTitle *window_title,
			 const char    *uri)
{
	g_free (window_title->uri);
	window_title->uri = g_strdup (uri);

	ev_window_title_update (window_title);
}

void
ev_window_title_free (EvWindowTitle *window_title)
{
	g_free (window_title->uri);
	g_free (window_title);
}
