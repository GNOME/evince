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

/* 
 * Type1 font support for MDVI
 *
 * We use T1lib only as a rasterizer, not to draw glyphs.
 */

#include <config.h>
#include "mdvi.h"

#ifdef WITH_TYPE1_FONTS

#include <stdio.h>
#include <t1lib.h>
#include "private.h"

static int	t1lib_initialized = 0;

typedef struct t1info {
	struct t1info *next;
	struct t1info *prev;
	char	*fontname;	/* (short) name of this font */
	int	t1id;		/* T1lib's id for this font */	
	int	hasmetrics;	/* have we processed this font? */
	TFMInfo *tfminfo;	/* TFM data is shared */
	DviFontMapInfo mapinfo;
	DviEncoding *encoding;
} T1Info;

static void  t1_font_remove __PROTO((T1Info *));
static int   t1_load_font __PROTO((DviParams *, DviFont *));
static int   t1_font_get_glyph __PROTO((DviParams *, DviFont *, int));
static void  t1_font_shrink_glyph 
		__PROTO((DviContext *, DviFont *, DviFontChar *, DviGlyph *));
static void  t1_free_data __PROTO((DviFont *));
static void  t1_reset_font __PROTO((DviFont *));
static char *t1_lookup_font __PROTO((const char *, Ushort *, Ushort *));

/* only symbol exported by this file */
DviFontInfo t1_font_info = {
	"Type1",
	1, /* scaling supported by format */
	t1_load_font,
	t1_font_get_glyph,
	t1_font_shrink_glyph,
	mdvi_shrink_glyph_grey,
	t1_free_data,
	t1_reset_font,
	t1_lookup_font,	/* lookup */
	kpse_type1_format,
	NULL
};

/* this seems good enough for most DVI files */
#define T1_HASH_SIZE	31

/* If these parameters change, we must delete all size information
 * in all fonts, and reset the device resolutions in T1lib */
static int t1lib_xdpi = -1;
static int t1lib_ydpi = -1;

static ListHead t1fonts = {NULL, NULL, 0};
static DviHashTable t1hash;

/* Type1 fonts need their own `lookup' function. Here is how it works: 
 * First we try to find the font by its given name. If that fails, we
 * query the font maps. A typical font map entry may contain the line
 * 
 * ptmr8rn Times-Roman ".82 ExtendFont TeXBase1Encoding ReEncodeFont" <8r.enc <ptmr
 *
 * which means: If you're looking for the font `ptmr8rn' load `Times-Roman'
 * which is in `ptmr' instead, and extend it by 0.82 points, then reencode
 * it with the vector TeXBase1Encoding from the file `8r.enc'. This will
 * fail if the entry looks like this:
 *
 * ptmr8rn Times-Roman ".82 ExtendFont TeXBase1Encoding ReEncodeFont" <8r.enc
 *
 * because to deal with this we would need to be able to locate the font file
 * for the `Times-Roman' font ourselves, and that's beyond the scope of mdvi.
 * But hey, we tried hard.
 */
char	*t1_lookup_font(const char *name, Ushort *hdpi, Ushort *vdpi)
{
	char	*filename;
	char	*newname;
	const char *ext;
	DviFontMapInfo info;

	DEBUG((DBG_TYPE1, "(t1) looking for `%s'\n", name));

	/* first let's try the font we were asked for */
	filename = kpse_find_file(name, kpse_type1_format, 1);
	if(filename != NULL) {
		/* we got it */
		return filename;
	}

	DEBUG((DBG_TYPE1, "(t1) %s: not found, querying font maps\n", name));	
	/* now query the fontmap */
	if(mdvi_query_fontmap(&info, name) < 0) {
		/* it's not there either */
		return NULL;
	}
	
	/* check what we got */
	if(info.fullfile) {
		DEBUG((DBG_TYPE1, "(t1) %s: found `%s' (cached)\n",
			name, info.fullfile));
		/* this is a cached lookup */
		return mdvi_strdup(info.fullfile);
	}
	
	/* no file associated to this font? */
	if(info.fontfile == NULL)
		return info.psname ? mdvi_ps_find_font(info.psname) : NULL;
		
	/* let's extract the extension */
	ext = file_extension(info.fontfile);
	if(ext && !STREQ(ext, "pfa") && !STREQ(ext, "pfb")) {
		DEBUG((DBG_TYPE1, 
			"(t1) %s: associated name `%s' is not Type1\n",
			name, info.fontfile));
		/* it's not a Type1 font */
		return NULL;
	}

	/* get the `base' name */
	if(ext) {
		newname = mdvi_strdup(info.fontfile);
		newname[ext - info.fontfile - 1] = 0;
	} else
		newname = (char *)name; /* we don't modify this */

	/* look it up */
	DEBUG((DBG_TYPE1, "(t1) looking for `%s' on behalf of `%s'\n",
		newname, name));
	filename = kpse_find_file(newname, kpse_type1_format, 1);

	/* we don't need this anymore */
	if(newname != name)
		mdvi_free(newname);
	if(filename == NULL) {
		DEBUG((DBG_TYPE1, "(t1) %s: not found\n", name));
		return NULL;
	}
	
	DEBUG((DBG_TYPE1, "(t1) %s: found as `%s'\n", name, filename));
	/* got it! let's remember this */
	mdvi_add_fontmap_file(name, filename);
	return filename;
}

