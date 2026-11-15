/*
 * Copyright (C) 2026 Germán Poo-Caamaño <gpoo@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include <unistd.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include "ev-document-factory.h"
#include "ev-document-model.h"
#include "ev-init.h"
#include "ev-sidebar-annotations.h"
#include "ev-sidebar-page.h"

#define CONTENTS "a < b & <i>c</i> </span>"
#define LABEL    "<b>Author</b> & co"

#define TIMEOUT_SECONDS 10

/* Text annotation on a blank page; offsets in the xref table are
 * computed when the file is written. */
static gchar *
write_pdf (void)
{
	const gchar *objects[] = {
		"<< /Type /Catalog /Pages 2 0 R >>",
		"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
		"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 200] /Annots [4 0 R] >>",
		"<< /Type /Annot /Subtype /Text /Rect [10 10 30 30] "
		"/Contents (" CONTENTS ") /T (" LABEL ") /M (D:20261115120000Z) >>",
	};
	GString *pdf = g_string_new ("%PDF-1.4\n");
	gsize    offsets[G_N_ELEMENTS (objects)];
	gsize    xref;
	GError  *error = NULL;
	gchar   *path;
	guint    i;
	gint     fd;

	for (i = 0; i < G_N_ELEMENTS (objects); i++) {
		offsets[i] = pdf->len;
		g_string_append_printf (pdf, "%u 0 obj\n%s\nendobj\n", i + 1, objects[i]);
	}

	xref = pdf->len;
	g_string_append_printf (pdf, "xref\n0 %u\n0000000000 65535 f \n",
				(guint) G_N_ELEMENTS (objects) + 1);
	for (i = 0; i < G_N_ELEMENTS (objects); i++)
		g_string_append_printf (pdf, "%010" G_GSIZE_FORMAT " 00000 n \n", offsets[i]);
	g_string_append_printf (pdf,
				"trailer\n<< /Size %u /Root 1 0 R >>\nstartxref\n%" G_GSIZE_FORMAT "\n%%%%EOF\n",
				(guint) G_N_ELEMENTS (objects) + 1, xref);

	fd = g_file_open_tmp ("test-ev-sidebar-annotations-XXXXXX.pdf", &path, &error);
	g_assert_no_error (error);
	close (fd);
	g_file_set_contents (path, pdf->str, pdf->len, &error);
	g_assert_no_error (error);
	g_string_free (pdf, TRUE);

	return path;
}

static GtkWidget *
find_tree_view (GtkWidget *widget)
{
	GList     *children, *l;
	GtkWidget *found = NULL;

	if (GTK_IS_TREE_VIEW (widget))
		return widget;
	if (!GTK_IS_CONTAINER (widget))
		return NULL;

	children = gtk_container_get_children (GTK_CONTAINER (widget));
	for (l = children; l != NULL && found == NULL; l = l->next)
		found = find_tree_view (l->data);
	g_list_free (children);

	return found;
}

static gboolean
timeout_cb (gpointer data)
{
	*(gboolean *) data = TRUE;

	return G_SOURCE_REMOVE;
}

/* The model holds a page row with the annotation rows as children. */
static gboolean
wait_for_annotation_row (GtkTreeView *tree_view,
			 GtkTreeIter *child)
{
	gboolean timed_out = FALSE;
	guint    timeout_id;

	timeout_id = g_timeout_add_seconds (TIMEOUT_SECONDS, timeout_cb, &timed_out);
	while (!timed_out) {
		GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
		GtkTreeIter   parent;

		if (GTK_IS_TREE_STORE (model) &&
		    gtk_tree_model_get_iter_first (model, &parent) &&
		    gtk_tree_model_iter_children (model, child, &parent)) {
			g_source_remove (timeout_id);
			return TRUE;
		}
		g_main_context_iteration (NULL, TRUE);
	}

	return FALSE;
}

static gchar *
markup_to_text (const gchar *markup)
{
	GError *error = NULL;
	gchar  *text = NULL;

	pango_parse_markup (markup, -1, 0, NULL, &text, NULL, &error);
	g_assert_no_error (error);

	return text;
}

static void
test_markup_escaped (void)
{
	EvDocumentModel *model;
	EvDocument      *document;
	GtkWidget       *window;
	GtkWidget       *sidebar;
	GtkWidget       *tree_view;
	GtkTreeIter      iter;
	GError          *error = NULL;
	gchar           *path, *uri;
	gchar           *markup = NULL, *tooltip = NULL;
	gchar           *text;

	path = write_pdf ();
	uri = g_filename_to_uri (path, NULL, &error);
	g_assert_no_error (error);

	document = ev_document_factory_get_document (uri, &error);
	if (document == NULL) {
		g_test_skip (error->message);
		g_clear_error (&error);
		goto out;
	}

	model = ev_document_model_new ();
	sidebar = ev_sidebar_annotations_new ();
	window = gtk_offscreen_window_new ();
	gtk_container_add (GTK_CONTAINER (window), sidebar);
	ev_sidebar_page_set_model (EV_SIDEBAR_PAGE (sidebar), model);
	ev_document_model_set_document (model, document);

	tree_view = find_tree_view (sidebar);
	g_assert_nonnull (tree_view);
	g_assert_true (wait_for_annotation_row (GTK_TREE_VIEW (tree_view), &iter));

	/* Columns 0 and 3: COLUMN_MARKUP and COLUMN_TOOLTIP. */
	gtk_tree_model_get (gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view)), &iter,
			    0, &markup, 3, &tooltip, -1);

	text = markup_to_text (markup);
	g_assert_cmpstr (text, ==, CONTENTS);
	g_free (text);

	text = markup_to_text (tooltip);
	g_assert_true (g_str_has_prefix (text, LABEL));
	g_free (text);

	g_free (markup);
	g_free (tooltip);
	gtk_widget_destroy (window);
	g_object_unref (model);
	g_object_unref (document);
out:
	g_unlink (path);
	g_free (uri);
	g_free (path);
}

int
main (int argc, char *argv[])
{
	int ret;

	g_test_init (&argc, &argv, NULL);

	/* 77 tells meson that the test was skipped. */
	if (!gtk_init_check (&argc, &argv)) {
		g_printerr ("No display available\n");
		return 77;
	}

	ev_init ();

	g_test_add_func ("/shell/sidebar-annotations/markup-escaped",
			 test_markup_escaped);

	ret = g_test_run ();
	ev_shutdown ();

	return ret;
}
