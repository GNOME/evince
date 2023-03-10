/* gf.c - GF font support */
/*
 * Copyright (C) 2000, Matias Atria
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

/* functions to read GF fonts */

#include <config.h>
#include <string.h>
#include "common.h"
#include "mdvi.h"
#include "private.h"

/* opcodes */

#define GF_PAINT0	0
#define GF_PAINT1	64
#define GF_PAINT2	65
#define GF_PAINT3	66
#define GF_BOC		67
#define GF_BOC1		68
#define GF_EOC		69
#define GF_SKIP0	70
#define GF_SKIP1	71
#define GF_SKIP2	72
#define GF_SKIP3	73
#define GF_NEW_ROW_0	74
#define GF_NEW_ROW_1	75
#define GF_NEW_ROW_MAX	238
#define GF_XXX1		239
#define GF_XXX2		240
#define GF_XXX3		241
#define GF_XXX4		242
#define GF_YYY		243
#define GF_NOOP		244
#define GF_LOC		245
#define GF_LOC0		246
#define GF_PRE		247
#define GF_POST		248
#define GF_POST_POST	249

#define GF_ID		131
#define GF_TRAILER	223

#define BLACK 1
#define WHITE 0

static int  gf_load_font __PROTO((DviParams *, DviFont *));
static int  gf_font_get_glyph __PROTO((DviParams *, DviFont *, int));

/* only symbol exported by this file */
DviFontInfo gf_font_info = {
	"GF",
	0, /* scaling not supported natively */
	gf_load_font,
	gf_font_get_glyph,
	mdvi_shrink_glyph,
	mdvi_shrink_glyph_grey,
	NULL,	/* free */
	NULL,	/* reset */
	NULL,	/* lookup */
	kpse_gf_format,
	NULL
};

