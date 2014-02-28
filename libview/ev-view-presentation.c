/* ev-view-presentation.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2010 Carlos Garcia Campos <carlosgc@gnome.org>
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

#include "config.h"

#include <stdlib.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>

#include "ev-view-presentation.h"
#include "ev-jobs.h"
#include "ev-job-scheduler.h"
#include "ev-transition-animation.h"
#include "ev-view-cursor.h"
#include "ev-page-cache.h"

enum {
	PROP_0,
	PROP_DOCUMENT,
	PROP_CURRENT_PAGE,
	PROP_ROTATION,
	PROP_INVERTED_COLORS
};

enum {
	CHANGE_PAGE,
	FINISHED,
        SIGNAL_EXTERNAL_LINK,
	N_SIGNALS
};

typedef enum {
	EV_PRESENTATION_NORMAL,
	EV_PRESENTATION_BLACK,
	EV_PRESENTATION_WHITE,
	EV_PRESENTATION_END
} EvPresentationState;

struct _EvViewPresentation
{
	GtkWidget base;

        guint                  is_constructing : 1;

	guint                  current_page;
	cairo_surface_t       *current_surface;
	EvDocument            *document;
	guint                  rotation;
	gboolean               inverted_colors;
	EvPresentationState    state;
	gint                   monitor_width;
	gint                   monitor_height;

	/* Cursors */
	EvViewCursor           cursor;
	guint                  hide_cursor_timeout_id;

	/* Goto Window */
	GtkWidget             *goto_window;
	GtkWidget             *goto_entry;

	/* Page Transition */
	guint                  trans_timeout_id;

	/* Animations */
	gboolean               enable_animations;
	EvTransitionAnimation *animation;

	/* Links */
	EvPageCache           *page_cache;

	EvJob *prev_job;
	EvJob *curr_job;
	EvJob *next_job;
};

struct _EvViewPresentationClass
{
	GtkWidgetClass base_class;

	/* signals */
	void (* change_page)   (EvViewPresentation *pview,
                                GtkScrollType       scroll);
	void (* finished)      (EvViewPresentation *pview);
        void (* external_link) (EvViewPresentation *pview,
                                EvLinkAction       *action);
};

static guint signals[N_SIGNALS] = { 0 };

static void ev_view_presentation_set_cursor_for_location (EvViewPresentation *pview,
							  gdouble             x,
							  gdouble             y);

#define HIDE_CURSOR_TIMEOUT 5

G_DEFINE_TYPE (EvViewPresentation, ev_view_presentation, GTK_TYPE_WIDGET)

static GdkRGBA black = { 0., 0., 0., 1. };
static GdkRGBA white = { 1., 1., 1., 1. };

static void
ev_view_presentation_set_normal (EvViewPresentation *pview)
{
	GtkWidget *widget = GTK_WIDGET (pview);

	if (pview->state == EV_PRESENTATION_NORMAL)
		return;

	pview->state = EV_PRESENTATION_NORMAL;
	gdk_window_set_background_rgba (gtk_widget_get_window (widget), &black);
	gtk_widget_queue_draw (widget);
}

static void
ev_view_presentation_set_black (EvViewPresentation *pview)
{
	GtkWidget *widget = GTK_WIDGET (pview);

	if (pview->state == EV_PRESENTATION_BLACK)
		return;

	pview->state = EV_PRESENTATION_BLACK;
	gdk_window_set_background_rgba (gtk_widget_get_window (widget), &black);
	gtk_widget_queue_draw (widget);
}

static void
ev_view_presentation_set_white (EvViewPresentation *pview)
{
	GtkWidget *widget = GTK_WIDGET (pview);

	if (pview->state == EV_PRESENTATION_WHITE)
		return;

	pview->state = EV_PRESENTATION_WHITE;
	gdk_window_set_background_rgba (gtk_widget_get_window (widget), &white);
	gtk_widget_queue_draw (widget);
}

static void
ev_view_presentation_set_end (EvViewPresentation *pview)
{
	GtkWidget *widget = GTK_WIDGET (pview);

	if (pview->state == EV_PRESENTATION_END)
		return;

	pview->state = EV_PRESENTATION_END;
	gtk_widget_queue_draw (widget);
}

static void
ev_view_presentation_get_view_size (EvViewPresentation *pview,
				    guint               page,
				    int                *view_width,
				    int                *view_height)
{
	gdouble width, height;

	ev_document_get_page_size (pview->document, page, &width, &height);
	if (pview->rotation == 90 || pview->rotation == 270) {
		gdouble tmp;

		tmp = width;
		width = height;
		height = tmp;
	}

	if (pview->monitor_width / width < pview->monitor_height / height) {
		*view_width = pview->monitor_width;
		*view_height = (int)((pview->monitor_width / width) * height + 0.5);
	} else {
		*view_width = (int)((pview->monitor_height / height) * width + 0.5);
		*view_height = pview->monitor_height;
	}
}

static void
ev_view_presentation_get_page_area (EvViewPresentation *pview,
				    GdkRectangle       *area)
{
	GtkWidget    *widget = GTK_WIDGET (pview);
	GtkAllocation allocation;
	gint          view_width, view_height;

	ev_view_presentation_get_view_size (pview, pview->current_page,
					    &view_width, &view_height);

	gtk_widget_get_allocation (widget, &allocation);

	area->x = (MAX (0, allocation.width - view_width)) / 2;
	area->y = (MAX (0, allocation.height - view_height)) / 2;
	area->width = view_width;
	area->height = view_height;
}

/* Page Transition */
static gboolean
transition_next_page (EvViewPresentation *pview)
{
	ev_view_presentation_next_page (pview);

	return FALSE;
}

static void
ev_view_presentation_transition_stop (EvViewPresentation *pview)
{
	if (pview->trans_timeout_id > 0)
		g_source_remove (pview->trans_timeout_id);
	pview->trans_timeout_id = 0;
}

static void
ev_view_presentation_transition_start (EvViewPresentation *pview)
{
	gdouble duration;

	if (!EV_IS_DOCUMENT_TRANSITION (pview->document))
		return;

	ev_view_presentation_transition_stop (pview);

	duration = ev_document_transition_get_page_duration (EV_DOCUMENT_TRANSITION (pview->document),
							     pview->current_page);
	if (duration >= 0) {
		        pview->trans_timeout_id =
				g_timeout_add_seconds (duration,
						       (GSourceFunc) transition_next_page,
						       pview);
	}
}

