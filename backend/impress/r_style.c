/* imposter (OO.org Impress viewer)
** Copyright (C) 2003-2005 Gurer Ozen
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU General Public License.
*/

#include <config.h>
#include "common.h"
#include "internal.h"

static char *
get_style(ImpRenderCtx *ctx, iks *node, char *style, char *attr)
{
	char *ret;
	iks *x;

	if (!style) return NULL;

	if (iks_root (node) == ctx->content) {
		x = iks_find_with_attrib (iks_find (ctx->content, "office:automatic-styles"),
			"style:style", "style:name", style);
	} else {
		x = iks_find_with_attrib (iks_find (ctx->styles, "office:automatic-styles"),
			"style:style", "style:name", style);
	}
	if (!x) return NULL;

	while (x) {
		ret = iks_find_attrib (iks_find (x, "style:properties"), attr);
		if (ret) return ret;
		ret = iks_find_attrib (iks_find (x, "style:text-properties"), attr);
		if (ret) return ret;
		ret = iks_find_attrib (iks_find (x, "style:paragraph-properties"), attr);
		if (ret) return ret;
		ret = iks_find_attrib (iks_find (x, "style:graphic-properties"), attr);
		if (ret) return ret;
		ret = iks_find_attrib (iks_find (x, "style:drawing-page-properties"), attr);
		if (ret) return ret;

		style = iks_find_attrib (x, "style:parent-style-name");
		if (!style) return NULL;

		x = iks_find_with_attrib (iks_find (ctx->styles, "office:styles"),
			"style:style", "style:name", style);

	}
	return NULL;
}

char *
r_get_style (ImpRenderCtx *ctx, iks *node, char *attr)
{
	char *ret, *s;
	iks *x;

	ret = iks_find_attrib (node, attr);
	if (ret) return ret;

	for (x = node; x; x = iks_parent (x)) {
		s = iks_find_attrib (x, "text:style-name");
		ret = get_style (ctx, node, s, attr);
		if (ret) return ret;
		s = iks_find_attrib (x, "presentation:style-name");
		ret = get_style (ctx, node, s, attr);
		if (ret) return ret;
		s = iks_find_attrib (x, "draw:style-name");
		ret = get_style (ctx, node, s, attr);
		if (ret) return ret;
	}
	return NULL;
}

#if 0
static iks *
get_style_x (ImpRenderCtx *ctx, iks *node, char *style, char *attr)
{
	iks *x;

	if (!style) return NULL;

	if (iks_root (node) == ctx->content) {
		x = iks_find_with_attrib (iks_find (ctx->content, "office:automatic-styles"),
			"text:list-style", "style:name", style);
	} else {
		x = iks_find_with_attrib (iks_find (ctx->styles, "office:automatic-styles"),
			"text:list-style", "style:name", style);
	}
	return x;
}

static iks *
r_get_bullet (ImpRenderCtx *ctx, iks *node, char *attr)
{
	iks *ret;
	char *s;
	iks *x;

	for (x = node; x; x = iks_parent (x)) {
		s = iks_find_attrib (x, "text:style-name");
		ret = get_style_x (ctx, node, s, attr);
		if (ret) return ret;
		s = iks_find_attrib (x, "presentation:style-name");
		ret = get_style_x (ctx, node, s, attr);
		if (ret) return ret;
		s = iks_find_attrib (x, "draw:style-name");
		ret = get_style_x (ctx, node, s, attr);
		if (ret) return ret;
	}
	return NULL;
}
#endif
