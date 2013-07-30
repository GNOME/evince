/* ev-timeline.h
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2007 Carlos Garnacho <carlos@imendio.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 * Boston, MA 02110-1301, USA.
 */

#if !defined (EVINCE_COMPILATION)
#error "This is a private header."
#endif

#ifndef __EV_TIMELINE_H__
#define __EV_TIMELINE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define EV_TYPE_TIMELINE                 (ev_timeline_get_type ())
#define EV_TIMELINE(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), EV_TYPE_TIMELINE, EvTimeline))
#define EV_TIMELINE_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass),  EV_TYPE_TIMELINE, EvTimelineClass))
#define EV_IS_TIMELINE(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EV_TYPE_TIMELINE))
#define EV_IS_TIMELINE_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass),  EV_TYPE_TIMELINE))
#define EV_TIMELINE_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj),  EV_TYPE_TIMELINE, EvTimelineClass))

typedef struct _EvTimeline      EvTimeline;
typedef struct _EvTimelineClass EvTimelineClass;

struct _EvTimeline {
	GObject parent_instance;
};

struct _EvTimelineClass {
	GObjectClass parent_class;

	/* vmethods */
	void (* start)             (EvTimeline *timeline);

	/* signals */
	void (* started)           (EvTimeline *timeline);
	void (* finished)          (EvTimeline *timeline);
	void (* paused)            (EvTimeline *timeline);

	void (* frame)             (EvTimeline *timeline,
				    gdouble     progress);
};


GType                 ev_timeline_get_type           (void) G_GNUC_CONST;

EvTimeline           *ev_timeline_new                (guint                    duration);

void                  ev_timeline_start              (EvTimeline             *timeline);
void                  ev_timeline_pause              (EvTimeline             *timeline);
void                  ev_timeline_rewind             (EvTimeline             *timeline);

gboolean              ev_timeline_is_running         (EvTimeline             *timeline);

guint                 ev_timeline_get_fps            (EvTimeline             *timeline);
void                  ev_timeline_set_fps            (EvTimeline             *timeline,
						      guint                   fps);

gboolean              ev_timeline_get_loop           (EvTimeline             *timeline);
void                  ev_timeline_set_loop           (EvTimeline             *timeline,
						      gboolean                loop);

guint                 ev_timeline_get_duration       (EvTimeline             *timeline);
void                  ev_timeline_set_duration       (EvTimeline             *timeline,
						      guint                   duration);

gdouble               ev_timeline_get_progress       (EvTimeline             *timeline);


G_END_DECLS

#endif /* __EV_TIMELINE_H__ */
