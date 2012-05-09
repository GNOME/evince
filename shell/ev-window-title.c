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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>
#include "ev-window-title.h"
#include "ev-utils.h"

#include <string.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

/* Known backends (for bad extensions fix) */
#define EV_BACKEND_PS  "PSDocument"
#define EV_BACKEND_PDF "PdfDocument"

typedef struct
{
	const gchar *backend;
	const gchar *text;
} BadTitleEntry;

struct _EvWindowTitle
{
	EvWindow *window;
	EvWindowTitleType type;
	EvDocument *document;
	char *uri;
};

static const BadTitleEntry bad_extensions[] = {
	{ EV_BACKEND_PS, ".dvi" },
	{ EV_BACKEND_PDF, ".doc" },
	{ EV_BACKEND_PDF, ".dvi" },
	{ EV_BACKEND_PDF, ".indd" },
	{ EV_BACKEND_PDF, ".rtf" }
};

static const BadTitleEntry bad_prefixes[] = {
	{ EV_BACKEND_PDF, "Microsoft Word - " },
	{ EV_BACKEND_PDF, "Microsoft PowerPoint - " }
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
	char *basename;
	
	filename = g_uri_unescape_string (uri, NULL);
	basename = g_path_get_basename (filename);
	g_free(filename);

	return basename;
}

/* Some docs report titles with confusing extensions (ex. .doc for pdf).
	   Erase the confusing extension of the title */
static void
ev_window_title_sanitize_title (EvWindowTitle *window_title, char **title) {
	const gchar *backend;
	int i;

	backend = G_OBJECT_TYPE_NAME (window_title->document);

	for (i = 0; i < G_N_ELEMENTS (bad_extensions); i++) {
		if (g_ascii_strcasecmp (bad_extensions[i].backend, backend) == 0 && 
		    g_str_has_suffix (*title, bad_extensions[i].text)) {
			char *new_title;
			char *filename = get_filename_from_uri (window_title->uri);

			new_title = g_strndup (*title, strlen(*title) - strlen(bad_extensions[i].text));
			g_free (*title);
			*title = new_title;

			g_free (filename);
		}
	}
	for (i = 0; i < G_N_ELEMENTS (bad_prefixes); i++) {
		if (g_ascii_strcasecmp (bad_prefixes[i].backend, backend) == 0 &&
		    g_str_has_prefix (*title, bad_prefixes[i].text)) {
			char *new_title;
			int len = strlen(bad_prefixes[i].text);
			
			new_title = g_strdup_printf ("%s", (*title) + len);
			g_free (*title);
			*title = new_title;
		}
	}
}

static void
ev_window_title_update (EvWindowTitle *window_title)
{
	GtkWindow *window = GTK_WINDOW (window_title->window);
	char *title = NULL, *password_title, *p;

	if (window_title->document != NULL) {
		gchar *doc_title;

		doc_title = g_strdup (ev_document_get_title (window_title->document));

		/* Make sure we get a valid title back */
		if (doc_title != NULL) {
			doc_title = g_strstrip (doc_title);

			if (doc_title[0] != '\0' &&
			    g_utf8_validate (doc_title, -1, NULL)) {
				title = g_strdup (doc_title);
			}

			g_free (doc_title);
		}
	}

	if (title && window_title->uri) {
		char *tmp_title;
		char *filename = get_filename_from_uri (window_title->uri);

		ev_window_title_sanitize_title (window_title, &title);
		tmp_title = g_strdup_printf ("%s — %s", filename, title);
                g_free (title);
                g_free (filename);

                title = tmp_title;
	} else if (window_title->uri) {
		title = get_filename_from_uri (window_title->uri);
	} else if (!title) {
		title = g_strdup (_("Document Viewer"));
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
		password_title = g_strdup_printf (_("%s — Password Required"), title);
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
