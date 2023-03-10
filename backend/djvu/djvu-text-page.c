/*
 * Implements search and copy functionality for Djvu files.
 * Copyright (C) 2006 Michael Hofmann <mh21@piware.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>
#include <string.h>
#include <glib.h>
#include <libdjvu/miniexp.h>
#include "djvu-text-page.h"


/**
 * djvu_text_page_union:
 * @target: first rectangle and result
 * @source: second rectangle
 *
 * Calculates the bounding box of two rectangles and stores the result
 * in the first.
 */
static void
djvu_text_page_union (EvRectangle *target,
		      EvRectangle *source)
{
	if (source->x1 < target->x1)
		target->x1 = source->x1;
	if (source->x2 > target->x2)
		target->x2 = source->x2;
	if (source->y1 < target->y1)
		target->y1 = source->y1;
	if (source->y2 > target->y2)
		target->y2 = source->y2;
}

/**
 * djvu_text_page_selection_process_box:
 * @page: #DjvuTextPage instance
 * @p: s-expression to append bounding box of
 * @delimit: character/word/... delimiter
 *
 * Appends bounding box of the line containing miniexp_t to page->results
 *
 * Returns: whether the end was not reached in this s-expression
 */
static gboolean
djvu_text_page_selection_process_box (DjvuTextPage *page,
				      miniexp_t     p,
				      int           delimit)
{
	if (page->results || p == page->start) {
		EvRectangle box;
		const char *text;

		box.x1 = miniexp_to_int (miniexp_nth (1, p));
		box.y1 = miniexp_to_int (miniexp_nth (2, p));
		box.x2 = miniexp_to_int (miniexp_nth (3, p));
		box.y2 = miniexp_to_int (miniexp_nth (4, p));
		text = miniexp_to_str (miniexp_nth (5, p));

		if (text != NULL && text[0] != '\0') {
			if (!(delimit & 2) && page->results != NULL) {
				EvRectangle *union_box = (EvRectangle *)page->results->data;

                                /* If still on the same line, add box to union */
				djvu_text_page_union (union_box, &box);
			} else {
				/* A new line, a new box */
				page->results = g_list_prepend (page->results, ev_rectangle_copy (&box));
			}
		}

		if (p == page->end)
			return FALSE;
	}
	return TRUE;
}

/**
 * djvu_text_page_selection_process_text:
 * @page: #DjvuTextPage instance
 * @p: s-expression to append
 * @delimit: character/word/... delimiter
 *
 * Appends the string in @p to the page text.
 *
 * Returns: whether the end was not reached in this s-expression
 */
static gboolean
djvu_text_page_selection_process_text (DjvuTextPage *page,
                                       miniexp_t     p,
                                       int           delimit)
{
	if (page->text || p == page->start) {
		char *token_text = (char *) miniexp_to_str (miniexp_nth (5, p));
		if (page->text) {
			char *new_text =
			    g_strjoin (delimit & 2 ? "\n" :
			    	       delimit & 1 ? " " : NULL,
				       page->text, token_text,
				       NULL);
			g_free (page->text);
			page->text = new_text;
		} else
			page->text = g_strdup (token_text);
		if (p == page->end)
			return FALSE;
	}
	return TRUE;
}

/**
 * djvu_text_page_selection:
 * @page: #DjvuTextPage instance
 * @p: tree to append
 * @delimit: character/word/... delimiter
 *
 * Walks the tree in @p and appends the text or bounding boxes with
 * djvu_text_page_selection_process_{text|box}() for all s-expressions
 * between the start and end fields.
 *
 * Returns: whether the end was not reached in this subtree
 */
static gboolean
djvu_text_page_selection (DjvuSelectionType type,
			  DjvuTextPage *page,
			  miniexp_t     p,
			  int           delimit)
{
        miniexp_t deeper;

	g_return_val_if_fail (miniexp_consp (p) && miniexp_symbolp
			      (miniexp_car (p)), FALSE);

	if (miniexp_car (p) != page->char_symbol)
		delimit |= miniexp_car (p) == page->word_symbol ? 1 : 2;

	deeper = miniexp_cddr (miniexp_cdddr (p));
	while (deeper != miniexp_nil) {
		miniexp_t str = miniexp_car (deeper);
		if (miniexp_stringp (str)) {
			if (type == DJVU_SELECTION_TEXT) {
				if (!djvu_text_page_selection_process_text (page, p, delimit))
					return FALSE;
			} else {
				if (!djvu_text_page_selection_process_box (page, p, delimit))
					return FALSE;
			}
		} else {
			if (!djvu_text_page_selection (type, page, str, delimit))
				return FALSE;
		}
		delimit = 0;
		deeper = miniexp_cdr (deeper);
	}
	return TRUE;
}

