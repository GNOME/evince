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

/* Bitmap manipulation routines */

#include <config.h>
#include <stdlib.h>

#include "mdvi.h"
#include "color.h"

/* bit_masks[n] contains a BmUnit with `n' contiguous bits */

static BmUnit bit_masks[] = {
	0x0,		0x1,		0x3,		0x7,
	0xf,		0x1f,		0x3f,		0x7f,
	0xff,
#if BITMAP_BYTES > 1
			0x1ff,		0x3ff,		0x7ff,
	0xfff,		0x1fff,		0x3fff,		0x7fff,
	0xffff,
#if BITMAP_BYTES > 2
			0x1ffff,	0x3ffff,	0x7ffff,
	0xfffff,	0x1fffff,	0x3fffff,	0x7fffff,
	0xffffff,	0x1ffffff,	0x3ffffff,	0x7ffffff,
	0xfffffff,	0x1fffffff,	0x3fffffff,	0x7fffffff,
	0xffffffff
#endif /* BITMAP_BYTES > 2 */
#endif /* BITMAP_BYTES > 1 */
};

#ifndef NODEBUG
#define SHOW_OP_DATA	(DEBUGGING(BITMAP_OPS) && DEBUGGING(BITMAP_DATA))
#endif

/*
 * Some useful macros to manipulate bitmap data
 * SEGMENT(m,n) = bit mask for a segment of `m' contiguous bits
 * starting at column `n'. These macros assume that
 *    m + n <= BITMAP_BITS, 0 <= m, n.
 */
#ifdef WORD_BIG_ENDIAN
#define SEGMENT(m,n)	(bit_masks[m] << (BITMAP_BITS - (m) - (n)))
#else
#define SEGMENT(m,n)	(bit_masks[m] << (n))
#endif

/* sampling and shrinking routines shamelessly stolen from xdvi */

static int sample_count[] = {
	0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
};

/* bit_swap[j] = j with all bits inverted (i.e. msb -> lsb) */
static Uchar bit_swap[] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};


/*
 * next we have three bitmap functions to convert bitmaps in LSB bit order
 * with 8, 16 and 32 bits per unit, to our internal format. The differences
 * are minimal, but writing a generic function to handle all unit sizes is
 * hopelessly slow.
 */

BITMAP	*bitmap_convert_lsb8(Uchar *bits, int w, int h, int stride)
{
	BITMAP	*bm;
	int	i;
	Uchar	*unit;
	register Uchar *curr;
	int	bytes;

	DEBUG((DBG_BITMAP_OPS, "convert LSB %dx%d@8 -> bitmap\n", w, h));

	bm = bitmap_alloc_raw(w, h);

	/* this is the number of bytes in the original bitmap */
	bytes = ROUND(w, 8);
	unit  = (Uchar *)bm->data;
	curr = bits;
	/* we try to do this as fast as we can */
	for(i = 0; i < h; i++) {
#ifdef WORD_LITTLE_ENDIAN
		memcpy(unit, curr, bytes);
		curr += stride;
#else
		int	j;

		for(j = 0; j < bytes; curr++, j++)
			unit[j] = bit_swap[*curr];
		cur += stride - bytes;
#endif
		memzero(unit + bytes, bm->stride - bytes);
		unit  += bm->stride;
	}
	if(SHOW_OP_DATA)
		bitmap_print(stderr, bm);
	return bm;
}

BITMAP	*bitmap_convert_msb8(Uchar *data, int w, int h, int stride)
{
	BITMAP	*bm;
	Uchar	*unit;
	Uchar	*curr;
	int	i;
	int	bytes;

	bm = bitmap_alloc(w, h);
	bytes = ROUND(w, 8);
	unit = (Uchar *)bm->data;
	curr = data;
	for(i = 0; i < h; i++) {
#ifdef WORD_LITTLE_ENDIAN
		int	j;

		for(j = 0; j < bytes; curr++, j++)
			unit[j] = bit_swap[*curr];
		curr += stride - bytes;
#else
		memcpy(unit, curr, bytes);
		curr += stride;
#endif
		memzero(unit + bytes, bm->stride - bytes);
		unit += bm->stride;
	}
	if(SHOW_OP_DATA)
		bitmap_print(stderr, bm);
	return bm;
}


