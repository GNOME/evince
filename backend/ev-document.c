/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/*
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
#include "ev-backend-marshalers.h"
#include "ev-job-queue.h"

static void ev_document_class_init (gpointer g_class);

enum
{
	PAGE_CHANGED,
	SCALE_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

GType
ev_document_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		static const GTypeInfo our_info =
		{
			sizeof (EvDocumentIface),
			NULL,
			NULL,
			(GClassInitFunc)ev_document_class_init
		};

		type = g_type_register_static (G_TYPE_INTERFACE,
					       "EvDocument",
					       &our_info, (GTypeFlags)0);
	}

	return type;
}

GQuark
ev_document_error_quark (void)
{
  static GQuark q = 0;
  if (q == 0)
    q = g_quark_from_static_string ("ev-document-error-quark");

  return q;
}

static void
ev_document_class_init (gpointer g_class)
{
	signals[PAGE_CHANGED] =
		g_signal_new ("page_changed",
			      EV_TYPE_DOCUMENT,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EvDocumentIface, page_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	signals[SCALE_CHANGED] =
		g_signal_new ("scale_changed",
			      EV_TYPE_DOCUMENT,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EvDocumentIface, scale_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	g_object_interface_install_property (g_class,
				g_param_spec_string ("title",
						     "Document Title",
						     "The title of the document",
						     NULL,
						     G_PARAM_READABLE));
}

gboolean
ev_document_load (EvDocument  *document,
		  const char  *uri,
		  GError     **error)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	gboolean retval;

	g_mutex_lock (EV_DOC_MUTEX);
	retval = iface->load (document, uri, error);
	g_mutex_unlock (EV_DOC_MUTEX);

	return retval;
}

gboolean
ev_document_save (EvDocument  *document,
		  const char  *uri,
		  GError     **error)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	gboolean retval;

	g_mutex_lock (EV_DOC_MUTEX);
	retval = iface->save (document, uri, error);
	g_mutex_unlock (EV_DOC_MUTEX);

	return retval;
}

char *
ev_document_get_title (EvDocument  *document)
{
	char *title;

	g_mutex_lock (EV_DOC_MUTEX);
	g_object_get (document, "title", &title, NULL);
	g_mutex_unlock (EV_DOC_MUTEX);

	return title;
}

int
ev_document_get_n_pages (EvDocument  *document)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	gint retval;

	g_mutex_lock (EV_DOC_MUTEX);
	retval = iface->get_n_pages (document);
	g_mutex_unlock (EV_DOC_MUTEX);

	return retval;
}

void
ev_document_set_page (EvDocument  *document,
		      int          page)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);

	g_mutex_lock (EV_DOC_MUTEX);
	iface->set_page (document, page);
	g_mutex_unlock (EV_DOC_MUTEX);
}

int
ev_document_get_page (EvDocument *document)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	int retval;

	g_mutex_lock (EV_DOC_MUTEX);
	retval = iface->get_page (document);
	g_mutex_unlock (EV_DOC_MUTEX);

	return retval;
}

void
ev_document_set_target (EvDocument  *document,
			GdkDrawable *target)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);

	g_mutex_lock (EV_DOC_MUTEX);
	iface->set_target (document, target);
	g_mutex_unlock (EV_DOC_MUTEX);
}

void
ev_document_set_scale (EvDocument   *document,
		       double        scale)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);

	g_mutex_lock (EV_DOC_MUTEX);
	iface->set_scale (document, scale);
	g_mutex_unlock (EV_DOC_MUTEX);
}

void
ev_document_set_page_offset (EvDocument  *document,
			     int          x,
			     int          y)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);

	g_mutex_lock (EV_DOC_MUTEX);
	iface->set_page_offset (document, x, y);
	g_mutex_unlock (EV_DOC_MUTEX);
}

void
ev_document_get_page_size   (EvDocument   *document,
			     int           page,
			     int          *width,
			     int          *height)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);

	/* FIXME: This is clearly unsafe.  But we can prolly get away with it in
	 * the short term to test. */
//	g_mutex_lock (EV_DOC_MUTEX);
	iface->get_page_size (document, page, width, height);
//	g_mutex_unlock (EV_DOC_MUTEX);
}

char *
ev_document_get_text (EvDocument   *document,
		      GdkRectangle *rect)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	char *retval;

	g_mutex_lock (EV_DOC_MUTEX);
	retval = iface->get_text (document, rect);
	g_mutex_unlock (EV_DOC_MUTEX);

	return retval;
}

EvLink *
ev_document_get_link (EvDocument   *document,
		      int           x,
		      int	    y)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	EvLink *retval;

	g_mutex_lock (EV_DOC_MUTEX);
	retval = iface->get_link (document, x, y);
	g_mutex_unlock (EV_DOC_MUTEX);

	return retval;
}

void
ev_document_render (EvDocument  *document,
		    int          clip_x,
		    int          clip_y,
		    int          clip_width,
		    int          clip_height)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);

	g_mutex_lock (EV_DOC_MUTEX);
	iface->render (document, clip_x, clip_y, clip_width, clip_height);
	g_mutex_unlock (EV_DOC_MUTEX);
}


GdkPixbuf *
ev_document_render_pixbuf (EvDocument *document)
{
	EvDocumentIface *iface = EV_DOCUMENT_GET_IFACE (document);
	GdkPixbuf *retval;

	g_assert (iface->render_pixbuf);

	retval = iface->render_pixbuf (document);

	return retval;
}


void
ev_document_page_changed (EvDocument *document)
{
	g_signal_emit (G_OBJECT (document), signals[PAGE_CHANGED], 0);
}

void
ev_document_scale_changed (EvDocument *document)
{
	g_signal_emit (G_OBJECT (document), signals[SCALE_CHANGED], 0);
}		    
