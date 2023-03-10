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
#include "mdvi.h"

#ifdef WITH_TRUETYPE_FONTS

#include <string.h>
#include <freetype.h>
#include <ftxpost.h>
#include <ftxerr18.h>

#include "private.h"

static TT_Engine tt_handle;
static int initialized = 0;

typedef struct ftinfo {
	struct ftinfo *next;
	struct ftinfo *prev;
	char	*fontname;
	char	*fmfname;
	TT_Face	face;
	TT_Instance	instance;
	TT_Glyph	glyph;
	int	hasmetrics;
	int	loaded;
	int	fmftype;
	TFMInfo *tfminfo;
	DviFontMapInfo mapinfo;
	DviEncoding	*encoding;
} FTInfo;

static int tt_load_font __PROTO((DviParams *, DviFont *));
static int tt_font_get_glyph __PROTO((DviParams *, DviFont *, int));
static void tt_free_data __PROTO((DviFont *));
static void tt_reset_font __PROTO((DviFont *));
static void tt_shrink_glyph
	__PROTO((DviContext *, DviFont *, DviFontChar *, DviGlyph *));
static void tt_font_remove __PROTO((FTInfo *));

DviFontInfo tt_font_info = {
	"TT",
	0,
	tt_load_font,
	tt_font_get_glyph,
	tt_shrink_glyph,
	mdvi_shrink_glyph_grey,
	tt_free_data,	/* free */
	tt_reset_font,	/* reset */
	NULL,	/* lookup */
	kpse_truetype_format,
	NULL
};

#define FT_HASH_SIZE	31

static ListHead	ttfonts = {NULL, NULL, 0};

static int init_freetype(void)
{
	TT_Error code;

	ASSERT(initialized == 0);
	code = TT_Init_FreeType(&tt_handle);
	if(code) {
		DEBUG((DBG_TT, "(tt) Init_Freetype: error %d\n", code));
		return -1;
	}
	code = TT_Init_Post_Extension(tt_handle);
	if(code) {
		TT_Done_FreeType(tt_handle);
		return -1;
	}
	/* we're on */
	initialized = 1;
	return 0;
}

static void tt_encode_font(DviFont *font, FTInfo *info)
{
	TT_Face_Properties prop;
	int	i;

	if(TT_Get_Face_Properties(info->face, &prop))
		return;

	for(i = 0; i < prop.num_Glyphs; i++) {
		char	*string;
		int	ndx;

		if(TT_Get_PS_Name(info->face, i, &string))
			continue;
		ndx = mdvi_encode_glyph(info->encoding, string);
		if(ndx < font->loc || ndx > font->hic)
			continue;
		font->chars[ndx - font->loc].code = i;
	}
}