BITMAP	*bitmap_copy(BITMAP *bm)
{
	BITMAP	*nb = bitmap_alloc(bm->width, bm->height);

	DEBUG((DBG_BITMAP_OPS, "copy %dx%d\n", bm->width, bm->height));
	memcpy(nb->data, bm->data, bm->height * bm->stride);
	return nb;
}

BITMAP	*bitmap_alloc(int w, int h)
{
	BITMAP	*bm;

	bm = xalloc(BITMAP);
	bm->width = w;
	bm->height = h;
	bm->stride = BM_BYTES_PER_LINE(bm);
	if(h && bm->stride)
		bm->data = (BmUnit *)mdvi_calloc(h, bm->stride);
	else
		bm->data = NULL;

	return bm;
}

BITMAP	*bitmap_alloc_raw(int w, int h)
{
	BITMAP	*bm;

	bm = xalloc(BITMAP);
	bm->width = w;
	bm->height = h;
	bm->stride = BM_BYTES_PER_LINE(bm);
	if(h && bm->stride)
		bm->data = (BmUnit *)mdvi_malloc(h * bm->stride);
	else
		bm->data = NULL;

	return bm;
}

void	bitmap_destroy(BITMAP *bm)
{
	if(bm->data)
		free(bm->data);
	free(bm);
}

void	bitmap_print(FILE *out, BITMAP *bm)
{
	int	i, j;
	BmUnit	*a, mask;
	static const char labels[] = {
		'1', '2', '3', '4', '5', '6', '7', '8', '9', '0'
	};
	int	sub;

	a = bm->data;
	fprintf(out, "    ");
	if(bm->width > 10) {
		putchar('0');
		sub = 0;
		for(j = 2; j <= bm->width; j++)
			if((j %10) == 0) {
				if((j % 100) == 0) {
					fprintf(out, "*");
					sub += 100;
				} else
					fprintf(out, "%d", (j - sub)/10);
			} else
				putc(' ', out);
		fprintf(out, "\n    ");
	}
	for(j = 0; j < bm->width; j++)
		putc(labels[j % 10], out);
	putchar('\n');
	for(i = 0; i < bm->height; i++) {
		mask = FIRSTMASK;
		a = (BmUnit *)((char *)bm->data + i * bm->stride);
		fprintf(out, "%3d ", i+1);
		for(j = 0; j < bm->width; j++) {
			if(*a & mask)
				putc('#', out);
			else
				putc('.', out);
			if(mask == LASTMASK) {
				a++;
				mask = FIRSTMASK;
			} else
				NEXTMASK(mask);
		}
		putchar('\n');
	}
}

void bitmap_set_col(BITMAP *bm, int row, int col, int count, int state)
{
	BmUnit	*ptr;
	BmUnit	mask;

	ptr = __bm_unit_ptr(bm, col, row);
	mask = FIRSTMASKAT(col);

	while(count-- > 0) {
		if(state)
			*ptr |= mask;
		else
			*ptr &= ~mask;
		/* move to next row */
		ptr = bm_offset(ptr, bm->stride);
	}
}

/*
 * to use this function you should first make sure that
 * there is room for `count' bits in the scanline
 *
 * A general-purpose (but not very efficient) function to paint `n' pixels
 * on a bitmap, starting at position (x, y) would be:
 *
 *    bitmap_paint_bits(__bm_unit_ptr(bitmap, x, y), x % BITMAP_BITS, n)
 *
 */
void	bitmap_paint_bits(BmUnit *ptr, int n, int count)
{
	/* paint the head */
	if(n + count > BITMAP_BITS) {
		*ptr |= SEGMENT(BITMAP_BITS - n, n);
		count -= BITMAP_BITS - n;
		ptr++;
	} else {
		*ptr |= SEGMENT(count, n);
		return;
	}

	/* paint the middle */
	for(; count >= BITMAP_BITS; count -= BITMAP_BITS)
		*ptr++ = bit_masks[BITMAP_BITS];

	/* paint the tail */
	if(count > 0)
		*ptr |= SEGMENT(count, 0);
}

/*
 * same as paint_bits but clears pixels instead of painting them. Written
 * as a separate function for efficiency reasons.
 */
void bitmap_clear_bits(BmUnit *ptr, int n, int count)
{
	if(n + count > BITMAP_BITS) {
		*ptr &= ~SEGMENT(BITMAP_BITS - n, n);
		count -= BITMAP_BITS;
		ptr++;
	} else {
		*ptr &= ~SEGMENT(count, n);
		return;
	}

	for(; count >= BITMAP_BITS; count -= BITMAP_BITS)
		*ptr++ = 0;

	if(count > 0)
		*ptr &= ~SEGMENT(count, 0);
}

