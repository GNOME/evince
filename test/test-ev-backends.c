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

#define RED   0xff0000
#define GREEN 0x00ff00
#define BLUE  0x0000ff
#define GREY  0x808080
#define WHITE 0xffffff

typedef struct {
	const gchar *backend;
	const gchar *filename;
	gboolean     built;
	gboolean     has_text;
	gboolean     has_text_page_labels;
	const gchar *labels[2];
	/* Page units per point and margin around the content, in page units */
	gdouble      units;
	gdouble      margin;
	const gchar *render_bug;
} BackendTest;

typedef struct {
	gdouble x;
	gdouble y;
	guint32 rgb;
} Sample;

static const BackendTest backend_tests[] = {
	{ "pdf", "shapes.pdf", EV_TEST_HAVE_PDF,
	  TRUE, TRUE, { "Cover", "Body" }, 1, 0, NULL },
	{ "ps", "shapes.ps", EV_TEST_HAVE_PS,
	  FALSE, FALSE, { "1", "2" }, 1, 0, NULL },
	{ "djvu", "shapes.djvu", EV_TEST_HAVE_DJVU,
	  TRUE, FALSE, { "1", "2" }, 1, 0, NULL },
	{ "dvi", "shapes.dvi", EV_TEST_HAVE_DVI,
	  FALSE, FALSE, { "1", "2" },
	  /* mdvi uses 75 dpi and adds a margin of 1in on each side */
	  75 / 72.27, 75,
	  "DVI content is only rendered at the requested scale when the "
	  "shrink factor is an integer; at 1.0 it is also stretched horizontally" },
	{ "tiff", "shapes.tiff", EV_TEST_HAVE_TIFF,
	  FALSE, FALSE, { "1", "2" }, 1, 0, NULL },
	{ "comics", "shapes.cbz", EV_TEST_HAVE_COMICS,
	  FALSE, FALSE, { "1", "2" }, 1, 0, NULL },
	{ "xps", "shapes.xps", EV_TEST_HAVE_XPS,
	  FALSE, FALSE, { "1", "2" }, 1, 0, NULL },
};

/* Positions in points of a 200x300 page, see data/generate.py */
static const Sample page_one_samples[] = {
	{  50,  50, RED },
	{ 150,  50, GREEN },
	{ 150, 250, BLUE },
	{  50, 250, WHITE },
	{ 100, 110, WHITE },
	{  10,  50, WHITE },
	{  90,  50, WHITE },
	{ 190,  50, WHITE },
	{  50,  10, WHITE },
	{  50,  90, WHITE },
};

static const Sample page_two_samples[] = {
	{ 100, 150, GREY },
	{  50,  50, WHITE },
	{ 150, 250, WHITE },
};

static gboolean
skip_if_not_built (const BackendTest *test)
{
	if (test->built)
		return FALSE;

	g_test_skip ("Backend not built");
	return TRUE;
}

static void
test_n_pages (gconstpointer data)
{
	const BackendTest *test = data;
	EvDocument *document;

	if (skip_if_not_built (test))
		return;

	document = ev_test_load_document_or_fail (test->filename);
	g_assert_cmpint (ev_document_get_n_pages (document), ==, 2);
	g_assert_true (ev_document_is_page_size_uniform (document));

	g_object_unref (document);
}

static void
test_page_size (gconstpointer data)
{
	const BackendTest *test = data;
	EvDocument *document;
	gint i;

	if (skip_if_not_built (test))
		return;

	document = ev_test_load_document_or_fail (test->filename);

	for (i = 0; i < 2; i++) {
		gdouble width, height;

		ev_document_get_page_size (document, i, &width, &height);
		g_assert_cmpfloat_with_epsilon (width, 200 * test->units + 2 * test->margin, 1);
		g_assert_cmpfloat_with_epsilon (height, 300 * test->units + 2 * test->margin, 1);
	}

	g_object_unref (document);
}

static void
test_page_labels (gconstpointer data)
{
	const BackendTest *test = data;
	EvDocument *document;
	gint i;

	if (skip_if_not_built (test))
		return;

	document = ev_test_load_document_or_fail (test->filename);
	g_assert_cmpint (ev_document_has_text_page_labels (document), ==,
			 test->has_text_page_labels);

	for (i = 0; i < 2; i++) {
		g_autofree gchar *label = ev_document_get_page_label (document, i);

		g_assert_cmpstr (label, ==, test->labels[i]);
	}

	g_object_unref (document);
}

static void
test_page_text (gconstpointer data)
{
	const BackendTest *test = data;
	const gchar *expected[] = { "Evince page one", "Evince page two" };
	EvDocument *document;
	gint i;

	if (skip_if_not_built (test))
		return;

	document = ev_test_load_document_or_fail (test->filename);
	g_assert_cmpint (EV_IS_DOCUMENT_TEXT (document), ==, test->has_text);

	if (!test->has_text) {
		g_object_unref (document);
		return;
	}

	for (i = 0; i < 2; i++) {
		g_autofree gchar *text = ev_test_get_page_text (document, i);

		g_assert_nonnull (text);
		g_assert_nonnull (strstr (text, expected[i]));
	}

	g_object_unref (document);
}

