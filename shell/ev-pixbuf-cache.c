#include "ev-pixbuf-cache.h"
#include "ev-job-queue.h"
#include "ev-page-cache.h"
#include "ev-selection.h"

typedef struct _CacheJobInfo
{
	EvJob *job;
	EvRenderContext *rc;

	/* Data we get from rendering */
	GdkPixbuf *pixbuf;
	GList *link_mapping;
	GdkRegion *text_mapping;
	
	/* Selection data. 
	 * Selection_points are the coordinates encapsulated in selection.
	 * target_points is the target selection size. */
	EvRectangle selection_points;
	EvRectangle target_points;
	gboolean    points_set;
	
	GdkPixbuf *selection;
	GdkRegion *selection_region;
} CacheJobInfo;

struct _EvPixbufCache
{
	GObject parent;

	/* We keep a link to our containing view just for style information. */
	GtkWidget *view;
	EvDocument *document;
	int start_page;
	int end_page;

	/* preload_cache_size is the number of pages prior to the current
	 * visible area that we cache.  It's normally 1, but could be 2 in the
	 * case of twin pages.
	 */
	int preload_cache_size;
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
static void          copy_job_to_job_info       (EvJobRender        *job_render,
						 CacheJobInfo       *job_info,
						 EvPixbufCache      *pixbuf_cache);
static gboolean      new_selection_pixbuf_needed(EvPixbufCache      *pixbuf_cache,
						 CacheJobInfo       *job_info,
						 gint                page,
						 gfloat              scale);


/* These are used for iterating through the prev and next arrays */
#define FIRST_VISABLE_PREV(pixbuf_cache) \
	(MAX (0, pixbuf_cache->preload_cache_size + 1 - pixbuf_cache->start_page))
#define VISIBLE_NEXT_LEN(pixbuf_cache, page_cache) \
	(MIN(pixbuf_cache->preload_cache_size, ev_page_cache_get_n_pages (page_cache) - (1 + pixbuf_cache->end_page)))
#define PAGE_CACHE_LEN(pixbuf_cache) \
	((pixbuf_cache->end_page - pixbuf_cache->start_page) + 1)

G_DEFINE_TYPE (EvPixbufCache, ev_pixbuf_cache, G_TYPE_OBJECT)

static void
ev_pixbuf_cache_init (EvPixbufCache *pixbuf_cache)
{
	pixbuf_cache->start_page = 0;
	pixbuf_cache->end_page = 0;
	pixbuf_cache->job_list = g_new0 (CacheJobInfo, PAGE_CACHE_LEN (pixbuf_cache));

	pixbuf_cache->preload_cache_size = 2;
	pixbuf_cache->prev_job = g_new0 (CacheJobInfo, pixbuf_cache->preload_cache_size);
	pixbuf_cache->next_job = g_new0 (CacheJobInfo, pixbuf_cache->preload_cache_size);
}

static void
ev_pixbuf_cache_class_init (EvPixbufCacheClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->finalize = ev_pixbuf_cache_finalize;
	object_class->dispose = ev_pixbuf_cache_dispose;

	signals[JOB_FINISHED] = g_signal_new ("job-finished",
					    G_OBJECT_CLASS_TYPE (object_class),
					    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
					    G_STRUCT_OFFSET (EvPixbufCacheClass, job_finished),
					    NULL, NULL,
					    g_cclosure_marshal_VOID__VOID,
					    G_TYPE_NONE, 0);
}

static void
ev_pixbuf_cache_finalize (GObject *object)
{
	EvPixbufCache *pixbuf_cache;

	pixbuf_cache = EV_PIXBUF_CACHE (object);

	g_free (pixbuf_cache->prev_job);
	g_free (pixbuf_cache->job_list);
	g_free (pixbuf_cache->next_job);
}

static void
dispose_cache_job_info (CacheJobInfo *job_info,
			gpointer      data)
{
	if (job_info == NULL)
		return;
	if (job_info->job) {
		g_signal_handlers_disconnect_by_func (job_info->job,
						      G_CALLBACK (job_finished_cb),
						      data);
		ev_job_queue_remove_job (job_info->job);
		g_object_unref (G_OBJECT (job_info->job));
		job_info->job = NULL;
	}
	if (job_info->pixbuf) {
		g_object_unref (G_OBJECT (job_info->pixbuf));
		job_info->pixbuf = NULL;
	}
	if (job_info->link_mapping) {
		ev_link_mapping_free (job_info->link_mapping);
		job_info->link_mapping = NULL;
	}
	if (job_info->text_mapping) {
		gdk_region_destroy (job_info->text_mapping);
		job_info->text_mapping = NULL;
	}
	if (job_info->selection) {
		g_object_unref (G_OBJECT (job_info->selection));
		job_info->selection = NULL;
	}
	if (job_info->selection_region) {
		gdk_region_destroy (job_info->selection_region);
		job_info->selection_region = NULL;
	}
	if (job_info->rc) {
		g_object_unref (G_OBJECT (job_info->rc));
		job_info->rc = NULL;
	}

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
}


EvPixbufCache *
ev_pixbuf_cache_new (GtkWidget  *view,
		     EvDocument *document)
{
	EvPixbufCache *pixbuf_cache;

	pixbuf_cache = (EvPixbufCache *) g_object_new (EV_TYPE_PIXBUF_CACHE, NULL);
	/* This is a backlink, so we don't ref this */ 
	pixbuf_cache->view = view;
	pixbuf_cache->document = document;

	return pixbuf_cache;
}

static void
job_finished_cb (EvJob         *job,
		 EvPixbufCache *pixbuf_cache)
{
	CacheJobInfo *job_info;
	EvJobRender *job_render = EV_JOB_RENDER (job);

	/* If the job is outside of our interest, we silently discard it */
	if ((job_render->rc->page < (pixbuf_cache->start_page - pixbuf_cache->preload_cache_size)) ||
	    (job_render->rc->page > (pixbuf_cache->end_page + pixbuf_cache->preload_cache_size))) {
		g_object_unref (job);
		return;
	}
	
	job_info = find_job_cache (pixbuf_cache, job_render->rc->page);

	copy_job_to_job_info (job_render, job_info, pixbuf_cache);

	g_signal_emit (pixbuf_cache, signals[JOB_FINISHED], 0);
}

/* This checks a job to see if the job would generate the right sized pixbuf
 * given a scale.  If it won't, it removes the job and clears it to NULL.
 */
static void
check_job_size_and_unref (EvPixbufCache *pixbuf_cache,
			  CacheJobInfo *job_info,
			  EvPageCache  *page_cache,
			  gfloat        scale)
{
	gint width;
	gint height;

	g_assert (job_info);

	if (job_info->job == NULL)
		return;

	ev_page_cache_get_size (page_cache,
				EV_JOB_RENDER (job_info->job)->rc->page,
				EV_JOB_RENDER (job_info->job)->rc->rotation,
				scale,
				&width, &height);
				
	if (width == EV_JOB_RENDER (job_info->job)->target_width &&
	    height == EV_JOB_RENDER (job_info->job)->target_height)
		return;

	g_signal_handlers_disconnect_by_func (job_info->job,
					      G_CALLBACK (job_finished_cb),
					      pixbuf_cache);
	ev_job_queue_remove_job (job_info->job);
	g_object_unref (job_info->job);
	job_info->job = NULL;
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
	      int            start_page,
	      int            end_page,
	      EvJobPriority  priority)
{
	CacheJobInfo *target_page = NULL;
	int page_offset;
	EvJobPriority new_priority;

	if (page < (start_page - pixbuf_cache->preload_cache_size) ||
	    page > (end_page + pixbuf_cache->preload_cache_size)) {
		dispose_cache_job_info (job_info, pixbuf_cache);
		return;
	}

	/* find the target page to copy it over to. */
	if (page < start_page) {
		page_offset = (page - (start_page - pixbuf_cache->preload_cache_size));

		g_assert (page_offset >= 0 &&
			  page_offset < pixbuf_cache->preload_cache_size);
		target_page = new_prev_job + page_offset;
		new_priority = EV_JOB_PRIORITY_LOW;
	} else if (page > end_page) {
		page_offset = (page - (end_page + 1));

		g_assert (page_offset >= 0 &&
			  page_offset < pixbuf_cache->preload_cache_size);
		target_page = new_next_job + page_offset;
		new_priority = EV_JOB_PRIORITY_LOW;
	} else {
		page_offset = page - start_page;
		g_assert (page_offset >= 0 &&
			  page_offset <= ((end_page - start_page) + 1));
		new_priority = EV_JOB_PRIORITY_HIGH;
		target_page = new_job_list + page_offset;
	}

	*target_page = *job_info;
	job_info->job = NULL;
	job_info->pixbuf = NULL;
	job_info->link_mapping = NULL;

	if (new_priority != priority && target_page->job) {
		ev_job_queue_update_job (target_page->job, new_priority);
	}
}



static void
ev_pixbuf_cache_update_range (EvPixbufCache *pixbuf_cache,
			      gint           start_page,
			      gint           end_page)
{
	CacheJobInfo *new_job_list;
	CacheJobInfo *new_prev_job;
	CacheJobInfo *new_next_job;
	EvPageCache *page_cache;
	int i, page;

	if (pixbuf_cache->start_page == start_page &&
	    pixbuf_cache->end_page == end_page)
		return;

	page_cache = ev_page_cache_get (pixbuf_cache->document);

	new_job_list = g_new0 (CacheJobInfo, (end_page - start_page) + 1);
	new_prev_job = g_new0 (CacheJobInfo, pixbuf_cache->preload_cache_size);
	new_next_job = g_new0 (CacheJobInfo, pixbuf_cache->preload_cache_size);

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
				      start_page, end_page, EV_JOB_PRIORITY_LOW);
		}
		page ++;
	}

	page = pixbuf_cache->start_page;
	for (i = 0; i < PAGE_CACHE_LEN (pixbuf_cache); i++) {
		move_one_job (pixbuf_cache->job_list + i,
			      pixbuf_cache, page,
			      new_job_list, new_prev_job, new_next_job,
			      start_page, end_page, EV_JOB_PRIORITY_HIGH);
		page ++;
	}

	for (i = 0; i < pixbuf_cache->preload_cache_size; i++) {
		if (page >= ev_page_cache_get_n_pages (page_cache)) {
			dispose_cache_job_info (pixbuf_cache->next_job + i, pixbuf_cache);
		} else {
			move_one_job (pixbuf_cache->next_job + i,
				      pixbuf_cache, page,
				      new_job_list, new_prev_job, new_next_job,
				      start_page, end_page, EV_JOB_PRIORITY_LOW);
		}
		page ++;
	}

	g_free (pixbuf_cache->job_list);
	g_free (pixbuf_cache->prev_job);
	g_free (pixbuf_cache->next_job);

	pixbuf_cache->job_list = new_job_list;
	pixbuf_cache->prev_job = new_prev_job;
	pixbuf_cache->next_job = new_next_job;

	pixbuf_cache->start_page = start_page;
	pixbuf_cache->end_page = end_page;
}

