/*
 *  Copyright (C) 2009 Carlos Garcia Campos
 *  Copyright (C) 2004 Marco Pesenti Gritti
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

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "ev-document-info.h"
#include "ev-xmp.h"

G_DEFINE_BOXED_TYPE (EvDocumentInfo, ev_document_info, ev_document_info_copy, ev_document_info_free)

/**
 * ev_document_info_new:
 *
 * Returns: (transfer full): a new, empty #EvDocumentInfo
 */
EvDocumentInfo *
ev_document_info_new (void)
{
        return g_new0 (EvDocumentInfo, 1);
}

/**
 * ev_document_info_copy:
 * @info: a #EvDocumentInfo
 *
 * Returns: (transfer full): a copy of @info
 */
EvDocumentInfo *
ev_document_info_copy (EvDocumentInfo *info)
{
        EvDocumentInfo *copy;

        g_return_val_if_fail (info != NULL, NULL);

        copy = ev_document_info_new ();

	copy->title = g_strdup (info->title);
	copy->format = g_strdup (info->format);
	copy->author = g_strdup (info->author);
	copy->subject = g_strdup (info->subject);
	copy->keywords = g_strdup (info->keywords);
	copy->security = g_strdup (info->security);
	copy->creator = g_strdup (info->creator);
	copy->producer = g_strdup (info->producer);
	copy->linearized = g_strdup (info->linearized);

        copy->creation_datetime = g_date_time_ref (info->creation_datetime);
        copy->modified_datetime = g_date_time_ref (info->modified_datetime);

	copy->layout = info->layout;
	copy->mode = info->mode;
	copy->ui_hints = info->ui_hints;
	copy->permissions = info->permissions;
	copy->n_pages = info->n_pages;
	copy->license = ev_document_license_copy (info->license);

	copy->fields_mask = info->fields_mask;

        return copy;
}

/**
 * ev_document_info_free:
 * @info: (transfer full): a #EvDocumentInfo
 *
 * Frees @info.
 */
void
ev_document_info_free (EvDocumentInfo *info)
{
        if (info == NULL)
                return;

	g_free (info->title);
	g_free (info->format);
	g_free (info->author);
	g_free (info->subject);
	g_free (info->keywords);
	g_free (info->creator);
	g_free (info->producer);
	g_free (info->linearized);
	g_free (info->security);
	ev_document_license_free (info->license);

        g_clear_pointer (&info->creation_datetime, g_date_time_unref);
        g_clear_pointer (&info->modified_datetime, g_date_time_unref);

        g_free (info);
}

/*
 * ev_document_info_take_created_datetime:
 * @info: a #EvDocumentInfo
 * @datetime: (transfer full): a #GDateTime
 *
 * Sets the #GDateTime for when the document was created.
 */
void
ev_document_info_take_created_datetime (EvDocumentInfo *info,
                                        GDateTime      *datetime)
{
	g_return_if_fail (info != NULL);
	g_clear_pointer (&info->creation_datetime, g_date_time_unref);

	info->creation_datetime = datetime;
	info->fields_mask |= EV_DOCUMENT_INFO_CREATION_DATETIME;
}

/**
 * ev_document_info_get_created_datetime:
 * @info: a #EvDocumentInfo
 *
 * Returns: (transfer none) (nullable): a #GDateTime for when the document was created
 */
GDateTime *
ev_document_info_get_created_datetime (const EvDocumentInfo *info)
{
	g_return_val_if_fail (info != NULL, NULL);
	g_return_val_if_fail (info->fields_mask & EV_DOCUMENT_INFO_CREATION_DATETIME, NULL);

        return info->creation_datetime;
}

/*
 * ev_document_info_take_modified_datetime:
 * @info: a #EvDocumentInfo
 * @datetime: (transfer full): a #GDateTime
 *
 * Sets the #GDateTime for when the document was last modified.
 */
void
ev_document_info_take_modified_datetime (EvDocumentInfo *info,
                                         GDateTime      *datetime)
{
	g_return_if_fail (info != NULL);

	g_clear_pointer (&info->modified_datetime, g_date_time_unref);
	info->modified_datetime = datetime;
	info->fields_mask |= EV_DOCUMENT_INFO_MOD_DATETIME;
}

/**
 * ev_document_info_get_modified_datetime:
 * @info: a #EvDocumentInfo
 *
 * Returns: (transfer none) (nullable): a #GDateTime for when the document was last modified
 */
GDateTime *
ev_document_info_get_modified_datetime (const EvDocumentInfo *info)
{
	g_return_val_if_fail (info != NULL, NULL);
	g_return_val_if_fail (info->fields_mask & EV_DOCUMENT_INFO_MOD_DATETIME, NULL);

	return info->modified_datetime;
}

/*
 * ev_document_info_set_from_xmp:
 * @info: a #EvDocumentInfo
 * @xmp: a XMP document
 * @size: the size of @xmp in bytes, or -1 if @xmp is a NUL-terminated string
 *
 * Parses the XMP document and sets @info from it.
 *
 * Returns: %TRUE iff @xmp could be successfully parsed as a XMP document
 */
gboolean
ev_document_info_set_from_xmp (EvDocumentInfo *info,
                               const char     *xmp,
                               gssize          size)
{
        return ev_xmp_parse (xmp, size != -1 ? size : strlen (xmp), info);
}

/* EvDocumentLicense */
G_DEFINE_BOXED_TYPE (EvDocumentLicense, ev_document_license, ev_document_license_copy, ev_document_license_free)

/**
 * ev_document_license_new:
 *
 * Returns: (transfer full): a new, empty #EvDocumentLicense
 */
EvDocumentLicense *
ev_document_license_new (void)
{
	return g_new0 (EvDocumentLicense, 1);
}

/**
 * ev_document_license_copy:
 * @license: (nullable): a #EvDocumentLicense
 *
 * Returns: (transfer full): a copy of @license, or %NULL
 */
EvDocumentLicense *
ev_document_license_copy (EvDocumentLicense *license)
{
	EvDocumentLicense *new_license;

	if (!license)
		return NULL;

	new_license = ev_document_license_new ();

	if (license->text)
		new_license->text = g_strdup (license->text);
	if (license->uri)
		new_license->uri = g_strdup (license->uri);
	if (license->web_statement)
		new_license->web_statement = g_strdup (license->web_statement);

	return new_license;
}

/**
 * ev_document_license_free:
 * @license: (transfer full): a #EvDocumentLicense
 *
 * Frees @license.
 */
void
ev_document_license_free (EvDocumentLicense *license)
{
	if (!license)
		return;

	g_free (license->text);
	g_free (license->uri);
	g_free (license->web_statement);

	g_free (license);
}

/**
 * ev_document_license_get_text:
 * @license: (transfer full): a #EvDocumentLicense
 *
 * Returns: (transfer none) (nullable): the license text
 */
const gchar *
ev_document_license_get_text (EvDocumentLicense *license)
{
	return license->text;
}

/**
 * ev_document_license_get_uri:
 * @license: (transfer full): a #EvDocumentLicense
 *
 * Returns: (transfer none) (nullable): the license URI
 */
const gchar *
ev_document_license_get_uri (EvDocumentLicense *license)
{
	return license->uri;
}

/**
 * ev_document_license_get_web_statement
 * @license: (transfer full): a #EvDocumentLicense
 *
 * Returns: (transfer none) (nullable): the license web statement
 */
const gchar *
ev_document_license_get_web_statement (EvDocumentLicense *license)
{
	return license->web_statement;
}
