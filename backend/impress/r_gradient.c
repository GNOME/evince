/* imposter (OO.org Impress viewer)
** Copyright (C) 2003-2005 Gurer Ozen
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU General Public License.
*/

#include <config.h>
#include "common.h"
#include "internal.h"
#include <math.h>

#define GRAD_LINEAR 0
#define GRAD_AXIAL 1
#define GRAD_SQUARE 2
#define GRAD_RECTANGULAR 3
#define GRAD_RADIAL 4
#define GRAD_ELLIPTICAL 5

typedef struct Gradient_s {
	int type;
	ImpColor start;
	int start_intensity;
	ImpColor end;
	int end_intensity;
	int angle;
	int border;
	int steps;
	int offset_x;
	int offset_y;
} Gradient;

typedef struct Rectangle_s {
	int Left;
	int Top;
	int Right;
	int Bottom;
} Rectangle;

static void
poly_rotate (ImpPoint *poly, int n, int cx, int cy, double fAngle)
{
	int i;
	long nX, nY;

	for (i = 0; i < n; i++) {
		nX = poly->x - cx;
		nY = poly->y - cy;
		poly->x = (cos(fAngle) * nX + sin(fAngle) * nY) + cx;
		poly->y = - (sin(fAngle)* nX - cos(fAngle) * nY) + cy;
		poly++;
	}
}