static void
copy_job_to_job_info (EvJobRender   *job_render,
		      CacheJobInfo  *job_info,
		      EvPixbufCache *pixbuf_cache)
{

	job_info->points_set = FALSE;

	if (job_info->pixbuf) {
		g_object_unref (G_OBJECT (job_info->pixbuf));
	}
	job_info->pixbuf = g_object_ref (job_render->pixbuf);

	if (job_info->rc) {
		g_object_unref (G_OBJECT (job_info->rc));
	}
	job_info->rc = g_object_ref (job_render->rc);

	if (job_render->include_links) {
		if (job_info->link_mapping)
			ev_link_mapping_free (job_info->link_mapping);
		job_info->link_mapping = job_render->link_mapping;
	}

	if (job_render->include_text) {
		if (job_info->text_mapping)
			gdk_region_destroy (job_info->text_mapping);
		job_info->text_mapping = job_render->text_mapping;
	}
	

	if (job_render->include_selection) {

		if (job_info->selection) {
			g_object_unref (G_OBJECT (job_info->selection));
			job_info->selection = NULL;
		}
		if (job_info->selection_region) {
			gdk_region_destroy (job_info->selection_region);
			job_info->selection_region = NULL;
		}
		
		job_info->selection_points = job_render->selection_points;
		job_info->selection_region = gdk_region_copy (job_render->selection_region);
		job_info->selection = g_object_ref (job_render->selection);
		g_assert (job_info->selection_points.x1 >= 0);
	}

	if (job_info->job) {
		g_signal_handlers_disconnect_by_func (job_info->job,
						      G_CALLBACK (job_finished_cb),
						      pixbuf_cache);
		ev_job_queue_remove_job (job_info->job);
		g_object_unref (G_OBJECT (job_info->job));
		job_info->job = NULL;
	}

}

