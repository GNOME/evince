/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2005 Red Hat, Inc
 *  Copyright (C) 2022 Qiu Wenbo <qiuwenbo@kylinos.com.cn>
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

#pragma once

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#include <evince-document.h>

G_BEGIN_DECLS

typedef enum {
	TITLE_PROPERTY,
	URI_PROPERTY,
	SUBJECT_PROPERTY,
	AUTHOR_PROPERTY,
	KEYWORDS_PROPERTY,
	PRODUCER_PROPERTY,
	CREATOR_PROPERTY,
	CREATION_DATE_PROPERTY,
	MOD_DATE_PROPERTY,
	N_PAGES_PROPERTY,
	LINEARIZED_PROPERTY,
	FORMAT_PROPERTY,
	SECURITY_PROPERTY,
	CONTAINS_JS_PROPERTY,
	PAPER_SIZE_PROPERTY,
	FILE_SIZE_PROPERTY,
	N_PROPERTIES,
} Property;

typedef struct {
	Property property;
	const char *label;
} PropertyInfo;

static const PropertyInfo properties_info[] = {
	{ TITLE_PROPERTY,         N_("Title") },
	{ URI_PROPERTY,           N_("Location") },
	{ SUBJECT_PROPERTY,       N_("Subject") },
	{ AUTHOR_PROPERTY,        N_("Author") },
	{ KEYWORDS_PROPERTY,      N_("Keywords") },
	{ PRODUCER_PROPERTY,      N_("Producer") },
	{ CREATOR_PROPERTY,       N_("Creator") },
	{ CREATION_DATE_PROPERTY, N_("Created") },
	{ MOD_DATE_PROPERTY,      N_("Modified") },
	{ N_PAGES_PROPERTY,       N_("Number of Pages") },
	{ LINEARIZED_PROPERTY,    N_("Optimized") },
	{ FORMAT_PROPERTY,        N_("Format") },
	{ SECURITY_PROPERTY,      N_("Security") },
	{ CONTAINS_JS_PROPERTY,   N_("Contains Javascript") },
	{ PAPER_SIZE_PROPERTY,    N_("Paper Size") },
	{ FILE_SIZE_PROPERTY,     N_("Size") }
};

char *		ev_regular_paper_size			(const EvDocumentInfo *info);

G_END_DECLS
