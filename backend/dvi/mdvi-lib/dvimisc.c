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