static void
djvu_text_page_limits_process (DjvuTextPage *page,
			       miniexp_t     p,
			       EvRectangle  *rect)
{
	EvRectangle current;
	const char *text;

	current.x1 = miniexp_to_int (miniexp_nth (1, p));
	current.y1 = miniexp_to_int (miniexp_nth (2, p));
	current.x2 = miniexp_to_int (miniexp_nth (3, p));
	current.y2 = miniexp_to_int (miniexp_nth (4, p));
	text = miniexp_to_str (miniexp_nth (5, p));
	if (current.x2 >= rect->x1 && current.y1 <= rect->y2 &&
	    current.x1 <= rect->x2 && current.y2 >= rect->y1 &&
	    text != NULL && text[0] != '\0') {
	    	if (page->start == miniexp_nil)
	    		page->start = p;
	    	page->end = p;
	}
}


static void
djvu_text_page_limits (DjvuTextPage *page,
			  miniexp_t     p,
			  EvRectangle  *rect)
{
        miniexp_t deeper;

	g_return_if_fail (miniexp_consp (p) &&
			  miniexp_symbolp (miniexp_car (p)));

	deeper = miniexp_cddr (miniexp_cdddr (p));
	while (deeper != miniexp_nil) {
		miniexp_t str = miniexp_car (deeper);
		if (miniexp_stringp (str))
			djvu_text_page_limits_process (page, p, rect);
		else
			djvu_text_page_limits (page, str, rect);

		deeper = miniexp_cdr (deeper);
	}
}

/**
 * djvu_text_page_get_selection:
 * @page: #DjvuTextPage instance
 * @rectangle: #EvRectangle of the selection
 *
 * Returns: The bounding boxes of the selection
 */
GList *
djvu_text_page_get_selection_region (DjvuTextPage *page,
                                     EvRectangle  *rectangle)
{
	page->start = miniexp_nil;
	page->end = miniexp_nil;

	/* Get page->start and page->end filled from selection rectangle */
	djvu_text_page_limits (page, page->text_structure, rectangle);
	/* Fills page->results with the bouding boxes */
	djvu_text_page_selection (DJVU_SELECTION_BOX,
	                          page, page->text_structure, 0);

	return g_list_reverse (page->results);
}

char *
djvu_text_page_copy (DjvuTextPage *page,
		     EvRectangle  *rectangle)
{
	char* text;

	page->start = miniexp_nil;
	page->end = miniexp_nil;
	djvu_text_page_limits (page, page->text_structure, rectangle);
	djvu_text_page_selection (DJVU_SELECTION_TEXT, page,
	                          page->text_structure, 0);

	/* Do not free the string */
	text = page->text;
	page->text = NULL;

	return text;
}

/**
 * djvu_text_page_position:
 * @page: #DjvuTextPage instance
 * @position: index in the page text
 *
 * Returns the closest s-expression that contains the given position in
 * the page text.
 *
 * Returns: closest s-expression
 */
static miniexp_t
djvu_text_page_position (DjvuTextPage *page,
			 int           position)
{
	GArray *links = page->links;
	int low = 0;
	int hi = links->len - 1;
	int mid = 0;

	g_return_val_if_fail (hi >= 0, miniexp_nil);

	/* Shamelessly copied from GNU classpath */
	while (low <= hi) {
                DjvuTextLink *link;

		mid = (low + hi) >> 1;
		link = &g_array_index (links, DjvuTextLink, mid);
		if (link->position == position)
			break;
		else if (link->position > position)
			hi = --mid;
		else
			low = mid + 1;
	}

	return g_array_index (page->links, DjvuTextLink, mid).pair;
}

/**
 * djvu_text_page_sexpr_process:
 * @page: #DjvuTextPage instance
 * @p: s-expression to append
 * @start: first s-expression in the selection
 * @end: last s-expression in the selection
 *
 * Appends the rectangle defined by @p to the internal bounding box rectangle.
 *
 * Returns: whether the end was not reached in this s-expression
 */
static gboolean
djvu_text_page_sexpr_process (DjvuTextPage *page,
                              miniexp_t     p,
                              miniexp_t     start,
                              miniexp_t     end)
{
	if (page->bounding_box || p == start) {
		EvRectangle *new_rectangle = ev_rectangle_new ();
		new_rectangle->x1 = miniexp_to_int (miniexp_nth (1, p));
		new_rectangle->y1 = miniexp_to_int (miniexp_nth (2, p));
		new_rectangle->x2 = miniexp_to_int (miniexp_nth (3, p));
		new_rectangle->y2 = miniexp_to_int (miniexp_nth (4, p));
		if (page->bounding_box) {
			djvu_text_page_union (page->bounding_box,
					      new_rectangle);
			g_free (new_rectangle);
		} else
			page->bounding_box = new_rectangle;
		if (p == end)
			return FALSE;
	}
	return TRUE;
}

/**
 * djvu_text_page_sexpr:
 * @page: #DjvuTextPage instance
 * @p: tree to append
 * @start: first s-expression in the selection
 * @end: last s-expression in the selection
 *
 * Walks the tree in @p and extends the rectangle with
 * djvu_text_page_process() for all s-expressions between @start and @end.
 *
 * Returns: whether the end was not reached in this subtree
 */
