/* ev-color-contrast.c
 * This file is part of Evince, a GNOME document viewer
 *
 * Copyright (C) 2020 Vanadiae <vanadiae35@gmail.com>
 *
 * Evince is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Evince is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <math.h>

#include "ev-color-contrast.h"

static double
get_srgb (const double color_component)
{
	/* calculation of sRGB color is based on note 1 of
         * https://www.w3.org/TR/2008/REC-WCAG20-20081211/#relativeluminancedef
	 */
	if (color_component <= 0.03928)
		return color_component / 12.92;
	else
		return powf (((color_component + 0.055) / 1.055), 2.4);
}

static double
get_relative_luminance (const GdkRGBA *color)
{
	/* calculation of relative luminance is based on note 1 of
	 * https://www.w3.org/TR/2008/REC-WCAG20-20081211/#relativeluminancedef
	 */
	return get_srgb (color->red) * 0.2126 + get_srgb (color->blue) * 0.0722 +
	       get_srgb (color->green) * 0.7152;
}

static double
get_contrast_level (const GdkRGBA *bg_color,
		    const GdkRGBA *fg_color)
{
	/* the contrast level calculus is based on WCAG 2.0 guideline 1.4  */
	/* https://www.w3.org/WAI/GL/UNDERSTANDING-WCAG20/visual-audio-contrast7.html#key-terms
	 */
	const double bg_luminance = get_relative_luminance (bg_color);
	const double fg_luminance = get_relative_luminance (fg_color);
	return (fmax (bg_luminance, fg_luminance) + 0.05) / (fmin (bg_luminance, fg_luminance) + 0.05);
}

/**
 * ev_color_get_most_readable_color:
 *
 * Returns: (transfer none): the most readable color on bg_color between
 *          first_color and second_color
 */
GdkRGBA *
ev_color_contrast_get_most_readable_color (const GdkRGBA *bg_color,
					   GdkRGBA       *first_color,
					   GdkRGBA       *second_color)
{
	const double first_contrast = get_contrast_level (bg_color, first_color);
	const double second_contrast = get_contrast_level (bg_color, second_color);

	/* higher is more readable (more contrast) */
	return first_contrast > second_contrast ? first_color : second_color;
}

/**
 * ev_color_get_best_foreground_color:
 *
 * Returns: (transfer full): the most readable foreground color on bg_color
 *          between black #000000 and white #FFFFFF
 */
GdkRGBA *
ev_color_contrast_get_best_foreground_color (const GdkRGBA *bg_color)
{
	GdkRGBA black, white;

	gdk_rgba_parse (&black, "#000000");
	gdk_rgba_parse (&white, "#FFFFFF");

	return gdk_rgba_copy (ev_color_contrast_get_most_readable_color (bg_color, &black, &white));
}
