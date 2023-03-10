/* tfmfile.c -- readers for TFM, AFM, OTFM-0 and OTFM-1 files */
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
#include <stdio.h> /* tex-file.h needs this */
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mdvi.h"
#include "private.h"

#ifdef WITH_AFM_FILES
#undef TRUE
#undef FALSE
#include "afmparse.h"
#endif

typedef struct tfmpool {
	struct tfmpool *next;
	struct tfmpool *prev;
	char	*short_name;
	int	links;
	TFMInfo	tfminfo;
} TFMPool;

static ListHead	tfmpool = {NULL, NULL, 0};
static DviHashTable tfmhash;

#define TFM_HASH_SIZE	31

#ifdef WORD_LITTLE_ENDIAN
static inline void swap_array(Uint32 *ptr, int n)
{
	Uint32 i;

	while(n-- > 0) {
		i = *ptr;
		*ptr++ = ((i & 0xff000000) >> 24)
		       | ((i & 0x00ff0000) >> 8)
		       | ((i & 0x0000ff00) << 8)
		       | ((i & 0x000000ff) << 24);
	}
}
#endif

#ifdef WITH_AFM_FILES

static int	__PROTO(ofm_load_file(const char *filename, TFMInfo *info));

/* reading of AFM files */
/* macro to convert between AFM and TFM units */
#define AFM2TFM(x)	FROUND((double)(x) * 0x100000 / 1000)
int	afm_load_file(const char *filename, TFMInfo *info)
{
	/* the information we want is:
	 *   - tfmwidth
	 *   - width and heights
	 *   - character origins
	 */
	FontInfo *fi = NULL;
	int	status;
	CharMetricInfo *cm;
	FILE	*in;

	in = fopen(filename, "rb");
	if(in == NULL)
		return -1;
	status = afm_parse_file(in, &fi, P_GM);
	fclose(in);

	if(status != ok) {
		mdvi_error(_("%s: Error reading AFM data\n"), filename);
		return -1;
	}

	/* aim high */
	info->chars = xnalloc(TFMChar, 256);
	info->loc = 256;
	info->hic = 0;
	info->design = 0xa00000; /* fake -- 10pt */
	info->checksum = 0; /* no checksum */
	info->type = DviFontAFM;
	mdvi_strncpy(info->coding, fi->gfi->encodingScheme, 63);
	mdvi_strncpy(info->family, fi->gfi->familyName, 63);

	/* now get the data */
	for(cm = fi->cmi; cm < fi->cmi + fi->numOfChars; cm++) {
		int	code;
		TFMChar	*ch;

		code = cm->code;
		if(code < 0 || code > 255)
			continue; /* ignore it */
		ch = &info->chars[code];
		ch->present = 1;
		if(code < info->loc)
			info->loc = code;
		if(code > info->hic)
			info->hic = code;
		ch->advance = AFM2TFM(cm->wx);
		/* this is the `leftSideBearing' */
		ch->left   = AFM2TFM(cm->charBBox.llx);
		/* this is the height (ascent - descent) -- the sign is to follow
		 * TeX conventions, as opposed to Adobe's ones */
		ch->depth  = -AFM2TFM(cm->charBBox.lly);
		/* this is the width (rightSideBearing - leftSideBearing) */
		ch->right  = AFM2TFM(cm->charBBox.urx);
		/* this is the `ascent' */
		ch->height = AFM2TFM(cm->charBBox.ury);
	}

	/* we don't need this anymore */
	afm_free_fontinfo(fi);

	/* optimize storage */
	if(info->loc > 0 || info->hic < 256) {
		memmove(&info->chars[0],
			&info->chars[info->loc],
			(info->hic - info->loc + 1) * sizeof(TFMChar));
		info->chars = mdvi_realloc(info->chars,
			(info->hic - info->loc + 1) * sizeof(TFMChar));
	}

	/* we're done */
	return 0;
}

#endif /* WITH_AFM_FILES */

