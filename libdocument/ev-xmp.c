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

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

/* Fields for checking the license info suggested by Creative Commons
 * Main reference: http://wiki.creativecommons.org/XMP
 */

/* fields from the XMP Rights Management Schema, XMP Specification Sept 2005, pag. 42 */
#define LICENSE_MARKED "/rdf:RDF/rdf:Description/xmpRights:Marked"
#define LICENSE_TEXT "/x:xmpmeta/rdf:RDF/rdf:Description/xmpRights:UsageTerms/rdf:Alt/rdf:li[lang('%s')]"
#define LICENSE_WEB_STATEMENT "/rdf:RDF/rdf:Description/xmpRights:WebStatement"
/* license field from Creative Commons schema, http://creativecommons.org/ns */
#define LICENSE_URI "/rdf:RDF/rdf:Description/cc:license/@rdf:resource"

/* alternative field from the Dublic Core Schema for checking the informal rights statement
 * as suggested by the Creative Commons template [1]. This field has been replaced or
 * complemented by its XMP counterpart [2].
 * References:
 *    [1] http://wiki.creativecommons.org/XMP_help_for_Adobe_applications
 *    [2] http://code.creativecommons.org/issues/issue505
 */
#define LICENSE_TEXT_ALT "/x:xmpmeta/rdf:RDF/rdf:Description/dc:rights/rdf:Alt/rdf:li[lang('%s')]"
#define GET_LICENSE_TEXT(a) ( (a < 1) ? LICENSE_TEXT : LICENSE_TEXT_ALT )

/* fields for authors and keywords */
#define AUTHORS "/rdf:RDF/rdf:Description/dc:creator/rdf:Seq/rdf:li"
#define KEYWORDS "/rdf:RDF/rdf:Description/dc:subject/rdf:Bag/rdf:li"
/* fields for title and subject */
#define TITLE "/rdf:RDF/rdf:Description/dc:title/rdf:Alt/rdf:li[lang('%s')]"
#define SUBJECT "/rdf:RDF/rdf:Description/dc:description/rdf:Alt/rdf:li[lang('%s')]"
/* fields for creation and modification dates */
#define MOD_DATE "/rdf:RDF/rdf:Description/xmp:ModifyDate"
#define CREATE_DATE "/rdf:RDF/rdf:Description/xmp:CreateDate"
#define META_DATE "/rdf:RDF/rdf:Description/xmp:MetadataDate"
/* fields for pdf creator tool and producer */
#define CREATOR "/rdf:RDF/rdf:Description/xmp:CreatorTool"
#define PRODUCER "/rdf:RDF/rdf:Description/pdf:Producer"

/*
 * strexchange:
 * @str: (transfer full): a string from libxml allocator
 *
 * Returns: (transfer full): @str from glib allocator
 */
static char *
strexchange (xmlChar *str)
{
        char *rv = g_strdup ((char*)str);
        xmlFree (str);
        return rv;
}

static xmlChar *
xmp_get_tag_from_xpath (xmlXPathContextPtr xpathCtx,
                        const char* xpath)
{
        xmlXPathObjectPtr xpathObj;
        xmlChar *result = NULL;
        char *xmpmetapath;

        /* Try in /rdf:RDF/ */
        xpathObj = xmlXPathEvalExpression (BAD_CAST xpath, xpathCtx);
        if (xpathObj == NULL)
                return NULL;

        if (xpathObj->nodesetval != NULL && xpathObj->nodesetval->nodeNr != 0)
                result = xmlNodeGetContent (xpathObj->nodesetval->nodeTab[0]);

        xmlXPathFreeObject (xpathObj);

        if (result != NULL)
                return result;

        /*
          Try in /x:xmpmeta/ (xmpmeta is optional)
          https://wwwimages2.adobe.com/content/dam/acom/en/devnet/xmp/pdfs/XMP SDK Release cc-2016-08/XMPSpecificationPart1.pdf (Section 7.3.3)
        */
        xmpmetapath = g_strdup_printf ("%s%s", "/x:xmpmeta", xpath);
        xpathObj = xmlXPathEvalExpression (BAD_CAST xmpmetapath, xpathCtx);
        g_free (xmpmetapath);
        if (xpathObj == NULL)
                return NULL;

        if (xpathObj->nodesetval != NULL && xpathObj->nodesetval->nodeNr != 0)
                result = xmlNodeGetContent (xpathObj->nodesetval->nodeTab[0]);

        xmlXPathFreeObject (xpathObj);
        return result;
}

