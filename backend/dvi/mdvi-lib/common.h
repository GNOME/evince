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
#ifndef _MDVI_COMMON_H
#define _MDVI_COMMON_H 1

#include <stdio.h>
#include <sys/types.h>
#include <math.h>

#include "sysdeps.h"

#if STDC_HEADERS
#  include <string.h>
#endif

#if !defined(STDC_HEADERS) || defined(__STRICT_ANSI__)
#  ifndef HAVE_STRCHR
#    define strchr         index
#    define strrchr        rindex
#  endif
#  ifndef HAVE_MEMCPY
#    define memcpy(a,b,n)  bcopy((b), (a), (n))
#    define memmove(a,b,n) bcopy((b), (a), (n))
#  endif
#endif

#if defined(STDC_HEADERS) || defined(HAVE_MEMCPY)
#define memzero(a,n) memset((a), 0, (n))
#else
#define memzero(a,n) bzero((a), (n))
#endif

typedef struct _List {
	struct _List *next;
	struct _List *prev;
} List;
#define LIST(x)	((List *)(x))

typedef struct {
	char	*data;
	size_t	size;
	size_t	length;
} Buffer;

typedef struct {
	List	*head;
	List	*tail;
	int	count;
} ListHead;
#define MDVI_EMPTY_LIST_HEAD	{NULL, NULL, 0}

typedef struct {
	char	*data;
	size_t	size;
	size_t	length;
} Dstring;

/* Functions to read numbers from streams and memory */

#define fgetbyte(p)	((unsigned)getc(p))

extern char	*program_name;

extern Ulong	fugetn __PROTO((FILE *, size_t));
extern long	fsgetn __PROTO((FILE *, size_t));
extern Ulong	mugetn __PROTO((const Uchar *, size_t));
extern long	msgetn __PROTO((const Uchar *, size_t));

/* To read from a stream (fu: unsigned, fs: signed) */
#define fuget4(p)	fugetn((p), 4)
#define fuget3(p)	fugetn((p), 3)
#define fuget2(p)	fugetn((p), 2)
#define fuget1(p)	fgetbyte(p)
#define fsget4(p)	fsgetn((p), 4)
#define fsget3(p)	fsgetn((p), 3)
#define fsget2(p)	fsgetn((p), 2)
#define fsget1(p)	fsgetn((p), 1)

/* To read from memory (mu: unsigned, ms: signed) */
#define MUGETN(p,n)	((p) += (n), mugetn((p)-(n), (n)))
#define MSGETN(p,n)	((p) += (n), msgetn((p)-(n), (n)))
#define muget4(p)	MUGETN((p), 4)
#define muget3(p)	MUGETN((p), 3)
#define muget2(p)	MUGETN((p), 2)
#define muget1(p)	MUGETN((p), 1)
#define msget4(p)	MSGETN((p), 4)
#define msget3(p)	MSGETN((p), 3)
#define msget2(p)	MSGETN((p), 2)
#define msget1(p)	MSGETN((p), 1)

#define ROUND(x,y)	(((x) + (y) - 1) / (y))
#define FROUND(x)	(int)((x) + 0.5)
#define SFROUND(x)	(int)((x) >= 0 ? floor((x) + 0.5) : ceil((x) + 0.5))

#define Max(a,b)	(((a) > (b)) ? (a) : (b))
#define Min(a,b)	(((a) < (b)) ? (a) : (b))

/* make 2byte number from 2 8bit quantities */
#define HALFWORD(a,b)	((((a) << 8) & 0xf) | (b))
#define FULLWORD(a,b,c,d) \
	((((Int8)(a) << 24) & 0xff000000) | \
	(((Uint8)(b) << 16) & 0x00ff0000) | \
	(((Uint8)(c) << 8)  & 0x0000ff00) | \
	((Uint8)(d) & 0xff))

#if defined(__GNUC__) && !defined(__STRICT_ANSI__)
#define SWAPINT(a,b) \
	({ int _s = a; a = b; b = _s; })
#else
#define SWAPINT(a,b) do { int _s = a; a = b; b = _s; } while(0)
#endif

#define STREQ(a,b)	(strcmp((a), (b)) == 0)
#define STRNEQ(a,b,n)	(strncmp((a), (b), (n)) == 0)
#define STRCEQ(a,b)	(strcasecmp((a), (b)) == 0)
#define STRNCEQ(a,b,n)	(strncasecmp((a), (b), (n)) == 0)

extern char	*read_string __PROTO((FILE *, int, char *, size_t));
extern size_t	read_bcpl __PROTO((FILE *, char *, size_t, size_t));
extern char	*read_alloc_bcpl __PROTO((FILE *, size_t, size_t *));

/* miscellaneous */

extern void mdvi_message __PROTO((const char *, ...));
extern void mdvi_crash __PROTO((const char *, ...));
extern void mdvi_fatal __PROTO((const char *, ...));
extern void mdvi_error __PROTO((const char *, ...));
extern void mdvi_warning __PROTO((const char *, ...));
extern int  unit2pix __PROTO((int, const char *));
extern double unit2pix_factor __PROTO((const char *));

#define LOG_NONE	-1
#define LOG_INFO	0
#define LOG_WARN	1
#define LOG_ERROR	2
#define LOG_DEBUG	3

