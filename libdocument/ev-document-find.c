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

/**
 * EvDocumentFindMatch:
 *
 * #EvDocumentFindMatch is the result of a find operation.
 *
 * Since: 3.34
 */
struct _EvDocumentFindMatch
{
	GList *area;
	gsize  start_offset;
	gsize  end_offset;
};

static EvDocumentFindMatch *_ev_document_find_match_copy (EvDocumentFindMatch *match);

G_DEFINE_INTERFACE (EvDocumentFind, ev_document_find, 0)
G_DEFINE_BOXED_TYPE (EvDocumentFindMatch, ev_document_find_match, _ev_document_find_match_copy, ev_document_find_match_free)

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

	if (iface->find_text)
		return iface->find_text (document_find, page, text, case_sensitive);

	return NULL;
}

/**
 * get_match_offset:
 * @areas: an array of rectangles to search through
 * @n_areas: number of rectangles
 * @match: rectangle to search for
 * @offset: offset in @areas to start searching at
 *
 * Find which rectangle in @areas matches the location of @match.
 *
 * Returns: index of the matching rectangle
 */
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
 * Searches for @text in @page of the document.
 *
 * Returns: (transfer container) (element-type EvDocumentFindMatch): a list of
 *          #EvDocumentFindMatch results
 *
 * Since: 3.34
 */
GPtrArray *
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
	GPtrArray    *match_array = NULL;
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
		g_list_free_full (list, (GDestroyNotify) ev_rectangle_free);
		return NULL;
	}

	match_array = g_ptr_array_new_with_free_func ((GDestroyNotify) ev_document_find_match_free);

	for (l = list, result = 0; l; l = g_list_next (l), result++) {
		EvRectangle         *rect_match = (EvRectangle *)l->data;
		EvDocumentFindMatch *off_match;
		gint                 end_offset;
		GList               *area_list;

		start_offset = get_match_offset (areas, n_areas, rect_match, start_offset);
		end_offset = start_offset + g_utf8_strlen (text, -1);

		if (start_offset == -1 || end_offset > n_areas) {
			g_warning ("No offset found for match \"%s\" at page "
				   "%d after processing %d results\n",
				   text, page->index, result);
			break;
		}

		area_list = g_list_prepend (NULL, rect_match);
		off_match = ev_document_find_match_new (area_list,
				                        start_offset,
							end_offset);
		g_ptr_array_add (match_array, off_match);
	}

	g_list_free (list);
	return match_array;
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

/**
 * ev_document_find_match_new: (skip)
 * @area: (transfer full): a list of #EvRrectangle. The #EvDocumentFindMatch
 *     takes ownership of the list.
 * @start_offset: starting offset into the page's text
 * @end_offset: ending offset (one past the end) into the page's text
 *
 * Creates a new #EvDocumentFindMatch.
 *
 * Returns: a new #EvDocumentFindMatch
 *
 * Since: 3.34
 * Stability: Private
 */
EvDocumentFindMatch *
ev_document_find_match_new (GList *area,
			    gsize  start_offset,
			    gsize  end_offset)
{
	EvDocumentFindMatch *m = g_slice_new (EvDocumentFindMatch);
	m->area = area;
	m->start_offset = start_offset;
	m->end_offset = end_offset;
	return m;
}

/**
 * ev_document_find_match_copy:
 * @match: an #EvDocumentFindMatch
 *
 * Copies a #EvDocumentFindMatch.
 *
 * Returns: a copy of @match
 *
 * Since: 3.34
 */
static EvDocumentFindMatch *
_ev_document_find_match_copy (EvDocumentFindMatch *match)
{
	GList *area = g_list_copy_deep (match->area,
					(GCopyFunc) ev_rectangle_copy,
					NULL);
	return ev_document_find_match_new (area,
					   match->start_offset,
					   match->end_offset);
}

/**
 * ev_document_find_match_free: (skip)
 * @match: an #EvDocumentFindMatch
 *
 * Frees the memory allocated for @match.
 *
 * Since: 3.34
 */
void
ev_document_find_match_free (EvDocumentFindMatch *match)
{
	g_list_free_full (match->area, (GDestroyNotify) ev_rectangle_free);
	g_slice_free (EvDocumentFindMatch, match);
}

/**
 * ev_document_find_match_get_area:
 * @match: an #EvDocumentFindMatch
 *
 * Returns the location of @match on the page as a list of #EvRectangle.
 *
 * Returns: (transfer none) (element-type EvRectangle): a list of #EvRectangle
 *
 * Since: 3.34
 */
GList *
ev_document_find_match_get_area (EvDocumentFindMatch *match)
{
	return match->area;
}

/**
 * ev_document_find_match_get_start_offset:
 * @match: an #EvDocumentFindMatch
 *
 * Returns the starting offset of @match into the page's text.
 *
 * Returns: the starting offset into the page's text
 *
 * Since: 3.34
 */
gsize
ev_document_find_match_get_start_offset (EvDocumentFindMatch *match)
{
	return match->start_offset;
}

/**
 * ev_document_find_match_get_end_offset:
 * @match: an #EvDocumentFindMatch
 *
 * Returns the ending offset (one past the end) of @match into the page's text.
 *
 * Returns: the ending offset into the page's text
 *
 * Since: 3.34
 */
gsize
ev_document_find_match_get_end_offset (EvDocumentFindMatch *match)
{
	return match->end_offset;
}