static int tt_really_load_font(DviParams *params, DviFont *font, FTInfo *info)
{
	DviFontChar *ch;
	TFMChar *ptr;
	Int32	z, alpha, beta;
	int	i;
	FTInfo	*old;
	TT_Error status;
	double	point_size;
	static int warned = 0;
	TT_CharMap cmap;
	TT_Face_Properties props;
	int	map_found;

	DEBUG((DBG_TT, "(tt) really_load_font(%s)\n", info->fontname));

	/* get the point size */
	point_size = (double)font->scale / (params->tfm_conv * 0x100000);
	point_size = 72.0 * point_size / 72.27;
	if(info->loaded) {
		/* just reset the size info */
		TT_Set_Instance_Resolutions(info->instance,
			params->dpi, params->vdpi);
		TT_Set_Instance_CharSize(info->instance, FROUND(point_size * 64));
		/* FIXME: should extend/slant again */
		info->hasmetrics = 1;
		return 0;
	}

	/* load the face */
	DEBUG((DBG_TT, "(tt) loading new face `%s'\n",
		info->fontname));
	status = TT_Open_Face(tt_handle, font->filename, &info->face);
	if(status) {
		mdvi_warning(_("(tt) %s: could not load face: %s\n"),
			info->fontname, TT_ErrToString18(status));
		return -1;
	}

	/* create a new instance of this face */
	status = TT_New_Instance(info->face, &info->instance);
	if(status) {
		mdvi_warning(_("(tt) %s: could not create face: %s\n"),
			info->fontname, TT_ErrToString18(status));
		TT_Close_Face(info->face);
		return -1;
	}

	/* create a glyph */
	status = TT_New_Glyph(info->face, &info->glyph);
	if(status) {
		mdvi_warning(_("(tt) %s: could not create glyph: %s\n"),
			info->fontname, TT_ErrToString18(status));
		goto tt_error;
	}

	/*
	 * We'll try to find a Unicode charmap. It's not that important that we
	 * actually find one, especially if the fontmap files are installed
	 * properly, but it's good to have some predefined behaviour
	 */
	TT_Get_Face_Properties(info->face, &props);

	map_found = -1;
	for(i = 0; map_found < 0 && i < props.num_CharMaps; i++) {
		TT_UShort	pid, eid;

		TT_Get_CharMap_ID(info->face, i, &pid, &eid);
		switch(pid) {
		case TT_PLATFORM_APPLE_UNICODE:
			map_found = i;
			break;
		case TT_PLATFORM_ISO:
			if(eid == TT_ISO_ID_7BIT_ASCII ||
			   eid == TT_ISO_ID_8859_1)
			   	map_found = 1;
			break;
		case TT_PLATFORM_MICROSOFT:
			if(eid == TT_MS_ID_UNICODE_CS)
				map_found = 1;
			break;
		}
	}
	if(map_found < 0) {
		mdvi_warning(_("(tt) %s: no acceptable map found, using #0\n"),
			info->fontname);
		map_found = 0;
	}
	DEBUG((DBG_TT, "(tt) %s: using charmap #%d\n",
		info->fontname, map_found));
	TT_Get_CharMap(info->face, map_found, &cmap);

	DEBUG((DBG_TT, "(tt) %s: Set_Char_Size(%.2f, %d, %d)\n",
		font->fontname, point_size, font->hdpi, font->vdpi));
	status = TT_Set_Instance_Resolutions(info->instance,
			params->dpi, params->vdpi);
	if(status) {
		error(_("(tt) %s: could not set resolution: %s\n"),
			info->fontname, TT_ErrToString18(status));
		goto tt_error;
	}
	status = TT_Set_Instance_CharSize(info->instance,
			FROUND(point_size * 64));
	if(status) {
		error(_("(tt) %s: could not set point size: %s\n"),
			info->fontname, TT_ErrToString18(status));
		goto tt_error;
	}

	/* after this point we don't fail */

	/* get information from the fontmap */
	status = mdvi_query_fontmap(&info->mapinfo, info->fontname);
	if(!status && info->mapinfo.encoding)
		info->encoding = mdvi_request_encoding(info->mapinfo.encoding);
	else
		info->encoding = NULL;

	if(info->encoding != NULL) {
		TT_Post	post;

		status = TT_Load_PS_Names(info->face, &post);
		if(status) {
			mdvi_warning(_("(tt) %s: could not load PS name table\n"),
				info->fontname);
			mdvi_release_encoding(info->encoding, 0);
			info->encoding = NULL;
		}
	}

	/* get the metrics. If this fails, it's not fatal, but certainly bad */
	info->tfminfo = get_font_metrics(info->fontname,
		info->fmftype, info->fmfname);

	if(info->tfminfo == NULL) {
		mdvi_warning("(tt) %s: no metrics data, font ignored\n",
			info->fontname);
		goto tt_error;
	}
	/* fix this */
	font->design = info->tfminfo->design;

	/* get the scaled character metrics */
	get_tfm_chars(params, font, info->tfminfo, 0);

	if(info->encoding)
		tt_encode_font(font, info);
	else {
		mdvi_warning(_("%s: no encoding vector found, expect bad output\n"),
			info->fontname);
		/* this is better than nothing */
		for(i = font->loc; i <= font->hic; i++)
			font->chars[i - font->loc].code = TT_Char_Index(cmap, i);
	}

	info->loaded = 1;
	info->hasmetrics = 1;
	return 0;

tt_error:
	tt_font_remove(info);
	mdvi_free(font->chars);
	font->chars = NULL;
	font->loc = font->hic = 0;
	return -1;
}

static int tt_load_font(DviParams *params, DviFont *font)
{
	int	i;
	FTInfo	*info;

	if(!initialized && init_freetype() < 0)
		return -1;

	if(font->in != NULL) {
		fclose(font->in);
		font->in = NULL;
	}

	info = xalloc(FTInfo);

	memzero(info, sizeof(FTInfo));
	info->fmftype    = DviFontAny; /* any metrics type will do */
	info->fmfname    = lookup_font_metrics(font->fontname, &info->fmftype);
	info->fontname   = font->fontname;
	info->hasmetrics = 0;
	info->loaded     = 0;

	/* these will be obtained from the fontmaps */
	info->mapinfo.psname   = NULL;
	info->mapinfo.encoding = NULL;
	info->mapinfo.fontfile = NULL;
	info->mapinfo.extend   = 0;
	info->mapinfo.slant    = 0;

	/* initialize these */
	font->chars = xnalloc(DviFontChar, 256);
	font->loc = 0;
	font->hic = 255;
	for(i = 0; i < 256; i++) {
		font->chars[i].offset = 1;
		font->chars[i].glyph.data = NULL;
		font->chars[i].shrunk.data = NULL;
		font->chars[i].grey.data = NULL;
	}

	if(info->fmfname == NULL)
		mdvi_warning(_("(tt) %s: no font metric data\n"), font->fontname);

	listh_append(&ttfonts, LIST(info));
	font->private = info;

	return 0;
}

