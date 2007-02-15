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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <string.h>
#include <glib.h>
#include <libdjvu/miniexp.h>

#include "djvu-document-private.h"
#include "djvu-document.h"
#include "djvu-text.h"
#include "djvu-text-page.h"
#include "ev-document-find.h"
#include "ev-document.h"



struct _DjvuText {
	DjvuDocument *document;
	gboolean case_sensitive;
	char *text;
	GList **pages;
	guint idle;
	int start_page;
	int search_page;
};

/**
 * djvu_text_idle_callback:
 * @data: #DjvuText instance
 * 
 * Idle callback that processes one page at a time.
 * 
 * Returns: whether there are more pages to be processed
 */
static gboolean 
djvu_text_idle_callback (void *data)
{
	DjvuText *djvu_text = (DjvuText *) data;
	DjvuDocument *djvu_document = djvu_text->document;
	int n_pages;
	miniexp_t page_text;

	ev_document_doc_mutex_lock ();
	while ((page_text =
		ddjvu_document_get_pagetext (djvu_document->d_document,
					     djvu_text->search_page,
					     "char")) == miniexp_dummy)
		djvu_handle_events (djvu_document, TRUE);

	if (page_text != miniexp_nil) {
		DjvuTextPage *page = djvu_text_page_new (page_text);
		djvu_text_page_prepare_search (page, djvu_text->case_sensitive); 
		if (page->links->len > 0) {
			djvu_text_page_search (page, djvu_text->text);
			djvu_text->pages[djvu_text->search_page] = page->results;
			ev_document_find_changed (EV_DOCUMENT_FIND
						  (djvu_document),
						  djvu_text->search_page);
		}
		djvu_text_page_free (page);
		ddjvu_miniexp_release (djvu_document->d_document,
				       page_text);
	}
	ev_document_doc_mutex_unlock ();

	n_pages =
	    djvu_document_get_n_pages (EV_DOCUMENT (djvu_text->document));
	djvu_text->search_page += 1;
	if (djvu_text->search_page == n_pages) {
		/* wrap around */
		djvu_text->search_page = 0;
	}

	if (djvu_text->search_page != djvu_text->start_page)
		return TRUE;

	/* We're done. */
	djvu_text->idle = 0;
	/* will return FALSE to remove */
	return FALSE;
}

/**
 * djvu_text_new:
 * @djvu_document: document to search
 * @start_page: first page to search
 * @case_sensitive: uses g_utf8_case_fold() to enable case-insensitive 
 * 	searching
 * @text: text to search
 * 
 * Creates a new #DjvuText instance to enable searching. An idle call
 * is used to process all pages starting from @start_page.
 * 
 * Returns: newly created instance
 */
DjvuText *
djvu_text_new (DjvuDocument *djvu_document,
	       int           start_page,
	       gboolean      case_sensitive, 
	       const char   *text)
{
	DjvuText *djvu_text;
	int n_pages;
	int i;

	n_pages = djvu_document_get_n_pages (EV_DOCUMENT (djvu_document));

	djvu_text = g_new0 (DjvuText, 1);

	if (case_sensitive)
		djvu_text->text = g_strdup (text);
	else
		djvu_text->text = g_utf8_casefold (text, -1);
	djvu_text->pages = g_new0 (GList *, n_pages);
	for (i = 0; i < n_pages; i++) {
		djvu_text->pages[i] = NULL;
	}

	djvu_text->document = djvu_document;

	/* We add at low priority so the progress bar repaints */
	djvu_text->idle = g_idle_add_full (G_PRIORITY_LOW,
					djvu_text_idle_callback,
					djvu_text, NULL);

	djvu_text->case_sensitive = case_sensitive;
	djvu_text->start_page = start_page;
	djvu_text->search_page = start_page;

	return djvu_text;
}

/**
 * djvu_text_copy:
 * @djvu_document: document to search
 * @page: page to search
 * @rectangle: rectangle to copy
 * 
 * Copies and returns the text in the given rectangle.
 * 
 * Returns: newly allocated text or NULL of none is available
 */