/* Animations */
static void
ev_view_presentation_animation_cancel (EvViewPresentation *pview)
{
	if (pview->animation) {
		g_object_unref (pview->animation);
		pview->animation = NULL;
	}
}

static void
ev_view_presentation_transition_animation_finish (EvViewPresentation *pview)
{
	ev_view_presentation_animation_cancel (pview);
	ev_view_presentation_transition_start (pview);
	gtk_widget_queue_draw (GTK_WIDGET (pview));
}

static void
ev_view_presentation_transition_animation_frame (EvViewPresentation *pview,
						 gdouble             progress)
{
	gtk_widget_queue_draw (GTK_WIDGET (pview));
}

static cairo_surface_t *
get_surface_from_job (EvViewPresentation *pview,
                      EvJob              *job)
{
        cairo_surface_t *surface;

        if (!job)
                return NULL;

        surface = EV_JOB_RENDER(job)->surface;
        if (!surface)
                return NULL;

#ifdef HAVE_HIDPI_SUPPORT
        {
                int scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (pview));
                cairo_surface_set_device_scale (surface, scale_factor, scale_factor);
        }
#endif

        return surface;
}

static void
ev_view_presentation_animation_start (EvViewPresentation *pview,
				      gint                new_page)
{
	EvTransitionEffect *effect = NULL;
	EvJob		   *job;
	cairo_surface_t    *surface;
	gint                jump;

	if (!pview->enable_animations)
		return;

	if (pview->current_page == new_page)
		return;

	effect = ev_document_transition_get_effect (EV_DOCUMENT_TRANSITION (pview->document),
						    new_page);
	if (!effect)
		return;

	pview->animation = ev_transition_animation_new (effect);

	surface = pview->curr_job ? EV_JOB_RENDER (pview->curr_job)->surface : NULL;
	ev_transition_animation_set_origin_surface (pview->animation,
						    surface != NULL ?
						    surface : pview->current_surface);

	jump = new_page - pview->current_page;
	if (jump == -1)
		job = pview->prev_job;
	else if (jump == 1)
		job = pview->next_job;
	else
		job = NULL;
	surface = get_surface_from_job (pview, job);
	if (surface)
		ev_transition_animation_set_dest_surface (pview->animation, surface);

	g_signal_connect_swapped (pview->animation, "frame",
				  G_CALLBACK (ev_view_presentation_transition_animation_frame),
				  pview);
	g_signal_connect_swapped (pview->animation, "finished",
				  G_CALLBACK (ev_view_presentation_transition_animation_finish),
				  pview);
}

/* Page Navigation */
static void
job_finished_cb (EvJob              *job,
		 EvViewPresentation *pview)
{
	EvJobRender *job_render = EV_JOB_RENDER (job);

	if (pview->inverted_colors)
		ev_document_misc_invert_surface (job_render->surface);

	if (job != pview->curr_job)
		return;

	if (pview->animation) {
		ev_transition_animation_set_dest_surface (pview->animation,
							  get_surface_from_job (pview, job));
	} else {
		ev_view_presentation_transition_start (pview);
		gtk_widget_queue_draw (GTK_WIDGET (pview));
	}
}

static EvJob *
ev_view_presentation_schedule_new_job (EvViewPresentation *pview,
				       gint                page,
				       EvJobPriority       priority)
{
	EvJob *job;
        int    view_width, view_height;

	if (page < 0 || page >= ev_document_get_n_pages (pview->document))
		return NULL;

        ev_view_presentation_get_view_size (pview, page, &view_width, &view_height);
#ifdef HAVE_HIDPI_SUPPORT
	{
		gint device_scale = gtk_widget_get_scale_factor (GTK_WIDGET (pview));
		view_width *= device_scale;
		view_height *= device_scale;
	}
#endif
        job = ev_job_render_new (pview->document, page, pview->rotation, 0.,
                                 view_width, view_height);
	g_signal_connect (job, "finished",
			  G_CALLBACK (job_finished_cb),
			  pview);
	ev_job_scheduler_push_job (job, priority);

	return job;
}

static void
ev_view_presentation_delete_job (EvViewPresentation *pview,
				 EvJob              *job)
{
	if (!job)
		return;

	g_signal_handlers_disconnect_by_func (job, job_finished_cb, pview);
	ev_job_cancel (job);
	g_object_unref (job);
}

static void
ev_view_presentation_reset_jobs (EvViewPresentation *pview)
{
        if (pview->curr_job) {
                ev_view_presentation_delete_job (pview, pview->curr_job);
                pview->curr_job = NULL;
        }

        if (pview->prev_job) {
                ev_view_presentation_delete_job (pview, pview->prev_job);
                pview->prev_job = NULL;
        }

        if (pview->next_job) {
                ev_view_presentation_delete_job (pview, pview->next_job);
                pview->next_job = NULL;
        }
}

