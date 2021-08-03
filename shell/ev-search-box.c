/* ev-search-box.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2015 Igalia S.L.
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
#include "ev-search-box.h"

#include <glib/gi18n.h>

enum {
        STARTED,
        UPDATED,
        FINISHED,
        CLEARED,

        NEXT,
        PREVIOUS,

        LAST_SIGNAL
};

enum
{
        PROP_0,

        PROP_DOCUMENT_MODEL,
        PROP_OPTIONS
};

typedef struct {
        EvDocumentModel *model;
        EvJob           *job;
        EvFindOptions    options;
        EvFindOptions    supported_options;

        GtkWidget       *entry;
        GtkWidget       *next_button;
        GtkWidget       *prev_button;
        GtkWidget       *progress;

        guint            pages_searched;
} EvSearchBoxPrivate;

static void ev_search_box_buildable_iface_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (EvSearchBox, ev_search_box, ADW_TYPE_BIN,
                         G_ADD_PRIVATE (EvSearchBox)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                ev_search_box_buildable_iface_init))

#define GET_PRIVATE(o) ev_search_box_get_instance_private(o)

static guint signals[LAST_SIGNAL] = { 0 };

#define FIND_PAGE_RATE_REFRESH 100

static void
ev_search_box_update_progress (EvSearchBox *box)
{
        EvSearchBoxPrivate *priv = GET_PRIVATE (box);
        gdouble fraction;

        fraction = priv->job ? MIN ((gdouble)priv->pages_searched / EV_JOB_FIND (priv->job)->n_pages, 1.) : 0.;
        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (priv->progress), fraction);
}

static void
ev_search_box_clear_job (EvSearchBox *box)
{
        EvSearchBoxPrivate *priv = GET_PRIVATE (box);

        if (!priv->job)
                return;

        if (!ev_job_is_finished (priv->job))
                ev_job_cancel (priv->job);

        g_signal_handlers_disconnect_matched (priv->job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, box);
	g_clear_object (&priv->job);
}

static void
find_job_finished_cb (EvJobFind   *job,
                      EvSearchBox *box)
{
        g_signal_emit (box, signals[FINISHED], 0);
        ev_search_box_clear_job (box);
        ev_search_box_update_progress (box);

        if (!ev_job_find_has_results (job)) {
                EvSearchBoxPrivate *priv = GET_PRIVATE (box);

		gtk_widget_add_css_class (priv->entry, "error");
        }
}

/**
 * find_bar_check_refresh_rate:
 *
 * Check whether the current page should trigger an status update in the
 * find bar given its document size and the rate page.
 *
 * For documents with less pages than page_rate, it will return TRUE for
 * every page.  For documents with more pages, it will return TRUE every
 * ((total_pages / page rate) + 1).
 *
 * This slow down the update rate in the GUI, making the search more
 * responsive.
 */
static inline gboolean
find_check_refresh_rate (EvJobFind *job,
                         gint       page_rate)
{
        /* Always update if this is the last page of the search */
        if ((job->current_page + 1) % job->n_pages == job->start_page)
                return TRUE;

        return ((job->current_page % (gint)((job->n_pages / page_rate) + 1)) == 0);
}

static void
find_job_updated_cb (EvJobFind   *job,
                     gint         page,
                     EvSearchBox *box)
{
        EvSearchBoxPrivate *priv = GET_PRIVATE (box);

        priv->pages_searched++;

        /* Adjust the status update when searching for a term according
         * to the document size in pages. For documents smaller (or equal)
         * than 100 pages, it will be updated in every page. A value of
         * 100 is enough to update the find bar every 1%.
         */
        if (find_check_refresh_rate (job, FIND_PAGE_RATE_REFRESH)) {
                gboolean has_results = ev_job_find_has_results (job);

                ev_search_box_update_progress (box);
                gtk_widget_set_sensitive (priv->next_button, has_results);
                gtk_widget_set_sensitive (priv->prev_button, has_results);
                g_signal_emit (box, signals[UPDATED], 0, page);
        }
}

