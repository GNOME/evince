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

#if !defined (__EV_EVINCE_DOCUMENT_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-document.h> can be included directly."
#endif

#ifndef __EV_IMAGE_H__
#define __EV_IMAGE_H__

#include <glib-object.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef struct _EvImage        EvImage;
typedef struct _EvImageClass   EvImageClass;
typedef struct _EvImagePrivate EvImagePrivate;

#define EV_TYPE_IMAGE              (ev_image_get_type())
#define EV_IMAGE(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_IMAGE, EvImage))
#define EV_IMAGE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_IMAGE, EvImageClass))
#define EV_IS_IMAGE(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_IMAGE))
#define EV_IS_IMAGE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_IMAGE))
#define EV_IMAGE_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_IMAGE, EvImageClass))

struct _EvImage {
	GObject base_instance;
	
	EvImagePrivate *priv;
};

struct _EvImageClass {
	GObjectClass base_class;
};

GType        ev_image_get_type         (void) G_GNUC_CONST;
EvImage     *ev_image_new              (gint             page,
					gint             img_id);
EvImage     *ev_image_new_from_pixbuf  (GdkPixbuf       *pixbuf);

gint         ev_image_get_id           (EvImage         *image);
gint         ev_image_get_page         (EvImage         *image);
GdkPixbuf   *ev_image_get_pixbuf       (EvImage         *image);
const gchar *ev_image_save_tmp         (EvImage         *image,
					GdkPixbuf       *pixbuf);
const gchar *ev_image_get_tmp_uri      (EvImage         *image);


G_END_DECLS

#endif /* __EV_IMAGE_H__ */