static void
ev_view_presentation_update_current_page (EvViewPresentation *pview,
					  guint               page)
{
	gint jump;

	if (page < 0 || page >= ev_document_get_n_pages (pview->document))
		return;

	ev_view_presentation_animation_cancel (pview);
	ev_view_presentation_animation_start (pview, page);

	jump = page - pview->current_page;

	switch (jump) {
	case 0:
		if (!pview->curr_job)
			pview->curr_job = ev_view_presentation_schedule_new_job (pview, page, EV_JOB_PRIORITY_URGENT);
		if (!pview->next_job)
			pview->next_job = ev_view_presentation_schedule_new_job (pview, page + 1, EV_JOB_PRIORITY_HIGH);
		if (!pview->prev_job)
			pview->prev_job = ev_view_presentation_schedule_new_job (pview, page - 1, EV_JOB_PRIORITY_LOW);
		break;
	case -1:
		ev_view_presentation_delete_job (pview, pview->next_job);
		pview->next_job = pview->curr_job;
		pview->curr_job = pview->prev_job;

		if (!pview->curr_job)
			pview->curr_job = ev_view_presentation_schedule_new_job (pview, page, EV_JOB_PRIORITY_URGENT);
		else
			ev_job_scheduler_update_job (pview->curr_job, EV_JOB_PRIORITY_URGENT);
		pview->prev_job = ev_view_presentation_schedule_new_job (pview, page - 1, EV_JOB_PRIORITY_HIGH);
		ev_job_scheduler_update_job (pview->next_job, EV_JOB_PRIORITY_LOW);

		break;
	case 1:
		ev_view_presentation_delete_job (pview, pview->prev_job);
		pview->prev_job = pview->curr_job;
		pview->curr_job = pview->next_job;

		if (!pview->curr_job)
			pview->curr_job = ev_view_presentation_schedule_new_job (pview, page, EV_JOB_PRIORITY_URGENT);
		else
			ev_job_scheduler_update_job (pview->curr_job, EV_JOB_PRIORITY_URGENT);
		pview->next_job = ev_view_presentation_schedule_new_job (pview, page + 1, EV_JOB_PRIORITY_HIGH);
		ev_job_scheduler_update_job (pview->prev_job, EV_JOB_PRIORITY_LOW);

		break;
	case -2:
		ev_view_presentation_delete_job (pview, pview->next_job);
		ev_view_presentation_delete_job (pview, pview->curr_job);
		pview->next_job = pview->prev_job;

		pview->curr_job = ev_view_presentation_schedule_new_job (pview, page, EV_JOB_PRIORITY_URGENT);
		pview->prev_job = ev_view_presentation_schedule_new_job (pview, page - 1, EV_JOB_PRIORITY_HIGH);
		if (!pview->next_job)
			pview->next_job = ev_view_presentation_schedule_new_job (pview, page + 1, EV_JOB_PRIORITY_LOW);
		else
			ev_job_scheduler_update_job (pview->next_job, EV_JOB_PRIORITY_LOW);
		break;
	case 2:
		ev_view_presentation_delete_job (pview, pview->prev_job);
		ev_view_presentation_delete_job (pview, pview->curr_job);
		pview->prev_job = pview->next_job;

		pview->curr_job = ev_view_presentation_schedule_new_job (pview, page, EV_JOB_PRIORITY_URGENT);
		pview->next_job = ev_view_presentation_schedule_new_job (pview, page + 1, EV_JOB_PRIORITY_HIGH);
		if (!pview->prev_job)
			pview->prev_job = ev_view_presentation_schedule_new_job (pview, page - 1, EV_JOB_PRIORITY_LOW);
		else
			ev_job_scheduler_update_job (pview->prev_job, EV_JOB_PRIORITY_LOW);
		break;
	default:
		ev_view_presentation_delete_job (pview, pview->prev_job);
		ev_view_presentation_delete_job (pview, pview->curr_job);
		ev_view_presentation_delete_job (pview, pview->next_job);

		pview->curr_job = ev_view_presentation_schedule_new_job (pview, page, EV_JOB_PRIORITY_URGENT);
		if (jump > 0) {
			pview->next_job = ev_view_presentation_schedule_new_job (pview, page + 1, EV_JOB_PRIORITY_HIGH);
			pview->prev_job = ev_view_presentation_schedule_new_job (pview, page - 1, EV_JOB_PRIORITY_LOW);
		} else {
			pview->prev_job = ev_view_presentation_schedule_new_job (pview, page - 1, EV_JOB_PRIORITY_HIGH);
			pview->next_job = ev_view_presentation_schedule_new_job (pview, page + 1, EV_JOB_PRIORITY_LOW);
		}
	}

	if (pview->current_page != page) {
		pview->current_page = page;
		g_object_notify (G_OBJECT (pview), "current-page");
	}

	if (pview->page_cache)
		ev_page_cache_set_page_range (pview->page_cache, page, page);

	if (pview->cursor != EV_VIEW_CURSOR_HIDDEN) {
		gint x, y;

		ev_document_misc_get_pointer_position (GTK_WIDGET (pview), &x, &y);
		ev_view_presentation_set_cursor_for_location (pview, x, y);
	}

	if (EV_JOB_RENDER (pview->curr_job)->surface)
		gtk_widget_queue_draw (GTK_WIDGET (pview));
}

static void
ev_view_presentation_set_current_page (EvViewPresentation *pview,
                                       guint               page)
{
	if (pview->current_page == page)
		return;

	if (!gtk_widget_get_realized (GTK_WIDGET (pview))) {
		pview->current_page = page;
		g_object_notify (G_OBJECT (pview), "current-page");
	} else {
		ev_view_presentation_update_current_page (pview, page);
	}
}

void
ev_view_presentation_next_page (EvViewPresentation *pview)
{
	guint n_pages;
	gint  new_page;

	switch (pview->state) {
	case EV_PRESENTATION_BLACK:
	case EV_PRESENTATION_WHITE:
		ev_view_presentation_set_normal (pview);
	case EV_PRESENTATION_END:
		return;
	case EV_PRESENTATION_NORMAL:
		break;
	}

	n_pages = ev_document_get_n_pages (pview->document);
	new_page = pview->current_page + 1;

	if (new_page == n_pages)
		ev_view_presentation_set_end (pview);
	else
		ev_view_presentation_update_current_page (pview, new_page);
}

void
ev_view_presentation_previous_page (EvViewPresentation *pview)
{
	gint new_page = 0;

	switch (pview->state) {
	case EV_PRESENTATION_BLACK:
	case EV_PRESENTATION_WHITE:
		ev_view_presentation_set_normal (pview);
		return;
	case EV_PRESENTATION_END:
		pview->state = EV_PRESENTATION_NORMAL;
		new_page = pview->current_page;
		break;
	case EV_PRESENTATION_NORMAL:
		new_page = pview->current_page - 1;
		break;
	}

	ev_view_presentation_update_current_page (pview, new_page);
}

/* Goto Window */
#define KEY_IS_NUMERIC(keyval) \
	((keyval >= GDK_KEY_0 && keyval <= GDK_KEY_9) || (keyval >= GDK_KEY_KP_0 && keyval <= GDK_KEY_KP_9))

/* Cut and paste from gtkwindow.c */
static void
send_focus_change (GtkWidget *widget,
		   gboolean   in)
{
	GdkEvent *fevent = gdk_event_new (GDK_FOCUS_CHANGE);

	fevent->focus_change.type = GDK_FOCUS_CHANGE;
	fevent->focus_change.window = gtk_widget_get_window (widget);
	fevent->focus_change.in = in;
	if (fevent->focus_change.window)
		g_object_ref (fevent->focus_change.window);

	gtk_widget_send_focus_change (widget, fevent);

	gdk_event_free (fevent);
}

static void
ev_view_presentation_goto_window_hide (EvViewPresentation *pview)
{
	/* send focus-in event */
	send_focus_change (pview->goto_entry, FALSE);
	gtk_widget_hide (pview->goto_window);
	gtk_entry_set_text (GTK_ENTRY (pview->goto_entry), "");
}

static gboolean
ev_view_presentation_goto_window_delete_event (GtkWidget          *widget,
					       GdkEventAny        *event,
					       EvViewPresentation *pview)
{
	ev_view_presentation_goto_window_hide (pview);

	return TRUE;
}

