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
#ifndef _MDVI_PRIVATE_H
#define _MDVI_PRIVATE_H 1

#define HAVE_PROTOTYPES 1

#if STDC_HEADERS
#  /* kpathsea's headers (wrongly!) redefine strchr() and strrchr() to
#     non ANSI C functions if HAVE_STRCHR and HAVE_STRRCHR are not defined.
#   */
#  ifndef HAVE_STRCHR
#     define HAVE_STRCHR
#   endif
#  ifndef HAVE_STRRCHR
#    define HAVE_STRRCHR
#  endif
#endif

#include <kpathsea/debug.h>
#include <kpathsea/tex-file.h>
#include <kpathsea/tex-glyph.h>
#include <kpathsea/cnf.h>
#include <kpathsea/proginit.h>
#include <kpathsea/progname.h>
#include <kpathsea/tex-make.h>
#include <kpathsea/lib.h>

#define ISSP(p)		(*(p) == ' ' || *(p) == '\t')
#define SKIPSP(p)	while(ISSP(p)) p++
#define SKIPNSP(p)	while(*(p) && !ISSP(p)) p++

#include <libintl.h>
#define _(x)	gettext(x)
#define _G(x)	x

#if defined (__i386__) && defined (__GNUC__) && __GNUC__ >= 2
#define	_BREAKPOINT()		do { __asm__ __volatile__ ("int $03"); } while(0)
#elif defined (__alpha__) && defined (__GNUC__) && __GNUC__ >= 2
#define	_BREAKPOINT()		do { __asm__ __volatile__ ("bpt"); } while(0)
#else	/* !__i386__ && !__alpha__ */
#define	_BREAKPOINT()
#endif	/* __i386__ */

#endif /* _MDVI_PRIVATE_H */
