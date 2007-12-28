/* imposter (OO.org Impress viewer)
** Copyright (C) 2003-2005 Gurer Ozen
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU General Public License.
*/

#include <config.h>
#include "common.h"
#include "internal.h"

int
_imp_fill_back(ImpRenderCtx *ctx, void *drw_data, iks *node)
{
	ImpColor col;
	char *type;
	char *stil, *gfx;
	iks *x;

	type = r_get_style(ctx, node, "draw:fill");
	if (type == 0) return 0;

	if (strcmp(type, "solid") == 0) {
		if (r_get_color(ctx, node, "draw:fill-color", &col)) {
			ctx->drw->set_fg_color(drw_data, &col);
		}
		ctx->drw->draw_rect(drw_data, 1, 0, 0, ctx->pix_w, ctx->pix_h);
	} else if (strcmp (type, "bitmap") == 0) {
		stil = r_get_style(ctx, node, "draw:fill-image-name");
		x = iks_find_with_attrib(iks_find(ctx->styles, "office:styles"),
			"draw:fill-image", "draw:name", stil
		);
		gfx = iks_find_attrib(x, "xlink:href");
		if (gfx) {
			if (iks_strcmp(r_get_style(ctx, node, "style:repeat"), "stretch") == 0) {
				_imp_draw_image(ctx, drw_data, gfx, 0, 0, ctx->pix_w, ctx->pix_h);
			} else {
				_imp_tile_image(ctx, drw_data, gfx, 0, 0, ctx->pix_w, ctx->pix_h);
			}
		}
	} else if (strcmp(type, "gradient") == 0) {
		r_draw_gradient(ctx, drw_data, node);
	} else {
		return 0;
	}
	return 1;
}