#define DBG_OPCODE	(1 << 0)
#define DBG_FONTS	(1 << 1)
#define DBG_FILES	(1 << 2)
#define DBG_DVI		(1 << 3)
#define DBG_PARAMS	(1 << 4)
#define DBG_SPECIAL	(1 << 5)
#define DBG_DEVICE	(1 << 6)
#define DBG_GLYPHS	(1 << 7)
#define DBG_BITMAPS	(1 << 8)
#define DBG_PATHS	(1 << 9)
#define DBG_SEARCH	(1 << 10)
#define DBG_VARS	(1 << 11)
#define DBG_BITMAP_OPS	(1 << 12)
#define DBG_BITMAP_DATA	(1 << 13)
#define DBG_TYPE1	(1 << 14)
#define DBG_TT		(1 << 15)
#define DBG_FT2		(1 << 16)
#define DBG_FMAP	(1 << 17)

#define DBG_SILENT	(1 << 31)

#ifdef NODEBUG
#define DEBUGGING(x)	0
#else
#define DEBUGGING(x)	(_mdvi_debug_mask & DBG_##x)
#endif

#ifndef NODEBUG
extern Uint32 _mdvi_debug_mask;
extern void __debug __PROTO((int, const char *, ...));
#define DEBUG(x)	__debug x
#define ASSERT(x) do { \
	if(!(x)) mdvi_crash("%s:%d: Assertion %s failed\n", \
		__FILE__, __LINE__, #x); \
	} while(0)
#define ASSERT_VALUE(x,y) do { \
	if((x) != (y)) \
		mdvi_crash("%s:%d: Assertion failed (%d = %s != %s)\n", \
		__FILE__, __LINE__, (x), #x, #x); \
	} while(0)
#else
#define DEBUG(x)	do { } while(0)
#define ASSERT(x)	do { } while(0)
#define ASSERT_VALUE(x,y)	do { } while(0)
#endif

#define set_debug_mask(m)	(_mdvi_debug_mask = (Uint32)(m))
#define add_debug_mask(m)	(_mdvi_debug_mask |= (Uint32)(m))
#define get_debug_mask()	_mdvi_debug_mask

/* memory allocation */

extern void  mdvi_free __PROTO((void *));
extern void *mdvi_malloc __PROTO((size_t));
extern void *mdvi_realloc __PROTO((void *, size_t));
extern void *mdvi_calloc __PROTO((size_t, size_t));
extern char *mdvi_strncpy __PROTO((char *, const char *, size_t));
extern char *mdvi_strdup __PROTO((const char *));
extern char *mdvi_strndup __PROTO((const char *, size_t));
extern void *mdvi_memdup __PROTO((const void *, size_t));
extern char *mdvi_build_path_from_cwd __PROTO((const char *));
extern char *mdvi_strrstr __PROTO((const char *, const char *));

/* macros to make memory allocation nicer */
#define xalloc(t)	(t *)mdvi_malloc(sizeof(t))
#define xnalloc(t,n)	(t *)mdvi_calloc((n), sizeof(t))
#define xresize(p,t,n)	(t *)mdvi_realloc((p), (n) * sizeof(t))

extern char *xstradd __PROTO((char *, size_t *, size_t, const char *, size_t));

extern Ulong get_mtime __PROTO((int));

/* lists */
extern void listh_init __PROTO((ListHead *));
extern void listh_prepend __PROTO((ListHead *, List *));
extern void listh_append __PROTO((ListHead *, List *));
extern void listh_add_before __PROTO((ListHead *, List *, List *));
extern void listh_add_after __PROTO((ListHead *, List *, List *));
extern void listh_remove __PROTO((ListHead *, List *));
extern void listh_concat __PROTO((ListHead *, ListHead *));
extern void listh_catcon __PROTO((ListHead *, ListHead *));

extern void buff_init __PROTO((Buffer *));
extern size_t buff_add __PROTO((Buffer *, const char *, size_t));
extern char *buff_gets __PROTO((Buffer *, size_t *));
extern void buff_free __PROTO((Buffer *));

extern char *getword __PROTO((char *, const char *, char **));
extern char *getstring __PROTO((char *, const char *, char **));

extern void dstring_init __PROTO((Dstring *));
extern int dstring_new __PROTO((Dstring *, const char *, int));
extern int dstring_append __PROTO((Dstring *, const char *, int));
extern int dstring_copy __PROTO((Dstring *, int, const char *, int));
extern int dstring_insert __PROTO((Dstring *, int, const char *, int));
extern void dstring_reset __PROTO((Dstring *));

#define dstring_length(d)	((d)->length)
#define dstring_strcat(d,s)	dstring_append((d), (s), -1)

extern char *dgets __PROTO((Dstring *, FILE *));
extern int  file_readable __PROTO((const char *));
extern int  file_exists __PROTO((const char *));
extern const char *file_basename __PROTO((const char *));
extern const char *file_extension __PROTO((const char *));

/*
 * Miscellaneous macros
 */

#define LIST_FOREACH(ptr, type, list) \
	for(ptr = (type *)(list)->head; ptr; ptr = (ptr)->next)

#define Size(x)	(sizeof(x) / sizeof((x)[0]))

/* multiply a fix_word by a 32bit number */
#define B0(x)	((x) & 0xff)
#define B1(x)	B0((x) >> 8)
#define B2(x)	B0((x) >> 16)
#define B3(x)	B0((x) >> 24)
#define __tfm_mul(z,t) \
	(((((B0(t) * (z)) >> 8) + (B1(t) * (z))) >> 8) + B2(t) * (z))
#define TFMSCALE(z,t,a,b) \
	((B3(t) == 255) ? \
		__tfm_mul((z), (t)) / (b) - (a) : \
		__tfm_mul((z), (t)) / (b))
#define TFMPREPARE(x,z,a,b) do { \
	a = 16; z = (x); \
	while(z > 040000000L) { z >>= 1; a <<= 1; } \
	b = 256 / a; a *= z; \
	} while(0)

#endif /* _MDVI_COMMON_H */
