/* 
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

#ifndef __DJVU_TEXT_PAGE_H__
#define __DJVU_TEXT_PAGE_H__

#include "ev-document.h"

#include <string.h>
#include <glib.h>
#include <libdjvu/miniexp.h>


typedef struct _DjvuTextPage DjvuTextPage;
typedef struct _DjvuTextLink DjvuTextLink;

struct _DjvuTextPage {
	GString *text;
	GArray *links;
	GList *results;
	miniexp_t char_symbol;
	miniexp_t word_symbol;
	GList *lines;
	miniexp_t text_structure;
	miniexp_t start;
	miniexp_t end;
	GArray *utf8_map;
	int utf8_count;
};

struct _DjvuTextLink {
	int position;
	miniexp_t pair;
};

typedef enum {
	DJVU_SELECTION_TEXT,
	DJVU_SELECTION_BOX,
} DjvuSelectionType;

GList        *djvu_text_page_get_selection_region (DjvuTextPage *page,
                                                   EvRectangle  *rectangle);
char         *djvu_text_page_copy                 (DjvuTextPage *page,
                                                   EvRectangle  *rectangle);
void          djvu_text_page_index_text           (DjvuTextPage *page,
                                                   gboolean      case_sensitive);
GPtrArray    *djvu_text_page_search               (DjvuTextPage *page,
                                                   const char   *text,
                                                   gdouble       height,
                                                   gdouble       dpi);
DjvuTextPage *djvu_text_page_new                  (miniexp_t     text);
void          djvu_text_page_free                 (DjvuTextPage *page);

#endif /* __DJVU_TEXT_PAGE_H__ */