static gboolean
ev_view_presentation_goto_window_key_press_event (GtkWidget          *widget,
						  GdkEventKey        *event,
						  EvViewPresentation *pview)
{
	switch (event->keyval) {
	case GDK_KEY_Escape:
	case GDK_KEY_Tab:
	case GDK_KEY_KP_Tab:
	case GDK_KEY_ISO_Left_Tab:
		ev_view_presentation_goto_window_hide (pview);
		return TRUE;
	case GDK_KEY_Return:
	case GDK_KEY_KP_Enter:
	case GDK_KEY_ISO_Enter:
	case GDK_KEY_BackSpace:
	case GDK_KEY_Delete:
		return FALSE;
	default:
		if (!KEY_IS_NUMERIC (event->keyval))
			return TRUE;
	}

	return FALSE;
}

static gboolean
ev_view_presentation_goto_window_button_press_event (GtkWidget          *widget,
						     GdkEventButton     *event,
						     EvViewPresentation *pview)
{
	ev_view_presentation_goto_window_hide (pview);

	return TRUE;
}

static void
ev_view_presentation_goto_entry_activate (GtkEntry           *entry,
					  EvViewPresentation *pview)
{
	const gchar *text;
	gint         page;

	text = gtk_entry_get_text (entry);
	page = atoi (text) - 1;

	ev_view_presentation_goto_window_hide (pview);
	ev_view_presentation_update_current_page (pview, page);
}

static void
ev_view_presentation_goto_window_create (EvViewPresentation *pview)
{
	GtkWidget *frame, *hbox, *label;
	GtkWindow *toplevel, *goto_window;

	toplevel = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (pview)));

	if (pview->goto_window) {
                goto_window = GTK_WINDOW (pview->goto_window);
		if (gtk_window_has_group (toplevel))
			gtk_window_group_add_window (gtk_window_get_group (toplevel), goto_window);
		else if (gtk_window_has_group (goto_window))
			gtk_window_group_remove_window (gtk_window_get_group (goto_window), goto_window);

		return;
	}

	pview->goto_window = gtk_window_new (GTK_WINDOW_POPUP);
        goto_window = GTK_WINDOW (pview->goto_window);
	gtk_window_set_screen (goto_window, gtk_widget_get_screen (GTK_WIDGET (pview)));

	if (gtk_window_has_group (toplevel))
		gtk_window_group_add_window (gtk_window_get_group (toplevel), goto_window);

	gtk_window_set_modal (goto_window, TRUE);

	g_signal_connect (pview->goto_window, "delete_event",
			  G_CALLBACK (ev_view_presentation_goto_window_delete_event),
			  pview);
	g_signal_connect (pview->goto_window, "key_press_event",
			  G_CALLBACK (ev_view_presentation_goto_window_key_press_event),
			  pview);
	g_signal_connect (pview->goto_window, "button_press_event",
			  G_CALLBACK (ev_view_presentation_goto_window_button_press_event),
			  pview);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
	gtk_container_add (GTK_CONTAINER (pview->goto_window), frame);
	gtk_widget_show (frame);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 3);
	gtk_container_add (GTK_CONTAINER (frame), hbox);
	gtk_widget_show (hbox);

	label = gtk_label_new (_("Jump to page:"));
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 3);
	gtk_widget_show (label);
	gtk_widget_realize (label);

	pview->goto_entry = gtk_entry_new ();
	g_signal_connect (pview->goto_entry, "activate",
			  G_CALLBACK (ev_view_presentation_goto_entry_activate),
			  pview);
	gtk_box_pack_start (GTK_BOX (hbox), pview->goto_entry, TRUE, TRUE, 0);
	gtk_widget_show (pview->goto_entry);
	gtk_widget_realize (pview->goto_entry);
}

static void
ev_view_presentation_goto_entry_grab_focus (EvViewPresentation *pview)
{
	GtkWidgetClass *entry_parent_class;

	entry_parent_class = g_type_class_peek_parent (GTK_ENTRY_GET_CLASS (pview->goto_entry));
	(entry_parent_class->grab_focus) (pview->goto_entry);

	send_focus_change (pview->goto_entry, TRUE);
}

static void
ev_view_presentation_goto_window_send_key_event (EvViewPresentation *pview,
						 GdkEvent           *event)
{
	GdkEventKey *new_event;
	GdkScreen   *screen;

	/* Move goto window off screen */
	screen = gtk_widget_get_screen (GTK_WIDGET (pview));
	gtk_window_move (GTK_WINDOW (pview->goto_window),
			 gdk_screen_get_width (screen) + 1,
			 gdk_screen_get_height (screen) + 1);
	gtk_widget_show (pview->goto_window);

	new_event = (GdkEventKey *) gdk_event_copy (event);
	g_object_unref (new_event->window);
	new_event->window = gtk_widget_get_window (pview->goto_window);
	if (new_event->window)
		g_object_ref (new_event->window);
	gtk_widget_realize (pview->goto_window);

	gtk_widget_event (pview->goto_window, (GdkEvent *)new_event);
	gdk_event_free ((GdkEvent *)new_event);
	gtk_widget_hide (pview->goto_window);
}

/* Links */
static gboolean
ev_view_presentation_link_is_supported (EvViewPresentation *pview,
					EvLink             *link)
{
	EvLinkAction *action;

	action = ev_link_get_action (link);
	if (!action)
		return FALSE;

	switch (ev_link_action_get_action_type (action)) {
	case EV_LINK_ACTION_TYPE_GOTO_DEST:
		return ev_link_action_get_dest (action) != NULL;
	case EV_LINK_ACTION_TYPE_NAMED:
        case EV_LINK_ACTION_TYPE_GOTO_REMOTE:
        case EV_LINK_ACTION_TYPE_EXTERNAL_URI:
        case EV_LINK_ACTION_TYPE_LAUNCH:
		return TRUE;
	default:
		return FALSE;
	}

	return FALSE;
}

