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

	gint64		       start_time;
	gdouble		       transition_time;
	gint                   animation_tick_id;

	guint                  current_page;
	guint                  previous_page;
	GdkTexture            *current_texture;
	GdkTexture	      *previous_texture;
	EvDocument            *document;
	guint                  rotation;
	gboolean               inverted_colors;
	EvPresentationState    state;

	/* Cursors */
	EvViewCursor           cursor;
	guint                  hide_cursor_timeout_id;

	/* Goto Window */
	GtkWidget             *goto_popup;
	GtkWidget             *goto_entry;

	/* Page Transition */
	guint                  trans_timeout_id;

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

static void ev_view_presentation_update_current_texture (EvViewPresentation *pview,
							 GdkTexture    *surface);

#define HIDE_CURSOR_TIMEOUT 5000

G_DEFINE_TYPE (EvViewPresentation, ev_view_presentation, GTK_TYPE_WIDGET)

static void
ev_view_presentation_set_normal (EvViewPresentation *pview)
{
	GtkWidget *widget = GTK_WIDGET (pview);

	if (pview->state == EV_PRESENTATION_NORMAL)
		return;

	pview->state = EV_PRESENTATION_NORMAL;

	gtk_widget_remove_css_class(widget, "white-mode");
        gtk_widget_queue_draw (widget);
}

static void
ev_view_presentation_set_black (EvViewPresentation *pview)
{
	GtkWidget *widget = GTK_WIDGET (pview);

	if (pview->state == EV_PRESENTATION_BLACK)
		return;

	pview->state = EV_PRESENTATION_BLACK;

	gtk_widget_remove_css_class(widget, "white-mode");
	gtk_widget_queue_draw (widget);
}

static void
ev_view_presentation_set_white (EvViewPresentation *pview)
{
	GtkWidget *widget = GTK_WIDGET (pview);

	if (pview->state == EV_PRESENTATION_WHITE)
		return;

	pview->state = EV_PRESENTATION_WHITE;

	gtk_widget_add_css_class(widget, "white-mode");
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
	int widget_width, widget_height;

	ev_document_get_page_size (pview->document, page, &width, &height);
	if (pview->rotation == 90 || pview->rotation == 270) {
		gdouble tmp;

		tmp = width;
		width = height;
		height = tmp;
	}

	widget_width = gtk_widget_get_width (GTK_WIDGET (pview));
	widget_height = gtk_widget_get_height (GTK_WIDGET (pview));

	if (widget_width / width < widget_height / height) {
		*view_width = widget_width;
		*view_height = (int)((widget_width / width) * height + 0.5);
	} else {
		*view_width = (int)((widget_height / height) * width + 0.5);
		*view_height = widget_height;
	}
}

static void
ev_view_presentation_get_page_area (EvViewPresentation *pview,
				    GdkRectangle       *area)
{
	GtkWidget    *widget = GTK_WIDGET (pview);
	gint          view_width, view_height, widget_width, widget_height;

	ev_view_presentation_get_view_size (pview, pview->current_page,
					    &view_width, &view_height);

	widget_width = gtk_widget_get_width (widget);
	widget_height = gtk_widget_get_height (widget);

	area->x = (MAX (0, widget_width - view_width)) / 2;
	area->y = (MAX (0, widget_height - view_height)) / 2;
	area->width = view_width;
	area->height = view_height;
}

/* Page Transition */
static void
transition_next_page (EvViewPresentation *pview)
{
	pview->trans_timeout_id = 0;
	ev_view_presentation_next_page (pview);
}

static void
ev_view_presentation_transition_stop (EvViewPresentation *pview)
{
	g_clear_handle_id (&pview->trans_timeout_id, g_source_remove);
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
				g_timeout_add_once (duration * 1000,
						    (GSourceOnceFunc) transition_next_page,
						    pview);
	}
}

