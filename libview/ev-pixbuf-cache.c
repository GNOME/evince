#include <config.h>
#include "ev-pixbuf-cache.h"
#include "ev-job-scheduler.h"
#include "ev-view-private.h"

typedef enum {
        SCROLL_DIRECTION_DOWN,
        SCROLL_DIRECTION_UP
} ScrollDirection;

typedef struct _CacheJobInfo
{
	EvJob *job;
	gboolean page_ready;

	/* Region of the page that needs to be drawn */
	cairo_region_t  *region;
	GdkTexture *texture;

	/* Device scale factor of target widget */
	int device_scale;

	/* Selection data.
	 * Selection_points are the coordinates encapsulated in selection.
	 * target_points is the target selection size. */
	EvRectangle      target_points;
	EvSelectionStyle selection_style;
	gboolean         points_set;

	GdkTexture	*selection_texture;
	gdouble          selection_scale;
	EvRectangle      selection_points;

	cairo_region_t *selection_region;
	gdouble         selection_region_scale;
	EvRectangle     selection_region_points;
} CacheJobInfo;

struct _EvPixbufCache
{
	GObject parent;

	/* We keep a link to our containing view just for style information. */
	GtkWidget *view;
	EvDocument *document;
	EvDocumentModel *model;
	int start_page;
	int end_page;
        ScrollDirection scroll_direction;

	gsize max_size;

	/* preload_cache_size is the number of pages prior to the current
	 * visible area that we cache.  It's normally 1, but could be 2 in the
	 * case of twin pages.
	 */
	int preload_cache_size;
	guint job_list_len;

	CacheJobInfo *prev_job;
	CacheJobInfo *job_list;
	CacheJobInfo *next_job;
};

struct _EvPixbufCacheClass
{
	GObjectClass parent_class;

	void (* job_finished) (EvPixbufCache *pixbuf_cache);
};


enum
{
	JOB_FINISHED,
	N_SIGNALS,
};

static guint signals[N_SIGNALS] = {0, };

static void          ev_pixbuf_cache_init       (EvPixbufCache      *pixbuf_cache);
static void          ev_pixbuf_cache_class_init (EvPixbufCacheClass *pixbuf_cache);
static void          ev_pixbuf_cache_finalize   (GObject            *object);
static void          ev_pixbuf_cache_dispose    (GObject            *object);
static void          job_finished_cb            (EvJob              *job,
						 EvPixbufCache      *pixbuf_cache);
static CacheJobInfo *find_job_cache             (EvPixbufCache      *pixbuf_cache,
						 int                 page);
static gboolean      new_selection_surface_needed(EvPixbufCache      *pixbuf_cache,
						  CacheJobInfo       *job_info,
						  gint                page,
						  gfloat              scale);


/* These are used for iterating through the prev and next arrays */
#define FIRST_VISIBLE_PREV(pixbuf_cache) \
	(MAX (0, pixbuf_cache->preload_cache_size - pixbuf_cache->start_page))
#define VISIBLE_NEXT_LEN(pixbuf_cache) \
	(MIN(pixbuf_cache->preload_cache_size, ev_document_get_n_pages (pixbuf_cache->document) - (1 + pixbuf_cache->end_page)))
#define PAGE_CACHE_LEN(pixbuf_cache) \
	((pixbuf_cache->end_page - pixbuf_cache->start_page) + 1)

#define MAX_PRELOADED_PAGES 3

G_DEFINE_TYPE (EvPixbufCache, ev_pixbuf_cache, G_TYPE_OBJECT)

static void
ev_pixbuf_cache_init (EvPixbufCache *pixbuf_cache)
{
	pixbuf_cache->start_page = -1;
	pixbuf_cache->end_page = -1;
}

static void
ev_pixbuf_cache_class_init (EvPixbufCacheClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->finalize = ev_pixbuf_cache_finalize;
	object_class->dispose = ev_pixbuf_cache_dispose;

	signals[JOB_FINISHED] =
		g_signal_new ("job-finished",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (EvPixbufCacheClass, job_finished),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);
}

static void
ev_pixbuf_cache_finalize (GObject *object)
{
	EvPixbufCache *pixbuf_cache;

	pixbuf_cache = EV_PIXBUF_CACHE (object);

	if (pixbuf_cache->job_list) {
		g_slice_free1 (sizeof (CacheJobInfo) * pixbuf_cache->job_list_len,
			       pixbuf_cache->job_list);
		pixbuf_cache->job_list = NULL;
	}
	if (pixbuf_cache->prev_job) {
		g_slice_free1 (sizeof (CacheJobInfo) * pixbuf_cache->preload_cache_size,
			       pixbuf_cache->prev_job);
		pixbuf_cache->prev_job = NULL;
	}
	if (pixbuf_cache->next_job) {
		g_slice_free1 (sizeof (CacheJobInfo) * pixbuf_cache->preload_cache_size,
			       pixbuf_cache->next_job);
		pixbuf_cache->next_job = NULL;
	}

	g_object_unref (pixbuf_cache->model);

	G_OBJECT_CLASS (ev_pixbuf_cache_parent_class)->finalize (object);
}

static void
end_job (CacheJobInfo *job_info,
	 gpointer      data)
{
	g_signal_handlers_disconnect_by_func (job_info->job,
					      G_CALLBACK (job_finished_cb),
					      data);
	ev_job_cancel (job_info->job);
	g_clear_object (&job_info->job);
}

