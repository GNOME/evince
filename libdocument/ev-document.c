/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/*
 *  Copyright (C) 2009 Carlos Garcia Campos
 *  Copyright (C) 2004 Marco Pesenti Gritti
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

#include "config.h"

#include "ev-document.h"

GMutex *ev_doc_mutex = NULL;
GMutex *ev_fc_mutex = NULL;

G_DEFINE_ABSTRACT_TYPE (EvDocument, ev_document, G_TYPE_OBJECT)

GQuark
ev_document_error_quark (void)
{
  static GQuark q = 0;
  if (q == 0)
    q = g_quark_from_static_string ("ev-document-error-quark");

  return q;
}

static EvPage *
ev_document_impl_get_page (EvDocument *document,
			   gint        index)
{
	return ev_page_new (index);
}

static void
ev_document_init (EvDocument *document)
{
}

static void
ev_document_class_init (EvDocumentClass *klass)
{
	klass->get_page = ev_document_impl_get_page;
}

GMutex *
ev_document_get_doc_mutex (void)
{
	if (ev_doc_mutex == NULL) {
		ev_doc_mutex = g_mutex_new ();
	}
	return ev_doc_mutex;
}

void
ev_document_doc_mutex_lock (void)
{
	g_mutex_lock (ev_document_get_doc_mutex ());
}

void
ev_document_doc_mutex_unlock (void)
{
	g_mutex_unlock (ev_document_get_doc_mutex ());
}

gboolean
ev_document_doc_mutex_trylock (void)
{
	return g_mutex_trylock (ev_document_get_doc_mutex ());
}

GMutex *
ev_document_get_fc_mutex (void)
{
	if (ev_fc_mutex == NULL) {
		ev_fc_mutex = g_mutex_new ();
	}
	return ev_fc_mutex;
}

void
ev_document_fc_mutex_lock (void)
{
	g_mutex_lock (ev_document_get_fc_mutex ());
}

void
ev_document_fc_mutex_unlock (void)
{
	g_mutex_unlock (ev_document_get_fc_mutex ());
}

gboolean
ev_document_fc_mutex_trylock (void)
{
	return g_mutex_trylock (ev_document_get_fc_mutex ());
}

/**
 * ev_document_load:
 * @document: a #EvDocument
 * @uri: the document's URI
 * @error: a #GError location to store an error, or %NULL
 *
 * Loads @document from @uri.
 * 
 * On failure, %FALSE is returned and @error is filled in.
 * If the document is encrypted, EV_DEFINE_ERROR_ENCRYPTED is returned.
 * If the backend cannot load the specific document, EV_DOCUMENT_ERROR_INVALID
 * is returned. Other errors are possible too, depending on the backend
 * used to load the document and the URI, e.g. #GIOError, #GFileError, and
 * #GConvertError.
 *
 * Returns: %TRUE on success, or %FALSE on failure.
 */
gboolean
ev_document_load (EvDocument  *document,
		  const char  *uri,
		  GError     **error)
{
	EvDocumentClass *klass = EV_DOCUMENT_GET_CLASS (document);
	gboolean retval;
	GError *err = NULL;

	retval = klass->load (document, uri, &err);
	if (!retval) {
		if (err) {
			g_propagate_error (error, err);
		} else {
			g_warning ("%s::EvDocument::load returned FALSE but did not fill in @error; fix the backend!\n",
				   G_OBJECT_TYPE_NAME (document));

			/* So upper layers don't crash */
			g_set_error_literal (error,
					     EV_DOCUMENT_ERROR,
					     EV_DOCUMENT_ERROR_INVALID,
					     "Internal error in backend");
		}
	}

	return retval;
}

/**
 * ev_document_save:
 * @document:
 * @uri: the target URI
 * @error: a #GError location to store an error, or %NULL
 *
 * Saves @document to @uri.
 * 
 * Returns: %TRUE on success, or %FALSE on error with @error filled in
 */