static void t1_reset_resolution(int xdpi, int ydpi)
{
	int	i;
	int	nfonts;

	DEBUG((DBG_TYPE1, "(t1) resetting device resolution (current: (%d,%d))\n",
		t1lib_xdpi, t1lib_ydpi));
#if T1LIB_VERSION < 5
	nfonts = T1_Get_no_fonts();
#else
	nfonts = T1_GetNoFonts();
#endif	

	for(i = 0; i < nfonts; i++)
		T1_DeleteAllSizes(i);
	/* reset device resolutions */
	if(T1_SetDeviceResolutions((float)xdpi, (float)ydpi) < 0)
		mdvi_warning(_("(t1) failed to reset device resolution\n"));
	else
		DEBUG((DBG_TYPE1, 
			"(t1) reset successful, new resolution is (%d, %d)\n",
			xdpi, ydpi));
	t1lib_xdpi = xdpi;
	t1lib_ydpi = ydpi;
}

static void t1_reset_font(DviFont *font)
{
	T1Info *info = (T1Info *)font->private;
	
	if(info == NULL)
		return;
	DEBUG((DBG_FONTS, "(t1) resetting font `%s'\n", font->fontname));
	/* just mark the font as not having metric info. It will be reset
	 * automatically later */
	info->hasmetrics = 0;
}

static void t1_transform_font(T1Info *info)
{
	if(!info->hasmetrics && info->encoding != NULL) {
		DEBUG((DBG_TYPE1, "(t1) %s: encoding with vector `%s'\n",
			info->fontname, info->encoding->name));
		T1_DeleteAllSizes(info->t1id);
		if(T1_ReencodeFont(info->t1id, info->encoding->vector) < 0)
			mdvi_warning(_("%s: could not encode font\n"), info->fontname);
	}
	if(info->mapinfo.slant) {
		DEBUG((DBG_TYPE1, "(t1) %s: slanting by %.3f\n", 
			info->fontname,
			MDVI_FMAP_SLANT(&info->mapinfo)));
		T1_SlantFont(info->t1id, 
			MDVI_FMAP_SLANT(&info->mapinfo));
	}
	if(info->mapinfo.extend) {
		DEBUG((DBG_TYPE1, "(t1) %s: extending by %.3f\n",
			info->fontname,
			MDVI_FMAP_EXTEND(&info->mapinfo)));
		T1_ExtendFont(info->t1id, 
			MDVI_FMAP_EXTEND(&info->mapinfo));
	}		
}