static void
dispose_cache_job_info (CacheJobInfo *job_info,
			gpointer      data)
{
	if (job_info == NULL)
		return;

	if (job_info->job)
		end_job (job_info, data);

	g_clear_object (&job_info->texture);
	g_clear_object (&job_info->selection_texture);
	g_clear_pointer (&job_info->region, cairo_region_destroy);
	g_clear_pointer (&job_info->selection_region, cairo_region_destroy);

	job_info->points_set = FALSE;
}

static void
ev_pixbuf_cache_dispose (GObject *object)
{
	EvPixbufCache *pixbuf_cache;
	int i;

	pixbuf_cache = EV_PIXBUF_CACHE (object);

	for (i = 0; i < pixbuf_cache->preload_cache_size; i++) {
		dispose_cache_job_info (pixbuf_cache->prev_job + i, pixbuf_cache);
		dispose_cache_job_info (pixbuf_cache->next_job + i, pixbuf_cache);
	}

	for (i = 0; i < PAGE_CACHE_LEN (pixbuf_cache); i++) {
		dispose_cache_job_info (pixbuf_cache->job_list + i, pixbuf_cache);
	}

	G_OBJECT_CLASS (ev_pixbuf_cache_parent_class)->dispose (object);
}


EvPixbufCache *
ev_pixbuf_cache_new (GtkWidget       *view,
		     EvDocumentModel *model,
		     gsize            max_size)
{
	EvPixbufCache *pixbuf_cache;

	pixbuf_cache = (EvPixbufCache *) g_object_new (EV_TYPE_PIXBUF_CACHE, NULL);
	/* This is a backlink, so we don't ref this */
	pixbuf_cache->view = view;
	pixbuf_cache->model = g_object_ref (model);
	pixbuf_cache->document = ev_document_model_get_document (model);
	pixbuf_cache->max_size = max_size;

	return pixbuf_cache;
}

void
ev_pixbuf_cache_set_max_size (EvPixbufCache *pixbuf_cache,
			      gsize          max_size)
{
	if (pixbuf_cache->max_size == max_size)
		return;

	if (pixbuf_cache->max_size > max_size)
		ev_pixbuf_cache_clear (pixbuf_cache);
	pixbuf_cache->max_size = max_size;
}

static int
get_device_scale (EvPixbufCache *pixbuf_cache)
{
        return gtk_widget_get_scale_factor (pixbuf_cache->view);
}

static void
set_device_scale_on_surface (cairo_surface_t *surface,
                             int              device_scale)
{
        cairo_surface_set_device_scale (surface, device_scale, device_scale);
}

static GdkTexture *
gdk_texture_new_for_surface (cairo_surface_t *surface)
{
	GdkTexture *texture;
	GBytes *bytes;

	g_return_val_if_fail (surface != NULL, NULL);
	g_return_val_if_fail (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_IMAGE, NULL);
	g_return_val_if_fail (cairo_image_surface_get_width (surface) > 0, NULL);
	g_return_val_if_fail (cairo_image_surface_get_height (surface) > 0, NULL);

	bytes = g_bytes_new_with_free_func (cairo_image_surface_get_data (surface),
					    cairo_image_surface_get_height (surface) * cairo_image_surface_get_stride (surface),
					    (GDestroyNotify)cairo_surface_destroy,
					    cairo_surface_reference (surface));

	texture = gdk_memory_texture_new (cairo_image_surface_get_width (surface),
					  cairo_image_surface_get_height (surface),
					  GDK_MEMORY_DEFAULT,
					  bytes,
					  cairo_image_surface_get_stride (surface));

	g_bytes_unref (bytes);

	return texture;
}

static void
copy_job_to_job_info (EvJobRenderTexture *job_render,
		      CacheJobInfo     *job_info,
		      EvPixbufCache    *pixbuf_cache)
{
	g_clear_object (&job_info->texture);

	job_info->texture = g_object_ref (job_render->texture);

	job_info->points_set = FALSE;
	if (job_render->include_selection) {
		g_clear_object (&job_info->selection_texture);
		g_clear_pointer (&job_info->selection_region, cairo_region_destroy);

		job_info->selection_points = job_render->selection_points;
		job_info->selection_scale = job_render->scale * job_info->device_scale;
		g_assert (job_info->selection_points.x1 >= 0);

		job_info->selection_region_points = job_render->selection_points;
		job_info->selection_region = cairo_region_reference (job_render->selection_region);
		job_info->selection_region_scale = job_render->scale;

		job_info->selection_texture = g_object_ref (job_render->selection);
		job_info->points_set = TRUE;
	}

	if (job_info->job)
		end_job (job_info, pixbuf_cache);

	job_info->page_ready = TRUE;
}

static void
job_finished_cb (EvJob         *job,
		 EvPixbufCache *pixbuf_cache)
{
	CacheJobInfo *job_info;
	EvJobRenderTexture *job_render = EV_JOB_RENDER_TEXTURE (job);

	/* If the job is outside of our interest, we silently discard it */
	if ((job_render->page < (pixbuf_cache->start_page - pixbuf_cache->preload_cache_size)) ||
	    (job_render->page > (pixbuf_cache->end_page + pixbuf_cache->preload_cache_size))) {
		g_object_unref (job);
		return;
	}

	job_info = find_job_cache (pixbuf_cache, job_render->page);

	if (ev_job_is_failed (job)) {
		g_clear_object (&job_info->job);
		return;
	}

	copy_job_to_job_info (job_render, job_info, pixbuf_cache);
	g_signal_emit (pixbuf_cache, signals[JOB_FINISHED], 0, job_info->region);
}

