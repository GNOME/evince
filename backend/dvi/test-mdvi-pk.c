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

/*
 * Decodes crafted packed glyphs of PK fonts with mdvi-lib. Each case runs
 * in a subprocess, so a crash is reported as a failure; build with
 * -Db_sanitize=address to catch writes out of bounds that do not crash.
 */

#include "config.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>

#include "mdvi.h"

extern DviFontInfo pk_font_info;

#define PK_CHAR 65

static gchar *tmp_dir;

static void
put (GByteArray *a, guint32 value, int n)
{
	while (n-- > 0) {
		guint8 byte = (value >> (8 * n)) & 0xff;

		g_byte_array_append (a, &byte, 1);
	}
}

/* A PK font with one character in the short form, whose raster is the
 * given nybbles (0-15, padded with zero to a byte).
 */
static GByteArray *
build_pk (int          dyn_f,
	  gboolean     black,
	  int          w,
	  int          h,
	  const gchar *nybbles)
{
	GByteArray *a = g_byte_array_new ();
	GByteArray *raster = g_byte_array_new ();
	gsize i, n = strlen (nybbles);

	for (i = 0; i < n; i += 2) {
		guint8 byte = g_ascii_xdigit_value (nybbles[i]) << 4;

		if (i + 1 < n)
			byte |= g_ascii_xdigit_value (nybbles[i + 1]);
		g_byte_array_append (raster, &byte, 1);
	}

	put (a, 247, 1);		/* pre */
	put (a, 89, 1);			/* PK id */
	put (a, 0, 1);			/* no comment */
	put (a, 10 << 20, 4);		/* design size */
	put (a, 0, 4);			/* checksum */
	put (a, 0x10000, 4);		/* hppp */
	put (a, 0x10000, 4);		/* vppp */

	put (a, (dyn_f << 4) | (black ? 8 : 0), 1);
	put (a, 8 + raster->len, 1);	/* packet length */
	put (a, PK_CHAR, 1);
	put (a, 0x100000, 3);		/* TFM width */
	put (a, w, 1);			/* dx */
	put (a, w, 1);
	put (a, h, 1);
	put (a, 0, 1);			/* x offset */
	put (a, h, 1);			/* y offset */
	g_byte_array_append (a, raster->data, raster->len);

	put (a, 245, 1);		/* post */

	g_byte_array_unref (raster);

	return a;
}

/* Returns the glyph, or NULL if it cannot be decoded */
static BITMAP *
load_glyph (GByteArray *a,
	    DviFont    *font)
{
	DviParams params;
	DviFontChar *ch;
	gchar *path = g_build_filename (tmp_dir, "test.pk", NULL);
	GError *error = NULL;
	int status;

	g_file_set_contents (path, (const gchar *) a->data, a->len, &error);
	g_assert_no_error (error);

	memset (&params, 0, sizeof (params));
	params.mag = 1.0;
	params.tfm_conv = 1.0;
	params.dpi = params.vdpi = 600;

	memset (font, 0, sizeof (DviFont));
	font->fontname = (char *) "test";
	font->scale = 10 << 20;
	font->in = fopen (path, "rb");
	g_assert_nonnull (font->in);

	g_assert_cmpint (pk_font_info.load (&params, font), ==, 0);
	g_assert_cmpint (font->loc, ==, PK_CHAR);
	g_assert_cmpint (font->hic, ==, PK_CHAR);

	status = pk_font_info.getglyph (&params, font, PK_CHAR);
	ch = FONTCHAR (font, PK_CHAR);

	fclose (font->in);
	font->in = NULL;
	g_free (path);

	return status == 0 ? ch->glyph.data : NULL;
}

static void
free_font (DviFont *font)
{
	DviFontChar *ch = FONTCHAR (font, PK_CHAR);

	if (ch->glyph.data)
		bitmap_destroy (ch->glyph.data);
	mdvi_free (font->chars);
}

static gboolean
bitmap_get_pixel (BITMAP *bm,
		  int     x,
		  int     y)
{
	BmUnit *unit = __bm_unit_ptr (bm, x, y);

	return (*unit & FIRSTMASKAT (x)) != 0;
}

