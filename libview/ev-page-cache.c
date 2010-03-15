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

#include <config.h>

#include "ev-jobs.h"
#include "ev-job-scheduler.h"
#include "ev-mapping.h"
#include "ev-selection.h"
#include "ev-document-links.h"
#include "ev-document-forms.h"
#include "ev-document-images.h"
#include "ev-document-annotations.h"
#include "ev-page-cache.h"

typedef struct _EvPageCacheData {
	EvJob     *job;
	gboolean   done : 1;

	GList     *link_mapping;
	GList     *image_mapping;
	GList     *form_field_mapping;
	GList     *annot_mapping;
	GdkRegion *text_mapping;
} EvPageCacheData;

struct _EvPageCache {
	GObject parent;

	EvDocument        *document;
	EvPageCacheData   *page_list;
	gint               n_pages;
	EvJobPageDataFlags flags;
};

struct _EvPageCacheClass {
	GObjectClass parent_class;
};

static void job_page_data_finished_cb (EvJob       *job,
				       EvPageCache *cache);

G_DEFINE_TYPE (EvPageCache, ev_page_cache, G_TYPE_OBJECT)

static void
ev_page_cache_data_free (EvPageCacheData *data)
{
	if (data->job) {
		g_object_unref (data->job);
		data->job = NULL;
	}

	if (data->link_mapping) {
		ev_mapping_list_free (data->link_mapping, g_object_unref);
		data->link_mapping = NULL;
	}

	if (data->image_mapping) {
		ev_mapping_list_free (data->image_mapping, g_object_unref);
		data->image_mapping = NULL;
	}

	if (data->form_field_mapping) {
		ev_mapping_list_free (data->form_field_mapping, g_object_unref);
		data->form_field_mapping = NULL;
	}

	if (data->annot_mapping) {
		ev_mapping_list_free (data->annot_mapping, g_object_unref);
		data->annot_mapping = NULL;
	}

	if (data->text_mapping) {
		gdk_region_destroy (data->text_mapping);
		data->text_mapping = NULL;
	}
}

static void
ev_page_cache_finalize (GObject *object)
{
	EvPageCache *cache = EV_PAGE_CACHE (object);
	gint         i;

	if (cache->page_list) {
		for (i = 0; i < cache->n_pages; i++) {
			EvPageCacheData *data;

			data = &cache->page_list[i];

			if (data->job)
				g_signal_handlers_disconnect_by_func (data->job,
								      G_CALLBACK (job_page_data_finished_cb),
								      cache);
			ev_page_cache_data_free (data);
		}

		g_free (cache->page_list);
		cache->page_list = NULL;
		cache->n_pages = 0;
	}

	if (cache->document) {
		g_object_unref (cache->document);
		cache->document = NULL;
	}

	G_OBJECT_CLASS (ev_page_cache_parent_class)->finalize (object);
}

static void
ev_page_cache_init (EvPageCache *cache)
{
}

static void
ev_page_cache_class_init (EvPageCacheClass *klass)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

	g_object_class->finalize = ev_page_cache_finalize;
}

static EvJobPageDataFlags
get_flags_for_document (EvDocument *document)
{
	EvJobPageDataFlags flags = EV_PAGE_DATA_INCLUDE_NONE;

	if (EV_IS_DOCUMENT_LINKS (document))
		flags |= EV_PAGE_DATA_INCLUDE_LINKS;
	if (EV_IS_DOCUMENT_IMAGES (document))
		flags |= EV_PAGE_DATA_INCLUDE_IMAGES;
	if (EV_IS_DOCUMENT_FORMS (document))
		flags |= EV_PAGE_DATA_INCLUDE_FORMS;
	if (EV_IS_DOCUMENT_ANNOTATIONS (document))
		flags |= EV_PAGE_DATA_INCLUDE_ANNOTS;
	if (EV_IS_SELECTION (document))
		flags |= EV_PAGE_DATA_INCLUDE_TEXT;

	return flags;
}

EvPageCache *
ev_page_cache_new (EvDocument *document)
{
	EvPageCache *cache;

	g_return_val_if_fail (EV_IS_DOCUMENT (document), NULL);

	cache = EV_PAGE_CACHE (g_object_new (EV_TYPE_PAGE_CACHE, NULL));
	cache->document = g_object_ref (document);
	cache->n_pages = ev_document_get_n_pages (document);
	cache->flags = get_flags_for_document (document);

	if (cache->flags != EV_PAGE_DATA_INCLUDE_NONE) {
		cache->page_list = g_new0 (EvPageCacheData, cache->n_pages);
	}

	return cache;
}

