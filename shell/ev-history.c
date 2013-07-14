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

enum {
	CHANGED,
        ACTIVATE_LINK,

	N_SIGNALS
};

#define EV_HISTORY_MAX_LENGTH (32)

static guint signals[N_SIGNALS] = {0, };

struct _EvHistoryPrivate {
	GList           *list;
        GList           *current;

        EvDocumentModel *model;
        gulong           page_changed_handler_id;

        guint            frozen;
};

G_DEFINE_TYPE (EvHistory, ev_history, G_TYPE_OBJECT)

static void ev_history_set_model (EvHistory       *history,
                                  EvDocumentModel *model);

static void
clear_list (GList *list)
{
        g_list_free_full (list, (GDestroyNotify) g_object_unref);
}

static void
ev_history_clear (EvHistory *history)
{
        clear_list (history->priv->list);
        history->priv->list = NULL;

        history->priv->current = NULL;
}

static void
ev_history_prune (EvHistory *history)
{
        EvHistoryPrivate *priv = history->priv;
        GList *l;
        guint i;

        g_assert (priv->current->next == NULL);

        for (i = 0, l = priv->current; i < EV_HISTORY_MAX_LENGTH && l != NULL; i++, l = l->prev)
                /* empty */;

        if (l == NULL)
                return;

        /* Throw away all history up to @l */
        l = l->next;
        l->prev->next = NULL;
        l->prev = NULL;

        clear_list (priv->list);
        priv->list = l;

        g_assert (g_list_length (priv->list) == EV_HISTORY_MAX_LENGTH);
}

static void
ev_history_finalize (GObject *object)
{
	EvHistory *history = EV_HISTORY (object);

        ev_history_clear (history);
        ev_history_set_model (history, NULL);

	G_OBJECT_CLASS (ev_history_parent_class)->finalize (object);
}