/* if this function is called, we really need this font */
static int t1_really_load_font(DviParams *params, DviFont *font, T1Info *info)
{
	int	i;
	T1Info	*old;
	int	t1id;
	int	copied;
	int	status;

	DEBUG((DBG_TYPE1, "(t1) really_load_font(%s)\n", info->fontname));

	/* if the parameters changed, reset T1lib */
	if(t1lib_xdpi != params->dpi || t1lib_ydpi != params->vdpi)
		t1_reset_resolution(params->dpi, params->vdpi);

	/* if we already have a T1lib id, do nothing */
	if(info->t1id != -1) {
		info->hasmetrics = 1;
		/* apply slant and extend again */
		t1_transform_font(info);
		return 0;
	}

	/* before we even attempt to load the font, make sure we have metric
	 * data for it */
	info->tfminfo = mdvi_ps_get_metrics(info->fontname);
	if(info->tfminfo == NULL) {
		DEBUG((DBG_FONTS, 
			"(t1) %s: no metric data, font ignored\n",
			info->fontname));
		goto t1_error;
	}
	/* fix this */
	font->design = info->tfminfo->design;

	/* check if we have a font with this name (maybe at a different size) */
	old = (T1Info *)mdvi_hash_lookup(&t1hash, (unsigned char *)info->fontname);
	if(old == info) {
		/* let's avoid confusion */
		old = NULL;
	}
	if(old && old->t1id != -1) {
		/* let's take advantage of T1lib's font sharing */
		t1id = T1_CopyFont(old->t1id);
		DEBUG((DBG_TYPE1, "(t1) %s -> %d (CopyFont)\n", 
			info->fontname, t1id));
		copied = 1;
	} else {
		t1id = T1_AddFont(font->filename);
		DEBUG((DBG_TYPE1, "(t1) %s -> %d (AddFont)\n",
			info->fontname, t1id));
		copied = 0;
	}
	if(t1id < 0)
		goto t1_error;
	info->t1id = t1id;

	/* 
	 * a minor optimization: If the old font in the hash table has
	 * not been loaded yet, replace it by this one, so we can use
	 * CopyFont later.
	 */
	if(old && old->t1id == -1) {
		DEBUG((DBG_TYPE1, "(t1) font `%s' exchanged in hash table\n",
			info->fontname));
		mdvi_hash_remove(&t1hash, (unsigned char *)old->fontname);
		mdvi_hash_add(&t1hash, (unsigned char *)info->fontname, 
			info, MDVI_HASH_UNCHECKED);
	}

	/* now let T1lib load it */
	if(!copied && T1_LoadFont(info->t1id) < 0) {
		DEBUG((DBG_TYPE1, "(t1) T1_LoadFont(%d) failed with error %d\n",
			info->t1id, T1_errno));
		goto t1_error;
	}
	DEBUG((DBG_TYPE1, "(t1) T1_LoadFont(%d) -> Ok\n", info->t1id));

	/* get information from the fontmap */
	status = mdvi_query_fontmap(&info->mapinfo, info->fontname);
	if(!status && info->mapinfo.encoding)
	   	info->encoding = mdvi_request_encoding(info->mapinfo.encoding);
	t1_transform_font(info);

	i = info->tfminfo->hic - info->tfminfo->loc + 1;
	if(i != font->hic - font->loc + 1) {
		/* reset to optimal size */
		font->chars = mdvi_realloc(font->chars, i * sizeof(DviFontChar));
	}

	/* get the scaled characters metrics */
	get_tfm_chars(params, font, info->tfminfo, 0);
	info->hasmetrics = 1;
	
	DEBUG((DBG_TYPE1, "(t1) font `%s' really-loaded\n", info->fontname));
	return 0;

t1_error:
	/* some error does not allows us to use this font. We need to reset
	 * the font structure, so the font system can try to read this
	 * font in a different class */
	
	/* first destroy the private data */
	t1_font_remove(info);
	/* now reset all chars -- this is the important part */
	mdvi_free(font->chars);
	font->chars = NULL;
	font->loc = font->hic = 0;
	return -1;
}

static int init_t1lib(DviParams *params)
{
	int	t1flags;

#ifdef WORD_LITTLE_ENDIAN
	/* try making T1lib use bitmaps in our format, but if this
	 * fails we'll convert the bitmap ourselves */
	T1_SetBitmapPad(BITMAP_BITS);
#endif
	T1_SetDeviceResolutions((float)params->dpi, (float)params->vdpi);
	t1flags = IGNORE_CONFIGFILE|IGNORE_FONTDATABASE|T1_NO_AFM;
	if(DEBUGGING(TYPE1))
		t1flags |= LOGFILE;
	if(T1_InitLib(t1flags) == NULL)
		return (t1lib_initialized = -1);
	if(DEBUGGING(TYPE1)) {
		DEBUG((DBG_TYPE1, "T1lib debugging output saved in t1lib.log\n"));
		T1_SetLogLevel(T1LOG_DEBUG);
	}
	/* initialize our hash table, but don't allocate memory for it
	 * until we use it */
	mdvi_hash_init(&t1hash);
	DEBUG((DBG_TYPE1, "(t1) t1lib %s initialized -- resolution is (%d, %d), pad is %d bits\n",
		T1_GetLibIdent(), params->dpi, params->vdpi, T1_GetBitmapPad()));
	t1lib_initialized = 1;	
	t1lib_xdpi = params->dpi;
	t1lib_ydpi = params->vdpi;
	return 0;
}