static CacheJobInfo*
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
	EvPageCache *page_cache;
	int i;

	page_cache = ev_page_cache_get (pixbuf_cache->document);

	for (i = 0; i < PAGE_CACHE_LEN (pixbuf_cache); i++) {
		check_job_size_and_unref (pixbuf_cache, pixbuf_cache->job_list + i, page_cache, scale);
	}

	for (i = 0; i < pixbuf_cache->preload_cache_size; i++) {
		check_job_size_and_unref (pixbuf_cache, pixbuf_cache->prev_job + i, page_cache, scale);
		check_job_size_and_unref (pixbuf_cache, pixbuf_cache->next_job + i, page_cache, scale);
	}
}

#define FIRST_VISABLE_PREV(pixbuf_cache) \
	(MAX (0, pixbuf_cache->preload_cache_size + 1 - pixbuf_cache->start_page))

static void
get_selection_colors (GtkWidget *widget, GdkColor **text, GdkColor **base)
{
    if (GTK_WIDGET_HAS_FOCUS (widget)) {
	*text = &widget->style->text [GTK_STATE_SELECTED];
	*base = &widget->style->base [GTK_STATE_SELECTED];
    } else {
	*text = &widget->style->text [GTK_STATE_ACTIVE];
	*base = &widget->style->base [GTK_STATE_ACTIVE];
    }
}

