/* this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2010 Carlos Garcia Campos <carlosgc@gnome.org>
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

#include <config.h>

#include <glib/gi18n-lib.h>
#include <libgxps/gxps.h>

#include "xps-document.h"
#include "ev-document-links.h"
#include "ev-document-print.h"
#include "ev-document-misc.h"

struct _XPSDocument {
	EvDocument    object;

	GFile        *file;
	GXPSFile     *xps;
	GXPSDocument *doc;
};

struct _XPSDocumentClass {
	EvDocumentClass parent_class;
};

static void xps_document_document_links_iface_init (EvDocumentLinksInterface *iface);
static void xps_document_document_print_iface_init (EvDocumentPrintInterface *iface);

G_DEFINE_TYPE_WITH_CODE (XPSDocument, xps_document, EV_TYPE_DOCUMENT,
			 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_LINKS,
						xps_document_document_links_iface_init)
			 G_IMPLEMENT_INTERFACE (EV_TYPE_DOCUMENT_PRINT,
						xps_document_document_print_iface_init))

/* XPSDocument */
static void
xps_document_init (XPSDocument *ps_document)
{
}

static void
xps_document_dispose (GObject *object)
{
	XPSDocument *xps = XPS_DOCUMENT (object);

	if (xps->file) {
		g_object_unref (xps->file);
		xps->file = NULL;
	}

	if (xps->xps) {
		g_object_unref (xps->xps);
		xps->xps = NULL;
	}

	if (xps->doc) {
		g_object_unref (xps->doc);
		xps->doc = NULL;
	}

	G_OBJECT_CLASS (xps_document_parent_class)->dispose (object);
}

/* EvDocumentIface */
static gboolean
xps_document_load (EvDocument *document,
		   const char *uri,
		   GError    **error)
{
	XPSDocument *xps = XPS_DOCUMENT (document);

	xps->file = g_file_new_for_uri (uri);
	xps->xps = gxps_file_new (xps->file, error);

	if (!xps->xps)
		return FALSE;

	/* FIXME: what if there are multiple docs? */
	xps->doc = gxps_file_get_document (xps->xps, 0, error);
	if (!xps->doc) {
		g_object_unref (xps->xps);
		xps->xps = NULL;

		return FALSE;
	}

	return TRUE;
}

static gboolean
xps_document_save (EvDocument *document,
		   const char *uri,
		   GError    **error)
{
	XPSDocument *xps = XPS_DOCUMENT (document);
	GFile       *dest;
	gboolean     retval;

	dest = g_file_new_for_uri (uri);
	retval = g_file_copy (xps->file, dest,
			      G_FILE_COPY_TARGET_DEFAULT_PERMS |
			      G_FILE_COPY_OVERWRITE,
			      NULL, NULL, NULL, error);
	g_object_unref (dest);

	return retval;
}

static gint
xps_document_get_n_pages (EvDocument *document)
{
	XPSDocument *xps = XPS_DOCUMENT (document);

	return gxps_document_get_n_pages (xps->doc);
}

static EvPage *
xps_document_get_page (EvDocument *document,
		       gint        index)
{
	XPSDocument *xps = XPS_DOCUMENT (document);
	GXPSPage    *xps_page;
	EvPage      *page;

	xps_page = gxps_document_get_page (xps->doc, index, NULL);
	page = ev_page_new (index);
	if (xps_page) {
		page->backend_page = (EvBackendPage)xps_page;
		page->backend_destroy_func = (EvBackendPageDestroyFunc)g_object_unref;
	}

	return page;
}

static void
xps_document_get_page_size (EvDocument *document,
			    EvPage     *page,
			    double     *width,
			    double     *height)
{
	gxps_page_get_size (GXPS_PAGE (page->backend_page), width, height);
}

static EvDocumentInfo *
xps_document_get_info (EvDocument *document)
{
	XPSDocument    *xps = XPS_DOCUMENT (document);
	EvDocumentInfo *info;

	info = ev_document_info_new ();
	info->fields_mask |=
		EV_DOCUMENT_INFO_N_PAGES |
		EV_DOCUMENT_INFO_PAPER_SIZE;


        info->n_pages = gxps_document_get_n_pages (xps->doc);
	if (info->n_pages > 0) {
                GXPSPage *gxps_page;

                gxps_page = gxps_document_get_page (xps->doc, 0, NULL);
		gxps_page_get_size (gxps_page, &(info->paper_width), &(info->paper_height));
                g_object_unref (gxps_page);

		info->paper_width  = info->paper_width / 96.0f * 25.4f;
		info->paper_height = info->paper_height / 96.0f * 25.4f;
	}

	return info;
}

static gboolean
xps_document_get_backend_info (EvDocument            *document,
			       EvDocumentBackendInfo *info)
{
	info->name = "libgxps";
	info->version = GXPS_VERSION_STRING;

	return TRUE;
}