static int t1_load_font(DviParams *params, DviFont *font)
{
	T1Info	*info;
	int	i;
				
	if(t1lib_initialized < 0)
		return -1;
	else if(t1lib_initialized == 0 && init_t1lib(params) < 0)
		return -1;

	if(font->in != NULL) {
		/* we don't need this */
		fclose(font->in);
		font->in = NULL;
	}

	info = xalloc(T1Info);

	/* 
	 * mark the font as `unregistered' with T1lib. It will
	 * be added when we actually use it
	 */
	info->t1id = -1;

	/* add the font to our list */
	info->fontname = font->fontname;
	info->hasmetrics = 0;
	info->encoding = NULL;
	info->mapinfo.psname = NULL;
	info->mapinfo.encoding = NULL;
	info->mapinfo.fontfile = NULL;
	info->mapinfo.extend = 0;
	info->mapinfo.slant = 0;
	info->encoding = NULL;
	
	/* create the hash table if we have not done so yet */
	if(t1hash.nbucks == 0)
		mdvi_hash_create(&t1hash, T1_HASH_SIZE);
	mdvi_hash_add(&t1hash, (unsigned char *) info->fontname, info, MDVI_HASH_UNIQUE);		
	listh_append(&t1fonts, LIST(info));

	font->private = info;
		
	/* reset everything */
	font->chars = xnalloc(DviFontChar, 256);
	font->loc = 0;
	font->hic = 255;
	for(i = 0; i < 256; i++) {
		font->chars[i].code = i;
		font->chars[i].offset = 1;
		font->chars[i].loaded = 0;
		font->chars[i].glyph.data = NULL;
		font->chars[i].shrunk.data = NULL;
		font->chars[i].grey.data = NULL;
	}
	
	return 0;
}

#define GLYPH_WIDTH(g) \
	((g)->metrics.rightSideBearing - (g)->metrics.leftSideBearing)
#define GLYPH_HEIGHT(g) \
	((g)->metrics.ascent - (g)->metrics.descent)

static inline BITMAP *t1_glyph_bitmap(GLYPH *glyph)
{
	int	w, h, pad;
	
	w = GLYPH_WIDTH(glyph);
	h = GLYPH_HEIGHT(glyph);

	if(!w || !h)
		return MDVI_GLYPH_EMPTY;

	pad = T1_GetBitmapPad();
	return bitmap_convert_lsb8((unsigned char *)glyph->bits, w, h, ROUND(w, pad) * (pad >> 3));
}

static void t1_font_shrink_glyph(DviContext *dvi, DviFont *font, DviFontChar *ch, DviGlyph *dest)
{
	double	size;
	GLYPH	*glyph;
	T1Info	*info;
	T1_TMATRIX matrix;
	
	info = (T1Info *)font->private;
	ASSERT(info != NULL);

	DEBUG((DBG_TYPE1, "(t1) shrinking glyph for character %d in `%s' (%d,%d)\n",
		ch->code, font->fontname, ch->width, ch->height));	
	size = (double)font->scale / (dvi->params.tfm_conv * 0x100000);
	size = 72.0 * size / 72.27;
	matrix.cxx = 1.0/(double)dvi->params.hshrink;
	matrix.cyy = 1.0/(double)dvi->params.vshrink;
	matrix.cxy = 0.0;
	matrix.cyx = 0.0;
	glyph = T1_SetChar(info->t1id, ch->code, (float)size, &matrix);

	dest->data = t1_glyph_bitmap(glyph);
	dest->x = -glyph->metrics.leftSideBearing;
	dest->y = glyph->metrics.ascent;
	dest->w = GLYPH_WIDTH(glyph);
	dest->h = GLYPH_HEIGHT(glyph);

#ifndef NODEBUG
	if(DEBUGGING(BITMAP_DATA)) {
		DEBUG((DBG_BITMAP_DATA, 
			"(t1) %s: t1_shrink_glyph(%d): (%dw,%dh,%dx,%dy) -> (%dw,%dh,%dx,%dy)\n",
			ch->glyph.w, ch->glyph.h, ch->glyph.x, ch->glyph.y,
			dest->w, dest->h, dest->x, dest->y));
		bitmap_print(stderr, (BITMAP *)dest->data);
	}
#endif
	/* transform the glyph - we could do this with t1lib, but we do
	 * it ourselves for now */
	font_transform_glyph(dvi->params.orientation, dest);
}