/* This checks a job to see if the job would generate the right sized pixbuf
 * given a scale.  If it won't, it removes the job and clears it to NULL.
 */
static void
check_job_size_and_unref (EvPixbufCache *pixbuf_cache,
			  CacheJobInfo  *job_info,
			  gfloat         scale)
{
	gint width, height;
	gint device_scale;

	g_assert (job_info);

	if (job_info->job == NULL)
		return;

        device_scale = get_device_scale (pixbuf_cache);
	if (job_info->device_scale == device_scale) {
		_get_page_size_for_scale_and_rotation (job_info->job->document,
						       EV_JOB_RENDER_TEXTURE (job_info->job)->page,
						       scale,
						       EV_JOB_RENDER_TEXTURE (job_info->job)->rotation,
						       &width, &height);
		if (width * device_scale == EV_JOB_RENDER_TEXTURE (job_info->job)->target_width &&
		    height * device_scale == EV_JOB_RENDER_TEXTURE (job_info->job)->target_height)
			return;
	}

	end_job (job_info, pixbuf_cache);
}

/* Do all function that copies a job from an older cache to it's position in the
 * new cache.  It clears the old job if it doesn't have a place.
 */
static void
move_one_job (CacheJobInfo  *job_info,
	      EvPixbufCache *pixbuf_cache,
	      int            page,
	      CacheJobInfo  *new_job_list,
	      CacheJobInfo  *new_prev_job,
	      CacheJobInfo  *new_next_job,
	      int            new_preload_cache_size,
	      int            start_page,
	      int            end_page,
	      gint           priority)
{
	CacheJobInfo *target_page = NULL;
	int page_offset;
	gint new_priority;

	if (page < (start_page - new_preload_cache_size) ||
	    page > (end_page + new_preload_cache_size)) {
		dispose_cache_job_info (job_info, pixbuf_cache);
		return;
	}

	/* find the target page to copy it over to. */
	if (page < start_page) {
		page_offset = (page - (start_page - new_preload_cache_size));

		g_assert (page_offset >= 0 &&
			  page_offset < new_preload_cache_size);
		target_page = new_prev_job + page_offset;
		new_priority = EV_JOB_PRIORITY_LOW;
	} else if (page > end_page) {
		page_offset = (page - (end_page + 1));

		g_assert (page_offset >= 0 &&
			  page_offset < new_preload_cache_size);
		target_page = new_next_job + page_offset;
		new_priority = EV_JOB_PRIORITY_LOW;
	} else {
		page_offset = page - start_page;
		g_assert (page_offset >= 0 &&
			  page_offset <= ((end_page - start_page) + 1));
		new_priority = EV_JOB_PRIORITY_URGENT;
		target_page = new_job_list + page_offset;
	}

	*target_page = *job_info;
	job_info->job = NULL;
	job_info->region = NULL;
	job_info->texture = NULL;

	if (new_priority != priority && target_page->job) {
		ev_job_scheduler_update_job (target_page->job, new_priority);
	}
}

static gsize
ev_pixbuf_cache_get_page_size (EvPixbufCache *pixbuf_cache,
			       gint           page_index,
			       gdouble        scale,
			       gint           rotation)
{
	gint width, height;

	_get_page_size_for_scale_and_rotation (pixbuf_cache->document,
					       page_index, scale, rotation,
					       &width, &height);
	return height * cairo_format_stride_for_width (CAIRO_FORMAT_RGB24, width);
}

static gint
ev_pixbuf_cache_get_preload_size (EvPixbufCache *pixbuf_cache,
				  gint           start_page,
				  gint           end_page,
				  gdouble        scale,
				  gint           rotation)
{
	gsize range_size = 0;
	gint  new_preload_cache_size = 0;
	gint  i;
	guint n_pages = ev_document_get_n_pages (pixbuf_cache->document);

	/* Get the size of the current range */
	for (i = start_page; i <= end_page; i++) {
		range_size += ev_pixbuf_cache_get_page_size (pixbuf_cache, i, scale, rotation);
	}

	if (range_size >= pixbuf_cache->max_size)
		return new_preload_cache_size;

	i = 1;
	while (((start_page - i > 0) || (end_page + i < n_pages)) &&
	       new_preload_cache_size < MAX_PRELOADED_PAGES) {
		gsize    page_size;
		gboolean updated = FALSE;

		if (end_page + i < n_pages) {
			page_size = ev_pixbuf_cache_get_page_size (pixbuf_cache, end_page + i,
								   scale, rotation);
			if (page_size + range_size <= pixbuf_cache->max_size) {
				range_size += page_size;
				new_preload_cache_size++;
				updated = TRUE;
			} else {
				break;
			}
		}

		if (start_page - i > 0) {
			page_size = ev_pixbuf_cache_get_page_size (pixbuf_cache, start_page - i,
								   scale, rotation);
			if (page_size + range_size <= pixbuf_cache->max_size) {
				range_size += page_size;
				if (!updated)
					new_preload_cache_size++;
			} else {
				break;
			}
		}
		i++;
	}

	return new_preload_cache_size;
}