static EvLink *
ev_view_presentation_get_link_at_location (EvViewPresentation *pview,
					   gdouble             x,
					   gdouble             y)
{
	GdkRectangle   page_area;
	EvMappingList *link_mapping;
	EvLink        *link;
	gdouble        width, height;
	gdouble        new_x, new_y;

	if (!pview->page_cache)
		return NULL;

	ev_document_get_page_size (pview->document, pview->current_page, &width, &height);
	ev_view_presentation_get_page_area (pview, &page_area);
	x = (x - page_area.x) / page_area.width;
	y = (y - page_area.y) / page_area.height;
	switch (pview->rotation) {
	case 0:
	case 360:
		new_x = width * x;
		new_y = height * y;
		break;
	case 90:
		new_x = width * y;
		new_y = height * (1 - x);
		break;
	case 180:
		new_x = width * (1 - x);
		new_y = height * (1 - y);
		break;
	case 270:
		new_x = width * (1 - y);
		new_y = height * x;
		break;
	default:
		g_assert_not_reached ();
	}

	link_mapping = ev_page_cache_get_link_mapping (pview->page_cache, pview->current_page);

	link = link_mapping ? ev_mapping_list_get_data (link_mapping, new_x, new_y) : NULL;

	return link && ev_view_presentation_link_is_supported (pview, link) ? link : NULL;
}

static void
ev_vew_presentation_handle_link (EvViewPresentation *pview,
                                 EvLink             *link)
{
	EvLinkAction *action;

	action = ev_link_get_action (link);

        switch (ev_link_action_get_action_type (action)) {
	case EV_LINK_ACTION_TYPE_NAMED: {
		const gchar *name = ev_link_action_get_name (action);

		if (g_ascii_strcasecmp (name, "FirstPage") == 0) {
			ev_view_presentation_update_current_page (pview, 0);
		} else if (g_ascii_strcasecmp (name, "PrevPage") == 0) {
			ev_view_presentation_update_current_page (pview, pview->current_page - 1);
		} else if (g_ascii_strcasecmp (name, "NextPage") == 0) {
			ev_view_presentation_update_current_page (pview, pview->current_page + 1);
		} else if (g_ascii_strcasecmp (name, "LastPage") == 0) {
			gint n_pages;

			n_pages = ev_document_get_n_pages (pview->document);
			ev_view_presentation_update_current_page (pview, n_pages - 1);
		}
        }
                break;

	case EV_LINK_ACTION_TYPE_GOTO_DEST: {
		EvLinkDest *dest;
		gint        page;

		dest = ev_link_action_get_dest (action);
		page = ev_document_links_get_dest_page (EV_DOCUMENT_LINKS (pview->document), dest);
		ev_view_presentation_update_current_page (pview, page);
        }
                break;
        case EV_LINK_ACTION_TYPE_GOTO_REMOTE:
        case EV_LINK_ACTION_TYPE_EXTERNAL_URI:
        case EV_LINK_ACTION_TYPE_LAUNCH:
                g_signal_emit (pview, signals[SIGNAL_EXTERNAL_LINK], 0, action);
                break;
        default:
                break;
	}
}

/* Cursors */
static void
ev_view_presentation_set_cursor (EvViewPresentation *pview,
				 EvViewCursor        view_cursor)
{
	GtkWidget  *widget;
	GdkCursor  *cursor;

	if (pview->cursor == view_cursor)
		return;

	widget = GTK_WIDGET (pview);
	if (!gtk_widget_get_realized (widget))
		gtk_widget_realize (widget);

	pview->cursor = view_cursor;

	cursor = ev_view_cursor_new (gtk_widget_get_display (widget), view_cursor);
	gdk_window_set_cursor (gtk_widget_get_window (widget), cursor);
	gdk_flush ();
	if (cursor)
		g_object_unref (cursor);
}

static void
ev_view_presentation_set_cursor_for_location (EvViewPresentation *pview,
					      gdouble             x,
					      gdouble             y)
{
	if (ev_view_presentation_get_link_at_location (pview, x, y))
		ev_view_presentation_set_cursor (pview, EV_VIEW_CURSOR_LINK);
	else
		ev_view_presentation_set_cursor (pview, EV_VIEW_CURSOR_NORMAL);
}

static gboolean
hide_cursor_timeout_cb (EvViewPresentation *pview)
{
	ev_view_presentation_set_cursor (pview, EV_VIEW_CURSOR_HIDDEN);
	pview->hide_cursor_timeout_id = 0;

	return FALSE;
}

static void
ev_view_presentation_hide_cursor_timeout_stop (EvViewPresentation *pview)
{
	if (pview->hide_cursor_timeout_id > 0)
		g_source_remove (pview->hide_cursor_timeout_id);
	pview->hide_cursor_timeout_id = 0;
}

static void
ev_view_presentation_hide_cursor_timeout_start (EvViewPresentation *pview)
{
	ev_view_presentation_hide_cursor_timeout_stop (pview);
	pview->hide_cursor_timeout_id =
		g_timeout_add_seconds (HIDE_CURSOR_TIMEOUT,
				       (GSourceFunc)hide_cursor_timeout_cb,
				       pview);
}

static void
ev_view_presentation_update_current_surface (EvViewPresentation *pview,
					     cairo_surface_t    *surface)
{
	if (!surface || pview->current_surface == surface)
		return;

	cairo_surface_reference (surface);
	if (pview->current_surface)
		cairo_surface_destroy (pview->current_surface);
	pview->current_surface = surface;
}

static void
ev_view_presentation_dispose (GObject *object)
{
	EvViewPresentation *pview = EV_VIEW_PRESENTATION (object);

	if (pview->document) {
		g_object_unref (pview->document);
		pview->document = NULL;
	}

	ev_view_presentation_animation_cancel (pview);
	ev_view_presentation_transition_stop (pview);
	ev_view_presentation_hide_cursor_timeout_stop (pview);
        ev_view_presentation_reset_jobs (pview);

	if (pview->current_surface) {
		cairo_surface_destroy (pview->current_surface);
		pview->current_surface = NULL;
	}

	if (pview->page_cache) {
		g_object_unref (pview->page_cache);
		pview->page_cache = NULL;
	}

	if (pview->goto_window) {
		gtk_widget_destroy (pview->goto_window);
		pview->goto_window = NULL;
		pview->goto_entry = NULL;
	}

	G_OBJECT_CLASS (ev_view_presentation_parent_class)->dispose (object);
}

static void
ev_view_presentation_get_preferred_width (GtkWidget *widget,
                                          gint      *minimum,
                                          gint      *natural)
{
        *minimum = *natural = 0;
}

static void
ev_view_presentation_get_preferred_height (GtkWidget *widget,
                                           gint      *minimum,
                                           gint      *natural)
{
        *minimum = *natural = 0;
}