static gboolean
animation_tick_cb (GtkWidget     *widget,
		   GdkFrameClock *clock,
		   gpointer       unused)
{
	EvViewPresentation *pview = EV_VIEW_PRESENTATION (widget);
	gint64 frame_time = gdk_frame_clock_get_frame_time (clock);
	EvTransitionEffect *effect;
	gdouble duration = 0;

	if (!EV_IS_DOCUMENT_TRANSITION (pview->document))
		return G_SOURCE_REMOVE;

	if (pview->start_time == 0)
		pview->start_time = frame_time;

	pview->transition_time = (frame_time - pview->start_time) / (float)G_USEC_PER_SEC;

	gtk_widget_queue_draw (widget);

	effect = ev_document_transition_get_effect (
			EV_DOCUMENT_TRANSITION (pview->document),
			pview->current_page);
	g_object_get (effect, "duration-real", &duration, NULL);

	if (pview->transition_time >= duration) {
		ev_view_presentation_transition_start (pview);
		return G_SOURCE_REMOVE;
	}
	else {
		return G_SOURCE_CONTINUE;
	}
}

static void
ev_view_presentation_animation_cancel (EvViewPresentation *pview)
{
	if (pview->animation_tick_id) {
		gtk_widget_remove_tick_callback (GTK_WIDGET (pview), pview->animation_tick_id);
		pview->animation_tick_id = 0;
	}
}

static void
ev_view_presentation_animation_start (EvViewPresentation *pview)
{
	GtkWidget *widget = GTK_WIDGET (pview);

	pview->start_time = 0;
	pview->animation_tick_id = gtk_widget_add_tick_callback (widget,
			animation_tick_cb, pview, NULL);
	gtk_widget_queue_draw (widget);
}

static GdkTexture *
get_texture_from_job (EvViewPresentation *pview,
                      EvJob              *job)
{
        if (!job)
                return NULL;

        return EV_JOB_RENDER_TEXTURE (job)->texture;
}

