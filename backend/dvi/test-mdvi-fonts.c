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
 * Loads crafted font files (TFM, OFM, VF, PK) with mdvi-lib. Valid files
 * must load, and malformed ones must be rejected without reading or
 * writing out of bounds. Each case runs in a subprocess, so a crash is
 * reported as a failure; build with -Db_sanitize=address to catch reads
 * out of bounds that do not crash.
 */

#include "config.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>

#include "mdvi.h"

extern DviFontInfo vf_font_info;
extern DviFontInfo pk_font_info;

static gchar *tmp_dir;

/* Big-endian writers */

static void
put (GByteArray *a, guint32 value, int n)
{
	while (n-- > 0) {
		guint8 byte = (value >> (8 * n)) & 0xff;

		g_byte_array_append (a, &byte, 1);
	}
}

static gchar *
write_file (const gchar *name,
	    GByteArray  *a,
	    gsize        length)
{
	gchar *path = g_build_filename (tmp_dir, name, NULL);
	GError *error = NULL;

	g_file_set_contents (path, (const gchar *) a->data, MIN (length, a->len), &error);
	g_assert_no_error (error);

	return path;
}

/*
 * TFM
 */

typedef struct {
	int lh;
	int bc, ec;
	int nw, nh, nd;
	int width_index;
	int coding_length;
	gboolean bad_lf;
} TfmSpec;

static const TfmSpec tfm_valid = {
	.lh = 18, .bc = 65, .ec = 65, .nw = 2, .nh = 2, .nd = 1,
	.width_index = 1, .coding_length = 8,
};

#define TFM_WIDTH 0x00100000

static GByteArray *
build_tfm (const TfmSpec *spec)
{
	GByteArray *a = g_byte_array_new ();
	int n_chars = MAX (spec->ec - spec->bc + 1, 0);
	int lf = 6 + spec->lh + n_chars + spec->nw + spec->nh + spec->nd;
	int i;

	put (a, spec->bad_lf ? lf + 100 : lf, 2);
	put (a, spec->lh, 2);
	put (a, spec->bc, 2);
	put (a, spec->ec, 2);
	put (a, spec->nw, 2);
	put (a, spec->nh, 2);
	put (a, spec->nd, 2);
	put (a, 0, 2);			/* italics */
	put (a, 0, 2);			/* lig/kern */
	put (a, 0, 2);			/* kern */
	put (a, 0, 2);			/* extensible */
	put (a, 0, 2);			/* parameters */

	/* header: checksum, design size, coding scheme, family */
	put (a, 0x12345678, 4);
	put (a, 10 << 20, 4);
	for (i = 2; i < spec->lh; i++) {
		if (i == 2) {
			put (a, ((guint32) (guint8) spec->coding_length << 24) | ('T' << 16) | ('E' << 8) | 'X', 4);
		} else if (i == 3) {
			put (a, (' ' << 24) | ('T' << 16) | ('E' << 8) | 'X', 4);
		} else if (i == 4) {
			put (a, ('T' << 24), 4);
		} else if (i == 12) {
			put (a, (3 << 24) | ('C' << 16) | ('M' << 8) | 'R', 4);
		} else {
			put (a, 0, 4);
		}
	}

	/* char info: width index, height and depth index */
	for (i = 0; i < n_chars; i++)
		put (a, ((guint32) spec->width_index << 24) | (0x10 << 16), 4);

	for (i = 0; i < spec->nw; i++)
		put (a, i == 0 ? 0 : TFM_WIDTH, 4);
	for (i = 0; i < spec->nh; i++)
		put (a, i == 0 ? 0 : 0x80000, 4);
	for (i = 0; i < spec->nd; i++)
		put (a, i == 0 ? 0 : 0x20000, 4);

	return a;
}

static int
load_tfm (const TfmSpec *spec,
	  gsize          length,
	  TFMInfo       *info)
{
	GByteArray *a = build_tfm (spec);
	gchar *path = write_file ("test.tfm", a, length);
	int status;

	memset (info, 0, sizeof (TFMInfo));
	status = tfm_load_file (path, info);

	g_byte_array_unref (a);
	g_free (path);

	return status;
}

