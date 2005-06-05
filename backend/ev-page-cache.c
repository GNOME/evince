#include "ev-page-cache.h"
#include "ev-job-queue.h"
#include <stdlib.h>
#include <string.h>

typedef struct _EvPageCacheInfo
{
	double width;
	double height;
}
EvPageCacheInfo;


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
	
	double uniform_width;
	double uniform_height;

	double  max_width;
	double  max_height;
	double* height_to_page;
	double* dual_height_to_page;

	EvPageCacheInfo *size_cache;
	EvDocumentInfo *page_info;
};

struct _EvPageCacheClass
{
	GObjectClass parent_class;

	void (* page_changed) (EvPageCache *page_cache, gint page);
};

enum
{
	PAGE_CHANGED,
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

}

static void
ev_page_cache_finalize (GObject *object)
{
	EvPageCache *page_cache;

	page_cache = EV_PAGE_CACHE (object);

	g_free (page_cache->title);
	g_free (page_cache->size_cache);
	g_free (page_cache->height_to_page);
	g_free (page_cache->dual_height_to_page);

	ev_document_info_free (page_cache->page_info);
}

EvPageCache *
_ev_page_cache_new (EvDocument *document)
{
	EvDocumentInfo *doc_info;
	EvPageCache *page_cache;
	EvPageCacheInfo *info;
	gint i;
	double saved_height;

	page_cache = (EvPageCache *) g_object_new (EV_TYPE_PAGE_CACHE, NULL);

	ev_document_doc_mutex_lock ();

	/* We read page information out of the document */

	/* Assume all pages are the same size until proven otherwise */
	page_cache->uniform = TRUE;
	page_cache->has_labels = FALSE;
	page_cache->n_pages = ev_document_get_n_pages (document);
	page_cache->page_labels = g_new0 (char *, page_cache->n_pages);
	page_cache->max_width = 0;
	page_cache->max_height = 0;

	doc_info = ev_document_get_info (document);
	if (doc_info->fields_mask & EV_DOCUMENT_INFO_TITLE) {
		page_cache->title = g_strdup (doc_info->title);
	} else {
		page_cache->title = NULL;
	}
	g_free (doc_info);

	for (i = 0; i < page_cache->n_pages; i++) {
		double page_width = 0;
		double page_height = 0;
		
		ev_document_get_page_size (document, i, &page_width, &page_height);

	    	page_cache->page_labels[i] = ev_document_get_page_label (document, i);
		
		if (page_cache->page_labels[i] != NULL) {
		
			page_cache->max_label_chars = MAX(page_cache->max_label_chars, 
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
	}

	page_cache->height_to_page = g_new0(double, page_cache->n_pages);
	page_cache->dual_height_to_page = g_new0(double, page_cache->n_pages / 2 + 1);
	
	saved_height = 0;
	for (i = 0; i < page_cache->n_pages; i++) {

		if (page_cache->uniform) {
			page_cache->height_to_page [i] = (i + 1) * page_cache->uniform_height;
		} else {
			page_cache->height_to_page [i] = saved_height + page_cache->size_cache [i].height;
			saved_height = page_cache->height_to_page [i];
		}
	}
	
	saved_height = 0;
	for (i = 0; i < page_cache->n_pages; i += 2) {

    		if (page_cache->uniform) {
			page_cache->dual_height_to_page [i / 2] = (i / 2 + 1) * page_cache->uniform_height;
		} else {
			if (i == page_cache->n_pages - 1) {
				page_cache->dual_height_to_page [i / 2] =
					saved_height + page_cache->size_cache [i].height;
			}
			else {
				page_cache->dual_height_to_page [i / 2] = saved_height +
				       MAX(page_cache->size_cache [i].height,			    	    
				    	   page_cache->size_cache [i + 1].height);
				saved_height = page_cache->dual_height_to_page [i / 2];
			}
		}
	}

	page_cache->page_info = ev_document_get_info (document);

	/* make some sanity check assertions */
	if (! page_cache->uniform)
		g_assert (page_cache->size_cache != NULL);
	if (page_cache->uniform)
		g_assert (page_cache->uniform_width > 0 && page_cache->uniform_height > 0);

	ev_document_doc_mutex_unlock ();

	if (page_cache->n_pages > 0)
		ev_page_cache_set_current_page (page_cache, 0);

	return page_cache;
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

	/* Next, parse the label, and see if the number fits */
	value = strtol (page_label, &endptr, 10);
	if (endptr[0] == '\0') {
		/* Page number is an integer */
		page = MIN (G_MAXINT, value);

		/* convert from a page label to a page offset */
		page --;
		if (page >= 0 &&
		    page < page_cache->n_pages &&
		    page_cache->page_labels[page] == NULL) {
			ev_page_cache_set_current_page (page_cache, page);
			return TRUE;
		}
	}

	return FALSE;
}

void
ev_page_cache_set_link (EvPageCache *page_cache,
			EvLink      *link)
{
	g_return_if_fail (EV_IS_PAGE_CACHE (page_cache));
	g_return_if_fail (EV_IS_LINK (link));

	ev_page_cache_set_current_page (page_cache, ev_link_get_page (link));
}

char *
ev_page_cache_get_title (EvPageCache *page_cache)
{
	g_return_val_if_fail (EV_IS_PAGE_CACHE (page_cache), NULL);

	return page_cache->title;
}

void
ev_page_cache_get_size (EvPageCache *page_cache,
			gint         page,
			gfloat       scale,
			gint        *width,
			gint        *height)
{
	g_return_if_fail (EV_IS_PAGE_CACHE (page_cache));
	g_return_if_fail (page >= 0 && page < page_cache->n_pages);

	if (page_cache->uniform) {
		if (width)
			*width = page_cache->uniform_width;
		if (height)
			*height = page_cache->uniform_height;
	} else {
		EvPageCacheInfo *info;

		info = &(page_cache->size_cache [page]);
		
		if (width)
			*width = info->width;
		if (height)
			*height = info->height;
	}

	if (width)
		*width = (int) ((*width) * scale + 0.5);
	if (width)
		*height = (int) ((*height) * scale + 0.5);

}


void
ev_page_cache_get_max_width      (EvPageCache *page_cache,
				  gfloat       scale,
				  gint        *width)
{
	g_return_if_fail (EV_IS_PAGE_CACHE (page_cache));

	if (width)
		*width = page_cache->max_width * scale;
}

void
ev_page_cache_get_max_height      (EvPageCache *page_cache,
				   gfloat       scale,
				   gint        *height)
{
	g_return_if_fail (EV_IS_PAGE_CACHE (page_cache));

	if (height)
		*height = page_cache->max_height * scale;
}

void    
ev_page_cache_get_height_to_page (EvPageCache *page_cache,
				  gint page,
				  gfloat       scale,
				  gint        *height,
				  gint 	      *dual_height)
{
	double result = 0.0;
	double dual_result = 0.0;
	
	g_return_if_fail (EV_IS_PAGE_CACHE (page_cache));

	if (page > 0)
		result = page_cache->height_to_page [page - 1];	
	
	if (height)
		*height = result * scale;

	if (page > 1)
		dual_result = page_cache->dual_height_to_page [page / 2 - 1];	
	
	if (dual_height)
		*dual_height = dual_result * scale;
}

gint
ev_page_cache_get_max_label_chars (EvPageCache *page_cache)
{
	g_return_val_if_fail (EV_IS_PAGE_CACHE (page_cache), 0);
	
	return page_cache->max_label_chars;
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


gboolean
ev_page_cache_next_page (EvPageCache *page_cache)
{
	g_return_val_if_fail (EV_IS_PAGE_CACHE (page_cache), FALSE);

	if (page_cache->current_page >= page_cache->n_pages - 1)
		return FALSE;

	ev_page_cache_set_current_page (page_cache, page_cache->current_page + 1);
	return TRUE;

}

gboolean
ev_page_cache_prev_page (EvPageCache *page_cache)
{
	g_return_val_if_fail (EV_IS_PAGE_CACHE (page_cache), FALSE);

	if (page_cache->current_page <= 0)
		return FALSE;

	ev_page_cache_set_current_page (page_cache, page_cache->current_page - 1);
	return TRUE;
}