static gboolean
djvu_text_page_sexpr (DjvuTextPage *page,
		      miniexp_t p,
		      miniexp_t start,
		      miniexp_t end)
{
        miniexp_t deeper;

	g_return_val_if_fail (miniexp_consp (p) && miniexp_symbolp
			      (miniexp_car (p)), FALSE);

	deeper = miniexp_cddr (miniexp_cdddr (p));
	while (deeper != miniexp_nil) {
		miniexp_t str = miniexp_car (deeper);
		if (miniexp_stringp (str)) {
			if (!djvu_text_page_sexpr_process
			    (page, p, start, end))
				return FALSE;
		} else {
			if (!djvu_text_page_sexpr
			    (page, str, start, end))
				return FALSE;
		}
		deeper = miniexp_cdr (deeper);
	}
	return TRUE;
}

/**
 * djvu_text_page_box:
 * @page: #DjvuTextPage instance
 * @start: first s-expression in the selection
 * @end: last s-expression in the selection
 *
 * Builds a rectangle that contains all s-expressions in the given range.
 */
static EvRectangle *
djvu_text_page_box (DjvuTextPage *page,
		    miniexp_t     start,
		    miniexp_t     end)
{
	page->bounding_box = NULL;
	djvu_text_page_sexpr (page, page->text_structure, start, end);
	return page->bounding_box;
}

/**
 * djvu_text_page_append_search:
 * @page: #DjvuTextPage instance
 * @p: tree to append
 * @case_sensitive: do not ignore case
 * @delimit: insert spaces because of higher (sentence/paragraph/...) break
 *
 * Appends the tree in @p to the internal text string.
 */
static void
djvu_text_page_append_text (DjvuTextPage *page,
			    miniexp_t     p,
			    gboolean      case_sensitive,
			    gboolean      delimit)
{
	char *token_text;
	miniexp_t deeper;

	g_return_if_fail (miniexp_consp (p) &&
			  miniexp_symbolp (miniexp_car (p)));

	delimit |= page->char_symbol != miniexp_car (p);

	deeper = miniexp_cddr (miniexp_cdddr (p));
	while (deeper != miniexp_nil) {
		miniexp_t data = miniexp_car (deeper);
		if (miniexp_stringp (data)) {
			DjvuTextLink link;
			link.position = page->text == NULL ? 0 :
			    strlen (page->text);
			link.pair = p;
			g_array_append_val (page->links, link);

			token_text = (char *) miniexp_to_str (data);
			if (!case_sensitive)
				token_text = g_utf8_casefold (token_text, -1);
			if (page->text == NULL)
				page->text = g_strdup (token_text);
			else {
				char *new_text =
				    g_strjoin (delimit ? " " : NULL,
					       page->text, token_text,
					       NULL);
				g_free (page->text);
				page->text = new_text;
			}
			if (!case_sensitive)
				g_free (token_text);
		} else
			djvu_text_page_append_text (page, data,
						    case_sensitive, delimit);
		delimit = FALSE;
		deeper = miniexp_cdr (deeper);
	}
}

/**
 * djvu_text_page_search:
 * @page: #DjvuTextPage instance
 * @text: text to search
 *
 * Searches the page for the given text. The results list has to be
 * externally freed afterwards.
 */
void
djvu_text_page_search (DjvuTextPage *page,
		       const char   *text)
{
	char *haystack = page->text;
	int search_len;
	EvRectangle *result;
	if (page->links->len == 0)
		return;

	search_len = strlen (text);
	while ((haystack = strstr (haystack, text)) != NULL) {
		int start_p = haystack - page->text;
		miniexp_t start = djvu_text_page_position (page, start_p);
		int end_p = start_p + search_len - 1;
		miniexp_t end = djvu_text_page_position (page, end_p);
		result = djvu_text_page_box (page, start, end);
		g_assert (result);
		page->results = g_list_prepend (page->results, result);
		haystack = haystack + search_len;
	}
	page->results = g_list_reverse (page->results);
}


/**
 * djvu_text_page_index_text:
 * @page: #DjvuTextPage instance
 * @case_sensitive: do not ignore case
 *
 * Indexes the page text and prepares the page for subsequent searches.
 */
void
djvu_text_page_index_text (DjvuTextPage *page,
	       		       gboolean      case_sensitive)
{
	djvu_text_page_append_text (page, page->text_structure,
				    case_sensitive, FALSE);
}

/**
 * djvu_text_page_new:
 * @text: S-expression of the page text
 *
 * Creates a new page to search.
 *
 * Returns: new #DjvuTextPage instance
 */
DjvuTextPage *
djvu_text_page_new (miniexp_t text)
{
	DjvuTextPage *page;

	page = g_new0 (DjvuTextPage, 1);
	page->links = g_array_new (FALSE, FALSE, sizeof (DjvuTextLink));
	page->char_symbol = miniexp_symbol ("char");
	page->word_symbol = miniexp_symbol ("word");
	page->text_structure = text;
	return page;
}

/**
 * djvu_text_page_free:
 * @page: #DjvuTextPage instance
 *
 * Frees the given #DjvuTextPage instance.
 */
void
djvu_text_page_free (DjvuTextPage *page)
{
	g_free (page->text);
	g_array_free (page->links, TRUE);
	g_free (page);
}
