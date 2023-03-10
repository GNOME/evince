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
#include "mdvi.h"
#include "color.h"

void	mdvi_set_color(DviContext *dvi, Ulong fg, Ulong bg)
{
	if(dvi->curr_fg != fg || dvi->curr_bg != bg) {
		DEBUG((DBG_DEVICE, "setting color to (%lu,%lu)\n", fg, bg));
		if(dvi->device.set_color)
			dvi->device.set_color(dvi->device.device_data, fg, bg);
		dvi->curr_fg = fg;
		dvi->curr_bg = bg;
	}
}

void	mdvi_push_color(DviContext *dvi, Ulong fg, Ulong bg)
{
	if(dvi->color_top == dvi->color_size) {
		dvi->color_size += 32;
		dvi->color_stack = mdvi_realloc(dvi->color_stack,
			dvi->color_size * sizeof(DviColorPair));
	}
	dvi->color_stack[dvi->color_top].fg = dvi->curr_fg;
	dvi->color_stack[dvi->color_top].bg = dvi->curr_bg;
	dvi->color_top++;
	mdvi_set_color(dvi, fg, bg);
}

void	mdvi_pop_color(DviContext *dvi)
{
	Ulong	fg, bg;

	if(dvi->color_top == 0)
		return;
	dvi->color_top--;
	fg = dvi->color_stack[dvi->color_top].fg;
	bg = dvi->color_stack[dvi->color_top].bg;
	mdvi_set_color(dvi, fg, bg);
}

void	mdvi_reset_color(DviContext *dvi)
{
	dvi->color_top = 0;
	mdvi_set_color(dvi, dvi->params.fg, dvi->params.bg);
}

/* cache for color tables, to avoid creating them for every glyph */
typedef struct {
	Ulong	fg;
	Ulong	bg;
	Uint	nlevels;
	Ulong	*pixels;
	int	density;
	double	gamma;
	Uint	hits;
} ColorCache;

#define CCSIZE		256
static ColorCache	color_cache[CCSIZE];
static int		cc_entries;

#define GAMMA_DIFF	0.005


/* create a color table */
Ulong	*get_color_table(DviDevice *dev,
			 int nlevels, Ulong fg, Ulong bg, double gamma, int density)
{
	ColorCache	*cc, *tofree;
	int		lohits;
	Ulong		*pixels;
	int		status;

	lohits = color_cache[0].hits;
	tofree = &color_cache[0];
	/* look in the cache and see if we have one that matches this request */
	for(cc = &color_cache[0]; cc < &color_cache[cc_entries]; cc++) {
		if(cc->hits < lohits) {
			lohits = cc->hits;
			tofree = cc;
		}
		if(cc->fg == fg && cc->bg == bg && cc->density == density &&
		   cc->nlevels == nlevels && fabs(cc->gamma - gamma) <= GAMMA_DIFF)
		   	break;
	}

	if(cc < &color_cache[cc_entries]) {
		cc->hits++;
		return cc->pixels;
	}

	DEBUG((DBG_DEVICE, "Adding color table to cache (fg=%lu, bg=%lu, n=%d)\n",
		fg, bg, nlevels));

	/* no entry was found in the cache, create a new one */
	if(cc_entries < CCSIZE) {
		cc = &color_cache[cc_entries++];
		cc->pixels = NULL;
	} else {
		cc = tofree;
		mdvi_free(cc->pixels);
	}
	pixels = xnalloc(Ulong, nlevels);
	status = dev->alloc_colors(dev->device_data,
		pixels, nlevels, fg, bg, gamma, density);
	if(status < 0) {
		mdvi_free(pixels);
		return NULL;
	}
	cc->fg = fg;
	cc->bg = bg;
	cc->gamma = gamma;
	cc->density = density;
	cc->nlevels = nlevels;
	cc->pixels = pixels;
	cc->hits = 1;
	return pixels;
}
