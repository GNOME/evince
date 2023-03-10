
/* Copyright (C) 2000, Matias Atria
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

/*
 * History:
 *
 * 11/3/2000:
 *    - First working version
 * 11/4/2000:
 *    - FIXED: entirely white/black rows were missed.
 * 11/8/2000:
 *    - TESTED: Glyphs are rendered correctly in different byte orders.
 *    - Made bitmap code much more efficient and compact.
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#include "mdvi.h"
#include "private.h"

#define PK_ID      89
#define PK_CMD_START 240
#define PK_X1     240
#define PK_X2     241
#define PK_X3     242
#define PK_X4     243
#define PK_Y      244
#define PK_POST   245
#define PK_NOOP   246
#define PK_PRE    247

#define PK_DYN_F(x)	(((x) >> 4) & 0xf)
#define PK_PACKED(x)	(PK_DYN_F(x) != 14)

static int pk_load_font __PROTO((DviParams *, DviFont *));
static int pk_font_get_glyph __PROTO((DviParams *, DviFont *, int));

static int pk_auto_generate = 1; /* this is ON by default */

typedef struct {
	char	currbyte;
	char	nybpos;
	int	dyn_f;
} pkread;

static char *pk_lookup __PROTO((const char *, Ushort *, Ushort *));
static char *pk_lookupn __PROTO((const char *, Ushort *, Ushort *));

/* only symbols exported by this file */
DviFontInfo pk_font_info = {
	"PK",
	0, /* scaling not supported natively */
	pk_load_font,
	pk_font_get_glyph,
	mdvi_shrink_glyph,
	mdvi_shrink_glyph_grey,
	NULL,	/* free */
	NULL,	/* reset */
	pk_lookup,	/* lookup */
	kpse_pk_format,
	NULL
};

DviFontInfo pkn_font_info = {
	"PKN",
	0, /* scaling not supported natively */
	pk_load_font,
	pk_font_get_glyph,
	mdvi_shrink_glyph,
	mdvi_shrink_glyph_grey,
	NULL,	/* free */
	NULL,	/* reset */
	pk_lookupn,	/* lookup */
	kpse_pk_format,
	NULL
};

static char *pk_lookup(const char *name, Ushort *hdpi, Ushort *vdpi)
{
	kpse_glyph_file_type type;
	char *filename;

	if(pk_auto_generate == 0) {
		kpse_set_program_enabled(kpse_pk_format, 1, kpse_src_cmdline);
		pk_auto_generate = 1;
	}
	filename = kpse_find_glyph(name, Max(*hdpi, *vdpi),
		kpse_pk_format, &type);
	if(filename && type.source == kpse_glyph_source_fallback) {
		mdvi_free(filename);
		filename = NULL;
	} else if(filename) {
		*hdpi = *vdpi = type.dpi;
	}
	return filename;
}

static char *pk_lookupn(const char *name, Ushort *hdpi, Ushort *vdpi)
{
	kpse_glyph_file_type type;
	char *filename;

	if(pk_auto_generate) {
		kpse_set_program_enabled(kpse_pk_format, 0, kpse_src_cmdline);
		pk_auto_generate = 0;
	}
	filename = kpse_find_glyph(name, Max(*hdpi, *vdpi),
		kpse_pk_format, &type);
	if(filename && type.source == kpse_glyph_source_fallback) {
		mdvi_free(filename);
		filename = NULL;
	} else if(filename) {
		*hdpi = *vdpi = type.dpi;
	}
	return filename;
}

static inline int pk_get_nyb(FILE *p, pkread *pk)
{
	unsigned t;
	int	nb;
	char	c;

	t = c = pk->currbyte;
	nb = pk->nybpos;

	switch(nb) {
	case 0:
		c = pk->currbyte = fuget1(p);
		t = (c >> 4);
		break;
	case 1:
		t = c;
		break;
	}
	pk->nybpos = !nb;
	return (t & 0xf);
}

/*
 * this is a bit cumbersome because we have to pass around
 * the `pkread' data...
 */