static void
ev_pixbuf_cache_update_range (EvPixbufCache *pixbuf_cache,
			      gint           start_page,
			      gint           end_page,
			      guint          rotation,
			      gdouble        scale)
{
	CacheJobInfo *new_job_list;
	CacheJobInfo *new_prev_job = NULL;
	CacheJobInfo *new_next_job = NULL;
	gint          new_preload_cache_size;
	guint         new_job_list_len;
	int           i, page;

	new_preload_cache_size = ev_pixbuf_cache_get_preload_size (pixbuf_cache,
								   start_page,
								   end_page,
								   scale,
								   rotation);
	if (pixbuf_cache->start_page == start_page &&
	    pixbuf_cache->end_page == end_page &&
	    pixbuf_cache->preload_cache_size == new_preload_cache_size)
		return;

	new_job_list_len = (end_page - start_page) + 1;
	new_job_list = g_slice_alloc0 (sizeof (CacheJobInfo) * new_job_list_len);
	if (new_preload_cache_size > 0) {
		new_prev_job = g_slice_alloc0 (sizeof (CacheJobInfo) * new_preload_cache_size);
		new_next_job = g_slice_alloc0 (sizeof (CacheJobInfo) * new_preload_cache_size);
	}

	/* We go through each job in the old cache and either clear it or move
	 * it to a new location. */

	/* Start with the prev cache. */
	page = pixbuf_cache->start_page - pixbuf_cache->preload_cache_size;
	for (i = 0; i < pixbuf_cache->preload_cache_size; i++) {
		if (page < 0) {
			dispose_cache_job_info (pixbuf_cache->prev_job + i, pixbuf_cache);
		} else {
			move_one_job (pixbuf_cache->prev_job + i,
				      pixbuf_cache, page,
				      new_job_list, new_prev_job, new_next_job,
				      new_preload_cache_size,
				      start_page, end_page, EV_JOB_PRIORITY_LOW);
		}
		page ++;
	}

	page = pixbuf_cache->start_page;
	for (i = 0; i < PAGE_CACHE_LEN (pixbuf_cache) && page >= 0; i++) {
		move_one_job (pixbuf_cache->job_list + i,
			      pixbuf_cache, page,
			      new_job_list, new_prev_job, new_next_job,
			      new_preload_cache_size,
			      start_page, end_page, EV_JOB_PRIORITY_URGENT);
		page ++;
	}

	for (i = 0; i < pixbuf_cache->preload_cache_size; i++) {
		if (page >= ev_document_get_n_pages (pixbuf_cache->document)) {
			dispose_cache_job_info (pixbuf_cache->next_job + i, pixbuf_cache);
		} else {
			move_one_job (pixbuf_cache->next_job + i,
				      pixbuf_cache, page,
				      new_job_list, new_prev_job, new_next_job,
				      new_preload_cache_size,
				      start_page, end_page, EV_JOB_PRIORITY_LOW);
		}
		page ++;
	}

	if (pixbuf_cache->job_list) {
		g_slice_free1 (sizeof (CacheJobInfo) * pixbuf_cache->job_list_len,
			       pixbuf_cache->job_list);
	}
	if (pixbuf_cache->prev_job) {
		g_slice_free1 (sizeof (CacheJobInfo) * pixbuf_cache->preload_cache_size,
			       pixbuf_cache->prev_job);
	}
	if (pixbuf_cache->next_job) {
		g_slice_free1 (sizeof (CacheJobInfo) * pixbuf_cache->preload_cache_size,
			       pixbuf_cache->next_job);
	}

	pixbuf_cache->preload_cache_size = new_preload_cache_size;
	pixbuf_cache->job_list_len = new_job_list_len;

	pixbuf_cache->job_list = new_job_list;
	pixbuf_cache->prev_job = new_prev_job;
	pixbuf_cache->next_job = new_next_job;

	pixbuf_cache->start_page = start_page;
	pixbuf_cache->end_page = end_page;
}

static CacheJobInfo *
find_job_cache (EvPixbufCache *pixbuf_cache,
		int            page)
{
	int page_offset;

	if (page < (pixbuf_cache->start_page - pixbuf_cache->preload_cache_size) ||
	    page > (pixbuf_cache->end_page + pixbuf_cache->preload_cache_size))
		return NULL;

	if (page < pixbuf_cache->start_page) {
		page_offset = (page - (pixbuf_cache->start_page - pixbuf_cache->preload_cache_size));

		g_assert (page_offset >= 0 &&
			  page_offset < pixbuf_cache->preload_cache_size);
		return pixbuf_cache->prev_job + page_offset;
	}

	if (page > pixbuf_cache->end_page) {
		page_offset = (page - (pixbuf_cache->end_page + 1));

		g_assert (page_offset >= 0 &&
			  page_offset < pixbuf_cache->preload_cache_size);
		return pixbuf_cache->next_job + page_offset;
	}

	page_offset = page - pixbuf_cache->start_page;
	g_assert (page_offset >= 0 &&
		  page_offset <= PAGE_CACHE_LEN(pixbuf_cache));
	return pixbuf_cache->job_list + page_offset;
}

static void
ev_pixbuf_cache_clear_job_sizes (EvPixbufCache *pixbuf_cache,
				 gfloat         scale)
{
	int i;

	for (i = 0; i < PAGE_CACHE_LEN (pixbuf_cache); i++) {
		check_job_size_and_unref (pixbuf_cache, pixbuf_cache->job_list + i, scale);
	}

	for (i = 0; i < pixbuf_cache->preload_cache_size; i++) {
		check_job_size_and_unref (pixbuf_cache, pixbuf_cache->prev_job + i, scale);
		check_job_size_and_unref (pixbuf_cache, pixbuf_cache->next_job + i, scale);
	}
}

