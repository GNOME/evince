#include <config.h>
#include "ev-page-cache.h"
#include "ev-document-thumbnails.h"
#include "ev-page.h"
#include <stdlib.h>
#include <string.h>

#define THUMBNAIL_WIDTH 100

typedef struct _EvPageCacheInfo
{
	double width;
	double height;
} EvPageCacheInfo;

typedef struct _EvPageThumbsInfo
{
	gint width;
	gint height;
} EvPageThumbsInfo;

struct _EvPageCache
{
	GObject parent;

	gint current_page;
	int n_pages;
	char *title;
	char **page_labels;
	
	gint max_label_chars;
	gboolean has_labels;
	gboolean uniform;
	gboolean dual_even_left;
	
	double uniform_width;
	double uniform_height;

	double  max_width;
	double  max_height;

	double* height_to_page;
	double* dual_height_to_page;

	int rotation;

	EvPageCacheInfo *size_cache;
	EvDocumentInfo *page_info;

	/* Thumbnail dimensions */
	gboolean thumbs_uniform;
	gint thumbs_uniform_width;
	gint thumbs_uniform_height;
	gint thumbs_max_width;
	gint thumbs_max_height;
	EvPageThumbsInfo *thumbs_size_cache;
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
	page_cache->max_label_chars = 0;
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
	EvPageCache *page_cache;

	page_cache = EV_PAGE_CACHE (object);

	if (page_cache->title) {
		g_free (page_cache->title);
		page_cache->title = NULL;
	}

	if (page_cache->size_cache) {
		g_free (page_cache->size_cache);
		page_cache->size_cache = NULL;
	}

	if (page_cache->thumbs_size_cache) {
		g_free (page_cache->thumbs_size_cache);
		page_cache->thumbs_size_cache = NULL;
	}

	if (page_cache->height_to_page) {
		g_free (page_cache->height_to_page);
		page_cache->height_to_page = NULL;
	}

	if (page_cache->dual_height_to_page) {
		g_free (page_cache->dual_height_to_page);
		page_cache->dual_height_to_page = NULL;
	}

	if (page_cache->page_labels) {
		gint i;

		for (i = 0; i < page_cache->n_pages; i++) {
			if (page_cache->page_labels[i])
				g_free (page_cache->page_labels[i]);
		}
		g_free (page_cache->page_labels);
		page_cache->page_labels = NULL;
	}

	if (page_cache->page_info) {
		ev_document_info_free (page_cache->page_info);
		page_cache->page_info = NULL;
	}

	G_OBJECT_CLASS (ev_page_cache_parent_class)->finalize (object);
}

static void
build_height_to_page (EvPageCache *page_cache)
{
	gboolean swap;
	int i;
	double uniform_height, page_height, next_page_height;
	double saved_height;

	swap = (page_cache->rotation == 90 ||
		page_cache->rotation == 270);

	g_free (page_cache->height_to_page);
	g_free (page_cache->dual_height_to_page);

	page_cache->height_to_page = g_new0(double, page_cache->n_pages + 1);
	page_cache->dual_height_to_page = g_new0(double, page_cache->n_pages + 2);
	
	saved_height = 0;
	for (i = 0; i <= page_cache->n_pages; i++) {
		if (page_cache->uniform) {
			if (!swap) {
				uniform_height = page_cache->uniform_height;
			} else {
				uniform_height = page_cache->uniform_width;
			}
			page_cache->height_to_page [i] = i * uniform_height;
		} else {
			if (i < page_cache->n_pages) {
				if (!swap) {
					page_height = page_cache->size_cache [i].height;
				} else {
					page_height = page_cache->size_cache [i].width;
				}
			} else {
				page_height = 0;
			}
			page_cache->height_to_page [i] = saved_height;
			saved_height += page_height;
		}
	}

	if (page_cache->dual_even_left && !page_cache->uniform) {
		if (!swap) {
			saved_height = page_cache->size_cache [0].height;
		} else {
			saved_height = page_cache->size_cache [0].width;
		}
	} else {
		saved_height = 0;
	}
	for (i = page_cache->dual_even_left; i < page_cache->n_pages + 2; i += 2) {
    		if (page_cache->uniform) {
			if (!swap) {
				uniform_height = page_cache->uniform_height;
			} else {
				uniform_height = page_cache->uniform_width;
			}
			page_cache->dual_height_to_page [i] = ((i + page_cache->dual_even_left) / 2) * uniform_height;
			if (i + 1 < page_cache->n_pages + 2)
				page_cache->dual_height_to_page [i + 1] = ((i + page_cache->dual_even_left) / 2) * uniform_height;
		} else {
			if (i + 1 < page_cache->n_pages) {
				if (!swap) {
					next_page_height = page_cache->size_cache [i + 1].height;
				} else {
					next_page_height = page_cache->size_cache [i + 1].width;
				}
			} else {
				next_page_height = 0;
			}
			if (i < page_cache->n_pages) {
				if (!swap) {
					page_height = page_cache->size_cache [i].height;
				} else {
					page_height = page_cache->size_cache [i].width;
				}
			} else {
				page_height = 0;
			}
			if (i + 1 < page_cache->n_pages + 2) {
				page_cache->dual_height_to_page [i] = saved_height;
				page_cache->dual_height_to_page [i + 1] = saved_height;
				saved_height += MAX(page_height, next_page_height);
			} else {
				page_cache->dual_height_to_page [i] = saved_height;
			}
		}
	}
}