/* Page Navigation */
static void
job_finished_cb (EvJob              *job,
		 EvViewPresentation *pview)
{
	if (job != pview->curr_job)
		return;

	ev_view_presentation_update_current_texture (pview,
			get_texture_from_job (pview, job));

	ev_view_presentation_animation_start (pview);
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
	gint device_scale = gtk_widget_get_scale_factor (GTK_WIDGET (pview));
	view_width *= device_scale;
	view_height *= device_scale;
	job = ev_job_render_texture_new (pview->document, page, pview->rotation, 0.,
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
ev_view_presentation_update_current_texture (EvViewPresentation *pview,
					     GdkTexture    *texture)
{
	if (!texture || pview->current_texture == texture)
		return;

	g_object_ref (texture);

	g_clear_object (&pview->previous_texture);

	pview->previous_texture = pview->current_texture;
	pview->current_texture = texture;
}

static void
ev_view_presentation_update_current_page (EvViewPresentation *pview,
					  guint               page)
{
	gint jump;

	if (page < 0 || page >= ev_document_get_n_pages (pview->document))
		return;

	ev_view_presentation_animation_cancel (pview);
	ev_view_presentation_transition_stop (pview);

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

		if (pview->prev_job)
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
		pview->previous_page = pview->current_page;
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

	if (EV_JOB_RENDER_TEXTURE (pview->curr_job)->texture) {
		ev_view_presentation_update_current_texture (pview,
				EV_JOB_RENDER_TEXTURE (pview->curr_job)->texture);

		ev_view_presentation_animation_start (pview);
	}
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
static int
key_to_digit (int keyval)
{
	if (keyval >= GDK_KEY_0 && keyval <= GDK_KEY_9)
		return keyval - GDK_KEY_0;

	if (keyval >= GDK_KEY_KP_0 && keyval <= GDK_KEY_KP_9)
		return keyval - GDK_KEY_KP_0;

	return -1;
}

static gboolean
key_is_numeric (int keyval)
{
	return key_to_digit (keyval) >= 0;
}

static void
ev_view_presentation_goto_entry_activate (GtkEntry           *entry,
					  EvViewPresentation *pview)
{
	const gchar *text;
	gint         page;

	text = gtk_editable_get_text (GTK_EDITABLE (entry));
	page = atoi (text) - 1;

	gtk_popover_popdown (GTK_POPOVER (pview->goto_popup));
	ev_view_presentation_update_current_page (pview, page);
}


static void
ev_view_presentation_goto_window_create (EvViewPresentation *pview)
{
	GtkWidget *hbox, *label;

	pview->goto_popup = gtk_popover_new ();
	gtk_popover_set_position (GTK_POPOVER (pview->goto_popup), GTK_POS_BOTTOM);
	gtk_popover_set_has_arrow (GTK_POPOVER (pview->goto_popup), FALSE);
	gtk_widget_set_halign (pview->goto_popup, GTK_ALIGN_START);
	gtk_widget_set_parent (pview->goto_popup, GTK_WIDGET (pview));

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_spacing (GTK_BOX (hbox), 3);
	gtk_widget_set_margin_top (hbox, 3);
	gtk_widget_set_margin_bottom (hbox, 3);
	gtk_widget_set_margin_start (hbox, 3);
	gtk_widget_set_margin_end (hbox, 3);
	gtk_popover_set_child (GTK_POPOVER (pview->goto_popup), hbox);

	label = gtk_label_new (_("Jump to page:"));
	gtk_box_append (GTK_BOX (hbox), label);

	pview->goto_entry = gtk_entry_new ();

	gtk_box_append (GTK_BOX (hbox), pview->goto_entry);

	g_signal_connect (pview->goto_entry, "activate",
			  G_CALLBACK (ev_view_presentation_goto_entry_activate),
			  pview);
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
ev_view_presentation_handle_link (EvViewPresentation *pview,
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
	GtkWidget  *widget = GTK_WIDGET (pview);

	if (pview->cursor == view_cursor)
		return;

	if (!gtk_widget_get_realized (widget))
		gtk_widget_realize (widget);

	pview->cursor = view_cursor;

	gtk_widget_set_cursor_from_name (widget,
			ev_view_cursor_name (view_cursor));
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

static void
hide_cursor_timeout_cb (EvViewPresentation *pview)
{
	ev_view_presentation_set_cursor (pview, EV_VIEW_CURSOR_HIDDEN);
	pview->hide_cursor_timeout_id = 0;
}

static void
ev_view_presentation_hide_cursor_timeout_stop (EvViewPresentation *pview)
{
	g_clear_handle_id (&pview->hide_cursor_timeout_id, g_source_remove);
}

static void
ev_view_presentation_hide_cursor_timeout_start (EvViewPresentation *pview)
{
	ev_view_presentation_hide_cursor_timeout_stop (pview);
	pview->hide_cursor_timeout_id =
		g_timeout_add_once (HIDE_CURSOR_TIMEOUT,
				    (GSourceOnceFunc)hide_cursor_timeout_cb,
				    pview);
}

static void
ev_view_presentation_dispose (GObject *object)
{
	EvViewPresentation *pview = EV_VIEW_PRESENTATION (object);

	g_clear_object (&pview->document);

	ev_view_presentation_transition_stop (pview);
	ev_view_presentation_hide_cursor_timeout_stop (pview);
        ev_view_presentation_reset_jobs (pview);

	g_clear_object (&pview->current_texture);
	g_clear_object (&pview->page_cache);
	g_clear_object (&pview->previous_texture);

	g_clear_pointer (&pview->goto_popup, gtk_widget_unparent);

	g_clear_handle_id (&pview->trans_timeout_id, g_source_remove);
	g_clear_handle_id (&pview->hide_cursor_timeout_id, g_source_remove);

	G_OBJECT_CLASS (ev_view_presentation_parent_class)->dispose (object);
}

static void
ev_view_presentation_snapshot_end_page (EvViewPresentation *pview, GtkSnapshot *snapshot)
{
	GtkWidget *widget = GTK_WIDGET (pview);
	PangoLayout *layout;
	PangoFontDescription *font_desc;
	gchar *markup;
	int text_width, text_height, x_center;
	const gchar *text = _("End of presentation. Press Esc or click to exit.");

	if (pview->state != EV_PRESENTATION_END)
		return;

	layout = gtk_widget_create_pango_layout (widget, NULL);
	markup = g_strdup_printf ("<span foreground=\"white\">%s</span>", text);
	pango_layout_set_markup (layout, markup, -1);
	g_free (markup);

	font_desc = pango_font_description_new ();
	pango_font_description_set_size (font_desc, 16 * PANGO_SCALE);
	pango_layout_set_font_description (layout, font_desc);
	pango_layout_get_pixel_size (layout, &text_width, &text_height);
	x_center = gtk_widget_get_width (widget) / 2 - text_width / 2;

	gtk_snapshot_render_layout (snapshot, gtk_widget_get_style_context (widget),
				x_center, 15, layout);

	pango_font_description_free (font_desc);
	g_object_unref (layout);
}

static GskGLShader *
ev_view_presentation_load_shader (EvTransitionEffectType type)
{
	static const char *shader_resources[] = {
		[EV_TRANSITION_EFFECT_REPLACE] = NULL,
		[EV_TRANSITION_EFFECT_SPLIT] = "/org/gnome/evince/shader/split.glsl",
		[EV_TRANSITION_EFFECT_WIPE] = "/org/gnome/evince/shader/wipe.glsl",
		[EV_TRANSITION_EFFECT_COVER] = "/org/gnome/evince/shader/cover.glsl",
		[EV_TRANSITION_EFFECT_UNCOVER] = "/org/gnome/evince/shader/uncover.glsl",
		[EV_TRANSITION_EFFECT_DISSOLVE] = "/org/gnome/evince/shader/dissolve.glsl",
		[EV_TRANSITION_EFFECT_PUSH] = "/org/gnome/evince/shader/push.glsl",
		[EV_TRANSITION_EFFECT_BOX] = "/org/gnome/evince/shader/box.glsl",
		[EV_TRANSITION_EFFECT_BLINDS] = "/org/gnome/evince/shader/blinds.glsl",
		[EV_TRANSITION_EFFECT_FLY] = "/org/gnome/evince/shader/fly.glsl",
		[EV_TRANSITION_EFFECT_GLITTER] = "/org/gnome/evince/shader/glitter.glsl",
		[EV_TRANSITION_EFFECT_FADE] = "/org/gnome/evince/shader/fade.glsl",
	};

	static GskGLShader *shaders[G_N_ELEMENTS (shader_resources)] = {};

	if (type >= G_N_ELEMENTS (shader_resources) || !shader_resources[type])
		return NULL;

	if (!shaders[type]) {
		shaders[type] = gsk_gl_shader_new_from_resource (shader_resources[type]);
		g_assert (shaders[type] != NULL);
	}

	return shaders[type];
}

static GBytes *
ev_view_presentation_build_shader_args (GskGLShader *shader,
					EvTransitionEffect *effect,
					gdouble progress)
{
	GskShaderArgsBuilder *builder = gsk_shader_args_builder_new (shader, NULL);
	EvTransitionEffectType type;
	EvTransitionEffectAlignment alignment;
	EvTransitionEffectDirection direction;
	gint angle;
	gdouble scale;

	g_object_get (effect, "type", &type, NULL);

	switch (type) {
	case EV_TRANSITION_EFFECT_SPLIT:
		g_object_get (effect, "direction", &direction,
				"alignment", &alignment, NULL);
		gsk_shader_args_builder_set_int (builder, 1, direction);
		gsk_shader_args_builder_set_int (builder, 2, alignment);
		break;
	case EV_TRANSITION_EFFECT_WIPE:
	case EV_TRANSITION_EFFECT_PUSH:
	case EV_TRANSITION_EFFECT_COVER:
	case EV_TRANSITION_EFFECT_UNCOVER:
	case EV_TRANSITION_EFFECT_GLITTER:
		g_object_get (effect, "angle", &angle, NULL);
		gsk_shader_args_builder_set_int (builder, 1, angle);
		break;
	case EV_TRANSITION_EFFECT_BOX:
		g_object_get (effect, "direction", &direction, NULL);
		gsk_shader_args_builder_set_int (builder, 1, direction);
		break;
	case EV_TRANSITION_EFFECT_BLINDS:
		g_object_get (effect, "alignment", &alignment, NULL);
		gsk_shader_args_builder_set_int (builder, 1, alignment);
		break;
	case EV_TRANSITION_EFFECT_FLY:
		g_object_get (effect, "angle", &angle,
				"scale", &scale, NULL);
		gsk_shader_args_builder_set_int (builder, 1, angle);
		gsk_shader_args_builder_set_float (builder, 2, scale);
		break;
	default:
		g_assert_not_reached ();
	}

	gsk_shader_args_builder_set_float (builder, 0, progress);

	return gsk_shader_args_builder_free_to_args (builder);
}

static void
ev_view_presentation_animation_snapshot (EvViewPresentation *pview,
					 GtkSnapshot *snapshot,
					 graphene_rect_t *area)
{
	GtkNative *native = gtk_widget_get_native (GTK_WIDGET (pview));
	GskRenderer *renderer = gtk_native_get_renderer (native);
	GskGLShader *shader;
	double duration;
	EvTransitionEffectType type;
	GError *error = NULL;

	int width = gtk_widget_get_width (GTK_WIDGET (pview));
	int height = gtk_widget_get_height (GTK_WIDGET (pview));

	EvTransitionEffect *effect = ev_document_transition_get_effect (
					EV_DOCUMENT_TRANSITION (pview->document),
					pview->current_page);
	g_object_get (effect, "duration-real", &duration, "type", &type, NULL);

	shader = ev_view_presentation_load_shader (type);

	gdouble progress = pview->transition_time / duration;

	if (shader && gsk_gl_shader_compile (shader, renderer, &error)) {
		gtk_snapshot_push_gl_shader (snapshot, shader, &GRAPHENE_RECT_INIT (0, 0, width, height),
					ev_view_presentation_build_shader_args (shader, effect, progress));

		// TODO: handle different page size
		if (pview->previous_texture)
			gtk_snapshot_append_texture (snapshot, pview->previous_texture, area);
		else
			gtk_snapshot_append_color (snapshot, &(GdkRGBA){0., 0., 0., 1.}, area);

		gtk_snapshot_gl_shader_pop_texture (snapshot); /* current child */

		gtk_snapshot_append_texture (snapshot, pview->current_texture, area);
		gtk_snapshot_gl_shader_pop_texture (snapshot); /* next child */
		gtk_snapshot_pop(snapshot);
	} else {
		if (error)
			g_warning ("failed to compile shader '%s'\n", error->message);
		else if (type != EV_TRANSITION_EFFECT_REPLACE)
			g_warning ("shader for type %d is not implemented\n", type);

		gtk_snapshot_append_texture (snapshot, pview->current_texture, area);
	}

	g_clear_pointer (&error, g_error_free);
}

static void ev_view_presentation_snapshot(GtkWidget *widget, GtkSnapshot *snapshot)
{
	EvViewPresentation *pview = EV_VIEW_PRESENTATION (widget);
	GtkStyleContext    *context = gtk_widget_get_style_context (widget);
	GdkRectangle        page_area;
	graphene_rect_t     area;

	gtk_snapshot_render_background (snapshot, context, 0, 0,
                               gtk_widget_get_width (widget),
                               gtk_widget_get_height (widget));

	switch (pview->state) {
	case EV_PRESENTATION_END:
		ev_view_presentation_snapshot_end_page (pview, snapshot);
		return;
	case EV_PRESENTATION_BLACK:
	case EV_PRESENTATION_WHITE:
		return;
	case EV_PRESENTATION_NORMAL:
		break;
	}

	if (!pview->curr_job) {
		ev_view_presentation_update_current_page (pview, pview->current_page);
		ev_view_presentation_hide_cursor_timeout_start (pview);
		return;
	}

	if (!pview->current_texture)
		return;

	ev_view_presentation_get_page_area (pview, &page_area);
	area = GRAPHENE_RECT_INIT (page_area.x, page_area.y,
				   page_area.width, page_area.height);

	if (pview->inverted_colors) {
		gtk_snapshot_push_blend (snapshot, GSK_BLEND_MODE_DIFFERENCE);
		gtk_snapshot_append_color (snapshot, &(GdkRGBA) { 1., 1., 1., 1.}, &area);
		gtk_snapshot_pop (snapshot);
	}

	if (EV_IS_DOCUMENT_TRANSITION (pview->document))
		ev_view_presentation_animation_snapshot (pview, snapshot, &area);
	else
		gtk_snapshot_append_texture (snapshot, pview->current_texture, &area);

	if (pview->inverted_colors)
		gtk_snapshot_pop (snapshot);
}

static gboolean
ev_view_presentation_key_press_event (GtkEventControllerKey *self,
				      guint keyval,
				      guint keycode,
				      GdkModifierType state,
				      GtkWidget *widget)
{
	EvViewPresentation *pview = EV_VIEW_PRESENTATION (widget);

	switch (keyval) {
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

	if (ev_document_get_n_pages (pview->document) > 1 && key_is_numeric (keyval)) {
		gint x, y;
		gchar *digit = g_strdup_printf ("%d", key_to_digit (keyval));

		ev_document_misc_get_pointer_position (GTK_WIDGET (pview), &x, &y);
		gtk_popover_set_pointing_to (GTK_POPOVER (pview->goto_popup),
				&(GdkRectangle) {x, y, 1, 1});

		gtk_editable_set_text (GTK_EDITABLE (pview->goto_entry), digit);
		gtk_editable_set_position (GTK_EDITABLE (pview->goto_entry), -1);
		gtk_entry_grab_focus_without_selecting (GTK_ENTRY (pview->goto_entry));
		gtk_popover_popup (GTK_POPOVER (pview->goto_popup));
		g_free (digit);
		return TRUE;
	}

	return FALSE;
}

static gboolean
ev_view_presentation_button_release_event (GtkGestureClick    *self,
					   gint		       n_press,
					   gdouble             x,
					   gdouble             y,
					   GtkWidget          *widget)
{
	EvViewPresentation *pview = EV_VIEW_PRESENTATION (widget);
	GdkEvent *event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (self));

	switch (gdk_button_event_get_button (event)) {
	case GDK_BUTTON_PRIMARY: {
		EvLink *link;

		if (pview->state == EV_PRESENTATION_END) {
			g_signal_emit (pview, signals[FINISHED], 0, NULL);

			return FALSE;
		}

		link = ev_view_presentation_get_link_at_location (pview, x, y);
		if (link)
			ev_view_presentation_handle_link (pview, link);
		else
			ev_view_presentation_next_page (pview);
	}
		break;
	case GDK_BUTTON_SECONDARY:
		ev_view_presentation_previous_page (pview);
		break;
	default:
		break;
	}

	return FALSE;
}

static void
ev_view_presentation_motion_notify_event (GtkEventControllerMotion	*self,
					  gdouble			 x,
					  gdouble			 y,
					  GtkWidget			*widget)
{
	EvViewPresentation *pview = EV_VIEW_PRESENTATION (widget);

	ev_view_presentation_hide_cursor_timeout_start (pview);
	ev_view_presentation_set_cursor_for_location (pview, x, y);
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
ev_view_presentation_scroll_event (GtkEventControllerScroll *self,
				   gdouble dx,
				   gdouble dy,
				   EvViewPresentation *pview)
{
	GtkEventController *controller = GTK_EVENT_CONTROLLER (self);
	GdkEvent *event = gtk_event_controller_get_current_event (controller);
	guint state = gtk_event_controller_get_current_event_state (controller)
			& gtk_accelerator_get_default_mod_mask ();
	if (state != 0)
		return FALSE;

	switch (gdk_scroll_event_get_direction (event)) {
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
add_change_page_binding_keypad (GtkWidgetClass *widget_class,
				guint           keyval,
				GdkModifierType modifiers,
				GtkScrollType   scroll)
{
	guint keypad_keyval = keyval - GDK_KEY_Left + GDK_KEY_KP_Left;

	gtk_widget_class_add_binding_signal (widget_class, keyval, modifiers,
					"change_page", "(i)", scroll);
	gtk_widget_class_add_binding_signal (widget_class, keypad_keyval, modifiers,
					"change_page", "(i)", scroll);
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
        if (!gtk_widget_get_realized (GTK_WIDGET (pview)))
                return;

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
ev_view_presentation_size_allocate (GtkWidget	*widget,
				    int		 width,
				    int		 height,
				    int		 baseline)
{
	EvViewPresentation *pview = EV_VIEW_PRESENTATION (widget);
	GtkWidgetClass *parent_class = GTK_WIDGET_CLASS (
		ev_view_presentation_parent_class);
	parent_class->size_allocate (widget, width, height, baseline);

	if (pview->goto_popup)
		gtk_popover_present (GTK_POPOVER (pview->goto_popup));
}

static void
ev_view_presentation_class_init (EvViewPresentationClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GObjectClass   *gobject_class = G_OBJECT_CLASS (klass);

	klass->change_page = ev_view_presentation_change_page;

        gobject_class->dispose = ev_view_presentation_dispose;

	widget_class->snapshot = ev_view_presentation_snapshot;
	widget_class->size_allocate = ev_view_presentation_size_allocate;

	gtk_widget_class_set_css_name (widget_class, "evpresentationview");

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

	add_change_page_binding_keypad (widget_class, GDK_KEY_Left,  0, GTK_SCROLL_PAGE_BACKWARD);
	add_change_page_binding_keypad (widget_class, GDK_KEY_Right, 0, GTK_SCROLL_PAGE_FORWARD);
	add_change_page_binding_keypad (widget_class, GDK_KEY_Up,    0, GTK_SCROLL_PAGE_BACKWARD);
	add_change_page_binding_keypad (widget_class, GDK_KEY_Down,  0, GTK_SCROLL_PAGE_FORWARD);

	gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_space, 0,
			"change_page", "(i)", GTK_SCROLL_PAGE_FORWARD);
	gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_space, 0,
			"change_page", "(i)", GTK_SCROLL_PAGE_FORWARD);
	gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_space, GDK_SHIFT_MASK,
			"change_page", "(i)", GTK_SCROLL_PAGE_BACKWARD);
	gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_BackSpace, 0,
			"change_page", "(i)", GTK_SCROLL_PAGE_BACKWARD);
	gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_Page_Down, 0,
			"change_page", "(i)", GTK_SCROLL_PAGE_FORWARD);
	gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_Page_Up, 0,
			"change_page", "(i)", GTK_SCROLL_PAGE_BACKWARD);
	gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_J, 0,
			"change_page", "(i)", GTK_SCROLL_PAGE_FORWARD);
	gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_H, 0,
			"change_page", "(i)", GTK_SCROLL_PAGE_BACKWARD);
	gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_L, 0,
			"change_page", "(i)", GTK_SCROLL_PAGE_FORWARD);
	gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_K, 0,
			"change_page", "(i)", GTK_SCROLL_PAGE_BACKWARD);
}