static int t1_font_get_glyph(DviParams *params, DviFont *font, int code)
{
	T1Info	*info = (T1Info *)font->private;
	GLYPH	*glyph;
	DviFontChar *ch;
	double	size;
	T1_TMATRIX matrix;
	int	dpi;
	
	ASSERT(info != NULL);
	if(!info->hasmetrics && t1_really_load_font(params, font, info) < 0)
		return -1;
	ch = FONTCHAR(font, code);	
	if(!ch || !glyph_present(ch))
		return -1;
	ch->loaded = 1;
	if(!ch->width || !ch->height) {
		ch->glyph.x = ch->x;
		ch->glyph.y = ch->y;
		ch->glyph.w = ch->width;
		ch->glyph.h = ch->height;
		ch->glyph.data = NULL;
		return 0;
	}

	/* load the glyph with T1lib (this is done only once for each glyph) */

	/* get size in TeX points (tfm_conv includes dpi and magnification) */
	size = (double)font->scale / (params->tfm_conv * 0x100000);
	/* and transform into PostScript points */
	size = 72.0 * size / 72.27;

	dpi = Max(font->hdpi, font->vdpi);
	/* we don't want the glyph to be cached twice (once by us, another by 
	 * T1lib), so we use an identity matrix to tell T1lib not to keep the
	 * glyph around */
	matrix.cxx = (double)font->hdpi / dpi;
	matrix.cyy = (double)font->vdpi / dpi;
	matrix.cxy = matrix.cyx = 0.0;
	glyph = T1_SetChar(info->t1id, ch->code, (float)size, &matrix);
	if(glyph == NULL) {
		ch->glyph.x = ch->x;
		ch->glyph.y = ch->y;
		ch->glyph.w = ch->width;
		ch->glyph.h = ch->height;
		ch->glyph.data = NULL;
		ch->missing = 1;
		return 0;
	}
	/* and make it a bitmap */
	ch->glyph.data = t1_glyph_bitmap(glyph);
	ch->glyph.x = -glyph->metrics.leftSideBearing;
	ch->glyph.y = glyph->metrics.ascent;
	ch->glyph.w = GLYPH_WIDTH(glyph);
	ch->glyph.h = GLYPH_HEIGHT(glyph);

	/* let's also fix the glyph's origin 
	 * (which is not contained in the TFM) */
	ch->x = ch->glyph.x;
	ch->y = ch->glyph.y;
	/* let's fix these too */
	ch->width = ch->glyph.w;
	ch->height = ch->glyph.h;
		
	return 0;
}

static void t1_font_remove(T1Info *info)
{
	T1Info	*old;
	
	/* first remove it from our list */
	listh_remove(&t1fonts, LIST(info));

	/* it it's in the hash table, we may need to replace this by another font */
	old = (T1Info *)mdvi_hash_lookup(&t1hash, (unsigned char *)info->fontname);
	if(old == info) {
		mdvi_hash_remove(&t1hash, (unsigned char *) info->fontname);
		/* go through the list and see if there is another 
		 * font with this name */
		for(old = (T1Info *)t1fonts.head; old; old = old->next)
			if(STREQ(old->fontname, info->fontname))
				break;
		if(old != NULL)
			mdvi_hash_add(&t1hash, (unsigned char *) old->fontname, old, 
				MDVI_HASH_UNCHECKED);
	}
	/* release our encoding vector */
	if(info->encoding) {
		DEBUG((DBG_TYPE1, "(t1) %s: releasing vector `%s'\n",
			info->fontname, info->encoding->name));
		mdvi_release_encoding(info->encoding, 1);
	}

	/* now get rid of it */
	if(info->t1id != -1) {
		DEBUG((DBG_TYPE1, "(t1) %s: T1_DeleteFont(%d)\n",
			info->fontname, info->t1id));
		T1_DeleteFont(info->t1id);
	} else
		DEBUG((DBG_TYPE1, "(t1) %s: not loaded yet, DeleteFont skipped\n",
			info->fontname));

	if(info->tfminfo)
		free_font_metrics(info->tfminfo);
	/*mdvi_free(info->fontname);*/
	mdvi_free(info);
}

static void t1_free_data(DviFont *font)
{
	/* called after all the glyphs are destroyed */

	if(font->private == NULL) {
		/* this is perfectly normal, it just means the font has 
		 * not been requested by MDVI yet */
		return;
	}
	
	/* destroy this data */

	t1_font_remove((T1Info *)font->private);
	font->private = NULL;

	/* 
	 * if this is the last T1 font, reset the T1 library
	 * It is important that we do this, because this is will be called
	 * when the resolution or the magnification changes.
	 */
	if(t1fonts.count == 0) {
		DEBUG((DBG_TYPE1, "(t1) last font removed -- closing T1lib\n"));
		T1_CloseLib();
		t1lib_initialized = 0;
		t1lib_xdpi = -1;
		t1lib_ydpi = -1;
	}
}

#endif /* WITH_TYPE1_FONTS */