static void
test_tfm_valid (void)
{
	TFMInfo info;

	g_assert_cmpint (load_tfm (&tfm_valid, G_MAXSIZE, &info), ==, 0);
	g_assert_cmpint (info.loc, ==, 65);
	g_assert_cmpint (info.hic, ==, 65);
	g_assert_cmpint (info.chars[0].advance, ==, TFM_WIDTH);
	g_assert_true (info.chars[0].present);
	g_assert_cmpstr (info.coding, ==, "TEX TEXT");
	g_assert_cmpstr (info.family, ==, "CMR");
	mdvi_free (info.chars);
}

static void
test_tfm_truncated (void)
{
	TFMInfo info;

	/* not even the table sizes */
	g_assert_cmpint (load_tfm (&tfm_valid, 10, &info), ==, -1);
}

static void
test_tfm_tables_past_end (void)
{
	TFMInfo info;
	TfmSpec spec = tfm_valid;
	GByteArray *a = build_tfm (&spec);
	gsize full = a->len;

	g_byte_array_unref (a);
	/* the table sizes add up, but the file ends before them */
	g_assert_cmpint (load_tfm (&spec, full - 8, &info), ==, -1);
}

static void
test_tfm_bad_lf (void)
{
	TFMInfo info;
	TfmSpec spec = tfm_valid;

	spec.bad_lf = TRUE;
	g_assert_cmpint (load_tfm (&spec, G_MAXSIZE, &info), ==, -1);
}

static void
test_tfm_width_index (void)
{
	TFMInfo info;
	TfmSpec spec = tfm_valid;

	/* only two widths, but the character uses the 201st */
	spec.width_index = 200;
	g_assert_cmpint (load_tfm (&spec, G_MAXSIZE, &info), ==, -1);
}

static void
test_tfm_negative_coding_length (void)
{
	TFMInfo info;
	TfmSpec spec = tfm_valid;

	/* a negative length used to move the read pointer back */
	spec.coding_length = 0x80;
	g_assert_cmpint (load_tfm (&spec, G_MAXSIZE, &info), ==, 0);
	g_assert_cmpstr (info.family, ==, "CMR");
	mdvi_free (info.chars);
}

static void
test_tfm_short_header (void)
{
	TFMInfo info;
	TfmSpec spec = tfm_valid;

	/* the coding scheme is longer than the 3-word header */
	spec.lh = 3;
	spec.coding_length = 39;
	g_assert_cmpint (load_tfm (&spec, G_MAXSIZE, &info), ==, 0);
	g_assert_cmpint (strlen (info.coding), ==, 3);
	mdvi_free (info.chars);
}

static void
test_tfm_empty (void)
{
	TFMInfo info;
	TfmSpec spec = tfm_valid;

	/* bc = ec + 1: no characters; this used to abort */
	spec.bc = 66;
	g_assert_cmpint (load_tfm (&spec, G_MAXSIZE, &info), ==, -1);
}

/*
 * OFM
 */

#define OFM_WIDTH 0x00200000

typedef struct {
	int level;
	int bc, ec;
	int nw, nh, nd;
	int width_index;
	int n_entries;
} OfmSpec;

static const OfmSpec ofm0_valid = {
	.level = 0, .bc = 65, .ec = 65, .nw = 2, .nh = 2, .nd = 2,
	.width_index = 1, .n_entries = 1,
};

