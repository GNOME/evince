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
r_parse_color(const char *color, ImpColor *ic)
{
	unsigned int cval;

	if (1 != sscanf(color, "#%X", &cval)) return;

	ic->red = (cval & 0xFF0000) >> 8;
	ic->green = cval & 0x00FF00;
	ic->blue = (cval & 0xFF) << 8;
}

int
r_get_color(ImpRenderCtx *ctx, iks *node, char *name, ImpColor *ic)
{
	char *color;

	color = r_get_style(ctx, node, name);
	if (!color) return 0;
	r_parse_color(color, ic);

	return 1;
}

static void
fg_color(ImpRenderCtx *ctx, void *drw_data, iks *node, char *name)
{
	ImpColor ic;

	if (r_get_color(ctx, node, name, &ic)) {
		ctx->drw->set_fg_color(drw_data, &ic);
	}
}

int
r_get_x (ImpRenderCtx *ctx, iks *node, char *name)
{
	char *val;

	val = iks_find_attrib (node, name);
	if (!val) return 0;
	return atof (val) * ctx->fact_x;
}

int
r_get_y (ImpRenderCtx *ctx, iks *node, char *name)
{
	char *val;

	val = iks_find_attrib (node, name);
	if (!val) return 0;
	return atof (val) * ctx->fact_y;
}

int
r_get_angle (iks *node, char *name, int def)
{
	char *tmp;

	tmp = iks_find_attrib (node, name);
	if (!tmp) return def;
	return atof (tmp);
}

static int x, y, w, h;
static int px, py, pw, ph;

static void
r_get_viewbox (iks *node)
{
	char *tmp;

	tmp = iks_find_attrib (node, "svg:viewBox");
	if (!tmp) return;
	sscanf (tmp, "%d %d %d %d", &px, &py, &pw, &ph);
}

void
r_polygon(ImpRenderCtx *ctx, void *drw_data, iks *node)
{
	char *data;
	ImpPoint *points;
	int i, cnt, j;
	int num;
	int fill = 1;

	data = r_get_style (ctx, node, "draw:fill");
	if (!data || strcmp (data, "solid") != 0) fill = 0;

	x = r_get_x (ctx, node, "svg:x");
	y = r_get_y (ctx, node, "svg:y");
	w = r_get_x (ctx, node, "svg:width");
	h = r_get_y (ctx, node, "svg:height");
	r_get_viewbox (node);

	data = iks_find_attrib (node, "draw:points");
	points = malloc (sizeof (ImpPoint) * strlen (data) / 4);

	cnt = 0;
	j = 0;
	num = -1;
	for (i = 0; data[i]; i++) {
		if (data[i] >= '0' && data[i] <= '9') {
			if (num == -1) num = i;
		} else {
			if (num != -1) {
				if (j == 0) {
					points[cnt].x = atoi (data + num);
					j = 1;
				} else {
					points[cnt++].y = atoi (data + num);
					j = 0;
				}
				num = -1;
			}
		}
	}
	if (num != -1) {
		if (j == 0) {
			points[cnt].x = atoi (data + num);
		} else {
			points[cnt++].y = atoi (data + num);
		}
	}
	for (i = 0; i < cnt; i++) {
		points[i].x = x + points[i].x * w / pw;
		points[i].y = y + points[i].y * h / ph;
	}

	if (fill) {
		fg_color(ctx, drw_data, node, "draw:fill-color");
		ctx->drw->draw_polygon(drw_data, 1, points, cnt);
	}
	fg_color(ctx, drw_data, node, "svg:stroke-color");
	ctx->drw->draw_polygon(drw_data, 0, points, cnt);

	free (points);
}

void
r_polyline(ImpRenderCtx *ctx, void *drw_data, iks *node)
{
	char *data;
	ImpPoint *points;
	int i, cnt, j;
	int num;
	int pen_x, pen_y;

	x = r_get_x (ctx, node, "svg:x");
	y = r_get_y (ctx, node, "svg:y");
	w = r_get_x (ctx, node, "svg:width");
	h = r_get_y (ctx, node, "svg:height");
	r_get_viewbox (node);

	data = iks_find_attrib (node, "draw:points");
	points = malloc (sizeof (ImpPoint) * strlen (data) / 4);

	cnt = 0;
	j = 0;
	num = -1;
	for (i = 0; data[i]; i++) {
		if (data[i] >= '0' && data[i] <= '9') {
			if (num == -1) num = i;
		} else {
			if (num != -1) {
				if (j == 0) {
					points[cnt].x = atoi (data + num);
					j = 1;
				} else {
					points[cnt++].y = atoi (data + num);
					j = 0;
				}
				num = -1;
			}
		}
	}
	if (num != -1) {
		if (j == 0) {
			points[cnt].x = atoi (data + num);
		} else {
			points[cnt++].y = atoi (data + num);
		}
	}

	pen_x = x + points[0].x * w /pw;
	pen_y = y + points[0].y * h / ph;
	fg_color(ctx, drw_data, node, "svg:stroke-color");
	for (i = 1; i < cnt; i++) {
		int tx, ty;
		tx = x + points[i].x * w / pw;
		ty = y + points[i].y * h / ph;
		ctx->drw->draw_line(drw_data, pen_x, pen_y, tx, ty);
		pen_x = tx;
		pen_y = ty;
	}
	free (points);
}