gboolean
ev_document_save (EvDocument  *document,
		  const char  *uri,
		  GError     **error)
{
	EvDocumentClass *klass = EV_DOCUMENT_GET_CLASS (document);

	return klass->save (document, uri, error);
}

int
ev_document_get_n_pages (EvDocument  *document)
{
	EvDocumentClass *klass = EV_DOCUMENT_GET_CLASS (document);

	return klass->get_n_pages (document);
}

EvPage *
ev_document_get_page (EvDocument *document,
		      gint        index)
{
	EvDocumentClass *klass = EV_DOCUMENT_GET_CLASS (document);

	return klass->get_page (document, index);
}

void
ev_document_get_page_size (EvDocument *document,
			   EvPage     *page,
			   double     *width,
			   double     *height)
{
	EvDocumentClass *klass = EV_DOCUMENT_GET_CLASS (document);

	klass->get_page_size (document, page, width, height);
}

gchar *
ev_document_get_page_label (EvDocument *document,
			    EvPage     *page)
{
	EvDocumentClass *klass = EV_DOCUMENT_GET_CLASS (document);

	return klass->get_page_label ?
		klass->get_page_label (document, page) : NULL;
}

EvDocumentInfo *
ev_document_get_info (EvDocument *document)
{
	EvDocumentClass *klass = EV_DOCUMENT_GET_CLASS (document);

	return klass->get_info (document);
}

cairo_surface_t *
ev_document_render (EvDocument      *document,
		    EvRenderContext *rc)
{
	EvDocumentClass *klass = EV_DOCUMENT_GET_CLASS (document);

	return klass->render (document, rc);
}

/* EvDocumentInfo */
EV_DEFINE_BOXED_TYPE (EvDocumentInfo, ev_document_info, ev_document_info_copy, ev_document_info_free)

EvDocumentInfo *
ev_document_info_copy (EvDocumentInfo *info)
{
	EvDocumentInfo *copy;
	
	g_return_val_if_fail (info != NULL, NULL);

	copy = g_new0 (EvDocumentInfo, 1);
	copy->title = g_strdup (info->title);
	copy->format = g_strdup (info->format);
	copy->author = g_strdup (info->author);
	copy->subject = g_strdup (info->subject);
	copy->keywords = g_strdup (info->keywords);
	copy->security = g_strdup (info->security);
	copy->creator = g_strdup (info->creator);
	copy->producer = g_strdup (info->producer);
	copy->linearized = g_strdup (info->linearized);
	
	copy->creation_date = info->creation_date;
	copy->modified_date = info->modified_date;
	copy->layout = info->layout;
	copy->mode = info->mode;
	copy->ui_hints = info->ui_hints;
	copy->permissions = info->permissions;
	copy->n_pages = info->n_pages;
	copy->fields_mask = info->fields_mask;

	return copy;
}

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
	
	g_free (info);
}

/* EvRectangle */
EV_DEFINE_BOXED_TYPE (EvRectangle, ev_rectangle, ev_rectangle_copy, ev_rectangle_free)

EvRectangle *
ev_rectangle_new (void)
{
	return g_new0 (EvRectangle, 1);
}

EvRectangle *
ev_rectangle_copy (EvRectangle *rectangle)
{
	EvRectangle *new_rectangle;

	g_return_val_if_fail (rectangle != NULL, NULL);

	new_rectangle = g_new (EvRectangle, 1);
	*new_rectangle = *rectangle;

	return new_rectangle;
}

void
ev_rectangle_free (EvRectangle *rectangle)
{
	g_free (rectangle);
}

/* Compares two rects.  returns 0 if they're equal */
#define EPSILON 0.0000001

gint
ev_rect_cmp (EvRectangle *a,
	     EvRectangle *b)
{
	if (a == b)
		return 0;
	if (a == NULL || b == NULL)
		return 1;

	return ! ((ABS (a->x1 - b->x1) < EPSILON) &&
		  (ABS (a->y1 - b->y1) < EPSILON) &&
		  (ABS (a->x2 - b->x2) < EPSILON) &&
		  (ABS (a->y2 - b->y2) < EPSILON));
}