static void
check_render (const BackendTest *test,
	      gint               index,
	      gint               rotation,
	      gdouble            scale,
	      const Sample      *samples,
	      guint              n_samples)
{
	EvDocument *document;
	cairo_surface_t *surface;
	gdouble width, height;
	gint expected_width, expected_height;
	guint i;

	document = ev_test_load_document_or_fail (test->filename);
	ev_document_get_page_size (document, index, &width, &height);

	surface = ev_test_render_page (document, index, rotation, scale);

	expected_width = (gint) (width * scale + 0.5);
	expected_height = (gint) (height * scale + 0.5);
	if (rotation == 90 || rotation == 270) {
		gint tmp = expected_width;

		expected_width = expected_height;
		expected_height = tmp;
	}

	g_assert_cmpint (ABS (cairo_image_surface_get_width (surface) - expected_width), <=, 1);
	g_assert_cmpint (ABS (cairo_image_surface_get_height (surface) - expected_height), <=, 1);

	for (i = 0; i < n_samples; i++) {
		gdouble x = test->margin + samples[i].x * test->units;
		gdouble y = test->margin + samples[i].y * test->units;
		gdouble rx, ry;

		switch (rotation) {
		case 90:
			rx = height - y;
			ry = x;
			break;
		case 180:
			rx = width - x;
			ry = height - y;
			break;
		case 270:
			rx = y;
			ry = width - x;
			break;
		default:
			rx = x;
			ry = y;
		}

		ev_test_assert_pixel (surface, (gint) (rx * scale),
				      (gint) (ry * scale), samples[i].rgb);
	}

	cairo_surface_destroy (surface);
	g_object_unref (document);
}

static gboolean
known_render_bug (const BackendTest *test)
{
	if (!test->render_bug || g_test_subprocess ())
		return FALSE;

	g_test_trap_subprocess (NULL, 0, 0);
	if (!g_test_trap_has_passed ())
		g_test_incomplete (test->render_bug);

	return TRUE;
}

static void
test_render (gconstpointer data)
{
	const BackendTest *test = data;

	if (skip_if_not_built (test) || known_render_bug (test))
		return;

	check_render (test, 0, 0, 1.0,
		      page_one_samples, G_N_ELEMENTS (page_one_samples));
}

static void
test_render_half (gconstpointer data)
{
	const BackendTest *test = data;

	if (skip_if_not_built (test) || known_render_bug (test))
		return;

	check_render (test, 0, 0, 0.5,
		      page_one_samples, G_N_ELEMENTS (page_one_samples));
}

static void
test_render_double (gconstpointer data)
{
	const BackendTest *test = data;

	if (skip_if_not_built (test))
		return;

	check_render (test, 0, 0, 2.0,
		      page_one_samples, G_N_ELEMENTS (page_one_samples));
}

static void
test_render_rotated (gconstpointer data)
{
	const BackendTest *test = data;
	gint rotation;

	if (skip_if_not_built (test) || known_render_bug (test))
		return;

	for (rotation = 90; rotation < 360; rotation += 90) {
		g_test_message ("rotation %d", rotation);
		check_render (test, 0, rotation, 1.0,
			      page_one_samples, G_N_ELEMENTS (page_one_samples));
	}
}

static void
test_render_page_two (gconstpointer data)
{
	const BackendTest *test = data;

	if (skip_if_not_built (test) || known_render_bug (test))
		return;

	check_render (test, 1, 0, 1.0,
		      page_two_samples, G_N_ELEMENTS (page_two_samples));
}

static void
add_backend_test (const BackendTest *test,
		  const gchar       *name,
		  GTestDataFunc      func)
{
	g_autofree gchar *path = NULL;

	path = g_strdup_printf ("/libdocument/%s/%s", test->backend, name);
	g_test_add_data_func (path, test, func);
}

int
main (int argc, char *argv[])
{
	int result;
	guint i;

	g_test_init (&argc, &argv, NULL);

	if (!ev_init ())
		g_error ("No document backends found");

	for (i = 0; i < G_N_ELEMENTS (backend_tests); i++) {
		const BackendTest *test = &backend_tests[i];

		add_backend_test (test, "shapes/n-pages", test_n_pages);
		add_backend_test (test, "shapes/page-size", test_page_size);
		add_backend_test (test, "shapes/page-labels", test_page_labels);
		add_backend_test (test, "shapes/page-text", test_page_text);
		add_backend_test (test, "shapes/render", test_render);
		add_backend_test (test, "shapes/render-half", test_render_half);
		add_backend_test (test, "shapes/render-double", test_render_double);
		add_backend_test (test, "shapes/render-rotated", test_render_rotated);
		add_backend_test (test, "shapes/render-page-two", test_render_page_two);
	}

	result = g_test_run ();

	ev_shutdown ();

	return result;
}