static void
add_job_if_needed (EvPixbufCache *pixbuf_cache,
		   CacheJobInfo  *job_info,
		   EvPageCache   *page_cache,
		   gint           page,
		   gint           rotation,
		   gfloat         scale,
		   EvJobPriority  priority)
{
	gboolean include_links = FALSE;
	gboolean include_text = FALSE;
	gboolean include_selection = FALSE;
	int width, height;
	GdkColor *text, *base;

	if (job_info->job)
		return;

	ev_page_cache_get_size (page_cache, page, rotation,
				scale, &width, &height);

	if (job_info->pixbuf &&
	    gdk_pixbuf_get_width (job_info->pixbuf) == width &&
	    gdk_pixbuf_get_height (job_info->pixbuf) == height)
		return;

	/* make a new job now */
	if (job_info->rc == NULL) {
		job_info->rc = ev_render_context_new (rotation, page, scale);
	} else {
		ev_render_context_set_rotation (job_info->rc, rotation);
		ev_render_context_set_page (job_info->rc, page);
		ev_render_context_set_scale (job_info->rc, scale);
	}

	/* Figure out what else we need for this job */
	if (job_info->link_mapping == NULL)
		include_links = TRUE;
	if (job_info->text_mapping == NULL)
		include_text = TRUE;
	if (new_selection_pixbuf_needed (pixbuf_cache, job_info, page, scale)) {
		include_selection = TRUE;
	}

	gtk_widget_ensure_style (pixbuf_cache->view);

	get_selection_colors (pixbuf_cache->view, &text, &base);

	job_info->job = ev_job_render_new (pixbuf_cache->document,
					   job_info->rc,
					   width, height,
					   &(job_info->target_points),
					   text, base,
					   include_links,
					   include_text,
					   include_selection);
	ev_job_queue_add_job (job_info->job, priority);
	g_signal_connect (job_info->job, "finished", G_CALLBACK (job_finished_cb), pixbuf_cache);
}


