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


#ifndef _COLOR_H_
#define _COLOR_H_

#include "common.h"

extern Ulong	*get_color_table(DviDevice *dev,
				 int nlevels, Ulong fg, Ulong bg, double gamma, int density);

extern void mdvi_set_color __PROTO((DviContext *, Ulong, Ulong));
extern void mdvi_push_color __PROTO((DviContext *, Ulong, Ulong));
extern void mdvi_pop_color __PROTO((DviContext *));
extern void mdvi_reset_color __PROTO((DviContext *));

#endif /* _COLOR_H_ */