static void
r_draw_gradient_simple (ImpRenderCtx *ctx, void *drw_data, Gradient *grad)
{
	Rectangle rRect = { 0, 0, ctx->pix_w - 1, ctx->pix_h - 1 };
	Rectangle aRect, aFullRect;
	ImpPoint poly[4], tempoly[2];
	ImpColor gcol;
	double fW, fH, fDX, fDY, fAngle;
	double fScanLine, fScanInc;
	long redSteps, greenSteps, blueSteps;
	long nBorder;
	int i, nSteps, nSteps2;
	int cx, cy;

	cx = rRect.Left + (rRect.Right - rRect.Left) / 2;
	cy = rRect.Top + (rRect.Bottom - rRect.Top) / 2;

	aRect = rRect;
	aRect.Top--; aRect.Left--; aRect.Bottom++; aRect.Right++;
	fW = rRect.Right - rRect.Left;
	fH = rRect.Bottom - rRect.Top;
	fAngle = (((double) grad->angle) * 3.14 / 1800.0);
	fDX = fW * fabs (cos (fAngle)) + fH * fabs (sin (fAngle));
	fDY = fH * fabs (cos (fAngle)) + fW * fabs (sin (fAngle));
	fDX = (fDX - fW) * 0.5 - 0.5;
	fDY = (fDY - fH) * 0.5 - 0.5;
	aRect.Left -= fDX;
	aRect.Right += fDX;
	aRect.Top -= fDY;
	aRect.Bottom += fDY;
	aFullRect = aRect;

	nBorder = grad->border * (aRect.Bottom - aRect.Top) / 100;
	if (grad->type == GRAD_LINEAR) {
		aRect.Top += nBorder;
	} else {
		nBorder >>= 1;
		aRect.Top += nBorder;
		aRect.Bottom -= nBorder;
	}

	if (aRect.Top > (aRect.Bottom - 1))
		aRect.Top = aRect.Bottom - 1;

	poly[0].x = aFullRect.Left;
	poly[0].y = aFullRect.Top;
	poly[1].x = aFullRect.Right;
	poly[1].y = aFullRect.Top;
	poly[2].x = aRect.Right;
	poly[2].y = aRect.Top;
	poly[3].x = aRect.Left;
	poly[3].y = aRect.Top;
	poly_rotate (&poly[0], 4, cx, cy, fAngle);

	redSteps = grad->end.red - grad->start.red;
	greenSteps = grad->end.green - grad->start.green;
	blueSteps = grad->end.blue - grad->start.blue;
	nSteps = grad->steps;
	if (nSteps == 0) {
		long mr;
		mr = aRect.Bottom - aRect.Top;
		if (mr < 50)
			nSteps = mr / 2;
		else
			nSteps = mr / 4;
		mr = abs(redSteps);
		if (abs(greenSteps) > mr) mr = abs(greenSteps);
		if (abs(blueSteps) > mr) mr = abs(blueSteps);
		if (mr < nSteps) nSteps = mr;
	}

	if (grad->type == GRAD_AXIAL) {
		if (nSteps & 1) nSteps++;
		nSteps2 = nSteps + 2;
		gcol = grad->end;
		redSteps <<= 1;
		greenSteps <<= 1;
		blueSteps <<= 1;
	} else {
		nSteps2 = nSteps + 1;
		gcol = grad->start;
	}

	fScanLine = aRect.Top;
	fScanInc  = (double)(aRect.Bottom - aRect.Top) / (double)nSteps;

	for (i = 0; i < nSteps2; i++) {
		// draw polygon
		ctx->drw->set_fg_color(drw_data, &gcol);
		ctx->drw->draw_polygon(drw_data, 1, &poly[0], 4);
		// calc next polygon
		aRect.Top = (long)(fScanLine += fScanInc);
		if (i == nSteps) {
			tempoly[0].x = aFullRect.Left;
			tempoly[0].y = aFullRect.Bottom;
			tempoly[1].x = aFullRect.Right;
			tempoly[1].y = aFullRect.Bottom;
		} else {
			tempoly[0].x = aRect.Left;
			tempoly[0].y = aRect.Top;
			tempoly[1].x = aRect.Right;
			tempoly[1].y = aRect.Top;
		}
		poly_rotate (&tempoly[0], 2, cx, cy, fAngle);
		poly[0] = poly[3];
		poly[1] = poly[2];
		poly[2] = tempoly[1];
		poly[3] = tempoly[0];
		// calc next color
		if (grad->type == GRAD_LINEAR) {
			gcol.red = grad->start.red + ((redSteps * i) / nSteps2);
			gcol.green = grad->start.green + ((greenSteps * i) / nSteps2);
			gcol.blue = grad->start.blue + ((blueSteps * i) / nSteps2);
		} else {
			if (i >= nSteps) {
				gcol.red = grad->end.red;
				gcol.green = grad->end.green;
				gcol.blue = grad->end.blue;
			} else {
				if (i <= (nSteps / 2)) {
					gcol.red = grad->end.red - ((redSteps * i) / nSteps2);
					gcol.green = grad->end.green - ((greenSteps * i) / nSteps2);
					gcol.blue = grad->end.blue - ((blueSteps * i) / nSteps2);
				} else {
					int i2 = i - nSteps / 2;
					gcol.red = grad->start.red + ((redSteps * i2) / nSteps2);
					gcol.green = grad->start.green + ((greenSteps * i2) / nSteps2);
					gcol.blue = grad->start.blue + ((blueSteps * i2) / nSteps2);
				}
			}
		}
	}
}

