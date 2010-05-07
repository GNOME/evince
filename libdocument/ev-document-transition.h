/* ev-document-transition.h
 *  this file is part of evince, a gnome document viewer
 * 
 * Copyright (C) 2006 Carlos Garcia Campos <carlosgc@gnome.org>
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

#ifndef EV_DOCUMENT_TRANSITION_H
#define EV_DOCUMENT_TRANSITION_H

#include <glib-object.h>

#include "ev-document.h"
#include "ev-transition-effect.h"

G_BEGIN_DECLS

#define EV_TYPE_DOCUMENT_TRANSITION	       (ev_document_transition_get_type ())
#define EV_DOCUMENT_TRANSITION(o)	       (G_TYPE_CHECK_INSTANCE_CAST ((o), EV_TYPE_DOCUMENT_TRANSITION, EvDocumentTransition))
#define EV_DOCUMENT_TRANSITION_IFACE(k)	       (G_TYPE_CHECK_CLASS_CAST((k), EV_TYPE_DOCUMENT_TRANSITION, EvDocumentTransitionInterface))
#define EV_IS_DOCUMENT_TRANSITION(o)	       (G_TYPE_CHECK_INSTANCE_TYPE ((o), EV_TYPE_DOCUMENT_TRANSITION))
#define EV_IS_DOCUMENT_TRANSITION_IFACE(k)     (G_TYPE_CHECK_CLASS_TYPE ((k), EV_TYPE_DOCUMENT_TRANSITION))
#define EV_DOCUMENT_TRANSITION_GET_IFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EV_TYPE_DOCUMENT_TRANSITION, EvDocumentTransitionInterface))

typedef struct _EvDocumentTransition          EvDocumentTransition;
typedef struct _EvDocumentTransitionInterface EvDocumentTransitionInterface;

struct _EvDocumentTransitionInterface
{
	GTypeInterface base_iface;

	/* Methods  */
	gdouble              (* get_page_duration) (EvDocumentTransition *document_trans,
						    gint                  page);
	EvTransitionEffect * (* get_effect)        (EvDocumentTransition *document_trans,
						    gint                  page);
};

GType                ev_document_transition_get_type          (void) G_GNUC_CONST;
gdouble              ev_document_transition_get_page_duration (EvDocumentTransition *document_trans,
							       gint                  page);
EvTransitionEffect * ev_document_transition_get_effect        (EvDocumentTransition *document_trans,
							       gint                  page);

G_END_DECLS

#endif /* EV_DOCUMENT_TRANSITION_H */
