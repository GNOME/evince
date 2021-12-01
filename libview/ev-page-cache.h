/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2009 Carlos Garcia Campos
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

#if !defined (EVINCE_COMPILATION)
#error "This is a private header."
#endif

#include <glib-object.h>
#include <gdk/gdk.h>
#include <evince-document.h>
#include <evince-view.h>

G_BEGIN_DECLS

#define EV_TYPE_PAGE_CACHE            (ev_page_cache_get_type ())
#define EV_PAGE_CACHE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_PAGE_CACHE, EvPageCache))
#define EV_IS_PAGE_CACHE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_PAGE_CACHE))
#define EV_PAGE_CACHE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_PAGE_CACHE, EvPageCacheClass))
#define EV_IS_PAGE_CACHE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_PAGE_CACHE))
#define EV_PAGE_CACHE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EV_TYPE_PAGE_CACHE, EvPageCacheClass))

typedef struct _EvPageCache        EvPageCache;
typedef struct _EvPageCacheClass   EvPageCacheClass;

GType              ev_page_cache_get_type               (void) G_GNUC_CONST;
EvPageCache       *ev_page_cache_new                    (EvDocument        *document);

void               ev_page_cache_set_page_range         (EvPageCache       *cache,
							 gint               start,
							 gint               end);
EvJobPageDataFlags ev_page_cache_get_flags              (EvPageCache       *cache);
void               ev_page_cache_set_flags              (EvPageCache       *cache,
							 EvJobPageDataFlags flags);
void               ev_page_cache_mark_dirty             (EvPageCache       *cache,
							 gint               page,
                                                         EvJobPageDataFlags flags);
EvMappingList     *ev_page_cache_get_link_mapping       (EvPageCache       *cache,
							 gint               page);
EvMappingList     *ev_page_cache_get_image_mapping      (EvPageCache       *cache,
							 gint               page);
EvMappingList     *ev_page_cache_get_form_field_mapping (EvPageCache       *cache,
							 gint               page);
EvMappingList     *ev_page_cache_get_annot_mapping      (EvPageCache       *cache,
							 gint               page);
EvMappingList     *ev_page_cache_get_media_mapping      (EvPageCache       *cache,
                                                         gint               page);
cairo_region_t    *ev_page_cache_get_text_mapping       (EvPageCache       *cache,
							 gint               page);
const gchar       *ev_page_cache_get_text               (EvPageCache       *cache,
							 gint               page);
gboolean           ev_page_cache_get_text_layout        (EvPageCache       *cache,
							 gint               page,
							 EvRectangle      **areas,
							 guint             *n_areas);
PangoAttrList     *ev_page_cache_get_text_attrs         (EvPageCache       *cache,
                                                         gint               page);
gboolean           ev_page_cache_get_text_log_attrs     (EvPageCache       *cache,
                                                         gint               page,
                                                         PangoLogAttr     **log_attrs,
                                                         gulong            *n_attrs);
void               ev_page_cache_ensure_page            (EvPageCache       *cache,
                                                         gint               page);
gboolean           ev_page_cache_is_page_cached         (EvPageCache       *cache,
                                                         gint               page);
G_END_DECLS