static void
ev_pixbuf_cache_add_jobs_if_needed (EvPixbufCache *pixbuf_cache,
				    gint           rotation,
				    gfloat         scale)
{
	EvPageCache *page_cache;
	CacheJobInfo *job_info;
	int page;
	int i;

	page_cache = ev_page_cache_get (pixbuf_cache->document);

	for (i = 0; i < PAGE_CACHE_LEN (pixbuf_cache); i++) {
		job_info = (pixbuf_cache->job_list + i);
		page = pixbuf_cache->start_page + i;

		add_job_if_needed (pixbuf_cache, job_info,
				   page_cache, page, rotation, scale,
				   EV_JOB_PRIORITY_HIGH);
	}

	for (i = FIRST_VISABLE_PREV(pixbuf_cache); i < pixbuf_cache->preload_cache_size; i++) {
		job_info = (pixbuf_cache->prev_job + i);
		page = pixbuf_cache->start_page - pixbuf_cache->preload_cache_size + i;

		add_job_if_needed (pixbuf_cache, job_info,
				   page_cache, page, rotation, scale,
				   EV_JOB_PRIORITY_LOW);
	}

	for (i = 0; i < VISIBLE_NEXT_LEN(pixbuf_cache, page_cache); i++) {
		job_info = (pixbuf_cache->next_job + i);
		page = pixbuf_cache->end_page + 1 + i;

		add_job_if_needed (pixbuf_cache, job_info,
				   page_cache, page, rotation, scale,
				   EV_JOB_PRIORITY_LOW);
	}

}

void
ev_pixbuf_cache_set_page_range (EvPixbufCache  *pixbuf_cache,
				gint            start_page,
				gint            end_page,
				gint            rotation,
				gfloat          scale,
				GList          *selection_list)
{
	EvPageCache *page_cache;

	g_return_if_fail (EV_IS_PIXBUF_CACHE (pixbuf_cache));

	page_cache = ev_page_cache_get (pixbuf_cache->document);

	g_return_if_fail (start_page >= 0 && start_page < ev_page_cache_get_n_pages (page_cache));
	g_return_if_fail (end_page >= 0 && end_page < ev_page_cache_get_n_pages (page_cache));
	g_return_if_fail (end_page >= start_page);

	/* First, resize the page_range as needed.  We cull old pages
	 * mercilessly. */
	ev_pixbuf_cache_update_range (pixbuf_cache, start_page, end_page);

	/* Then, we update the current jobs to see if any of them are the wrong
	 * size, we remove them if we need to. */
	ev_pixbuf_cache_clear_job_sizes (pixbuf_cache, scale);

	/* Next, we update the target selection for our pages */
	ev_pixbuf_cache_set_selection_list (pixbuf_cache, selection_list);

	/* Finally, we add the new jobs for all the sizes that don't have a
	 * pixbuf */
	ev_pixbuf_cache_add_jobs_if_needed (pixbuf_cache, rotation, scale);
}

GdkPixbuf *
ev_pixbuf_cache_get_pixbuf (EvPixbufCache *pixbuf_cache,
			    gint           page)
{
	CacheJobInfo *job_info;

	job_info = find_job_cache (pixbuf_cache, page);
	if (job_info == NULL)
		return NULL;

	/* We don't need to wait for the idle to handle the callback */
	if (job_info->job &&
	    EV_JOB (job_info->job)->finished) {
		copy_job_to_job_info (EV_JOB_RENDER (job_info->job), job_info, pixbuf_cache);
	}

	return job_info->pixbuf;
}

GList *
ev_pixbuf_cache_get_link_mapping (EvPixbufCache *pixbuf_cache,
				  gint           page)
{
	CacheJobInfo *job_info;

	job_info = find_job_cache (pixbuf_cache, page);
	if (job_info == NULL)
		return NULL;

	/* We don't need to wait for the idle to handle the callback */
	if (job_info->job &&
	    EV_JOB (job_info->job)->finished) {
		copy_job_to_job_info (EV_JOB_RENDER (job_info->job), job_info, pixbuf_cache);
	}
	
	return job_info->link_mapping;
}

