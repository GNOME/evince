/* imposter (OO.org Impress viewer)
** Copyright (C) 2003-2005 Gurer Ozen
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU General Public License.
*/

#include "zip.h"

#ifndef INTERNAL_H
#define INTERNAL_H

struct ImpDoc_struct {
	ikstack *stack;
	zip *zfile;
	iks *content;
	iks *styles;
	iks *meta;
	ImpPage *pages;
	ImpPage *last_page;
	int nr_pages;
	void (*get_geometry)(ImpRenderCtx *ctx);
	void (*render_page)(ImpRenderCtx *ctx, void *drw_data);
};

struct ImpPage_struct {
	struct ImpPage_struct *next;
	struct ImpPage_struct *prev;
	ImpDoc *doc;
	iks *page;
	const char *name;
	int nr;
};

struct ImpRenderCtx_struct {
	const ImpDrawer *drw;
	ImpPage *page;
	iks *content;
	iks *styles;
	iks *last_element;
	int step;
	int pix_w, pix_h;
	double cm_w, cm_h;
	double fact_x, fact_y;
};

char *r_get_style (ImpRenderCtx *ctx, iks *node, char *attr);
int r_get_color(ImpRenderCtx *ctx, iks *node, char *name, ImpColor *ic);
void r_parse_color(const char *color, ImpColor *ic);
int r_get_x (ImpRenderCtx *ctx, iks *node, char *name);
int r_get_y (ImpRenderCtx *ctx, iks *node, char *name);
int r_get_angle (iks *node, char *name, int def);

enum {
	IMP_LE_NONE = 0,
	IMP_LE_ARROW,
	IMP_LE_SQUARE,
	IMP_LE_DIMENSION,
	IMP_LE_DOUBLE_ARROW,
	IMP_LE_SMALL_ARROW,
	IMP_LE_ROUND_ARROW,
	IMP_LE_SYM_ARROW,
	IMP_LE_LINE_ARROW,
	IMP_LE_ROUND_LARGE_ARROW,
	IMP_LE_CIRCLE,
	IMP_LE_SQUARE_45,
	IMP_LE_CONCAVE_ARROW
};

void _imp_draw_rect(ImpRenderCtx *ctx, void *drw_data, int fill, int x, int y, int w, int h, int round);
void _imp_draw_line_end(ImpRenderCtx *ctx, void *drw_data, int type, int size, int x, int y, int x2, int y2);
void _imp_draw_image(ImpRenderCtx *ctx, void *drw_data, const char *name, int x, int y, int w, int h);
void _imp_tile_image(ImpRenderCtx *ctx, void *drw_data, const char *name, int x, int y, int w, int h);

int _imp_fill_back(ImpRenderCtx *ctx, void *drw_data, iks *node);
void r_text(ImpRenderCtx *ctx, void *drw_data, iks *node);
void r_polygon(ImpRenderCtx *ctx, void *drw_data, iks *node);
void r_circle(ImpRenderCtx *ctx, void *drw_data, iks *node);
void r_polyline(ImpRenderCtx *ctx, void *drw_data, iks *node);
void r_draw_gradient (ImpRenderCtx *ctx, void *drw_data, iks *node);

int _imp_oo13_load(ImpDoc *doc);
int _imp_oasis_load(ImpDoc *doc);


#endif	/* INTERNAL_H */
