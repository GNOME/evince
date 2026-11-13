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

#include "ev-test-utils.h"

static void
test_n_pages (void)
{
	EvDocument *three = ev_test_load_document_or_fail ("3-page.pdf");
	EvDocument *four = ev_test_load_document_or_fail ("4-page.pdf");

	g_assert_cmpint (ev_document_get_n_pages (three), ==, 3);
	g_assert_cmpint (ev_document_get_n_pages (four), ==, 4);

	g_object_unref (three);
	g_object_unref (four);
}

static void
test_page_text (void)
{
	EvDocument *document = ev_test_load_document_or_fail ("4-page.pdf");
	gint i;

	for (i = 0; i < ev_document_get_n_pages (document); i++) {
		g_autofree gchar *text = ev_test_get_page_text (document, i);
		g_autofree gchar *expected = g_strdup_printf ("%d", i + 1);

		g_assert_nonnull (text);
		g_assert_cmpstr (g_strstrip (text), ==, expected);
	}

	g_object_unref (document);
}

static void
test_page_labels (void)
{
	const gchar *expected[] = { "i", "ii", "I", "II", "III" };
	EvDocument *document = ev_test_load_document_or_fail ("test-page-labels.pdf");
	guint i;

	g_assert_cmpint (ev_document_get_n_pages (document), ==, G_N_ELEMENTS (expected));
	g_assert_true (ev_document_has_text_page_labels (document));

	for (i = 0; i < G_N_ELEMENTS (expected); i++) {
		g_autofree gchar *label = ev_document_get_page_label (document, i);

		g_assert_cmpstr (label, ==, expected[i]);
	}

	g_object_unref (document);
}

static void
test_links (void)
{
	EvDocument *document = ev_test_load_document_or_fail ("test-links.pdf");
	EvPage *page;
	EvDocumentLinks *links;
	EvMappingList *mapping_list;
	GList *list;
	EvLink *link;
	EvLinkAction *action;

	g_assert_true (EV_IS_DOCUMENT_LINKS (document));
	links = EV_DOCUMENT_LINKS (document);

	page = ev_document_get_page (document, 0);
	mapping_list = ev_document_links_get_links (links, page);
	g_assert_nonnull (mapping_list);

	list = ev_mapping_list_get_list (mapping_list);
	g_assert_cmpuint (g_list_length (list), ==, 1);

	link = EV_LINK (((EvMapping *) list->data)->data);
	action = ev_link_get_action (link);
	g_assert_cmpint (ev_link_action_get_action_type (action), ==,
			 EV_LINK_ACTION_TYPE_GOTO_DEST);
	g_assert_cmpint (ev_document_links_get_dest_page (links,
							  ev_link_action_get_dest (action)),
			 ==, 1);

	ev_mapping_list_unref (mapping_list);
	g_object_unref (page);
	g_object_unref (document);
}

static void
test_encrypted (void)
{
	g_autoptr(GError) error = NULL;
	EvDocument *document;

	document = ev_test_load_document ("test-encrypt.pdf", &error);
	g_assert_error (error, EV_DOCUMENT_ERROR, EV_DOCUMENT_ERROR_ENCRYPTED);
	g_assert_true (EV_IS_DOCUMENT_SECURITY (document));

	g_object_unref (document);
}

static void
test_missing_file (void)
{
	g_autoptr(GError) error = NULL;
	EvDocument *document;

	document = ev_test_load_document ("does-not-exist.pdf", &error);
	g_assert_null (document);
	g_assert_nonnull (error);
}

int
main (int argc, char *argv[])
{
	int result;

	g_test_init (&argc, &argv, NULL);

	if (!ev_init ())
		g_error ("No document backends found");

	g_test_add_func ("/libdocument/pdf/n-pages", test_n_pages);
	g_test_add_func ("/libdocument/pdf/page-text", test_page_text);
	g_test_add_func ("/libdocument/pdf/page-labels", test_page_labels);
	g_test_add_func ("/libdocument/pdf/links", test_links);
	g_test_add_func ("/libdocument/pdf/encrypted", test_encrypted);
	g_test_add_func ("/libdocument/missing-file", test_missing_file);

	result = g_test_run ();

	ev_shutdown ();

	return result;
}