static void
r_draw_gradient_complex (ImpRenderCtx *ctx, void *drw_data, Gradient *grad)
{
	Rectangle rRect = { 0, 0, ctx->pix_w - 1, ctx->pix_h - 1 };
	Rectangle aRect = rRect;
	ImpColor gcol;
	ImpPoint poly[4];
	double fAngle = (((double) grad->angle) * 3.14 / 1800.0);
	long redSteps, greenSteps, blueSteps;
	long nZW, nZH;
	long bX, bY;
	long sW, sH;
	long cx, cy;
	int i;
	long nSteps;
	double sTop, sLeft, sRight, sBottom, sInc;
	int minRect;

	redSteps = grad->end.red - grad->start.red;
	greenSteps = grad->end.green - grad->start.green;
	blueSteps = grad->end.blue - grad->start.blue;

	if (grad->type == GRAD_SQUARE || grad->type == GRAD_RECTANGULAR) {
		double fW = aRect.Right - aRect.Left;
		double fH = aRect.Bottom - aRect.Top;
		double fDX = fW * fabs (cos (fAngle)) + fH * fabs (sin (fAngle));
		double fDY = fH * fabs (cos (fAngle)) + fW * fabs (sin (fAngle));
		fDX = (fDX - fW) * 0.5 - 0.5;
		fDY = (fDY - fH) * 0.5 - 0.5;
		aRect.Left -= fDX;
		aRect.Right += fDX;
		aRect.Top -= fDY;
		aRect.Bottom += fDY;
	}

	sW = aRect.Right - aRect.Left;
	sH = aRect.Bottom - aRect.Top;

	if (grad->type == GRAD_SQUARE) {
		if (sW > sH) sH = sW; else sW = sH;
	} else if (grad->type == GRAD_RADIAL) {
		sW = 0.5 + sqrt ((double)sW*(double)sW + (double)sH*(double)sH);
		sH = sW;
	} else if (grad->type == GRAD_ELLIPTICAL) {
		sW = 0.5 + (double)sW * 1.4142;
		sH = 0.5 + (double)sH * 1.4142;
	}

	nZW = (aRect.Right - aRect.Left) * grad->offset_x / 100;
	nZH = (aRect.Bottom - aRect.Top) * grad->offset_y / 100;
	bX = grad->border * sW / 100;
	bY = grad->border * sH / 100;
	cx = aRect.Left + nZW;
	cy = aRect.Top + nZH;

	sW -= bX;
	sH -= bY;

	aRect.Left = cx - ((aRect.Right - aRect.Left) >> 1);
	aRect.Top = cy - ((aRect.Bottom - aRect.Top) >> 1);

	nSteps = grad->steps;
	minRect = aRect.Right - aRect.Left;
	if (aRect.Bottom - aRect.Top < minRect) minRect = aRect.Bottom - aRect.Top;
	if (nSteps == 0) {
		long mr;
		if (minRect < 50)
			nSteps = minRect / 2;
		else
			nSteps = minRect / 4;
		mr = abs(redSteps);
		if (abs(greenSteps) > mr) mr = abs(greenSteps);
		if (abs(blueSteps) > mr) mr = abs(blueSteps);
		if (mr < nSteps) nSteps = mr;
	}

	sLeft = aRect.Left;
	sTop = aRect.Top;
	sRight = aRect.Right;
	sBottom = aRect.Bottom;
	sInc = (double) minRect / (double) nSteps * 0.5;

	gcol = grad->start;
	poly[0].x = rRect.Left;
	poly[0].y = rRect.Top;
	poly[1].x = rRect.Right;
	poly[1].y = rRect.Top;
	poly[2].x = rRect.Right;
	poly[2].y = rRect.Bottom;
	poly[3].x = rRect.Left;
	poly[3].y = rRect.Bottom;
	ctx->drw->set_fg_color(drw_data, &gcol);
	ctx->drw->draw_polygon(drw_data, 1, &poly[0], 4);

	for (i = 0; i < nSteps; i++) {
		aRect.Left = (long) (sLeft += sInc);
		aRect.Top = (long) (sTop += sInc);
		aRect.Right = (long) (sRight -= sInc);
		aRect.Bottom = (long) (sBottom -= sInc);
		if (aRect.Bottom - aRect.Top < 2 || aRect.Right - aRect.Left < 2)
			break;

		gcol.red = grad->start.red + (redSteps * (i+1) / nSteps);
		gcol.green = grad->start.green + (greenSteps * (i+1) / nSteps);
		gcol.blue = grad->start.blue + (blueSteps * (i+1) / nSteps);
		ctx->drw->set_fg_color(drw_data, &gcol);

		if (grad->type == GRAD_RADIAL || grad->type == GRAD_ELLIPTICAL) {
			ctx->drw->draw_arc(drw_data, 1, aRect.Left, aRect.Top,
				aRect.Right - aRect.Left, aRect.Bottom - aRect.Top,
				0, 360);
		} else {
			poly[0].x = aRect.Left;
			poly[0].y = aRect.Top;
			poly[1].x = aRect.Right;
			poly[1].y = aRect.Top;
			poly[2].x = aRect.Right;
			poly[2].y = aRect.Bottom;
			poly[3].x = aRect.Left;
			poly[3].y = aRect.Bottom;
			poly_rotate (&poly[0], 4, cx, cy, fAngle);
			ctx->drw->draw_polygon(drw_data, 1, &poly[0], 4);
		}
	}
}