static void
search_changed_cb (GtkSearchEntry *entry,
                   EvSearchBox    *box)
{
        const char *search_string;
        EvSearchBoxPrivate *priv = GET_PRIVATE (box);

        ev_search_box_clear_job (box);
        priv->pages_searched = 0;
        ev_search_box_update_progress (box);

        gtk_widget_set_sensitive (priv->next_button, FALSE);
        gtk_widget_set_sensitive (priv->prev_button, FALSE);

	gtk_widget_remove_css_class(priv->entry, "error");

        search_string = gtk_editable_get_text (GTK_EDITABLE (entry));

        if (search_string && search_string[0]) {
                EvDocument *doc = ev_document_model_get_document (priv->model);

                priv->job = ev_job_find_new (doc,
                                             ev_document_model_get_page (priv->model),
                                             ev_document_get_n_pages (doc),
                                             search_string,
					     priv->options);
                g_signal_connect (priv->job, "finished",
                                  G_CALLBACK (find_job_finished_cb),
                                  box);
                g_signal_connect (priv->job, "updated",
                                  G_CALLBACK (find_job_updated_cb),
                                  box);

                g_signal_emit (box, signals[STARTED], 0, priv->job);
                ev_job_scheduler_push_job (priv->job, EV_JOB_PRIORITY_NONE);
        } else {
                g_signal_emit (box, signals[CLEARED], 0);
        }
}

static void
previous_clicked_cb (GtkButton   *button,
                     EvSearchBox *box)
{
        g_signal_emit (box, signals[PREVIOUS], 0);
}

static void
next_clicked_cb (GtkButton   *button,
                 EvSearchBox *box)
{
        g_signal_emit (box, signals[NEXT], 0);
}

static void
ev_search_box_set_supported_options (EvSearchBox  *box,
                                     EvFindOptions options)
{
        EvSearchBoxPrivate *priv = GET_PRIVATE (box);

        priv->supported_options = options;
}

static void
ev_search_box_setup_document (EvSearchBox *box,
                              EvDocument  *document)
{
        if (!document || !EV_IS_DOCUMENT_FIND (document)) {
                ev_search_box_set_supported_options (box, EV_FIND_DEFAULT);
                gtk_widget_set_sensitive (GTK_WIDGET (box), FALSE);
                return;
        }

        ev_search_box_set_supported_options (box, ev_document_find_get_supported_options (EV_DOCUMENT_FIND (document)));
        gtk_widget_set_sensitive (GTK_WIDGET (box), ev_document_get_n_pages (document) > 0);
}

static void
document_changed_cb (EvDocumentModel *model,
                     GParamSpec      *pspec,
                     EvSearchBox     *box)
{
        ev_search_box_setup_document (box, ev_document_model_get_document (model));
}

static void
ev_search_box_set_options (EvSearchBox  *box,
                           EvFindOptions options)
{
        EvSearchBoxPrivate *priv = GET_PRIVATE (box);

        if (priv->options == options)
                return;

        priv->options = options;
        search_changed_cb (GTK_SEARCH_ENTRY (priv->entry), box);
}


static void
whole_words_only_toggled_cb (GtkCheckButton	*button,
                             EvSearchBox	*box)
{
        EvSearchBoxPrivate *priv = GET_PRIVATE (box);
        EvFindOptions options = priv->options;

        if (gtk_check_button_get_active (button))
                options |= EV_FIND_WHOLE_WORDS_ONLY;
        else
                options &= ~EV_FIND_WHOLE_WORDS_ONLY;
        ev_search_box_set_options (box, options);
}

static void
case_sensitive_toggled_cb (GtkCheckButton	*button,
                           EvSearchBox		*box)
{
        EvSearchBoxPrivate *priv = GET_PRIVATE (box);
        EvFindOptions options = priv->options;

        if (gtk_check_button_get_active (button))
                options |= EV_FIND_CASE_SENSITIVE;
        else
                options &= ~EV_FIND_CASE_SENSITIVE;
        ev_search_box_set_options (box, options);
}

static void
entry_activate_cb (GtkEntry    *entry,
                   EvSearchBox *box)
{
        g_signal_emit (box, signals[NEXT], 0);
}

static void
entry_next_match_cb (GtkSearchEntry *entry,
                     EvSearchBox *box)
{
        g_signal_emit (box, signals[NEXT], 0);
}

