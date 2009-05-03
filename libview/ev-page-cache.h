/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2005 Red Hat, Inc
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#if !defined (__EV_EVINCE_VIEW_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-view.h> can be included directly."
#endif

#ifndef __EV_PAGE_CACHE_H__
#define __EV_PAGE_CACHE_H__

#include <gtk/gtk.h>

#include <evince-document.h>

G_BEGIN_DECLS
#define EV_TYPE_PAGE_CACHE            (ev_page_cache_get_type ())
#define EV_PAGE_CACHE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_PAGE_CACHE, EvPageCache))
#define EV_IS_PAGE_CACHE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_PAGE_CACHE))

GType          ev_page_cache_get_type            (void) G_GNUC_CONST;

/* Used by ev-document.c only */
EvPageCache   *ev_page_cache_new                 (EvDocument    *document);
gint           ev_page_cache_get_n_pages         (EvPageCache   *page_cache);
const char    *ev_page_cache_get_title           (EvPageCache   *page_cache);
void           ev_page_cache_get_size            (EvPageCache   *page_cache,
						  gint           page,
						  gint           rotation,
						  gfloat         scale,
						  gint          *width,
						  gint          *height);
void           ev_page_cache_get_max_width       (EvPageCache   *page_cache,
						  gint           rotation,
						  gfloat         scale,
						  gint          *width);
void           ev_page_cache_get_max_height      (EvPageCache   *page_cache,
						  gint           rotation,
						  gfloat         scale,
						  gint          *height);
void           ev_page_cache_get_height_to_page  (EvPageCache   *page_cache,
						  gint           page,
						  gint           rotation,
						  gfloat         scale,
						  gint          *height,
						  gint	        *dual_height);
void           ev_page_cache_get_thumbnail_size  (EvPageCache   *page_cache,
						  gint           page,
						  gint           rotation,
						  gint          *width,
						  gint          *height);
gint           ev_page_cache_get_max_label_chars (EvPageCache   *page_cache);
char          *ev_page_cache_get_page_label      (EvPageCache   *page_cache,
						  gint           page);
gboolean       ev_page_cache_has_nonnumeric_page_labels (EvPageCache *page_cache);
const EvDocumentInfo *ev_page_cache_get_info            (EvPageCache *page_cache);

gboolean       ev_page_cache_get_dual_even_left  (EvPageCache *page_cache);

/* Navigation */
gint           ev_page_cache_get_current_page    (EvPageCache *page_cache);
void           ev_page_cache_set_current_page    (EvPageCache *page_cache,
						  int          page);
void           ev_page_cache_set_current_page_history  (EvPageCache *page_cache,
							int          page);
gboolean       ev_page_cache_set_page_label      (EvPageCache *page_cache,
						  const char  *page_label);

EvPageCache   *ev_page_cache_get		 (EvDocument *document);

gboolean       ev_page_cache_check_dimensions	 (EvPageCache *page_cache);

G_END_DECLS

#endif /* __EV_PAGE_CACHE_H__ */
