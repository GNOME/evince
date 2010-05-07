/* ev-document-layers.h
 *  this file is part of evince, a gnome document viewer
 * 
 * Copyright (C) 2008 Carlos Garcia Campos  <carlosgc@gnome.org>
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

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#ifndef EV_DOCUMENT_LAYERS_H
#define EV_DOCUMENT_LAYERS_H

#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "ev-layer.h"

G_BEGIN_DECLS

#define EV_TYPE_DOCUMENT_LAYERS		   (ev_document_layers_get_type ())
#define EV_DOCUMENT_LAYERS(o)		   (G_TYPE_CHECK_INSTANCE_CAST ((o), EV_TYPE_DOCUMENT_LAYERS, EvDocumentLayers))
#define EV_DOCUMENT_LAYERS_IFACE(k)	   (G_TYPE_CHECK_CLASS_CAST((k), EV_TYPE_DOCUMENT_LAYERS, EvDocumentLayersInterface))
#define EV_IS_DOCUMENT_LAYERS(o)	   (G_TYPE_CHECK_INSTANCE_TYPE ((o), EV_TYPE_DOCUMENT_LAYERS))
#define EV_IS_DOCUMENT_LAYERS_IFACE(k)	   (G_TYPE_CHECK_CLASS_TYPE ((k), EV_TYPE_DOCUMENT_LAYERS))
#define EV_DOCUMENT_LAYERS_GET_IFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EV_TYPE_DOCUMENT_LAYERS, EvDocumentLayersInterface))

typedef struct _EvDocumentLayers          EvDocumentLayers;
typedef struct _EvDocumentLayersInterface EvDocumentLayersInterface;

enum {
	EV_DOCUMENT_LAYERS_COLUMN_TITLE,
	EV_DOCUMENT_LAYERS_COLUMN_LAYER,
	EV_DOCUMENT_LAYERS_COLUMN_VISIBLE,
	EV_DOCUMENT_LAYERS_COLUMN_ENABLED,
	EV_DOCUMENT_LAYERS_COLUMN_SHOWTOGGLE,
	EV_DOCUMENT_LAYERS_COLUMN_RBGROUP,
	EV_DOCUMENT_LAYERS_N_COLUMNS
};

struct _EvDocumentLayersInterface
{
	GTypeInterface base_iface;

	/* Methods  */
	gboolean      (* has_layers)       (EvDocumentLayers *document_layers);
	GtkTreeModel *(* get_layers)       (EvDocumentLayers *document_layers);

	void          (* show_layer)       (EvDocumentLayers *document_layers,
					    EvLayer          *layer);
	void          (* hide_layer)       (EvDocumentLayers *document_layers,
					    EvLayer          *layer);
	gboolean      (* layer_is_visible) (EvDocumentLayers *document_layers,
					    EvLayer          *layer);
};

GType         ev_document_layers_get_type         (void) G_GNUC_CONST;

gboolean      ev_document_layers_has_layers       (EvDocumentLayers *document_layers);
GtkTreeModel *ev_document_layers_get_layers       (EvDocumentLayers *document_layers);
void          ev_document_layers_show_layer       (EvDocumentLayers *document_layers,
						   EvLayer          *layer);
void          ev_document_layers_hide_layer       (EvDocumentLayers *document_layers,
						   EvLayer          *layer);
gboolean      ev_document_layers_layer_is_visible (EvDocumentLayers *document_layers,
						   EvLayer          *layer);

G_END_DECLS

#endif /* EV_DOCUMENT_LAYERS_H */