static int gf_read_bitmap(FILE *p, DviFontChar *ch)
{
	int	op;
	int	min_n, max_n;
	int	min_m, max_m;
	int	paint_switch;
	int	x, y;
	int	bpl;
	Int32	par;
	BmUnit	*line;
	BITMAP	*map;

	fseek(p, (long)ch->offset, SEEK_SET);
	op = fuget1(p);
	if(op == GF_BOC) {
		/* skip character code */
		fuget4(p);
		/* skip pointer */
		fuget4(p);
		min_m = fsget4(p);
		max_m = fsget4(p);
		min_n = fsget4(p);
		max_n = fsget4(p);
	} else if(op == GF_BOC1) {
		/* skip character code */
		fuget1(p);
		min_m = fuget1(p); /* this is max_m - min_m */
		max_m = fuget1(p);
		min_n = fuget1(p); /* this is max_n - min_n */
		max_n = fuget1(p);
		min_m = max_m - min_m;
		min_n = max_n - min_n;
	} else {
		mdvi_error(_("GF: invalid opcode %d in character %d\n"),
			   op, ch->code);
		return -1;
	}

	ch->x = -min_m;
	ch->y = max_n;
	ch->width = max_m - min_m + 1;
	ch->height = max_n - min_n + 1;
	map = bitmap_alloc(ch->width, ch->height);

	ch->glyph.data = map;
	ch->glyph.x = ch->x;
	ch->glyph.y = ch->y;
	ch->glyph.w = ch->width;
	ch->glyph.h = ch->height;

#define COLOR(x)	((x) ? "BLACK" : "WHITE")

	paint_switch = WHITE;
	x = y = 0;
	line = map->data;
	bpl = map->stride;
	DEBUG((DBG_BITMAPS, "(gf) reading character %d\n", ch->code));
	while((op = fuget1(p)) != GF_EOC) {
		Int32	n;

		if(feof(p))
			break;
		if(op == GF_PAINT0) {
			DEBUG((DBG_BITMAPS, "(gf) Paint0 %s -> %s\n",
				COLOR(paint_switch), COLOR(!paint_switch)));
			paint_switch = !paint_switch;
		} else if(op <= GF_PAINT3) {
			if(op < GF_PAINT1)
				par = op;
			else
				par = fugetn(p, op - GF_PAINT1 + 1);
			if(y >= ch->height || x + par >= ch->width)
				goto toobig;
			/* paint everything between columns x and x + par - 1 */
			DEBUG((DBG_BITMAPS, "(gf) Paint %d %s from (%d,%d)\n",
				par, COLOR(paint_switch), x, y));
			if(paint_switch == BLACK)
				bitmap_paint_bits(line + (x / BITMAP_BITS),
					x % BITMAP_BITS, par);
			paint_switch = !paint_switch;
			x += par;
		} else if(op >= GF_NEW_ROW_0 && op <= GF_NEW_ROW_MAX) {
			y++;
			line = bm_offset(line, bpl);
			x = op - GF_NEW_ROW_0;
			paint_switch = BLACK;
			DEBUG((DBG_BITMAPS, "(gf) new_row_%d\n", x));
		} else switch(op) {
			case GF_SKIP0:
				y++;
				line = bm_offset(line, bpl);
				x = 0;
				paint_switch = WHITE;
				DEBUG((DBG_BITMAPS, "(gf) skip_0\n"));
				break;
			case GF_SKIP1:
			case GF_SKIP2:
			case GF_SKIP3:
				par = fugetn(p, op - GF_SKIP1 + 1);
				y += par + 1;
				line = bm_offset(line, (par + 1) * bpl);
				x = 0;
				paint_switch = WHITE;
				DEBUG((DBG_BITMAPS, "(gf) skip_%d\n", op - GF_SKIP1));
				break;
			case GF_XXX1:
			case GF_XXX2:
			case GF_XXX3:
			case GF_XXX4: {
#ifndef NODEBUG
				char	*s;

				s = read_string(p, op - GF_XXX1 + 1, NULL, 0);
				DEBUG((DBG_SPECIAL, "(gf) Character %d: Special \"%s\"\n",
					ch->code, s));
				mdvi_free(s);
#else
				n = fugetn(p, op - GF_XXX1 + 1);
				fseek(p, (long)n, SEEK_CUR);
#endif
				break;
			}
			case GF_YYY:
				n = fuget4(p);
				DEBUG((DBG_SPECIAL, "(gf) Character %d: MF special %u\n",
					ch->code, n));
				break;
			case GF_NOOP:
				DEBUG((DBG_BITMAPS, "(gf) no_op\n"));
				break;
			default:
				mdvi_error(_("(gf) Character %d: invalid opcode %d\n"),
					   ch->code, op);
				goto error;
		}
		/* chech that we're still inside the bitmap */
		if(x > ch->width || y > ch->height)
			goto toobig;
		DEBUG((DBG_BITMAPS, "(gf) curr_loc @ (%d,%d)\n", x, y));
	}

	if(op != GF_EOC)
		goto error;
	DEBUG((DBG_BITMAPS, "(gf) end of character %d\n", ch->code));
	return 0;

toobig:
	mdvi_error(_("(gf) character %d has an incorrect bounding box\n"),
		   ch->code);
error:
	bitmap_destroy(map);
	ch->glyph.data = NULL;
	return -1;
}