static GDateTime *
xmp_get_datetime_from_xpath (xmlXPathContextPtr xpathCtx,
                             const char* xpath)
{
        xmlChar *tag;
        GDateTime *datetime;

        tag = xmp_get_tag_from_xpath (xpathCtx, xpath);
        if (tag == NULL)
                return NULL;

        datetime = g_date_time_new_from_iso8601 ((const char*)tag, NULL);
        xmlFree (tag);

        return datetime;
}

/* Reference:
 * http://www.pdfa.org/lib/exe/fetch.php?id=pdfa%3Aen%3Atechdoc&cache=cache&media=pdfa:techdoc:tn0001_pdfa-1_and_namespaces_2008-03-18.pdf
 */
static char *
xmp_get_pdf_format (xmlXPathContextPtr xpathCtx)
{
        xmlChar *part = NULL;
        xmlChar *conf = NULL;
        xmlChar *pdfxid = NULL;
        char *result = NULL;
        int i;

        /* reads pdf/a part */
        /* first syntax: child node */
        part = xmp_get_tag_from_xpath (xpathCtx, "/rdf:RDF/rdf:Description/pdfaid:part");
        if (part == NULL) {
                /* second syntax: attribute */
                part = xmp_get_tag_from_xpath (xpathCtx, "/rdf:RDF/rdf:Description/@pdfaid:part");
        }

        /* reads pdf/a conformance */
        /* first syntax: child node */
        conf =  xmp_get_tag_from_xpath (xpathCtx, "/rdf:RDF/rdf:Description/pdfaid:conformance");
        if (conf == NULL) {
                /* second syntax: attribute */
                conf =  xmp_get_tag_from_xpath (xpathCtx, "/rdf:RDF/rdf:Description/@pdfaid:conformance");
        }

        /* reads pdf/x id  */
        /* first syntax: pdfxid */
        pdfxid = xmp_get_tag_from_xpath (xpathCtx, "/rdf:RDF/rdf:Description/pdfxid:GTS_PDFXVersion");
        if (pdfxid == NULL) {
                /* second syntax: pdfx */
                pdfxid = xmp_get_tag_from_xpath (xpathCtx, "/rdf:RDF/rdf:Description/pdfx:GTS_PDFXVersion");
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

        /* Cleanup */
        xmlFree (part);
        xmlFree (conf);
        xmlFree (pdfxid);
        return result;
}

static char *
xmp_get_lists_from_dc_tags (xmlXPathContextPtr xpathCtx,
                            const char* xpath)
{
        xmlXPathObjectPtr xpathObj;
        int i;
        char* elements = NULL;
        char* tmp_elements = NULL;
        char* result = NULL;
        xmlChar* content;

        /* reads pdf/a sequence*/
        xpathObj = xmlXPathEvalExpression (BAD_CAST xpath, xpathCtx);
        if (xpathObj == NULL)
                return NULL;

        if (xpathObj->nodesetval != NULL && xpathObj->nodesetval->nodeNr != 0) {
                for (i = 0; i < xpathObj->nodesetval->nodeNr; i++) {
                        content = xmlNodeGetContent (xpathObj->nodesetval->nodeTab[i]);
                        if (i) {
                                tmp_elements = g_strdup (elements);
                                g_free (elements);
                                elements = g_strdup_printf ("%s, %s", tmp_elements, content);
                                g_free (tmp_elements);
                        } else {
                                elements = g_strdup_printf ("%s", content);
                        }
                        xmlFree(content);
                }
        }
        xmlXPathFreeObject (xpathObj);

        if (elements != NULL) {
                /* return buffer */
                result = g_strdup (elements);
        }

        /* Cleanup */
        g_free (elements);

        return result;
}

static char *
xmp_get_author (xmlXPathContextPtr xpathCtx)
{
        char* result = NULL;
        char* xmpmetapath;

        /* Try in /rdf:RDF/ */
        result = xmp_get_lists_from_dc_tags (xpathCtx, AUTHORS);
        if (result != NULL)
                return result;

        /* Try in /x:xmpmeta/ */
        xmpmetapath = g_strdup_printf ("%s%s", "/x:xmpmeta", AUTHORS);
        result = xmp_get_lists_from_dc_tags (xpathCtx, xmpmetapath);
        g_free (xmpmetapath);

        return result;
}

static char *
xmp_get_keywords (xmlXPathContextPtr xpathCtx)
{
        char* result = NULL;
        char* xmpmetapath;

        /* Try in /rdf:RDF/ */
        result = xmp_get_lists_from_dc_tags (xpathCtx, KEYWORDS);
        if (result != NULL)
                return result;

        /* Try in /x:xmpmeta/ */
        xmpmetapath = g_strdup_printf ("%s%s", "/x:xmpmeta", KEYWORDS);
        result = xmp_get_lists_from_dc_tags (xpathCtx, xmpmetapath);
        g_free (xmpmetapath);

        return result;
}

static G_GNUC_FORMAT (2) char *
xmp_get_localized_object_from_xpath_format (xmlXPathContextPtr xpathCtx,
                                            const char* xpath_format)
{
        const char *language_string;
        char  *aux;
        gchar **tags;
        gchar *tag, *tag_aux;
        int i, j;
        xmlChar *loc_object= NULL;

        /* 1) checking for a suitable localized string */
        language_string = pango_language_to_string (pango_language_get_default ());
        tags = g_strsplit (language_string, "-", -1);
        i = g_strv_length (tags);
        while (i-- && !loc_object) {
                tag = g_strdup (tags[0]);
                for (j = 1; j <= i; j++) {
                        tag_aux = g_strdup_printf ("%s-%s", tag, tags[j]);
                        g_free (tag);
                        tag = tag_aux;
                }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
                aux = g_strdup_printf (xpath_format, tag);
#pragma GCC diagnostic pop
                loc_object = xmp_get_tag_from_xpath (xpathCtx, aux);
                g_free (tag);
                g_free (aux);
        }
        g_strfreev (tags);

        /* 2) if not, use the default string */
        if (!loc_object) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
                aux = g_strdup_printf (xpath_format, "x-default");
#pragma GCC diagnostic pop
                loc_object = xmp_get_tag_from_xpath (xpathCtx, aux);
                g_free (aux);
        }
        return strexchange (loc_object);
}

