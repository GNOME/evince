/* ev-timeline.c
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

#include <glib.h>
#include <math.h>
#include <gdk/gdk.h>
#include "ev-timeline.h"

#define EV_TIMELINE_GET_PRIV(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), EV_TYPE_TIMELINE, EvTimelinePriv))
#define MSECS_PER_SEC 1000
#define FRAME_INTERVAL(nframes) (MSECS_PER_SEC / nframes)
#define DEFAULT_FPS 30

typedef struct EvTimelinePriv EvTimelinePriv;

struct EvTimelinePriv {
	guint duration;
	guint fps;
	guint source_id;

	GTimer *timer;

	guint loop : 1;
};

enum {
	PROP_0,
	PROP_FPS,
	PROP_DURATION,
	PROP_LOOP
};

enum {
	STARTED,
	PAUSED,
	FINISHED,
	FRAME,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0, };


G_DEFINE_TYPE (EvTimeline, ev_timeline, G_TYPE_OBJECT)


static void
ev_timeline_init (EvTimeline *timeline)
{
	EvTimelinePriv *priv;

	priv = EV_TIMELINE_GET_PRIV (timeline);

	priv->fps = DEFAULT_FPS;
	priv->duration = 0;
}

static void
ev_timeline_set_property (GObject      *object,
			  guint         prop_id,
			  const GValue *value,
			  GParamSpec   *pspec)
{
	EvTimeline *timeline;

	timeline = EV_TIMELINE (object);

	switch (prop_id) {
	case PROP_FPS:
		ev_timeline_set_fps (timeline, g_value_get_uint (value));
		break;
	case PROP_DURATION:
		ev_timeline_set_duration (timeline, g_value_get_uint (value));
		break;
	case PROP_LOOP:
		ev_timeline_set_loop (timeline, g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
ev_timeline_get_property (GObject    *object,
			  guint       prop_id,
			  GValue     *value,
			  GParamSpec *pspec)
{
	EvTimeline     *timeline;
	EvTimelinePriv *priv;

	timeline = EV_TIMELINE (object);
	priv = EV_TIMELINE_GET_PRIV (timeline);

	switch (prop_id) {
	case PROP_FPS:
		g_value_set_uint (value, priv->fps);
		break;
	case PROP_DURATION:
		g_value_set_uint (value, priv->duration);
		break;
	case PROP_LOOP:
		g_value_set_boolean (value, priv->loop);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
ev_timeline_finalize (GObject *object)
{
	EvTimelinePriv *priv;

	priv = EV_TIMELINE_GET_PRIV (object);

	if (priv->source_id) {
		g_source_remove (priv->source_id);
		priv->source_id = 0;
	}

	if (priv->timer)
		g_timer_destroy (priv->timer);

	G_OBJECT_CLASS (ev_timeline_parent_class)->finalize (object);
}

static gboolean
ev_timeline_run_frame (EvTimeline *timeline)
{
	EvTimelinePriv *priv;
	gdouble         progress;
	guint           elapsed_time;

	gdk_threads_enter ();

	priv = EV_TIMELINE_GET_PRIV (timeline);

	elapsed_time = (guint) (g_timer_elapsed (priv->timer, NULL) * 1000);
	progress = (gdouble) elapsed_time / priv->duration;
	progress = CLAMP (progress, 0., 1.);

	g_signal_emit (timeline, signals [FRAME], 0, progress);

	if (progress >= 1.0) {
		if (!priv->loop) {
			if (priv->source_id) {
				g_source_remove (priv->source_id);
				priv->source_id = 0;
			}

			g_signal_emit (timeline, signals [FINISHED], 0);
			return FALSE;
		} else {
			ev_timeline_rewind (timeline);
		}
	}

	gdk_threads_leave ();

	return TRUE;
}

static void
ev_timeline_real_start (EvTimeline *timeline)
{
	EvTimelinePriv *priv;

	priv = EV_TIMELINE_GET_PRIV (timeline);

	if (!priv->source_id) {
		if (priv->timer)
			g_timer_continue (priv->timer);
		else
			priv->timer = g_timer_new ();

		/* sanity check */
		g_assert (priv->fps > 0);

		g_signal_emit (timeline, signals [STARTED], 0);

		priv->source_id = g_timeout_add (FRAME_INTERVAL (priv->fps),
						 (GSourceFunc) ev_timeline_run_frame,
						 timeline);
	}
}