static GByteArray *
build_ofm (const OfmSpec *spec)
{
	GByteArray *a = g_byte_array_new ();
	int n_chars = spec->ec - spec->bc + 1;
	int lh = 2;
	int i;

	put (a, 0, 2);
	put (a, spec->level, 2);

	if (spec->level == 0) {
		put (a, 14 + lh + 2 * n_chars + spec->nw + spec->nh + spec->nd, 4);
		put (a, lh, 4);
		put (a, spec->bc, 4);
		put (a, spec->ec, 4);
		put (a, spec->nw, 4);
		put (a, spec->nh, 4);
		put (a, spec->nd, 4);
		for (i = 0; i < 6; i++)	/* italics ... font direction */
			put (a, 0, 4);
		put (a, 0x12345678, 4);
		put (a, 10 << 20, 4);
		for (i = 0; i < n_chars; i++) {
			put (a, spec->width_index, 2);
			put (a, 1, 1);
			put (a, 1, 1);
			put (a, 0, 4);
		}
	} else {
		put (a, lh, 4);
		put (a, spec->bc, 4);
		put (a, spec->ec, 4);
		put (a, spec->nw, 4);
		put (a, spec->nh, 4);
		put (a, spec->nd, 4);
		for (i = 0; i < 6; i++)	/* italics ... font direction */
			put (a, 0, 4);
		put (a, 16 + lh, 4);	/* nco: words before the char info */
		put (a, spec->n_entries, 4);
		put (a, 0, 4);		/* npc */
		put (a, 0x12345678, 4);
		put (a, 10 << 20, 4);
		for (i = 0; i < spec->n_entries; i++) {
			put (a, spec->width_index, 2);
			put (a, 1, 1);
			put (a, 1, 1);
			put (a, 0, 4);
			put (a, 0, 2);	/* no repeats */
		}
	}

	for (i = 0; i < spec->nw; i++)
		put (a, i == 0 ? 0 : OFM_WIDTH, 4);
	for (i = 0; i < spec->nh; i++)
		put (a, i == 0 ? 0 : 0x80000, 4);
	for (i = 0; i < spec->nd; i++)
		put (a, i == 0 ? 0 : 0x20000, 4);

	return a;
}

static TFMInfo *
load_ofm (const OfmSpec *spec)
{
	GByteArray *a = build_ofm (spec);
	gchar *path = write_file ("test.ofm", a, G_MAXSIZE);
	TFMInfo *info;

	info = get_font_metrics ("test", DviFontOFM, path);

	g_byte_array_unref (a);
	g_free (path);

	return info;
}

static void
test_ofm_valid (void)
{
	OfmSpec spec = ofm0_valid;
	TFMInfo *info;

	info = load_ofm (&spec);
	g_assert_nonnull (info);
	g_assert_cmpint (info->chars[0].advance, ==, OFM_WIDTH);
}

static void
test_ofm_level1_valid (void)
{
	OfmSpec spec = ofm0_valid;
	TFMInfo *info;

	/* the char info entries fill the table exactly */
	spec.level = 1;
	info = load_ofm (&spec);
	g_assert_nonnull (info);
	g_assert_cmpint (info->chars[0].advance, ==, OFM_WIDTH);
}

static void
test_ofm_width_index (void)
{
	OfmSpec spec = ofm0_valid;

	spec.width_index = 300;
	g_assert_null (load_ofm (&spec));

	/* level 1, with a char left out so the table is not full */
	spec.level = 1;
	spec.ec = 66;
	g_assert_null (load_ofm (&spec));
}

static void
test_ofm_bad_sizes (void)
{
	OfmSpec spec = ofm0_valid;

	spec.nh = 0;
	g_assert_null (load_ofm (&spec));

	spec = ofm0_valid;
	spec.level = 1;
	spec.nd = 1000;
	g_assert_null (load_ofm (&spec));
}

/*
 * VF and PK, loaded from an open file as the font search does
 */

static int
load_font (DviFontInfo *finfo,
	   GByteArray  *a,
	   DviFont     *font)
{
	DviParams params;
	gchar *path = write_file ("test.font", a, G_MAXSIZE);
	int status;

	memset (&params, 0, sizeof (params));
	params.mag = 1.0;
	params.tfm_conv = 1.0;
	params.dpi = params.vdpi = 600;

	memset (font, 0, sizeof (DviFont));
	font->fontname = (char *) "test";
	font->scale = 10 << 20;
	font->in = fopen (path, "rb");
	g_assert_nonnull (font->in);

	status = finfo->load (&params, font);

	fclose (font->in);
	g_free (path);

	return status;
}