static int pk_packed_num(FILE *p, pkread *pkr, int *repeat)
{
	int	i, j;
	int	dyn_f = pkr->dyn_f;

	i = pk_get_nyb(p, pkr);
	if(i == 0) {
		do {
			j = pk_get_nyb(p, pkr);
			i++;
		} while(j == 0);
		while(i-- > 0)
			j = (j << 4) + pk_get_nyb(p, pkr);
		return (j - 15 + ((13 - dyn_f) << 4) +
			dyn_f);
	} else if(i <= dyn_f)
		return i;
	else if(i < 14)
		return ((i - dyn_f - 1) << 4) +
			pk_get_nyb(p, pkr) + dyn_f + 1;
	else {
		*repeat = 1;
		if(i == 14)
			*repeat = pk_packed_num(p, pkr, repeat);
		return pk_packed_num(p, pkr, repeat);
	}
}

#define ROUND(x,y)	(((x) + (y) - 1) / (y))

static BITMAP *get_bitmap(FILE *p, int w, int h, int flags)
{
	int	i, j;
	BmUnit	*ptr;
	BITMAP	*bm;
	int	bitpos;
	int	currch;

	flags = 0; /* shut up that compiler */
	bitpos = -1;
	if((bm = bitmap_alloc(w, h)) == NULL)
		return NULL;
	DEBUG((DBG_BITMAPS, "get_bitmap(%d,%d,%d): reading raw bitmap\n",
		w, h, flags));
	ptr = bm->data;
	currch = 0;
	for(i = 0; i < h; i++) {
		BmUnit	mask;

		mask = FIRSTMASK;
		for(j = 0; j < w; j++) {
			if(bitpos < 0) {
				currch = fuget1(p);
				bitpos = 7;
			}
			if(currch & (1 << bitpos))
				*ptr |= mask;
			bitpos--;
			if(mask == LASTMASK) {
				ptr++;
				mask = FIRSTMASK;
			} else
				NEXTMASK(mask);
		}
		ptr = bm_offset(ptr, bm->stride);
	}
	return bm;
}

static BITMAP *get_packed(FILE *p, int w, int h, int flags)
{
	int	inrow, count;
	int	row;
	BITMAP	*bm;
	int	repeat_count;
	int	paint;
	pkread	pkr;

	pkr.nybpos = 0;
	pkr.currbyte = 0;
	pkr.dyn_f = PK_DYN_F(flags);
	paint = !!(flags & 0x8);

	repeat_count = 0;
	row = 0;
	inrow = w;
	if((bm = bitmap_alloc(w, h)) == NULL)
		return NULL;
	DEBUG((DBG_BITMAPS, "get_packed(%d,%d,%d): reading packed glyph\n",
		w, h, flags));
	while(row < h) {
		int	i = 0;

		count = pk_packed_num(p, &pkr, &i);
		if(i > 0) {
			if(repeat_count)
				fprintf(stderr, "second repeat count for this row (had %d and got %d)\n",
					repeat_count, i);
			repeat_count = i;
		}

		if(count >= inrow) {
			Uchar	*r, *t;
			BmUnit	*a, mask;

			/* first finish current row */
			if(paint)
				bitmap_set_row(bm, row, w - inrow, inrow, paint);
			/* now copy it as many times as required */
			r = (Uchar *)bm->data + row * bm->stride;
			while(repeat_count-- > 0) {
				t = r + bm->stride;
				/* copy entire lines */
				memcpy(t, r, bm->stride);
				r = t;
				row++;
			}
			repeat_count = 0;
			/* count first row we drew */
			row++;
			/* update run count */
			count -= inrow;
			/* now r points to the beginning of the last row we finished */
			if(paint)
				mask = ~((BmUnit)0);
			else
				mask = 0;
			/* goto next row */
			a = (BmUnit *)(r + bm->stride);
			/* deal with entirely with/black rows */
			while(count >= w) {
				/* count number of atoms in a row */
				i = ROUND(w, BITMAP_BITS);
				while(i-- > 0)
					*a++ = mask;
				count -= w;
				row++;
			}
			inrow = w;
		}
		if(count > 0)
			bitmap_set_row(bm, row, w - inrow, count, paint);
		inrow -= count;
		paint = !paint;
	}
	if(row != h || inrow != w) {
		mdvi_error(_("Bad PK file: More bits than required\n"));
		bitmap_destroy(bm);
		return NULL;
	}
	return bm;
}