static cairo_surface_t *
xps_document_render (EvDocument      *document,
		     EvRenderContext *rc)
{
	GXPSPage        *xps_page;
	gdouble          page_width, page_height;
	gint             width, height;
	double           scale_x, scale_y;
	cairo_surface_t *surface;
	cairo_t         *cr;
	GError          *error = NULL;

	xps_page = GXPS_PAGE (rc->page->backend_page);

	gxps_page_get_size (xps_page, &page_width, &page_height);
	ev_render_context_compute_transformed_size (rc, page_width, page_height,
                                                    &width, &height);

	surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
					      width, height);
	cr = cairo_create (surface);

	cairo_set_source_rgb (cr, 1., 1., 1.);
	cairo_paint (cr);

	switch (rc->rotation) {
	case 90:
		cairo_translate (cr, width, 0);
		break;
	case 180:
		cairo_translate (cr, width, height);
		break;
	case 270:
		cairo_translate (cr, 0, height);
		break;
	default:
		cairo_translate (cr, 0, 0);
	}

	ev_render_context_compute_scales (rc, page_width, page_height,
					  &scale_x, &scale_y);
	cairo_scale (cr, scale_x, scale_y);

	cairo_rotate (cr, rc->rotation * G_PI / 180.0);
	gxps_page_render (xps_page, cr, &error);
	cairo_destroy (cr);

	if (error) {
		g_warning ("Error rendering page %d: %s\n",
			   rc->page->index, error->message);
		g_error_free (error);
	}

	return surface;
}

static void
xps_document_class_init (XPSDocumentClass *klass)
{
	GObjectClass    *object_class = G_OBJECT_CLASS (klass);
	EvDocumentClass *ev_document_class = EV_DOCUMENT_CLASS (klass);

	object_class->dispose = xps_document_dispose;

	ev_document_class->load = xps_document_load;
	ev_document_class->save = xps_document_save;
	ev_document_class->get_n_pages = xps_document_get_n_pages;
	ev_document_class->get_page = xps_document_get_page;
	ev_document_class->get_page_size = xps_document_get_page_size;
	ev_document_class->get_info = xps_document_get_info;
	ev_document_class->get_backend_info = xps_document_get_backend_info;
	ev_document_class->render = xps_document_render;
}

/* EvDocumentLinks */
static gboolean
xps_document_links_has_document_links (EvDocumentLinks *document_links)
{
	XPSDocument           *xps_document = XPS_DOCUMENT (document_links);
	GXPSDocumentStructure *structure;
	gboolean               retval;

	structure = gxps_document_get_structure (xps_document->doc);
	if (!structure)
		return FALSE;

	retval = gxps_document_structure_has_outline (structure);
	g_object_unref (structure);

	return retval;
}

static EvLinkAction *
link_action_from_target (XPSDocument    *xps_document,
                         GXPSLinkTarget *target)
{
	EvLinkAction *ev_action;

	if (gxps_link_target_is_internal (target)) {
		EvLinkDest  *dest = NULL;
		gint         doc;
		const gchar *anchor;

		anchor = gxps_link_target_get_anchor (target);

		/* FIXME: multidoc */
		doc = gxps_file_get_document_for_link_target (xps_document->xps, target);
		if (doc == 0) {
			if (!anchor)
				return NULL;

			dest = ev_link_dest_new_named (anchor);
			ev_action = ev_link_action_new_dest (dest);
			g_object_unref (dest);
		} else if (doc == -1 && anchor &&
			   gxps_document_get_page_for_anchor (xps_document->doc, anchor) >= 0) {
			/* Internal, but source is not a doc,
			 * let's try with doc = 0
			 */
			dest = ev_link_dest_new_named (anchor);
			ev_action = ev_link_action_new_dest (dest);
			g_object_unref (dest);
		} else {
			gchar *filename;

			/* FIXME: remote uri? */
			filename = g_file_get_path (xps_document->file);

			if (anchor)
				dest = ev_link_dest_new_named (anchor);
			ev_action = ev_link_action_new_remote (dest, filename);
			g_clear_object (&dest);
			g_free (filename);
		}
	} else {
		const gchar *uri;

		uri = gxps_link_target_get_uri (target);
		ev_action = ev_link_action_new_external_uri (uri);
	}

        return ev_action;
}

static void
build_tree (XPSDocument     *xps_document,
	    GtkTreeModel    *model,
	    GtkTreeIter     *parent,
	    GXPSOutlineIter *iter)
{
	do {
		GtkTreeIter     tree_iter;
		GXPSOutlineIter child_iter;
                EvLinkAction   *action;
		EvLink         *link;
		GXPSLinkTarget *target;
		gchar          *title;

		target = gxps_outline_iter_get_target (iter);
		title = g_markup_escape_text (gxps_outline_iter_get_description (iter), -1);
                action = link_action_from_target (xps_document, target);
		link = ev_link_new (title, action);
                g_object_unref (action);
		gxps_link_target_free (target);

		gtk_tree_store_append (GTK_TREE_STORE (model), &tree_iter, parent);
		gtk_tree_store_set (GTK_TREE_STORE (model), &tree_iter,
				    EV_DOCUMENT_LINKS_COLUMN_MARKUP, title,
				    EV_DOCUMENT_LINKS_COLUMN_LINK, link,
				    EV_DOCUMENT_LINKS_COLUMN_EXPAND, FALSE,
				    -1);
		g_object_unref (link);
		g_free (title);

		if (gxps_outline_iter_children (&child_iter, iter))
			build_tree (xps_document, model, &tree_iter, &child_iter);
	} while (gxps_outline_iter_next (iter));
}