static GByteArray *
build_vf (gboolean with_char)
{
	GByteArray *a = g_byte_array_new ();

	put (a, 247, 1);		/* pre */
	put (a, 202, 1);		/* VF id */
	put (a, 0, 1);			/* no comment */
	put (a, 0x12345678, 4);
	put (a, 10 << 20, 4);
	if (with_char) {
		put (a, 2, 1);		/* short char, 2 bytes of DVI */
		put (a, 65, 1);
		put (a, 0x100000, 3);
		put (a, 138, 1);	/* nop */
		put (a, 138, 1);
	}
	put (a, 248, 1);		/* post */

	return a;
}

static void
test_vf_valid (void)
{
	GByteArray *a = build_vf (TRUE);
	DviFont font;

	g_assert_cmpint (load_font (&vf_font_info, a, &font), ==, 0);
	g_assert_cmpint (font.loc, ==, 65);
	g_assert_cmpint (font.hic, ==, 65);
	mdvi_free (font.chars);
	mdvi_free (font.private);
	g_byte_array_unref (a);
}

static void
test_vf_no_chars (void)
{
	GByteArray *a = build_vf (FALSE);
	DviFont font;

	/* this used to read before the start of the char array */
	g_assert_cmpint (load_font (&vf_font_info, a, &font), ==, -1);
	g_assert_null (font.chars);
	g_byte_array_unref (a);
}

static void
test_pk_truncated_preamble (void)
{
	GByteArray *a = g_byte_array_new ();
	DviFont font;

	/* the comment length is EOF; this overflowed a stack buffer */
	put (a, 247, 1);
	put (a, 89, 1);
	g_assert_cmpint (load_font (&pk_font_info, a, &font), ==, -1);
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

	g_test_trap_subprocess (NULL, 0, 0);
	g_test_trap_assert_passed ();
}

int
main (int argc, char **argv)
{
	static const struct {
		const gchar *path;
		GTestFunc    func;
	} tests[] = {
		{ "/mdvi/tfm/valid", test_tfm_valid },
		{ "/mdvi/tfm/truncated", test_tfm_truncated },
		{ "/mdvi/tfm/tables-past-end", test_tfm_tables_past_end },
		{ "/mdvi/tfm/bad-lf", test_tfm_bad_lf },
		{ "/mdvi/tfm/width-index", test_tfm_width_index },
		{ "/mdvi/tfm/negative-coding-length", test_tfm_negative_coding_length },
		{ "/mdvi/tfm/short-header", test_tfm_short_header },
		{ "/mdvi/tfm/empty", test_tfm_empty },
		{ "/mdvi/ofm/valid", test_ofm_valid },
		{ "/mdvi/ofm/level1-valid", test_ofm_level1_valid },
		{ "/mdvi/ofm/width-index", test_ofm_width_index },
		{ "/mdvi/ofm/bad-sizes", test_ofm_bad_sizes },
		{ "/mdvi/vf/valid", test_vf_valid },
		{ "/mdvi/vf/no-chars", test_vf_no_chars },
		{ "/mdvi/pk/truncated-preamble", test_pk_truncated_preamble },
	};
	guint i;
	int result;

	g_test_init (&argc, &argv, NULL);

	tmp_dir = g_dir_make_tmp ("test-mdvi-fonts-XXXXXX", NULL);
	g_assert_nonnull (tmp_dir);

	for (i = 0; i < G_N_ELEMENTS (tests); i++)
		g_test_add_data_func (tests[i].path, &tests[i].func, run_in_subprocess);

	result = g_test_run ();

	{
		GDir *dir = g_dir_open (tmp_dir, 0, NULL);
		const gchar *name;

		while (dir != NULL && (name = g_dir_read_name (dir)) != NULL) {
			gchar *path = g_build_filename (tmp_dir, name, NULL);

			g_unlink (path);
			g_free (path);
		}
		if (dir != NULL)
			g_dir_close (dir);
		g_rmdir (tmp_dir);
		g_free (tmp_dir);
	}

	return result;
}
