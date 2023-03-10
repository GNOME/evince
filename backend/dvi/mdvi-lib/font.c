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

#include "mdvi.h"
#include "private.h"

static ListHead fontlist;

extern char *_mdvi_fallback_font;

extern void vf_free_macros(DviFont *);

#define finfo	search.info
#define TYPENAME(font)	\
	((font)->finfo ? (font)->finfo->name : "none")

int	font_reopen(DviFont *font)
{
	if(font->in)
		fseek(font->in, (long)0, SEEK_SET);
	else if((font->in = fopen(font->filename, "rb")) == NULL) {
		DEBUG((DBG_FILES, "reopen(%s) -> Error\n", font->filename));
		return -1;
	}
	DEBUG((DBG_FILES, "reopen(%s) -> Ok.\n", font->filename));
	return 0;
}

/* used from context: params and device */
static int load_font_file(DviParams *params, DviFont *font)
{
	int	status;

	if(SEARCH_DONE(font->search))
		return -1;
	if(font->in == NULL && font_reopen(font) < 0)
		return -1;
	DEBUG((DBG_FONTS, "%s: loading %s font from `%s'\n",
		font->fontname,
		font->finfo->name, font->filename));
	do {
		status = font->finfo->load(params, font);
	} while(status < 0 && mdvi_font_retry(params, font) == 0);
	if(status < 0)
		return -1;
	if(font->in) {
		fclose(font->in);
		font->in = NULL;
	}
	DEBUG((DBG_FONTS, "reload_font(%s) -> %s\n",
		font->fontname, status < 0 ? "Error" : "Ok"));
	return 0;
}

void	font_drop_one(DviFontRef *ref)
{
	DviFont *font;

	font = ref->ref;
	mdvi_free(ref);
	/* drop all children */
	for(ref = font->subfonts; ref; ref = ref->next) {
		/* just adjust the reference counts */
		ref->ref->links--;
	}
	if(--font->links == 0) {
		/*
		 * this font doesn't have any more references, but
		 * we still keep it around in case a virtual font
		 * requests it.
		 */
		if(font->in) {
			fclose(font->in);
			font->in = NULL;
		}
		if(LIST(font) != fontlist.tail) {
			/* move it to the end of the list */
			listh_remove(&fontlist, LIST(font));
			listh_append(&fontlist, LIST(font));
		}
	}
	DEBUG((DBG_FONTS, "%s: reference dropped, %d more left\n",
		font->fontname, font->links));
}

void	font_drop_chain(DviFontRef *head)
{
	DviFontRef *ptr;

	for(; (ptr = head); ) {
		head = ptr->next;
		font_drop_one(ptr);
	}
}

int	font_free_unused(DviDevice *dev)
{
	DviFont	*font, *next;
	int	count = 0;

	DEBUG((DBG_FONTS, "destroying unused fonts\n"));
	for(font = (DviFont *)fontlist.head; font; font = next) {
		DviFontRef *ref;

		next = font->next;
		if(font->links)
			continue;
		count++;
		DEBUG((DBG_FONTS, "removing unused %s font `%s'\n",
			TYPENAME(font), font->fontname));
		listh_remove(&fontlist, LIST(font));
		if(font->in)
			fclose(font->in);
		/* get rid of subfonts (but can't use `drop_chain' here) */
		for(; (ref = font->subfonts); ) {
			font->subfonts = ref->next;
			mdvi_free(ref);
		}
		/* remove this font */
		font_reset_font_glyphs(dev, font, MDVI_FONTSEL_GLYPH);
		/* let the font destroy its private data */
		if(font->finfo->freedata)
			font->finfo->freedata(font);
		/* destroy characters */
		if(font->chars)
			mdvi_free(font->chars);
		mdvi_free(font->fontname);
		mdvi_free(font->filename);
		mdvi_free(font);
	}
	DEBUG((DBG_FONTS, "%d unused fonts removed\n", count));
	return count;
}

