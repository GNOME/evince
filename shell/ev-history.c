/*
 *  Copyright (C) 2005 Marco Pesenti Gritti
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  $Id$
 */

#include "config.h"

#include <glib/gi18n.h>

#include "ev-history.h"

struct _EvHistoryPrivate
{
	GList *links;
	int current_index;
};

static void ev_history_init       (EvHistory *history);
static void ev_history_class_init (EvHistoryClass *class);

static GObjectClass *parent_class = NULL;

G_DEFINE_TYPE (EvHistory, ev_history, G_TYPE_OBJECT)

#define EV_HISTORY_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_HISTORY, EvHistoryPrivate))

static void
ev_history_init (EvHistory *history)
{
	history->priv = EV_HISTORY_GET_PRIVATE (history);

	history->priv->links = NULL;
}

static void
free_links_list (GList *l)
{
	g_list_foreach (l, (GFunc)g_object_unref, NULL);
	g_list_free (l);
}

static void
ev_history_finalize (GObject *object)
{
	EvHistory *history = EV_HISTORY (object);

	free_links_list (history->priv->links);

	parent_class->finalize (object);
}

static void
ev_history_class_init (EvHistoryClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = ev_history_finalize;

	parent_class = g_type_class_peek_parent (class);

	g_type_class_add_private (object_class, sizeof (EvHistoryPrivate));
}

void
ev_history_add_link (EvHistory *history, EvLink *link)
{
	int length;

	g_return_if_fail (EV_IS_HISTORY (history));
	g_return_if_fail (EV_IS_LINK (link));

	g_object_ref (link);
	history->priv->links = g_list_append (history->priv->links,
					      link);

	length = g_list_length (history->priv->links);
	history->priv->current_index = length - 1;

	g_print ("Set current\n");
}

void
ev_history_add_page (EvHistory *history, int page)
{
	EvLink *link;
	char *title;

	g_return_if_fail (EV_IS_HISTORY (history));

	title = g_strdup_printf (_("Page %d\n"), page);
	link = ev_link_new_page (title, page);
	g_free (title);

	ev_history_add_link (history, link);
}

EvLink *
ev_history_get_link_nth	(EvHistory *history, int index)
{
	GList *l;

	g_return_val_if_fail (EV_IS_HISTORY (history), NULL);

	l = g_list_nth (history->priv->links, index);

	return EV_LINK (l->data);
}

int
ev_history_get_n_links (EvHistory *history)
{
	g_return_val_if_fail (EV_IS_HISTORY (history), -1);

	return g_list_length (history->priv->links);
}

int
ev_history_get_current_index (EvHistory *history)
{
	g_return_val_if_fail (EV_IS_HISTORY (history), -1);

	return history->priv->current_index;
}

void
ev_history_set_current_index (EvHistory *history, int index)
{
	g_return_if_fail (EV_IS_HISTORY (history));

	history->priv->current_index = index;
}

EvHistory *
ev_history_new (void)
{
	return EV_HISTORY (g_object_new (EV_TYPE_HISTORY, NULL));
}
