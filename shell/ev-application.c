/* this file is part of evince, a gnome document viewer
 *
 *  Copyright (C) 2004 Martin Kretzschmar
 *
 *  Author:
 *    Martin Kretzschmar <martink@gnome.org>
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

#include "ev-application.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gtk/gtkfilechooserdialog.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkmain.h>

struct _EvApplicationPrivate {
	GList *windows;
};

G_DEFINE_TYPE (EvApplication, ev_application, G_TYPE_OBJECT);

#define EV_APPLICATION_GET_PRIVATE(object) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), EV_TYPE_APPLICATION, EvApplicationPrivate))

EvApplication *
ev_application_get_instance (void)
{
	static EvApplication *instance;

	if (!instance)
		instance = EV_APPLICATION (
			g_object_new (EV_TYPE_APPLICATION, NULL));

	return instance;
}

static void
window_destroy_cb (GtkObject *object, gpointer user_data)
{
	EvApplication *application;
	
	g_return_if_fail (EV_IS_WINDOW (object));
	g_return_if_fail (EV_IS_APPLICATION (user_data));

	application = EV_APPLICATION (user_data);
	application->priv->windows =
		g_list_remove (application->priv->windows, object);

	if (application->priv->windows == NULL)
		gtk_main_quit ();
}

EvWindow *
ev_application_new_window (EvApplication *application)
{
	EvWindow *ev_window;

	ev_window = EV_WINDOW (g_object_new (EV_TYPE_WINDOW,
					     "type", GTK_WINDOW_TOPLEVEL,
					     "default-height", 600,
					     "default-width", 600,
					     NULL));
	application->priv->windows =
		g_list_prepend (application->priv->windows, ev_window);
	g_signal_connect (G_OBJECT (ev_window), "destroy",
			  G_CALLBACK (window_destroy_cb), application);

	return ev_window;
}

static int
is_window_empty (const EvWindow *ev_window, gconstpointer dummy)
{
	g_return_val_if_fail (EV_IS_WINDOW (ev_window), 0);

	return ev_window_is_empty (ev_window)
		? 0
		: -1;
}

EvWindow *
ev_application_get_empty_window (EvApplication *application)
{
	GList *node;

	node = g_list_find_custom (application->priv->windows, NULL,
				   (GCompareFunc)is_window_empty);

	return node && node->data
		? EV_WINDOW (node->data)
		: ev_application_new_window (application);
}

void
ev_application_open (EvApplication *application, GError *err)
{
	EvWindow *ev_window;
	GtkWidget *chooser;
	GtkFileFilter *documents_filter;
	GtkFileFilter *pdf_filter;
	GtkFileFilter *ps_filter;
	GtkFileFilter *pixbuf_filter;
	GtkFileFilter *all_filter;
	static gchar  *folder = NULL;
#ifdef ENABLE_DJVU
	GtkFileFilter *djvu_filter;
#endif
#ifdef ENABLE_DVI
	GtkFileFilter *dvi_filter;
#endif

	ev_window = ev_application_get_empty_window (application);

	chooser = gtk_file_chooser_dialog_new (_("Open document"),
					       GTK_WINDOW (ev_window),
					       GTK_FILE_CHOOSER_ACTION_OPEN,
					       GTK_STOCK_CANCEL,
					       GTK_RESPONSE_CANCEL,
					       GTK_STOCK_OPEN, GTK_RESPONSE_OK,
					       NULL);

	if (folder) {
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser),
						     folder);
	}

	documents_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (documents_filter,
				  _("All Documents"));
	gtk_file_filter_add_mime_type (documents_filter, "application/postscript");
	gtk_file_filter_add_mime_type (documents_filter, "application/x-gzpostscript");
	gtk_file_filter_add_mime_type (documents_filter, "image/x-eps");
	gtk_file_filter_add_mime_type (documents_filter, "application/pdf");
#ifdef ENABLE_DVI
	gtk_file_filter_add_mime_type (documents_filter, "application/x-dvi");
#endif
	gtk_file_filter_add_pixbuf_formats (documents_filter);
#ifdef ENABLE_DJVU
	gtk_file_filter_add_mime_type (documents_filter, "image/vnd.djvu");
#endif
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), documents_filter);

	ps_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (ps_filter, _("PostScript Documents"));
	gtk_file_filter_add_mime_type (ps_filter, "application/postscript");
	gtk_file_filter_add_mime_type (ps_filter, "application/x-gzpostscript");
	gtk_file_filter_add_mime_type (ps_filter, "image/x-eps");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), ps_filter);

	pdf_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (pdf_filter, _("PDF Documents"));
	gtk_file_filter_add_mime_type (pdf_filter, "application/pdf");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), pdf_filter);

#ifdef ENABLE_DVI
	dvi_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (dvi_filter, _("DVI Documents"));
	gtk_file_filter_add_mime_type (dvi_filter, "application/x-dvi");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), dvi_filter);
#endif

	pixbuf_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (pixbuf_filter, _("Images"));
	gtk_file_filter_add_pixbuf_formats (pixbuf_filter);
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), pixbuf_filter);

#ifdef ENABLE_DJVU
	djvu_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (djvu_filter, _("Djvu Documents"));
	gtk_file_filter_add_mime_type (djvu_filter, "image/vnd.djvu");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), djvu_filter);
#endif	
	
	all_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (all_filter, _("All Files"));
	gtk_file_filter_add_pattern (all_filter, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), all_filter);
	
	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (chooser), documents_filter);

	gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (chooser), TRUE);

	if (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_OK) {
		GSList *uris;

		uris = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (chooser));
		folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (chooser));

		ev_window_open_uri_list (ev_window, uris);
		
		g_slist_free (uris);
	} else {
		if (!GTK_WIDGET_VISIBLE (ev_window))
			gtk_widget_destroy (GTK_WIDGET (ev_window));
	}

	gtk_widget_destroy (GTK_WIDGET (chooser));
}

static void
ev_application_class_init (EvApplicationClass *ev_application_class)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS (ev_application_class);

	g_type_class_add_private (g_object_class,
				  sizeof (EvApplicationPrivate));
}

static void
ev_application_init (EvApplication *ev_application)
{
	ev_application->priv = EV_APPLICATION_GET_PRIVATE (ev_application);
}