static void
add_job (EvPixbufCache  *pixbuf_cache,
	 CacheJobInfo   *job_info,
	 cairo_region_t *region,
	 gint            width,
	 gint            height,
	 gint            page,
	 gint            rotation,
	 gfloat          scale,
	 EvJobPriority   priority)
{
	job_info->device_scale = get_device_scale (pixbuf_cache);
	job_info->page_ready = FALSE;

	if (job_info->region)
		cairo_region_destroy (job_info->region);
	job_info->region = region ? cairo_region_reference (region) : NULL;

	if (job_info->job)
		end_job (job_info, pixbuf_cache);

	job_info->job = ev_job_render_texture_new (pixbuf_cache->document,
						 page, rotation,
						 scale * job_info->device_scale,
						 width * job_info->device_scale,
						 height * job_info->device_scale);

	if (new_selection_surface_needed (pixbuf_cache, job_info, page, scale)) {
		GdkRGBA text, base;

		_ev_view_get_selection_colors (EV_VIEW (pixbuf_cache->view), &base, &text);
		ev_job_render_texture_set_selection_info (EV_JOB_RENDER_TEXTURE (job_info->job),
							&(job_info->target_points),
							job_info->selection_style,
							&text, &base);
	}

	g_signal_connect (job_info->job, "finished",
			  G_CALLBACK (job_finished_cb),
			  pixbuf_cache);
	ev_job_scheduler_push_job (job_info->job, priority);
}

static void
add_job_if_needed (EvPixbufCache *pixbuf_cache,
		   CacheJobInfo  *job_info,
		   gint           page,
		   gint           rotation,
		   gfloat         scale,
		   EvJobPriority  priority)
{
	gint device_scale = get_device_scale (pixbuf_cache);
	gint width, height;

	if (job_info->job)
		return;

	_get_page_size_for_scale_and_rotation (pixbuf_cache->document,
					       page, scale, rotation,
					       &width, &height);

	if (job_info->texture &&
	    job_info->device_scale == device_scale &&
	    gdk_texture_get_width (job_info->texture) == width * device_scale &&
	    gdk_texture_get_height (job_info->texture) == height * device_scale)
		return;

	/* Free old surfaces for non visible pages */
	if (priority == EV_JOB_PRIORITY_LOW) {
		g_clear_object (&job_info->texture);
		g_clear_object (&job_info->selection_texture);
	}

	add_job (pixbuf_cache, job_info, NULL,
		 width, height, page, rotation, scale,
		 priority);
}

static void
add_prev_jobs_if_needed (EvPixbufCache *pixbuf_cache,
                         gint           rotation,
                         gfloat         scale)
{
        CacheJobInfo *job_info;
        int page;
        int i;

        for (i = pixbuf_cache->preload_cache_size - 1; i >= FIRST_VISIBLE_PREV(pixbuf_cache); i--) {
                job_info = (pixbuf_cache->prev_job + i);
                page = pixbuf_cache->start_page - pixbuf_cache->preload_cache_size + i;

                add_job_if_needed (pixbuf_cache, job_info,
                                   page, rotation, scale,
                                   EV_JOB_PRIORITY_LOW);
        }
}

static void
add_next_jobs_if_needed (EvPixbufCache *pixbuf_cache,
                         gint           rotation,
                         gfloat         scale)
{
        CacheJobInfo *job_info;
        int page;
        int i;

        for (i = 0; i < VISIBLE_NEXT_LEN(pixbuf_cache); i++) {
                job_info = (pixbuf_cache->next_job + i);
                page = pixbuf_cache->end_page + 1 + i;

                add_job_if_needed (pixbuf_cache, job_info,
                                   page, rotation, scale,
                                   EV_JOB_PRIORITY_LOW);
        }
}

static void
ev_pixbuf_cache_add_jobs_if_needed (EvPixbufCache *pixbuf_cache,
				    gint           rotation,
				    gfloat         scale)
{
	CacheJobInfo *job_info;
	int page;
	int i;

	for (i = 0; i < PAGE_CACHE_LEN (pixbuf_cache); i++) {
		job_info = (pixbuf_cache->job_list + i);
		page = pixbuf_cache->start_page + i;

		add_job_if_needed (pixbuf_cache, job_info,
				   page, rotation, scale,
				   EV_JOB_PRIORITY_URGENT);
	}

        if (pixbuf_cache->scroll_direction == SCROLL_DIRECTION_UP) {
                add_prev_jobs_if_needed (pixbuf_cache, rotation, scale);
                add_next_jobs_if_needed (pixbuf_cache, rotation, scale);
        } else {
                add_next_jobs_if_needed (pixbuf_cache, rotation, scale);
                add_prev_jobs_if_needed (pixbuf_cache, rotation, scale);
        }
}

static ScrollDirection
ev_pixbuf_cache_get_scroll_direction (EvPixbufCache *pixbuf_cache,
                                      gint           start_page,
                                      gint           end_page)
{
        if (start_page < pixbuf_cache->start_page)
                return SCROLL_DIRECTION_UP;

        if (end_page > pixbuf_cache->end_page)
                return SCROLL_DIRECTION_DOWN;

        if (start_page > pixbuf_cache->start_page)
                return SCROLL_DIRECTION_DOWN;

        if (end_page < pixbuf_cache->end_page)
                return SCROLL_DIRECTION_UP;

        return pixbuf_cache->scroll_direction;
}