int	tfm_load_file(const char *filename, TFMInfo *info)
{
	int	lf, lh, bc, ec, nw, nh, nd, ne;
	int	i, n;
	Uchar	*tfm;
	Uchar	*ptr;
	struct stat st;
	int	size;
	FILE	*in;
	Int32	*cb;
	Int32	*charinfo;
	Int32	*widths;
	Int32	*heights;
	Int32	*depths;
	Uint32	checksum;

	in = fopen(filename, "rb");
	if(in == NULL)
		return -1;
	tfm = NULL;

	DEBUG((DBG_FONTS, "(mt) reading TFM file `%s'\n",
		filename));
	/* We read the entire TFM file into core */
	if(fstat(fileno(in), &st) < 0)
		return -1;
	/* according to the spec, TFM files are smaller than 16K */
	if(st.st_size == 0 || st.st_size >= 16384)
		goto bad_tfm;

	/* allocate a word-aligned buffer to hold the file */
	size = 4 * ROUND(st.st_size, 4);
	if(size != st.st_size)
		mdvi_warning(_("Warning: TFM file `%s' has suspicious size\n"),
			     filename);
	tfm = (Uchar *)mdvi_malloc(size);
	if(fread(tfm, st.st_size, 1, in) != 1)
		goto error;
	/* we don't need this anymore */
	fclose(in);
	in = NULL;

	/* not a checksum, but serves a similar purpose */
	checksum = 0;

	ptr = tfm;
	/* get the counters */
	lf = muget2(ptr);
	lh = muget2(ptr); checksum += 6 + lh;
	bc = muget2(ptr);
	ec = muget2(ptr); checksum += ec - bc + 1;
	nw = muget2(ptr); checksum += nw;
	nh = muget2(ptr); checksum += nh;
	nd = muget2(ptr); checksum += nd;
	checksum += muget2(ptr); /* skip italics correction count */
	checksum += muget2(ptr); /* skip lig/kern table size */
	checksum += muget2(ptr); /* skip kern table size */
	ne = muget2(ptr); checksum += ne;
	checksum += muget2(ptr); /* skip # of font parameters */

	size = ec - bc + 1;
	cb = (Int32 *)tfm; cb += 6 + lh;
	charinfo    = cb;  cb += size;
	widths      = cb;  cb += nw;
	heights     = cb;  cb += nh;
	depths      = cb;

	if(widths[0] || heights[0] || depths[0] ||
	   checksum != lf || bc - 1 > ec || ec > 255 || ne > 256)
		goto bad_tfm;

	/* from this point on, no error checking is done */

	/* now we're at the header */
	/* get the checksum */
	info->checksum = muget4(ptr);
	/* get the design size */
	info->design = muget4(ptr);
	/* get the coding scheme */
	if(lh > 2) {
		/* get the coding scheme */
		i = n = msget1(ptr);
		if(n < 0 || n > 39) {
			mdvi_warning(_("%s: font coding scheme truncated to 40 bytes\n"),
				     filename);
			n = 39;
		}
		memcpy(info->coding, ptr, n);
		info->coding[n] = 0;
		ptr += i;
	} else
		strcpy(info->coding, "FontSpecific");
	/* get the font family */
	if(lh > 12) {
		n = msget1(ptr);
		if(n > 0) {
			i = Max(n, 63);
			memcpy(info->family, ptr, i);
			info->family[i] = 0;
		} else
			strcpy(info->family, "unspecified");
		ptr += n;
	}
	/* now we don't read from `ptr' anymore */

	info->loc = bc;
	info->hic = ec;
	info->type = DviFontTFM;

	/* allocate characters */
	info->chars = xnalloc(TFMChar, size);


#ifdef WORD_LITTLE_ENDIAN
	/* byte-swap the three arrays at once (they are consecutive in memory) */
	swap_array((Uint32 *)widths, nw + nh + nd);
#endif

	/* get the relevant data */
	ptr = (Uchar *)charinfo;
	for(i = bc; i <= ec; ptr += 3, i++) {
		int	ndx;

		ndx = (int)*ptr; ptr++;
		info->chars[i-bc].advance = widths[ndx];
		/* TFM files lack this information */
		info->chars[i-bc].left = 0;
		info->chars[i-bc].right = widths[ndx];
		info->chars[i-bc].present = (ndx != 0);
		if(ndx) {
			ndx = ((*ptr >> 4) & 0xf);
			info->chars[i-bc].height = heights[ndx];
			ndx = (*ptr & 0xf);
			info->chars[i-bc].depth = depths[ndx];
		}
	}

	/* free everything */
	mdvi_free(tfm);

	return 0;

bad_tfm:
	mdvi_error(_("%s: File corrupted, or not a TFM file\n"), filename);
error:
	if(tfm) mdvi_free(tfm);
	if(in)  fclose(in);
	return -1;
}