static void
test_pk_valid (void)
{
	/* 2x2, black: a run of 4 fills the glyph */
	GByteArray *a = build_pk (13, TRUE, 2, 2, "4");
	DviFont font;
	BITMAP *bm;

	bm = load_glyph (a, &font);
	g_assert_nonnull (bm);
	g_assert_cmpint (bm->width, ==, 2);
	g_assert_cmpint (bm->height, ==, 2);
	g_assert_true (bitmap_get_pixel (bm, 0, 0));
	g_assert_true (bitmap_get_pixel (bm, 1, 1));

	free_font (&font);
	g_byte_array_unref (a);
}

static void
test_pk_repeat_valid (void)
{
	/* 2x3: white, black; repeat the row once; then a white row */
	GByteArray *a = build_pk (13, FALSE, 2, 3, "1F12");
	DviFont font;
	BITMAP *bm;

	bm = load_glyph (a, &font);
	g_assert_nonnull (bm);
	g_assert_false (bitmap_get_pixel (bm, 0, 0));
	g_assert_true (bitmap_get_pixel (bm, 1, 0));
	g_assert_false (bitmap_get_pixel (bm, 0, 1));
	g_assert_true (bitmap_get_pixel (bm, 1, 1));
	g_assert_false (bitmap_get_pixel (bm, 0, 2));
	g_assert_false (bitmap_get_pixel (bm, 1, 2));

	free_font (&font);
	g_byte_array_unref (a);
}

static void
test_pk_negative_count (void)
{
	/* A long run count that overflows to -64, then a run of 1: this
	 * painted 64 pixels before the start of the bitmap (issue #2169) */
	GByteArray *a = build_pk (13, TRUE, 8, 8, "0000000FFFFFFC21");
	DviFont font;

	g_assert_null (load_glyph (a, &font));
	free_font (&font);
	g_byte_array_unref (a);
}

static void
test_pk_repeat_past_end (void)
{
	/* 2x2: repeat the first row 5 times; this copied rows past the end
	 * of the bitmap */
	GByteArray *a = build_pk (13, TRUE, 2, 2, "E52");
	DviFont font;

	g_assert_null (load_glyph (a, &font));
	free_font (&font);
	g_byte_array_unref (a);
}

static void
test_pk_rows_past_end (void)
{
	/* 2x2: a run of 163 pixels; this filled rows past the end of the
	 * bitmap */
	GByteArray *a = build_pk (2, TRUE, 2, 2, "D0");
	DviFont font;

	g_assert_null (load_glyph (a, &font));
	free_font (&font);
	g_byte_array_unref (a);
}

static void
test_pk_truncated (void)
{
	/* No raster: the postamble and then the end of the file are read
	 * as repeat counts, which never ended */
	GByteArray *a = build_pk (13, TRUE, 8, 8, "");
	DviFont font;

	g_assert_null (load_glyph (a, &font));
	free_font (&font);
	g_byte_array_unref (a);
}

static void
run_in_subprocess (gconstpointer data)
{
	const GTestFunc *func = data;

	if (g_test_subprocess ()) {
		(*func) ();
		return;
	}

	/* a glyph that never ends must not hang */
	g_test_trap_subprocess (NULL, 10 * G_USEC_PER_SEC, G_TEST_SUBPROCESS_DEFAULT);
	g_test_trap_assert_passed ();
}

int
main (int argc, char **argv)
{
	static const struct {
		const gchar *path;
		GTestFunc    func;
	} tests[] = {
		{ "/mdvi/pk/valid", test_pk_valid },
		{ "/mdvi/pk/repeat-valid", test_pk_repeat_valid },
		{ "/mdvi/pk/negative-count", test_pk_negative_count },
		{ "/mdvi/pk/repeat-past-end", test_pk_repeat_past_end },
		{ "/mdvi/pk/rows-past-end", test_pk_rows_past_end },
		{ "/mdvi/pk/truncated", test_pk_truncated },
	};
	gchar *path;
	guint i;
	int result;

	g_test_init (&argc, &argv, NULL);

	tmp_dir = g_dir_make_tmp ("test-mdvi-pk-XXXXXX", NULL);
	g_assert_nonnull (tmp_dir);

	for (i = 0; i < G_N_ELEMENTS (tests); i++)
		g_test_add_data_func (tests[i].path, &tests[i].func, run_in_subprocess);

	result = g_test_run ();

	path = g_build_filename (tmp_dir, "test.pk", NULL);
	g_unlink (path);
	g_free (path);
	g_rmdir (tmp_dir);
	g_free (tmp_dir);

	return result;
}
