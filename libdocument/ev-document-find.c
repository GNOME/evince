/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/*
 *  Copyright (C) 2004 Red Hat, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include "ev-document-find.h"
#include "ev-document-text.h"

G_DEFINE_INTERFACE (EvDocumentFind, ev_document_find, 0)

static void
ev_document_find_default_init (EvDocumentFindInterface *klass)
{
}

/**
 * ev_document_find_find_text:
 * @document_find: an #EvDocumentFind
 * @page: an #EvPage
 * @text: text to find
 * @case_sensitive: whether to match the string case
 *
 * Returns: (transfer full) (element-type EvRectangle): a list of results
 */
GList *
ev_document_find_find_text (EvDocumentFind *document_find,
			    EvPage         *page,
			    const gchar    *text,
			    gboolean        case_sensitive)
{
	EvDocumentFindInterface *iface = EV_DOCUMENT_FIND_GET_IFACE (document_find);
	
	return iface->find_text (document_find, page, text, case_sensitive);
}

static gint
get_match_offset (EvRectangle *areas,
                  guint        n_areas,
                  EvRectangle *match,
                  gint         offset)
{
        gdouble x, y;
        gint i;

        x = match->x1;
        y = (match->y1 + match->y2) / 2;

        i = offset;

        do {
                EvRectangle *area = areas + i;

                if (x >= area->x1 && x < area->x2 &&
                    y >= area->y1 && y <= area->y2) {
                        return i;
                }

                i = (i + 1) % n_areas;
        } while (i != offset);

        return -1;
}

/**
 * ev_document_find_find_text_offset:
 * @document_find: an #EvDocumentFind
 * @page: an #EvPage
 * @text: text to find
 * @options: a set of #EvFindOptions
 *
 * Returns: (transfer full) (element-type EvDocumentFindMatch): a list of results
 */
GList *
ev_document_find_find_text_offset (EvDocumentFind *document_find,
			           EvPage         *page,
			           const gchar    *text,
				   EvFindOptions   options)
{
	EvDocumentFindInterface *iface = EV_DOCUMENT_FIND_GET_IFACE (document_find);
	gint          result;
	EvRectangle  *areas = NULL;
	guint         n_areas;
	gboolean      success;
	GList        *list, *l;
	GList        *out_list = NULL;
	gint          start_offset = 0;

	if (iface->find_text_offset)
		return iface->find_text_offset (document_find, page, text, options);

	/* Backend didn't implement find_text_offset. Fallback to
	 * find_text_with_options and convert the result */
	list = ev_document_find_find_text_with_options (document_find, page, text, options);
	if (!list) {
		return NULL;
	}

	success = ev_document_text_get_text_layout (EV_DOCUMENT_TEXT (document_find),
		                                    page, &areas, &n_areas);
	if (!success) {
		g_list_free_full (list, (GDestroyNotify)ev_rectangle_free);
		return NULL;
	}

	for (l = list, result = 0; l; l = g_list_next (l), result++) {
		EvRectangle         *rect_match = (EvRectangle *)l->data;
		EvDocumentFindMatch *off_match;
		gint                 end_offset;

		start_offset = get_match_offset (areas, n_areas, rect_match, start_offset);
		end_offset = start_offset + g_utf8_strlen (text, -1);

		if (start_offset == -1) {
			g_warning ("No offset found for match \"%s\" at page "
				   "%d after processing %d results\n",
			text, page->index, result);
			break;
		}

		off_match = ev_document_find_match_new ();
		off_match->area = *(EvRectangle *)l->data;
		off_match->start_offset = start_offset;
		off_match->end_offset = end_offset;
		out_list = g_list_prepend (out_list, off_match);
	}

	return g_list_reverse (out_list);
}

/**
 * ev_document_find_find_text_with_options:
 * @document_find: an #EvDocumentFind
 * @page: an #EvPage
 * @text: text to find
 * @options: a set of #EvFindOptions
 *
 * Returns: (transfer full) (element-type EvRectangle): a list of results
 */
GList *
ev_document_find_find_text_with_options (EvDocumentFind *document_find,
					 EvPage         *page,
					 const gchar    *text,
					 EvFindOptions   options)
{
	EvDocumentFindInterface *iface = EV_DOCUMENT_FIND_GET_IFACE (document_find);

	if (iface->find_text_with_options)
		return iface->find_text_with_options (document_find, page, text, options);

	return ev_document_find_find_text (document_find, page, text, options & EV_FIND_CASE_SENSITIVE);
}

EvFindOptions
ev_document_find_get_supported_options (EvDocumentFind *document_find)
{
	EvDocumentFindInterface *iface = EV_DOCUMENT_FIND_GET_IFACE (document_find);

	if (iface->get_supported_options)
		return iface->get_supported_options (document_find);
	return 0;
}

EvDocumentFindMatch *
ev_document_find_match_new  (void)
{
	return g_slice_new (EvDocumentFindMatch);
}

void
ev_document_find_match_free (EvDocumentFindMatch *match)
{
	g_slice_free (EvDocumentFindMatch, match);
}