void
r_draw_gradient (ImpRenderCtx *ctx, void *drw_data, iks *node)
{
//	GdkGC *gc;
	Gradient grad;
	char *stil, *tmp;
	iks *x;

	stil = r_get_style (ctx, node, "draw:fill-gradient-name");
	x = iks_find_with_attrib (iks_find (ctx->styles, "office:styles"),
		"draw:gradient", "draw:name", stil);
	if (x) {
		memset (&grad, 0, sizeof (Gradient));
		grad.type = -1;
		grad.offset_x = 50;
		grad.offset_y = 50;

		tmp = iks_find_attrib (x, "draw:start-color");
		if (tmp) r_parse_color (tmp, &grad.start);
		tmp = iks_find_attrib (x, "draw:start-intensity");
		if (tmp) {
			int val = atoi (tmp);
			grad.start.red = grad.start.red * val / 100;
			grad.start.green = grad.start.green * val / 100;
			grad.start.blue = grad.start.blue * val / 100;
		}
		tmp = iks_find_attrib (x, "draw:end-color");
		if (tmp) r_parse_color (tmp, &grad.end);
		tmp = iks_find_attrib (x, "draw:end-intensity");
		if (tmp) {
			int val = atoi (tmp);
			grad.end.red = grad.end.red * val / 100;
			grad.end.green = grad.end.green * val / 100;
			grad.end.blue = grad.end.blue * val / 100;
		}
		tmp = iks_find_attrib (x, "draw:angle");
		if (tmp) grad.angle = atoi(tmp) % 3600;
		tmp = iks_find_attrib (x, "draw:border");
		if (tmp) grad.border = atoi(tmp);
		tmp = r_get_style (ctx, node, "draw:gradient-step-count");
		if (tmp) grad.steps = atoi (tmp);
		tmp = iks_find_attrib (x, "draw:cx");
		if (tmp) grad.offset_x = atoi (tmp);
		tmp = iks_find_attrib (x, "draw:cy");
		if (tmp) grad.offset_y = atoi (tmp);
		tmp = iks_find_attrib (x, "draw:style");
		if (iks_strcmp (tmp, "linear") == 0)
			grad.type = GRAD_LINEAR;
		else if (iks_strcmp (tmp, "axial") == 0)
			grad.type = GRAD_AXIAL;
		else if (iks_strcmp (tmp, "radial") == 0)
			grad.type = GRAD_RADIAL;
		else if (iks_strcmp (tmp, "rectangular") == 0)
			grad.type = GRAD_RECTANGULAR;
		else if (iks_strcmp (tmp, "ellipsoid") == 0)
			grad.type = GRAD_ELLIPTICAL;
		else if (iks_strcmp (tmp, "square") == 0)
			grad.type = GRAD_SQUARE;

		if (grad.type == -1) return;

//		gc = ctx->gc;
//		ctx->gc = gdk_gc_new (ctx->d);
//		gdk_gc_copy (ctx->gc, gc);

		if (grad.type == GRAD_LINEAR || grad.type == GRAD_AXIAL)
			r_draw_gradient_simple (ctx, drw_data, &grad);
		else
			r_draw_gradient_complex (ctx, drw_data, &grad);

//		g_object_unref (ctx->gc);
//		ctx->gc = gc;
	}
}
