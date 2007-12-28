/* imposter (OO.org Impress viewer)
** Copyright (C) 2003-2005 Gurer Ozen
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU General Public License.
*/

#include <config.h>
#include "common.h"
#include "internal.h"

static void
render_object(ImpRenderCtx *ctx, void *drw_data, iks *node)
{
	char *tag, *t;
	ImpColor fg;

	tag = iks_name(node);
	if (strcmp(tag, "draw:g") == 0) {
		iks *x;
		for (x = iks_first_tag(node); x; x = iks_next_tag(x)) {
			render_object(ctx, drw_data, x);
		}
	} else if (strcmp(tag, "draw:frame") == 0) {
		iks *x;
		for (x = iks_first_tag(node); x; x = iks_next_tag(x)) {
			render_object(ctx, drw_data, x);
		}
	} else if (strcmp(tag, "draw:line") == 0) {
		r_get_color(ctx, node, "svg:stroke-color", &fg);
		ctx->drw->set_fg_color(drw_data, &fg);
		ctx->drw->draw_line(drw_data,
			r_get_x(ctx, node, "svg:x1"), r_get_y(ctx, node, "svg:y1"),
			r_get_x(ctx, node, "svg:x2"), r_get_y(ctx, node, "svg:y2")
		);
	} else if (strcmp(tag, "draw:rect") == 0) {
		int x, y, w, h, r = 0;
		char *t;
		x = r_get_x(ctx, node, "svg:x");
		y = r_get_y(ctx, node, "svg:y");
		w = r_get_x(ctx, node, "svg:width");
		h = r_get_y(ctx, node, "svg:height");
		t = r_get_style(ctx, node, "draw:corner-radius");
		if (t) r = atof(t) * ctx->fact_x;
		if (r_get_style(ctx, node, "draw:fill")) {
			r_get_color(ctx, node, "draw:fill-color", &fg);
			ctx->drw->set_fg_color(drw_data, &fg);
			_imp_draw_rect(ctx, drw_data, 1, x, y, w, h, r);
		}
		r_get_color(ctx, node, "svg:stroke-color", &fg);
		ctx->drw->set_fg_color(drw_data, &fg);
		_imp_draw_rect(ctx, drw_data, 0, x, y, w, h, r);
		r_text(ctx, drw_data, node);
	} else if (strcmp(tag, "draw:ellipse") == 0 || strcmp(tag, "draw:circle") == 0) {
		int sa, ea, fill = 0;
		r_get_color(ctx, node, "svg:stroke-color", &fg);
		sa = r_get_angle(node, "draw:start-angle", 0);
		ea = r_get_angle(node, "draw:end-angle", 360);
		if (ea > sa) ea = ea - sa; else ea = 360 + ea - sa;
		t = r_get_style(ctx, node, "draw:fill");
		if (t) fill = 1;
		ctx->drw->set_fg_color(drw_data, &fg);
		ctx->drw->draw_arc(drw_data,
			fill,
			r_get_x(ctx, node, "svg:x"), r_get_y(ctx, node, "svg:y"),
			r_get_x(ctx, node, "svg:width"), r_get_y(ctx, node, "svg:height"),
			sa, ea
		);
	} else if (strcmp(tag, "draw:polygon") == 0) {
		// FIXME:
		r_polygon(ctx, drw_data, node);
	} else if (strcmp(tag, "draw:text-box") == 0) {
		// FIXME:
		r_text(ctx, drw_data, node);
	} else if (strcmp(tag, "draw:image") == 0) {
		char *name;

		name = iks_find_attrib(node, "xlink:href");
		if (!name) return;
		if (name[0] == '#') ++name;

		_imp_draw_image(ctx, drw_data,
			name,
			r_get_x(ctx, node, "svg:x"),
			r_get_y(ctx, node, "svg:y"),
			r_get_x(ctx, node, "svg:width"),
			r_get_y(ctx, node, "svg:height")
		);
	} else {
		printf("Unknown element: %s\n", tag);
	}
}

static void
render_page(ImpRenderCtx *ctx, void *drw_data)
{
	iks *x;
	char *element;
	int i;

	i = _imp_fill_back(ctx, drw_data, ctx->page->page);
	element = iks_find_attrib(ctx->page->page, "draw:master-page-name");
	if (element) {
		x = iks_find_with_attrib(
			iks_find(ctx->page->doc->styles, "office:master-styles"),
			"style:master-page", "style:name", element
		);
		if (x) {
			if (i == 0) _imp_fill_back(ctx, drw_data, x);
			for (x = iks_first_tag(x); x; x = iks_next_tag(x)) {
				if (iks_find_attrib(x, "presentation:class"))
					continue;
				render_object(ctx, drw_data, x);
			}
		}
	}
	for (x = iks_first_tag(ctx->page->page); x; x = iks_next_tag(x)) {
		render_object(ctx, drw_data, x);
	}
}

static void
get_geometry(ImpRenderCtx *ctx)
{
	char *tmp;
	iks *x, *y;

	tmp = iks_find_attrib(ctx->page->page, "draw:master-page-name");
	x = iks_find(ctx->page->doc->styles, "office:master-styles");
	y = iks_find_with_attrib(x, "style:master-page", "style:name", tmp);
	x = iks_find(ctx->page->doc->styles, "office:automatic-styles");
	y = iks_find_with_attrib(x, "style:page-layout", "style:name",
		iks_find_attrib(y, "style:page-layout-name"));
	ctx->cm_w = atof(iks_find_attrib(iks_find(y, "style:page-layout-properties"), "fo:page-width"));
	ctx->cm_h = atof(iks_find_attrib(iks_find(y, "style:page-layout-properties"), "fo:page-height"));
}

int
_imp_oasis_load(ImpDoc *doc)
{
	ImpPage *page;
	iks *x, *pres;
	int i;

	pres = iks_find(iks_find(doc->content, "office:body"), "office:presentation");
	if (!pres) return IMP_NOTIMP;

	x = iks_find(pres, "draw:page");
	if (!x) return IMP_NOTIMP;
	i = 0;
	for (; x; x = iks_next_tag(x)) {
		if (strcmp(iks_name(x), "draw:page") == 0) {
			page = iks_stack_alloc(doc->stack, sizeof(ImpPage));
			if (!page) return IMP_NOMEM;
			memset(page, 0, sizeof(ImpPage));
			page->page = x;
			page->nr = ++i;
			page->name = iks_find_attrib(x, "draw:name");
			page->doc = doc;
			if (!doc->pages) doc->pages = page;
			page->prev = doc->last_page;
			if (doc->last_page) doc->last_page->next = page;
			doc->last_page = page;
		}
	}
	doc->nr_pages = i;
	doc->get_geometry = get_geometry;
	doc->render_page = render_page;

	return 0;
}
