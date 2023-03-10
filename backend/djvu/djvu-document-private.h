/*
 * Declarations used throughout the djvu classes
 *
 * Copyright (C) 2006, Michael Hofmann <mh21@piware.de>
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

#pragma once

#include "djvu-document.h"

#include <libdjvu/ddjvuapi.h>

struct _DjvuDocument {
	EvDocument        parent_instance;

	ddjvu_context_t  *d_context;
	ddjvu_document_t *d_document;
	ddjvu_format_t   *d_format;
	ddjvu_format_t   *thumbs_format;

	gchar            *uri;

        /* PS exporter */
        gchar		 *ps_filename;
        GString 	 *opts;
	ddjvu_fileinfo_t *fileinfo_pages;
	gint		  n_pages;
	GHashTable	 *file_ids;
};

int  djvu_document_get_n_pages (EvDocument   *document);
void djvu_handle_events        (DjvuDocument *djvu_document,
			        int           wait,
				GError      **error);