static gboolean
new_selection_pixbuf_needed (EvPixbufCache *pixbuf_cache,
			     CacheJobInfo  *job_info,
			     gint           page,
			     gfloat         scale)
{
	EvPageCache *page_cache;
	gint width, height;

	if (job_info->selection) {
		page_cache = ev_page_cache_get (pixbuf_cache->document);
		ev_page_cache_get_size (page_cache, page, job_info->rc->rotation,
					scale, &width, &height);
		
		if (width != gdk_pixbuf_get_width (job_info->selection) ||
		    height != gdk_pixbuf_get_height (job_info->selection))
			return TRUE;
	} else {
		if (job_info->points_set)
			return TRUE;
	}
	return FALSE;
}

static void
clear_selection_if_needed (EvPixbufCache *pixbuf_cache,
			   CacheJobInfo  *job_info,
			   gint           page,
			   gfloat         scale)
{
	if (new_selection_pixbuf_needed (pixbuf_cache, job_info, page, scale)) {
		if (job_info->selection)
			g_object_unref (job_info->selection);
		job_info->selection = NULL;
		job_info->selection_points.x1 = -1;
	}
}

GdkRegion *
ev_pixbuf_cache_get_text_mapping      (EvPixbufCache *pixbuf_cache,
				       gint           page)
{
	CacheJobInfo *job_info;

	job_info = find_job_cache (pixbuf_cache, page);
	if (job_info == NULL)
		return NULL;

	/* We don't need to wait for the idle to handle the callback */
	if (job_info->job &&
	    EV_JOB (job_info->job)->finished) {
		copy_job_to_job_info (EV_JOB_RENDER (job_info->job), job_info, pixbuf_cache);
	}
	
	return job_info->text_mapping;
}

/* Clears the cache of jobs and pixbufs.
 */
void
ev_pixbuf_cache_clear (EvPixbufCache *pixbuf_cache)
{
	int i;

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

	/* FIXME: doesn't update running jobs. */
	for (i = 0; i < pixbuf_cache->preload_cache_size; i++) {
		CacheJobInfo *job_info;

		job_info = pixbuf_cache->prev_job + i;
		if (job_info->selection) {
			g_object_unref (G_OBJECT (job_info->selection));
			job_info->selection = NULL;
		}

		job_info = pixbuf_cache->next_job + i;
		if (job_info->selection) {
			g_object_unref (G_OBJECT (job_info->selection));
			job_info->selection = NULL;
		}
	}

	for (i = 0; i < PAGE_CACHE_LEN (pixbuf_cache); i++) {
		CacheJobInfo *job_info;

		job_info = pixbuf_cache->job_list + i;
		if (job_info->selection) {
			g_object_unref (G_OBJECT (job_info->selection));
			job_info->selection = NULL;
		}
	}
}

GdkPixbuf *
ev_pixbuf_cache_get_selection_pixbuf (EvPixbufCache  *pixbuf_cache,
				      gint            page,
				      gfloat          scale,
				      GdkRegion     **region)
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

	/* Update the rc */
	g_assert (job_info->rc);
	ev_render_context_set_scale (job_info->rc, scale);

	/* If we have a running job, we just return what we have under the
	 * assumption that it'll be updated later and we can scale it as need
	 * be */
	if (job_info->job && EV_JOB_RENDER (job_info->job)->include_selection)
		return job_info->selection;

	/* Now, lets see if we need to resize the image.  If we do, we clear the
	 * old one. */
	clear_selection_if_needed (pixbuf_cache, job_info, page, scale);

	/* Finally, we see if the two scales are the same, and get a new pixbuf
	 * if needed.  We do this synchronously for now.  At some point, we
	 * _should_ be able to get rid of the doc_mutex, so the synchronicity
	 * doesn't kill us.  Rendering a few glyphs should really be fast.
	 */
	if (ev_rect_cmp (&(job_info->target_points), &(job_info->selection_points))) {
		EvRectangle *old_points;
		GdkColor *text, *base;

		/* we need to get a new selection pixbuf */
		ev_document_doc_mutex_lock ();
		if (job_info->selection_points.x1 < 0) {
			g_assert (job_info->selection == NULL);
			old_points = NULL;
		} else {
			g_assert (job_info->selection != NULL);
			old_points = &(job_info->selection_points);
		}

		if (job_info->selection_region)
			gdk_region_destroy (job_info->selection_region);
		job_info->selection_region =
			ev_selection_get_selection_region (EV_SELECTION (pixbuf_cache->document),
							   job_info->rc,
							   &(job_info->target_points));

		gtk_widget_ensure_style (pixbuf_cache->view);

		get_selection_colors (pixbuf_cache->view, &text, &base);

		ev_selection_render_selection (EV_SELECTION (pixbuf_cache->document),
					       job_info->rc, &(job_info->selection),
					       &(job_info->target_points),
					       old_points,
					       text, base);
		job_info->selection_points = job_info->target_points;
		ev_document_doc_mutex_unlock ();
	}
	if (region)
		*region = job_info->selection_region;
	return job_info->selection;
}

