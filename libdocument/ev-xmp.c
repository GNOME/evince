/*
 * Copyright (C) 2018, Evangelos Rigas <erigas@rnd2.org>
 * Copyright (C) 2009, Juanjo Marín <juanj.marin@juntadeandalucia.es>
 * Copyright (C) 2004, Red Hat, Inc.
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

#include "config.h"

#include <string.h>

#include "ev-xmp.h"

#include <glib/gi18n-lib.h>
#include <pango/pango.h>

#include <exempi/xmp.h>
#include <exempi/xmpconsts.h>

#define NS_PDFA_ID	"http://www.aiim.org/pdfa/ns/id/"
#define NS_PDFX_ID	"http://www.npes.org/pdfx/ns/id/"
#define NS_PDFX		"http://ns.adobe.com/pdfx/1.3/"

static char *
ev_xmp_get_name (XmpPtr xmp,
		 const char *ns,
		 const char *name)
{
	XmpStringPtr property = xmp_string_new ();
	char *result = NULL;

	if (xmp_get_property (xmp, ns, name, property, NULL))
		result = g_strdup (xmp_string_cstr (property));

	xmp_string_free (property);

	return result;
}

static GDateTime *
ev_xmp_get_datetime (XmpPtr xmp, const gchar *name)
{
        GDateTime *datetime = NULL;
	XmpStringPtr date = xmp_string_new ();

	if (xmp_get_property (xmp, NS_XAP, name, date, NULL))
	        datetime = g_date_time_new_from_iso8601 (xmp_string_cstr (date), NULL);

	xmp_string_free (date);

        return datetime;
}

/* Reference:
 * http://www.pdfa.org/lib/exe/fetch.php?id=pdfa%3Aen%3Atechdoc&cache=cache&media=pdfa:techdoc:tn0001_pdfa-1_and_namespaces_2008-03-18.pdf
 */
static char *
ev_xmp_get_pdf_format (XmpPtr xmp)
{
        g_autofree char *part = NULL;
        g_autofree char *conf = NULL;
        g_autofree char *pdfxid = NULL;
        char *result = NULL;
        int i;

        /* reads pdf/a part */
        part = ev_xmp_get_name (xmp, NS_PDFA_ID, "part");

        /* reads pdf/a conformance */
        conf = ev_xmp_get_name (xmp, NS_PDFA_ID, "conformance");

        /* reads pdf/x id  */
        pdfxid = ev_xmp_get_name (xmp, NS_PDFX_ID, "GTS_PDFXVersion");
        if (pdfxid == NULL) {
                pdfxid = ev_xmp_get_name (xmp, NS_PDFX, "GTS_PDFXVersion");
        }

        if (part != NULL && conf != NULL) {
                /* makes conf lowercase */
                for (i = 0; conf[i]; i++)
                        conf[i] = g_ascii_tolower (conf[i]);

                /* return buffer */
                result = g_strdup_printf ("PDF/A - %s%s", part, conf);
        }
        else if (pdfxid != NULL) {
                result = g_strdup_printf ("%s", pdfxid);
        }

        return result;
}

static char *
ev_xmp_get_lists_from_dc_tags (XmpPtr xmp,
                               const char *name)
{
	char* elements = NULL;
	char* tmp_elements = NULL;
	XmpStringPtr content = xmp_string_new ();
	int index = 1;

	while (xmp_get_array_item (xmp, NS_DC, name, index, content, NULL)) {
		if (index > 1) {
			tmp_elements = g_strdup (elements);
			g_free (elements);
			elements = g_strdup_printf ("%s, %s", tmp_elements,
						    xmp_string_cstr (content));
			g_free (tmp_elements);
		} else {
			elements = g_strdup_printf ("%s", xmp_string_cstr (content));
		}

		index++;
	}

	xmp_string_free (content);

	return elements;
}

static char *
ev_xmp_get_author (XmpPtr xmp)
{
	return ev_xmp_get_lists_from_dc_tags (xmp, "creator");
}

static char *
ev_xmp_get_keywords (XmpPtr xmp)
{
	return ev_xmp_get_lists_from_dc_tags (xmp, "subject");
}

static char *
ev_xmp_get_localized_object (XmpPtr xmp,
			  const char *ns,
			  const char *name)
{
        const char *language_string;
	gchar **tags, *result = NULL;
	XmpStringPtr value = xmp_string_new ();

        /* 1) checking for a suitable localized string */
        language_string = pango_language_to_string (pango_language_get_default ());

        tags = g_strsplit (language_string, "-", -1);

	/* This function will fallback to x-default when tag[0] is NULL */
	if (xmp_get_localized_text (xmp, ns, name, tags[0], language_string,
				    NULL, value, NULL))
		result = g_strdup (xmp_string_cstr (value));

        g_strfreev (tags);
	xmp_string_free (value);

        return result;
}

static char *
ev_xmp_get_title (XmpPtr xmp)
{
        return ev_xmp_get_localized_object (xmp, NS_DC, "title");
}

static char *
ev_xmp_get_subject (XmpPtr xmp)
{
	return ev_xmp_get_localized_object (xmp, NS_DC, "description");
}

