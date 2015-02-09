/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2015 Lauri Kasanen
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

#include <glib.h>
#include <math.h>
#include <stdlib.h>
#include "ev-document-text.h"
#include "ev-margin-cache.h"
#include "ev-view-private.h"

#define MINIFY_FACTOR 3

static unsigned num_pages;
static unsigned processing_page;

static void ev_margin_cache_compute_margins (EvView          *view,
					     gint             page,
					     cairo_surface_t *surface);
static void job_finished_cb                 (EvJob           *job,
					     EvView          *view);

static void
start_job (EvView *view, gint page)
{
	double float_width, float_height;
	int width, height;
	EvJob *job;

	ev_document_get_page_size (view->document, page, &float_width, &float_height);

	// Use a third in size for faster rendering. It's not exact anyway, as we add
	// some extra margins for readability's sake.
	width = ceil (float_width) / MINIFY_FACTOR;
	height = ceil (float_height) / MINIFY_FACTOR;

	job = ev_job_render_new (view->document, page, 0, 1, width, height);
	g_signal_connect (job, "finished",
			  G_CALLBACK (job_finished_cb),
			  view);
	ev_job_scheduler_push_job (job, EV_JOB_PRIORITY_HIGH);
}

GdkRectangle *
ev_margin_cache_new (EvView *view)
{
	GdkRectangle *rects;

	g_assert (EV_IS_DOCUMENT (view->document));

	num_pages = ev_document_get_n_pages (view->document);
	rects = g_new0 (GdkRectangle, num_pages);

	// Kick off the computation process in a thread, for all pages
	processing_page = 0;
	start_job (view, 0);

	return rects;
}

void
ev_margin_cache_free (EvView *view)
{
	g_assert (view != NULL);

	g_free (view->margin_cache);
	view->margin_cache = NULL;
}

gboolean
ev_margin_cache_is_page_cached (GdkRectangle *cache,
			        gint          page)
{
	g_assert (cache != NULL);

	return cache[page].width > 1;
}

static gboolean
nonwhite (const unsigned char * const pixel)
{
	// This is not guaranteed to be 4-byte aligned, so it could be
	// slightly sped up only on arches allowing unaligned reads.
	// However, the rendering takes ~200ms vs 160us for find_bounds.

	return pixel[0] != 0xff ||
	       pixel[1] != 0xff ||
	       pixel[2] != 0xff ||
	       pixel[3] != 0xff;
}

static void
find_bounds (cairo_surface_t * const surface,
	     const int               width,
	     const int               height,
	     int * const             minx,
	     int * const             maxx,
	     int * const             miny,
	     int * const             maxy)
{
	int i, j, stride;
	const unsigned char *data;
	gboolean found;

	*minx = *miny = 0;
	*maxx = width - 1;
	*maxy = height - 1;

	// Check that the surface is in the expected format
	g_assert (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_IMAGE);
	g_assert (cairo_image_surface_get_format (surface) == CAIRO_FORMAT_ARGB32);

	// Parsing. Four separate loops as we don't need to traverse every pixel.
	data = cairo_image_surface_get_data (surface);
	stride = cairo_image_surface_get_stride (surface);

	found = FALSE;
	for (i = 0; i < width && !found; i++) {
		for (j = 0; j < height && !found; j++) {
			const unsigned char * const pixel = &data[stride * j + i * 4];
			if (nonwhite (pixel)) {
				found = TRUE;
				*minx = i;
			}
		}
	}

	found = FALSE;
	for (j = 0; j < height && !found; j++) {
		for (i = *minx; i < width && !found; i++) {
			const unsigned char * const pixel = &data[stride * j + i * 4];
			if (nonwhite (pixel)) {
				found = TRUE;
				*miny = j;
			}
		}
	}

	found = FALSE;
	for (i = width - 1; i >= 0 && !found; i--) {
		for (j = height - 1; j >= 0 && !found; j--) {
			const unsigned char * const pixel = &data[stride * j + i * 4];
			if (nonwhite (pixel)) {
				found = TRUE;
				*maxx = i;
			}
		}
	}

	found = FALSE;
	for (j = height - 1; j >= 0 && !found; j--) {
		for (i = *maxx; i >= 0 && !found; i--) {
			const unsigned char * const pixel = &data[stride * j + i * 4];
			if (nonwhite (pixel)) {
				found = TRUE;
				*maxy = j;
			}
		}
	}
}

#define BORDER 18

static void
ev_margin_cache_compute_margins (EvView          *view,
                                 gint             page,
                                 cairo_surface_t *surface)
{
	double float_width, float_height;
	int width, height;
	int minx, miny, maxx, maxy;

	// Do nothing if already computed
	if (view->margin_cache[page].width > 1)
		return;

	ev_document_get_page_size (view->document, page, &float_width, &float_height);

	width = ceil (float_width) / MINIFY_FACTOR;
	height = ceil (float_height) / MINIFY_FACTOR;

	// Now get the content bounding box. We need to render it,
	// as the API can only give us the text bbox, which would fail
	// for image-heavy pages.
	//
	// While Poppler may be extended to do this faster in the future,
	// other backends may still require rendering.
	find_bounds (surface, width, height, &minx, &maxx, &miny, &maxy);

	// Add a quarter inch of border, if possible. At 72ppi (PDF standard), it's 18px.
	minx *= MINIFY_FACTOR;
	maxx *= MINIFY_FACTOR;
	miny *= MINIFY_FACTOR;
	maxy *= MINIFY_FACTOR;

	if (minx >= BORDER)
		minx -= BORDER;
	else
		minx = 0;

	if (maxx < (MINIFY_FACTOR * width) - BORDER)
		maxx += BORDER;
	else
		maxx = (MINIFY_FACTOR * width) - 1;

	if (miny >= BORDER)
		miny -= BORDER;
	else
		miny = 0;

	if (maxy < (MINIFY_FACTOR * height) - BORDER)
		maxy += BORDER;
	else
		maxy = (MINIFY_FACTOR * height) - 1;

	view->margin_cache[page].x = minx;
	view->margin_cache[page].y = miny;
	view->margin_cache[page].width = maxx - minx;
	view->margin_cache[page].height = maxy - miny;
}

static void
job_finished_cb (EvJob           *job,
		 EvView          *view)
{
	int diff;
	EvJobRender *job_render = EV_JOB_RENDER (job);

	ev_margin_cache_compute_margins (view, processing_page, job_render->surface);

	// If it was around the visible page, request a redraw
	diff = abs (view->current_page - (int) processing_page);
	if (diff < 2 && view->sizing_mode == EV_SIZING_FIT_CONTENTS) {
		gtk_widget_queue_draw (GTK_WIDGET (view));
	}

	g_signal_handlers_disconnect_by_func (job,
					      G_CALLBACK (job_finished_cb),
					      view);
	ev_job_cancel (job);
	g_object_unref (job);

	// Do we have more pages to render?
	if (processing_page != num_pages - 1) {
		processing_page++;
		start_job (view, processing_page);
	}
}