static void
job_page_data_finished_cb (EvJob       *job,
			   EvPageCache *cache)
{
	EvJobPageData   *job_data = EV_JOB_PAGE_DATA (job);
	EvPageCacheData *data;

	data = &cache->page_list[job_data->page];
	data->link_mapping = job_data->link_mapping;
	data->image_mapping = job_data->image_mapping;
	data->form_field_mapping = job_data->form_field_mapping;
	data->annot_mapping = job_data->annot_mapping;
	data->text_mapping = job_data->text_mapping;
	data->done = TRUE;

	g_object_unref (data->job);
	data->job = NULL;
}

void
ev_page_cache_set_page_range (EvPageCache *cache,
			      gint         start,
			      gint         end)
{
	gint i;

	if (cache->flags == EV_PAGE_DATA_INCLUDE_NONE)
		return;

	for (i = start; i <= end; i++) {
		EvPageCacheData *data = &cache->page_list[i];

		if (data->done || data->job)
			continue;

		data->job = ev_job_page_data_new (cache->document, i, cache->flags);
		g_signal_connect (data->job, "finished",
				  G_CALLBACK (job_page_data_finished_cb),
				  cache);
		ev_job_scheduler_push_job (data->job, EV_JOB_PRIORITY_NONE);
	}
}

EvJobPageDataFlags
ev_page_cache_get_flags (EvPageCache *cache)
{
	return cache->flags;
}

void
ev_page_cache_set_flags (EvPageCache       *cache,
			 EvJobPageDataFlags flags)
{
	cache->flags = flags;
}

GList *
ev_page_cache_get_link_mapping (EvPageCache *cache,
				gint         page)
{
	EvPageCacheData *data;

	g_return_val_if_fail (EV_IS_PAGE_CACHE (cache), NULL);
	g_return_val_if_fail (page >= 0 && page < cache->n_pages, NULL);

	if (!(cache->flags & EV_PAGE_DATA_INCLUDE_LINKS))
		return NULL;

	data = &cache->page_list[page];
	if (data->done)
		return data->link_mapping;

	if (data->job)
		return EV_JOB_PAGE_DATA (data->job)->link_mapping;

	return data->link_mapping;
}

GList *
ev_page_cache_get_image_mapping (EvPageCache *cache,
				 gint         page)
{
	EvPageCacheData *data;

	g_return_val_if_fail (EV_IS_PAGE_CACHE (cache), NULL);
	g_return_val_if_fail (page >= 0 && page < cache->n_pages, NULL);

	if (!(cache->flags & EV_PAGE_DATA_INCLUDE_IMAGES))
		return NULL;

	data = &cache->page_list[page];
	if (data->done)
		return data->image_mapping;

	if (data->job)
		return EV_JOB_PAGE_DATA (data->job)->image_mapping;

	return data->image_mapping;
}

GList *
ev_page_cache_get_form_field_mapping (EvPageCache *cache,
				      gint         page)
{
	EvPageCacheData *data;

	g_return_val_if_fail (EV_IS_PAGE_CACHE (cache), NULL);
	g_return_val_if_fail (page >= 0 && page < cache->n_pages, NULL);

	if (!(cache->flags & EV_PAGE_DATA_INCLUDE_FORMS))
		return NULL;

	data = &cache->page_list[page];
	if (data->done)
		return data->form_field_mapping;

	if (data->job)
		return EV_JOB_PAGE_DATA (data->job)->form_field_mapping;

	return data->form_field_mapping;
}

GList *
ev_page_cache_get_annot_mapping (EvPageCache *cache,
				 gint         page)
{
	EvPageCacheData *data;

	g_return_val_if_fail (EV_IS_PAGE_CACHE (cache), NULL);
	g_return_val_if_fail (page >= 0 && page < cache->n_pages, NULL);

	if (!(cache->flags & EV_PAGE_DATA_INCLUDE_ANNOTS))
		return NULL;

	data = &cache->page_list[page];
	if (data->done)
		return data->annot_mapping;

	if (data->job)
		return EV_JOB_PAGE_DATA (data->job)->annot_mapping;

	return data->annot_mapping;
}

GdkRegion *
ev_page_cache_get_text_mapping (EvPageCache *cache,
				gint         page)
{
	EvPageCacheData *data;

	g_return_val_if_fail (EV_IS_PAGE_CACHE (cache), NULL);
	g_return_val_if_fail (page >= 0 && page < cache->n_pages, NULL);

	if (!(cache->flags & EV_PAGE_DATA_INCLUDE_TEXT))
		return NULL;

	data = &cache->page_list[page];
	if (data->done)
		return data->text_mapping;

	if (data->job)
		return EV_JOB_PAGE_DATA (data->job)->text_mapping;

	return data->text_mapping;
}