/* used from context: params and device */
DviFontRef *
font_reference(
	DviParams *params, 	/* rendering parameters */
	Int32 id, 		/* external id number */
	const char *name, 	/* font name */
	Int32 sum, 		/* checksum (from DVI of VF) */
	int hdpi, 		/* resolution */
	int vdpi,
	Int32 scale)		/* scaling factor (from DVI or VF) */
{
	DviFont	*font;
	DviFontRef *ref;
	DviFontRef *subfont_ref;

	/* see if there is a font with the same characteristics */
	for(font = (DviFont *)fontlist.head; font; font = font->next) {
		if(strcmp(name, font->fontname) == 0
		   && (!sum || !font->checksum || font->checksum == sum)
		   && font->hdpi == hdpi
		   && font->vdpi == vdpi
		   && font->scale == scale)
		   	break;
	}
	/* try to load the font */
	if(font == NULL) {
		font = mdvi_add_font(name, sum, hdpi, vdpi, scale);
		if(font == NULL)
			return NULL;
		listh_append(&fontlist, LIST(font));
	}
	if(!font->links && !font->chars && load_font_file(params, font) < 0) {
		DEBUG((DBG_FONTS, "font_reference(%s) -> Error\n", name));
		return NULL;
	}
	ref = xalloc(DviFontRef);
	ref->ref = font;

	font->links++;
	for(subfont_ref = font->subfonts; subfont_ref; subfont_ref = subfont_ref->next) {
		/* just adjust the reference counts */
		subfont_ref->ref->links++;
	}

	ref->fontid = id;

	if(LIST(font) != fontlist.head) {
		listh_remove(&fontlist, LIST(font));
		listh_prepend(&fontlist, LIST(font));
	}

	DEBUG((DBG_FONTS, "font_reference(%s) -> %d links\n",
		font->fontname, font->links));
	return ref;
}

void	font_transform_glyph(DviOrientation orient, DviGlyph *g)
{
	BITMAP	*map;
	int	x, y;

	map = (BITMAP *)g->data;
	if(MDVI_GLYPH_ISEMPTY(map))
		map = NULL;

	/* put the glyph in the right orientation */
	switch(orient) {
	case MDVI_ORIENT_TBLR:
		break;
	case MDVI_ORIENT_TBRL:
		g->x = g->w - g->x;
		if(map) bitmap_flip_horizontally(map);
		break;
	case MDVI_ORIENT_BTLR:
		g->y = g->h - g->y;
		if(map) bitmap_flip_vertically(map);
		break;
	case MDVI_ORIENT_BTRL:
		g->x = g->w - g->x;
		g->y = g->h - g->y;
		if(map) bitmap_flip_diagonally(map);
		break;
	case MDVI_ORIENT_RP90:
		if(map) bitmap_rotate_counter_clockwise(map);
		y = g->y;
		x = g->w - g->x;
		g->x = y;
		g->y = x;
		SWAPINT(g->w, g->h);
		break;
	case MDVI_ORIENT_RM90:
		if(map) bitmap_rotate_clockwise(map);
		y = g->h - g->y;
		x = g->x;
		g->x = y;
		g->y = x;
		SWAPINT(g->w, g->h);
		break;
	case MDVI_ORIENT_IRP90:
		if(map) bitmap_flip_rotate_counter_clockwise(map);
		y = g->y;
		x = g->x;
		g->x = y;
		g->y = x;
		SWAPINT(g->w, g->h);
		break;
	case MDVI_ORIENT_IRM90:
		if(map) bitmap_flip_rotate_clockwise(map);
		y = g->h - g->y;
		x = g->w - g->x;
		g->x = y;
		g->y = x;
		SWAPINT(g->w, g->h);
		break;
	}
}