static void
entry_previous_match_cb (GtkSearchEntry *entry,
                         EvSearchBox *box)
{
        g_signal_emit (box, signals[PREVIOUS], 0);
}

static void
ev_search_box_finalize (GObject *object)
{
        EvSearchBox *box = EV_SEARCH_BOX (object);
        EvSearchBoxPrivate *priv = GET_PRIVATE (box);

        if (priv->model) {
                g_object_remove_weak_pointer (G_OBJECT (priv->model),
                                              (gpointer)&priv->model);
        }

        G_OBJECT_CLASS (ev_search_box_parent_class)->finalize (object);
}

static void
ev_search_box_dispose (GObject *object)
{
        EvSearchBox *box = EV_SEARCH_BOX (object);

        ev_search_box_clear_job (box);

        G_OBJECT_CLASS (ev_search_box_parent_class)->dispose (object);
}

static void
ev_search_box_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
        EvSearchBox *box = EV_SEARCH_BOX (object);
        EvSearchBoxPrivate *priv = GET_PRIVATE (box);

        switch (prop_id) {
        case PROP_DOCUMENT_MODEL:
                priv->model = g_value_get_object (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
ev_search_box_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
        EvSearchBox *box = EV_SEARCH_BOX (object);
        EvSearchBoxPrivate *priv = GET_PRIVATE (box);

        switch (prop_id) {
        case PROP_OPTIONS:
                g_value_set_flags (value, priv->options);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
ev_search_box_constructed (GObject *object)
{
        EvSearchBox *box = EV_SEARCH_BOX (object);
        EvSearchBoxPrivate *priv = GET_PRIVATE (box);

        G_OBJECT_CLASS (ev_search_box_parent_class)->constructed (object);

        g_object_add_weak_pointer (G_OBJECT (priv->model),
                                   (gpointer)&priv->model);

        ev_search_box_setup_document (box, ev_document_model_get_document (priv->model));
        g_signal_connect_object (priv->model, "notify::document",
                                 G_CALLBACK (document_changed_cb),
                                 box, 0);
}

static gboolean
ev_search_box_grab_focus (GtkWidget *widget)
{
        EvSearchBox *box = EV_SEARCH_BOX (widget);
        EvSearchBoxPrivate *priv = GET_PRIVATE (box);

        return gtk_widget_grab_focus (priv->entry);
}

static void
ev_search_box_class_init (EvSearchBoxClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = ev_search_box_finalize;
        object_class->dispose = ev_search_box_dispose;
        object_class->constructed = ev_search_box_constructed;
        object_class->set_property = ev_search_box_set_property;
        object_class->get_property = ev_search_box_get_property;

        widget_class->grab_focus = ev_search_box_grab_focus;

	gtk_widget_class_set_template_from_resource (widget_class,
		"/org/gnome/evince/ui/search-box.ui");
	gtk_widget_class_bind_template_child_private (widget_class, EvSearchBox, entry);
	gtk_widget_class_bind_template_child_private (widget_class, EvSearchBox, progress);
	gtk_widget_class_bind_template_child_private (widget_class, EvSearchBox, prev_button);
	gtk_widget_class_bind_template_child_private (widget_class, EvSearchBox, next_button);

	gtk_widget_class_bind_template_callback (widget_class, case_sensitive_toggled_cb);
	gtk_widget_class_bind_template_callback (widget_class, whole_words_only_toggled_cb);

	gtk_widget_class_bind_template_callback (widget_class, search_changed_cb);
	gtk_widget_class_bind_template_callback (widget_class, entry_activate_cb);
	gtk_widget_class_bind_template_callback (widget_class, entry_next_match_cb);
	gtk_widget_class_bind_template_callback (widget_class, entry_previous_match_cb);
	gtk_widget_class_bind_template_callback (widget_class, previous_clicked_cb);
	gtk_widget_class_bind_template_callback (widget_class, next_clicked_cb);

	g_object_class_install_property (object_class,
                                         PROP_DOCUMENT_MODEL,
                                         g_param_spec_object ("document-model",
                                                              "DocumentModel",
                                                              "The document model",
                                                              EV_TYPE_DOCUMENT_MODEL,
                                                              G_PARAM_WRITABLE |
                                                              G_PARAM_CONSTRUCT_ONLY |
                                                              G_PARAM_STATIC_STRINGS));
        g_object_class_install_property (object_class,
                                         PROP_OPTIONS,
                                         g_param_spec_flags ("options",
                                                             "Search options",
                                                             "The search options",
                                                             EV_TYPE_FIND_OPTIONS,
                                                             EV_FIND_DEFAULT,
                                                             G_PARAM_READABLE |
                                                             G_PARAM_STATIC_STRINGS));

        signals[STARTED] =
                g_signal_new ("started",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_LAST,
                              0, NULL, NULL,
                              g_cclosure_marshal_VOID__OBJECT,
                              G_TYPE_NONE, 1,
                              EV_TYPE_JOB_FIND);

        signals[UPDATED] =
                g_signal_new ("updated",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_LAST,
                              0, NULL, NULL,
                              g_cclosure_marshal_VOID__INT,
                              G_TYPE_NONE, 1,
                              G_TYPE_INT);
        signals[FINISHED] =
                g_signal_new ("finished",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_LAST,
                              0, NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
        signals[CLEARED] =
                g_signal_new ("cleared",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_LAST,
                              0, NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
        signals[NEXT] =
                g_signal_new ("next",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                              0, NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
        signals[PREVIOUS] =
                g_signal_new ("previous",
                              G_OBJECT_CLASS_TYPE (object_class),
                              G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                              0, NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);


        gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_Return, GDK_SHIFT_MASK,
                                      "previous", NULL);
        gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_ISO_Enter, GDK_SHIFT_MASK,
                                      "previous", NULL);
        gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_KP_Enter, GDK_SHIFT_MASK,
                                      "previous", NULL);
        gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_Up, GDK_CONTROL_MASK,
                                      "previous", NULL);
        gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_Down, GDK_CONTROL_MASK,
                                      "next", NULL);
}

static void
ev_search_box_init (EvSearchBox *box)
{
	gtk_widget_init_template (GTK_WIDGET (box));
}

GtkWidget *
ev_search_box_new (EvDocumentModel *model)
{
        g_return_val_if_fail (EV_IS_DOCUMENT_MODEL (model), NULL);

        return GTK_WIDGET (g_object_new (EV_TYPE_SEARCH_BOX,
                                         "document-model", model,
                                         NULL));
}

GtkSearchEntry *
ev_search_box_get_entry (EvSearchBox *box)
{
        EvSearchBoxPrivate *priv;

        g_return_val_if_fail (EV_IS_SEARCH_BOX (box), NULL);

	priv = GET_PRIVATE (box);

        return GTK_SEARCH_ENTRY (priv->entry);
}

gboolean
ev_search_box_has_results (EvSearchBox *box)
{
        EvSearchBoxPrivate *priv;

        g_return_val_if_fail (EV_IS_SEARCH_BOX (box), FALSE);

	priv = GET_PRIVATE (box);

        return gtk_widget_get_sensitive (priv->next_button);
}

void
ev_search_box_restart (EvSearchBox *box)
{
        EvSearchBoxPrivate *priv;

        g_return_if_fail (EV_IS_SEARCH_BOX (box));

	priv = GET_PRIVATE (box);

        search_changed_cb (GTK_SEARCH_ENTRY (priv->entry), box);
}


static GtkBuildableIface *parent_buildable_iface;

static GObject *
ev_search_box_buildable_get_internal_child (GtkBuildable *buildable,
                             GtkBuilder   *builder,
                             const char   *childname)
{
        EvSearchBox *box = EV_SEARCH_BOX (buildable);

        if (strcmp (childname, "entry") == 0)
                return G_OBJECT (ev_search_box_get_entry (box));

        return parent_buildable_iface->get_internal_child (buildable, builder, childname);
}

static void
ev_search_box_buildable_iface_init (GtkBuildableIface *iface)
{
        parent_buildable_iface = g_type_interface_peek_parent (iface);

        iface->get_internal_child = ev_search_box_buildable_get_internal_child;
}
