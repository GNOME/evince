#include <config.h>
#include "ev-page-cache.h"
#include "ev-document-thumbnails.h"
#include "ev-page.h"
#include <stdlib.h>
#include <string.h>

struct _EvPageCache
{
	GObject parent;

	EvDocument *document;

	gint current_page;

	gboolean dual_even_left;

	double* height_to_page;
	double* dual_height_to_page;

	int rotation;
};

struct _EvPageCacheClass
{
	GObjectClass parent_class;

	void (* page_changed) (EvPageCache *page_cache, gint page);
	void (* history_changed) (EvPageCache *page_cache, gint page);
};

enum
{
	PAGE_CHANGED,
	HISTORY_CHANGED,
	N_SIGNALS,
};

static guint signals[N_SIGNALS] = {0, };

static void ev_page_cache_init       (EvPageCache      *page_cache);
static void ev_page_cache_class_init (EvPageCacheClass *page_cache);
static void ev_page_cache_finalize   (GObject *object);

G_DEFINE_TYPE (EvPageCache, ev_page_cache, G_TYPE_OBJECT)

static void
ev_page_cache_init (EvPageCache *page_cache)
{
	page_cache->current_page = -1;
}

static void
ev_page_cache_class_init (EvPageCacheClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->finalize = ev_page_cache_finalize;

	signals [PAGE_CHANGED] =
		g_signal_new ("page-changed",
			      EV_TYPE_PAGE_CACHE,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EvPageCacheClass, page_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1,
			      G_TYPE_INT);

	signals [HISTORY_CHANGED] =
		g_signal_new ("history-changed",
			      EV_TYPE_PAGE_CACHE,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EvPageCacheClass, history_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1,
			      G_TYPE_INT);

}

static void
ev_page_cache_finalize (GObject *object)
{
	EvPageCache *page_cache = EV_PAGE_CACHE (object);

	page_cache->document = NULL;

	if (page_cache->height_to_page) {
		g_free (page_cache->height_to_page);
		page_cache->height_to_page = NULL;
	}

	if (page_cache->dual_height_to_page) {
		g_free (page_cache->dual_height_to_page);
		page_cache->dual_height_to_page = NULL;
	}

	G_OBJECT_CLASS (ev_page_cache_parent_class)->finalize (object);
}

static void
build_height_to_page (EvPageCache *page_cache)
{
	gboolean swap, uniform, dual_even_left;
	int i;
	double uniform_height, page_height, next_page_height;
	double saved_height;
	gdouble u_width, u_height;
	gint n_pages;

	swap = (page_cache->rotation == 90 ||
		page_cache->rotation == 270);

	uniform = ev_document_is_page_size_uniform (page_cache->document);
	n_pages = ev_document_get_n_pages (page_cache->document);
	dual_even_left = (n_pages > 2);

	g_free (page_cache->height_to_page);
	g_free (page_cache->dual_height_to_page);

	page_cache->height_to_page = g_new0 (double, n_pages + 1);
	page_cache->dual_height_to_page = g_new0 (double, n_pages + 2);

	if (uniform)
		ev_document_get_page_size (page_cache->document, 0, &u_width, &u_height);

	saved_height = 0;
	for (i = 0; i <= n_pages; i++) {
		if (uniform) {
			uniform_height = swap ? u_width : u_height;
			page_cache->height_to_page[i] = i * uniform_height;
		} else {
			if (i < n_pages) {
				gdouble w, h;

				ev_document_get_page_size (page_cache->document, i, &w, &h);
				page_height = swap ? w : h;
			} else {
				page_height = 0;
			}
			page_cache->height_to_page[i] = saved_height;
			saved_height += page_height;
		}
	}

	if (dual_even_left && !uniform) {
		gdouble w, h;

		ev_document_get_page_size (page_cache->document, 0, &w, &h);
		saved_height = swap ? w : h;
	} else {
		saved_height = 0;
	}

	for (i = dual_even_left; i < n_pages + 2; i += 2) {
    		if (uniform) {
			uniform_height = swap ? u_width : u_height;
			page_cache->dual_height_to_page[i] = ((i + dual_even_left) / 2) * uniform_height;
			if (i + 1 < n_pages + 2)
				page_cache->dual_height_to_page[i + 1] = ((i + dual_even_left) / 2) * uniform_height;
		} else {
			if (i + 1 < n_pages) {
				gdouble w, h;

				ev_document_get_page_size (page_cache->document, i + 1, &w, &h);
				next_page_height = swap ? w : h;
			} else {
				next_page_height = 0;
			}

			if (i < n_pages) {
				gdouble w, h;

				ev_document_get_page_size (page_cache->document, i, &w, &h);
				page_height = swap ? w : h;
			} else {
				page_height = 0;
			}

			if (i + 1 < n_pages + 2) {
				page_cache->dual_height_to_page[i] = saved_height;
				page_cache->dual_height_to_page[i + 1] = saved_height;
				saved_height += MAX(page_height, next_page_height);
			} else {
				page_cache->dual_height_to_page[i] = saved_height;
			}
		}
	}
}

