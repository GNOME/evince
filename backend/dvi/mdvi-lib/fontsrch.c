/* fontsearch.c -- implements the font lookup mechanism in MDVI */
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
 * How this works:
 *   Fonts are divided into MAX_CLASS priority classes. The first
 * MAX_CLASS-1 ones correspond to `real' fonts (pk, gf, vf, type1, truetype,
 * etc). The last one corresponds to `metric' fonts that are used as a last
 * resort (tfm, afm, ofm, ...). When a font is looked up, it is tried in a
 * `high' priority class (0 being the highest priority). The priority is
 * lowered until it reaches MAX_CLASS-1. Then the whole thing is repeated
 * for the fallback font. When the search reaches MAX_CLASS-1, we lookup the
 * original font, and then the fallback font. The search can be done
 * incrementally, with several calls to mdvi_lookup_font(). If this function
 * is called again to continue a search, the function assumes the previous
 * font it returned was not valid, and it goes on to the next step.
 *
 * Reason for this:
 *   Some font types are quite expensive to load (e.g. Type1),  so loading
 * them is deferred until the last possible moment. This means that a font that
 * was supposed to exist may have to be discarded. Until now, MDVI had no ability to
 * "resume" a search, so in this case it would have produced an error, regardless
 * of whether the offending font existed in other formats.
 *   Also, given the large number of font types supported by MDVI, some mechanism
 * was necessary to bring some order into the chaos.
 *
 * This mechanism fixes these two problems. For the first one, a search can
 * be "resumed" and all the font formats tried for the missing font, and
 * again for the fallback font (see above). As for the second, the
 * hierarchical division in classes gives a lot of flexibility in how the
 * fonts are configured.
 */

#include <config.h>
#include "mdvi.h"

#define HAVE_PROTOTYPES 1
#include <kpathsea/tex-file.h>
#include <kpathsea/tex-glyph.h>

struct _DviFontClass {
	DviFontClass *next;
	DviFontClass *prev;
	DviFontInfo  info;
	int	links;
	int	id;
};

char *_mdvi_fallback_font = MDVI_FALLBACK_FONT;

/* this leaves classes 0 and 1 for `real' fonts */
#define MAX_CLASS	3
static ListHead font_classes[MAX_CLASS];
static int initialized = 0;

static void init_font_classes(void)
{
	int	i;

	for(i = 0; i < MAX_CLASS; i++)
		listh_init(&font_classes[i]);
	initialized = 1;
}

int	mdvi_get_font_classes(void)
{
	return (MAX_CLASS - 2);
}

char	**mdvi_list_font_class(int klass)
{
	char	**list;
	int	i, n;
	DviFontClass *fc;

	if(klass == -1)
		klass = MAX_CLASS-1;
	if(klass < 0 || klass >= MAX_CLASS)
		return NULL;
	n = font_classes[klass].count;
	list = xnalloc(char *, n + 1);
	fc = (DviFontClass *)font_classes[klass].head;
	for(i = 0; i < n; fc = fc->next, i++) {
		list[i] = mdvi_strdup(fc->info.name);
	}
	list[i] = NULL;
	return list;
}

int	mdvi_register_font_type(DviFontInfo *info, int klass)
{
	DviFontClass *fc;

	if(klass == -1)
		klass = MAX_CLASS-1;
	if(klass < 0 || klass >= MAX_CLASS)
		return -1;
	if(!initialized)
		init_font_classes();
	fc = xalloc(struct _DviFontClass);
	fc->links = 0;
	fc->id = klass;
	fc->info.name = mdvi_strdup(info->name);
	fc->info.scalable = info->scalable;
	fc->info.load = info->load;
	fc->info.getglyph = info->getglyph;
	fc->info.shrink0 = info->shrink0;
	fc->info.shrink1 = info->shrink1;
	fc->info.freedata = info->freedata;
	fc->info.reset = info->reset;
	fc->info.lookup = info->lookup;
	fc->info.kpse_type = info->kpse_type;
	listh_append(&font_classes[klass], LIST(fc));
	return 0;
}

int	mdvi_unregister_font_type(const char *name, int klass)
{
	DviFontClass *fc;
	int	k;

	if(klass == -1)
		klass = MAX_CLASS - 1;

	if(klass >= 0 && klass < MAX_CLASS) {
		k = klass;
		LIST_FOREACH(fc, DviFontClass, &font_classes[k]) {
			if(STREQ(fc->info.name, name))
				break;
		}
	} else if(klass < 0) {
		for(k = 0; k < MAX_CLASS; k++) {
			LIST_FOREACH(fc, DviFontClass, &font_classes[k]) {
				if(STREQ(fc->info.name, name))
					break;
			}
			if(fc) break;
		}
	} else
		return -1;

	if(fc == NULL || fc->links)
		return -1;
	/* remove it */
	listh_remove(&font_classes[k], LIST(fc));

	/* and destroy it */
	mdvi_free(fc->info.name);
	mdvi_free(fc);
	return 0;
}

static char *lookup_font(DviFontClass *ptr, const char *name, Ushort *h, Ushort *v)
{
	char	*filename;

	/*
	 * If the font type registered a function to do the lookup, use that.
	 * Otherwise we use kpathsea.
	 */
	if(ptr->info.lookup)
		filename = ptr->info.lookup(name, h, v);
	else if(ptr->info.kpse_type <= kpse_any_glyph_format) {
		kpse_glyph_file_type type;

		filename = kpse_find_glyph(name, Max(*h, *v),
			ptr->info.kpse_type, &type);
		/* if kpathsea returned a fallback font, reject it */
		if(filename && type.source == kpse_glyph_source_fallback) {
			mdvi_free(filename);
			filename = NULL;
		} else if(filename)
			*h = *v = type.dpi;
	} else
		filename = kpse_find_file(name, ptr->info.kpse_type, 1);
	return filename;
}

