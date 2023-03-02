/* ev-bookmarks.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2010 Carlos Garcia Campos  <carlosgc@gnome.org>
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

#include "config.h"

#include <string.h>

#include "ev-bookmarks.h"

enum {
        PROP_0,
        PROP_METADATA
};

enum {
        CHANGED,
        N_SIGNALS
};

struct _EvBookmarks {
        GObject base;

        EvMetadata *metadata;
        GList *items;
};

struct _EvBookmarksClass {
        GObjectClass base_class;

        void (*changed) (EvBookmarks *bookmarks);
};

G_DEFINE_TYPE (EvBookmarks, ev_bookmarks, G_TYPE_OBJECT)

static guint signals[N_SIGNALS];

static gint
ev_bookmark_compare (EvBookmark *a,
                     EvBookmark *b)
{
        if (a->page < b->page)
                return -1;
        if (a->page > b->page)
                return 1;
        return 0;
}

static void
ev_bookmark_free (EvBookmark *bm)
{
        if (G_UNLIKELY(!bm))
                return;

        g_free (bm->title);
        g_slice_free (EvBookmark, bm);
}

static void
ev_bookmarks_finalize (GObject *object)
{
        EvBookmarks *bookmarks = EV_BOOKMARKS (object);

        if (bookmarks->items) {
                g_list_free_full (bookmarks->items, (GDestroyNotify)ev_bookmark_free);
                bookmarks->items = NULL;
        }

	g_clear_object (&bookmarks->metadata);

        G_OBJECT_CLASS (ev_bookmarks_parent_class)->finalize (object);
}

static void
ev_bookmarks_init (EvBookmarks *bookmarks)
{
}

static void
ev_bookmarks_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
        EvBookmarks *bookmarks = EV_BOOKMARKS (object);

        switch (prop_id) {
        case PROP_METADATA:
                bookmarks->metadata = (EvMetadata *)g_value_dup_object (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
ev_bookmarks_constructed (GObject *object)
{
        EvBookmarks *bookmarks = EV_BOOKMARKS (object);
        gchar       *bm_list_str;
        GVariant    *bm_list;
        GVariantIter iter;
        GVariant    *child;
        GError      *error = NULL;

        if (!ev_metadata_get_string (bookmarks->metadata, "bookmarks", &bm_list_str))
                return;

        if (!bm_list_str || bm_list_str[0] == '\0')
                return;

        bm_list = g_variant_parse ((const GVariantType *)"a(us)",
                                   bm_list_str, NULL, NULL,
                                   &error);
        if (!bm_list) {
                g_warning ("Error getting bookmarks: %s\n", error->message);
                g_error_free (error);

                return;
        }

        g_variant_iter_init (&iter, bm_list);
        while ((child = g_variant_iter_next_value (&iter))) {
                EvBookmark *bm = g_slice_new (EvBookmark);

                g_variant_get (child, "(us)", &bm->page, &bm->title);
                if (bm->title && strlen (bm->title) > 0)
                        bookmarks->items = g_list_prepend (bookmarks->items, bm);
                g_variant_unref (child);
        }
        g_variant_unref (bm_list);

        bookmarks->items = g_list_reverse (bookmarks->items);
}

static void
ev_bookmarks_class_init (EvBookmarksClass *klass)
{
        GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

        gobject_class->set_property = ev_bookmarks_set_property;
        gobject_class->finalize = ev_bookmarks_finalize;
        gobject_class->constructed = ev_bookmarks_constructed;

        g_object_class_install_property (gobject_class,
                                         PROP_METADATA,
                                         g_param_spec_object ("metadata",
                                                              "Metadata",
                                                              "The document metadata",
                                                              EV_TYPE_METADATA,
                                                              G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                                                              G_PARAM_STATIC_STRINGS));
        /* Signals */
        signals[CHANGED] =
                g_signal_new ("changed",
                              EV_TYPE_BOOKMARKS,
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (EvBookmarksClass, changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
}

EvBookmarks *
ev_bookmarks_new (EvMetadata *metadata)
{
        g_return_val_if_fail (EV_IS_METADATA (metadata), NULL);

        return EV_BOOKMARKS (g_object_new (EV_TYPE_BOOKMARKS,
                                           "metadata", metadata, NULL));
}

static void
ev_bookmarks_save (EvBookmarks *bookmarks)
{
        GList          *l;
        GVariantBuilder builder;
        GVariant       *bm_list;
        gchar          *bm_list_str;

        if (!bookmarks->items) {
                ev_metadata_set_string (bookmarks->metadata, "bookmarks", "");
                return;
        }

        g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
        for (l = bookmarks->items; l; l = g_list_next (l)) {
                EvBookmark *bm = (EvBookmark *)l->data;

                g_variant_builder_add (&builder, "(u&s)", bm->page, bm->title);
        }
        bm_list = g_variant_builder_end (&builder);

        bm_list_str = g_variant_print (bm_list, FALSE);
        g_variant_unref (bm_list);
        ev_metadata_set_string (bookmarks->metadata, "bookmarks", bm_list_str);
        g_free (bm_list_str);
}

GList *
ev_bookmarks_get_bookmarks (EvBookmarks *bookmarks)
{
        g_return_val_if_fail (EV_IS_BOOKMARKS (bookmarks), NULL);

        return g_list_copy (bookmarks->items);
}

gboolean
ev_bookmarks_has_bookmarks (EvBookmarks *bookmarks)
{
        g_return_val_if_fail (EV_IS_BOOKMARKS (bookmarks), FALSE);

        return bookmarks->items != NULL;
}

void
ev_bookmarks_add (EvBookmarks *bookmarks,
                  EvBookmark  *bookmark)
{
        EvBookmark *bm;

        g_return_if_fail (EV_IS_BOOKMARKS (bookmarks));
        g_return_if_fail (bookmark->title != NULL);

        if (g_list_find_custom (bookmarks->items, bookmark, (GCompareFunc)ev_bookmark_compare))
                return;

        bm = g_slice_new (EvBookmark);
        *bm = *bookmark;
        bookmarks->items = g_list_append (bookmarks->items, bm);
        g_signal_emit (bookmarks, signals[CHANGED], 0);
        ev_bookmarks_save (bookmarks);
}

void
ev_bookmarks_delete (EvBookmarks *bookmarks,
                     EvBookmark  *bookmark)
{
        GList *bm_link;

        g_return_if_fail (EV_IS_BOOKMARKS (bookmarks));

        bm_link = g_list_find_custom (bookmarks->items, bookmark, (GCompareFunc)ev_bookmark_compare);
        if (!bm_link)
                return;

        bookmarks->items = g_list_delete_link (bookmarks->items, bm_link);
        g_signal_emit (bookmarks, signals[CHANGED], 0);
        ev_bookmarks_save (bookmarks);
}

void
ev_bookmarks_update (EvBookmarks *bookmarks,
                     EvBookmark  *bookmark)
{
        GList      *bm_link;
        EvBookmark *bm;

        g_return_if_fail (EV_IS_BOOKMARKS (bookmarks));
        g_return_if_fail (bookmark->title != NULL);

        bm_link = g_list_find_custom (bookmarks->items, bookmark, (GCompareFunc)ev_bookmark_compare);
        if (!bm_link)
                return;

        bm = (EvBookmark *)bm_link->data;

        if (strcmp (bookmark->title, bm->title) == 0)
                return;

        g_free (bm->title);
        *bm = *bookmark;
        g_signal_emit (bookmarks, signals[CHANGED], 0);
        ev_bookmarks_save (bookmarks);
}