/* the general function to paint rows. Still used by the PK reader, but that
 * will change soon (The GF reader already uses bitmap_paint_bits()).
 */
void	bitmap_set_row(BITMAP *bm, int row, int col, int count, int state)
{
	BmUnit	*ptr;

	ptr = __bm_unit_ptr(bm, col, row);
	if(state)
		bitmap_paint_bits(ptr, col & (BITMAP_BITS-1), count);
	else
		bitmap_clear_bits(ptr, col & (BITMAP_BITS-1), count);
}

/*
 * Now several `flipping' operations
 */

void bitmap_flip_horizontally(BITMAP *bm)
{
	BITMAP	nb;
	BmUnit	*fptr, *tptr;
	BmUnit	fmask, tmask;
	int	w, h;

	nb.width = bm->width;
	nb.height = bm->height;
	nb.stride = bm->stride;
	nb.data = mdvi_calloc(bm->height, bm->stride);

	fptr = bm->data;
	tptr = __bm_unit_ptr(&nb, nb.width-1, 0);
	for(h = 0; h < bm->height; h++) {
		BmUnit	*fline, *tline;

		fline = fptr;
		tline = tptr;
		fmask = FIRSTMASK;
		tmask = FIRSTMASKAT(nb.width-1);
		for(w = 0; w < bm->width; w++) {
			if(*fline & fmask)
				*tline |= tmask;
			if(fmask == LASTMASK) {
				fmask = FIRSTMASK;
				fline++;
			} else
				NEXTMASK(fmask);
			if(tmask == FIRSTMASK) {
				tmask = LASTMASK;
				tline--;
			} else
				PREVMASK(tmask);
		}
		fptr = bm_offset(fptr, bm->stride);
		tptr = bm_offset(tptr, bm->stride);
	}
	DEBUG((DBG_BITMAP_OPS, "flip_horizontally (%d,%d) -> (%d,%d)\n",
		bm->width, bm->height, nb.width, nb.height));
	mdvi_free(bm->data);
	bm->data = nb.data;
	if(SHOW_OP_DATA)
		bitmap_print(stderr, bm);
}

void	bitmap_flip_vertically(BITMAP *bm)
{
	BITMAP	nb;
	BmUnit	*fptr, *tptr;
	BmUnit	fmask;
	int	w, h;

	nb.width = bm->width;
	nb.height = bm->height;
	nb.stride = bm->stride;
	nb.data = mdvi_calloc(bm->height, bm->stride);

	fptr = bm->data;
	tptr = __bm_unit_ptr(&nb, 0, nb.height-1);
	for(h = 0; h < bm->height; h++) {
		BmUnit	*fline, *tline;

		fline = fptr;
		tline = tptr;
		fmask = FIRSTMASK;
		for(w = 0; w < bm->width; w++) {
			if(*fline & fmask)
				*tline |= fmask;
			if(fmask == LASTMASK) {
				fmask = FIRSTMASK;
				fline++;
				tline++;
			} else
				NEXTMASK(fmask);
		}
		fptr = bm_offset(fptr, bm->stride);
		tptr = (BmUnit *)((char *)tptr - bm->stride);
	}
	DEBUG((DBG_BITMAP_OPS, "flip_vertically (%d,%d) -> (%d,%d)\n",
		bm->width, bm->height, nb.width, nb.height));
	mdvi_free(bm->data);
	bm->data = nb.data;
	if(SHOW_OP_DATA)
		bitmap_print(stderr, bm);
}