/*
 * Class MAX_CLASS-1 is special: it consists of `metric' fonts that should
 * be tried as a last resort
 */
char	*mdvi_lookup_font(DviFontSearch *search)
{
	int kid;
	int k;
	DviFontClass *ptr;
	DviFontClass *last;
	char	*filename = NULL;
	const char *name;
	Ushort	hdpi, vdpi;

	if(search->id < 0)
		return NULL;

	if(search->curr == NULL) {
		/* this is the initial search */
		name = search->wanted_name;
		hdpi = search->hdpi;
		vdpi = search->vdpi;
		kid = 0;
		last = NULL;
	} else {
		name = search->actual_name;
		hdpi = search->actual_hdpi;
		vdpi = search->actual_vdpi;
		kid = search->id;
		last = search->curr;
	}

	ptr = NULL;
again:
	/* try all classes except MAX_CLASS-1 */
	for(k = kid; !filename && k < MAX_CLASS-1; k++) {
		if(last == NULL)
			ptr = (DviFontClass *)font_classes[k].head;
		else
			ptr = last->next;
		while(ptr) {
			DEBUG((DBG_FONTS, "%d: trying `%s' at (%d,%d)dpi as `%s'\n",
				k, name, hdpi, vdpi, ptr->info.name));
			/* lookup the font in this class */
			filename = lookup_font(ptr, name, &hdpi, &vdpi);
			if(filename)
				break;
			ptr = ptr->next;
		}
		last = NULL;
	}
	if(filename != NULL) {
		search->id = k-1;
		search->curr = ptr;
		search->actual_name = name;
		search->actual_hdpi = hdpi;
		search->actual_vdpi = vdpi;
		search->info = &ptr->info;
		ptr->links++;
		return filename;
	}

	if(kid < MAX_CLASS - 1 && !STREQ(name, _mdvi_fallback_font)) {
		mdvi_warning("font `%s' at %dx%d not found, trying `%s' instead\n",
			     name, hdpi, vdpi, _mdvi_fallback_font);
		name = _mdvi_fallback_font;
		kid = 0;
		goto again;
	}

	/* we tried the fallback font, and all the `real' classes. Let's
	 * try the `metric' class now */
	name = search->wanted_name;
	hdpi = search->hdpi;
	vdpi = search->vdpi;
	if(kid == MAX_CLASS-1) {
		/* we were looking into this class from the beginning */
		if(last == NULL) {
			/* no more fonts to try */
			return NULL;
		}
		ptr = last->next;
	} else {
		mdvi_warning("font `%s' not found, trying metric files instead\n",
			     name);
		ptr = (DviFontClass *)font_classes[MAX_CLASS-1].head;
	}

metrics:
	while(ptr) {
		DEBUG((DBG_FONTS, "metric: trying `%s' at (%d,%d)dpi as `%s'\n",
			name, hdpi, vdpi, ptr->info.name));
		filename = lookup_font(ptr, name, &hdpi, &vdpi);
		if(filename)
			break;
		ptr = ptr->next;
	}
	if(filename != NULL) {
		if(STREQ(name, _mdvi_fallback_font))
			search->id = MAX_CLASS;
		else
			search->id = MAX_CLASS - 1;
		search->curr = ptr;
		search->actual_name = name;
		search->actual_hdpi = hdpi;
		search->actual_vdpi = vdpi;
		search->info = &ptr->info;
		ptr->links++;
		return filename;
	}
	if(!STREQ(name, _mdvi_fallback_font)) {
		mdvi_warning("metric file for `%s' not found, trying `%s' instead\n",
			     name, _mdvi_fallback_font);
		name = _mdvi_fallback_font;
		ptr = (DviFontClass *)font_classes[MAX_CLASS-1].head;
		goto metrics;
	}

	search->id = -1;
	search->actual_name = NULL;

	/* tough luck, nothing found */
	return NULL;
}

/* called by `font_reference' to do the initial lookup */
DviFont	*mdvi_add_font(const char *name, Int32 sum,
	int hdpi, int vdpi, Int32 scale)
{
	DviFont	*font;

	font = xalloc(DviFont);
	font->fontname = mdvi_strdup(name);
	SEARCH_INIT(font->search, font->fontname, hdpi, vdpi);
	font->filename = mdvi_lookup_font(&font->search);
	if(font->filename == NULL) {
		/* this answer is final */
		mdvi_free(font->fontname);
		mdvi_free(font);
		return NULL;
	}
	font->hdpi = font->search.actual_hdpi;
	font->vdpi = font->search.actual_vdpi;
	font->scale = scale;
	font->design = 0;
	font->checksum = sum;
	font->type = 0;
	font->links = 0;
	font->loc = 0;
	font->hic = 0;
	font->in = NULL;
	font->chars = NULL;
	font->subfonts = NULL;

	return font;
}

int	mdvi_font_retry(DviParams *params, DviFont *font)
{
	/* try the search again */
	char	*filename;

	ASSERT(font->search.curr != NULL);
	/* we won't be using this class anymore */
	font->search.curr->links--;

	filename = mdvi_lookup_font(&font->search);
	if(filename == NULL)
		return -1;
	mdvi_free(font->filename);
	font->filename = filename;
	/* copy the new information */
	font->hdpi = font->search.actual_hdpi;
	font->vdpi = font->search.actual_vdpi;

	return 0;
}
