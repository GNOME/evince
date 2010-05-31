/*
 *  Copyright (C) 2003 Christian Persch
 *
 *  Modified 2005 by James Bowes for use in evince.
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

#ifndef EPHY_ZOOM_H
#define EPHY_ZOOM_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>

G_BEGIN_DECLS

#define EPHY_ZOOM_BEST_FIT  (-3.0)
#define EPHY_ZOOM_FIT_WIDTH (-4.0)
#define EPHY_ZOOM_SEPARATOR (-5.0)

static const
struct
{
	gchar *name;
	float level;
}

zoom_levels[] =
{
	{ N_("Best Fit"),       EPHY_ZOOM_BEST_FIT  },
	{ N_("Fit Page Width"), EPHY_ZOOM_FIT_WIDTH },
	{ NULL,                 EPHY_ZOOM_SEPARATOR },
	{ N_("50%"), 0.5 },
	{ N_("70%"), 0.7071067811 },
	{ N_("85%"), 0.8408964152 },
	{ N_("100%"), 1.0 },
	{ N_("125%"), 1.1892071149 },
	{ N_("150%"), 1.4142135623 },
	{ N_("175%"), 1.6817928304 },
	{ N_("200%"), 2.0 },
	{ N_("300%"), 2.8284271247 },
	{ N_("400%"), 4.0 },
	{ N_("800%"), 8.0 },
	{ N_("1600%"), 16.0 },
	{ N_("3200%"), 32.0 },
	{ N_("6400%"), 64.0 }
};
static const guint n_zoom_levels = G_N_ELEMENTS (zoom_levels);

#define ZOOM_MINIMAL	(EPHY_ZOOM_SEPARATOR)
#define ZOOM_MAXIMAL	(zoom_levels[n_zoom_levels - 1].level)
#define ZOOM_IN		(-1.0)
#define ZOOM_OUT	(-2.0)

guint	ephy_zoom_get_zoom_level_index	 (float level);

float	ephy_zoom_get_changed_zoom_level (float level, gint steps);

float	ephy_zoom_get_nearest_zoom_level (float level);

G_END_DECLS

#endif