void	bitmap_flip_diagonally(BITMAP *bm)
{
	BITMAP	nb;
	BmUnit	*fptr, *tptr;
	BmUnit	fmask, tmask;
	int	w, h;

	nb.width = bm->width;
	nb.height = bm->height;
	nb.stride = bm->stride;
	nb.data = mdvi_calloc(bm->height, bm->stride);

	fptr = bm->data;
	tptr = __bm_unit_ptr(&nb, nb.width-1, nb.height-1);
	for(h = 0; h < bm->height; h++) {
		BmUnit	*fline, *tline;

		fline = fptr;
		tline = tptr;
		fmask = FIRSTMASK;
		tmask = FIRSTMASKAT(nb.width-1);
		for(w = 0; w < bm->width; w++) {
			if(*fline & fmask)
				*tline |= tmask;
			if(fmask == LASTMASK) {
				fmask = FIRSTMASK;
				fline++;
			} else
				NEXTMASK(fmask);
			if(tmask == FIRSTMASK) {
				tmask = LASTMASK;
				tline--;
			} else
				PREVMASK(tmask);
		}
		fptr = bm_offset(fptr, bm->stride);
		tptr = bm_offset(tptr, -nb.stride);
	}
	DEBUG((DBG_BITMAP_OPS, "flip_diagonally (%d,%d) -> (%d,%d)\n",
		bm->width, bm->height, nb.width, nb.height));
	mdvi_free(bm->data);
	bm->data = nb.data;
	if(SHOW_OP_DATA)
		bitmap_print(stderr, bm);
}

void	bitmap_rotate_clockwise(BITMAP *bm)
{
	BITMAP	nb;
	BmUnit	*fptr, *tptr;
	BmUnit	fmask, tmask;
	int	w, h;

	nb.width = bm->height;
	nb.height = bm->width;
	nb.stride = BM_BYTES_PER_LINE(&nb);
	nb.data = mdvi_calloc(nb.height, nb.stride);

	fptr = bm->data;
	tptr = __bm_unit_ptr(&nb, nb.width - 1, 0);

	tmask = FIRSTMASKAT(nb.width-1);
	for(h = 0; h < bm->height; h++) {
		BmUnit	*fline, *tline;

		fmask = FIRSTMASK;
		fline = fptr;
		tline = tptr;
		for(w = 0; w < bm->width; w++) {
			if(*fline & fmask)
				*tline |= tmask;
			if(fmask == LASTMASK) {
				fmask = FIRSTMASK;
				fline++;
			} else
				NEXTMASK(fmask);
			/* go to next row */
			tline = bm_offset(tline, nb.stride);
		}
		fptr = bm_offset(fptr, bm->stride);
		if(tmask == FIRSTMASK) {
			tmask = LASTMASK;
			tptr--;
		} else
			PREVMASK(tmask);
	}

	DEBUG((DBG_BITMAP_OPS, "rotate_clockwise (%d,%d) -> (%d,%d)\n",
		bm->width, bm->height, nb.width, nb.height));
	mdvi_free(bm->data);
	bm->data = nb.data;
	bm->width = nb.width;
	bm->height = nb.height;
	bm->stride = nb.stride;
	if(SHOW_OP_DATA)
		bitmap_print(stderr, bm);
}

void	bitmap_rotate_counter_clockwise(BITMAP *bm)
{
	BITMAP	nb;
	BmUnit	*fptr, *tptr;
	BmUnit	fmask, tmask;
	int	w, h;

	nb.width = bm->height;
	nb.height = bm->width;
	nb.stride = BM_BYTES_PER_LINE(&nb);
	nb.data = mdvi_calloc(nb.height, nb.stride);

	fptr = bm->data;
	tptr = __bm_unit_ptr(&nb, 0, nb.height - 1);

	tmask = FIRSTMASK;
	for(h = 0; h < bm->height; h++) {
		BmUnit	*fline, *tline;

		fmask = FIRSTMASK;
		fline = fptr;
		tline = tptr;
		for(w = 0; w < bm->width; w++) {
			if(*fline & fmask)
				*tline |= tmask;
			if(fmask == LASTMASK) {
				fmask = FIRSTMASK;
				fline++;
			} else
				NEXTMASK(fmask);
			/* go to previous row */
			tline = bm_offset(tline, -nb.stride);
		}
		fptr = bm_offset(fptr, bm->stride);
		if(tmask == LASTMASK) {
			tmask = FIRSTMASK;
			tptr++;
		} else
			NEXTMASK(tmask);
	}

	DEBUG((DBG_BITMAP_OPS, "rotate_counter_clockwise (%d,%d) -> (%d,%d)\n",
		bm->width, bm->height, nb.width, nb.height));
	mdvi_free(bm->data);
	bm->data = nb.data;
	bm->width = nb.width;
	bm->height = nb.height;
	bm->stride = nb.stride;
	if(SHOW_OP_DATA)
		bitmap_print(stderr, bm);
}

