/* vf.c -- VF font support */
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

#include <config.h>
#include <string.h>

#include "mdvi.h"
#include "private.h"

static int  vf_load_font __PROTO((DviParams *, DviFont *));
static void vf_free_macros __PROTO((DviFont *));

/* only symbol exported by this file */
DviFontInfo vf_font_info = {
	"VF",
	1, /* virtual fonts scale just fine */
	vf_load_font,
	NULL,	/* get_glyph */
	NULL,	/* shrink0 */
	NULL,	/* shrink1 */
	vf_free_macros,
	NULL,	/* reset */
	NULL,	/* lookup */
	kpse_vf_format,
	NULL
};

DviFontInfo ovf_font_info = {
	"OVF",
	1, /* virtual fonts scale just fine */
	vf_load_font,
	NULL,	/* get_glyph */
	NULL,	/* shrink0 */
	NULL,	/* shrink1 */
	vf_free_macros,
	NULL,	/* reset */
	NULL,	/* lookup */
	kpse_ovf_format,
	NULL
};

static int vf_load_font(DviParams *params, DviFont *font)
{
	FILE	*p;
	Uchar	*macros;
	int	msize;
	int	mlen;
	Int32	checksum;
	long	alpha, beta, z;
	int	op;
	int	i;
	int	nchars;
	int	loc, hic;
	DviFontRef *last;

	macros = NULL;
	msize = mlen = 0;
	p = font->in;

	if(fuget1(p) != 247 || fuget1(p) != 202)
		goto badvf;
	mlen = fuget1(p);
	fseek(p, (long)mlen, SEEK_CUR);
	checksum = fuget4(p);
	if(checksum && font->checksum && checksum != font->checksum) {
		mdvi_warning(_("%s: Checksum mismatch (expected %u, got %u)\n"),
			     font->fontname, font->checksum, checksum);
	} else if(!font->checksum)
		font->checksum = checksum;
	font->design = fuget4(p);

	/* read all the fonts in the preamble */
	last = NULL;

	/* initialize alpha, beta and z for TFM width computation */
	TFMPREPARE(font->scale, z, alpha, beta);

	op = fuget1(p);
	while(op >= DVI_FNT_DEF1 && op <= DVI_FNT_DEF4) {
		DviFontRef *ref;
		Int32	scale, design;
		Uint32	checksum;
		int	id;
		int	n;
		int	hdpi;
		int	vdpi;
		char	*name;

		/* process fnt_def commands */

		id = fugetn(p, op - DVI_FNT_DEF1 + 1);
		checksum = fuget4(p);
		scale = fuget4(p);
		design = fuget4(p);

		/* scale this font according to our parent's scale */
		scale = TFMSCALE(scale, z, alpha, beta);
		design = FROUND(params->tfm_conv * design);

		/* compute the resolution */
		hdpi = FROUND(params->mag * params->dpi * scale / design);
		vdpi = FROUND(params->mag * params->vdpi * scale / design);
		n = fuget1(p) + fuget1(p);
		name = mdvi_malloc(n + 1);
		fread(name, 1, n, p);
		name[n] = 0;
		DEBUG((DBG_FONTS, "(vf) %s: defined font `%s' at %.1fpt (%dx%d dpi)\n",
			font->fontname, name,
			(double)scale / (params->tfm_conv * 0x100000), hdpi, vdpi));

		/* get the font */
		ref = font_reference(params, id, name, checksum, hdpi, vdpi, scale);
		if(ref == NULL) {
			mdvi_error(_("(vf) %s: could not load font `%s'\n"),
				   font->fontname, name);
			goto error;
		}
		mdvi_free(name);
		if(last == NULL)
			font->subfonts = last = ref;
		else {
			last->next = ref;
			last = ref;
		}
		ref->next = NULL;
		op = fuget1(p);
	}

	if(op >= DVI_FNT_DEF1 && op <= DVI_FNT_DEF4)
		goto error;

	/* This function correctly reads both .vf and .ovf files */

	font->chars = xnalloc(DviFontChar, 256);
	for(i = 0; i < 256; i++)
		font->chars[i].offset = 0;
	nchars = 256;
	loc = -1; hic = -1;
	/* now read the characters themselves */
	while(op <= 242) {
		int	pl;
		Int32	cc;
		Int32	tfm;

		if(op == 242) {
			pl = fuget4(p);
			cc = fuget4(p);
			tfm = fuget4(p);
		} else {
			pl = op;
			cc = fuget1(p);
			tfm = fuget3(p);
		}
		if (cc < 0 || cc > 65536) {
			/* TeX engines do not support char codes bigger than 65535 */
			mdvi_error(_("(vf) %s: unexpected character %d\n"),
				   font->fontname, cc);
			goto error;
		}
		if(loc < 0 || cc < loc)
			loc = cc;
		if(hic < 0 || cc > hic)
			hic = cc;
		if(cc >= nchars) {
			font->chars = xresize(font->chars,
				DviFontChar, cc + 16);
			for(i = nchars; i < cc + 16; i++)
				font->chars[i].offset = 0;
			nchars = cc + 16;
		}
		if(font->chars[cc].offset) {
			mdvi_error(_("(vf) %s: character %d redefined\n"),
				   font->fontname, cc);
			goto error;
		}

		DEBUG((DBG_GLYPHS, "(vf) %s: defined character %d (macro length %d)\n",
			font->fontname, cc, pl));
		font->chars[cc].width = pl + 1;
		font->chars[cc].code = cc;
		font->chars[cc].tfmwidth = TFMSCALE(tfm, z, alpha, beta);
		font->chars[cc].offset = mlen;
		font->chars[cc].loaded = 1;
		if(mlen + pl + 1 > msize) {
			msize = mlen + pl + 256;
			macros = xresize(macros, Uchar, msize);
		}
		if(pl && fread(macros + mlen, 1, pl, p) != pl)
			break;
		macros[mlen+pl] = DVI_EOP;
		mlen += pl + 1;
		op = fuget1(p);
	}
	if(op != 248) {
		mdvi_error(_("(vf) %s: no postamble\n"), font->fontname);
		goto error;
	}

	/* make macro memory just big enough */
	if(msize > mlen) {
		macros = xresize(macros, Uchar, mlen);
		msize = mlen;
	}

	DEBUG((DBG_FONTS|DBG_GLYPHS,
		"(vf) %s: macros use %d bytes\n", font->fontname, msize));

	if(loc > 0 || hic < nchars-1) {
		memmove(font->chars, font->chars + loc,
			(hic - loc + 1) * sizeof(DviFontChar));
		font->chars = xresize(font->chars,
			DviFontChar, hic - loc + 1);
	}
	font->loc = loc;
	font->hic = hic;
	font->private = macros;

	return 0;

badvf:
	mdvi_error(_("%s: File corrupted, or not a VF file.\n"), font->fontname);
error:
	if(font->chars)
		mdvi_free(font->chars);
	if(macros)
		mdvi_free(macros);
	return -1;
}

static void vf_free_macros(DviFont *font)
{
	mdvi_free(font->private);
}
