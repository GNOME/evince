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

#ifndef __EV_PAGE_CACHE_H__
#define __EV_PAGE_CACHE_H__

#include <gtk/gtkwidget.h>

#include "ev-document.h"

G_BEGIN_DECLS

#define EV_TYPE_PAGE_CACHE            (ev_page_cache_get_type ())
#define EV_PAGE_CACHE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_PAGE_CACHE, EvPageCache))
#define EV_IS_PAGE_CACHE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_PAGE_CACHE))

typedef struct _EvPageCache       EvPageCache;
typedef struct _EvPageCacheClass  EvPageCacheClass;

GType          ev_page_cache_get_type        (void) G_GNUC_CONST;
EvPageCache   *ev_page_cache_new             (void);
void           ev_page_cache_set_document    (EvPageCache *page_cache,
					      EvDocument  *document);
gint           ev_page_cache_get_n_pages     (EvPageCache *page_cache);
void           ev_page_cache_get_size        (EvPageCache *page_cache,
					      gint         page,
					      gint        *width,
					      gint        *height);


G_END_DECLS

#endif /* __EV_PAGE_CACHE_H__ */