static BITMAP *get_char(FILE *p, int w, int h, int flags)
{
	/* check if dyn_f == 14 */
	if(((flags >> 4) & 0xf) == 14)
		return get_bitmap(p, w, h, flags);
	else
		return get_packed(p, w, h, flags);
}

/* supports any number of characters in a font */
static int pk_load_font(DviParams *unused, DviFont *font)
{
	int	i;
	int	flag_byte;
	int	hic, maxch;
	Int32	checksum;
	FILE	*p;
#ifndef NODEBUG
	char	s[256];
#endif
	long	alpha, beta, z;
	unsigned int loc;

	font->chars = xnalloc(DviFontChar, 256);
	p = font->in;
	memzero(font->chars, 256 * sizeof(DviFontChar));
	for(i = 0; i < 256; i++)
		font->chars[i].offset = 0;

	/* check the preamble */
	loc = fuget1(p); hic = fuget1(p);
	if(loc != PK_PRE || hic != PK_ID)
		goto badpk;
	i = fuget1(p);
#ifndef NODEBUG
	for(loc = 0; loc < i; loc++)
		s[loc] = fuget1(p);
	s[loc] = 0;
	DEBUG((DBG_FONTS, "(pk) %s: %s\n", font->fontname, s));
#else
	fseek(in, (long)i, SEEK_CUR);
#endif
	/* get the design size */
	font->design = fuget4(p);
	/* get the checksum */
	checksum = fuget4(p);
	if(checksum && font->checksum && font->checksum != checksum) {
		mdvi_warning(_("%s: checksum mismatch (expected %u, got %u)\n"),
			     font->fontname, font->checksum, checksum);
	} else if(!font->checksum)
		font->checksum = checksum;
	/* skip pixel per point ratios */
	fuget4(p);
	fuget4(p);
	if(feof(p))
		goto badpk;

	/* now start reading the font */
	loc = 256; hic = -1; maxch = 256;

	/* initialize alpha and beta for TFM width computation */
	TFMPREPARE(font->scale, z, alpha, beta);

	while((flag_byte = fuget1(p)) != PK_POST) {
		if(feof(p))
			break;
		if(flag_byte >= PK_CMD_START) {
			switch(flag_byte) {
			case PK_X1:
			case PK_X2:
			case PK_X3:
			case PK_X4: {
#ifndef NODEBUG
				char	*t;
				int	n;

				i = fugetn(p, flag_byte - PK_X1 + 1);
				if(i < 256)
					t = &s[0];
				else
					t = mdvi_malloc(i + 1);
				for(n = 0; n < i; n++)
					t[n] = fuget1(p);
				t[n] = 0;
				DEBUG((DBG_SPECIAL, "(pk) %s: Special \"%s\"\n",
					font->fontname, t));
				if(t != &s[0])
					mdvi_free(t);
#else
				i = fugetn(p, flag_byte - PK_X1 + 1);
				while(i-- > 0)
					fuget1(p);
#endif
				break;
			}
			case PK_Y:
				i = fuget4(p);
				DEBUG((DBG_SPECIAL, "(pk) %s: MF special %u\n",
					font->fontname, (unsigned)i));
				break;
			case PK_POST:
			case PK_NOOP:
				break;
			case PK_PRE:
				mdvi_error(_("%s: unexpected preamble\n"), font->fontname);
				goto error;
			}
		} else {
			int	pl;
			int	cc;
			int	w, h;
			int	x, y;
			int	offset;
			long	tfm;

			switch(flag_byte & 0x7) {
			case 7:
				pl = fuget4(p);
				cc = fuget4(p);
				offset = ftell(p) + pl;
				tfm = fuget4(p);
				fsget4(p); /* skip dx */
				fsget4(p); /* skip dy */
				w  = fuget4(p);
				h  = fuget4(p);
				x  = fsget4(p);
				y  = fsget4(p);
				break;
			case 4:
			case 5:
			case 6:
				pl = (flag_byte % 4) * 65536 + fuget2(p);
				cc = fuget1(p);
				offset = ftell(p) + pl;
				tfm = fuget3(p);
				fsget2(p); /* skip dx */
				           /* dy assumed 0 */
				w = fuget2(p);
				h = fuget2(p);
				x = fsget2(p);
				y = fsget2(p);
				break;
			default:
				pl = (flag_byte % 4) * 256 + fuget1(p);
				cc = fuget1(p);
				offset = ftell(p) + pl;
				tfm = fuget3(p);
				fsget1(p); /* skip dx */
				           /* dy assumed 0 */
				w = fuget1(p);
				h = fuget1(p);
				x = fsget1(p);
				y = fsget1(p);
			}
			if(feof(p))
				break;

			/* Although the PK format support bigger char codes,
                         * XeTeX and other extended TeX engines support charcodes up to
                         * 65536, while normal TeX engine supports only charcode up to 255.*/
			if (cc < 0 || cc > 65536) {
				mdvi_error (_("%s: unexpected charcode (%d)\n"),
					    font->fontname,cc);
				goto error;
			}
			if(cc < loc)
				loc = cc;
			if(cc > hic)
				hic = cc;
			if(cc > maxch) {
				font->chars = xresize(font->chars,
					DviFontChar, cc + 16);
				for(i = maxch; i < cc + 16; i++)
					font->chars[i].offset = 0;
				maxch = cc + 16;
			}
			font->chars[cc].code = cc;
			font->chars[cc].flags = flag_byte;
			font->chars[cc].offset = ftell(p);
			font->chars[cc].width = w;
			font->chars[cc].height = h;
			font->chars[cc].glyph.data = NULL;
			font->chars[cc].x = x;
			font->chars[cc].y = y;
			font->chars[cc].glyph.x = x;
			font->chars[cc].glyph.y = y;
			font->chars[cc].glyph.w = w;
			font->chars[cc].glyph.h = h;
			font->chars[cc].grey.data = NULL;
			font->chars[cc].shrunk.data = NULL;
			font->chars[cc].tfmwidth = TFMSCALE(z, tfm, alpha, beta);
			font->chars[cc].loaded = 0;
			fseek(p, (long)offset, SEEK_SET);
		}
	}
	if(flag_byte != PK_POST) {
		mdvi_error(_("%s: unexpected end of file (no postamble)\n"),
			   font->fontname);
		goto error;
	}
	while((flag_byte = fuget1(p)) != EOF) {
		if(flag_byte != PK_NOOP) {
			mdvi_error(_("invalid PK file! (junk in postamble)\n"));
			goto error;
		}
	}

	/* resize font char data */
	if(loc > 0 || hic < maxch-1) {
		memmove(font->chars, font->chars + loc,
			(hic - loc + 1) * sizeof(DviFontChar));
		font->chars = xresize(font->chars,
			DviFontChar, hic - loc + 1);
	}
	font->loc = loc;
	font->hic = hic;
	return 0;

badpk:
	mdvi_error(_("%s: File corrupted, or not a PK file\n"), font->fontname);
error:
	mdvi_free(font->chars);
	font->chars = NULL;
	font->loc = font->hic = 0;
	return -1;
}