static GtkTreeModel *
xps_document_links_get_links_model (EvDocumentLinks *document_links)
{
	XPSDocument           *xps_document = XPS_DOCUMENT (document_links);
	GXPSDocumentStructure *structure;
	GXPSOutlineIter        iter;
	GtkTreeModel          *model = NULL;

	structure = gxps_document_get_structure (xps_document->doc);
	if (!structure)
		return NULL;

	if (gxps_document_structure_outline_iter_init (&iter, structure)) {
		model = (GtkTreeModel *) gtk_tree_store_new (EV_DOCUMENT_LINKS_COLUMN_NUM_COLUMNS,
							     G_TYPE_STRING,
							     G_TYPE_OBJECT,
							     G_TYPE_BOOLEAN,
							     G_TYPE_STRING);
		build_tree (xps_document, model, NULL, &iter);
	}

	g_object_unref (structure);

	return model;
}

static EvMappingList *
xps_document_links_get_links (EvDocumentLinks *document_links,
			      EvPage          *page)
{
	XPSDocument *xps_document = XPS_DOCUMENT (document_links);
	GXPSPage    *xps_page;
	GList       *retval = NULL;
	GList       *mapping_list;
	GList       *list;

	xps_page = GXPS_PAGE (page->backend_page);
	mapping_list = gxps_page_get_links (xps_page, NULL);

	for (list = mapping_list; list; list = list->next) {
		GXPSLink *xps_link;
		GXPSLinkTarget *target;
                EvLinkAction *action;
		EvMapping *ev_link_mapping;
		cairo_rectangle_t area;

		xps_link = (GXPSLink *)list->data;
		ev_link_mapping = g_new (EvMapping, 1);
		gxps_link_get_area (xps_link, &area);
		target = gxps_link_get_target (xps_link);
                action = link_action_from_target (xps_document, target);

		ev_link_mapping->data = ev_link_new (NULL, action);
		ev_link_mapping->area.x1 = area.x;
		ev_link_mapping->area.x2 = area.x + area.width;
		ev_link_mapping->area.y1 = area.y;
		ev_link_mapping->area.y2 = area.y + area.height;

		retval = g_list_prepend (retval, ev_link_mapping);
		gxps_link_free (xps_link);
                g_object_unref (action);
	}

	g_list_free (mapping_list);

	return ev_mapping_list_new (page->index, g_list_reverse (retval), (GDestroyNotify)g_object_unref);
}

static EvLinkDest *
xps_document_links_find_link_dest (EvDocumentLinks *document_links,
				   const gchar     *link_name)
{
	XPSDocument       *xps_document = XPS_DOCUMENT (document_links);
	GXPSPage          *xps_page;
	gint               page;
	cairo_rectangle_t  area;
	EvLinkDest        *dest = NULL;

	page = gxps_document_get_page_for_anchor (xps_document->doc, link_name);
	if (page == -1)
		return NULL;

	xps_page = gxps_document_get_page (xps_document->doc, page, NULL);
	if (!xps_page)
		return NULL;

	if (gxps_page_get_anchor_destination (xps_page, link_name, &area, NULL))
		dest = ev_link_dest_new_xyz (page, area.x, area.y, 1., TRUE, TRUE, FALSE);

	g_object_unref (xps_page);

	return dest;
}

static gint
xps_document_links_find_link_page (EvDocumentLinks *document_links,
				   const gchar     *link_name)
{
	XPSDocument *xps_document = XPS_DOCUMENT (document_links);

	return gxps_document_get_page_for_anchor (xps_document->doc, link_name);
}

static void
xps_document_document_links_iface_init (EvDocumentLinksInterface *iface)
{
	iface->has_document_links = xps_document_links_has_document_links;
	iface->get_links_model = xps_document_links_get_links_model;
	iface->get_links = xps_document_links_get_links;
	iface->find_link_dest = xps_document_links_find_link_dest;
	iface->find_link_page = xps_document_links_find_link_page;
}

/* EvDocumentPrint */
static void
xps_document_print_print_page (EvDocumentPrint *document,
			       EvPage          *page,
			       cairo_t         *cr)
{
	GError *error = NULL;

	gxps_page_render (GXPS_PAGE (page->backend_page), cr, &error);
	if (error) {
		g_warning ("Error rendering page %d for printing: %s\n",
			   page->index, error->message);
		g_error_free (error);
	}
}

static void
xps_document_document_print_iface_init (EvDocumentPrintInterface *iface)
{
	iface->print_page = xps_document_print_print_page;
}