static void
ev_view_presentation_draw_end_page (EvViewPresentation *pview,
                                    cairo_t *cr)
{
	GtkWidget *widget = GTK_WIDGET (pview);
	PangoLayout *layout;
	PangoFontDescription *font_desc;
	gchar *markup;
	const gchar *text = _("End of presentation. Click to exit.");

	if (pview->state != EV_PRESENTATION_END)
		return;

	layout = gtk_widget_create_pango_layout (widget, NULL);
	markup = g_strdup_printf ("<span foreground=\"white\">%s</span>", text);
	pango_layout_set_markup (layout, markup, -1);
	g_free (markup);

	font_desc = pango_font_description_new ();
	pango_font_description_set_size (font_desc, 16 * PANGO_SCALE);
	pango_layout_set_font_description (layout, font_desc);

        gtk_render_layout (gtk_widget_get_style_context (widget),
                           cr, 15, 15, layout);

	pango_font_description_free (font_desc);
	g_object_unref (layout);
}

static gboolean
ev_view_presentation_draw (GtkWidget *widget,
                           cairo_t   *cr)
{
	EvViewPresentation *pview = EV_VIEW_PRESENTATION (widget);
	GdkRectangle        page_area;
	GdkRectangle        overlap;
	cairo_surface_t    *surface;
        GdkRectangle        clip_rect;

        if (!gdk_cairo_get_clip_rectangle (cr, &clip_rect))
                return FALSE;

	switch (pview->state) {
	case EV_PRESENTATION_END:
		ev_view_presentation_draw_end_page (pview, cr);
		return FALSE;
	case EV_PRESENTATION_BLACK:
	case EV_PRESENTATION_WHITE:
		return FALSE;
	case EV_PRESENTATION_NORMAL:
		break;
	}

	if (pview->animation) {
		if (ev_transition_animation_ready (pview->animation)) {
			ev_view_presentation_get_page_area (pview, &page_area);

                        cairo_save (cr);

			/* normalize to x=0, y=0 */
			cairo_translate (cr, page_area.x, page_area.y);
			page_area.x = page_area.y = 0;

			/* Try to fix rounding errors */
			page_area.width--;

			ev_transition_animation_paint (pview->animation, cr, page_area);

                        cairo_restore (cr);
		}

		return TRUE;
	}

	surface = get_surface_from_job (pview, pview->curr_job);
	if (surface) {
		ev_view_presentation_update_current_surface (pview, surface);
	} else if (pview->current_surface) {
		surface = pview->current_surface;
	} else {
		return FALSE;
	}

	ev_view_presentation_get_page_area (pview, &page_area);
	if (gdk_rectangle_intersect (&page_area, &clip_rect, &overlap)) {
                cairo_save (cr);

		/* Try to fix rounding errors. See bug #438760 */
		if (overlap.width == page_area.width)
			overlap.width--;

		cairo_rectangle (cr, overlap.x, overlap.y, overlap.width, overlap.height);
		cairo_set_source_surface (cr, surface, page_area.x, page_area.y);
		cairo_fill (cr);

                cairo_restore (cr);
	}

	return FALSE;
}

static gboolean
ev_view_presentation_key_press_event (GtkWidget   *widget,
				      GdkEventKey *event)
{
	EvViewPresentation *pview = EV_VIEW_PRESENTATION (widget);

	if (pview->state == EV_PRESENTATION_END)
                return gtk_bindings_activate_event (G_OBJECT (widget), event);

        if (event->state != 0)
                return gtk_bindings_activate_event (G_OBJECT (widget), event);

	switch (event->keyval) {
	case GDK_KEY_b:
	case GDK_KEY_B:
	case GDK_KEY_period:
	case GDK_KEY_KP_Decimal:
		if (pview->state == EV_PRESENTATION_BLACK)
			ev_view_presentation_set_normal (pview);
		else
			ev_view_presentation_set_black (pview);

		return TRUE;
	case GDK_KEY_w:
	case GDK_KEY_W:
		if (pview->state == EV_PRESENTATION_WHITE)
			ev_view_presentation_set_normal (pview);
		else
			ev_view_presentation_set_white (pview);

		return TRUE;
	case GDK_KEY_Home:
		if (pview->state == EV_PRESENTATION_NORMAL) {
			ev_view_presentation_update_current_page (pview, 0);
			return TRUE;
		}
		break;
	case GDK_KEY_End:
		if (pview->state == EV_PRESENTATION_NORMAL) {
			gint page;

			page = ev_document_get_n_pages (pview->document) - 1;
			ev_view_presentation_update_current_page (pview, page);

			return TRUE;
		}
		break;
	default:
		break;
	}

	ev_view_presentation_set_normal (pview);

	if (ev_document_get_n_pages (pview->document) > 1 && KEY_IS_NUMERIC (event->keyval)) {
		gint x, y;

		ev_view_presentation_goto_window_create (pview);
		ev_view_presentation_goto_window_send_key_event (pview, (GdkEvent *)event);
		ev_document_misc_get_pointer_position (GTK_WIDGET (pview), &x, &y);
		gtk_window_move (GTK_WINDOW (pview->goto_window), x, y);
		gtk_widget_show (pview->goto_window);
		ev_view_presentation_goto_entry_grab_focus (pview);

		return TRUE;
	}

	return gtk_bindings_activate_event (G_OBJECT (widget), event);
}

static gboolean
ev_view_presentation_button_release_event (GtkWidget      *widget,
					   GdkEventButton *event)
{
	EvViewPresentation *pview = EV_VIEW_PRESENTATION (widget);

	switch (event->button) {
	case 1: {
		EvLink *link;

		if (pview->state == EV_PRESENTATION_END) {
			g_signal_emit (pview, signals[FINISHED], 0, NULL);

			return FALSE;
		}

		link = ev_view_presentation_get_link_at_location (pview,
								  event->x,
								  event->y);
		if (link)
			ev_vew_presentation_handle_link (pview, link);
		else
			ev_view_presentation_next_page (pview);
	}
		break;
	case 3:
		ev_view_presentation_previous_page (pview);
		break;
	default:
		break;
	}

	return FALSE;
}

static gint
ev_view_presentation_focus_out (GtkWidget     *widget,
				GdkEventFocus *event)
{
	EvViewPresentation *pview = EV_VIEW_PRESENTATION (widget);

	if (pview->goto_window)
		ev_view_presentation_goto_window_hide (pview);

	return FALSE;
}

static gboolean
ev_view_presentation_motion_notify_event (GtkWidget      *widget,
					  GdkEventMotion *event)
{
	EvViewPresentation *pview = EV_VIEW_PRESENTATION (widget);

	ev_view_presentation_hide_cursor_timeout_start (pview);
	ev_view_presentation_set_cursor_for_location (pview, event->x, event->y);

	return FALSE;
}