static void
update_job_selection (CacheJobInfo    *job_info,
		      EvViewSelection *selection)
{
	job_info->points_set = TRUE;		
	job_info->target_points = selection->rect;
}

static void
clear_job_selection (CacheJobInfo *job_info)
{
	job_info->points_set = FALSE;
	job_info->selection_points.x1 = -1;

	if (job_info->selection) {
		g_object_unref (job_info->selection);
		job_info->selection = NULL;
	}
}

/* This function will reset the selection on pages that no longer have them, and
 * will update the target_selection on those that need it.  It will _not_ free
 * the previous selection_list -- that's up to caller to do.
 */
void
ev_pixbuf_cache_set_selection_list (EvPixbufCache *pixbuf_cache,
				    GList         *selection_list)
{
	EvPageCache *page_cache;
	EvViewSelection *selection;
	GList *list = selection_list;
	int page;
	int i;

	g_return_if_fail (EV_IS_PIXBUF_CACHE (pixbuf_cache));

	if (!EV_IS_SELECTION (pixbuf_cache->document))
		return;

	page_cache = ev_page_cache_get (pixbuf_cache->document);

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
		if (page >= ev_page_cache_get_n_pages (page_cache))
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
	EvPageCache *page_cache;
	EvViewSelection *selection;
	GList *retval = NULL;
	int page;
	int i;

	g_return_val_if_fail (EV_IS_PIXBUF_CACHE (pixbuf_cache), NULL);

	page_cache = ev_page_cache_get (pixbuf_cache->document);

	/* We check each area to see what needs updating, and what needs freeing; */
	page = pixbuf_cache->start_page - pixbuf_cache->preload_cache_size;
	for (i = 0; i < pixbuf_cache->preload_cache_size; i++) {
		if (page < 0) {
			page ++;
			continue;
		}

		if (pixbuf_cache->prev_job[i].selection_points.x1 != -1) {
			selection = g_new0 (EvViewSelection, 1);
			selection->page = page;
			selection->rect = pixbuf_cache->prev_job[i].selection_points;
			if (pixbuf_cache->prev_job[i].selection_region)
				selection->covered_region = gdk_region_copy (pixbuf_cache->prev_job[i].selection_region);
			retval = g_list_append (retval, selection);
		}
		
		page ++;
	}

	page = pixbuf_cache->start_page;
	for (i = 0; i < PAGE_CACHE_LEN (pixbuf_cache); i++) {
		if (pixbuf_cache->job_list[i].selection_points.x1 != -1) {
			selection = g_new0 (EvViewSelection, 1);
			selection->page = page;
			selection->rect = pixbuf_cache->job_list[i].selection_points;
			if (pixbuf_cache->job_list[i].selection_region)
				selection->covered_region = gdk_region_copy (pixbuf_cache->job_list[i].selection_region);
			retval = g_list_append (retval, selection);
		}
		
		page ++;
	}

	for (i = 0; i < pixbuf_cache->preload_cache_size; i++) {
		if (page >= ev_page_cache_get_n_pages (page_cache))
			break;

		if (pixbuf_cache->next_job[i].selection_points.x1 != -1) {
			selection = g_new0 (EvViewSelection, 1);
			selection->page = page;
			selection->rect = pixbuf_cache->next_job[i].selection_points;
			if (pixbuf_cache->next_job[i].selection_region)
				selection->covered_region = gdk_region_copy (pixbuf_cache->next_job[i].selection_region);
			retval = g_list_append (retval, selection);
		}
		
		page ++;
	}

	return retval;
}

