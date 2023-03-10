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
#ifndef _SYSDEP_H
#define _SYSDEP_H 1

/*
 * The purpose of this file is to define symbols that describe the
 * system-dependent features we use. Namely, byte order, native integer
 * types of various sizes, and safe pointer<->integer conversion.
 */

#include "config.h"

#ifdef WORDS_BIGENDIAN
#define WORD_BIG_ENDIAN 1
#else
#define WORD_LITTLE_ENDIAN 1
#endif

typedef unsigned long	Ulong;
typedef unsigned int	Uint;
typedef unsigned short	Ushort;
typedef unsigned char	Uchar;

/* this one's easy */
typedef unsigned char	Uint8;
typedef char		Int8;

/* define a datatype for 32bit integers (either int or long) */
#if SIZEOF_LONG == 4
typedef unsigned long	Uint32;
typedef long		Int32;
#else	/* SIZEOF_LONG != 4 */
#if SIZEOF_INT == 4
typedef unsigned int	Uint32;
typedef int		Int32;
#else	/* SIZEOF_INT != 4 */
#ifdef __cplusplus
#include "No.appropriate.32bit.native.type.found.Fix.sysdeps.h"
#else
#error No appropriate 32bit native type found. Fix sysdeps.h
#endif	/* ! __cplusplus */
#endif	/* SIZEOF_INT != 4 */
#endif	/* SIZEOF_LONG != 4 */

/* now 16bit integers (one of long, int or short) */
#if SIZEOF_SHORT == 2
typedef unsigned short	Uint16;
typedef short		Int16;
#else	/* SIZEOF_SHORT != 2 */
#if SIZEOF_INT == 2
typedef unsigned int	Uint16;
typedef short		Int16;
#else	/* SIZEOF_INT != 2 */
#ifdef __cplusplus
#include "No.appropriate.16bit.native.type.found.Fix.sysdeps.h"
#else
#error No appropriate 16bit native type found. Fix sysdeps.h
#endif	/* ! __cplusplus */
#endif	/* SIZEOF_INT != 2 */
#endif	/* SIZEOF_SHORT != 2 */

/*
 * An integer type to convert to and from pointers safely. All we do here is
 * look for an integer type with the same size as a pointer.
 */
#if SIZEOF_LONG == SIZEOF_VOID_P
typedef unsigned long	UINT;
typedef long		INT;
#else
#if SIZEOF_INT == SIZEOF_VOID_P
typedef unsigned int	UINT;
typedef int		INT;
#else
#if SIZEOF_SHORT == SIZEOF_VOID_P
typedef unsigned short	UINT;
typedef short		INT;
#else
#if SIZEOF_LONG_LONG == SIZEOF_VOID_P
typedef unsigned long long	UINT;
typedef long long		INT;
#else
#ifdef __cplusplus
#include "No.native.pointer-compatible.integer.type.found.Fix.sysdeps.h"
#else
#error No native pointer-compatible integer type found. Fix sysdeps.h
#endif
#endif
#endif
#endif
#endif

/* nice, uh? */
typedef void	*Pointer;

/* macros to do the safe pointer <-> integer conversions */
#define Ptr2Int(x)	((INT)((Pointer)(x)))
#define Int2Ptr(x)	((Pointer)((INT)(x)))

#ifdef _NO_PROTO
#define __PROTO(x)	()
#else
#define __PROTO(x)	x
#endif

#endif	/* _SYSDEP_H */
