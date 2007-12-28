/* imposter (OO.org Impress viewer)
** Copyright (C) 2003-2005 Gurer Ozen
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU General Public License.
*/

#include <config.h>
#include "common.h"
#include "internal.h"

ImpRenderCtx *
imp_create_context(const ImpDrawer *drw)
{
	ImpRenderCtx *ctx;

	ctx = calloc(1, sizeof(ImpRenderCtx));
	if (!ctx) return NULL;
	ctx->drw = drw;
	return ctx;
}

void
imp_context_set_page(ImpRenderCtx *ctx, ImpPage *page)
{
	ctx->page = page;
	ctx->content = page->doc->content;
	ctx->styles = page->doc->styles;
}

void
imp_context_set_step(ImpRenderCtx *ctx, int step)
{
	ctx->step = step;
}

void
imp_render(ImpRenderCtx *ctx, void *drw_data)
{
	// find drawing area size
	ctx->drw->get_size(drw_data, &ctx->pix_w, &ctx->pix_h);
	// find page size
	ctx->page->doc->get_geometry(ctx);
	// calculate ratio
	ctx->fact_x = ctx->pix_w / ctx->cm_w;
	ctx->fact_y = ctx->pix_h / ctx->cm_h;
	// call renderer
	ctx->page->doc->render_page(ctx, drw_data);
}

void
imp_delete_context(ImpRenderCtx *ctx)
{
	free(ctx);
}
