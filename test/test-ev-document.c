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
test_page_label_out_of_range (void)
{
	EvDocument *document;
	gint n_pages;

	if (g_test_subprocess ()) {
		g_autofree gchar *label = NULL;

		document = ev_test_load_document_or_fail ("test-page-labels.pdf");
		n_pages = ev_document_get_n_pages (document);

		g_test_expect_message ("EvinceDocument", G_LOG_LEVEL_CRITICAL, "*page_index*");
		label = ev_document_get_page_label (document, n_pages);
		g_test_assert_expected_messages ();
		g_assert_null (label);

		g_object_unref (document);
		return;
	}

	g_test_trap_subprocess (NULL, 0, 0);
	if (!g_test_trap_has_passed ())
		g_test_incomplete ("ev_document_get_page_label() does not check the upper bound "
				   "of page_index (|| instead of &&)");
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
test_password (void)
{
	g_autoptr(GError) error = NULL;
	g_autofree gchar *uri = ev_test_get_uri ("test-password.pdf");
	g_autofree gchar *text = NULL;
	EvDocument *document;

	document = ev_document_factory_get_document (uri, &error);
	g_assert_error (error, EV_DOCUMENT_ERROR, EV_DOCUMENT_ERROR_ENCRYPTED);
	g_assert_true (EV_IS_DOCUMENT_SECURITY (document));
	g_clear_error (&error);

	ev_document_security_set_password (EV_DOCUMENT_SECURITY (document), "wrong");
	g_assert_false (ev_document_load (document, uri, &error));
	g_assert_error (error, EV_DOCUMENT_ERROR, EV_DOCUMENT_ERROR_ENCRYPTED);
	g_clear_error (&error);

	ev_document_security_set_password (EV_DOCUMENT_SECURITY (document), "evince");
	g_assert_true (ev_document_load (document, uri, &error));
	g_assert_no_error (error);

	g_assert_cmpint (ev_document_get_n_pages (document), ==, 2);
	text = ev_test_get_page_text (document, 0);
	g_assert_nonnull (strstr (text, "Evince page one"));

	g_object_unref (document);
}

static void
test_security (void)
{
	g_autoptr(GError) error = NULL;
	g_autofree gchar *uri = ev_test_get_uri ("test-password.pdf");
	EvDocument *document;
	EvDocumentInfo *info;

	document = ev_document_factory_get_document (uri, &error);
	g_assert_error (error, EV_DOCUMENT_ERROR, EV_DOCUMENT_ERROR_ENCRYPTED);
	g_clear_error (&error);

	ev_document_security_set_password (EV_DOCUMENT_SECURITY (document), "evince");
	g_assert_true (ev_document_load (document, uri, &error));
	g_assert_no_error (error);

	info = ev_document_get_info (document);
	g_assert_true (info->fields_mask & EV_DOCUMENT_INFO_SECURITY);

	if (!ev_document_security_has_document_security (EV_DOCUMENT_SECURITY (document)))
		g_test_incomplete ("The PDF backend never reports document security");

	g_object_unref (document);
}

static void
test_info (void)
{
	EvDocument *document = ev_test_load_document_or_fail ("shapes.pdf");
	EvDocumentInfo *info;
	GDateTime *created;
	guint mask;

	info = ev_document_get_info (document);
	g_assert_nonnull (info);

	mask = EV_DOCUMENT_INFO_TITLE | EV_DOCUMENT_INFO_AUTHOR |
	       EV_DOCUMENT_INFO_SUBJECT | EV_DOCUMENT_INFO_KEYWORDS |
	       EV_DOCUMENT_INFO_CREATOR | EV_DOCUMENT_INFO_N_PAGES |
	       EV_DOCUMENT_INFO_PAPER_SIZE | EV_DOCUMENT_INFO_CREATION_DATE;
	g_assert_cmpuint (info->fields_mask & mask, ==, mask);

	g_assert_cmpstr (info->title, ==, "Evince test document");
	g_assert_cmpstr (info->author, ==, "Evince developers");
	g_assert_cmpstr (info->subject, ==, "Shapes");
	g_assert_cmpstr (info->keywords, ==, "evince, test");
	g_assert_cmpstr (info->creator, ==, "generate.py");
	g_assert_cmpint (info->n_pages, ==, 2);
	g_assert_true (g_str_has_prefix (info->format, "PDF-"));

	/* Paper size is in millimetres */
	g_assert_cmpfloat_with_epsilon (info->paper_width, 200 * 25.4 / 72, 0.5);
	g_assert_cmpfloat_with_epsilon (info->paper_height, 300 * 25.4 / 72, 0.5);

	created = ev_document_info_get_created_datetime (info);
	g_assert_nonnull (created);
	g_assert_cmpint (g_date_time_to_unix (created), ==, 1767225600);

	g_object_unref (document);
}

static guint
count_matches (EvDocument    *document,
	       gint           index,
	       const gchar   *text,
	       EvFindOptions  options)
{
	EvPage *page;
	GList *matches;
	guint n;

	page = ev_document_get_page (document, index);
	matches = ev_document_find_find_text_extended (EV_DOCUMENT_FIND (document),
						       page, text, options);
	n = g_list_length (matches);

	g_list_free_full (matches, (GDestroyNotify) ev_find_rectangle_free);
	g_object_unref (page);

	return n;
}

static void
test_find (void)
{
	EvDocument *document = ev_test_load_document_or_fail ("shapes.pdf");
	EvPage *page;
	GList *matches;
	EvFindRectangle *rect;

	g_assert_true (EV_IS_DOCUMENT_FIND (document));

	g_assert_cmpuint (count_matches (document, 0, "page", EV_FIND_DEFAULT), ==, 2);
	g_assert_cmpuint (count_matches (document, 0, "PAGE", EV_FIND_DEFAULT), ==, 2);
	g_assert_cmpuint (count_matches (document, 0, "PAGE", EV_FIND_CASE_SENSITIVE), ==, 0);
	g_assert_cmpuint (count_matches (document, 0, "pag", EV_FIND_WHOLE_WORDS_ONLY), ==, 0);
	g_assert_cmpuint (count_matches (document, 0, "two", EV_FIND_DEFAULT), ==, 1);
	g_assert_cmpuint (count_matches (document, 1, "two", EV_FIND_DEFAULT), ==, 1);
	g_assert_cmpuint (count_matches (document, 1, "one", EV_FIND_DEFAULT), ==, 0);

	/* "Evince page one" is drawn at the baseline y = 150, see generate.py */
	page = ev_document_get_page (document, 0);
	matches = ev_document_find_find_text_extended (EV_DOCUMENT_FIND (document),
						       page, "Evince", EV_FIND_CASE_SENSITIVE);
	g_assert_cmpuint (g_list_length (matches), ==, 1);
	rect = matches->data;
	g_assert_cmpfloat (rect->x1, >=, 19);
	g_assert_cmpfloat (rect->x1, <, rect->x2);
	g_assert_cmpfloat (rect->x2, <, 100);
	g_assert_cmpfloat (rect->y1, >, 130);
	g_assert_cmpfloat (rect->y1, <, rect->y2);
	g_assert_cmpfloat (rect->y2, <, 160);

	g_list_free_full (matches, (GDestroyNotify) ev_find_rectangle_free);

	/* The older API returns a list of EvRectangle */
	matches = ev_document_find_find_text_with_options (EV_DOCUMENT_FIND (document),
							   page, "page", EV_FIND_DEFAULT);
	g_assert_cmpuint (g_list_length (matches), ==, 2);
	g_list_free_full (matches, (GDestroyNotify) ev_rectangle_free);

	g_object_unref (page);
	g_object_unref (document);
}

static void
test_outline (void)
{
	EvDocument *document = ev_test_load_document_or_fail ("shapes.pdf");
	EvDocumentLinks *links;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint i = 0;

	g_assert_true (EV_IS_DOCUMENT_LINKS (document));
	links = EV_DOCUMENT_LINKS (document);
	g_assert_true (ev_document_links_has_document_links (links));

	model = ev_document_links_get_links_model (links);
	g_assert_nonnull (model);
	g_assert_cmpint (gtk_tree_model_iter_n_children (model, NULL), ==, 2);

	g_assert_true (gtk_tree_model_get_iter_first (model, &iter));
	do {
		g_autofree gchar *markup = NULL;
		g_autofree gchar *expected = NULL;
		EvLink *link = NULL;

		gtk_tree_model_get (model, &iter,
				    EV_DOCUMENT_LINKS_COLUMN_MARKUP, &markup,
				    EV_DOCUMENT_LINKS_COLUMN_LINK, &link,
				    -1);

		expected = g_strdup_printf ("Chapter %d", i + 1);
		g_assert_cmpstr (markup, ==, expected);
		g_assert_nonnull (link);
		g_assert_cmpint (ev_document_links_get_link_page (links, link), ==, i);

		g_object_unref (link);
		i++;
	} while (gtk_tree_model_iter_next (model, &iter));

	g_object_unref (model);
	g_object_unref (document);
}

static void
test_named_dest (void)
{
	EvDocument *document = ev_test_load_document_or_fail ("shapes.pdf");
	EvDocumentLinks *links = EV_DOCUMENT_LINKS (document);
	EvPage *page;
	EvMappingList *mapping_list;
	GList *list;
	EvLinkAction *action;
	EvLinkDest *dest;

	page = ev_document_get_page (document, 0);
	mapping_list = ev_document_links_get_links (links, page);
	g_assert_nonnull (mapping_list);

	list = ev_mapping_list_get_list (mapping_list);
	g_assert_cmpuint (g_list_length (list), ==, 1);

	action = ev_link_get_action (EV_LINK (((EvMapping *) list->data)->data));
	g_assert_cmpint (ev_link_action_get_action_type (action), ==,
			 EV_LINK_ACTION_TYPE_GOTO_DEST);
	dest = ev_link_action_get_dest (action);
	g_assert_cmpint (ev_link_dest_get_dest_type (dest), ==, EV_LINK_DEST_TYPE_NAMED);
	g_assert_cmpstr (ev_link_dest_get_named_dest (dest), ==, "second");
	g_assert_cmpint (ev_document_links_get_dest_page (links, dest), ==, 1);
	g_assert_cmpint (ev_document_links_find_link_page (links, "second"), ==, 1);

	ev_mapping_list_unref (mapping_list);
	g_object_unref (page);
	g_object_unref (document);
}

static void
test_annotations (void)
{
	EvDocument *document = ev_test_load_document_or_fail ("test-features.pdf");
	EvPage *page;
	EvMappingList *mapping_list;
	GList *list;
	EvAnnotation *annot;
	GdkRGBA rgba;

	g_assert_true (EV_IS_DOCUMENT_ANNOTATIONS (document));

	/* The square annotation is not implemented, see ev-poppler.c */
	g_test_expect_message (NULL, G_LOG_LEVEL_WARNING,
			       "Unimplemented annotation: POPPLER_ANNOT_SQUARE*");

	page = ev_document_get_page (document, 0);
	mapping_list = ev_document_annotations_get_annotations (EV_DOCUMENT_ANNOTATIONS (document),
								page);
	g_test_assert_expected_messages ();
	g_assert_nonnull (mapping_list);

	list = ev_mapping_list_get_list (mapping_list);
	g_assert_cmpuint (g_list_length (list), ==, 1);

	annot = EV_ANNOTATION (((EvMapping *) list->data)->data);
	g_assert_true (EV_IS_ANNOTATION_TEXT (annot));
	g_assert_cmpint (ev_annotation_get_annotation_type (annot), ==, EV_ANNOTATION_TYPE_TEXT);
	g_assert_cmpstr (ev_annotation_get_contents (annot), ==, "A sticky note");
	g_assert_cmpstr (ev_annotation_get_name (annot), ==, "note-1");
	g_assert_cmpuint (ev_annotation_get_page_index (annot), ==, 0);
	g_assert_cmpstr (ev_annotation_markup_get_label (EV_ANNOTATION_MARKUP (annot)), ==, "Evince");
	g_assert_cmpint (ev_annotation_text_get_icon (EV_ANNOTATION_TEXT (annot)), ==,
			 EV_ANNOTATION_TEXT_ICON_COMMENT);

	ev_annotation_get_rgba (annot, &rgba);
	g_assert_cmpfloat_with_epsilon (rgba.red, 1, 0.01);
	g_assert_cmpfloat_with_epsilon (rgba.green, 1, 0.01);
	g_assert_cmpfloat_with_epsilon (rgba.blue, 0, 0.01);

	ev_mapping_list_unref (mapping_list);
	g_object_unref (page);
	g_object_unref (document);
}

static void
test_attachments (void)
{
	EvDocument *document = ev_test_load_document_or_fail ("test-features.pdf");
	EvDocumentAttachments *attachments;
	g_autoptr(GError) error = NULL;
	g_autoptr(GFileIOStream) stream = NULL;
	g_autoptr(GFile) file = NULL;
	g_autofree gchar *contents = NULL;
	EvAttachment *attachment;
	GList *list;

	g_assert_true (EV_IS_DOCUMENT_ATTACHMENTS (document));
	attachments = EV_DOCUMENT_ATTACHMENTS (document);
	g_assert_true (ev_document_attachments_has_attachments (attachments));

	list = ev_document_attachments_get_attachments (attachments);
	g_assert_cmpuint (g_list_length (list), ==, 1);

	attachment = list->data;
	g_assert_cmpstr (ev_attachment_get_name (attachment), ==, "hello.txt");
	g_assert_cmpstr (ev_attachment_get_description (attachment), ==, "A text attachment");

	file = g_file_new_tmp ("test-ev-attachment-XXXXXX", &stream, &error);
	g_assert_no_error (error);
	g_assert_true (ev_attachment_save (attachment, file, &error));
	g_assert_no_error (error);

	g_assert_true (g_file_load_contents (file, NULL, &contents, NULL, NULL, &error));
	g_assert_no_error (error);
	g_assert_cmpstr (contents, ==, "Hello, Evince!\n");

	g_file_delete (file, NULL, NULL);
	g_list_free_full (list, g_object_unref);
	g_object_unref (document);
}

static void
test_forms (void)
{
	EvDocument *document = ev_test_load_document_or_fail ("test-features.pdf");
	EvDocumentForms *forms;
	EvPage *page;
	EvMappingList *mapping_list;
	EvFormField *text = NULL;
	EvFormField *button = NULL;
	g_autofree gchar *value = NULL;
	GList *l;

	g_assert_true (EV_IS_DOCUMENT_FORMS (document));
	forms = EV_DOCUMENT_FORMS (document);

	page = ev_document_get_page (document, 0);
	mapping_list = ev_document_forms_get_form_fields (forms, page);
	g_assert_nonnull (mapping_list);
	g_assert_cmpuint (g_list_length (ev_mapping_list_get_list (mapping_list)), ==, 2);

	for (l = ev_mapping_list_get_list (mapping_list); l; l = l->next) {
		EvFormField *field = ((EvMapping *) l->data)->data;

		if (EV_IS_FORM_FIELD_TEXT (field))
			text = field;
		else if (EV_IS_FORM_FIELD_BUTTON (field))
			button = field;
	}

	g_assert_nonnull (text);
	g_assert_nonnull (button);

	value = ev_document_forms_form_field_text_get_text (forms, text);
	g_assert_cmpstr (value, ==, "Evince");
	g_assert_cmpint (EV_FORM_FIELD_BUTTON (button)->type, ==, EV_FORM_FIELD_BUTTON_CHECK);
	g_assert_true (ev_document_forms_form_field_button_get_state (forms, button));
	g_assert_false (ev_document_forms_document_is_modified (forms));

	ev_document_forms_form_field_text_set_text (forms, text, "Changed");
	g_clear_pointer (&value, g_free);
	value = ev_document_forms_form_field_text_get_text (forms, text);
	g_assert_cmpstr (value, ==, "Changed");
	g_assert_true (ev_document_forms_document_is_modified (forms));

	ev_mapping_list_unref (mapping_list);
	g_object_unref (page);
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
	g_test_add_func ("/libdocument/pdf/page-label-out-of-range", test_page_label_out_of_range);
	g_test_add_func ("/libdocument/pdf/links", test_links);
	g_test_add_func ("/libdocument/pdf/encrypted", test_encrypted);
	g_test_add_func ("/libdocument/pdf/password", test_password);
	g_test_add_func ("/libdocument/pdf/security", test_security);
	g_test_add_func ("/libdocument/pdf/info", test_info);
	g_test_add_func ("/libdocument/pdf/find", test_find);
	g_test_add_func ("/libdocument/pdf/outline", test_outline);
	g_test_add_func ("/libdocument/pdf/named-dest", test_named_dest);
	g_test_add_func ("/libdocument/pdf/annotations", test_annotations);
	g_test_add_func ("/libdocument/pdf/attachments", test_attachments);
	g_test_add_func ("/libdocument/pdf/forms", test_forms);
	g_test_add_func ("/libdocument/missing-file", test_missing_file);

	result = g_test_run ();

	ev_shutdown ();

	return result;
}
