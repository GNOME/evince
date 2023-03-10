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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "mdvi.h"
#include "private.h"

void	mdvi_init_kpathsea(const char *program,
	const char *mfmode, const char *font, int dpi,
	const char *texmfcnf)
{
	const char *p;

	/* Stop meaningless output generation. */
	kpse_make_tex_discard_errors = FALSE;

	p = strrchr(program, '/');
	p = (p ? p + 1 : program);
	kpse_set_program_name(program, p);
	kpse_init_prog(p, dpi, mfmode, font);
	kpse_set_program_enabled(kpse_any_glyph_format, 1, kpse_src_compile);
	kpse_set_program_enabled(kpse_pk_format, 1, kpse_src_compile);
	kpse_set_program_enabled(kpse_tfm_format, 1, kpse_src_compile);
	kpse_set_program_enabled(kpse_ofm_format, 1, kpse_src_compile);
	if (texmfcnf != NULL)
		xputenv("TEXMFCNF", texmfcnf);
}

