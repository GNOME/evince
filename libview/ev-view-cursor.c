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

#include "ev-view-cursor.h"

static const gchar *cursors[] = {
	[EV_VIEW_CURSOR_NORMAL]		= NULL,
	[EV_VIEW_CURSOR_IBEAM]		= "text",
	[EV_VIEW_CURSOR_LINK]		= "pointer",
	[EV_VIEW_CURSOR_WAIT]		= "wait",
	[EV_VIEW_CURSOR_HIDDEN]		= "none",
	[EV_VIEW_CURSOR_DRAG]		= "grabbing",
	[EV_VIEW_CURSOR_AUTOSCROLL]	= "move",
	[EV_VIEW_CURSOR_ADD]		= "crosshair",
};

const gchar *
ev_view_cursor_name (EvViewCursor cursor)
{
	if (cursor < G_N_ELEMENTS (cursors))
		return cursors[cursor];

	return NULL;
}
