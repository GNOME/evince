/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2015 Igalia S.L.
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

#ifndef __EV_MEDIA_H__
#define __EV_MEDIA_H__

#include <glib-object.h>
#include "ev-page.h"

G_BEGIN_DECLS

typedef struct _EvMedia        EvMedia;
typedef struct _EvMediaClass   EvMediaClass;
typedef struct _EvMediaPrivate EvMediaPrivate;

#define EV_TYPE_MEDIA              (ev_media_get_type())
#define EV_MEDIA(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), EV_TYPE_MEDIA, EvMedia))
#define EV_IS_MEDIA(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), EV_TYPE_MEDIA))
#define EV_MEDIA_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), EV_TYPE_MEDIA, EvMediaClass))
#define EV_IS_MEDIA_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), EV_TYPE_MEDIA))
#define EV_MEDIA_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), EV_TYPE_MEDIA, EvMediaClass))

struct _EvMedia {
        GObject base_instance;

        EvMediaPrivate *priv;
};

struct _EvMediaClass {
        GObjectClass base_class;
};

GType        ev_media_get_type          (void) G_GNUC_CONST;

EvMedia     *ev_media_new_for_uri       (EvPage      *page,
                                         const gchar *uri);
const gchar *ev_media_get_uri           (EvMedia     *media);
guint        ev_media_get_page_index    (EvMedia     *media);
gboolean     ev_media_get_show_controls (EvMedia     *media);
void         ev_media_set_show_controls (EvMedia     *media,
                                         gboolean     show_controls);

G_END_DECLS

#endif /* __EV_MEDIA_H__ */
