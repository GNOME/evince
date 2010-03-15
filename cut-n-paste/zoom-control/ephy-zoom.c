/*
 *  Copyright (C) 2003 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  $Id$
 */

#include "config.h"

#include "ephy-zoom.h"

#include <math.h>

guint
ephy_zoom_get_zoom_level_index (float level)
{
	guint i;
	float previous, current, mean;

	/* Handle our options at the beginning of the list. */
	if (level == EPHY_ZOOM_BEST_FIT) {
	  return 0;
	} else if (level == EPHY_ZOOM_FIT_WIDTH) {
	  return 1;
	}

	previous = zoom_levels[3].level;

	for (i = 4; i < n_zoom_levels; i++)
	{
		current = zoom_levels[i].level;
		mean = sqrt (previous * current);

		if (level <= mean) return i - 1;

		previous = current;
	}

	return n_zoom_levels - 1;
}


float
ephy_zoom_get_changed_zoom_level (float level, gint steps)
{
	guint index;

	index = ephy_zoom_get_zoom_level_index (level);
	return zoom_levels[CLAMP(index + steps, 3, n_zoom_levels - 1)].level;
}

float	ephy_zoom_get_nearest_zoom_level (float level)
{
	return ephy_zoom_get_changed_zoom_level (level, 0);
}