void	bitmap_flip_rotate_clockwise(BITMAP *bm)
{
	BITMAP	nb;
	BmUnit	*fptr, *tptr;
	BmUnit	fmask, tmask;
	int	w, h;

	nb.width = bm->height;
	nb.height = bm->width;
	nb.stride = BM_BYTES_PER_LINE(&nb);
	nb.data = mdvi_calloc(nb.height, nb.stride);

	fptr = bm->data;
	tptr = __bm_unit_ptr(&nb, nb.width-1, nb.height-1);

	tmask = FIRSTMASKAT(nb.width-1);
	for(h = 0; h < bm->height; h++) {
		BmUnit	*fline, *tline;

		fmask = FIRSTMASK;
		fline = fptr;
		tline = tptr;
		for(w = 0; w < bm->width; w++) {
			if(*fline & fmask)
				*tline |= tmask;
			if(fmask == LASTMASK) {
				fmask = FIRSTMASK;
				fline++;
			} else
				NEXTMASK(fmask);
			/* go to previous line */
			tline = bm_offset(tline, -nb.stride);
		}
		fptr = bm_offset(fptr, bm->stride);
		if(tmask == FIRSTMASK) {
			tmask = LASTMASK;
			tptr--;
		} else
			PREVMASK(tmask);
	}
	DEBUG((DBG_BITMAP_OPS, "flip_rotate_clockwise (%d,%d) -> (%d,%d)\n",
		bm->width, bm->height, nb.width, nb.height));
	mdvi_free(bm->data);
	bm->data = nb.data;
	bm->width = nb.width;
	bm->height = nb.height;
	bm->stride = nb.stride;
	if(SHOW_OP_DATA)
		bitmap_print(stderr, bm);
}

void	bitmap_flip_rotate_counter_clockwise(BITMAP *bm)
{
	BITMAP	nb;
	BmUnit	*fptr, *tptr;
	BmUnit	fmask, tmask;
	int	w, h;

	nb.width = bm->height;
	nb.height = bm->width;
	nb.stride = BM_BYTES_PER_LINE(&nb);
	nb.data = mdvi_calloc(nb.height, nb.stride);

	fptr = bm->data;
	tptr = nb.data;
	tmask = FIRSTMASK;

	for(h = 0; h < bm->height; h++) {
		BmUnit	*fline, *tline;

		fmask = FIRSTMASK;
		fline = fptr;
		tline = tptr;
		for(w = 0; w < bm->width; w++) {
			if(*fline & fmask)
				*tline |= tmask;
			if(fmask == LASTMASK) {
				fmask = FIRSTMASK;
				fline++;
			} else
				NEXTMASK(fmask);
			/* go to next line */
			tline = bm_offset(tline, nb.stride);
		}
		fptr = bm_offset(fptr, bm->stride);
		if(tmask == LASTMASK) {
			tmask = FIRSTMASK;
			tptr++;
		} else
			NEXTMASK(tmask);
	}

	DEBUG((DBG_BITMAP_OPS, "flip_rotate_counter_clockwise (%d,%d) -> (%d,%d)\n",
		bm->width, bm->height, nb.width, nb.height));
	mdvi_free(bm->data);
	bm->data = nb.data;
	bm->width = nb.width;
	bm->height = nb.height;
	bm->stride = nb.stride;
	if(SHOW_OP_DATA)
		bitmap_print(stderr, bm);
}

#if 0
void	bitmap_transform(BITMAP *map, DviOrientation orient)
{
	switch(orient) {
		case MDVI_ORIENT_TBLR:
			break;
		case MDVI_ORIENT_TBRL:
			bitmap_flip_horizontally(map);
			break;
		case MDVI_ORIENT_BTLR:
			bitmap_flip_vertically(map);
			break;
		case MDVI_ORIENT_BTRL:
			bitmap_flip_diagonally(map);
			break;
		case MDVI_ORIENT_RP90:
			bitmap_rotate_counter_clockwise(map);
			break;
		case MDVI_ORIENT_RM90:
			bitmap_rotate_clockwise(map);
			break;
		case MDVI_ORIENT_IRP90:
			bitmap_flip_rotate_counter_clockwise(map);
			break;
		case MDVI_ORIENT_IRM90:
			bitmap_flip_rotate_clockwise(map);
			break;
	}
}
#endif

/*
 * Count the number of non-zero bits in a box of dimensions w x h, starting
 * at column `step' in row `data'.
 *
 * Shamelessly stolen from xdvi.
 */
