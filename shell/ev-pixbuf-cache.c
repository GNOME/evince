#include "ev-pixbuf-cache.h"
#include "ev-job-queue.h"


typedef struct _CacheJobInfo
{
	EvJob *job;
	GdkPixbuf *pixbuf;
	GList *link_mapping;
} CacheJobInfo;

struct _EvPixbufCache
{
	GObject parent;

	EvDocument  *document;
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

	pixbuf_cache->preload_cache_size = 1;
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
ev_pixbuf_cache_new (EvDocument *document)
{
	EvPixbufCache *pixbuf_cache;

	pixbuf_cache = (EvPixbufCache *) g_object_new (EV_TYPE_PIXBUF_CACHE, NULL);
	pixbuf_cache->document = document;

	return pixbuf_cache;
}

static void
job_finished_cb (EvJob         *job,
		 EvPixbufCache *pixbuf_cache)
{
	CacheJobInfo *job_info;
	EvJobRender *job_render = EV_JOB_RENDER (job);
	GdkPixbuf *pixbuf;

	/* If the job is outside of our interest, we silently discard it */
	if ((job_render->page < (pixbuf_cache->start_page - pixbuf_cache->preload_cache_size)) ||
	    (job_render->page > (pixbuf_cache->end_page + pixbuf_cache->preload_cache_size))) {
		g_object_unref (job);
		return;
	}
	
	job_info = find_job_cache (pixbuf_cache, job_render->page);

	pixbuf = g_object_ref (job_render->pixbuf);
	if (job_info->pixbuf)
		g_object_unref (job_info->pixbuf);
	job_info->pixbuf = pixbuf;

	if (job_render->link_mapping) {
		if (job_info->link_mapping)
			ev_link_mapping_free (job_info->link_mapping);
		job_info->link_mapping = job_render->link_mapping;
	}
	
	if (job_info->job == job)
		job_info->job = NULL;
	g_object_unref (job);

	g_signal_emit (pixbuf_cache, signals[JOB_FINISHED], 0);
}

/* This checks a job to see if the job would generate the right sized pixbuf
 * given a scale.  If it won't, it removes the job and clears it to NULL.
 */
static void
check_job_size_and_unref (CacheJobInfo *job_info,
			  EvPageCache  *page_cache,
			  gfloat        scale)
{
	gint width;
	gint height;

	g_assert (job_info);

	if (job_info->job == NULL)
		return;

	ev_page_cache_get_size (page_cache,
				EV_JOB_RENDER (job_info->job)->page,
				scale,
				&width, &height);
				
	if (width == EV_JOB_RENDER (job_info->job)->target_width &&
	    height == EV_JOB_RENDER (job_info->job)->target_height)
		return;

	/* Try to remove the job.  If we can't, then the thread has already
	 * picked it up and we are going get a signal when it's done.  If we
	 * can, then the job is fully dead and will never rnu.. */
	if (ev_job_queue_remove_job (job_info->job))
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
		g_print ("FIXME: update priority \n");
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

	page_cache = ev_document_get_page_cache (pixbuf_cache->document);

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
		page++;
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
	GdkPixbuf *pixbuf;

	pixbuf = g_object_ref (job_render->pixbuf);

	dispose_cache_job_info (job_info, pixbuf_cache);

	job_info->pixbuf = pixbuf;
	if (job_render->link_mapping)
		job_info->link_mapping = job_render->link_mapping;
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
	EvPageCache *page_cache;
	int i;

	page_cache = ev_document_get_page_cache (pixbuf_cache->document);

	for (i = 0; i < PAGE_CACHE_LEN (pixbuf_cache); i++) {
		check_job_size_and_unref (pixbuf_cache->job_list + i, page_cache, scale);
	}

	for (i = 0; i < pixbuf_cache->preload_cache_size; i++) {
		check_job_size_and_unref (pixbuf_cache->prev_job + i, page_cache, scale);
		check_job_size_and_unref (pixbuf_cache->next_job + i, page_cache, scale);
	}
}

#define FIRST_VISABLE_PREV(pixbuf_cache) \
	(MAX (0, pixbuf_cache->preload_cache_size + 1 - pixbuf_cache->start_page))

static void
add_job_if_needed (EvPixbufCache *pixbuf_cache,
		   CacheJobInfo  *job_info,
		   EvPageCache   *page_cache,
		   gint           page,
		   gfloat         scale,
		   EvJobPriority  priority)
{
	int width, height;

	if (job_info->job)
		return;

	ev_page_cache_get_size (page_cache,
				page, scale,
				&width, &height);

	if (job_info->pixbuf &&
	    gdk_pixbuf_get_width (job_info->pixbuf) == width &&
	    gdk_pixbuf_get_height (job_info->pixbuf) == height)
		return;

	/* make a new job now */
	job_info->job = ev_job_render_new (pixbuf_cache->document,
					   page, scale,
					   width, height,
					   (job_info->link_mapping == NULL)?TRUE:FALSE);
	ev_job_queue_add_job (job_info->job, priority);
	g_signal_connect (job_info->job, "finished", G_CALLBACK (job_finished_cb), pixbuf_cache);
}


static void
ev_pixbuf_cache_add_jobs_if_needed (EvPixbufCache *pixbuf_cache,
				    gfloat         scale)
{
	EvPageCache *page_cache;
	CacheJobInfo *job_info;
	int page;
	int i;

	page_cache = ev_document_get_page_cache (pixbuf_cache->document);

	for (i = 0; i < PAGE_CACHE_LEN (pixbuf_cache); i++) {
		job_info = (pixbuf_cache->job_list + i);
		page = pixbuf_cache->start_page + i;

		add_job_if_needed (pixbuf_cache, job_info,
				   page_cache, page, scale,
				   EV_JOB_PRIORITY_HIGH);
	}

	for (i = FIRST_VISABLE_PREV(pixbuf_cache); i < pixbuf_cache->preload_cache_size; i++) {
		job_info = (pixbuf_cache->prev_job + i);
		page = pixbuf_cache->start_page - pixbuf_cache->preload_cache_size + i;

		add_job_if_needed (pixbuf_cache, job_info,
				   page_cache, page, scale,
				   EV_JOB_PRIORITY_LOW);
	}

	for (i = 0; i < VISIBLE_NEXT_LEN(pixbuf_cache, page_cache); i++) {
		job_info = (pixbuf_cache->next_job + i);
		page = pixbuf_cache->end_page + 1 + i;

		add_job_if_needed (pixbuf_cache, job_info,
				   page_cache, page, scale,
				   EV_JOB_PRIORITY_LOW);
	}

}

void
ev_pixbuf_cache_set_page_range (EvPixbufCache *pixbuf_cache,
				gint           start_page,
				gint           end_page,
				gfloat         scale)
{
	EvPageCache *page_cache;

	g_return_if_fail (EV_IS_PIXBUF_CACHE (pixbuf_cache));

	page_cache = ev_document_get_page_cache (pixbuf_cache->document);

	g_return_if_fail (start_page >= 0 && start_page < ev_page_cache_get_n_pages (page_cache));
	g_return_if_fail (end_page >= 0 && end_page < ev_page_cache_get_n_pages (page_cache));
	g_return_if_fail (end_page >= start_page);

	/* First, resize the page_range as needed.  We cull old pages
	 * mercilessly. */
	ev_pixbuf_cache_update_range (pixbuf_cache, start_page, end_page);

	/* Then, we update the current jobs to see if any of them are the wrong
	 * size, we remove them if we need to. */
	ev_pixbuf_cache_clear_job_sizes (pixbuf_cache, scale);

	/* Finally, we add the new jobs for all the sizes that don't have a
	 * pixbuf */
	ev_pixbuf_cache_add_jobs_if_needed (pixbuf_cache, scale);
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