void
ev_pixbuf_cache_set_page_range (EvPixbufCache  *pixbuf_cache,
				gint            start_page,
				gint            end_page,
				GList          *selection_list)
{
	gdouble scale = ev_document_model_get_scale (pixbuf_cache->model);
	gint    rotation = ev_document_model_get_rotation (pixbuf_cache->model);

	g_return_if_fail (EV_IS_PIXBUF_CACHE (pixbuf_cache));

	g_return_if_fail (start_page >= 0 && start_page < ev_document_get_n_pages (pixbuf_cache->document));
	g_return_if_fail (end_page >= 0 && end_page < ev_document_get_n_pages (pixbuf_cache->document));
	g_return_if_fail (end_page >= start_page);

        pixbuf_cache->scroll_direction = ev_pixbuf_cache_get_scroll_direction (pixbuf_cache, start_page, end_page);

	/* First, resize the page_range as needed.  We cull old pages
	 * mercilessly. */
	ev_pixbuf_cache_update_range (pixbuf_cache, start_page, end_page, rotation, scale);

	/* Then, we update the current jobs to see if any of them are the wrong
	 * size, we remove them if we need to. */
	ev_pixbuf_cache_clear_job_sizes (pixbuf_cache, scale);

	/* Next, we update the target selection for our pages */
	ev_pixbuf_cache_set_selection_list (pixbuf_cache, selection_list);

	/* Finally, we add the new jobs for all the sizes that don't have a
	 * pixbuf */
	ev_pixbuf_cache_add_jobs_if_needed (pixbuf_cache, rotation, scale);
}

GdkTexture *
ev_pixbuf_cache_get_texture (EvPixbufCache *pixbuf_cache,
			     gint           page)
{
	CacheJobInfo *job_info;

	job_info = find_job_cache (pixbuf_cache, page);
	if (job_info == NULL)
		return NULL;

	if (job_info->page_ready)
		return job_info->texture;

	/* We don't need to wait for the idle to handle the callback */
	if (job_info->job &&
	    EV_JOB_RENDER_TEXTURE (job_info->job)->page_ready) {
		copy_job_to_job_info (EV_JOB_RENDER_TEXTURE (job_info->job), job_info, pixbuf_cache);
		g_signal_emit (pixbuf_cache, signals[JOB_FINISHED], 0, job_info->region);
	}

	return job_info->texture;
}

static gboolean
new_selection_surface_needed (EvPixbufCache *pixbuf_cache,
			      CacheJobInfo  *job_info,
			      gint           page,
			      gfloat         scale)
{
	if (job_info->selection_texture)
		return job_info->selection_scale != scale;
	return job_info->points_set;
}

static gboolean
new_selection_region_needed (EvPixbufCache *pixbuf_cache,
			     CacheJobInfo  *job_info,
			     gint           page,
			     gfloat         scale)
{
	if (job_info->selection_region)
		return job_info->selection_region_scale != scale;
	return job_info->points_set;
}

static void
clear_selection_surface_if_needed (EvPixbufCache *pixbuf_cache,
                                   CacheJobInfo  *job_info,
                                   gint           page,
                                   gfloat         scale)
{
	if (new_selection_surface_needed (pixbuf_cache, job_info, page, scale)) {
		g_clear_object (&job_info->selection_texture);
		job_info->selection_points.x1 = -1;
	}
}

static void
clear_selection_region_if_needed (EvPixbufCache *pixbuf_cache,
                                  CacheJobInfo  *job_info,
                                  gint           page,
                                  gfloat         scale)
{
	if (new_selection_region_needed (pixbuf_cache, job_info, page, scale)) {
		g_clear_pointer (&job_info->selection_region, cairo_region_destroy);
		job_info->selection_region_points.x1 = -1;
	}
}

/* Clears the cache of jobs and pixbufs.
 */
void
ev_pixbuf_cache_clear (EvPixbufCache *pixbuf_cache)
{
	int i;

	if (!pixbuf_cache->job_list)
		return;

	for (i = 0; i < pixbuf_cache->preload_cache_size; i++) {
		dispose_cache_job_info (pixbuf_cache->prev_job + i, pixbuf_cache);
		dispose_cache_job_info (pixbuf_cache->next_job + i, pixbuf_cache);
	}

	for (i = 0; i < PAGE_CACHE_LEN (pixbuf_cache); i++) {
		dispose_cache_job_info (pixbuf_cache->job_list + i, pixbuf_cache);
	}
}


void
ev_pixbuf_cache_style_changed (EvPixbufCache *pixbuf_cache)
{
	gint i;

	if (!pixbuf_cache->job_list)
		return;

	/* FIXME: doesn't update running jobs. */
	for (i = 0; i < pixbuf_cache->preload_cache_size; i++) {
		CacheJobInfo *job_info;

		job_info = pixbuf_cache->prev_job + i;
		if (job_info->selection_texture) {
			g_clear_object (&job_info->selection_texture);
			job_info->selection_points.x1 = -1;
		}

		job_info = pixbuf_cache->next_job + i;

		if (job_info->selection_texture) {
			g_clear_object (&job_info->selection_texture);
			job_info->selection_points.x1 = -1;
		}
	}

	for (i = 0; i < PAGE_CACHE_LEN (pixbuf_cache); i++) {
		CacheJobInfo *job_info;

		job_info = pixbuf_cache->job_list + i;

		if (job_info->selection_texture) {
			g_clear_object (&job_info->selection_texture);
			job_info->selection_points.x1 = -1;
		}
	}
}