static int tt_get_bitmap(DviParams *params, DviFont *font,
	int code, double xscale, double yscale, DviGlyph *glyph)
{
	TT_Outline	outline;
	TT_Raster_Map	raster;
	TT_BBox		bbox;
	TT_Glyph_Metrics	metrics;
	TT_Matrix	mat;
	FTInfo	*info;
	int	error;
	int	have_outline = 0;
	int	w, h;

	info = (FTInfo *)font->private;
	if(info == NULL)
		return -1;

	error = TT_Load_Glyph(info->instance, info->glyph,
		code, TTLOAD_DEFAULT);
	if(error) goto tt_error;
	error = TT_Get_Glyph_Outline(info->glyph, &outline);
	if(error) goto tt_error;
	have_outline = 1;
	mat.xx = FROUND(xscale * 65536);
	mat.yy = FROUND(yscale * 65536);
	mat.yx = 0;
	mat.xy = 0;
	TT_Transform_Outline(&outline, &mat);
	error = TT_Get_Outline_BBox(&outline, &bbox);
	if(error) goto tt_error;
	bbox.xMin &= -64;
	bbox.yMin &= -64;
	bbox.xMax = (bbox.xMax + 63) & -64;
	bbox.yMax = (bbox.yMax + 63) & -64;
	w = (bbox.xMax - bbox.xMin) / 64;
	h = (bbox.yMax - bbox.yMin) / 64;

	glyph->w = w;
	glyph->h = h;
	glyph->x = -bbox.xMin / 64;
	glyph->y = bbox.yMax / 64;
	if(!w || !h)
		goto tt_error;
	raster.rows = h;
	raster.width = w;
	raster.cols = ROUND(w, 8);
	raster.size = h * raster.cols;
	raster.flow = TT_Flow_Down;
	raster.bitmap = mdvi_calloc(h, raster.cols);

	TT_Translate_Outline(&outline, -bbox.xMin, -bbox.yMin);
	TT_Get_Outline_Bitmap(tt_handle, &outline, &raster);
	glyph->data = bitmap_convert_msb8(raster.bitmap, w, h, ROUND(w, 8));
	TT_Done_Outline(&outline);
	mdvi_free(raster.bitmap);

	return 0;
tt_error:
	if(have_outline)
		TT_Done_Outline(&outline);
	return -1;
}

static int tt_font_get_glyph(DviParams *params, DviFont *font, int code)
{
	FTInfo *info = (FTInfo *)font->private;
	DviFontChar *ch;
	int	error;
	double	xs, ys;
	int	dpi;

	ASSERT(info != NULL);
	if(!info->hasmetrics && tt_really_load_font(params, font, info) < 0)
		return -1;
	ch = FONTCHAR(font, code);
	if(!ch || !glyph_present(ch))
		return -1;
	ch->loaded = 1;
	if(!ch->width || !ch->height)
		goto blank;
	if(ch->code == 0) {
		ch->glyph.data = NULL;
		goto missing;
	}
	/* get the glyph */
	dpi = Max(font->hdpi, font->vdpi);
	error = tt_get_bitmap(params, font, ch->code,
		(double)font->hdpi / dpi,
		(double)font->vdpi / dpi,
		&ch->glyph);
	if(error)
		goto missing;
	ch->x = ch->glyph.x;
	ch->y = ch->glyph.y;

	return 0;

missing:
	ch->glyph.data = MDVI_GLYPH_EMPTY;
	ch->missing = 1;
blank:
	ch->glyph.w = ch->width;
	ch->glyph.h = ch->height;
	ch->glyph.x = ch->x;
	ch->glyph.y = ch->y;
	return 0;
}

static void tt_shrink_glyph(DviContext *dvi, DviFont *font, DviFontChar *ch, DviGlyph *dest)
{
	tt_get_bitmap(&dvi->params, font,
		ch->code,
		(double)font->hdpi / (dvi->params.dpi * dvi->params.hshrink),
		(double)font->vdpi / (dvi->params.vdpi * dvi->params.vshrink),
		dest);
	/* transform the glyph for the current orientation */
	font_transform_glyph(dvi->params.orientation, dest);
}

static void tt_reset_font(DviFont *font)
{
	FTInfo	*info = (FTInfo *)font->private;

	if(info == NULL)
		return;
	info->hasmetrics = 0;
}

static void tt_font_remove(FTInfo *info)
{
	FTInfo	*old;

	if(info->loaded) {
		/* all fonts in the hash table have called TT_Open_Face */
		TT_Done_Instance(info->instance);
		TT_Close_Face(info->face);
	}
	listh_remove(&ttfonts, LIST(info));
	/* release our encodings */
	if(info->encoding)
		mdvi_release_encoding(info->encoding, 1);
	/* and destroy the font */
	if(info->tfminfo)
		free_font_metrics(info->tfminfo);
	if(info->fmfname)
		mdvi_free(info->fmfname);
	mdvi_free(info);
}

static void tt_free_data(DviFont *font)
{
	if(font->private == NULL)
		return;

	tt_font_remove((FTInfo *)font->private);
	if(initialized && ttfonts.count == 0) {
		DEBUG((DBG_TT, "(tt) last font removed -- closing FreeType\n"));
		TT_Done_FreeType(tt_handle);
		initialized = 0;
	}
}

#endif /* WITH_TRUETYPE_FONTS */