EvPageCache *
ev_page_cache_new (EvDocument *document)
{
	EvPageCache *page_cache;
	EvPageCacheInfo *info;
	EvPageThumbsInfo *thumb_info;
	EvRenderContext *rc = NULL;
	gboolean has_thumbs;
	gint i;

	page_cache = (EvPageCache *) g_object_new (EV_TYPE_PAGE_CACHE, NULL);

	ev_document_doc_mutex_lock ();

	/* We read page information out of the document */

	/* Assume all pages are the same size until proven otherwise */
	page_cache->uniform = TRUE;
	page_cache->has_labels = FALSE;
	page_cache->n_pages = ev_document_get_n_pages (document);
	page_cache->dual_even_left = (page_cache->n_pages > 2);
	page_cache->page_labels = g_new0 (char *, page_cache->n_pages);
	page_cache->max_width = 0;
	page_cache->max_height = 0;
	page_cache->page_info = ev_document_get_info (document);
	page_cache->thumbs_uniform = TRUE;

	if (page_cache->page_info->fields_mask & EV_DOCUMENT_INFO_TITLE) {
		page_cache->title = g_strdup (page_cache->page_info->title);
	} else {
		page_cache->title = NULL;
	}

	has_thumbs = EV_IS_DOCUMENT_THUMBNAILS (document);
	
	for (i = 0; i < page_cache->n_pages; i++) {
		EvPage *page;
		double  page_width = 0;
		double  page_height = 0;
		gint    thumb_width = 0;
		gint    thumb_height = 0;

		page = ev_document_get_page (document, i);
		
		ev_document_get_page_size (document, page, &page_width, &page_height);

	    	page_cache->page_labels[i] = ev_document_get_page_label (document, page);
		
		if (page_cache->page_labels[i] != NULL) {
		
			page_cache->max_label_chars = MAX (page_cache->max_label_chars, 
							   g_utf8_strlen (page_cache->page_labels[i], 256));
			if (!page_cache->has_labels) {
				gchar *expected_label;
			
				expected_label = g_strdup_printf ("%d", i + 1);
				if (strcmp (expected_label, page_cache->page_labels[i]))  
					page_cache->has_labels = TRUE;
				g_free (expected_label);
			}
		}

		if (page_width > page_cache->max_width) {
			page_cache->max_width = page_width;
		}

		if (page_height > page_cache->max_height) {
			page_cache->max_height = page_height;
		}
			
		if (i == 0) {
			page_cache->uniform_width = page_width;
			page_cache->uniform_height = page_height;
		} else if (page_cache->uniform &&
			   (page_cache->uniform_width != page_width ||
			    page_cache->uniform_height != page_height)) {
			/* It's a different page size.  Backfill the array. */
			int j;

			page_cache->size_cache = g_new0 (EvPageCacheInfo, page_cache->n_pages);

			for (j = 0; j < i; j++) {
				info = &(page_cache->size_cache [j]);
				info->width = page_cache->uniform_width;
				info->height = page_cache->uniform_height;
			}
			page_cache->uniform = FALSE;

		}

		if (! page_cache->uniform) {
			info = &(page_cache->size_cache [i]);

			info->width = page_width;
			info->height = page_height;
		}

		if (!has_thumbs) {
			g_object_unref (page);
			continue;
		}

		if (!rc) {
			rc = ev_render_context_new (page, 0, (gdouble)THUMBNAIL_WIDTH / page_width);
		} else {
			ev_render_context_set_page (rc, page);
			ev_render_context_set_scale (rc, (gdouble)THUMBNAIL_WIDTH / page_width);
		}

		ev_document_thumbnails_get_dimensions (EV_DOCUMENT_THUMBNAILS (document),
						       rc, &thumb_width, &thumb_height);
		
		if (thumb_width > page_cache->thumbs_max_width) {
			page_cache->thumbs_max_width = thumb_width;
		}

		if (thumb_height > page_cache->thumbs_max_height) {
			page_cache->thumbs_max_height = thumb_height;
		}
			
		if (i == 0) {
			page_cache->thumbs_uniform_width = thumb_width;
			page_cache->thumbs_uniform_height = thumb_height;
		} else if (page_cache->thumbs_uniform &&
			   (page_cache->thumbs_uniform_width != thumb_width ||
			    page_cache->thumbs_uniform_height != thumb_height)) {
			/* It's a different thumbnail size.  Backfill the array. */
			int j;

			page_cache->thumbs_size_cache = g_new0 (EvPageThumbsInfo, page_cache->n_pages);

			for (j = 0; j < i; j++) {
				thumb_info = &(page_cache->thumbs_size_cache [j]);
				thumb_info->width = page_cache->thumbs_uniform_width;
				thumb_info->height = page_cache->thumbs_uniform_height;
			}
			page_cache->thumbs_uniform = FALSE;
		}

		if (! page_cache->thumbs_uniform) {
			thumb_info = &(page_cache->thumbs_size_cache [i]);

			thumb_info->width = thumb_width;
			thumb_info->height = thumb_height;
		}

		g_object_unref (page);
	}

	if (rc) {
		g_object_unref (rc);
	}

	build_height_to_page (page_cache);

	/* make some sanity check assertions */
	if (! page_cache->uniform)
		g_assert (page_cache->size_cache != NULL);

	ev_document_doc_mutex_unlock ();

	if (page_cache->n_pages > 0)
		ev_page_cache_set_current_page (page_cache, 0);

	return page_cache;
}