GdkTexture *
ev_pixbuf_cache_get_selection_texture (EvPixbufCache   *pixbuf_cache,
				       gint             page,
				       gfloat           scale)
{
	CacheJobInfo *job_info;

	/* the document does not implement the selection interface */
	if (!EV_IS_SELECTION (pixbuf_cache->document))
		return NULL;

	job_info = find_job_cache (pixbuf_cache, page);
	if (job_info == NULL)
		return NULL;

	/* No selection on this page */
	if (!job_info->points_set)
		return NULL;

	/* If we have a running job, we just return what we have under the
	 * assumption that it'll be updated later and we can scale it as need
	 * be */
	if (job_info->job && EV_JOB_RENDER_TEXTURE (job_info->job)->include_selection)
		return job_info->selection_texture;

	/* Now, lets see if we need to resize the image.  If we do, we clear the
	 * old one. */
	clear_selection_surface_if_needed (pixbuf_cache, job_info, page, scale);

	/* Finally, we see if the two scales are the same, and get a new pixbuf
	 * if needed.  We do this synchronously for now.  At some point, we
	 * _should_ be able to get rid of the doc_mutex, so the synchronicity
	 * doesn't kill us.  Rendering a few glyphs should really be fast.
	 */
	if (ev_rect_cmp (&(job_info->target_points), &(job_info->selection_points))) {
		EvRectangle *old_points;
		GdkRGBA text, base;
		EvRenderContext *rc;
		EvPage *ev_page;
		gint width, height;
		cairo_surface_t *selection = NULL;

		/* we need to get a new selection pixbuf */
		ev_document_doc_mutex_lock ();
		if (job_info->selection_points.x1 < 0) {
			g_assert (job_info->selection_texture == NULL);
			old_points = NULL;
		} else {
			old_points = &(job_info->selection_points);
		}

		ev_page = ev_document_get_page (pixbuf_cache->document, page);
                _get_page_size_for_scale_and_rotation (pixbuf_cache->document,
                                                       page,
						       scale * job_info->device_scale,
						       0, &width, &height);

		rc = ev_render_context_new (ev_page, 0, scale * job_info->device_scale);
                ev_render_context_set_target_size (rc, width, height);
		g_object_unref (ev_page);

		_ev_view_get_selection_colors (EV_VIEW (pixbuf_cache->view), &base, &text);
		ev_selection_render_selection (EV_SELECTION (pixbuf_cache->document),
					       rc, &selection,
					       &(job_info->target_points),
					       old_points,
					       job_info->selection_style,
					       &text, &base);
                if (selection)
                        set_device_scale_on_surface (selection, job_info->device_scale);
		job_info->selection_points = job_info->target_points;
		job_info->selection_scale = scale * job_info->device_scale;
		job_info->selection_texture = gdk_texture_new_for_surface (selection);
		cairo_surface_destroy (selection);
		g_object_unref (rc);
		ev_document_doc_mutex_unlock ();
	}
	return job_info->selection_texture;
}

cairo_region_t *
ev_pixbuf_cache_get_selection_region (EvPixbufCache *pixbuf_cache,
				      gint           page,
				      gfloat         scale)
{
	CacheJobInfo *job_info;

	/* the document does not implement the selection interface */
	if (!EV_IS_SELECTION (pixbuf_cache->document))
		return NULL;

	job_info = find_job_cache (pixbuf_cache, page);
	if (job_info == NULL)
		return NULL;

	/* No selection on this page */
	if (!job_info->points_set)
		return NULL;

	/* If we have a running job, we just return what we have under the
	 * assumption that it'll be updated later and we can scale it as need
	 * be */
	if (job_info->job && EV_JOB_RENDER_CAIRO (job_info->job)->include_selection)
		return job_info->selection_region && !cairo_region_is_empty(job_info->selection_region) ?
                        job_info->selection_region : NULL;

	/* Now, lets see if we need to resize the region.  If we do, we clear the
	 * old one. */
	clear_selection_region_if_needed (pixbuf_cache, job_info, page, scale);

	/* Finally, we see if the two scales are the same, and get a new region
	 * if needed.
	 */
	if (ev_rect_cmp (&(job_info->target_points), &(job_info->selection_region_points))) {
		EvRenderContext *rc;
		EvPage *ev_page;
		gint width, height;

		ev_document_doc_mutex_lock ();
		ev_page = ev_document_get_page (pixbuf_cache->document, page);

		_get_page_size_for_scale_and_rotation (pixbuf_cache->document,
						       page, scale, 0,
						       &width, &height);

		rc = ev_render_context_new (ev_page, 0, 0.);
		ev_render_context_set_target_size (rc, width, height);
		g_object_unref (ev_page);

		if (job_info->selection_region)
			cairo_region_destroy (job_info->selection_region);
		job_info->selection_region =
			ev_selection_get_selection_region (EV_SELECTION (pixbuf_cache->document),
							   rc, job_info->selection_style,
							   &(job_info->target_points));
		job_info->selection_region_points = job_info->target_points;
		job_info->selection_region_scale = scale;
		g_object_unref (rc);
		ev_document_doc_mutex_unlock ();
	}
	return job_info->selection_region && !cairo_region_is_empty(job_info->selection_region) ?
                job_info->selection_region : NULL;
}

static void
update_job_selection (CacheJobInfo    *job_info,
		      EvViewSelection *selection)
{
	job_info->points_set = TRUE;
	job_info->target_points = selection->rect;
	job_info->selection_style = selection->style;
}

static void
clear_job_selection (CacheJobInfo *job_info)
{
	job_info->points_set = FALSE;
	job_info->selection_points.x1 = -1;

	g_clear_object (&job_info->selection_texture);
	g_clear_pointer (&job_info->selection_region, cairo_region_destroy);
}

/* This function will reset the selection on pages that no longer have them, and
 * will update the target_selection on those that need it.  It will _not_ free
 * the previous selection_list -- that's up to caller to do.
 */
