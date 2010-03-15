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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include <glib/gi18n.h>
#include <string.h>

#include "ev-history.h"


enum
{
	HISTORY_CHANGED,
	N_SIGNALS
};

static guint signals[N_SIGNALS] = {0, };

struct _EvHistoryPrivate
{
	GList *links;
};

static void ev_history_init       (EvHistory *history);
static void ev_history_class_init (EvHistoryClass *class);

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

	G_OBJECT_CLASS (ev_history_parent_class)->finalize (object);
}

static void
ev_history_class_init (EvHistoryClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = ev_history_finalize;

	signals[HISTORY_CHANGED] = 
		    g_signal_new ("changed",
		 	          G_OBJECT_CLASS_TYPE (object_class),
				  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
				  G_STRUCT_OFFSET (EvHistoryClass, changed),
				  NULL, NULL,
				  g_cclosure_marshal_VOID__VOID,
				  G_TYPE_NONE, 0);

	g_type_class_add_private (object_class, sizeof (EvHistoryPrivate));
}

#define HISTORY_LENGTH   7

void
ev_history_add_link (EvHistory *history, EvLink *link)
{
	GList *l;

	g_return_if_fail (EV_IS_HISTORY (history));
	g_return_if_fail (EV_IS_LINK (link));

	for (l = history->priv->links; l; l = l->next) {
		if (!strcmp (ev_link_get_title (EV_LINK (l->data)), ev_link_get_title (link))) {
			g_object_unref (G_OBJECT (l->data));
			history->priv->links = g_list_delete_link (history->priv->links, l);
			break;
		}
	}

	g_object_ref (link);
	history->priv->links = g_list_append (history->priv->links,
					      link);
					      
	if (g_list_length (history->priv->links) > HISTORY_LENGTH) {
		g_object_unref (G_OBJECT (history->priv->links->data));
		history->priv->links = g_list_delete_link (history->priv->links, 
							   history->priv->links);
	}
	
	g_signal_emit (history, signals[HISTORY_CHANGED], 0);
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

EvHistory *
ev_history_new (void)
{
	return EV_HISTORY (g_object_new (EV_TYPE_HISTORY, NULL));
}