char *
djvu_text_copy (DjvuDocument *djvu_document,
       	        int           page,
	        EvRectangle  *rectangle)
{
	miniexp_t page_text;
	char* text = NULL;

	while ((page_text =
		ddjvu_document_get_pagetext (djvu_document->d_document,
					     page, "char")) == miniexp_dummy)
		djvu_handle_events (djvu_document, TRUE);

	if (page_text != miniexp_nil) {
		DjvuTextPage *page = djvu_text_page_new (page_text);
		text = djvu_text_page_copy (page, rectangle);
		djvu_text_page_free (page);
		ddjvu_miniexp_release (djvu_document->d_document, page_text);
	}
	
	return text;
}

/**
 * djvu_text_free:
 * @djvu_text: instance to free
 * 
 * Frees the given #DjvuText instance.
 */
void djvu_text_free (DjvuText * djvu_text)
{
	DjvuDocument *djvu_document = djvu_text->document;
	int n_pages;
	int i;

	if (djvu_text->idle != 0)
		g_source_remove (djvu_text->idle);

	n_pages = djvu_document_get_n_pages (EV_DOCUMENT (djvu_document));
	for (i = 0; i < n_pages; i++) {
		g_list_foreach (djvu_text->pages[i], (GFunc) g_free, NULL);
		g_list_free (djvu_text->pages[i]);
	}

	g_free (djvu_text->text);
}

/**
 * djvu_text_get_text:
 * @djvu_text: #DjvuText instance
 * 
 * Returns the search text. This is mainly to be able to avoid reinstantiation 
 * for the same search text.
 * 
 * Returns: the text this instance of #DjvuText is looking for
 */
const char *
djvu_text_get_text (DjvuText *djvu_text)
{
	return djvu_text->text;
}

/**
 * djvu_text_n_results:
 * @djvu_text: #DjvuText instance
 * @page: page number
 * 
 * Returns the number of search results available for the given page.
 * 
 * Returns: number of search results
 */
int 
djvu_text_n_results (DjvuText *djvu_text, 
		     int       page)
{
	return g_list_length (djvu_text->pages[page]);
}

/**
 * djvu_text_has_results:
 * @djvu_text: #DjvuText instance
 * @page: page number
 * 
 * Returns whether there are search results available for the given page.
 * This method executes faster than djvu_text_n_results().
 * 
 * Returns: whether there are search results
 */
int 
djvu_text_has_results (DjvuText *djvu_text, 
                       int       page)
{
	return djvu_text->pages[page] != NULL;
}

/**
 * djvu_text_get_result:
 * @djvu_text: #DjvuText instance
 * @page: page number
 * @n_result: result number
 * 
 * Returns the n-th search result of a given page. The coordinates are 
 * Djvu-specific and need to be processed to be compatible with the Evince
 * coordinate system. The result may span several lines!
 * 
 * Returns: the rectangle for the search result
 */
EvRectangle *
djvu_text_get_result (DjvuText *djvu_text, 
                      int       page,
		      int       n_result)
{
	return (EvRectangle *) g_list_nth_data (djvu_text->pages[page],
						n_result);
}

/**
 * djvu_text_get_progress:
 * @djvu_text: #DjvuText instance
 * 
 * Returns the percentage of pages done searching.
 * 
 * Returns: the progress as value between 0 and 1
 */
double
djvu_text_get_progress (DjvuText *djvu_text)
{
	int pages_done;
	int n_pages;

	n_pages =
	    djvu_document_get_n_pages (EV_DOCUMENT (djvu_text->document));
	if (djvu_text->search_page > djvu_text->start_page) {
		pages_done = djvu_text->search_page - djvu_text->start_page + 1;
	} else if (djvu_text->search_page == djvu_text->start_page) {
		pages_done = n_pages;
	} else {
		pages_done =
		    n_pages - djvu_text->start_page + djvu_text->search_page;
	}
	return pages_done / (double) n_pages;
}