static void
ev_view_presentation_init (EvViewPresentation *pview)
{
	GtkWidget *widget = GTK_WIDGET (pview);
	GtkEventController *controller;

	gtk_widget_set_can_focus (widget, TRUE);
	gtk_widget_set_focusable (widget, TRUE);
	pview->is_constructing = TRUE;

	controller = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
	g_signal_connect (G_OBJECT (controller), "scroll",
			G_CALLBACK (ev_view_presentation_scroll_event), widget);
	gtk_widget_add_controller (widget, controller);

	controller = gtk_event_controller_key_new ();
	g_signal_connect (G_OBJECT (controller), "key-pressed",
			G_CALLBACK (ev_view_presentation_key_press_event), widget);
	gtk_widget_add_controller (widget, controller);

	controller = gtk_event_controller_motion_new ();
	g_signal_connect (G_OBJECT (controller), "motion",
			G_CALLBACK (ev_view_presentation_motion_notify_event), widget);
	gtk_widget_add_controller (widget, controller);

	controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
	g_signal_connect (G_OBJECT (controller), "released",
			G_CALLBACK (ev_view_presentation_button_release_event), widget);
	gtk_widget_add_controller (widget, controller);

	ev_view_presentation_goto_window_create (pview);
}

EvViewPresentation *
ev_view_presentation_new (EvDocument *document,
			  guint       current_page,
			  guint       rotation,
			  gboolean    inverted_colors)
{
	g_return_val_if_fail (EV_IS_DOCUMENT (document), NULL);
	g_return_val_if_fail (current_page < ev_document_get_n_pages (document), NULL);

	return g_object_new (EV_TYPE_VIEW_PRESENTATION,
			     "document", document,
			     "current_page", current_page,
			     "rotation", rotation,
			     "inverted_colors", inverted_colors,
			     NULL);
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
