/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2004 Red Hat, Inc
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

#if !defined (__EV_EVINCE_VIEW_H_INSIDE__) && !defined (EVINCE_COMPILATION)
#error "Only <evince-view.h> can be included directly."
#endif

#ifndef __EV_VIEW_CURSOR_H__
#define __EV_VIEW_CURSOR_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum {
	EV_VIEW_CURSOR_NORMAL,
	EV_VIEW_CURSOR_IBEAM,
	EV_VIEW_CURSOR_LINK,
	EV_VIEW_CURSOR_WAIT,
	EV_VIEW_CURSOR_HIDDEN,
	EV_VIEW_CURSOR_DRAG,
	EV_VIEW_CURSOR_AUTOSCROLL,
	EV_VIEW_CURSOR_ADD
} EvViewCursor;

GdkCursor *ev_view_cursor_new (GdkDisplay  *display,
			       EvViewCursor cursor);

G_END_DECLS

#endif /* __EV_VIEW_CURSOR_H__ */