static int do_sample(BmUnit *data, int stride, int step, int w, int h)
{
	BmUnit	*ptr, *end, *cp;
	int	shift, n;
	int	bits_left;
	int	wid;

	ptr = data + step / BITMAP_BITS;
	end = bm_offset(data, h * stride);
	shift = FIRSTSHIFTAT(step);
	bits_left = w;
	n = 0;
	while(bits_left) {
#ifndef WORD_BIG_ENDIAN
		wid = BITMAP_BITS - shift;
#else
		wid = shift;
#endif
		if(wid > bits_left)
			wid = bits_left;
		if(wid > 8)
			wid = 8;
#ifdef WORD_BIG_ENDIAN
		shift -= wid;
#endif
		for(cp = ptr; cp < end; cp = bm_offset(cp, stride))
			n += sample_count[(*cp >> shift) & bit_masks[wid]];
#ifndef WORD_BIG_ENDIAN
		shift += wid;
#endif
#ifdef WORD_BIG_ENDIAN
		if(shift == 0) {
			shift = BITMAP_BITS;
			ptr++;
		}
#else
		if(shift == BITMAP_BITS) {
			shift = 0;
			ptr++;
		}
#endif
		bits_left -= wid;
	}
	return n;
}

void	mdvi_shrink_box(DviContext *dvi, DviFont *font,
	DviFontChar *pk, DviGlyph *dest)
{
	int	x, y, z;
	DviGlyph *glyph;
	int	hs, vs;

	hs = dvi->params.hshrink;
	vs = dvi->params.vshrink;
	glyph = &pk->glyph;

	x = (int)glyph->x / hs;
	if((int)glyph->x - x * hs > 0)
		x++;
	dest->w = x + ROUND((int)glyph->w - glyph->x, hs);

	z = (int)glyph->y + 1;
	y = z / vs;
	if(z - y * vs <= 0)
		y--;
	dest->h = y + ROUND((int)glyph->h - z, vs) + 1;
	dest->x = x;
	dest->y = glyph->y / vs;
	dest->data = MDVI_GLYPH_EMPTY;
	DEBUG((DBG_BITMAPS, "shrink_box: (%dw,%dh,%dx,%dy) -> (%dw,%dh,%dx,%dy)\n",
		glyph->w, glyph->h, glyph->x, glyph->y,
		dest->w, dest->h, dest->x, dest->y));
}

void	mdvi_shrink_glyph(DviContext *dvi, DviFont *font,
	DviFontChar *pk, DviGlyph *dest)
{
	int	rows_left, rows, init_cols;
	int	cols_left, cols;
	BmUnit	*old_ptr, *new_ptr;
	BITMAP	*oldmap, *newmap;
	BmUnit	m, *cp;
	DviGlyph *glyph;
	int	sample, min_sample;
	int	old_stride;
	int	new_stride;
	int	x, y;
	int	w, h;
	int	hs, vs;

	hs = dvi->params.hshrink;
	vs = dvi->params.vshrink;

	min_sample = vs * hs * dvi->params.density / 100;

	glyph = &pk->glyph;
	oldmap = (BITMAP *)glyph->data;

	x = (int)glyph->x / hs;
	init_cols = (int)glyph->x - x * hs;
	if(init_cols <= 0)
		init_cols += hs;
	else
		x++;
	w = x + ROUND((int)glyph->w - glyph->x, hs);

	cols = (int)glyph->y + 1;
	y = cols / vs;
	rows = cols - y * vs;
	if(rows <= 0) {
		rows += vs;
		y--;
	}
	h = y + ROUND((int)glyph->h - cols, vs) + 1;

	/* create the new glyph */
	newmap = bitmap_alloc(w, h);
	dest->data = newmap;
	dest->x = x;
	dest->y = glyph->y / vs;
	dest->w = w;
	dest->h = h;

	old_ptr = oldmap->data;
	old_stride = oldmap->stride;
	new_ptr = newmap->data;
	new_stride = newmap->stride;
	rows_left = glyph->h;

	while(rows_left) {
		if(rows > rows_left)
			rows = rows_left;
		cols_left = glyph->w;
		m = FIRSTMASK;
		cp = new_ptr;
		cols = init_cols;
		while(cols_left > 0) {
			if(cols > cols_left)
				cols = cols_left;
			sample = do_sample(old_ptr, old_stride,
				glyph->w - cols_left, cols, rows);
			if(sample >= min_sample)
				*cp |= m;
			if(m == LASTMASK) {
				m = FIRSTMASK;
				cp++;
			} else
				NEXTMASK(m);
			cols_left -= cols;
			cols = hs;
		}
		new_ptr = bm_offset(new_ptr, new_stride);
		old_ptr = bm_offset(old_ptr, rows * old_stride);
		rows_left -= rows;
		rows = vs;
	}
	DEBUG((DBG_BITMAPS, "shrink_glyph: (%dw,%dh,%dx,%dy) -> (%dw,%dh,%dx,%dy)\n",
		glyph->w, glyph->h, glyph->x, glyph->y,
		dest->w, dest->h, dest->x, dest->y));
	if(DEBUGGING(BITMAP_DATA))
		bitmap_print(stderr, newmap);
}