static int ofm1_load_file(FILE *in, TFMInfo *info)
{
	int	lh, bc, ec, nw, nh, nd;
	int	nco, ncw, npc;
	int	i;
	int	n;
	int	size;
	Int32	*tfm;
	Int32	*widths;
	Int32	*heights;
	Int32	*depths;
	TFMChar	*tch;
	TFMChar	*end;

	lh = fuget4(in);
	bc = fuget4(in);
	ec = fuget4(in);
	nw = fuget4(in);
	nh = fuget4(in);
	nd = fuget4(in);
	fuget4(in); /* italics */
	fuget4(in); /* lig-kern */
	fuget4(in); /* kern */
	fuget4(in); /* extensible recipe */
	fuget4(in); /* parameters */
	fuget4(in); /* direction */
	nco = fuget4(in);
	ncw = fuget4(in);
	npc = fuget4(in);

	/* get the checksum */
	info->checksum = fuget4(in);
	/* the design size */
	info->design = fuget4(in);
	/* get the coding scheme */
	if(lh > 2) {
		/* get the coding scheme */
		i = n = fsget1(in);
		if(n < 0 || n > 39)
			n = 39;
		fread(info->coding, 39, 1, in);
		info->coding[n] = 0;
	} else
		strcpy(info->coding, "FontSpecific");
	/* get the font family */
	if(lh > 12) {
		n = fsget1(in);
		if(n > 0) {
			i = Max(n, 63);
			fread(info->family, i, 1, in);
			info->family[i] = 0;
		} else
			strcpy(info->family, "unspecified");
	}
	tfm = NULL;

	/* jump to the beginning of the char-info table */
	fseek(in, 4L*nco, SEEK_SET);

	size = ec - bc + 1;
	info->loc = bc;
	info->hic = ec;
	info->chars = xnalloc(TFMChar, size);
	end = info->chars + size;

	for(tch = info->chars, i = 0; i < ncw; i++) {
		TFMChar	ch;
		int	nr;

		/* in the characters we store the actual indices */
		ch.advance = fuget2(in);
		ch.height  = fuget1(in);
		ch.depth   = fuget1(in);
		/* skip 2nd word */
		fuget4(in);
		/* get # of repeats */
		nr = fuget2(in);
		/* skip parameters */
		fseek(in, (long)npc * 2, SEEK_CUR);
		/* if npc is odd, skip padding */
		if(npc & 1) fuget2(in);

		/* now repeat the character */
		while(nr-- >= 0 && tch < end)
			memcpy(tch++, &ch, sizeof(TFMChar));
		if(tch == end)
			goto bad_tfm;
	}

	/* I wish we were done, but we aren't */

	/* get the widths, heights and depths */
	size = nw + nh + nd;
	tfm = xnalloc(Int32, size);
	/* read them in one sweep */
	if(fread(tfm, 4, size, in) != size) {
		mdvi_free(tfm);
		goto bad_tfm;
	}

	/* byte-swap things if necessary */
#ifdef WORD_LITTLE_ENDIAN
	swap_array((Uint32 *)tfm, size);
#endif
	widths  = tfm;
	heights = widths + nw;
	depths  = heights + nh;

	if(widths[0] || heights[0] || depths[0])
		goto bad_tfm;

	/* now fix the characters */
	size = ec - bc + 1;
	for(tch = info->chars; tch < end; tch++) {
		tch->present = (tch->advance != 0);
		tch->advance = widths[tch->advance];
		tch->height  = heights[tch->height];
		tch->depth   = depths[tch->depth];
		tch->left    = 0;
		tch->right   = tch->advance;
	}

	/* NOW we're done */
	mdvi_free(tfm);
	return 0;

bad_tfm:
	if(tfm) mdvi_free(tfm);
	return -1;
}

