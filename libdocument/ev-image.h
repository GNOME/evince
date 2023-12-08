/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2006 Carlos Garcia Campos <carlosgc@gnome.org>
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
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "ev-macros.h"

G_BEGIN_DECLS

#define EV_TYPE_IMAGE              (ev_image_get_type())

EV_PUBLIC
G_DECLARE_FINAL_TYPE (EvImage, ev_image, EV, IMAGE, GObject)

struct _EvImage {
	GObject base_instance;
};

EV_PUBLIC
EvImage     *ev_image_new              (gint             page,
					gint             img_id);
EV_PUBLIC
EvImage     *ev_image_new_from_pixbuf  (GdkPixbuf       *pixbuf);

EV_PUBLIC
gint         ev_image_get_id           (EvImage         *image);
EV_PUBLIC
gint         ev_image_get_page         (EvImage         *image);
EV_PUBLIC
GdkPixbuf   *ev_image_get_pixbuf       (EvImage         *image);
EV_PUBLIC
const gchar *ev_image_save_tmp         (EvImage         *image,
					GdkPixbuf       *pixbuf);
EV_PUBLIC
const gchar *ev_image_get_tmp_uri      (EvImage         *image);

G_END_DECLS