void	mdvi_shrink_glyph_grey(DviContext *dvi, DviFont *font,
	DviFontChar *pk, DviGlyph *dest)
{
	int	rows_left, rows;
	int	cols_left, cols, init_cols;
	long	sampleval, samplemax;
	BmUnit	*old_ptr;
	void	*image;
	int	w, h;
	int	x, y;
	DviGlyph *glyph;
	BITMAP 	*map;
	Ulong	*pixels;
	int	npixels;
	Ulong	colortab[2];
	int	hs, vs;
	DviDevice *dev;

	hs = dvi->params.hshrink;
	vs = dvi->params.vshrink;
	dev = &dvi->device;

	glyph = &pk->glyph;
	map = (BITMAP *)glyph->data;

	x = (int)glyph->x / hs;
	init_cols = (int)glyph->x - x * hs;
	if(init_cols <= 0)
		init_cols += hs;
	else
		x++;
	w = x + ROUND((int)glyph->w - glyph->x, hs);

	cols = (int)glyph->y + 1;
	y = cols / vs;
	rows = cols - y * vs;
	if(rows <= 0) {
		rows += vs;
		y--;
	}
	h = y + ROUND((int)glyph->h - cols, vs) + 1;
	ASSERT(w && h);

	/* before touching anything, do this */
	image = dev->create_image(dev->device_data, w, h, BITMAP_BITS);
	if(image == NULL) {
		mdvi_shrink_glyph(dvi, font, pk, dest);
		return;
	}

	/* save these colors */
	pk->fg = MDVI_CURRFG(dvi);
	pk->bg = MDVI_CURRBG(dvi);

	samplemax = vs * hs;
	npixels = samplemax + 1;
	pixels = get_color_table(&dvi->device, npixels, pk->fg, pk->bg,
			dvi->params.gamma, dvi->params.density);
	if(pixels == NULL) {
		npixels = 2;
		colortab[0] = pk->fg;
		colortab[1] = pk->bg;
		pixels = &colortab[0];
	}

	/* setup the new glyph */
	dest->data = image;
	dest->x = x;
	dest->y = glyph->y / vs;
	dest->w = w;
	dest->h = h;

	y = 0;
	old_ptr = map->data;
	rows_left = glyph->h;

	while(rows_left && y < h) {
		x = 0;
		if(rows > rows_left)
			rows = rows_left;
		cols_left = glyph->w;
		cols = init_cols;
		while(cols_left && x < w) {
			if(cols > cols_left)
				cols = cols_left;
			sampleval = do_sample(old_ptr, map->stride,
				glyph->w - cols_left, cols, rows);
			/* scale the sample value by the number of grey levels */
			if(npixels - 1 != samplemax)
				sampleval = ((npixels-1) * sampleval) / samplemax;
			ASSERT(sampleval < npixels);
			dev->put_pixel(image, x, y, pixels[sampleval]);
			cols_left -= cols;
			cols = hs;
			x++;
		}
		for(; x < w; x++)
			dev->put_pixel(image, x, y, pixels[0]);
		old_ptr = bm_offset(old_ptr, rows * map->stride);
		rows_left -= rows;
		rows = vs;
		y++;
	}

	for(; y < h; y++) {
		for(x = 0; x < w; x++)
			dev->put_pixel(image, x, y, pixels[0]);
	}

        dev->image_done(image);
	DEBUG((DBG_BITMAPS, "shrink_glyph_grey: (%dw,%dh,%dx,%dy) -> (%dw,%dh,%dx,%dy)\n",
		glyph->w, glyph->h, glyph->x, glyph->y,
		dest->w, dest->h, dest->x, dest->y));
}

