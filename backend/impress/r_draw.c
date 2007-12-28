/* imposter (OO.org Impress viewer)
** Copyright (C) 2003-2005 Gurer Ozen
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU General Public License.
*/

#include <config.h>
#include "common.h"
#include "internal.h"
#include <math.h>

void
_imp_draw_rect(ImpRenderCtx *ctx, void *drw_data, int fill, int x, int y, int w, int h, int round)
{
	int a;

	if (0 == round) {
		ctx->drw->draw_rect(drw_data, fill, x, y, w, h);
		return;
	}

	ctx->drw->draw_arc(drw_data, fill,
		x, y, round, round, 90, 90);
	ctx->drw->draw_arc(drw_data, fill,
		x + w - round, y, round, round, 0, 90);
	ctx->drw->draw_arc(drw_data, fill,
		x + w - round, y + h - round, round, round, 270, 90);
	ctx->drw->draw_arc(drw_data, fill,
		x, y + h - round, round, round, 180, 90);

	a = round / 2;
	if (fill) {
		ctx->drw->draw_rect(drw_data, 1, x + a, y, w - a - a, h);
		ctx->drw->draw_rect(drw_data, 1, x, y + a, w, h - a - a);
		return;
	}
	ctx->drw->draw_line(drw_data, x + a, y, x + w - a, y);
	ctx->drw->draw_line(drw_data, x + a, y + h, x + w - a, y + h);
	ctx->drw->draw_line(drw_data, x, y + a, x, y + h - a);
	ctx->drw->draw_line(drw_data, x + w, y + a, x + w, y + h - a);
}

void
_imp_draw_line_end(ImpRenderCtx *ctx, void *drw_data, int type, int size, int x, int y, int x2, int y2)
{
	ImpPoint pts[4];
	double ia, a;

	// FIXME: different types and sizes

	pts[0].x = x2;
	pts[0].y = y2;

	ia = 20 * 3.14 * 2 / 360;

	if (x2-x == 0) {
		if (y < y2) a = 3.14 + (3.14 / 2); else a = (3.14 / 2);
	} else if (y2-y == 0) {
		if (x < x2) a = 3.14; else a = 0;
	} else
		a = atan ((y2-y) / (x2-x)) - 3.14;

	pts[1].x = x2 + 0.3 * ctx->fact_x * cos (a - ia);
	pts[1].y = y2 + 0.3 * ctx->fact_y * sin (a - ia);

	pts[2].x = x2 + 0.3 * ctx->fact_x * cos (a + ia);
	pts[2].y = y2 + 0.3 * ctx->fact_y * sin (a + ia);

	ctx->drw->draw_polygon(drw_data, 1, pts, 3);
}

void
_imp_draw_image(ImpRenderCtx *ctx, void *drw_data, const char *name, int x, int y, int w, int h)
{
	void *img1, *img2;
	char *pix;
	size_t len;

	len = zip_get_size(ctx->page->doc->zfile, name);
	pix = malloc(len);
	if (!pix) return;
	zip_load(ctx->page->doc->zfile, name, pix);

	img1 = ctx->drw->open_image(drw_data, pix, len);
	free(pix);
	if (!img1) return;
	img2 = ctx->drw->scale_image(drw_data, img1, w, h);
	if (img2) {
		ctx->drw->draw_image(drw_data, img2, x, y, w, h);
		ctx->drw->close_image(drw_data, img2);
	}
	ctx->drw->close_image(drw_data, img1);
}

void
_imp_tile_image(ImpRenderCtx *ctx, void *drw_data, const char *name, int x, int y, int w, int h)
{
	void *img1;
	char *pix;
	size_t len;
	int gx, gy, gw, gh;

	len = zip_get_size(ctx->page->doc->zfile, name);
	pix = malloc(len);
	if (!pix) return;
	zip_load(ctx->page->doc->zfile, name, pix);

	img1 = ctx->drw->open_image(drw_data, pix, len);
	free(pix);
	if (!img1) return;

	ctx->drw->get_image_size(drw_data, img1, &gw, &gh);
	for (gx = x; gx < w; gx += gw) {
		for (gy = y; gy < h; gy += gh) {
			ctx->drw->draw_image(drw_data, img1, gx, gy, gw, gh);
		}
	}

	ctx->drw->close_image(drw_data, img1);
}
