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
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mdvi.h"
#include "private.h"

static int tfm_load_font __PROTO((DviParams *, DviFont *));
static int tfm_font_get_glyph __PROTO((DviParams *, DviFont *, int));

DviFontInfo tfm_font_info = {
	"TFM",
	0, /* scaling not supported by format */
	tfm_load_font,
	tfm_font_get_glyph,
	mdvi_shrink_box,
	mdvi_shrink_box,
	NULL,	/* free */
	NULL,	/* reset */
	NULL,	/* lookup */
	kpse_tfm_format,
	NULL
};

DviFontInfo ofm_font_info = {
	"OFM",
	0, /* scaling not supported by format */
	tfm_load_font,
	tfm_font_get_glyph,
	mdvi_shrink_box,
	mdvi_shrink_box,
	NULL,	/* free */
	NULL,	/* reset */
	NULL,	/* lookup */
	kpse_ofm_format,
	NULL
};

DviFontInfo afm_font_info = {
	"AFM",
	0, /* scaling not supported by format */
	tfm_load_font,
	tfm_font_get_glyph,
	mdvi_shrink_box,
	mdvi_shrink_box,
	NULL,	/* free */
	NULL,	/* reset */
	NULL,	/* lookup */
	kpse_afm_format,
	NULL
};

#define TYPENAME(font)	\
	((font)->search.info ? (font)->search.info : "none")

/*
 * Although it does not seem that way, this conversion is independent of the
 * shrinking factors, within roundoff (that's because `conv' and `vconv'
 * have already been scaled by hshrink and vshrink, respectively). We
 * should really use `dviconv' and `dvivconv', but I'm not so sure those
 * should be moved to the DviParams structure.
 */
#define XCONV(x)	FROUND(params->conv * (x) * params->hshrink)
#define YCONV(y)	FROUND(params->vconv * (y) * params->vshrink)

/* this is used quite often in several places, so I made it standalone */
int	get_tfm_chars(DviParams *params, DviFont *font, TFMInfo *info, int loaded)
{
	Int32	z, alpha, beta;
	int	n;
	DviFontChar *ch;
	TFMChar	*ptr;

	n = info->hic - info->loc + 1;
	if(n != FONT_GLYPH_COUNT(font)) {
		font->chars = mdvi_realloc(font->chars,
			n * sizeof(DviFontChar));
	}
	font->loc = info->loc;
	font->hic = info->hic;
	ch = font->chars;
	ptr = info->chars;

	/* Prepare z, alpha and beta for TFM width computation */
	TFMPREPARE(font->scale, z, alpha, beta);

	/* get the character metrics */
	for(n = info->loc; n <= info->hic; ch++, ptr++, n++) {
		int	a, b, c, d;

		ch->offset = ptr->present;
		if(ch->offset == 0)
			continue;
		/* this is what we came here for */
		ch->tfmwidth    = TFMSCALE(z, ptr->advance, alpha, beta);
		/* scale all other TFM units (so they are in DVI units) */
		a = TFMSCALE(z, ptr->left, alpha, beta);
		b = TFMSCALE(z, ptr->right, alpha, beta);
		c = TFMSCALE(z, ptr->height, alpha, beta);
		d = TFMSCALE(z, ptr->depth, alpha, beta);

		/* now convert to unscaled pixels */
		ch->width = XCONV(b - a);
		ch->height = YCONV(c - d);
		if(ch->height < 0) ch->height = -ch->height;
		ch->x = XCONV(a);
		ch->y = YCONV(c);
		/*
		 * the offset is not used, but we might as well set it to
		 * something meaningful (and it MUST be non-zero)
		 */
		ch->flags       = 0;
		ch->code        = n;
		ch->glyph.data  = NULL;
		ch->grey.data   = NULL;
		ch->shrunk.data = NULL;
		ch->loaded      = loaded;
	}

	return 0;
}

/*
 * We use this function as a last resort to find the character widths in a
 * font The DVI rendering code can correctly skip over a glyph if it knows
 * its TFM width, which is what we try to find here.
 */
static int tfm_load_font(DviParams *params, DviFont *font)
{
	TFMInfo	*tfm;
	int	type;

	switch(font->search.info->kpse_type) {
	case kpse_tfm_format:
		type = DviFontTFM;
		break;
	case kpse_afm_format:
		type = DviFontAFM;
		break;
	case kpse_ofm_format:
		type = DviFontOFM;
		break;
	default:
		return -1;
	}

	/* we don't need this */
	if(font->in) {
		fclose(font->in);
		font->in = NULL;
	}
	tfm = get_font_metrics(font->fontname, type, font->filename);
	if(tfm == NULL)
		return -1;

	if(tfm->checksum && font->checksum && tfm->checksum != font->checksum) {
		mdvi_warning(_("%s: Checksum mismatch (got %u, expected %u)\n"),
			     font->fontname, (unsigned)tfm->checksum,
			     (unsigned)font->checksum);
	}
	font->checksum = tfm->checksum;
	font->design = tfm->design;
	font->loc = 0;
	font->hic = 0;
	font->chars = NULL;
	get_tfm_chars(params, font, tfm, 1);

	/* free everything */
	free_font_metrics(tfm);

	return 0;
}

static int tfm_font_get_glyph(DviParams *params, DviFont *font, int code)
{
	DviFontChar *ch;

	ch = FONTCHAR(font, code);
	if(!glyph_present(ch))
		return -1;
	ch->glyph.x = ch->x;
	ch->glyph.y = ch->y;
	ch->glyph.w = ch->width;
	ch->glyph.h = ch->height;
	/*
	 * This has two purposes: (1) avoid unnecessary calls to this function,
	 * and (2) detect when the glyph data for a TFM font is actually used
	 * (we'll get a SEGV). Any occurrence of that is a bug.
	 */
	ch->glyph.data = MDVI_GLYPH_EMPTY;

	return 0;
}
