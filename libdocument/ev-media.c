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

#include <config.h>

#include "ev-media.h"

struct _EvMediaPrivate {
        guint    page;
        gchar   *uri;
        gboolean show_controls;
};

G_DEFINE_TYPE_WITH_PRIVATE (EvMedia, ev_media, G_TYPE_OBJECT)

static void
ev_media_finalize (GObject *object)
{
        EvMedia *media = EV_MEDIA (object);

        g_clear_pointer (&media->priv->uri, g_free);

        G_OBJECT_CLASS (ev_media_parent_class)->finalize (object);
}

static void
ev_media_class_init (EvMediaClass *klass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

        g_object_class->finalize = ev_media_finalize;
}

static void
ev_media_init (EvMedia *media)
{
        media->priv = ev_media_get_instance_private (media);
}

EvMedia *
ev_media_new_for_uri (EvPage      *page,
                      const gchar *uri)
{
        EvMedia *media;

        g_return_val_if_fail (EV_IS_PAGE (page), NULL);
        g_return_val_if_fail (uri != NULL, NULL);

        media = EV_MEDIA (g_object_new (EV_TYPE_MEDIA, NULL));
        media->priv->page = page->index;
        media->priv->uri = g_strdup (uri);

        return media;
}

const gchar *
ev_media_get_uri (EvMedia *media)
{
        g_return_val_if_fail (EV_IS_MEDIA (media), NULL);

        return media->priv->uri;
}

guint
ev_media_get_page_index (EvMedia *media)
{
        g_return_val_if_fail (EV_IS_MEDIA (media), 0);

        return media->priv->page;
}

gboolean
ev_media_get_show_controls (EvMedia *media)
{
        g_return_val_if_fail (EV_IS_MEDIA (media), FALSE);

        return media->priv->show_controls;
}

void
ev_media_set_show_controls (EvMedia *media,
                            gboolean show_controls)
{
        g_return_if_fail (EV_IS_MEDIA (media));

        media->priv->show_controls = show_controls;
}
