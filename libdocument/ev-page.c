/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2008 Carlos Garcia Campos <carlosgc@gnome.org>
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

#include <config.h>

#include "ev-page.h"

G_DEFINE_TYPE (EvPage, ev_page, G_TYPE_OBJECT)

static void
ev_page_init (EvPage *page)
{
}

static void
ev_page_finalize (GObject *object)
{
	EvPage *page = EV_PAGE (object);

	if (page->backend_destroy_func) {
		page->backend_destroy_func (page->backend_page);
		page->backend_destroy_func = NULL;
	}
	page->backend_page = NULL;

	(* G_OBJECT_CLASS (ev_page_parent_class)->finalize) (object);
}

static void
ev_page_class_init (EvPageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = ev_page_finalize;
}

EvPage *
ev_page_new (gint index)
{
	EvPage *page;

	page = EV_PAGE (g_object_new (EV_TYPE_PAGE, NULL));
	page->index = index;

	return page;
}
