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
#ifndef _BITMAP_H
#define _BITMAP_H 1

#include "sysdeps.h"

/* Structures and functions to manipulate bitmaps */

/* bitmap unit (as in X's docs) */
typedef Uint32	BmUnit;

/* size (in bytes) of a bitmap atom */
#define BITMAP_BYTES	4

/* size (in bits) of a bitmap atom */
#define BITMAP_BITS	(BITMAP_BYTES << 3)

typedef struct {
	int	width;
	int	height;
	int	stride;
	BmUnit	*data;
} BITMAP;

#define BM_BYTES_PER_LINE(b)	\
	(ROUND((b)->width, BITMAP_BITS) * BITMAP_BYTES)
#define BM_WIDTH(b)	(((BITMAP *)(b))->width)
#define BM_HEIGHT(b)	(((BITMAP *)(b))->height)

#define BMBIT(n)	((BmUnit)1 << (n))

/* Macros to manipulate individual pixels in a bitmap
 * (they are slow, don't use them)
 */

#define bm_offset(b,o) (BmUnit *)((Uchar *)(b) + (o))

#define __bm_unit_ptr(b,x,y) \
	bm_offset((b)->data, (y) * (b)->stride + \
	((x) / BITMAP_BITS) * BITMAP_BYTES)

#define __bm_unit(b,x,y)    __bm_unit_ptr((b), (x), (y))[0]

#define BM_GETPIXEL(b,x,y)  __bm_unit((b), (x), (y))
#define BM_SETPIXEL(b,x,y) (__bm_unit((b), (x), (y)) |= FIRSTMASKAT(x))
#define BM_CLRPIXEL(b,x,y) (__bm_unit((b), (x), (y)) &= ~FIRSTMASKAT(x))

/*
 * These macros are used to access pixels in a bitmap. They are supposed
 * to be used like this:
 */
#if 0
    BmUnit	*row, mask;

    mask = FIRSTMASK;

    /* position `unit' at coordinates (column_number, row_number) */
    unit = (BmUnit *)((char *)bitmap->data + row_number * bitmap->stride
                      + (column_number / BITMAP_BITS);
    /* loop over all pixels IN THE SAME ROW */
    for(i = 0; i < number_of_pixels; i++) {
       /* to test if a pixel is set */
       if(*unit & mask) {
          /* yes, it is, do something with it */
       }
       /* to set/clear a pixel */
       if(painting)
          *unit |= mask;  /* now you see it */
       else
          *unit &= ~mask; /* now you don't */
       /* move to next pixel */
       if(mask == LASTMASK) {
          unit++;
          UPDATEMASK(mask);
       }
    }
/* end of sample code */
#endif

/* bitmaps are stored in native byte order */
#ifdef WORD_BIG_ENDIAN
#define FIRSTSHIFT	(BITMAP_BITS - 1)
#define LASTSHIFT	0
#define NEXTMASK(m)	((m) >>= 1)
#define PREVMASK(m)	((m) <<= 1)
#define FIRSTSHIFTAT(c)	(BITMAP_BITS - ((c) % BITMAP_BITS) - 1)
#else
#define FIRSTSHIFT	0
#define LASTSHIFT	(BITMAP_BITS - 1)
#define NEXTMASK(m)	((m) <<= 1)
#define PREVMASK(m)	((m) >>= 1)
#define FIRSTSHIFTAT(c)	((c) % BITMAP_BITS)
#endif

#define FIRSTMASK	BMBIT(FIRSTSHIFT)
#define FIRSTMASKAT(c)	BMBIT(FIRSTSHIFTAT(c))
#define LASTMASK	BMBIT(LASTSHIFT)

extern BITMAP	*bitmap_alloc __PROTO((int, int));
extern BITMAP	*bitmap_alloc_raw __PROTO((int, int));
extern void	bitmap_destroy __PROTO((BITMAP *));

/*
 * set_row(bm, row, col, count, state):
 *   sets `count' pixels to state `onoff', starting from pixel
 *   at position (col, row). All pixels must lie in the same
 *   row.
 */
extern void bitmap_set_col __PROTO((BITMAP *, int, int, int, int));
extern void bitmap_set_row __PROTO((BITMAP *, int, int, int, int));

extern void bitmap_paint_bits __PROTO((BmUnit *, int, int));
extern void bitmap_clear_bits __PROTO((BmUnit *, int, int));

extern BITMAP *bitmap_copy __PROTO((BITMAP *));
extern void bitmap_flip_horizontally __PROTO((BITMAP *));
extern void bitmap_flip_vertically __PROTO((BITMAP *));
extern void bitmap_flip_diagonally __PROTO((BITMAP *));
extern void bitmap_rotate_clockwise __PROTO((BITMAP *));
extern void bitmap_rotate_counter_clockwise __PROTO((BITMAP *));
extern void bitmap_flip_rotate_clockwise __PROTO((BITMAP *));
extern void bitmap_flip_rotate_counter_clockwise __PROTO((BITMAP *));
extern BITMAP *bitmap_convert_lsb8 __PROTO((Uchar *, int, int, int));
extern BITMAP *bitmap_convert_msb8 __PROTO((Uchar *, int, int, int));

#include <stdio.h>
extern void	bitmap_print __PROTO((FILE *, BITMAP *));

#endif /* _BITMAP_H */
