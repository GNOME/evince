/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2006 Carlos Garcia Campos <carlosgc@gnome.org>
 *  Copyright (C) 2005 Red Hat, Inc.
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

#pragma once

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#include <glib-object.h>

#include "ev-macros.h"

G_BEGIN_DECLS

#define EV_TYPE_LINK_DEST              (ev_link_dest_get_type())

EV_PUBLIC
G_DECLARE_FINAL_TYPE (EvLinkDest, ev_link_dest, EV, LINK_DEST, GObject)

typedef enum {
	EV_LINK_DEST_TYPE_PAGE,
	EV_LINK_DEST_TYPE_XYZ,
	EV_LINK_DEST_TYPE_FIT,
	EV_LINK_DEST_TYPE_FITH,
	EV_LINK_DEST_TYPE_FITV,
	EV_LINK_DEST_TYPE_FITR,
	EV_LINK_DEST_TYPE_NAMED,
	EV_LINK_DEST_TYPE_PAGE_LABEL,
	EV_LINK_DEST_TYPE_UNKNOWN
} EvLinkDestType;

EV_PUBLIC
EvLinkDestType  ev_link_dest_get_dest_type  (EvLinkDest  *self);
EV_PUBLIC
gint            ev_link_dest_get_page       (EvLinkDest  *self);
EV_PUBLIC
gdouble         ev_link_dest_get_top        (EvLinkDest  *self,
					     gboolean    *change_top);
EV_PUBLIC
gdouble         ev_link_dest_get_left       (EvLinkDest  *self,
					     gboolean    *change_left);
EV_PUBLIC
gdouble         ev_link_dest_get_bottom     (EvLinkDest  *self);
EV_PUBLIC
gdouble         ev_link_dest_get_right      (EvLinkDest  *self);
EV_PUBLIC
gdouble         ev_link_dest_get_zoom       (EvLinkDest  *self,
					     gboolean    *change_zoom);
EV_PUBLIC
const gchar    *ev_link_dest_get_named_dest (EvLinkDest  *self);
EV_PUBLIC
const gchar    *ev_link_dest_get_page_label (EvLinkDest  *self);

EV_PUBLIC
EvLinkDest     *ev_link_dest_new_page       (gint         page);
EV_PUBLIC
EvLinkDest     *ev_link_dest_new_xyz        (gint         page,
					     gdouble      left,
					     gdouble      top,
					     gdouble      zoom,
					     gboolean     change_left,
					     gboolean     change_top,
					     gboolean     change_zoom);
EV_PUBLIC
EvLinkDest     *ev_link_dest_new_fit        (gint         page);
EV_PUBLIC
EvLinkDest     *ev_link_dest_new_fith       (gint         page,
					     gdouble      top,
					     gboolean     change_top);
EV_PUBLIC
EvLinkDest     *ev_link_dest_new_fitv       (gint         page,
					     gdouble      left,
					     gboolean     change_left);
EV_PUBLIC
EvLinkDest     *ev_link_dest_new_fitr       (gint         page,
					     gdouble      left,
					     gdouble      bottom,
					     gdouble      right,
					     gdouble      top);
EV_PUBLIC
EvLinkDest     *ev_link_dest_new_named      (const gchar *named_dest);
EV_PUBLIC
EvLinkDest     *ev_link_dest_new_page_label (const gchar *page_label);

EV_PUBLIC
gboolean        ev_link_dest_equal          (EvLinkDest  *a,
                                             EvLinkDest  *b);

G_END_DECLS