static EvPageCache *
ev_page_cache_new (EvDocument *document)
{
	EvPageCache *page_cache;

	page_cache = (EvPageCache *) g_object_new (EV_TYPE_PAGE_CACHE, NULL);
	page_cache->document = document;

	build_height_to_page (page_cache);

	if (ev_document_get_n_pages (page_cache->document) > 0)
		ev_page_cache_set_current_page (page_cache, 0);

	return page_cache;
}

gint
ev_page_cache_get_current_page (EvPageCache *page_cache)
{
	g_return_val_if_fail (EV_IS_PAGE_CACHE (page_cache), 0);

	return page_cache->current_page;
}

void
ev_page_cache_set_current_page (EvPageCache *page_cache,
				int          page)
{
	g_return_if_fail (EV_IS_PAGE_CACHE (page_cache));

	if (page == page_cache->current_page)
		return;

	page_cache->current_page = page;
	g_signal_emit (page_cache, signals[PAGE_CHANGED], 0, page);
}

void
ev_page_cache_set_current_page_history (EvPageCache *page_cache,
					int          page)
{
	if (abs (page - page_cache->current_page) > 1)
		g_signal_emit (page_cache, signals [HISTORY_CHANGED], 0, page);
	
	ev_page_cache_set_current_page (page_cache, page);
}

gboolean
ev_page_cache_set_page_label (EvPageCache *page_cache,
			      const gchar *page_label)
{
	gint page;

	g_return_val_if_fail (EV_IS_PAGE_CACHE (page_cache), FALSE);

	if (ev_document_find_page_by_label (page_cache->document, page_label, &page)) {
		ev_page_cache_set_current_page (page_cache, page);
		return TRUE;
	}

	return FALSE;
}

void
ev_page_cache_get_size (EvPageCache  *page_cache,
			gint          page,
			gint          rotation,
			gfloat        scale,
			gint         *width,
			gint         *height)
{
	double w, h;

	g_return_if_fail (EV_IS_PAGE_CACHE (page_cache));

	ev_document_get_page_size (page_cache->document, page, &w, &h);

	w = w * scale + 0.5;
	h = h * scale + 0.5;

	if (rotation == 0 || rotation == 180) {
		if (width) *width = (int)w;
		if (height) *height = (int)h;
	} else {
		if (width) *width = (int)h;
		if (height) *height = (int)w;
	}
}

void
ev_page_cache_get_max_width (EvPageCache   *page_cache,
			     gint	    rotation,
			     gfloat         scale,
			     gint          *width)
{
	double w, h;

	g_return_if_fail (EV_IS_PAGE_CACHE (page_cache));

	if (!width)
		return;

	ev_document_get_max_page_size (page_cache->document, &w, &h);
	*width = (rotation == 0 || rotation == 180) ? w * scale : h * scale;
}

void
ev_page_cache_get_max_height (EvPageCache   *page_cache,
			      gint           rotation,
			      gfloat         scale,
			      gint          *height)
{
	double w, h;

	g_return_if_fail (EV_IS_PAGE_CACHE (page_cache));

	if (!height)
		return;

	ev_document_get_max_page_size (page_cache->document, &w, &h);
	*height = (rotation == 0 || rotation == 180) ? h * scale : w * scale;
}

void
ev_page_cache_get_height_to_page (EvPageCache   *page_cache,
				  gint           page,
				  gint           rotation,
				  gfloat         scale,
				  gint          *height,
				  gint 	        *dual_height)
{
	g_return_if_fail (EV_IS_PAGE_CACHE (page_cache));
	g_return_if_fail (page >= 0);

	if (page_cache->rotation != rotation) {
		page_cache->rotation = rotation;
		build_height_to_page (page_cache);
	}

	if (height)
		*height = page_cache->height_to_page[page] * scale;

	if (dual_height)
		*dual_height = page_cache->dual_height_to_page[page] * scale;
}

gboolean
ev_page_cache_get_dual_even_left (EvPageCache *page_cache)
{
	g_return_val_if_fail (EV_IS_PAGE_CACHE (page_cache), 0);

	return (ev_document_get_n_pages (page_cache->document) > 2);
}

#define PAGE_CACHE_STRING "ev-page-cache"

EvPageCache *
ev_page_cache_get (EvDocument *document)
{
	EvPageCache *page_cache;

	g_return_val_if_fail (EV_IS_DOCUMENT (document), NULL);

	page_cache = g_object_get_data (G_OBJECT (document), PAGE_CACHE_STRING);
	if (page_cache == NULL) {
		page_cache = ev_page_cache_new (document);
		g_object_set_data_full (G_OBJECT (document), PAGE_CACHE_STRING, page_cache, g_object_unref);
	}

	return page_cache;
}