gboolean
ev_page_cache_check_dimensions (EvPageCache *page_cache)
{
	gint document_width, document_height;

	if (page_cache->uniform && page_cache->n_pages > 0)
		if (page_cache->uniform_width <= 0 || page_cache->uniform_height <= 0)
			return TRUE;

	ev_page_cache_get_max_width (page_cache,
    		    	    	     0, 1.0,
				     &document_width);
	ev_page_cache_get_max_height (page_cache,
				      0, 1.0,
				      &document_height);

	if (document_width <= 0 || document_height <= 0)
		return TRUE;

	return FALSE;
}

gint
ev_page_cache_get_n_pages (EvPageCache *page_cache)
{
	g_return_val_if_fail (EV_IS_PAGE_CACHE (page_cache), 0);

	return page_cache->n_pages;
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
	g_return_if_fail (page >= 0 || page < page_cache->n_pages);

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
			      const char  *page_label)
{
	gint i, page;
	long value;
	char *endptr = NULL;
	
	g_return_val_if_fail (EV_IS_PAGE_CACHE (page_cache), FALSE);
	g_return_val_if_fail (page_label != NULL, FALSE);

	/* First, look for a literal label match */
	for (i = 0; i < page_cache->n_pages; i ++) {
		if (page_cache->page_labels[i] != NULL &&
		    ! strcmp (page_label, page_cache->page_labels[i])) {
			ev_page_cache_set_current_page (page_cache, i);
			return TRUE;
		}
	}

	/* Second, look for a match with case insensitively */
	for (i = 0; i < page_cache->n_pages; i++) {
		if (page_cache->page_labels[i] != NULL &&
		    ! strcasecmp (page_label, page_cache->page_labels[i])) {
			ev_page_cache_set_current_page (page_cache, i);
			return TRUE;
		}
	}

	/* Next, parse the label, and see if the number fits */
	value = strtol (page_label, &endptr, 10);
	if (endptr[0] == '\0') {
		/* Page number is an integer */
		page = MIN (G_MAXINT, value);

		/* convert from a page label to a page offset */
		page --;
		if (page >= 0 &&
		    page < page_cache->n_pages) {
			ev_page_cache_set_current_page (page_cache, page);
			return TRUE;
		}
	}

	return FALSE;
}