static void
ev_timeline_class_init (EvTimelineClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->set_property = ev_timeline_set_property;
	object_class->get_property = ev_timeline_get_property;
	object_class->finalize = ev_timeline_finalize;

	class->start = ev_timeline_real_start;

	g_object_class_install_property (object_class,
					 PROP_FPS,
					 g_param_spec_uint ("fps",
							    "FPS",
							    "Frames per second for the timeline",
							    1,
							    G_MAXUINT,
							    DEFAULT_FPS,
							    G_PARAM_READWRITE |
                                                            G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (object_class,
					 PROP_DURATION,
					 g_param_spec_uint ("duration",
							    "Animation Duration",
							    "Animation Duration",
							    0,
							    G_MAXUINT,
							    0,
							    G_PARAM_READWRITE |
                                                            G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (object_class,
					 PROP_LOOP,
					 g_param_spec_boolean ("loop",
							       "Loop",
							       "Whether the timeline loops or not",
							       FALSE,
							       G_PARAM_READWRITE |
                                                               G_PARAM_STATIC_STRINGS));
	signals[STARTED] =
		g_signal_new ("started",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EvTimelineClass, started),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals[PAUSED] =
		g_signal_new ("paused",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EvTimelineClass, paused),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals[FINISHED] =
		g_signal_new ("finished",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EvTimelineClass, finished),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals[FRAME] =
		g_signal_new ("frame",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EvTimelineClass, frame),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__DOUBLE,
			      G_TYPE_NONE, 1,
			      G_TYPE_DOUBLE);

	g_type_class_add_private (class, sizeof (EvTimelinePriv));
}

EvTimeline *
ev_timeline_new (guint duration)
{
	return g_object_new (EV_TYPE_TIMELINE,
			     "duration", duration,
			     NULL);
}

void
ev_timeline_start (EvTimeline *timeline)
{
	g_return_if_fail (EV_IS_TIMELINE (timeline));

	EV_TIMELINE_GET_CLASS (timeline)->start (timeline);
}

void
ev_timeline_pause (EvTimeline *timeline)
{
	EvTimelinePriv *priv;

	g_return_if_fail (EV_IS_TIMELINE (timeline));

	priv = EV_TIMELINE_GET_PRIV (timeline);

	if (priv->source_id) {
		g_source_remove (priv->source_id);
		priv->source_id = 0;
		g_timer_stop (priv->timer);
		g_signal_emit (timeline, signals [PAUSED], 0);
	}
}

void
ev_timeline_rewind (EvTimeline *timeline)
{
	EvTimelinePriv *priv;

	g_return_if_fail (EV_IS_TIMELINE (timeline));

	priv = EV_TIMELINE_GET_PRIV (timeline);

	/* destroy and re-create timer if neccesary  */
	if (priv->timer) {
		g_timer_destroy (priv->timer);

		if (ev_timeline_is_running (timeline))
			priv->timer = g_timer_new ();
		else
			priv->timer = NULL;
	}
}

gboolean
ev_timeline_is_running (EvTimeline *timeline)
{
	EvTimelinePriv *priv;

	g_return_val_if_fail (EV_IS_TIMELINE (timeline), FALSE);

	priv = EV_TIMELINE_GET_PRIV (timeline);

	return (priv->source_id != 0);
}

guint
ev_timeline_get_fps (EvTimeline *timeline)
{
	EvTimelinePriv *priv;

	g_return_val_if_fail (EV_IS_TIMELINE (timeline), 1);

	priv = EV_TIMELINE_GET_PRIV (timeline);
	return priv->fps;
}

void
ev_timeline_set_fps (EvTimeline *timeline,
		     guint       fps)
{
	EvTimelinePriv *priv;

	g_return_if_fail (EV_IS_TIMELINE (timeline));

	priv = EV_TIMELINE_GET_PRIV (timeline);

	priv->fps = fps;

	if (ev_timeline_is_running (timeline)) {
		g_source_remove (priv->source_id);
		priv->source_id = g_timeout_add (FRAME_INTERVAL (priv->fps),
						 (GSourceFunc) ev_timeline_run_frame,
						 timeline);
	}

	g_object_notify (G_OBJECT (timeline), "fps");
}

gboolean
ev_timeline_get_loop (EvTimeline *timeline)
{
	EvTimelinePriv *priv;

	g_return_val_if_fail (EV_IS_TIMELINE (timeline), FALSE);

	priv = EV_TIMELINE_GET_PRIV (timeline);
	return priv->loop;
}

void
ev_timeline_set_loop (EvTimeline *timeline,
		      gboolean    loop)
{
	EvTimelinePriv *priv;

	g_return_if_fail (EV_IS_TIMELINE (timeline));

	priv = EV_TIMELINE_GET_PRIV (timeline);
	priv->loop = loop;

	g_object_notify (G_OBJECT (timeline), "loop");
}

void
ev_timeline_set_duration (EvTimeline *timeline,
			  guint       duration)
{
	EvTimelinePriv *priv;

	g_return_if_fail (EV_IS_TIMELINE (timeline));

	priv = EV_TIMELINE_GET_PRIV (timeline);

	priv->duration = duration;

	g_object_notify (G_OBJECT (timeline), "duration");
}

guint
ev_timeline_get_duration (EvTimeline *timeline)
{
	EvTimelinePriv *priv;

	g_return_val_if_fail (EV_IS_TIMELINE (timeline), 0);

	priv = EV_TIMELINE_GET_PRIV (timeline);

	return priv->duration;
}

gdouble
ev_timeline_get_progress (EvTimeline *timeline)
{
	EvTimelinePriv *priv;
	gdouble         progress;
	guint           elapsed_time;

	g_return_val_if_fail (EV_IS_TIMELINE (timeline), 0.0);

	priv = EV_TIMELINE_GET_PRIV (timeline);

	if (!priv->timer)
		return 0.;

	elapsed_time = (guint) (g_timer_elapsed (priv->timer, NULL) * 1000);
	progress = (gdouble) elapsed_time / priv->duration;

	return CLAMP (progress, 0., 1.);
}
