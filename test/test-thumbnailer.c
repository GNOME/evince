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


#include <glib/gstdio.h>

#include "ev-test-utils.h"

#define SIZE 64

typedef struct {
	const gchar *backend;
	const gchar *filename;
	gboolean     built;
} ThumbnailTest;

static const ThumbnailTest thumbnail_tests[] = {
	{ "pdf", "shapes.pdf", EV_TEST_HAVE_PDF },
	{ "ps", "shapes.ps", EV_TEST_HAVE_PS },
	{ "djvu", "shapes.djvu", EV_TEST_HAVE_DJVU },
	{ "tiff", "shapes.tiff", EV_TEST_HAVE_TIFF },
	{ "comics", "shapes.cbz", EV_TEST_HAVE_COMICS },
	{ "xps", "shapes.xps", EV_TEST_HAVE_XPS },
};

static gboolean
run_thumbnailer (const gchar *input,
		 const gchar *output)
{
	g_autoptr(GError) error = NULL;
	g_autofree gchar *size = g_strdup_printf ("%d", SIZE);
	const gchar *thumbnailer = g_getenv ("EV_TEST_THUMBNAILER");
	const gchar *argv[] = { thumbnailer, "-s", size, input, output, NULL };
	gint status;

	g_assert_nonnull (thumbnailer);

	g_spawn_sync (NULL, (gchar **) argv, NULL, G_SPAWN_STDOUT_TO_DEV_NULL,
		      NULL, NULL, NULL, NULL, &status, &error);
	g_assert_no_error (error);

	return g_spawn_check_wait_status (status, NULL);
}

static void
test_thumbnail (gconstpointer data)
{
	const ThumbnailTest *test = data;
	g_autofree gchar *input = NULL;
	g_autofree gchar *dir = NULL;
	g_autofree gchar *output = NULL;
	cairo_surface_t *surface;
	g_autoptr(GError) error = NULL;

	if (!test->built) {
		g_test_skip ("Backend not built");
		return;
	}

	/* The thumbnailer saves the thumbnail with gdk-pixbuf */
	if (!ev_test_can_decode_images ()) {
		g_test_skip ("Images cannot be decoded");
		return;
	}

	input = g_test_build_filename (G_TEST_DIST, "data", test->filename, NULL);
	dir = g_dir_make_tmp ("test-thumbnailer-XXXXXX", &error);
	g_assert_no_error (error);
	output = g_build_filename (dir, "thumbnail.png", NULL);

	g_assert_true (run_thumbnailer (input, output));

	surface = cairo_image_surface_create_from_png (output);
	g_assert_cmpint (cairo_surface_status (surface), ==, CAIRO_STATUS_SUCCESS);

	/* The page is 200x300, see data/generate.py */
	g_assert_cmpint (cairo_image_surface_get_height (surface), ==, SIZE);
	g_assert_cmpint (ABS (cairo_image_surface_get_width (surface) - SIZE * 2 / 3), <=, 1);

	ev_test_assert_pixel (surface, 50 * SIZE / 300, 50 * SIZE / 300, 0xff0000);
	ev_test_assert_pixel (surface, 150 * SIZE / 300, 250 * SIZE / 300, 0x0000ff);
	ev_test_assert_pixel (surface, 50 * SIZE / 300, 250 * SIZE / 300, 0xffffff);

	cairo_surface_destroy (surface);
	g_unlink (output);
	g_rmdir (dir);
}

static void
test_missing_file (void)
{
	g_autofree gchar *dir = NULL;
	g_autofree gchar *output = NULL;
	g_autoptr(GError) error = NULL;

	dir = g_dir_make_tmp ("test-thumbnailer-XXXXXX", &error);
	g_assert_no_error (error);
	output = g_build_filename (dir, "thumbnail.png", NULL);

	g_assert_false (run_thumbnailer ("does-not-exist.pdf", output));
	g_assert_false (g_file_test (output, G_FILE_TEST_EXISTS));

	g_rmdir (dir);
}

int
main (int argc, char *argv[])
{
	guint i;

	g_test_init (&argc, &argv, NULL);

	for (i = 0; i < G_N_ELEMENTS (thumbnail_tests); i++) {
		const ThumbnailTest *test = &thumbnail_tests[i];
		g_autofree gchar *path = NULL;

		path = g_strdup_printf ("/thumbnailer/%s", test->backend);
		g_test_add_data_func (path, test, test_thumbnail);
	}

	g_test_add_func ("/thumbnailer/missing-file", test_missing_file);

	return g_test_run ();
}