void
ev_pixbuf_cache_set_selection_list (EvPixbufCache *pixbuf_cache,
				    GList         *selection_list)
{
	EvViewSelection *selection;
	GList *list = selection_list;
	int page;
	int i;

	g_return_if_fail (EV_IS_PIXBUF_CACHE (pixbuf_cache));

	if (!EV_IS_SELECTION (pixbuf_cache->document))
		return;

        if (pixbuf_cache->start_page == -1 || pixbuf_cache->end_page == -1)
                return;

	/* We check each area to see what needs updating, and what needs freeing; */
	page = pixbuf_cache->start_page - pixbuf_cache->preload_cache_size;
	for (i = 0; i < pixbuf_cache->preload_cache_size; i++) {
		if (page < 0) {
			page ++;
			continue;
		}

		selection = NULL;
		while (list) {
			if (((EvViewSelection *)list->data)->page == page) {
				selection = list->data;
				break;
			} else if (((EvViewSelection *)list->data)->page > page)
				break;
			list = list->next;
		}

		if (selection)
			update_job_selection (pixbuf_cache->prev_job + i, selection);
		else
			clear_job_selection (pixbuf_cache->prev_job + i);
		page ++;
	}

	page = pixbuf_cache->start_page;
	for (i = 0; i < PAGE_CACHE_LEN (pixbuf_cache); i++) {
		selection = NULL;
		while (list) {
			if (((EvViewSelection *)list->data)->page == page) {
				selection = list->data;
				break;
			} else if (((EvViewSelection *)list->data)->page > page)
				break;
			list = list->next;
		}

		if (selection)
			update_job_selection (pixbuf_cache->job_list + i, selection);
		else
			clear_job_selection (pixbuf_cache->job_list + i);
		page ++;
	}

	for (i = 0; i < pixbuf_cache->preload_cache_size; i++) {
		if (page >= ev_document_get_n_pages (pixbuf_cache->document))
			break;

		selection = NULL;
		while (list) {
			if (((EvViewSelection *)list->data)->page == page) {
				selection = list->data;
				break;
			} else if (((EvViewSelection *)list->data)->page > page)
				break;
			list = list->next;
		}

		if (selection)
			update_job_selection (pixbuf_cache->next_job + i, selection);
		else
			clear_job_selection (pixbuf_cache->next_job + i);
		page ++;
	}
}


/* Returns what the pixbuf cache thinks is */

GList *
ev_pixbuf_cache_get_selection_list (EvPixbufCache *pixbuf_cache)
{
	EvViewSelection *selection;
	GList *retval = NULL;
	int page;
	int i;

	g_return_val_if_fail (EV_IS_PIXBUF_CACHE (pixbuf_cache), NULL);

        if (pixbuf_cache->start_page == -1 || pixbuf_cache->end_page == -1)
                return NULL;

	/* We check each area to see what needs updating, and what needs freeing; */
	page = pixbuf_cache->start_page - pixbuf_cache->preload_cache_size;
	for (i = 0; i < pixbuf_cache->preload_cache_size; i++) {
		if (page < 0) {
			page ++;
			continue;
		}

		if (pixbuf_cache->prev_job[i].selection_points.x1 != -1) {
			selection = g_slice_new0 (EvViewSelection);
			selection->page = page;
			selection->rect = pixbuf_cache->prev_job[i].selection_points;
			if (pixbuf_cache->prev_job[i].selection_region)
				selection->covered_region = cairo_region_reference (pixbuf_cache->prev_job[i].selection_region);
			retval = g_list_prepend (retval, selection);
		}

		page ++;
	}

	page = pixbuf_cache->start_page;
	for (i = 0; i < PAGE_CACHE_LEN (pixbuf_cache); i++) {
		if (pixbuf_cache->job_list[i].selection_points.x1 != -1) {
			selection = g_slice_new0 (EvViewSelection);
			selection->page = page;
			selection->rect = pixbuf_cache->job_list[i].selection_points;
			if (pixbuf_cache->job_list[i].selection_region)
				selection->covered_region = cairo_region_reference (pixbuf_cache->job_list[i].selection_region);
			retval = g_list_prepend (retval, selection);
		}

		page ++;
	}

	for (i = 0; i < pixbuf_cache->preload_cache_size; i++) {
		if (page >= ev_document_get_n_pages (pixbuf_cache->document))
			break;

		if (pixbuf_cache->next_job[i].selection_points.x1 != -1) {
			selection = g_slice_new0 (EvViewSelection);
			selection->page = page;
			selection->rect = pixbuf_cache->next_job[i].selection_points;
			if (pixbuf_cache->next_job[i].selection_region)
				selection->covered_region = cairo_region_reference (pixbuf_cache->next_job[i].selection_region);
			retval = g_list_prepend (retval, selection);
		}

		page ++;
	}

	return g_list_reverse (retval);
}

void
ev_pixbuf_cache_reload_page (EvPixbufCache  *pixbuf_cache,
			     cairo_region_t *region,
			     gint            page,
			     gint            rotation,
			     gdouble         scale)
{
	CacheJobInfo *job_info;
        gint width, height;

	job_info = find_job_cache (pixbuf_cache, page);
	if (job_info == NULL)
		return;

	_get_page_size_for_scale_and_rotation (pixbuf_cache->document,
					       page, scale, rotation,
					       &width, &height);
        add_job (pixbuf_cache, job_info, region,
		 width, height, page, rotation, scale,
		 EV_JOB_PRIORITY_URGENT);
}
