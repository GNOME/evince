/* ev-annotations-toolbar.c
 *  this file is part of evince, a gnome document viewer
 *
 * Copyright (C) 2015 Carlos Garcia Campos  <carlosgc@gnome.org>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "ev-annotations-toolbar.h"
#include <evince-document.h>
#include <glib/gi18n.h>

enum {
        BEGIN_ADD_ANNOT,
        CANCEL_ADD_ANNOT,
        N_SIGNALS
};

struct _EvAnnotationsToolbar {
	GtkBox base_instance;

        GtkWidget *text_button;
        GtkWidget *highlight_button;
};

static guint signals[N_SIGNALS];

G_DEFINE_TYPE (EvAnnotationsToolbar, ev_annotations_toolbar, GTK_TYPE_BOX)

static void
ev_annotations_toolbar_annot_button_toggled (GtkWidget            *button,
                                             EvAnnotationsToolbar *toolbar)
{
        EvAnnotationType annot_type;

        if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button))) {
                g_signal_emit (toolbar, signals[CANCEL_ADD_ANNOT], 0, NULL);
                return;
        }

        if (button == toolbar->text_button) {
                annot_type = EV_ANNOTATION_TYPE_TEXT;
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toolbar->highlight_button), FALSE);
        } else if (button == toolbar->highlight_button) {
                annot_type = EV_ANNOTATION_TYPE_TEXT_MARKUP;
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toolbar->text_button), FALSE);
        } else {
                g_assert_not_reached ();
        }

        g_signal_emit (toolbar, signals[BEGIN_ADD_ANNOT], 0, annot_type);
}

static gboolean
ev_annotations_toolbar_toggle_button_if_active (EvAnnotationsToolbar *toolbar,
                                                GtkToggleButton      *button)
{
        if (!gtk_toggle_button_get_active (button))
                return FALSE;

        g_signal_handlers_block_by_func (button,
                                         ev_annotations_toolbar_annot_button_toggled,
                                         toolbar);
        gtk_toggle_button_set_active (button, FALSE);
        g_signal_handlers_unblock_by_func (button,
                                           ev_annotations_toolbar_annot_button_toggled,
                                           toolbar);

        return TRUE;
}

static GtkWidget *
ev_annotations_toolbar_create_toggle_button (EvAnnotationsToolbar *toolbar,
                                             const gchar          *label,
                                             const gchar          *icon_name,
                                             const gchar          *tooltip)
{
        GtkWidget *button = GTK_WIDGET (gtk_toggle_button_new ());

        gtk_widget_set_tooltip_text (button, tooltip);

	if (label)
                gtk_button_set_label (GTK_BUTTON (button), label);

        if (icon_name)
                gtk_button_set_icon_name (GTK_BUTTON (button), icon_name);

        /* For some reason adding text-button class to the GtkToogleButton makes the button smaller */
	gtk_widget_add_css_class(button, "text-button");
        g_signal_connect (button, "toggled",
                          G_CALLBACK (ev_annotations_toolbar_annot_button_toggled),
                          toolbar);

        return button;
}

static void
ev_annotations_toolbar_init (EvAnnotationsToolbar *toolbar)
{
        gtk_orientable_set_orientation (GTK_ORIENTABLE (toolbar), GTK_ORIENTATION_HORIZONTAL);

	gtk_widget_add_css_class(GTK_WIDGET (toolbar), "inline-toolbar");
	gtk_widget_add_css_class(GTK_WIDGET (toolbar), "linked");

        /* Use Text label until we have:
         *   1. More buttons in the toolbar (lack of space)
         *   2. Clear icons for an action.
         */
        toolbar->text_button = ev_annotations_toolbar_create_toggle_button (toolbar,
                               /* Translators: an annotation that looks like a "sticky note" */
                                                                            _("Note text"),
                                                                            NULL,
                                                                            _("Add text annotation"));
        gtk_box_append (GTK_BOX(toolbar), toolbar->text_button);

        toolbar->highlight_button = ev_annotations_toolbar_create_toggle_button (toolbar,
                                                                                 _("Highlight text"),
                                                                                 NULL,
                                                                                 _("Add highlight annotation"));

        gtk_box_append (GTK_BOX (toolbar), toolbar->highlight_button);
}

static void
ev_annotations_toolbar_class_init (EvAnnotationsToolbarClass *klass)
{
        GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

        signals[BEGIN_ADD_ANNOT] =
                g_signal_new ("begin-add-annot",
                              G_TYPE_FROM_CLASS (g_object_class),
                              G_SIGNAL_RUN_LAST,
                              0,
                              NULL, NULL,
                              g_cclosure_marshal_VOID__ENUM,
                              G_TYPE_NONE, 1,
                              EV_TYPE_ANNOTATION_TYPE);

        signals[CANCEL_ADD_ANNOT] =
                g_signal_new ("cancel-add-annot",
                              G_TYPE_FROM_CLASS (g_object_class),
                              G_SIGNAL_RUN_LAST,
                              0,
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0,
                              G_TYPE_NONE);
}

GtkWidget *
ev_annotations_toolbar_new (void)
{
	return GTK_WIDGET (g_object_new (EV_TYPE_ANNOTATIONS_TOOLBAR, NULL));
}

void
ev_annotations_toolbar_add_annot_finished (EvAnnotationsToolbar *toolbar)
{
        g_return_if_fail (EV_IS_ANNOTATIONS_TOOLBAR (toolbar));

        if (ev_annotations_toolbar_toggle_button_if_active (toolbar, GTK_TOGGLE_BUTTON (toolbar->text_button)))
                return;

        ev_annotations_toolbar_toggle_button_if_active (toolbar, GTK_TOGGLE_BUTTON (toolbar->highlight_button));
}