/* we don't read OFM files into memory, because they can potentially be large */
static int	ofm_load_file(const char *filename, TFMInfo *info)
{
	int	lf, lh, bc, ec, nw, nh, nd;
	int	i, n;
	Int32	*tfm;
	Uchar	*ptr;
	int	size;
	FILE	*in;
	Int32	*cb;
	Int32	*charinfo;
	Int32	*widths;
	Int32	*heights;
	Int32	*depths;
	Uint32	checksum;
	int	olevel;
	int	nwords;

	in = fopen(filename, "rb");
	if(in == NULL)
		return -1;

	/* not a checksum, but serves a similar purpose */
	checksum = 0;

	/* get the counters */
	/* get file level */
	olevel = fsget2(in);
	if(olevel != 0)
		goto bad_tfm;
	olevel = fsget2(in);
	if(olevel != 0) {
		DEBUG((DBG_FONTS, "(mt) reading Level-1 OFM file `%s'\n",
			filename));
		/* we handle level-1 files separately */
		if(ofm1_load_file(in, info) < 0)
			goto bad_tfm;
		return 0;
	}

	DEBUG((DBG_FONTS, "(mt) reading Level-0 OFM file `%s'\n", filename));
	nwords = 14;
	lf = fuget4(in); checksum  = nwords;
	lh = fuget4(in); checksum += lh;
	bc = fuget4(in);
	ec = fuget4(in); checksum += 2 * (ec - bc + 1);
	nw = fuget4(in); checksum += nw;
	nh = fuget4(in); checksum += nh;
	nd = fuget4(in); checksum += nd;
	checksum +=   fuget4(in); /* skip italics correction count */
	checksum += 2*fuget4(in); /* skip lig/kern table size */
	checksum +=   fuget4(in); /* skip kern table size */
	checksum += 2*fuget4(in); /* skip extensible recipe count */
	checksum +=   fuget4(in); /* skip # of font parameters */

	/* I have found several .ofm files that seem to have the
	 * font-direction word missing, so we try to detect that here */
	if(checksum == lf + 1) {
		DEBUG((DBG_FONTS, "(mt) font direction missing in `%s'\n",
			filename));
		checksum--;
		nwords--;
	} else {
		/* skip font direction */
		fuget4(in);
	}

	if(checksum != lf || bc > ec + 1 || ec > 65535)
		goto bad_tfm;

	/* now we're at the header */

	/* get the checksum */
	info->checksum = fuget4(in);
	/* get the design size */
	info->design = fuget4(in);

	/* get the coding scheme */
	if(lh > 2) {
		/* get the coding scheme */
		i = n = fsget1(in);
		if(n < 0 || n > 39) {
			mdvi_warning(_("%s: font coding scheme truncated to 40 bytes\n"),
				     filename);
			n = 39;
		}
		fread(info->coding, 39, 1, in);
		info->coding[n] = 0;
	} else
		strcpy(info->coding, "FontSpecific");
	/* get the font family */
	if(lh > 12) {
		n = fsget1(in);
		if(n > 0) {
			i = Max(n, 63);
			fread(info->family, i, 1, in);
			info->family[i] = 0;
		} else
			strcpy(info->family, "unspecified");
	}

	/* now skip anything else in the header */
	fseek(in, 4L*(nwords + lh), SEEK_SET);
	/* and read everything at once */
	size = 2*(ec - bc + 1) + nw + nh + nd;
	tfm = xnalloc(Int32, size * sizeof(Int32));
	if(fread(tfm, 4, size, in) != size) {
		mdvi_free(tfm);
		goto bad_tfm;
	}
	/* byte-swap all the tables at once */
#ifdef WORD_LITTLE_ENDIAN
	swap_array((Uint32 *)tfm, size);
#endif
	cb = tfm;
	charinfo = cb; cb += 2*(ec - bc + 1);
	widths   = cb; cb += nw;
	heights  = cb; cb += nh;
	depths   = cb;

	if(widths[0] || heights[0] || depths[0]) {
	   	mdvi_free(tfm);
		goto bad_tfm;
	}

	/* from this point on, no error checking is done */

	/* we don't need this anymore */
	fclose(in);

	/* now we don't read from `ptr' anymore */

	info->loc = bc;
	info->hic = ec;
	info->type = DviFontTFM;

	/* allocate characters */
	info->chars = xnalloc(TFMChar, size);

	/* get the relevant data */
	ptr = (Uchar *)charinfo;
	for(i = bc; i <= ec; ptr += 4, i++) {
		int	ndx;

		ndx = muget2(ptr);
		info->chars[i-bc].advance = widths[ndx];
		/* TFM files lack this information */
		info->chars[i-bc].left = 0;
		info->chars[i-bc].right = widths[ndx];
		info->chars[i-bc].present = (ndx != 0);
		ndx = muget1(ptr);
		info->chars[i-bc].height = heights[ndx];
		ndx = muget1(ptr);
		info->chars[i-bc].depth = depths[ndx];
	}

	mdvi_free(tfm);
	return 0;

bad_tfm:
	mdvi_error(_("%s: File corrupted, or not a TFM file\n"), filename);
	fclose(in);
	return -1;
}

