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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __DJVU_TEXT_H__
#define __DJVU_TEXT_H__

#include "ev-document.h"

#include <glib.h>

typedef struct _DjvuText DjvuText;

DjvuText    *djvu_text_new          (DjvuDocument *djvu_document,
			             int           start_page,
			             gboolean      case_sensitive, 
			             const char   *text);
const char  *djvu_text_get_text     (DjvuText     *djvu_text);
int          djvu_text_n_results    (DjvuText     *djvu_text, 
				     int           page);
EvRectangle *djvu_text_get_result   (DjvuText     *djvu_text, 
				     int           page,
				     int           n_result);
int          djvu_text_has_results  (DjvuText     *djvu_text, 
                                     int           page);
double       djvu_text_get_progress (DjvuText     *djvu_text);
char 	    *djvu_text_copy 	    (DjvuDocument *djvu_document,
       	        		     int           page,
	        		     EvRectangle  *rectangle);

#endif /* __DJVU_TEXT_H__ */
