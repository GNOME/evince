/* ev-document-transition.c
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

#include <config.h>
#include "ev-document-transition.h"

EV_DEFINE_INTERFACE (EvDocumentTransition, ev_document_transition, 0)

static void
ev_document_transition_class_init (EvDocumentTransitionIface *klass)
{
}

gdouble
ev_document_transition_get_page_duration (EvDocumentTransition *document_trans,
					  gint                  page)
{
	EvDocumentTransitionIface *iface = EV_DOCUMENT_TRANSITION_GET_IFACE (document_trans);

	if (iface->get_page_duration)
		return iface->get_page_duration (document_trans, page);

	return -1;
}

EvTransitionEffect *
ev_document_transition_get_effect (EvDocumentTransition *document_trans,
				   gint                  page)
{
	EvDocumentTransitionIface *iface = EV_DOCUMENT_TRANSITION_GET_IFACE (document_trans);
	EvTransitionEffect *effect = NULL;

	if (iface->get_effect)
		effect = iface->get_effect (document_trans, page);

	if (!effect)
		return ev_transition_effect_new (EV_TRANSITION_EFFECT_REPLACE, NULL);

	return effect;
}
