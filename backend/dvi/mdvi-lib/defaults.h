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
#ifndef _MDVI_DEFAULTS_H
#define _MDVI_DEFAULTS_H 1

/* resolution */
#define	MDVI_DPI	600
#define MDVI_VDPI	MDVI_DPI

/* horizontal margins */
#define MDVI_HMARGIN	"1in"

/* vertical margins */
#define MDVI_VMARGIN	"1in"

/* rulers */
#define MDVI_HRUNITS	"1in"
#define MDVI_VRUNITS	"1in"

/* paper */
#define MDVI_PAPERNAME	"letter"

/* magnification */
#define MDVI_MAGNIFICATION 1.0

/* fallback font */
#define MDVI_FALLBACK_FONT	"cmr10"

/* metafont mode */
#define MDVI_MFMODE	NULL

/* default shrinking factor */
#define MDVI_DEFAULT_SHRINKING	-1 /* based on resolution */

/* default pixel density */
#define MDVI_DEFAULT_DENSITY	50

/* default gamma correction */
#define MDVI_DEFAULT_GAMMA	1.0

/* default window geometry */
#define MDVI_GEOMETRY	NULL

/* default orientation */
#define MDVI_ORIENTATION	"tblr"

/* colors */
#define MDVI_FOREGROUND		"black"
#define MDVI_BACKGROUND		"white"

/* flags */
#define MDVI_DEFAULT_FLAGS	MDVI_ANTIALIASED

#define MDVI_DEFAULT_CONFIG	"mdvi.conf"

#endif /* _MDVI_DEAFAULTS_H */
