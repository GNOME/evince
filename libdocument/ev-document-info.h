/*
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
 *  Copyright © 2021 Christian Persch
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

#pragma once

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#include <glib-object.h>
#include <glib.h>

#include "ev-macros.h"

G_BEGIN_DECLS

typedef struct _EvDocumentInfo    EvDocumentInfo;
typedef struct _EvDocumentLicense EvDocumentLicense;

#define EV_TYPE_DOCUMENT_INFO (ev_document_info_get_type())

typedef enum
{
	EV_DOCUMENT_LAYOUT_SINGLE_PAGE,
	EV_DOCUMENT_LAYOUT_ONE_COLUMN,
	EV_DOCUMENT_LAYOUT_TWO_COLUMN_LEFT,
	EV_DOCUMENT_LAYOUT_TWO_COLUMN_RIGHT,
	EV_DOCUMENT_LAYOUT_TWO_PAGE_LEFT,
	EV_DOCUMENT_LAYOUT_TWO_PAGE_RIGHT
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
	EV_DOCUMENT_CONTAINS_JS_UNKNOWN,
	EV_DOCUMENT_CONTAINS_JS_NO,
	EV_DOCUMENT_CONTAINS_JS_YES
} EvDocumentContainsJS;

typedef enum
{
	EV_DOCUMENT_UI_HINT_HIDE_TOOLBAR = 1 << 0,
	EV_DOCUMENT_UI_HINT_HIDE_MENUBAR = 1 << 1,
	EV_DOCUMENT_UI_HINT_HIDE_WINDOWUI = 1 << 2,
	EV_DOCUMENT_UI_HINT_FIT_WINDOW = 1 << 3,
	EV_DOCUMENT_UI_HINT_CENTER_WINDOW = 1 << 4,
	EV_DOCUMENT_UI_HINT_DISPLAY_DOC_TITLE = 1 << 5,
	EV_DOCUMENT_UI_HINT_DIRECTION_RTL = 1 << 6
} EvDocumentUIHints;

/* This define is needed because glib-mkenums chokes with multiple lines */
#define _PERMISSIONS_FULL (EV_DOCUMENT_PERMISSIONS_OK_TO_PRINT  \
			 | EV_DOCUMENT_PERMISSIONS_OK_TO_MODIFY \
			 | EV_DOCUMENT_PERMISSIONS_OK_TO_COPY   \
			 | EV_DOCUMENT_PERMISSIONS_OK_TO_ADD_NOTES)

typedef enum
{
	EV_DOCUMENT_PERMISSIONS_OK_TO_PRINT = 1 << 0,
	EV_DOCUMENT_PERMISSIONS_OK_TO_MODIFY = 1 << 1,
	EV_DOCUMENT_PERMISSIONS_OK_TO_COPY = 1 << 2,
	EV_DOCUMENT_PERMISSIONS_OK_TO_ADD_NOTES = 1 << 3,
	EV_DOCUMENT_PERMISSIONS_FULL = _PERMISSIONS_FULL
} EvDocumentPermissions;

typedef enum
{
	EV_DOCUMENT_INFO_TITLE = 1 << 0,
	EV_DOCUMENT_INFO_FORMAT = 1 << 1,
	EV_DOCUMENT_INFO_AUTHOR = 1 << 2,
	EV_DOCUMENT_INFO_SUBJECT = 1 << 3,
	EV_DOCUMENT_INFO_KEYWORDS = 1 << 4,
	EV_DOCUMENT_INFO_LAYOUT = 1 << 5,
	EV_DOCUMENT_INFO_CREATOR = 1 << 6,
	EV_DOCUMENT_INFO_PRODUCER = 1 << 7,
	EV_DOCUMENT_INFO_CREATION_DATETIME = 1 << 8,
	EV_DOCUMENT_INFO_MOD_DATETIME = 1 << 9,
	EV_DOCUMENT_INFO_LINEARIZED = 1 << 10,
	EV_DOCUMENT_INFO_START_MODE = 1 << 11,
	EV_DOCUMENT_INFO_UI_HINTS = 1 << 12,
	EV_DOCUMENT_INFO_PERMISSIONS = 1 << 13,
	EV_DOCUMENT_INFO_N_PAGES = 1 << 14,
	EV_DOCUMENT_INFO_SECURITY = 1 << 15,
	EV_DOCUMENT_INFO_PAPER_SIZE = 1 << 16,
	EV_DOCUMENT_INFO_LICENSE = 1 << 17,
	EV_DOCUMENT_INFO_CONTAINS_JS = 1 << 18
} EvDocumentInfoFields;

struct _EvDocumentInfo
{
	char *title;
	char *format; /* eg, "pdf-1.5" */
	char *author;
	char *subject;
	char *keywords;
	char *creator;
	char *producer;
	char *linearized;
	char *security;
	GDateTime *creation_datetime;
	GDateTime *modified_datetime;
	EvDocumentLayout layout;
	EvDocumentMode mode;
	guint ui_hints;
	guint permissions;
	int   n_pages;
	double paper_height;
	double paper_width;
	EvDocumentLicense *license;
	EvDocumentContainsJS contains_js; /* wheter it contains any javascript */

	/* Mask of all the valid fields */
	guint fields_mask;
};

EV_PUBLIC
GType           ev_document_info_get_type (void) G_GNUC_CONST;
EV_PUBLIC
EvDocumentInfo* ev_document_info_new      (void);
EV_PUBLIC
EvDocumentInfo *ev_document_info_copy     (EvDocumentInfo *info);
EV_PUBLIC
void            ev_document_info_free     (EvDocumentInfo *info);
EV_PUBLIC
GDateTime      *ev_document_info_get_created_datetime   (const EvDocumentInfo *info);
EV_PUBLIC
GDateTime      *ev_document_info_get_modified_datetime  (const EvDocumentInfo *info);

EV_PRIVATE
void            ev_document_info_take_created_datetime  (EvDocumentInfo *info,
                                                         GDateTime      *datetime);
EV_PRIVATE
void            ev_document_info_take_modified_datetime (EvDocumentInfo *info,
                                                         GDateTime      *datetime);
EV_PRIVATE
gboolean        ev_document_info_set_from_xmp           (EvDocumentInfo *info,
                                                         const char     *xmp,
                                                         gssize          size);

/* EvDocumentLicense */
#define EV_TYPE_DOCUMENT_LICENSE (ev_document_license_get_type())
struct _EvDocumentLicense {
	gchar *text;
	gchar *uri;
	gchar *web_statement;
};
EV_PUBLIC
GType              ev_document_license_get_type          (void) G_GNUC_CONST;
EV_PUBLIC
EvDocumentLicense *ev_document_license_new               (void);
EV_PUBLIC
EvDocumentLicense *ev_document_license_copy              (EvDocumentLicense *license);
EV_PUBLIC
void               ev_document_license_free              (EvDocumentLicense *license);
EV_PUBLIC
const gchar       *ev_document_license_get_text          (EvDocumentLicense *license);
EV_PUBLIC
const gchar       *ev_document_license_get_uri           (EvDocumentLicense *license);
EV_PUBLIC
const gchar       *ev_document_license_get_web_statement (EvDocumentLicense *license);

G_END_DECLS
