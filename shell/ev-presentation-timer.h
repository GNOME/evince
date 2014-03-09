/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* this file is part of evince, a gnome document viewer
 *
 *
 *  Author:
 *    Lukas Bezdicka <255993@mail.muni.cz>
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

#ifndef _EV_PRESENTATION_TIMER_H_
#define _EV_PRESENTATION_TIMER_H_

G_BEGIN_DECLS

typedef struct _EvPresentationTimerClass	EvPresentationTimerClass;
typedef struct _EvPresentationTimer		EvPresentationTimer;
typedef struct _EvPresentationTimerPrivate      EvPresentationTimerPrivate;

#define EV_TYPE_PRESENTATION_TIMER     		(ev_presentation_timer_get_type ())
#define EV_PRESENTATION_TIMER(object)  		(G_TYPE_CHECK_INSTANCE_CAST ((object), EV_TYPE_PRESENTATION_TIMER, EvPresentationTimer))
#define EV_PRESENTATION_TIMER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), EV_TYPE_PRESENTATION_TIMER, EvPresentationTimerClass))
#define EV_IS_PRESENTATION_TIMER(object)	(G_TYPE_CHECK_INSTANCE_TYPE ((object), EV_TYPE_PRESENTATION_TIMER))
#define EV_IS_PRESENTATION_TIMER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), EV_TYPE_PRESENTATION_TIMER))
#define EV_PRESENTATION_TIMER_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS ((object), EV_TYPE_PRESENTATION_TIMER, EvPresentationTimerClass))

struct _EvPresentationTimerClass
{
	GtkDrawingAreaClass parent_class;
};

struct _EvPresentationTimer
{
	GtkDrawingArea			parent_instance;
	EvPresentationTimerPrivate     *priv;
};

void			ev_presentation_timer_set_pages		(EvPresentationTimer *ev_timer,
								 guint pages);
void			ev_presentation_timer_set_page		(EvPresentationTimer *ev_timer,
								 guint page);
void			ev_presentation_timer_start		(EvPresentationTimer *ev_timer);
void			ev_presentation_timer_stop		(EvPresentationTimer *ev_timer);
void			ev_presentation_timer_set_time		(EvPresentationTimer *ev_timer,
								 gint time);
GType			ev_presentation_timer_get_type		(void);
GtkWidget	       *ev_presentation_timer_new		(void);

G_END_DECLS

#endif /* _EV_PRESENTATION_TIMER_H_ */
