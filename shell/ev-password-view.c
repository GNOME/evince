/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2005 Red Hat, Inc
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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "ev-marshal.h"
#include "ev-password-view.h"

enum {
	UNLOCK,
	LAST_SIGNAL
};
struct _EvPasswordViewPrivate {
	GtkWidget *label;
};

#define EV_PASSWORD_VIEW_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_PASSWORD_VIEW, EvPasswordViewPrivate));

static guint password_view_signals [LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (EvPasswordView, ev_password_view, GTK_TYPE_VIEWPORT)


static void
ev_password_view_class_init (EvPasswordViewClass *class)
{
	GObjectClass *g_object_class;
	GtkWidgetClass *widget_class;

	g_object_class = G_OBJECT_CLASS (class);
	widget_class = GTK_WIDGET_CLASS (class);

	password_view_signals [UNLOCK] =
		g_signal_new ("unlock",
			      G_TYPE_FROM_CLASS (g_object_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (EvPasswordViewClass, unlock),
			      NULL, NULL,
			      ev_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	g_type_class_add_private (g_object_class, sizeof (EvPasswordViewPrivate));
}

static void
ev_password_view_clicked_cb (GtkWidget      *button,
			     EvPasswordView *password_view)
{
	g_signal_emit (password_view, password_view_signals [UNLOCK], 0);
}

static void
ev_password_view_init (EvPasswordView *password_view)
{
	GtkWidget *align;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *image;
	GtkWidget *button;
	GtkWidget *label;
	gchar *markup;

	password_view->priv = EV_PASSWORD_VIEW_GET_PRIVATE (password_view);

	gtk_widget_push_composite_child ();

	/* set ourselves up */
	align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
	vbox = gtk_vbox_new (FALSE, 24);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 24);
	gtk_container_add (GTK_CONTAINER (password_view), align);
	gtk_container_add (GTK_CONTAINER (align), vbox);

	password_view->priv->label =
		(GtkWidget *) g_object_new (GTK_TYPE_LABEL,
					    "wrap", TRUE,
					    "selectable", TRUE,
					    NULL);
	gtk_box_pack_start (GTK_BOX (vbox), password_view->priv->label, FALSE, FALSE, 0);

	image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_AUTHENTICATION,
					  GTK_ICON_SIZE_DIALOG);
	gtk_box_pack_start (GTK_BOX (vbox), image, FALSE, FALSE, 0);

	label = gtk_label_new (NULL);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	markup = g_strdup_printf ("<span size=\"x-large\">%s</span>",
				  _("This document is locked and can only be read by entering the correct password."));
	gtk_label_set_markup (GTK_LABEL (label), markup);
	g_free (markup);
			      
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	button = gtk_button_new_with_mnemonic (_("_Unlock Document"));
	g_signal_connect (button, "clicked", G_CALLBACK (ev_password_view_clicked_cb), password_view);
	gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);

	gtk_widget_show_all (align);
	gtk_widget_pop_composite_child ();
}


/* Public functions */
void
ev_password_view_set_file_name (EvPasswordView *password_view,
				const char     *file_name)
{
	gchar *markup;

	g_return_if_fail (EV_IS_PASSWORD_VIEW (password_view));
	g_return_if_fail (file_name != NULL);

	markup = g_markup_printf_escaped ("<span size=\"x-large\" weight=\"bold\">%s</span>", file_name);

	gtk_label_set_markup (GTK_LABEL (password_view->priv->label), markup);

	g_free (markup);
}

GtkWidget *
ev_password_view_new (void)
{
	GtkWidget *retval;

	retval = (GtkWidget *) g_object_new (EV_TYPE_PASSWORD_VIEW, NULL);

	return retval;
}
