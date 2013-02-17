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

static guint signals[N_SIGNALS] = {0, };

struct _EvHistoryPrivate {
        EvLink          *current;
	GList           *back_list;
        GList           *forward_list;

        EvDocumentModel *model;
        gulong           page_changed_handler_id;
        gboolean         activating_current_link;
};

G_DEFINE_TYPE (EvHistory, ev_history, G_TYPE_OBJECT)

static void ev_history_set_model (EvHistory       *history,
                                  EvDocumentModel *model);

static void
ev_history_clear (EvHistory *history)
{
        g_clear_object (&history->priv->current);

        g_list_free_full (history->priv->back_list, (GDestroyNotify)g_object_unref);
        history->priv->back_list = NULL;

        g_list_free_full (history->priv->forward_list, (GDestroyNotify)g_object_unref);
        history->priv->forward_list = NULL;
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

void
ev_history_add_link (EvHistory *history,
                     EvLink    *link)
{
	g_return_if_fail (EV_IS_HISTORY (history));
	g_return_if_fail (EV_IS_LINK (link));

        if (history->priv->activating_current_link)
                return;

        if (ev_history_go_to_link (history, link))
            return;

        if (history->priv->current) {
                history->priv->back_list = g_list_prepend (history->priv->back_list,
                                                           history->priv->current);
        }

        history->priv->current = g_object_ref (link);

        g_list_free_full (history->priv->forward_list, (GDestroyNotify)g_object_unref);
        history->priv->forward_list = NULL;

        /* TODO: Decide a history limit and delete old links when the limit is reached */

	g_signal_emit (history, signals[CHANGED], 0);
}

static void
ev_history_activate_current_link (EvHistory *history)
{
        history->priv->activating_current_link = TRUE;
        g_signal_handler_block (history->priv->model, history->priv->page_changed_handler_id);
        g_signal_emit (history, signals[ACTIVATE_LINK], 0, history->priv->current);
        g_signal_handler_unblock (history->priv->model, history->priv->page_changed_handler_id);
        history->priv->activating_current_link = FALSE;

        g_signal_emit (history, signals[CHANGED], 0);
}

gboolean
ev_history_can_go_back (EvHistory *history)
{
        g_return_val_if_fail (EV_IS_HISTORY (history), FALSE);

        return history->priv->back_list != NULL;
}

void
ev_history_go_back (EvHistory *history)
{
        g_return_if_fail (EV_IS_HISTORY (history));

        if (!history->priv->current || !history->priv->back_list)
                return;

        history->priv->forward_list = g_list_prepend (history->priv->forward_list,
                                                      history->priv->current);
        history->priv->current = EV_LINK (history->priv->back_list->data);
        history->priv->back_list = g_list_delete_link (history->priv->back_list,
                                                       history->priv->back_list);

        ev_history_activate_current_link (history);
}

gboolean
ev_history_can_go_forward (EvHistory *history)
{
        g_return_val_if_fail (EV_IS_HISTORY (history), FALSE);

        return history->priv->forward_list != NULL;
}

void
ev_history_go_forward (EvHistory *history)
{
        g_return_if_fail (EV_IS_HISTORY (history));

        if (!history->priv->current || !history->priv->forward_list)
                return;

        history->priv->back_list = g_list_prepend (history->priv->back_list,
                                                   history->priv->current);
        history->priv->current = EV_LINK (history->priv->forward_list->data);
        history->priv->forward_list = g_list_delete_link (history->priv->forward_list,
                                                          history->priv->forward_list);

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

gboolean
ev_history_go_to_link (EvHistory *history,
                       EvLink    *link)
{
        GList *l;

        g_return_val_if_fail (EV_IS_HISTORY (history), FALSE);
        g_return_val_if_fail (EV_IS_LINK (link), FALSE);

        if (!history->priv->current || (!history->priv->back_list && !history->priv->forward_list))
                return FALSE;

        l = g_list_find_custom (history->priv->back_list, link, (GCompareFunc)compare_link);
        if (l) {
                if (l->next)
                        l->next->prev = NULL;
                if (l->prev)
                        l->prev->next = NULL;

                history->priv->forward_list = g_list_prepend (history->priv->forward_list,
                                                              history->priv->current);
                history->priv->forward_list = g_list_concat (g_list_reverse (history->priv->back_list),
                                                             history->priv->forward_list);
                history->priv->back_list = l->next;
                history->priv->current = EV_LINK (l->data);
                g_list_free_1 (l);

                ev_history_activate_current_link (history);

                return TRUE;
        }

        l = g_list_find_custom (history->priv->forward_list, link, (GCompareFunc)compare_link);
        if (l) {
                if (l->next)
                        l->next->prev = NULL;
                if (l->prev)
                        l->prev->next = NULL;

                history->priv->back_list = g_list_prepend (history->priv->back_list,
                                                           history->priv->current);
                history->priv->back_list = g_list_concat (g_list_reverse (history->priv->forward_list),
                                                          history->priv->back_list);
                history->priv->forward_list = l->next;
                history->priv->current = EV_LINK (l->data);
                g_list_free_1 (l);

                ev_history_activate_current_link (history);

                return TRUE;
        }

        return FALSE;
}

GList *
ev_history_get_back_list (EvHistory *history)
{
        g_return_val_if_fail (EV_IS_HISTORY (history), NULL);

        return history->priv->back_list;
}

GList *
ev_history_get_forward_list (EvHistory *history)
{
        g_return_val_if_fail (EV_IS_HISTORY (history), NULL);

        return history->priv->forward_list;
}

static gint
ev_history_get_current_page (EvHistory *history)
{
        EvDocument   *document;
        EvLinkDest   *dest;
        EvLinkAction *action;

        if (!history->priv->current)
                return -1;

        action = ev_link_get_action (history->priv->current);
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