static int pk_font_get_glyph(DviParams *params, DviFont *font, int code)
{
	DviFontChar	*ch;

	if((ch = FONTCHAR(font, code)) == NULL)
		return -1;

	if(ch->offset == 0)
		return -1;
	DEBUG((DBG_GLYPHS, "(pk) loading glyph for character %d (%dx%d) in font `%s'\n",
		code, ch->width, ch->height, font->fontname));
	if(font->in == NULL && font_reopen(font) < 0)
		return -1;
	if(!ch->width || !ch->height) {
		/* this happens for ` ' (ASCII 32) in some fonts */
		ch->glyph.x = ch->x;
		ch->glyph.y = ch->y;
		ch->glyph.w = ch->width;
		ch->glyph.h = ch->height;
		ch->glyph.data = NULL;
		return 0;
	}
	if(fseek(font->in, ch->offset, SEEK_SET) == -1)
		return -1;
	ch->glyph.data = get_char(font->in,
		ch->width, ch->height, ch->flags);
	if(ch->glyph.data) {
		/* restore original settings */
		ch->glyph.x = ch->x;
		ch->glyph.y = ch->y;
		ch->glyph.w = ch->width;
		ch->glyph.h = ch->height;
	} else
		return -1;
	ch->loaded = 1;
	return 0;
}