static void
ev_view_presentation_update_monitor_geometry (EvViewPresentation *pview)
{
	GdkScreen          *screen = gtk_widget_get_screen (GTK_WIDGET (pview));
	GdkRectangle        monitor;
	gint                monitor_num;

	monitor_num = gdk_screen_get_monitor_at_window (screen, gtk_widget_get_window (GTK_WIDGET (pview)));
	gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);
	pview->monitor_width = monitor.width;
	pview->monitor_height = monitor.height;
}

static gboolean
init_presentation (GtkWidget *widget)
{
	EvViewPresentation *pview = EV_VIEW_PRESENTATION (widget);

	ev_view_presentation_update_monitor_geometry (pview);

	ev_view_presentation_update_current_page (pview, pview->current_page);
	ev_view_presentation_hide_cursor_timeout_start (pview);

	return FALSE;
}

static void
ev_view_presentation_realize (GtkWidget *widget)
{
	GdkWindow     *window;
	GdkWindowAttr  attributes;
	GtkAllocation  allocation;

	gtk_widget_set_realized (widget, TRUE);

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);

	gtk_widget_get_allocation (widget, &allocation);
	attributes.x = allocation.x;
	attributes.y = allocation.y;
	attributes.width = allocation.width;
	attributes.height = allocation.height;
	attributes.event_mask = GDK_EXPOSURE_MASK |
		GDK_BUTTON_PRESS_MASK |
		GDK_BUTTON_RELEASE_MASK |
		GDK_SCROLL_MASK |
		GDK_KEY_PRESS_MASK |
		GDK_POINTER_MOTION_MASK |
		GDK_POINTER_MOTION_HINT_MASK |
		GDK_ENTER_NOTIFY_MASK |
		GDK_LEAVE_NOTIFY_MASK;

	window = gdk_window_new (gtk_widget_get_parent_window (widget),
				 &attributes,
				 GDK_WA_X | GDK_WA_Y |
				 GDK_WA_VISUAL);

	gdk_window_set_user_data (window, widget);
	gtk_widget_set_window (widget, window);
        gtk_style_context_set_background (gtk_widget_get_style_context (widget),
                                          window);

	g_idle_add ((GSourceFunc)init_presentation, widget);
}

static void
ev_view_presentation_change_page (EvViewPresentation *pview,
				  GtkScrollType       scroll)
{
	switch (scroll) {
	case GTK_SCROLL_PAGE_FORWARD:
		ev_view_presentation_next_page (pview);
		break;
	case GTK_SCROLL_PAGE_BACKWARD:
		ev_view_presentation_previous_page (pview);
		break;
	default:
		g_assert_not_reached ();
	}
}

static gboolean
ev_view_presentation_scroll_event (GtkWidget      *widget,
				   GdkEventScroll *event)
{
	EvViewPresentation *pview = EV_VIEW_PRESENTATION (widget);
	guint               state;

	state = event->state & gtk_accelerator_get_default_mod_mask ();
	if (state != 0)
		return FALSE;

	switch (event->direction) {
	case GDK_SCROLL_DOWN:
	case GDK_SCROLL_RIGHT:
		ev_view_presentation_change_page (pview, GTK_SCROLL_PAGE_FORWARD);
		break;
	case GDK_SCROLL_UP:
	case GDK_SCROLL_LEFT:
		ev_view_presentation_change_page (pview, GTK_SCROLL_PAGE_BACKWARD);
		break;
        case GDK_SCROLL_SMOOTH:
                return FALSE;
	}

	return TRUE;
}


static void
add_change_page_binding_keypad (GtkBindingSet  *binding_set,
				guint           keyval,
				GdkModifierType modifiers,
				GtkScrollType   scroll)
{
	guint keypad_keyval = keyval - GDK_KEY_Left + GDK_KEY_KP_Left;

	gtk_binding_entry_add_signal (binding_set, keyval, modifiers,
				      "change_page", 1,
				      GTK_TYPE_SCROLL_TYPE, scroll);
	gtk_binding_entry_add_signal (binding_set, keypad_keyval, modifiers,
				      "change_page", 1,
				      GTK_TYPE_SCROLL_TYPE, scroll);
}