static char *
xmp_get_title (xmlXPathContextPtr xpathCtx)
{
        return xmp_get_localized_object_from_xpath_format (xpathCtx, TITLE);
}

static char *
xmp_get_subject (xmlXPathContextPtr xpathCtx)
{
        return xmp_get_localized_object_from_xpath_format (xpathCtx, SUBJECT);
}

static EvDocumentLicense *
xmp_get_license (xmlXPathContextPtr xpathCtx)
{
        xmlChar *marked = NULL;
        EvDocumentLicense *license;

        /* checking if the document has been marked as defined on the XMP Rights
         * Management Schema */
        marked = xmp_get_tag_from_xpath (xpathCtx, LICENSE_MARKED);

        /* a) Not marked => No XMP Rights information */
        if (!marked)
                return NULL;

        license = ev_document_license_new ();

        /* b) Marked False => Public Domain, no copyrighted material and no
         * license needed */
        if (g_strrstr ((char *) marked, "False") != NULL) {
                license->text = g_strdup (_("This work is in the Public Domain"));
                /* c) Marked True => Copyrighted material */
        } else {
                /* Checking usage terms as defined by the XMP Rights Management
                 * Schema. This field is recomended to be checked by Creative
                 * Commons */
                /* 1) checking for a suitable localized string */
                int lt;

                for (lt = 0; !license->text && lt < 2; lt++)
                        license->text = xmp_get_localized_object_from_xpath_format (xpathCtx,
                                                                                    GET_LICENSE_TEXT (lt));

                /* Checking the license URI as defined by the Creative Commons
                 * Schema. This field is recomended to be checked by Creative
                 * Commons */
                license->uri = strexchange (xmp_get_tag_from_xpath (xpathCtx, LICENSE_URI));

                /* Checking the web statement as defined by the XMP Rights
                 * Management Schema. Checking it out is a sort of above-and-beyond
                 * the basic recommendations by Creative Commons. It can be
                 * considered as a "reinforcement" approach to add certainty. */
                license->web_statement = strexchange (xmp_get_tag_from_xpath (xpathCtx, LICENSE_WEB_STATEMENT));
        }
        xmlFree (marked);

        if (!license->text && !license->uri && !license->web_statement) {
                ev_document_license_free (license);
                return NULL;
        }

        return license;
}

/*
 * ev_xmp_parse:
 * @metadata: a XMP document as a string
 * @info: a #EvDocumentInfo
 *
 * Returns: %TRUE iff @metadata could be successfully parsed
 */