static void
ev_history_class_init (EvHistoryClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = ev_history_finalize;

	signals[CHANGED] =
                g_signal_new ("changed",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                              G_STRUCT_OFFSET (EvHistoryClass, changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);

        signals[ACTIVATE_LINK] =
                g_signal_new ("activate-link",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                              G_STRUCT_OFFSET (EvHistoryClass, activate_link),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__OBJECT,
                              G_TYPE_NONE, 1,
                              G_TYPE_OBJECT);

	g_type_class_add_private (object_class, sizeof (EvHistoryPrivate));
}

static void
ev_history_init (EvHistory *history)
{
	history->priv = G_TYPE_INSTANCE_GET_PRIVATE (history, EV_TYPE_HISTORY, EvHistoryPrivate);
}

gboolean
ev_history_is_frozen (EvHistory *history)
{
        return history->priv->frozen > 0;
}

void
ev_history_add_link (EvHistory *history,
                     EvLink    *link)
{
        EvHistoryPrivate *priv;

	g_return_if_fail (EV_IS_HISTORY (history));
	g_return_if_fail (EV_IS_LINK (link));

        if (ev_history_is_frozen (history))
                return;

        priv = history->priv;

        if (priv->current) {
                /* Truncate forward history at @current */
                clear_list (priv->current->next);
                priv->current->next = NULL;
        }

        /* Push @link to the list */
        priv->current = g_list_append (NULL, g_object_ref (link));
        priv->list = g_list_concat (priv->list, priv->current);

        ev_history_prune (history);

	g_signal_emit (history, signals[CHANGED], 0);
}

static void
ev_history_activate_current_link (EvHistory *history)
{
        g_assert (history->priv->current);

        ev_history_freeze (history);
        g_signal_handler_block (history->priv->model, history->priv->page_changed_handler_id);
        g_signal_emit (history, signals[ACTIVATE_LINK], 0, history->priv->current->data);
        g_signal_handler_unblock (history->priv->model, history->priv->page_changed_handler_id);
        ev_history_thaw (history);

        g_signal_emit (history, signals[CHANGED], 0);
}

gboolean
ev_history_can_go_back (EvHistory *history)
{
        EvHistoryPrivate *priv;

        g_return_val_if_fail (EV_IS_HISTORY (history), FALSE);

        if (ev_history_is_frozen (history))
                return FALSE;

        priv = history->priv;
        return priv->current && priv->current->prev;
}

void
ev_history_go_back (EvHistory *history)
{
        EvHistoryPrivate *priv;

        g_return_if_fail (EV_IS_HISTORY (history));

        if (!ev_history_can_go_back (history))
                return;

        priv = history->priv;

        /* Move current back one step */
        priv->current = priv->current->prev;

        ev_history_activate_current_link (history);
}

gboolean
ev_history_can_go_forward (EvHistory *history)
{
        EvHistoryPrivate *priv;

        g_return_val_if_fail (EV_IS_HISTORY (history), FALSE);

        if (ev_history_is_frozen (history))
                return FALSE;

        priv = history->priv;
        return priv->current && priv->current->next;
}

void
ev_history_go_forward (EvHistory *history)
{
        EvHistoryPrivate *priv;

        g_return_if_fail (EV_IS_HISTORY (history));

        if (!ev_history_can_go_forward (history))
                return;

        priv = history->priv;

        /* Move current forward one step */
        priv->current = priv->current->next;

        ev_history_activate_current_link (history);
}

static gint
compare_link (EvLink *a,
              EvLink *b)
{
        if (a == b)
                return 0;

        return ev_link_action_equal (ev_link_get_action (a), ev_link_get_action (b)) ? 0 : 1;
}

/*
 * ev_history_go_to_link:
 * @history: a #EvHistory
 * @link: a #EvLink
 *
 * Goes to the link, if it is in the history.
 *
 * Returns: %TRUE if the link was in the history and history isn't frozen; %FALSE otherwise
 */
gboolean
ev_history_go_to_link (EvHistory *history,
                       EvLink    *link)
{
        EvHistoryPrivate *priv;
        GList *l;

        g_return_val_if_fail (EV_IS_HISTORY (history), FALSE);
        g_return_val_if_fail (EV_IS_LINK (link), FALSE);

        if (ev_history_is_frozen (history))
                return FALSE;

        priv = history->priv;

        l = g_list_find_custom (priv->list, link, (GCompareFunc) compare_link);
        if (l == NULL)
                return FALSE;

        /* Set the link as current */
        priv->current = l;

        ev_history_activate_current_link (history);

        return TRUE;
}

/**
 * ev_history_get_back_list:
 * @history: a #EvHistory
 *
 * Returns: (transfer container): the back history
 */
GList *
ev_history_get_back_list (EvHistory *history)
{
        EvHistoryPrivate *priv;
        GList *list, *l;

        g_return_val_if_fail (EV_IS_HISTORY (history), NULL);

        priv = history->priv;

        if (priv->current == NULL)
                return NULL;

        list = NULL;
        for (l = priv->current->prev; l != NULL; l = l->prev)
                list = g_list_prepend (list, l->data);

        return g_list_reverse (list);
}

/**
 * ev_history_get_forward_list:
 * @history: a #EvHistory
 *
 * Returns: (transfer container): the forward history
 */
GList *
ev_history_get_forward_list (EvHistory *history)
{
        g_return_val_if_fail (EV_IS_HISTORY (history), NULL);

        return g_list_copy (history->priv->current->next);
}

void
ev_history_freeze (EvHistory *history)
{
        g_return_if_fail (EV_IS_HISTORY (history));

        history->priv->frozen++;
}

void
ev_history_thaw (EvHistory *history)
{
        g_return_if_fail (EV_IS_HISTORY (history));
        g_return_if_fail (history->priv->frozen > 0);

        history->priv->frozen--;
}

static gint
ev_history_get_current_page (EvHistory *history)
{
        EvLink       *link;
        EvDocument   *document;
        EvLinkDest   *dest;
        EvLinkAction *action;

        if (!history->priv->current)
                return -1;

        link = history->priv->current->data;
        action = ev_link_get_action (link);
        if (!action)
                return -1;

        dest = ev_link_action_get_dest (action);
        if (!dest)
                return -1;

        switch (ev_link_dest_get_dest_type (dest)) {
        case EV_LINK_DEST_TYPE_NAMED:
                document = ev_document_model_get_document (history->priv->model);
                if (!EV_IS_DOCUMENT_LINKS (document))
                        return -1;

                return ev_document_links_find_link_page (EV_DOCUMENT_LINKS (document),
                                                         ev_link_dest_get_named_dest (dest));
        case EV_LINK_DEST_TYPE_PAGE_LABEL: {
                gint page = -1;

                document = ev_document_model_get_document (history->priv->model);
                ev_document_find_page_by_label (document,
                                                ev_link_dest_get_page_label (dest),
                                                &page);

                return page;
        }
        default:
                return ev_link_dest_get_page (dest);
        }

        return -1;
}

static void
ev_history_add_link_for_page (EvHistory *history,
                              gint       page)
{
        EvDocument   *document;
        EvLinkDest   *dest;
        EvLinkAction *action;
        EvLink       *link;
        gchar        *page_label;
        gchar        *title;

        if (ev_history_is_frozen (history))
                return;

        if (ev_history_get_current_page (history) == page)
                return;

        document = ev_document_model_get_document (history->priv->model);
        if (!document)
                return;

        page_label = ev_document_get_page_label (document, page);
        if (!page_label)
                return;

        title = g_strdup_printf (_("Page %s"), page_label);
        g_free (page_label);

        dest = ev_link_dest_new_page (page);
        action = ev_link_action_new_dest (dest);
        g_object_unref (dest);

        link = ev_link_new (title, action);
        g_object_unref (action);
        g_free (title);

        ev_history_add_link (history, link);
        g_object_unref (link);
}

static void
page_changed_cb (EvDocumentModel *model,
                 gint             old_page,
                 gint             new_page,
                 EvHistory       *history)
{
        if (ABS (new_page - old_page) > 1)
                ev_history_add_link_for_page (history, new_page);
}

static void
document_changed_cb (EvDocumentModel *model,
                     GParamSpec      *pspec,
                     EvHistory       *history)
{
        ev_history_clear (history);
        ev_history_add_link_for_page (history, ev_document_model_get_page (model));
}

static void
ev_history_set_model (EvHistory       *history,
                      EvDocumentModel *model)
{
        if (history->priv->model == model)
                return;

        if (history->priv->model) {
                g_object_remove_weak_pointer (G_OBJECT (history->priv->model),
                                              (gpointer)&history->priv->model);

                if (history->priv->page_changed_handler_id) {
                        g_signal_handler_disconnect (history->priv->model,
                                                     history->priv->page_changed_handler_id);
                        history->priv->page_changed_handler_id = 0;
                }
        }

        history->priv->model = model;
        if (!model)
                return;

        g_object_add_weak_pointer (G_OBJECT (model),
                                   (gpointer)&history->priv->model);

        g_signal_connect (history->priv->model, "notify::document",
                          G_CALLBACK (document_changed_cb),
                          history);
        history->priv->page_changed_handler_id =
                g_signal_connect (history->priv->model, "page-changed",
                                  G_CALLBACK (page_changed_cb),
                                  history);
}

EvHistory *
ev_history_new (EvDocumentModel *model)
{
        EvHistory *history;

        g_return_val_if_fail (EV_IS_DOCUMENT_MODEL (model), NULL);

        history = EV_HISTORY (g_object_new (EV_TYPE_HISTORY, NULL));
        ev_history_set_model (history, model);

        return history;
}