static void
ev_view_presentation_set_property (GObject      *object,
				   guint         prop_id,
				   const GValue *value,
				   GParamSpec   *pspec)
{
	EvViewPresentation *pview = EV_VIEW_PRESENTATION (object);

	switch (prop_id) {
	case PROP_DOCUMENT:
		pview->document = g_value_dup_object (value);
		pview->enable_animations = EV_IS_DOCUMENT_TRANSITION (pview->document);
		break;
	case PROP_CURRENT_PAGE:
		ev_view_presentation_set_current_page (pview, g_value_get_uint (value));
		break;
	case PROP_ROTATION:
                ev_view_presentation_set_rotation (pview, g_value_get_uint (value));
		break;
	case PROP_INVERTED_COLORS:
		pview->inverted_colors = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
ev_view_presentation_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
        EvViewPresentation *pview = EV_VIEW_PRESENTATION (object);

        switch (prop_id) {
        case PROP_CURRENT_PAGE:
                g_value_set_uint (value, pview->current_page);
                break;
        case PROP_ROTATION:
                g_value_set_uint (value, ev_view_presentation_get_rotation (pview));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
ev_view_presentation_notify_scale_factor (EvViewPresentation *pview)
{
        ev_view_presentation_update_monitor_geometry (pview);
        ev_view_presentation_reset_jobs (pview);
        ev_view_presentation_update_current_page (pview, pview->current_page);
}

static GObject *
ev_view_presentation_constructor (GType                  type,
				  guint                  n_construct_properties,
				  GObjectConstructParam *construct_params)
{
	GObject            *object;
	EvViewPresentation *pview;

	object = G_OBJECT_CLASS (ev_view_presentation_parent_class)->constructor (type,
										  n_construct_properties,
										  construct_params);
	pview = EV_VIEW_PRESENTATION (object);
        pview->is_constructing = FALSE;

	if (EV_IS_DOCUMENT_LINKS (pview->document)) {
		pview->page_cache = ev_page_cache_new (pview->document);
		ev_page_cache_set_flags (pview->page_cache, EV_PAGE_DATA_INCLUDE_LINKS);
	}

        g_signal_connect (object, "notify::scale-factor",
                          G_CALLBACK (ev_view_presentation_notify_scale_factor), NULL);

	return object;
}

static void
ev_view_presentation_class_init (EvViewPresentationClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GObjectClass   *gobject_class = G_OBJECT_CLASS (klass);
	GtkBindingSet  *binding_set;

	klass->change_page = ev_view_presentation_change_page;

        gobject_class->dispose = ev_view_presentation_dispose;

	widget_class->get_preferred_width = ev_view_presentation_get_preferred_width;
	widget_class->get_preferred_height = ev_view_presentation_get_preferred_height;
	widget_class->realize = ev_view_presentation_realize;
        widget_class->draw = ev_view_presentation_draw;
	widget_class->key_press_event = ev_view_presentation_key_press_event;
	widget_class->button_release_event = ev_view_presentation_button_release_event;
	widget_class->focus_out_event = ev_view_presentation_focus_out;
	widget_class->motion_notify_event = ev_view_presentation_motion_notify_event;
	widget_class->scroll_event = ev_view_presentation_scroll_event;

	gobject_class->constructor = ev_view_presentation_constructor;
	gobject_class->set_property = ev_view_presentation_set_property;
        gobject_class->get_property = ev_view_presentation_get_property;

	g_object_class_install_property (gobject_class,
					 PROP_DOCUMENT,
					 g_param_spec_object ("document",
							      "Document",
							      "Document",
							      EV_TYPE_DOCUMENT,
							      G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class,
					 PROP_CURRENT_PAGE,
					 g_param_spec_uint ("current-page",
							    "Current Page",
							    "The current page",
							    0, G_MAXUINT, 0,
							    G_PARAM_READWRITE |
							    G_PARAM_CONSTRUCT |
                                                            G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class,
					 PROP_ROTATION,
					 g_param_spec_uint ("rotation",
							    "Rotation",
							    "Current rotation angle",
							    0, 360, 0,
							    G_PARAM_READWRITE |
							    G_PARAM_CONSTRUCT |
                                                            G_PARAM_STATIC_STRINGS));
	g_object_class_install_property (gobject_class,
					 PROP_INVERTED_COLORS,
					 g_param_spec_boolean ("inverted-colors",
							       "Inverted Colors",
							       "Whether presentation is displayed with inverted colors",
							       FALSE,
							       G_PARAM_WRITABLE |
							       G_PARAM_CONSTRUCT_ONLY |
                                                               G_PARAM_STATIC_STRINGS));

	signals[CHANGE_PAGE] =
		g_signal_new ("change_page",
			      G_OBJECT_CLASS_TYPE (gobject_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (EvViewPresentationClass, change_page),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__ENUM,
			      G_TYPE_NONE, 1,
			      GTK_TYPE_SCROLL_TYPE);
	signals[FINISHED] =
		g_signal_new ("finished",
			      G_OBJECT_CLASS_TYPE (gobject_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (EvViewPresentationClass, finished),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0,
			      G_TYPE_NONE);
        signals[SIGNAL_EXTERNAL_LINK] =
                g_signal_new ("external-link",
                              G_TYPE_FROM_CLASS (gobject_class),
                              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                              G_STRUCT_OFFSET (EvViewPresentationClass, external_link),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__OBJECT,
                              G_TYPE_NONE, 1,
                              G_TYPE_OBJECT);

	binding_set = gtk_binding_set_by_class (klass);
	add_change_page_binding_keypad (binding_set, GDK_KEY_Left,  0, GTK_SCROLL_PAGE_BACKWARD);
	add_change_page_binding_keypad (binding_set, GDK_KEY_Right, 0, GTK_SCROLL_PAGE_FORWARD);
	add_change_page_binding_keypad (binding_set, GDK_KEY_Up,    0, GTK_SCROLL_PAGE_BACKWARD);
	add_change_page_binding_keypad (binding_set, GDK_KEY_Down,  0, GTK_SCROLL_PAGE_FORWARD);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_space, 0,
				      "change_page", 1,
				      GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_PAGE_FORWARD);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_BackSpace, 0,
				      "change_page", 1,
				      GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_PAGE_BACKWARD);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_Page_Down, 0,
				      "change_page", 1,
				      GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_PAGE_FORWARD);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_Page_Up, 0,
				      "change_page", 1,
				      GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_PAGE_BACKWARD);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_J, 0,
				      "change_page", 1,
				      GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_PAGE_FORWARD);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_H, 0,
				      "change_page", 1,
				      GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_PAGE_BACKWARD);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_L, 0,
				      "change_page", 1,
				      GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_PAGE_FORWARD);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_K, 0,
				      "change_page", 1,
				      GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_PAGE_BACKWARD);
}

static void
ev_view_presentation_init (EvViewPresentation *pview)
{
        static gsize initialization_value = 0;

	gtk_widget_set_can_focus (GTK_WIDGET (pview), TRUE);
        pview->is_constructing = TRUE;

	if (g_once_init_enter (&initialization_value)) {
		GtkCssProvider *provider;

		provider = gtk_css_provider_new ();
		gtk_css_provider_load_from_data (provider,
						 "EvViewPresentation {\n"
						 " background-color: black; }",
						 -1, NULL);
		gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
							   GTK_STYLE_PROVIDER (provider),
							   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		g_object_unref (provider);
		g_once_init_leave (&initialization_value, 1);
	}
}

GtkWidget *
ev_view_presentation_new (EvDocument *document,
			  guint       current_page,
			  guint       rotation,
			  gboolean    inverted_colors)
{
	g_return_val_if_fail (EV_IS_DOCUMENT (document), NULL);
	g_return_val_if_fail (current_page < ev_document_get_n_pages (document), NULL);

	return GTK_WIDGET (g_object_new (EV_TYPE_VIEW_PRESENTATION,
					 "document", document,
					 "current_page", current_page,
					 "rotation", rotation,
					 "inverted_colors", inverted_colors,
					 NULL));
}

guint
ev_view_presentation_get_current_page (EvViewPresentation *pview)
{
	return pview->current_page;
}

void
ev_view_presentation_set_rotation (EvViewPresentation *pview,
                                   gint                rotation)
{
        if (rotation >= 360)
                rotation -= 360;
        else if (rotation < 0)
                rotation += 360;

        if (pview->rotation == rotation)
                return;

        pview->rotation = rotation;
        g_object_notify (G_OBJECT (pview), "rotation");
        if (pview->is_constructing)
                return;

        ev_view_presentation_reset_jobs (pview);
        ev_view_presentation_update_current_page (pview, pview->current_page);
}

guint
ev_view_presentation_get_rotation (EvViewPresentation *pview)
{
        return pview->rotation;
}