gboolean
ev_xmp_parse (const gchar    *metadata,
              EvDocumentInfo *info)
{
        xmlDocPtr          doc;
        xmlXPathContextPtr xpathCtx;
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

        doc = xmlParseMemory (metadata, strlen (metadata));
        if (doc == NULL)
                return FALSE; /* invalid xml metadata */

        xpathCtx = xmlXPathNewContext (doc);
        if (xpathCtx == NULL) {
                xmlFreeDoc (doc);
                return FALSE; /* invalid xpath context */
        }

        /* Register namespaces */
        /* XMP */
        xmlXPathRegisterNs (xpathCtx, BAD_CAST "dc", BAD_CAST "http://purl.org/dc/elements/1.1/");
        xmlXPathRegisterNs (xpathCtx, BAD_CAST "x", BAD_CAST "adobe:ns:meta/");
        xmlXPathRegisterNs (xpathCtx, BAD_CAST "xmp", BAD_CAST "http://ns.adobe.com/xap/1.0/");
        /* XMP Rights Management Schema */
        xmlXPathRegisterNs (xpathCtx, BAD_CAST "xmpRights", BAD_CAST "http://ns.adobe.com/xap/1.0/rights/");
        /* RDF */
        xmlXPathRegisterNs (xpathCtx, BAD_CAST "rdf", BAD_CAST "http://www.w3.org/1999/02/22-rdf-syntax-ns#");
        /* PDF/A and PDF/X namespaces */
        xmlXPathRegisterNs (xpathCtx, BAD_CAST "pdf", BAD_CAST "http://ns.adobe.com/pdf/1.3/");
        xmlXPathRegisterNs (xpathCtx, BAD_CAST "pdfaid", BAD_CAST "http://www.aiim.org/pdfa/ns/id/");
        xmlXPathRegisterNs (xpathCtx, BAD_CAST "pdfx", BAD_CAST "http://ns.adobe.com/pdfx/1.3/");
        xmlXPathRegisterNs (xpathCtx, BAD_CAST "pdfxid", BAD_CAST "http://www.npes.org/pdfx/ns/id/");
        /* Creative Commons Schema */
        xmlXPathRegisterNs (xpathCtx, BAD_CAST "cc", BAD_CAST "http://creativecommons.org/ns#");

        /* Read metadata date */
        metadata_datetime = xmp_get_datetime_from_xpath (xpathCtx, META_DATE);

        /* From PDF spec, if the PDF modified date is newer than metadata date,
         * it indicates that the file was edited by a non-XMP aware software.
         * Then, the information dictionary is considered authoritative and the
         * XMP metadata should not be displayed.
         */
        modified_datetime = ev_document_info_get_modified_datetime (info);
        if (modified_datetime == NULL ||
            metadata_datetime == NULL ||
            g_date_time_compare (metadata_datetime, modified_datetime) >= 0) {

                fmt = xmp_get_pdf_format (xpathCtx);
                if (fmt != NULL) {
                        g_free (info->format);
                        info->format = fmt;
                        info->fields_mask |= EV_DOCUMENT_INFO_FORMAT;
                }

                author = xmp_get_author (xpathCtx);
                if (author != NULL) {
                        g_free (info->author);
                        info->author = author;
                        info->fields_mask |= EV_DOCUMENT_INFO_AUTHOR;
                }

                keywords = xmp_get_keywords (xpathCtx);
                if (keywords != NULL) {
                        g_free (info->keywords);
                        info->keywords = keywords;
                        info->fields_mask |= EV_DOCUMENT_INFO_KEYWORDS;
                }

                title = xmp_get_title (xpathCtx);
                if (title != NULL) {
                        g_free (info->title);
                        info->title = title;
                        info->fields_mask |= EV_DOCUMENT_INFO_TITLE;
                }

                subject = xmp_get_subject (xpathCtx);
                if (subject != NULL) {
                        g_free (info->subject);
                        info->subject = subject;
                        info->fields_mask |= EV_DOCUMENT_INFO_SUBJECT;
                }

                creatortool = strexchange (xmp_get_tag_from_xpath (xpathCtx, CREATOR));
                if (creatortool != NULL) {
                        g_free (info->creator);
                        info->creator = creatortool;
                        info->fields_mask |= EV_DOCUMENT_INFO_CREATOR;
                }

                producer = strexchange (xmp_get_tag_from_xpath (xpathCtx, PRODUCER));
                if (producer != NULL) {
                        g_free (info->producer);
                        info->producer = producer;
                        info->fields_mask |= EV_DOCUMENT_INFO_PRODUCER;
                }

                /* reads modify date */
                datetime = xmp_get_datetime_from_xpath (xpathCtx, MOD_DATE);
                if (datetime)
                        ev_document_info_take_modified_datetime (info, datetime);

                /* reads pdf create date */
                datetime = xmp_get_datetime_from_xpath (xpathCtx, CREATE_DATE);
                if (datetime)
                        ev_document_info_take_created_datetime (info, datetime);
        }

        info->license = xmp_get_license (xpathCtx);
        if (info->license)
                info->fields_mask |= EV_DOCUMENT_INFO_LICENSE;


        g_clear_pointer (&metadata_datetime, g_date_time_unref);
        xmlXPathFreeContext (xpathCtx);
        xmlFreeDoc (doc);

        return TRUE;
}