static EvDocumentLicense *
ev_xmp_get_license (XmpPtr xmp)
{
        bool marked, has_mark;
        EvDocumentLicense *license;

        /* checking if the document has been marked as defined on the XMP Rights
         * Management Schema */
	has_mark = xmp_get_property_bool (xmp, NS_XAP_RIGHTS, "Marked", &marked, NULL);

        /* a) Not marked => No XMP Rights information */
        if (!has_mark)
                return NULL;

        license = ev_document_license_new ();

        /* b) Marked False => Public Domain, no copyrighted material and no
         * license needed */
        if (!marked) {
                license->text = g_strdup (_("This work is in the Public Domain"));
                /* c) Marked True => Copyrighted material */
        } else {
                /* Checking usage terms as defined by the XMP Rights Management
                 * Schema. This field is recomended to be checked by Creative
                 * Commons */
                /* 1) checking for a suitable localized string */

		/* alternative field from the Dublic Core Schema for checking the informal rights statement
		 * as suggested by the Creative Commons template [1]. This field has been replaced or
		 * complemented by its XMP counterpart [2].
		 * References:
		 *    [1] http://wiki.creativecommons.org/XMP_help_for_Adobe_applications
		 *    [2] http://code.creativecommons.org/issues/issue505
		 */
		license->text = ev_xmp_get_localized_object (xmp, NS_XAP_RIGHTS, "UsageTerms");

		if (!license->text)
			license->text = ev_xmp_get_localized_object (xmp, NS_DC, "rights");

                /* Checking the license URI as defined by the Creative Commons
                 * Schema. This field is recomended to be checked by Creative
                 * Commons */
                license->uri = ev_xmp_get_name (xmp, NS_CC, "license");

                /* Checking the web statement as defined by the XMP Rights
                 * Management Schema. Checking it out is a sort of above-and-beyond
                 * the basic recommendations by Creative Commons. It can be
                 * considered as a "reinforcement" approach to add certainty. */
                license->web_statement = ev_xmp_get_name (xmp, NS_XAP_RIGHTS, "WebStatement");
        }

        if (!license->text && !license->uri && !license->web_statement) {
                ev_document_license_free (license);
                return NULL;
        }

        return license;
}

/*
 * ev_xmp_parse:
 * @metadata: XMP document data
 * @size: size of @metadata in bytes
 * @info: a #EvDocumentInfo
 *
 * Returns: %TRUE iff @metadata could be successfully parsed
 */
gboolean
ev_xmp_parse (const char    *metadata,
              gsize          size,
              EvDocumentInfo *info)
{
        gchar             *fmt;
        gchar             *author;
        gchar             *keywords;
        gchar             *title;
        gchar             *subject;
        gchar             *creatortool;
        gchar             *producer;
        GDateTime         *modified_datetime;
        GDateTime         *metadata_datetime = NULL;
        GDateTime         *datetime;
	XmpPtr             xmp;

	xmp = xmp_new (metadata, size);

        if (xmp == NULL)
                return FALSE; /* invalid xmp metadata */

        /* Read metadata date */
        metadata_datetime = ev_xmp_get_datetime (xmp, "MetadataDate");

        /* From PDF spec, if the PDF modified date is newer than metadata date,
         * it indicates that the file was edited by a non-XMP aware software.
         * Then, the information dictionary is considered authoritative and the
         * XMP metadata should not be displayed.
         */
        modified_datetime = ev_document_info_get_modified_datetime (info);
        if (modified_datetime == NULL ||
            metadata_datetime == NULL ||
            g_date_time_compare (metadata_datetime, modified_datetime) >= 0) {
                fmt = ev_xmp_get_pdf_format (xmp);
                if (fmt != NULL) {
                        g_free (info->format);
                        info->format = fmt;
                        info->fields_mask |= EV_DOCUMENT_INFO_FORMAT;
                }

                author = ev_xmp_get_author (xmp);
                if (author != NULL) {
                        g_free (info->author);
                        info->author = author;
                        info->fields_mask |= EV_DOCUMENT_INFO_AUTHOR;
                }

                keywords = ev_xmp_get_keywords (xmp);
                if (keywords != NULL) {
                        g_free (info->keywords);
                        info->keywords = keywords;
                        info->fields_mask |= EV_DOCUMENT_INFO_KEYWORDS;
                }

                title = ev_xmp_get_title (xmp);
                if (title != NULL) {
                        g_free (info->title);
                        info->title = title;
                        info->fields_mask |= EV_DOCUMENT_INFO_TITLE;
                }

                subject = ev_xmp_get_subject (xmp);
                if (subject != NULL) {
                        g_free (info->subject);
                        info->subject = subject;
                        info->fields_mask |= EV_DOCUMENT_INFO_SUBJECT;
                }

                creatortool = ev_xmp_get_name (xmp, NS_XAP, "CreatorTool");
                if (creatortool != NULL) {
                        g_free (info->creator);
                        info->creator = creatortool;
                        info->fields_mask |= EV_DOCUMENT_INFO_CREATOR;
                }

                producer = ev_xmp_get_name (xmp, NS_PDF, "Producer");
                if (producer != NULL) {
                        g_free (info->producer);
                        info->producer = producer;
                        info->fields_mask |= EV_DOCUMENT_INFO_PRODUCER;
                }

                /* reads modify date */
                datetime = ev_xmp_get_datetime (xmp, "ModifyDate");
                if (datetime)
                        ev_document_info_take_modified_datetime (info, datetime);

                /* reads pdf create date */
                datetime = ev_xmp_get_datetime (xmp, "CreateDate");
                if (datetime)
                        ev_document_info_take_created_datetime (info, datetime);
        }

        info->license = ev_xmp_get_license (xmp);
        if (info->license)
                info->fields_mask |= EV_DOCUMENT_INFO_LICENSE;

        g_clear_pointer (&metadata_datetime, g_date_time_unref);
	xmp_free (xmp);

        return TRUE;
}