static int load_one_glyph(DviContext *dvi, DviFont *font, int code)
{
	BITMAP *map;
	DviFontChar *ch;
	int	status;

#ifndef NODEBUG
	ch = FONTCHAR(font, code);
	DEBUG((DBG_GLYPHS, "loading glyph code %d in %s (at %u)\n",
		code, font->fontname, ch->offset));
#endif
	if(font->finfo->getglyph == NULL) {
		/* font type does not need to load glyphs (e.g. vf) */
		return 0;
	}

	status = font->finfo->getglyph(&dvi->params, font, code);
	if(status < 0)
		return -1;
	/* get the glyph again (font->chars may have changed) */
	ch = FONTCHAR(font, code);
#ifndef NODEBUG
	map = (BITMAP *)ch->glyph.data;
	if(DEBUGGING(BITMAP_DATA)) {
		DEBUG((DBG_BITMAP_DATA,
			"%s: new %s bitmap for character %d:\n",
			font->fontname, TYPENAME(font), code));
		if(MDVI_GLYPH_ISEMPTY(map))
			DEBUG((DBG_BITMAP_DATA, "blank bitmap\n"));
		else
			bitmap_print(stderr, map);
	}
#endif
	/* check if we have to scale it */
	if(!font->finfo->scalable && font->hdpi != font->vdpi) {
		int	hs, vs, d;

		/* we scale it ourselves */
		d = Max(font->hdpi, font->vdpi);
		hs = d / font->hdpi;
		vs = d / font->vdpi;
		if(ch->width && ch->height && (hs > 1 || vs > 1)) {
			int	h, v;
			DviGlyph glyph;

			DEBUG((DBG_FONTS,
				"%s: scaling glyph %d to resolution %dx%d\n",
				font->fontname, code, font->hdpi, font->vdpi));
			h = dvi->params.hshrink;
			v = dvi->params.vshrink;
			d = dvi->params.density;
			dvi->params.hshrink = hs;
			dvi->params.vshrink = vs;
			dvi->params.density = 50;
			/* shrink it */
			font->finfo->shrink0(dvi, font, ch, &glyph);
			/* restore parameters */
			dvi->params.hshrink = h;
			dvi->params.vshrink = v;
			dvi->params.density = d;
			/* update glyph data */
			if(!MDVI_GLYPH_ISEMPTY(ch->glyph.data))
				bitmap_destroy((BITMAP *)ch->glyph.data);
			ch->glyph.data = glyph.data;
			ch->glyph.x = glyph.x;
			ch->glyph.y = glyph.y;
			ch->glyph.w = glyph.w;
			ch->glyph.h = glyph.h;
		}

	}
	font_transform_glyph(dvi->params.orientation, &ch->glyph);

	return 0;
}

DviFontChar *font_get_glyph(DviContext *dvi, DviFont *font, int code)
{
	DviFontChar *ch;

again:
	/* if we have not loaded the font yet, do so now */
	if(!font->chars && load_font_file(&dvi->params, font) < 0)
		return NULL;

	/* get the unscaled glyph, maybe loading it from disk */
	ch = FONTCHAR(font, code);
	if(!ch)
		return NULL;
	if(!ch->loaded && load_one_glyph(dvi, font, code) == -1) {
		if(font->chars == NULL) {
			/* we need to try another font class */
			goto again;
		}
		return NULL;
	}
	/* yes, we have to do this again */
	ch = FONTCHAR(font, code);

	/* Got the glyph. If we also have the right scaled glyph, do no more */
	if(!ch->width || !ch->height ||
	   font->finfo->getglyph == NULL ||
	   (dvi->params.hshrink == 1 && dvi->params.vshrink == 1))
		return ch;

	/* If the glyph is empty, we just need to shrink the box */
	if(ch->missing || MDVI_GLYPH_ISEMPTY(ch->glyph.data)) {
		if(MDVI_GLYPH_UNSET(ch->shrunk.data))
			mdvi_shrink_box(dvi, font, ch, &ch->shrunk);
		return ch;
	} else if(MDVI_ENABLED(dvi, MDVI_PARAM_ANTIALIASED)) {
		if(ch->grey.data &&
		   !MDVI_GLYPH_ISEMPTY(ch->grey.data) &&
		   ch->fg == dvi->curr_fg &&
		   ch->bg == dvi->curr_bg)
		   	return ch;
		if(ch->grey.data &&
		   !MDVI_GLYPH_ISEMPTY(ch->grey.data)) {
			if(dvi->device.free_image)
				dvi->device.free_image(ch->grey.data);
			ch->grey.data = NULL;
		}
		font->finfo->shrink1(dvi, font, ch, &ch->grey);
	} else if(!ch->shrunk.data)
		font->finfo->shrink0(dvi, font, ch, &ch->shrunk);

	return ch;
}