char	*lookup_font_metrics(const char *name, int *type)
{
	char	*file;

	switch(*type) {
#ifndef WITH_AFM_FILES
		case DviFontAny:
#endif
		case DviFontTFM:
			file = kpse_find_tfm(name);
                        *type = DviFontTFM;
			break;
		case DviFontOFM: {
			file = kpse_find_ofm(name);
			/* we may have gotten a TFM back */
			if(file != NULL) {
				const char *ext = file_extension(file);
				if(ext && STREQ(ext, "tfm"))
					*type = DviFontTFM;
			}
			break;
		}
#ifdef WITH_AFM_FILES
		case DviFontAFM:
			file = kpse_find_file(name, kpse_afm_format, 0);
			break;
		case DviFontAny:
			file = kpse_find_file(name, kpse_afm_format, 0);
			*type = DviFontAFM;
			if(file == NULL) {
				file = kpse_find_tfm(name);
				*type = DviFontTFM;
			}
			break;
#endif
		default:
			return NULL;
	}

	return file;
}

/*
 * The next two functions are just wrappers for the font metric loaders,
 * and use the pool of TFM data
 */

/* this is how we interpret arguments:
 *  - if filename is NULL, we look for files of the given type,
 *    unless type is DviFontAny, in which case we try all the
 *    types we know of.
 *  - if filename is not NULL, we look at `type' to decide
 *    how to read the file. If type is DviFontAny, we just
 *    return an error.
 */
TFMInfo	*get_font_metrics(const char *short_name, int type, const char *filename)
{
	TFMPool *tfm = NULL;
	int	status;
	char	*file;

	if(tfmpool.count) {
		tfm = (TFMPool *)mdvi_hash_lookup(&tfmhash,
			MDVI_KEY(short_name));
		if(tfm != NULL) {
			DEBUG((DBG_FONTS, "(mt) reusing metric file `%s' (%d links)\n",
				short_name, tfm->links));
			tfm->links++;
			return &tfm->tfminfo;
		}
	}

	file = filename ? (char *)filename : lookup_font_metrics(short_name, &type);
	if(file == NULL)
		return NULL;

	tfm = xalloc(TFMPool);
	DEBUG((DBG_FONTS, "(mt) loading font metric data from `%s'\n", file, file));
	switch(type) {
	case DviFontTFM:
		status = tfm_load_file(file, &tfm->tfminfo);
		break;
	case DviFontOFM:
		status = ofm_load_file(file, &tfm->tfminfo);
		break;
#ifdef WITH_AFM_FILES
	case DviFontAFM:
		status = afm_load_file(file, &tfm->tfminfo);
		break;
#endif
	default:
		status = -1;
		break;
	}
	if(file != filename)
		mdvi_free(file);
	if(status < 0) {
		mdvi_free(tfm);
		return NULL;
	}
	tfm->short_name = mdvi_strdup(short_name);

	/* add it to the pool */
	if(tfmpool.count == 0)
		mdvi_hash_create(&tfmhash, TFM_HASH_SIZE);
	mdvi_hash_add(&tfmhash, MDVI_KEY(tfm->short_name),
		tfm, MDVI_HASH_UNCHECKED);
	listh_prepend(&tfmpool, LIST(tfm));
	tfm->links = 1;

	return &tfm->tfminfo;
}

void	free_font_metrics(TFMInfo *info)
{
	TFMPool *tfm;

	if(tfmpool.count == 0)
		return;
	/* get the entry -- can't use the hash table for this, because
	 * we don't have the short name */
	for(tfm = (TFMPool *)tfmpool.head; tfm; tfm = tfm->next)
		if(info == &tfm->tfminfo)
			break;
	if(tfm == NULL)
		return;
	if(--tfm->links > 0) {
		DEBUG((DBG_FONTS, "(mt) %s not removed, still in use\n",
			tfm->short_name));
		return;
	}
	mdvi_hash_remove_ptr(&tfmhash, MDVI_KEY(tfm->short_name));

	DEBUG((DBG_FONTS, "(mt) removing unused TFM data for `%s'\n", tfm->short_name));
	listh_remove(&tfmpool, LIST(tfm));
	mdvi_free(tfm->short_name);
	mdvi_free(tfm->tfminfo.chars);
	mdvi_free(tfm);
}

void	flush_font_metrics(void)
{
	TFMPool	*ptr;

	for(; (ptr = (TFMPool *)tfmpool.head); ) {
		tfmpool.head = LIST(ptr->next);

		mdvi_free(ptr->short_name);
		mdvi_free(ptr->tfminfo.chars);
		mdvi_free(ptr);
	}
	mdvi_hash_reset(&tfmhash, 0);
}
