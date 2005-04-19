/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/*
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef EV_DOCUMENT_INFO_H
#define EV_DOCUMENT_INFO_H

#include <glib-object.h>
#include <glib.h>
#include "ev-link.h"

G_BEGIN_DECLS

typedef struct _EvDocumentInfo    EvDocumentInfo;

typedef enum
{
	EV_DOCUMENT_LAYOUT_SINGLE_PAGE,
	EV_DOCUMENT_LAYOUT_ONE_COLUMN,
	EV_DOCUMENT_LAYOUT_TWO_COLUMN_LEFT,
	EV_DOCUMENT_LAYOUT_TWO_COLUMN_RIGHT,
	EV_DOCUMENT_LAYOUT_TWO_PAGE_LEFT,
	EV_DOCUMENT_LAYOUT_TWO_PAGE_RIGHT,
} EvDocumentLayout;

typedef enum
{
	EV_DOCUMENT_MODE_NONE,
	EV_DOCUMENT_MODE_USE_OC,
	EV_DOCUMENT_MODE_USE_THUMBS,
	EV_DOCUMENT_MODE_FULL_SCREEN,
	EV_DOCUMENT_MODE_USE_ATTACHMENTS,
	EV_DOCUMENT_MODE_PRESENTATION = EV_DOCUMENT_MODE_FULL_SCREEN /* Will these be different? */
} EvDocumentMode;

typedef enum
{
	EV_DOCUMENT_UI_HINT_HIDE_TOOLBAR = 1 << 0,
	EV_DOCUMENT_UI_HINT_HIDE_MENUBAR = 1 << 1,
	EV_DOCUMENT_UI_HINT_HIDE_WINDOWUI = 1 << 2,
	EV_DOCUMENT_UI_HINT_FIT_WINDOW = 1 << 3,
	EV_DOCUMENT_UI_HINT_CENTER_WINDOW = 1 << 4,
	EV_DOCUMENT_UI_HINT_DISPLAY_DOC_TITLE = 1 << 5,
	EV_DOCUMENT_UI_HINT_DIRECTION_RTL = 1 << 6,
} EvDocumentUIHints;

typedef enum
{
	EV_DOCUMENT_INFO_TITLE = 1 << 0,
	EV_DOCUMENT_INFO_FORMAT = 1 << 1,
	EV_DOCUMENT_INFO_AUTHOR = 1 << 2,
	EV_DOCUMENT_INFO_SUBJECT = 1 << 3,
	EV_DOCUMENT_INFO_KEYWORDS = 1 << 4,
	EV_DOCUMENT_INFO_LAYOUT = 1 << 5,
	EV_DOCUMENT_INFO_START_MODE = 1 << 6,
	EV_DOCUMENT_INFO_CREATION_DATE = 1 << 7,
	EV_DOCUMENT_INFO_UI_HINTS = 1 << 8,
} EvDocumentInfoFields;

struct _EvDocumentInfo
{
	const char *title;
	const char *format; /* eg, "pdf-1.5" */
	const char *author;
	const char *subject;
	const char *keywords;
	EvDocumentLayout layout;
	EvDocumentMode mode;
	GDate *creation_date;
	guint ui_hints;

	/* Mask of all the valid fields */
	guint fields_mask;
};

G_END_DECLS

#endif /* EV_DOCUMENT_INFO_H */