void	font_reset_one_glyph(DviDevice *dev, DviFontChar *ch, int what)
{
	if(!glyph_present(ch))
		return;
	if(what & MDVI_FONTSEL_BITMAP) {
		if(MDVI_GLYPH_NONEMPTY(ch->shrunk.data))
			bitmap_destroy((BITMAP *)ch->shrunk.data);
		ch->shrunk.data = NULL;
	}
	if(what & MDVI_FONTSEL_GREY) {
		if(MDVI_GLYPH_NONEMPTY(ch->grey.data)) {
			if(dev->free_image)
				dev->free_image(ch->grey.data);
		}
		ch->grey.data = NULL;
	}
	if(what & MDVI_FONTSEL_GLYPH) {
		if(MDVI_GLYPH_NONEMPTY(ch->glyph.data))
			bitmap_destroy((BITMAP *)ch->glyph.data);
		ch->glyph.data = NULL;
		ch->loaded = 0;
	}
}

void	font_reset_font_glyphs(DviDevice *dev, DviFont *font, int what)
{
	int	i;
	DviFontChar *ch;

	if(what & MDVI_FONTSEL_GLYPH)
		what |= MDVI_FONTSEL_BITMAP|MDVI_FONTSEL_GREY;
	if(font->subfonts) {
		DviFontRef *ref;

		for(ref = font->subfonts; ref; ref = ref->next)
			font_reset_font_glyphs(dev, ref->ref, what);
	}
	if(font->in) {
		DEBUG((DBG_FILES, "close(%s)\n", font->filename));
		fclose(font->in);
		font->in = NULL;
	}
	if(font->finfo->getglyph == NULL)
		return;
	DEBUG((DBG_FONTS, "resetting glyphs in font `%s'\n", font->fontname));
	for(ch = font->chars, i = font->loc; i <= font->hic; ch++, i++) {
		if(glyph_present(ch))
			font_reset_one_glyph(dev, ch, what);
	}
	if((what & MDVI_FONTSEL_GLYPH) && font->finfo->reset)
		font->finfo->reset(font);
}

void	font_reset_chain_glyphs(DviDevice *dev, DviFontRef *head, int what)
{
	DviFontRef *ref;

	for(ref = head; ref; ref = ref->next)
		font_reset_font_glyphs(dev, ref->ref, what);
}

static int compare_refs(const void *p1, const void *p2)
{
	return ((*(DviFontRef **)p1)->fontid - (*(DviFontRef **)p2)->fontid);
}

void	font_finish_definitions(DviContext *dvi)
{
	int	count;
	DviFontRef **map, *ref;

	/* first get rid of unused fonts */
	font_free_unused(&dvi->device);

	if(dvi->fonts == NULL) {
		mdvi_warning(_("%s: no fonts defined\n"), dvi->filename);
		return;
	}
	map = xnalloc(DviFontRef *, dvi->nfonts);
	for(count = 0, ref = dvi->fonts; ref; ref = ref->next)
		map[count++] = ref;
	/* sort the array by font id */
	qsort(map, dvi->nfonts, sizeof(DviFontRef *), compare_refs);
	dvi->fontmap = map;
}

DviFontRef *font_find_flat(DviContext *dvi, Int32 id)
{
	DviFontRef *ref;

	for(ref = dvi->fonts; ref; ref = ref->next)
		if(ref->fontid == id)
			break;
	return ref;
}

DviFontRef *font_find_mapped(DviContext *dvi, Int32 id)
{
	int	lo, hi, n;
	DviFontRef **map;

	/* do a binary search */
	lo = 0; hi = dvi->nfonts;
	map = dvi->fontmap;
	while(lo < hi) {
		int	sign;

		n = (hi + lo) >> 1;
		sign = (map[n]->fontid - id);
		if(sign == 0)
			break;
		else if(sign < 0)
			lo = n;
		else
			hi = n;
	}
	if(lo >= hi)
		return NULL;
	return map[n];
}