const char *
ev_page_cache_get_title (EvPageCache *page_cache)
{
	g_return_val_if_fail (EV_IS_PAGE_CACHE (page_cache), NULL);

	return page_cache->title;
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
	g_return_if_fail (page >= 0 && page < page_cache->n_pages);

	if (page_cache->uniform) {
		w = page_cache->uniform_width;
		h = page_cache->uniform_height;
	} else {
		EvPageCacheInfo *info;

		info = &(page_cache->size_cache [page]);
		
		w = info->width;
		h = info->height;
	}

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
	g_return_if_fail (EV_IS_PAGE_CACHE (page_cache));

	if (width) {
		if (rotation == 0 || rotation == 180) {
			*width = page_cache->max_width * scale;
		} else {
			*width = page_cache->max_height * scale;
		}
	}
}

void
ev_page_cache_get_max_height (EvPageCache   *page_cache,
			      gint           rotation,
			      gfloat         scale,
			      gint          *height)
{
	g_return_if_fail (EV_IS_PAGE_CACHE (page_cache));

	if (height) {
		if (rotation == 0 || rotation == 180) {
			*height = page_cache->max_height * scale;
		} else {
			*height = page_cache->max_width * scale;
		}
	}
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
	g_return_if_fail (!height || page <= page_cache->n_pages);
	g_return_if_fail (!dual_height || page <= page_cache->n_pages + 1);

	if (page_cache->rotation != rotation) {
		page_cache->rotation = rotation;
		build_height_to_page (page_cache);
	}
	
	if (height)
		*height = page_cache->height_to_page [page] * scale;

	if (dual_height)
		*dual_height = page_cache->dual_height_to_page [page] * scale;
}

void
ev_page_cache_get_thumbnail_size (EvPageCache  *page_cache,
				  gint          page,
				  gint          rotation,
				  gint         *width,
				  gint         *height)
{
	gint w, h;

	g_return_if_fail (EV_IS_PAGE_CACHE (page_cache));
	g_return_if_fail (page >= 0 && page < page_cache->n_pages);

	if (page_cache->thumbs_uniform) {
		w = page_cache->thumbs_uniform_width;
		h = page_cache->thumbs_uniform_height;
	} else {
		EvPageThumbsInfo *info;

		info = &(page_cache->thumbs_size_cache [page]);
		
		w = info->width;
		h = info->height;
	}

	if (rotation == 0 || rotation == 180) {
		if (width) *width = w;
		if (height) *height = h;
	} else {
		if (width) *width = h;
		if (height) *height = w;
	}
}

gint
ev_page_cache_get_max_label_chars (EvPageCache *page_cache)
{
	g_return_val_if_fail (EV_IS_PAGE_CACHE (page_cache), 0);
	
	return page_cache->max_label_chars;
}

gboolean
ev_page_cache_get_dual_even_left (EvPageCache *page_cache)
{
	g_return_val_if_fail (EV_IS_PAGE_CACHE (page_cache), 0);
	
	return page_cache->dual_even_left;
}

gchar *
ev_page_cache_get_page_label (EvPageCache *page_cache,
			      gint         page)
{
	g_return_val_if_fail (EV_IS_PAGE_CACHE (page_cache), NULL);
	g_return_val_if_fail (page >= 0 && page < page_cache->n_pages, NULL);

	if (page_cache->page_labels[page] == NULL)
		return g_strdup_printf ("%d", page + 1);

	return g_strdup (page_cache->page_labels[page]);
}

gboolean
ev_page_cache_has_nonnumeric_page_labels (EvPageCache *page_cache)
{
	g_return_val_if_fail (EV_IS_PAGE_CACHE (page_cache), FALSE);
	return page_cache->has_labels;
}

const EvDocumentInfo *
ev_page_cache_get_info (EvPageCache *page_cache)
{
	g_return_val_if_fail (EV_IS_PAGE_CACHE (page_cache), NULL);

	return page_cache->page_info;
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