static int gf_load_font(DviParams *unused, DviFont *font)
{
	int	i;
	int	n;
	int	loc;
	int	hic;
	FILE	*p;
	Int32	word;
	int	op;
	long	alpha, beta, z;
#ifndef NODEBUG
	char	s[256];
#endif

	p = font->in;

	/* check preamble */
	loc = fuget1(p); hic = fuget1(p);
	if(loc != GF_PRE || hic != GF_ID)
		goto badgf;
	loc = fuget1(p);
#ifndef NODEBUG
	for(i = 0; i < loc; i++)
		s[i] = fuget1(p);
	s[i] = 0;
	DEBUG((DBG_FONTS, "(gf) %s: %s\n", font->fontname, s));
#else
	fseek(p, (long)loc, SEEK_CUR);
#endif
	/* now read character locators in postamble */
	if(fseek(p, (long)-1, SEEK_END) == -1)
		return -1;

	n = 0;
	while((op = fuget1(p)) == GF_TRAILER) {
		if(fseek(p, (long)-2, SEEK_CUR) < 0)
			break;
		n++;
	}
	if(op != GF_ID || n < 4)
		goto badgf;
	/* get the pointer to the postamble */
	fseek(p, (long)-5, SEEK_CUR);
	op = fuget4(p);
	/* jump to it */
	fseek(p, (long)op, SEEK_SET);
	if(fuget1(p) != GF_POST)
		goto badgf;
	/* skip pointer to last EOC */
	fuget4(p);
	/* get the design size */
	font->design = fuget4(p);
	/* the checksum */
	word = fuget4(p);
	if(word && font->checksum && font->checksum != word) {
		mdvi_warning(_("%s: bad checksum (expected %u, found %u)\n"),
			     font->fontname, font->checksum, word);
	} else if(!font->checksum)
		font->checksum = word;
	/* skip pixels per point ratio */
	fuget4(p);
	fuget4(p);
	font->chars = xnalloc(DviFontChar, 256);
	for(loc = 0; loc < 256; loc++)
		font->chars[loc].offset = 0;
	/* skip glyph "bounding box" */
	fseek(p, (long)16, SEEK_CUR);
	loc = 256;
	hic = -1;
	TFMPREPARE(font->scale, z, alpha, beta);
	while((op = fuget1(p)) != GF_POST_POST) {
		DviFontChar *ch;
		int	cc;

		/* get the character code */
		cc = fuget1(p);
		if(cc < loc)
			loc = cc;
		if(cc > hic)
			hic = cc;
		ch = &font->chars[cc];
		switch(op) {
		case GF_LOC:
			fsget4(p); /* skip dx */
			fsget4(p); /* skip dy */
			break;
		case GF_LOC0:
			fuget1(p); /* skip dx */
			           /* dy assumed 0 */
			break;
		default:
			mdvi_error(_("%s: junk in postamble\n"), font->fontname);
			goto error;
		}
		ch->code = cc;
		ch->tfmwidth = fuget4(p);
		ch->tfmwidth = TFMSCALE(ch->tfmwidth, z, alpha, beta);
		ch->offset = fuget4(p);
		if(ch->offset == -1)
			ch->offset = 0;
		/* initialize the rest of the glyph information */
		ch->x = 0;
		ch->y = 0;
		ch->width = 0;
		ch->height = 0;
		ch->glyph.data = NULL;
		ch->shrunk.data = NULL;
		ch->grey.data = NULL;
		ch->flags = 0;
		ch->loaded = 0;
	}

	if(op != GF_POST_POST)
		goto badgf;

	if(loc > 0 || hic < 255) {
		/* shrink to optimal size */
		memmove(font->chars, font->chars + loc,
			(hic - loc + 1) * sizeof(DviFontChar));
		font->chars = xresize(font->chars,
			DviFontChar, hic - loc + 1);
	}
	font->loc = loc;
	font->hic = hic;

	return 0;

badgf:
	mdvi_error(_("%s: File corrupted, or not a GF file\n"), font->fontname);
error:
	if(font->chars) {
		mdvi_free(font->chars);
		font->chars = NULL;
	}
	font->loc = font->hic = 0;
	return -1;
}

static int gf_font_get_glyph(DviParams *params, DviFont *font, int code)
{
	DviFontChar	*ch;

	if(code < font->loc || code > font->hic || !font->chars)
		return -1;
	ch = &font->chars[code - font->loc];

	if(!ch->loaded) {
		if(ch->offset == 0)
			return -1;
		DEBUG((DBG_GLYPHS, "(gf) %s: loading GF glyph for character %d\n",
			font->fontname, code));
		if(font->in == NULL && font_reopen(font) < 0)
			return -1;
		if(fseek(font->in, ch->offset, SEEK_SET) == -1)
			return -1;
		if(gf_read_bitmap(font->in, ch) < 0)
			return -1;
		ch->loaded = 1;
	}
	return 0;
}
