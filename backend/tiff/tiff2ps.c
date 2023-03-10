/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-indent-level: 8 -*- */
/* $Id$ */

/*
 * Copyright (c) 1988-1997 Sam Leffler
 * Copyright (c) 1991-1997 Silicon Graphics, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * Sam Leffler and Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Sam Leffler and Silicon Graphics.
 *
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
 * ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

/*
 * Modified for use as Evince TIFF ps exporter by
 * Matthew S. Wilson <msw@rpath.com>
 * Modifications Copyright (C) 2005 rpath, Inc.
 *
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>			/* for atof */
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "tiff2ps.h"

/*
 * Revision history
 *
 * 2001-Mar-21
 *    I (Bruce A. Mallett) added this revision history comment ;)
 *
 *    Fixed PS_Lvl2page() code which outputs non-ASCII85 raw
 *    data.  Moved test for when to output a line break to
 *    *after* the output of a character.  This just serves
 *    to fix an eye-nuisance where the first line of raw
 *    data was one character shorter than subsequent lines.
 *
 *    Added an experimental ASCII85 encoder which can be used
 *    only when there is a single buffer of bytes to be encoded.
 *    This version is much faster at encoding a straight-line
 *    buffer of data because it can avoid alot of the loop
 *    overhead of the byte-by-bye version.  To use this version
 *    you need to define EXP_ASCII85ENCODER (experimental ...).
 *
 *    Added bug fix given by Michael Schmidt to PS_Lvl2page()
 *    in which an end-of-data marker ('>') was not being output
 *    when producing non-ASCII85 encoded PostScript Level 2
 *    data.
 *
 *    Fixed PS_Lvl2colorspace() so that it no longer assumes that
 *    a TIFF having more than 2 planes is a CMYK.  This routine
 *    no longer looks at the samples per pixel but instead looks
 *    at the "photometric" value.  This change allows support of
 *    CMYK TIFFs.
 *
 *    Modified the PostScript L2 imaging loop so as to test if
 *    the input stream is still open before attempting to do a
 *    flushfile on it.  This was done because some RIPs close
 *    the stream after doing the image operation.
 *
 *    Got rid of the realloc() being done inside a loop in the
 *    PSRawDataBW() routine.  The code now walks through the
 *    byte-size array outside the loop to determine the largest
 *    size memory block that will be needed.
 *
 *    Added "-m" switch to ask tiff2ps to, where possible, use the
 *    "imagemask" operator instead of the "image" operator.
 *
 *    Added the "-i #" switch to allow interpolation to be disabled.
 *
 *    Unrolled a loop or two to improve performance.
 */

/*
 * Define EXP_ASCII85ENCODER if you want to use an experimental
 * version of the ASCII85 encoding routine.  The advantage of
 * using this routine is that tiff2ps will convert to ASCII85
 * encoding at between 3 and 4 times the speed as compared to
 * using the old (non-experimental) encoder.  The disadvantage
 * is that you will be using a new (and unproven) encoding
 * routine.  So user beware, you have been warned!
 */

#define	EXP_ASCII85ENCODER

/*
 * NB: this code assumes uint32_t works with printf's %l[ud].
 */

struct _TIFF2PSContext
{
	char *filename;		/* input filename */
	FILE *fd;		/* output file stream */
	int ascii85;		/* use ASCII85 encoding */
	int interpolate;	/* interpolate level2 image */
	int level2;		/* generate PostScript level 2 */
	int level3;		/* generate PostScript level 3 */
	int generateEPSF;	/* generate Encapsulated PostScript */
	int PSduplex;		/* enable duplex printing */
	int PStumble;		/* enable top edge binding */
	int PSavoiddeadzone;	/* enable avoiding printer deadzone */
	double maxPageHeight;	/* maximum size to fit on page */
	double splitOverlap;	/* amount for split pages to overlag */
	int rotate;		/* rotate image by 180 degrees */
	int useImagemask;	/* Use imagemask instead of image operator */
	uint16_t res_unit;	/* Resolution units: 2 - inches, 3 - cm */
	int npages;		/* number of pages processed */

	tsize_t tf_bytesperrow;
	tsize_t ps_bytesperrow;
	tsize_t	tf_rowsperstrip;
	tsize_t	tf_numberstrips;

	/*
	 * ASCII85 Encoding Support.
	 */
	unsigned char ascii85buf[10];
	int ascii85count;
	int ascii85breaklen;
	uint16_t samplesperpixel;
	uint16_t bitspersample;
	uint16_t planarconfiguration;
	uint16_t photometric;
	uint16_t compression;
	uint16_t extrasamples;
	int alpha;
};

static void PSpage(TIFF2PSContext*, TIFF*, uint32_t, uint32_t);
static void PSColorContigPreamble(TIFF2PSContext*, uint32_t, uint32_t, int);
static void PSColorSeparatePreamble(TIFF2PSContext*, uint32_t, uint32_t, int);
static void PSDataColorContig(TIFF2PSContext*, TIFF*, uint32_t, uint32_t, int);
static void PSDataColorSeparate(TIFF2PSContext*, TIFF*, uint32_t, uint32_t, int);
static void PSDataPalette(TIFF2PSContext*, TIFF*, uint32_t, uint32_t);
static void PSDataBW(TIFF2PSContext*, TIFF*, uint32_t, uint32_t);
static void Ascii85Init(TIFF2PSContext*);
static void Ascii85Put(TIFF2PSContext*, unsigned char);
static void Ascii85Flush(TIFF2PSContext*);
static void PSHead(TIFF2PSContext*, TIFF*, uint32_t, uint32_t,
		   double, double, double, double);
static void PSTail(TIFF2PSContext*);

#if defined( EXP_ASCII85ENCODER )
static int Ascii85EncodeBlock(TIFF2PSContext*, uint8_t * ascii85_p,
			      unsigned f_eod, const uint8_t * raw_p, int raw_l);
#endif

#define IMAGEOP(ctx) ((ctx)->useImagemask && ((ctx)->bitspersample == 1)) ? "imagemask" : "image"

TIFF2PSContext* tiff2ps_context_new(const gchar *filename) {
	TIFF2PSContext* ctx;

	ctx = g_new0(TIFF2PSContext, 1);
	ctx->filename = g_strdup(filename);
	ctx->fd = g_fopen(ctx->filename, "w");
	if (ctx->fd == NULL) {
		g_free (ctx->filename);
		g_free (ctx);
		return NULL;
	}
	ctx->interpolate = TRUE;     /* interpolate level2 image */
	ctx->PSavoiddeadzone = TRUE; /* enable avoiding printer deadzone */
	return ctx;
}

void tiff2ps_context_finalize(TIFF2PSContext *ctx) {
	PSTail(ctx);
	fclose(ctx->fd);
	g_free(ctx->filename);
	g_free(ctx);
}

static int
checkImage(TIFF2PSContext *ctx, TIFF* tif)
{
	switch (ctx->photometric) {
	case PHOTOMETRIC_YCBCR:
		if ((ctx->compression == COMPRESSION_JPEG
		     || ctx->compression == COMPRESSION_OJPEG)
		    && ctx->planarconfiguration == PLANARCONFIG_CONTIG) {
			/* can rely on libjpeg to convert to RGB */
			TIFFSetField(tif, TIFFTAG_JPEGCOLORMODE,
				     JPEGCOLORMODE_RGB);
			ctx->photometric = PHOTOMETRIC_RGB;
		} else {
			if (ctx->level2 || ctx->level3)
				break;
			TIFFError(ctx->filename, "Can not handle image with %s",
			    "Ctx->PhotometricInterpretation=YCbCr");
			return (0);
		}
		/* fall thru... */
	case PHOTOMETRIC_RGB:
		if (ctx->alpha && ctx->bitspersample != 8) {
			TIFFError(ctx->filename,
			    "Can not handle %d-bit/sample RGB image with ctx->alpha",
			    ctx->bitspersample);
			return (0);
		}
		/* fall thru... */
	case PHOTOMETRIC_SEPARATED:
	case PHOTOMETRIC_PALETTE:
	case PHOTOMETRIC_MINISBLACK:
	case PHOTOMETRIC_MINISWHITE:
		break;
	case PHOTOMETRIC_LOGL:
	case PHOTOMETRIC_LOGLUV:
		if (ctx->compression != COMPRESSION_SGILOG &&
		    ctx->compression != COMPRESSION_SGILOG24) {
			TIFFError(ctx->filename,
		    "Can not handle %s data with ctx->compression other than SGILog",
			    (ctx->photometric == PHOTOMETRIC_LOGL) ?
				"LogL" : "LogLuv"
			);
			return (0);
		}
		/* rely on library to convert to RGB/greyscale */
		TIFFSetField(tif, TIFFTAG_SGILOGDATAFMT, SGILOGDATAFMT_8BIT);
		ctx->photometric = (ctx->photometric == PHOTOMETRIC_LOGL) ?
		    PHOTOMETRIC_MINISBLACK : PHOTOMETRIC_RGB;
		ctx->bitspersample = 8;
		break;
	case PHOTOMETRIC_CIELAB:
		/* fall thru... */
	default:
		TIFFError(ctx->filename,
		    "Can not handle image with Ctx->PhotometricInterpretation=%d",
		    ctx->photometric);
		return (0);
	}
	switch (ctx->bitspersample) {
	case 1: case 2:
	case 4: case 8:
		break;
	default:
		TIFFError(ctx->filename, "Can not handle %d-bit/sample image",
		    ctx->bitspersample);
		return (0);
	}
	if (ctx->planarconfiguration == PLANARCONFIG_SEPARATE &&
	    ctx->extrasamples > 0)
		TIFFWarning(ctx->filename, "Ignoring extra samples");
	return (1);
}

#define PS_UNIT_SIZE	72.0F
#define	PSUNITS(npix,res)	((npix) * (PS_UNIT_SIZE / (res)))

static	char RGBcolorimage[] = "\
/bwproc {\n\
    rgbproc\n\
    dup length 3 idiv string 0 3 0\n\
    5 -1 roll {\n\
	add 2 1 roll 1 sub dup 0 eq {\n\
	    pop 3 idiv\n\
	    3 -1 roll\n\
	    dup 4 -1 roll\n\
	    dup 3 1 roll\n\
	    5 -1 roll put\n\
	    1 add 3 0\n\
	} { 2 1 roll } ifelse\n\
    } forall\n\
    pop pop pop\n\
} def\n\
/colorimage where {pop} {\n\
    /colorimage {pop pop /rgbproc exch def {bwproc} image} bind def\n\
} ifelse\n\
";

/*
 * Adobe Photoshop requires a comment line of the form:
 *
 * %ImageData: <cols> <rows> <depth>  <main channels> <pad channels>
 *	<block size> <1 for binary|2 for hex> "data start"
 *
 * It is claimed to be part of some future revision of the EPS spec.
 */
G_GNUC_PRINTF (6, 7) static void
PhotoshopBanner(TIFF2PSContext* ctx, uint32_t w, uint32_t h, int bs, int nc,
		const char* startline, ...)
{
	va_list args;
	fprintf(ctx->fd, "%%ImageData: %ld %ld %d %d 0 %d 2 \"",
	    (long) w, (long) h, ctx->bitspersample, nc, bs);

	va_start(args, startline);
	vfprintf(ctx->fd, startline, args);
	va_end(args);

	fprintf(ctx->fd, "\"\n");
}

/*
 *   pw : image width in pixels
 *   ph : image height in pixels
 * pprw : image width in PS units (72 dpi)
 * pprh : image height in PS units (72 dpi)
 */
static void
setupPageState(TIFF2PSContext *ctx, TIFF* tif, uint32_t* pw, uint32_t* ph,
	       double* pprw, double* pprh)
{
	float xres = 0.0F, yres = 0.0F;

	TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, pw);
	TIFFGetField(tif, TIFFTAG_IMAGELENGTH, ph);
	if (ctx->res_unit == 0)
		TIFFGetFieldDefaulted(tif, TIFFTAG_RESOLUTIONUNIT, &ctx->res_unit);
	/*
	 * Calculate printable area.
	 */
	if (!TIFFGetField(tif, TIFFTAG_XRESOLUTION, &xres)
            || fabs(xres) < 0.0000001)
		xres = PS_UNIT_SIZE;
	if (!TIFFGetField(tif, TIFFTAG_YRESOLUTION, &yres)
            || fabs(yres) < 0.0000001)
		yres = PS_UNIT_SIZE;
	switch (ctx->res_unit) {
	case RESUNIT_CENTIMETER:
		xres *= 2.54F, yres *= 2.54F;
		break;
	case RESUNIT_INCH:
		break;
	case RESUNIT_NONE:
	default:
		xres *= PS_UNIT_SIZE, yres *= PS_UNIT_SIZE;
		break;
	}
	*pprh = PSUNITS(*ph, yres);
	*pprw = PSUNITS(*pw, xres);
}

static int
isCCITTCompression(TIFF* tif)
{
    uint16_t compress;
    TIFFGetField(tif, TIFFTAG_COMPRESSION, &compress);
    return (compress == COMPRESSION_CCITTFAX3 ||
	    compress == COMPRESSION_CCITTFAX4 ||
	    compress == COMPRESSION_CCITTRLE ||
	    compress == COMPRESSION_CCITTRLEW);
}

static	char *hex = "0123456789abcdef";

/*
 * imagewidth & imageheight are 1/72 inches
 * pagewidth & pageheight are inches
 */
static int
PlaceImage(TIFF2PSContext *ctx, double pagewidth, double pageheight,
	   double imagewidth, double imageheight, int splitpage,
	   double lm, double bm, int cnt)
{
	double xtran = 0;
	double ytran = 0;
	double xscale = 1;
	double yscale = 1;
	double left_offset = lm * PS_UNIT_SIZE;
	double bottom_offset = bm * PS_UNIT_SIZE;
	double subimageheight;
	double splitheight;
	double overlap;
	/* buffers for locale-insitive number formatting */
	gchar buf[2][G_ASCII_DTOSTR_BUF_SIZE];

	pagewidth *= PS_UNIT_SIZE;
	pageheight *= PS_UNIT_SIZE;

	if (ctx->maxPageHeight==0)
		splitheight = 0;
	else
		splitheight = ctx->maxPageHeight * PS_UNIT_SIZE;
	overlap = ctx->splitOverlap * PS_UNIT_SIZE;

	/*
	 * WIDTH:
	 *      if too wide, scrunch to fit
	 *      else leave it alone
	 */
	if (imagewidth <= pagewidth) {
		xscale = imagewidth;
	} else {
		xscale = pagewidth;
	}

	/* HEIGHT:
	 *      if too long, scrunch to fit
	 *      if too short, move to top of page
	 */
	if (imageheight <= pageheight) {
		yscale = imageheight;
		ytran = pageheight - imageheight;
	} else if (imageheight > pageheight &&
		(splitheight == 0 || imageheight <= splitheight)) {
		yscale = pageheight;
	} else /* imageheight > splitheight */ {
		subimageheight = imageheight - (pageheight-overlap)*splitpage;
		if (subimageheight <= pageheight) {
			yscale = imageheight;
			ytran = pageheight - subimageheight;
			splitpage = 0;
		} else if ( subimageheight > pageheight && subimageheight <= splitheight) {
			yscale = imageheight * pageheight / subimageheight;
			ytran = 0;
			splitpage = 0;
		} else /* sumimageheight > splitheight */ {
			yscale = imageheight;
			ytran = pageheight - subimageheight;
			splitpage++;
		}
	}

	bottom_offset += ytran / (cnt?2:1);
	if (cnt)
		left_offset += xtran / 2;

	fprintf(ctx->fd, "%s %s translate\n",
		g_ascii_dtostr(buf[0], sizeof(buf[0]), left_offset),
		g_ascii_dtostr(buf[1], sizeof(buf[1]), bottom_offset));
	fprintf(ctx->fd, "%s %s scale\n",
		g_ascii_dtostr(buf[0], sizeof(buf[0]), xscale),
		g_ascii_dtostr(buf[1], sizeof(buf[1]), yscale));
	if (ctx->rotate)
		fputs ("1 1 translate 180 ctx->rotate\n", ctx->fd);

	return splitpage;
}


void
tiff2ps_process_page(TIFF2PSContext* ctx, TIFF* tif, double pw, double ph,
		     double lm, double bm, gboolean cnt)
{
	uint32_t w, h;
	float ox, oy;
        double prw, prh;
	double scale = 1.0;
	double left_offset = lm * PS_UNIT_SIZE;
	double bottom_offset = bm * PS_UNIT_SIZE;
	uint16_t* sampleinfo;
	int split;
	/* buffers for locale-insitive number formatting */
	gchar buf[2][G_ASCII_DTOSTR_BUF_SIZE];

	if (!TIFFGetField(tif, TIFFTAG_XPOSITION, &ox))
		ox = 0;
	if (!TIFFGetField(tif, TIFFTAG_YPOSITION, &oy))
		oy = 0;
	setupPageState(ctx, tif, &w, &h, &prw, &prh);

	ctx->tf_numberstrips = TIFFNumberOfStrips(tif);
	TIFFGetFieldDefaulted(tif, TIFFTAG_ROWSPERSTRIP,
			      &ctx->tf_rowsperstrip);
	setupPageState(ctx, tif, &w, &h, &prw, &prh);
	if (!ctx->npages)
		PSHead(ctx, tif, w, h, prw, prh, ox, oy);
	TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE,
			      &ctx->bitspersample);
	TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL,
			      &ctx->samplesperpixel);
	TIFFGetFieldDefaulted(tif, TIFFTAG_PLANARCONFIG,
			      &ctx->planarconfiguration);
	TIFFGetField(tif, TIFFTAG_COMPRESSION, &ctx->compression);
	TIFFGetFieldDefaulted(tif, TIFFTAG_EXTRASAMPLES,
			      &ctx->extrasamples, &sampleinfo);
	ctx->alpha = (ctx->extrasamples == 1 &&
		      sampleinfo[0] == EXTRASAMPLE_ASSOCALPHA);
	if (!TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &ctx->photometric)) {
		switch (ctx->samplesperpixel - ctx->extrasamples) {
		case 1:
			if (isCCITTCompression(tif))
				ctx->photometric = PHOTOMETRIC_MINISWHITE;
			else
				ctx->photometric = PHOTOMETRIC_MINISBLACK;
			break;
		case 3:
			ctx->photometric = PHOTOMETRIC_RGB;
			break;
		case 4:
			ctx->photometric = PHOTOMETRIC_SEPARATED;
			break;
		}
	}
	if (checkImage(ctx, tif)) {
		ctx->tf_bytesperrow = TIFFScanlineSize(tif);
		ctx->npages++;
		fprintf(ctx->fd, "%%%%Page: %d %d\n", ctx->npages,
			ctx->npages);
		if (!ctx->generateEPSF && ( ctx->level2 || ctx->level3 )) {
			double psw = 0.0, psh = 0.0;
			if (psw != 0.0) {
				psw = pw * PS_UNIT_SIZE;
				if (ctx->res_unit == RESUNIT_CENTIMETER)
					psw *= 2.54F;
			} else
				psw=ctx->rotate ? prh:prw;
			if (psh != 0.0) {
				psh = ph * PS_UNIT_SIZE;
				if (ctx->res_unit == RESUNIT_CENTIMETER)
					psh *= 2.54F;
			} else
				psh=ctx->rotate ? prw:prh;
			fprintf(ctx->fd,
				"1 dict begin /PageSize [ %s %s ] def currentdict end setpagedevice\n",
				g_ascii_dtostr(buf[0], sizeof(buf[0]), psw),
				g_ascii_dtostr(buf[1], sizeof(buf[1]), psh));
			fputs(
			      "<<\n  /Policies <<\n    /PageSize 3\n  >>\n>> setpagedevice\n",
			      ctx->fd);
		}
		fprintf(ctx->fd, "gsave\n");
		fprintf(ctx->fd, "100 dict begin\n");
		if (pw != 0 || ph != 0) {
			if (!pw)
				pw = prw;
			if (!ph)
				ph = prh;
			if (ctx->maxPageHeight) { /* used -H option */
				split = PlaceImage(ctx,pw,ph,prw,prh,
						   0,lm,bm,cnt);
				while( split ) {
					PSpage(ctx, tif, w, h);
					fprintf(ctx->fd, "end\n");
					fprintf(ctx->fd, "grestore\n");
					fprintf(ctx->fd, "showpage\n");
					ctx->npages++;
					fprintf(ctx->fd, "%%%%Page: %d %d\n",
						ctx->npages, ctx->npages);
					fprintf(ctx->fd, "gsave\n");
					fprintf(ctx->fd, "100 dict begin\n");
					split = PlaceImage(ctx,pw,ph,prw,prh,
							   split,lm,bm,cnt);
				}
			} else {
				pw *= PS_UNIT_SIZE;
				ph *= PS_UNIT_SIZE;

				/* NB: maintain image aspect ratio */
				scale = pw/prw < ph/prh ?
					pw/prw : ph/prh;
				if (scale > 1.0)
					scale = 1.0;
				if (cnt) {
					bottom_offset +=
						(ph - prh * scale) / 2;
					left_offset +=
						(pw - prw * scale) / 2;
				}
				fprintf(ctx->fd, "%s %s translate\n",
					g_ascii_dtostr(buf[0], sizeof(buf[0]), left_offset),
					g_ascii_dtostr(buf[1], sizeof(buf[1]), bottom_offset));
				fprintf(ctx->fd, "%s %s scale\n",
					g_ascii_dtostr(buf[0], sizeof(buf[0]), prw * scale),
					g_ascii_dtostr(buf[1], sizeof(buf[1]), prh * scale));
				if (ctx->rotate)
					fputs ("1 1 translate 180 ctx->rotate\n", ctx->fd);
			}
		} else {
			fprintf(ctx->fd, "%s %s scale\n",
				g_ascii_dtostr(buf[0], sizeof(buf[0]), prw),
				g_ascii_dtostr(buf[1], sizeof(buf[1]), prh));
			if (ctx->rotate)
				fputs ("1 1 translate 180 ctx->rotate\n", ctx->fd);
		}
		PSpage(ctx, tif, w, h);
		fprintf(ctx->fd, "end\n");
		fprintf(ctx->fd, "grestore\n");
		fprintf(ctx->fd, "showpage\n");
	}
}


static char DuplexPreamble[] = "\
%%BeginFeature: *Duplex True\n\
systemdict begin\n\
  /languagelevel where { pop languagelevel } { 1 } ifelse\n\
  2 ge { 1 dict dup /Duplex true put setpagedevice }\n\
  { statusdict /setduplex known { statusdict begin setduplex true end } if\n\
  } ifelse\n\
end\n\
%%EndFeature\n\
";

static char TumblePreamble[] = "\
%%BeginFeature: *Tumble True\n\
systemdict begin\n\
  /languagelevel where { pop languagelevel } { 1 } ifelse\n\
  2 ge { 1 dict dup /Tumble true put setpagedevice }\n\
  { statusdict /settumble known { statusdict begin true settumble end } if\n\
  } ifelse\n\
end\n\
%%EndFeature\n\
";

static char AvoidDeadZonePreamble[] = "\
gsave newpath clippath pathbbox grestore\n\
  4 2 roll 2 copy translate\n\
  exch 3 1 roll sub 3 1 roll sub exch\n\
  currentpagedevice /PageSize get aload pop\n\
  exch 3 1 roll div 3 1 roll div abs exch abs\n\
  2 copy gt { exch } if pop\n\
  dup 1 lt { dup scale } { pop } ifelse\n\
";

void
PSHead(TIFF2PSContext *ctx, TIFF *tif, uint32_t w, uint32_t h,
       double pw, double ph, double ox, double oy)
{
	time_t t;

	(void) tif; (void) w; (void) h;
	t = time(0);
	fprintf(ctx->fd, "%%!PS-Adobe-3.0%s\n",
		ctx->generateEPSF ? " EPSF-3.0" : "");
	fprintf(ctx->fd, "%%%%Creator: Evince\n");
	fprintf(ctx->fd, "%%%%CreationDate: %s", ctime(&t));
	fprintf(ctx->fd, "%%%%DocumentData: Clean7Bit\n");
	fprintf(ctx->fd, "%%%%Origin: %ld %ld\n", (long) ox, (long) oy);
	/* NB: should use PageBoundingBox */
	fprintf(ctx->fd, "%%%%BoundingBox: 0 0 %ld %ld\n",
		(long) ceil(pw), (long) ceil(ph));
	fprintf(ctx->fd, "%%%%LanguageLevel: %d\n",
		(ctx->level3 ? 3 : (ctx->level2 ? 2 : 1)));
	fprintf(ctx->fd, "%%%%Pages: (atend)\n");
	fprintf(ctx->fd, "%%%%EndComments\n");
	fprintf(ctx->fd, "%%%%BeginSetup\n");
	if (ctx->PSduplex)
		fprintf(ctx->fd, "%s", DuplexPreamble);
	if (ctx->PStumble)
		fprintf(ctx->fd, "%s", TumblePreamble);
	if (ctx->PSavoiddeadzone && (ctx->level2 || ctx->level3))
		fprintf(ctx->fd, "%s", AvoidDeadZonePreamble);
	fprintf(ctx->fd, "%%%%EndSetup\n");
}

static void
PSTail(TIFF2PSContext *ctx)
{
	if (!ctx->npages)
		return;
	fprintf(ctx->fd, "%%%%Trailer\n");
	fprintf(ctx->fd, "%%%%Pages: %d\n", ctx->npages);
	fprintf(ctx->fd, "%%%%EOF\n");
}

static int
checkcmap(TIFF2PSContext* ctx, TIFF* tif, int n,
	  uint16_t* r, uint16_t* g, uint16_t* b)
{
	(void) tif;
	while (n-- > 0)
		if (*r++ >= 256 || *g++ >= 256 || *b++ >= 256)
			return (16);
	TIFFWarning(ctx->filename, "Assuming 8-bit colormap");
	return (8);
}

static void
PS_Lvl2colorspace(TIFF2PSContext* ctx, TIFF* tif)
{
	uint16_t *rmap, *gmap, *bmap;
	int i, num_colors;
	const char * colorspace_p;

	switch ( ctx->photometric )
	{
	case PHOTOMETRIC_SEPARATED:
		colorspace_p = "CMYK";
		break;

	case PHOTOMETRIC_RGB:
		colorspace_p = "RGB";
		break;

	default:
		colorspace_p = "Gray";
	}

	/*
	 * Set up PostScript Level 2 colorspace according to
	 * section 4.8 in the PostScript reference manual.
	 */
	fputs("% PostScript Level 2 only.\n", ctx->fd);
	if (ctx->photometric != PHOTOMETRIC_PALETTE) {
		if (ctx->photometric == PHOTOMETRIC_YCBCR) {
		    /* MORE CODE HERE */
		}
		fprintf(ctx->fd, "/Device%s setcolorspace\n", colorspace_p );
		return;
	}

	/*
	 * Set up an indexed/palette colorspace
	 */
	num_colors = (1 << ctx->bitspersample);
	if (!TIFFGetField(tif, TIFFTAG_COLORMAP, &rmap, &gmap, &bmap)) {
		TIFFError(ctx->filename,
			"Palette image w/o \"Colormap\" tag");
		return;
	}
	if (checkcmap(ctx, tif, num_colors, rmap, gmap, bmap) == 16) {
		/*
		 * Convert colormap to 8-bits values.
		 */
#define	CVT(x)		(((x) * 255) / ((1L<<16)-1))
		for (i = 0; i < num_colors; i++) {
			rmap[i] = CVT(rmap[i]);
			gmap[i] = CVT(gmap[i]);
			bmap[i] = CVT(bmap[i]);
		}
#undef CVT
	}
	fprintf(ctx->fd, "[ /Indexed /DeviceRGB %d", num_colors - 1);
	if (ctx->ascii85) {
		Ascii85Init(ctx);
		fputs("\n<~", ctx->fd);
		ctx->ascii85breaklen -= 2;
	} else
		fputs(" <", ctx->fd);
	for (i = 0; i < num_colors; i++) {
		if (ctx->ascii85) {
			Ascii85Put(ctx, (unsigned char)rmap[i]);
			Ascii85Put(ctx, (unsigned char)gmap[i]);
			Ascii85Put(ctx, (unsigned char)bmap[i]);
		} else {
			fputs((i % 8) ? " " : "\n  ", ctx->fd);
			fprintf(ctx->fd, "%02x%02x%02x",
			    rmap[i], gmap[i], bmap[i]);
		}
	}
	if (ctx->ascii85)
		Ascii85Flush(ctx);
	else
		fputs(">\n", ctx->fd);
	fputs("] setcolorspace\n", ctx->fd);
}

static int
PS_Lvl2ImageDict(TIFF2PSContext* ctx, TIFF* tif, uint32_t w, uint32_t h)
{
	int use_rawdata;
	uint32_t tile_width, tile_height;
	uint16_t predictor, minsamplevalue, maxsamplevalue;
	int repeat_count;
	char im_h[64], im_x[64], im_y[64];

	(void)strcpy(im_x, "0");
	(void)sprintf(im_y, "%lu", (long) h);
	(void)sprintf(im_h, "%lu", (long) h);
	tile_width = w;
	tile_height = h;
	if (TIFFIsTiled(tif)) {
		repeat_count = TIFFNumberOfTiles(tif);
		TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tile_width);
		TIFFGetField(tif, TIFFTAG_TILELENGTH, &tile_height);
		if (tile_width > w || tile_height > h ||
		    (w % tile_width) != 0 || (h % tile_height != 0)) {
			/*
			 * The tiles does not fit image width and height.
			 * Set up a clip rectangle for the image unit square.
			 */
			fputs("0 0 1 1 rectclip\n", ctx->fd);
		}
		if (tile_width < w) {
			fputs("/im_x 0 def\n", ctx->fd);
			(void)strcpy(im_x, "im_x neg");
		}
		if (tile_height < h) {
			fputs("/im_y 0 def\n", ctx->fd);
			(void)sprintf(im_y, "%lu im_y sub", (unsigned long) h);
		}
	} else {
		repeat_count = ctx->tf_numberstrips;
		tile_height = ctx->tf_rowsperstrip;
		if (tile_height > h)
			tile_height = h;
		if (repeat_count > 1) {
			fputs("/im_y 0 def\n", ctx->fd);
			fprintf(ctx->fd, "/im_h %lu def\n",
			    (unsigned long) tile_height);
			(void)strcpy(im_h, "im_h");
			(void)sprintf(im_y, "%lu im_y sub", (unsigned long) h);
		}
	}

	/*
	 * Output start of exec block
	 */
	fputs("{ % exec\n", ctx->fd);

	if (repeat_count > 1)
		fprintf(ctx->fd, "%d { %% repeat\n", repeat_count);

	/*
	 * Output filter options and image dictionary.
	 */
	if (ctx->ascii85)
		fputs(" /im_stream currentfile /ASCII85Decode filter def\n",
		      ctx->fd);
	fputs(" <<\n", ctx->fd);
	fputs("  /ImageType 1\n", ctx->fd);
	fprintf(ctx->fd, "  /Width %lu\n", (unsigned long) tile_width);
	/*
	 * Workaround for some software that may crash when last strip
	 * of image contains fewer number of scanlines than specified
	 * by the `/Height' variable. So for stripped images with multiple
	 * strips we will set `/Height' as `im_h', because one is
	 * recalculated for each strip - including the (smaller) final strip.
	 * For tiled images and images with only one strip `/Height' will
	 * contain number of scanlines in tile (or image height in case of
	 * one-stripped image).
	 */
	if (TIFFIsTiled(tif) || ctx->tf_numberstrips == 1)
		fprintf(ctx->fd, "  /Height %lu\n", (unsigned long) tile_height);
	else
		fprintf(ctx->fd, "  /Height im_h\n");

	if (ctx->planarconfiguration == PLANARCONFIG_SEPARATE && ctx->samplesperpixel > 1)
		fputs("  /MultipleDataSources true\n", ctx->fd);
	fprintf(ctx->fd, "  /ImageMatrix [ %lu 0 0 %ld %s %s ]\n",
	    (unsigned long) w, - (long)h, im_x, im_y);
	fprintf(ctx->fd, "  /BitsPerComponent %d\n", ctx->bitspersample);
	fprintf(ctx->fd, "  /Ctx->Interpolate %s\n", ctx->interpolate ? "true" : "false");

	switch (ctx->samplesperpixel - ctx->extrasamples) {
	case 1:
		switch (ctx->photometric) {
		case PHOTOMETRIC_MINISBLACK:
			fputs("  /Decode [0 1]\n", ctx->fd);
			break;
		case PHOTOMETRIC_MINISWHITE:
			switch (ctx->compression) {
			case COMPRESSION_CCITTRLE:
			case COMPRESSION_CCITTRLEW:
			case COMPRESSION_CCITTFAX3:
			case COMPRESSION_CCITTFAX4:
				/*
				 * Manage inverting with /Blackis1 flag
				 * since there might be uncompressed parts
				 */
				fputs("  /Decode [0 1]\n", ctx->fd);
				break;
			default:
				/*
				 * ERROR...
				 */
				fputs("  /Decode [1 0]\n", ctx->fd);
				break;
			}
			break;
		case PHOTOMETRIC_PALETTE:
			TIFFGetFieldDefaulted(tif, TIFFTAG_MINSAMPLEVALUE,
			    &minsamplevalue);
			TIFFGetFieldDefaulted(tif, TIFFTAG_MAXSAMPLEVALUE,
			    &maxsamplevalue);
			fprintf(ctx->fd, "  /Decode [%u %u]\n",
				    minsamplevalue, maxsamplevalue);
			break;
		default:
			/*
			 * ERROR ?
			 */
			fputs("  /Decode [0 1]\n", ctx->fd);
			break;
		}
		break;
	case 3:
		switch (ctx->photometric) {
		case PHOTOMETRIC_RGB:
			fputs("  /Decode [0 1 0 1 0 1]\n", ctx->fd);
			break;
		case PHOTOMETRIC_MINISWHITE:
		case PHOTOMETRIC_MINISBLACK:
		default:
			/*
			 * ERROR??
			 */
			fputs("  /Decode [0 1 0 1 0 1]\n", ctx->fd);
			break;
		}
		break;
	case 4:
		/*
		 * ERROR??
		 */
		fputs("  /Decode [0 1 0 1 0 1 0 1]\n", ctx->fd);
		break;
	}
	fputs("  /DataSource", ctx->fd);
	if (ctx->planarconfiguration == PLANARCONFIG_SEPARATE &&
	    ctx->samplesperpixel > 1)
		fputs(" [", ctx->fd);
	if (ctx->ascii85)
		fputs(" im_stream", ctx->fd);
	else
		fputs(" currentfile /ASCIIHexDecode filter", ctx->fd);

	use_rawdata = TRUE;
	switch (ctx->compression) {
	case COMPRESSION_NONE:		/* 1: uncompressed */
		break;
	case COMPRESSION_CCITTRLE:	/* 2: CCITT modified Huffman RLE */
	case COMPRESSION_CCITTRLEW:	/* 32771: #1 w/ word alignment */
	case COMPRESSION_CCITTFAX3:	/* 3: CCITT Group 3 fax encoding */
	case COMPRESSION_CCITTFAX4:	/* 4: CCITT Group 4 fax encoding */
		fputs("\n\t<<\n", ctx->fd);
		if (ctx->compression == COMPRESSION_CCITTFAX3) {
			uint32_t g3_options;

			fputs("\t /EndOfLine true\n", ctx->fd);
			fputs("\t /EndOfBlock false\n", ctx->fd);
			if (!TIFFGetField(tif, TIFFTAG_GROUP3OPTIONS,
					    &g3_options))
				g3_options = 0;
			if (g3_options & GROUP3OPT_2DENCODING)
				fprintf(ctx->fd, "\t /K %s\n", im_h);
			if (g3_options & GROUP3OPT_UNCOMPRESSED)
				fputs("\t /Uncompressed true\n", ctx->fd);
			if (g3_options & GROUP3OPT_FILLBITS)
				fputs("\t /EncodedByteAlign true\n", ctx->fd);
		}
		if (ctx->compression == COMPRESSION_CCITTFAX4) {
			uint32_t g4_options;

			fputs("\t /K -1\n", ctx->fd);
			TIFFGetFieldDefaulted(tif, TIFFTAG_GROUP4OPTIONS,
					       &g4_options);
			if (g4_options & GROUP4OPT_UNCOMPRESSED)
				fputs("\t /Uncompressed true\n", ctx->fd);
		}
		if (!(tile_width == w && w == 1728U))
			fprintf(ctx->fd, "\t /Columns %lu\n",
			    (unsigned long) tile_width);
		fprintf(ctx->fd, "\t /Rows %s\n", im_h);
		if (ctx->compression == COMPRESSION_CCITTRLE ||
		    ctx->compression == COMPRESSION_CCITTRLEW) {
			fputs("\t /EncodedByteAlign true\n", ctx->fd);
			fputs("\t /EndOfBlock false\n", ctx->fd);
		}
		if (ctx->photometric == PHOTOMETRIC_MINISBLACK)
			fputs("\t /BlackIs1 true\n", ctx->fd);
		fprintf(ctx->fd, "\t>> /CCITTFaxDecode filter");
		break;
	case COMPRESSION_LZW:	/* 5: Lempel-Ziv & Welch */
		TIFFGetFieldDefaulted(tif, TIFFTAG_PREDICTOR, &predictor);
		if (predictor == 2) {
			fputs("\n\t<<\n", ctx->fd);
			fprintf(ctx->fd, "\t /Predictor %u\n", predictor);
			fprintf(ctx->fd, "\t /Columns %lu\n",
			    (unsigned long) tile_width);
			fprintf(ctx->fd, "\t /Colors %u\n", ctx->samplesperpixel);
			fprintf(ctx->fd, "\t /BitsPerComponent %u\n",
			    ctx->bitspersample);
			fputs("\t>>", ctx->fd);
		}
		fputs(" /LZWDecode filter", ctx->fd);
		break;
	case COMPRESSION_DEFLATE:	/* 5: ZIP */
	case COMPRESSION_ADOBE_DEFLATE:
		if ( ctx->level3 ) {
			 TIFFGetFieldDefaulted(tif, TIFFTAG_PREDICTOR, &predictor);
			 if (predictor > 1) {
				fprintf(ctx->fd, "\t %% PostScript Level 3 only.");
				fputs("\n\t<<\n", ctx->fd);
				fprintf(ctx->fd, "\t /Predictor %u\n", predictor);
				fprintf(ctx->fd, "\t /Columns %lu\n",
					(unsigned long) tile_width);
				fprintf(ctx->fd, "\t /Colors %u\n", ctx->samplesperpixel);
					fprintf(ctx->fd, "\t /BitsPerComponent %u\n",
					ctx->bitspersample);
				fputs("\t>>", ctx->fd);
			 }
			 fputs(" /FlateDecode filter", ctx->fd);
		} else {
			use_rawdata = FALSE ;
		}
		break;
	case COMPRESSION_PACKBITS:	/* 32773: Macintosh RLE */
		fputs(" /RunLengthDecode filter", ctx->fd);
		use_rawdata = TRUE;
	    break;
	case COMPRESSION_OJPEG:		/* 6: !6.0 JPEG */
	case COMPRESSION_JPEG:		/* 7: %JPEG DCT ctx->compression */
#ifdef notdef
		/*
		 * Code not tested yet
		 */
		fputs(" /DCTDecode filter", ctx->fd);
		use_rawdata = TRUE;
#else
		use_rawdata = FALSE;
#endif
		break;
	case COMPRESSION_NEXT:		/* 32766: NeXT 2-bit RLE */
	case COMPRESSION_THUNDERSCAN:	/* 32809: ThunderScan RLE */
	case COMPRESSION_PIXARFILM:	/* 32908: Pixar companded 10bit LZW */
	case COMPRESSION_JBIG:		/* 34661: ISO JBIG */
		use_rawdata = FALSE;
		break;
	case COMPRESSION_SGILOG:	/* 34676: SGI LogL or LogLuv */
	case COMPRESSION_SGILOG24:	/* 34677: SGI 24-bit LogLuv */
		use_rawdata = FALSE;
		break;
	default:
		/*
		 * ERROR...
		 */
		use_rawdata = FALSE;
		break;
	}
	if (ctx->planarconfiguration == PLANARCONFIG_SEPARATE &&
	    ctx->samplesperpixel > 1) {
		uint16_t i;

		/*
		 * NOTE: This code does not work yet...
		 */
		for (i = 1; i < ctx->samplesperpixel; i++)
			fputs(" dup", ctx->fd);
		fputs(" ]", ctx->fd);
	}

	fprintf( ctx->fd, "\n >> %s\n", IMAGEOP(ctx) );
	if (ctx->ascii85)
		fputs(" im_stream status { im_stream flushfile } if\n", ctx->fd);
	if (repeat_count > 1) {
		if (tile_width < w) {
			fprintf(ctx->fd, " /im_x im_x %lu add def\n",
			    (unsigned long) tile_width);
			if (tile_height < h) {
				fprintf(ctx->fd, " im_x %lu ge {\n",
				    (unsigned long) w);
				fputs("  /im_x 0 def\n", ctx->fd);
				fprintf(ctx->fd, " /im_y im_y %lu add def\n",
				    (unsigned long) tile_height);
				fputs(" } if\n", ctx->fd);
			}
		}
		if (tile_height < h) {
			if (tile_width >= w) {
				fprintf(ctx->fd, " /im_y im_y %lu add def\n",
				    (unsigned long) tile_height);
				if (!TIFFIsTiled(tif)) {
					fprintf(ctx->fd, " /im_h %lu im_y sub",
					    (unsigned long) h);
					fprintf(ctx->fd, " dup %lu gt { pop",
					    (unsigned long) tile_height);
					fprintf(ctx->fd, " %lu } if def\n",
					    (unsigned long) tile_height);
				}
			}
		}
		fputs("} repeat\n", ctx->fd);
	}
	/*
	 * End of exec function
	 */
	fputs("}\n", ctx->fd);

	return(use_rawdata);
}

#define MAXLINE		36

static int
PS_Lvl2page(TIFF2PSContext* ctx, TIFF* tif, uint32_t w, uint32_t h)
{
	uint16_t fillorder;
	int use_rawdata, tiled_image, breaklen = MAXLINE;
	uint32_t chunk_no, num_chunks, *bc;
	unsigned char *buf_data, *cp;
	tsize_t chunk_size, byte_count;

#if defined( EXP_ASCII85ENCODER )
	int			ascii85_l;	/* Length, in bytes, of ascii85_p[] data */
	uint8_t		*	ascii85_p = 0;	/* Holds ASCII85 encoded data */
#endif

	PS_Lvl2colorspace(ctx, tif);
	use_rawdata = PS_Lvl2ImageDict(ctx, tif, w, h);

/* See http://bugzilla.remotesensing.org/show_bug.cgi?id=80 */
#ifdef ENABLE_BROKEN_BEGINENDDATA
	fputs("%%BeginData:\n", ctx->fd);
#endif
	fputs("exec\n", ctx->fd);

	tiled_image = TIFFIsTiled(tif);
	if (tiled_image) {
		num_chunks = TIFFNumberOfTiles(tif);
		TIFFGetField(tif, TIFFTAG_TILEBYTECOUNTS, &bc);
	} else {
		num_chunks = TIFFNumberOfStrips(tif);
		TIFFGetField(tif, TIFFTAG_STRIPBYTECOUNTS, &bc);
	}

	if (use_rawdata) {
		chunk_size = (tsize_t) bc[0];
		for (chunk_no = 1; chunk_no < num_chunks; chunk_no++)
			if ((tsize_t) bc[chunk_no] > chunk_size)
				chunk_size = (tsize_t) bc[chunk_no];
	} else {
		if (tiled_image)
			chunk_size = TIFFTileSize(tif);
		else
			chunk_size = TIFFStripSize(tif);
	}
	buf_data = (unsigned char *)_TIFFmalloc(chunk_size);
	if (!buf_data) {
		TIFFError(ctx->filename, "Can't alloc %u bytes for %s.",
			(unsigned int) chunk_size, tiled_image ? "tiles" : "strips");
		return(FALSE);
	}

#if defined( EXP_ASCII85ENCODER )
	if ( ctx->ascii85 ) {
	    /*
	     * Allocate a buffer to hold the ASCII85 encoded data.  Note
	     * that it is allocated with sufficient room to hold the
	     * encoded data (5*chunk_size/4) plus the EOD marker (+8)
	     * and formatting line breaks.  The line breaks are more
	     * than taken care of by using 6*chunk_size/4 rather than
	     * 5*chunk_size/4.
	     */

	    ascii85_p = _TIFFmalloc( (chunk_size+(chunk_size/2)) + 8 );

	    if ( !ascii85_p ) {
		_TIFFfree( buf_data );

		TIFFError( ctx->filename,
			   "Cannot allocate ASCII85 encoding buffer." );
		return ( FALSE );
	    }
	}
#endif

	TIFFGetFieldDefaulted(tif, TIFFTAG_FILLORDER, &fillorder);
	for (chunk_no = 0; chunk_no < num_chunks; chunk_no++) {
		if (ctx->ascii85)
			Ascii85Init(ctx);
		else
			breaklen = MAXLINE;
		if (use_rawdata) {
			if (tiled_image)
				byte_count = TIFFReadRawTile(tif, chunk_no,
						  buf_data, chunk_size);
			else
				byte_count = TIFFReadRawStrip(tif, chunk_no,
						  buf_data, chunk_size);
			if (fillorder == FILLORDER_LSB2MSB)
			    TIFFReverseBits(buf_data, byte_count);
		} else {
			if (tiled_image)
				byte_count = TIFFReadEncodedTile(tif,
						chunk_no, buf_data,
						chunk_size);
			else
				byte_count = TIFFReadEncodedStrip(tif,
						chunk_no, buf_data,
						chunk_size);
		}
		if (byte_count < 0) {
			TIFFError(ctx->filename, "Can't read %s %d.",
				tiled_image ? "tile" : "strip", chunk_no);
			if (ctx->ascii85)
				Ascii85Put(ctx, '\0');
		}
		/*
		 * For images with ctx->alpha, matte against a white background;
		 * i.e. Cback * (1 - Aimage) where Cback = 1. We will fill the
		 * lower part of the buffer with the modified values.
		 *
		 * XXX: needs better solution
		 */
		if (ctx->alpha) {
			int adjust, i, j = 0;
			int ncomps = ctx->samplesperpixel - ctx->extrasamples;
			for (i = 0; i < byte_count; i+=ctx->samplesperpixel) {
				adjust = 255 - buf_data[i + ncomps];
				switch (ncomps) {
					case 1:
						buf_data[j++] = buf_data[i] + adjust;
						break;
					case 2:
						buf_data[j++] = buf_data[i] + adjust;
						buf_data[j++] = buf_data[i+1] + adjust;
						break;
					case 3:
						buf_data[j++] = buf_data[i] + adjust;
						buf_data[j++] = buf_data[i+1] + adjust;
						buf_data[j++] = buf_data[i+2] + adjust;
						break;
				}
			}
			byte_count -= j;
		}

		if (ctx->ascii85) {
#if defined( EXP_ASCII85ENCODER )
			ascii85_l = Ascii85EncodeBlock(ctx, ascii85_p, 1,
						       buf_data, byte_count);

			if ( ascii85_l > 0 )
				fwrite( ascii85_p, ascii85_l, 1, ctx->fd );
#else
			for (cp = buf_data; byte_count > 0; byte_count--)
				Ascii85Put(ctx, *cp++);
#endif
		}
		else
		{
			for (cp = buf_data; byte_count > 0; byte_count--) {
				putc(hex[((*cp)>>4)&0xf], ctx->fd);
				putc(hex[(*cp)&0xf], ctx->fd);
				cp++;

				if (--breaklen <= 0) {
					putc('\n', ctx->fd);
					breaklen = MAXLINE;
				}
			}
		}

		if ( !ctx->ascii85 ) {
			if ( ctx->level2 || ctx->level3 )
				putc( '>', ctx->fd );
			putc('\n', ctx->fd);
		}
#if !defined( EXP_ASCII85ENCODER )
		else
			Ascii85Flush(ctx);
#endif
	}

#if defined( EXP_ASCII85ENCODER )
	if ( ascii85_p )
	    _TIFFfree( ascii85_p );
#endif
	_TIFFfree(buf_data);
#ifdef ENABLE_BROKEN_BEGINENDDATA
	fputs("%%EndData\n", ctx->fd);
#endif
	return(TRUE);
}

void
PSpage(TIFF2PSContext* ctx, TIFF* tif, uint32_t w, uint32_t h)
{
	if ((ctx->level2 || ctx->level3) && PS_Lvl2page(ctx, tif, w, h))
		return;
	ctx->ps_bytesperrow = ctx->tf_bytesperrow - (ctx->extrasamples * ctx->bitspersample / 8)*w;
	switch (ctx->photometric) {
	case PHOTOMETRIC_RGB:
		if (ctx->planarconfiguration == PLANARCONFIG_CONTIG) {
			fprintf(ctx->fd, "%s", RGBcolorimage);
			PSColorContigPreamble(ctx, w, h, 3);
			PSDataColorContig(ctx, tif, w, h, 3);
		} else {
			PSColorSeparatePreamble(ctx, w, h, 3);
			PSDataColorSeparate(ctx, tif, w, h, 3);
		}
		break;
	case PHOTOMETRIC_SEPARATED:
		/* XXX should emit CMYKcolorimage */
		if (ctx->planarconfiguration == PLANARCONFIG_CONTIG) {
			PSColorContigPreamble(ctx, w, h, 4);
			PSDataColorContig(ctx, tif, w, h, 4);
		} else {
			PSColorSeparatePreamble(ctx, w, h, 4);
			PSDataColorSeparate(ctx, tif, w, h, 4);
		}
		break;
	case PHOTOMETRIC_PALETTE:
		fprintf(ctx->fd, "%s", RGBcolorimage);
		PhotoshopBanner(ctx, w, h, 1, 3, "false 3 colorimage");
		fprintf(ctx->fd, "/scanLine %ld string def\n",
			(long) ctx->ps_bytesperrow * 3L);
		fprintf(ctx->fd, "%lu %lu 8\n",
			(unsigned long) w, (unsigned long) h);
		fprintf(ctx->fd, "[%lu 0 0 -%lu 0 %lu]\n",
			(unsigned long) w, (unsigned long) h,
			(unsigned long) h);
		fprintf(ctx->fd,
			"{currentfile scanLine readhexstring pop} bind\n");
		fprintf(ctx->fd, "false 3 colorimage\n");
		PSDataPalette(ctx, tif, w, h);
		break;
	case PHOTOMETRIC_MINISBLACK:
	case PHOTOMETRIC_MINISWHITE:
		PhotoshopBanner(ctx, w, h, 1, 1, IMAGEOP(ctx));
		fprintf(ctx->fd, "/scanLine %ld string def\n",
		    (long) ctx->ps_bytesperrow);
		fprintf(ctx->fd, "%lu %lu %d\n",
		    (unsigned long) w, (unsigned long) h, ctx->bitspersample);
		fprintf(ctx->fd, "[%lu 0 0 -%lu 0 %lu]\n",
		    (unsigned long) w, (unsigned long) h, (unsigned long) h);
		fprintf(ctx->fd,
		    "{currentfile scanLine readhexstring pop} bind\n");
		fprintf(ctx->fd, "%s\n", IMAGEOP(ctx));
		PSDataBW(ctx, tif, w, h);
		break;
	}
	putc('\n', ctx->fd);
}

void
PSColorContigPreamble(TIFF2PSContext* ctx, uint32_t w, uint32_t h, int nc)
{
	ctx->ps_bytesperrow = nc * (ctx->tf_bytesperrow / ctx->samplesperpixel);
	PhotoshopBanner(ctx, w, h, 1, nc, "false %d colorimage", nc);
	fprintf(ctx->fd, "/line %ld string def\n", (long) ctx->ps_bytesperrow);
	fprintf(ctx->fd, "%lu %lu %d\n",
	    (unsigned long) w, (unsigned long) h, ctx->bitspersample);
	fprintf(ctx->fd, "[%lu 0 0 -%lu 0 %lu]\n",
	    (unsigned long) w, (unsigned long) h, (unsigned long) h);
	fprintf(ctx->fd, "{currentfile line readhexstring pop} bind\n");
	fprintf(ctx->fd, "false %d colorimage\n", nc);
}

void
PSColorSeparatePreamble(TIFF2PSContext* ctx, uint32_t w, uint32_t h, int nc)
{
	int i;

	PhotoshopBanner(ctx, w, h, ctx->ps_bytesperrow, nc, "true %d colorimage", nc);
	for (i = 0; i < nc; i++)
		fprintf(ctx->fd, "/line%d %ld string def\n",
		    i, (long) ctx->ps_bytesperrow);
	fprintf(ctx->fd, "%lu %lu %d\n",
	    (unsigned long) w, (unsigned long) h, ctx->bitspersample);
	fprintf(ctx->fd, "[%lu 0 0 -%lu 0 %lu] \n",
	    (unsigned long) w, (unsigned long) h, (unsigned long) h);
	for (i = 0; i < nc; i++)
		fprintf(ctx->fd, "{currentfile line%d readhexstring pop}bind\n", i);
	fprintf(ctx->fd, "true %d colorimage\n", nc);
}

#define	DOBREAK(len, howmany, fd) \
	if (((len) -= (howmany)) <= 0) {	\
		putc('\n', fd);			\
		(len) = MAXLINE-(howmany);	\
	}
#define	PUTHEX(c,fd)	putc(hex[((c)>>4)&0xf],fd); putc(hex[(c)&0xf],fd)

void
PSDataColorContig(TIFF2PSContext* ctx, TIFF* tif, uint32_t w, uint32_t h, int nc)
{
	uint32_t row;
	int breaklen = MAXLINE, cc, es = ctx->samplesperpixel - nc;
	unsigned char *tf_buf;
	unsigned char *cp, c;

	(void) w;
	tf_buf = (unsigned char *) _TIFFmalloc(ctx->tf_bytesperrow);
	if (tf_buf == NULL) {
		TIFFError(ctx->filename, "No space for scanline buffer");
		return;
	}
	for (row = 0; row < h; row++) {
		if (TIFFReadScanline(tif, tf_buf, row, 0) < 0)
			break;
		cp = tf_buf;
		if (ctx->alpha) {
			int adjust;
			cc = 0;
			for (; cc < ctx->tf_bytesperrow; cc += ctx->samplesperpixel) {
				DOBREAK(breaklen, nc, ctx->fd);
				/*
				 * For images with ctx->alpha, matte against
				 * a white background; i.e.
				 *    Cback * (1 - Aimage)
				 * where Cback = 1.
				 */
				adjust = 255 - cp[nc];
				switch (nc) {
				case 4: c = *cp++ + adjust; PUTHEX(c,ctx->fd);
				case 3: c = *cp++ + adjust; PUTHEX(c,ctx->fd);
				case 2: c = *cp++ + adjust; PUTHEX(c,ctx->fd);
				case 1: c = *cp++ + adjust; PUTHEX(c,ctx->fd);
				}
				cp += es;
			}
		} else {
			cc = 0;
			for (; cc < ctx->tf_bytesperrow; cc += ctx->samplesperpixel) {
				DOBREAK(breaklen, nc, ctx->fd);
				switch (nc) {
				case 4: c = *cp++; PUTHEX(c,ctx->fd);
				case 3: c = *cp++; PUTHEX(c,ctx->fd);
				case 2: c = *cp++; PUTHEX(c,ctx->fd);
				case 1: c = *cp++; PUTHEX(c,ctx->fd);
				}
				cp += es;
			}
		}
	}
	_TIFFfree((char *) tf_buf);
}

void
PSDataColorSeparate(TIFF2PSContext* ctx, TIFF* tif, uint32_t w, uint32_t h, int nc)
{
	uint32_t row;
	int breaklen = MAXLINE, cc;
	tsample_t s, maxs;
	unsigned char *tf_buf;
	unsigned char *cp, c;

	(void) w;
	tf_buf = (unsigned char *) _TIFFmalloc(ctx->tf_bytesperrow);
	if (tf_buf == NULL) {
		TIFFError(ctx->filename, "No space for scanline buffer");
		return;
	}
	maxs = (ctx->samplesperpixel > nc ? nc : ctx->samplesperpixel);
	for (row = 0; row < h; row++) {
		for (s = 0; s < maxs; s++) {
			if (TIFFReadScanline(tif, tf_buf, row, s) < 0)
				break;
			for (cp = tf_buf, cc = 0; cc < ctx->tf_bytesperrow; cc++) {
				DOBREAK(breaklen, 1, ctx->fd);
				c = *cp++;
				PUTHEX(c,ctx->fd);
			}
		}
	}
	_TIFFfree((char *) tf_buf);
}

#define	PUTRGBHEX(c,fd) \
	PUTHEX(rmap[c],fd); PUTHEX(gmap[c],fd); PUTHEX(bmap[c],fd)

void
PSDataPalette(TIFF2PSContext* ctx, TIFF* tif, uint32_t w, uint32_t h)
{
	uint16_t *rmap, *gmap, *bmap;
	uint32_t row;
	int breaklen = MAXLINE, cc, nc;
	unsigned char *tf_buf;
	unsigned char *cp, c;

	(void) w;
	if (!TIFFGetField(tif, TIFFTAG_COLORMAP, &rmap, &gmap, &bmap)) {
		TIFFError(ctx->filename, "Palette image w/o \"Colormap\" tag");
		return;
	}
	switch (ctx->bitspersample) {
	case 8:	case 4: case 2: case 1:
		break;
	default:
		TIFFError(ctx->filename, "Depth %d not supported", ctx->bitspersample);
		return;
	}
	nc = 3 * (8 / ctx->bitspersample);
	tf_buf = (unsigned char *) _TIFFmalloc(ctx->tf_bytesperrow);
	if (tf_buf == NULL) {
		TIFFError(ctx->filename, "No space for scanline buffer");
		return;
	}
	if (checkcmap(ctx, tif, 1<<ctx->bitspersample, rmap, gmap, bmap) == 16) {
		int i;
#define	CVT(x)		((unsigned short) (((x) * 255) / ((1U<<16)-1)))
		for (i = (1<<ctx->bitspersample)-1; i >= 0; i--) {
			rmap[i] = CVT(rmap[i]);
			gmap[i] = CVT(gmap[i]);
			bmap[i] = CVT(bmap[i]);
		}
#undef CVT
	}
	for (row = 0; row < h; row++) {
		if (TIFFReadScanline(tif, tf_buf, row, 0) < 0)
			break;
		for (cp = tf_buf, cc = 0; cc < ctx->tf_bytesperrow; cc++) {
			DOBREAK(breaklen, nc, ctx->fd);
			switch (ctx->bitspersample) {
			case 8:
				c = *cp++; PUTRGBHEX(c, ctx->fd);
				break;
			case 4:
				c = *cp++; PUTRGBHEX(c&0xf, ctx->fd);
				c >>= 4;   PUTRGBHEX(c, ctx->fd);
				break;
			case 2:
				c = *cp++; PUTRGBHEX(c&0x3, ctx->fd);
				c >>= 2;   PUTRGBHEX(c&0x3, ctx->fd);
				c >>= 2;   PUTRGBHEX(c&0x3, ctx->fd);
				c >>= 2;   PUTRGBHEX(c, ctx->fd);
				break;
			case 1:
				c = *cp++; PUTRGBHEX(c&0x1, ctx->fd);
				c >>= 1;   PUTRGBHEX(c&0x1, ctx->fd);
				c >>= 1;   PUTRGBHEX(c&0x1, ctx->fd);
				c >>= 1;   PUTRGBHEX(c&0x1, ctx->fd);
				c >>= 1;   PUTRGBHEX(c&0x1, ctx->fd);
				c >>= 1;   PUTRGBHEX(c&0x1, ctx->fd);
				c >>= 1;   PUTRGBHEX(c&0x1, ctx->fd);
				c >>= 1;   PUTRGBHEX(c, ctx->fd);
				break;
			}
		}
	}
	_TIFFfree((char *) tf_buf);
}

void
PSDataBW(TIFF2PSContext* ctx, TIFF* tif, uint32_t w, uint32_t h)
{
	int breaklen = MAXLINE;
	unsigned char* tf_buf;
	unsigned char* cp;
	tsize_t stripsize = TIFFStripSize(tif);
	tstrip_t s;

#if defined( EXP_ASCII85ENCODER )
	int	ascii85_l;		/* Length, in bytes, of ascii85_p[] data */
	uint8_t	*ascii85_p = 0;		/* Holds ASCII85 encoded data */
#endif

	(void) w; (void) h;
	tf_buf = (unsigned char *) _TIFFmalloc(stripsize);
        memset(tf_buf, 0, stripsize);
	if (tf_buf == NULL) {
		TIFFError(ctx->filename, "No space for scanline buffer");
		return;
	}

#if defined( EXP_ASCII85ENCODER )
	if ( ctx->ascii85 ) {
	    /*
	     * Allocate a buffer to hold the ASCII85 encoded data.  Note
	     * that it is allocated with sufficient room to hold the
	     * encoded data (5*stripsize/4) plus the EOD marker (+8)
	     * and formatting line breaks.  The line breaks are more
	     * than taken care of by using 6*stripsize/4 rather than
	     * 5*stripsize/4.
	     */

	    ascii85_p = _TIFFmalloc( (stripsize+(stripsize/2)) + 8 );

	    if ( !ascii85_p ) {
		_TIFFfree( tf_buf );

		TIFFError( ctx->filename,
			   "Cannot allocate ASCII85 encoding buffer." );
		return;
	    }
	}
#endif

	if (ctx->ascii85)
		Ascii85Init(ctx);

	for (s = 0; s < TIFFNumberOfStrips(tif); s++) {
		int cc = TIFFReadEncodedStrip(tif, s, tf_buf, stripsize);
		if (cc < 0) {
			TIFFError(ctx->filename, "Can't read strip");
			break;
		}
		cp = tf_buf;
		if (ctx->photometric == PHOTOMETRIC_MINISWHITE) {
			for (cp += cc; --cp >= tf_buf;)
				*cp = ~*cp;
			cp++;
		}
		if (ctx->ascii85) {
#if defined( EXP_ASCII85ENCODER )
			if (ctx->alpha) {
				int adjust, i;
				for (i = 0; i < cc; i+=2) {
					adjust = 255 - cp[i + 1];
				    cp[i / 2] = cp[i] + adjust;
				}
				cc /= 2;
			}

			ascii85_l = Ascii85EncodeBlock(ctx, ascii85_p, 1, cp,
						       cc);

			if ( ascii85_l > 0 )
			    fwrite( ascii85_p, ascii85_l, 1, ctx->fd );
#else
			while (cc-- > 0)
				Ascii85Put(ctx, *cp++);
#endif /* EXP_ASCII85_ENCODER */
		} else {
			unsigned char c;

			if (ctx->alpha) {
				int adjust;
				while (cc-- > 0) {
					DOBREAK(breaklen, 1, ctx->fd);
					/*
					 * For images with ctx->alpha, matte against
					 * a white background; i.e.
					 *    Cback * (1 - Aimage)
					 * where Cback = 1.
					 */
					adjust = 255 - cp[1];
					c = *cp++ + adjust; PUTHEX(c,ctx->fd);
					cp++, cc--;
				}
			} else {
				while (cc-- > 0) {
					c = *cp++;
					DOBREAK(breaklen, 1, ctx->fd);
					PUTHEX(c, ctx->fd);
				}
			}
		}
	}

	if ( !ctx->ascii85 )
	{
	    if ( ctx->level2 || ctx->level3)
		fputs(">\n", ctx->fd);
	}
#if !defined( EXP_ASCII85ENCODER )
	else
	    Ascii85Flush(ctx);
#else
	if ( ascii85_p )
	    _TIFFfree( ascii85_p );
#endif

	_TIFFfree(tf_buf);
}

static void
Ascii85Init(TIFF2PSContext *ctx)
{
	ctx->ascii85breaklen = 2*MAXLINE;
	ctx->ascii85count = 0;
}

static void
Ascii85Encode(unsigned char* raw, char *buf)
{
	uint32_t word;

	word = (((raw[0]<<8)+raw[1])<<16) + (raw[2]<<8) + raw[3];
	if (word != 0L) {
		uint32_t q;
		uint16_t w1;

		q = word / (85L*85*85*85);	/* actually only a byte */
		buf[0] = (char) (q + '!');

		word -= q * (85L*85*85*85); q = word / (85L*85*85);
		buf[1] = (char) (q + '!');

		word -= q * (85L*85*85); q = word / (85*85);
		buf[2] = (char) (q + '!');

		w1 = (uint16_t) (word - q*(85L*85));
		buf[3] = (char) ((w1 / 85) + '!');
		buf[4] = (char) ((w1 % 85) + '!');
		buf[5] = '\0';
	} else
		buf[0] = 'z', buf[1] = '\0';
}

void
Ascii85Put(TIFF2PSContext *ctx, unsigned char code)
{
	ctx->ascii85buf[ctx->ascii85count++] = code;
	if (ctx->ascii85count >= 4) {
		unsigned char* p;
		int n;
		char buf[6];

		for (n = ctx->ascii85count, p = ctx->ascii85buf;
		     n >= 4; n -= 4, p += 4) {
			char* cp;
			Ascii85Encode(p, buf);
			for (cp = buf; *cp; cp++) {
				putc(*cp, ctx->fd);
				if (--ctx->ascii85breaklen == 0) {
					putc('\n', ctx->fd);
					ctx->ascii85breaklen = 2*MAXLINE;
				}
			}
		}
		_TIFFmemcpy(ctx->ascii85buf, p, n);
		ctx->ascii85count = n;
	}
}

void
Ascii85Flush(TIFF2PSContext* ctx)
{
	if (ctx->ascii85count > 0) {
		char res[6];
		_TIFFmemset(&ctx->ascii85buf[ctx->ascii85count], 0, 3);
		Ascii85Encode(ctx->ascii85buf, res);
		fwrite(res[0] == 'z' ? "!!!!" : res, ctx->ascii85count + 1, 1, ctx->fd);
	}
	fputs("~>\n", ctx->fd);
}
#if	defined( EXP_ASCII85ENCODER)

#define A85BREAKCNTR    ctx->ascii85breaklen
#define A85BREAKLEN     (2*MAXLINE)

/*****************************************************************************
*
* Name:         Ascii85EncodeBlock( ascii85_p, f_eod, raw_p, raw_l )
*
* Description:  This routine will encode the raw data in the buffer described
*               by raw_p and raw_l into ASCII85 format and store the encoding
*               in the buffer given by ascii85_p.
*
* Parameters:   ctx         -   TIFF2PS context
*               ascii85_p   -   A buffer supplied by the caller which will
*                               contain the encoded ASCII85 data.
*               f_eod       -   Flag: Nz means to end the encoded buffer with
*                               an End-Of-Data marker.
*               raw_p       -   Pointer to the buffer of data to be encoded
*               raw_l       -   Number of bytes in raw_p[] to be encoded
*
* Returns:      (int)   <   0   Error, see errno
*                       >=  0   Number of bytes written to ascii85_p[].
*
* Notes:        An external variable given by A85BREAKCNTR is used to
*               determine when to insert newline characters into the
*               encoded data.  As each byte is placed into ascii85_p this
*               external is decremented.  If the variable is decrement to
*               or past zero then a newline is inserted into ascii85_p
*               and the A85BREAKCNTR is then reset to A85BREAKLEN.
*                   Note:  for efficiency reasons the A85BREAKCNTR variable
*                          is not actually checked on *every* character
*                          placed into ascii85_p but often only for every
*                          5 characters.
*
*               THE CALLER IS RESPONSIBLE FOR ENSURING THAT ASCII85_P[] IS
*               SUFFICIENTLY LARGE TO THE ENCODED DATA!
*                   You will need at least 5 * (raw_l/4) bytes plus space for
*                   newline characters and space for an EOD marker (if
*                   requested).  A safe calculation is to use 6*(raw_l/4) + 8
*                   to size ascii85_p.
*
*****************************************************************************/

int Ascii85EncodeBlock( TIFF2PSContext *ctx, uint8_t * ascii85_p,
			unsigned f_eod, const uint8_t * raw_p, int raw_l )

{
    char                        ascii85[5];     /* Encoded 5 tuple */
    int                         ascii85_l;      /* Number of bytes written to ascii85_p[] */
    int                         rc;             /* Return code */
    uint32_t                      val32;          /* Unencoded 4 tuple */

    ascii85_l = 0;                              /* Nothing written yet */

    if ( raw_p )
    {
        --raw_p;                                /* Prepare for pre-increment fetches */

        for ( ; raw_l > 3; raw_l -= 4 )
        {
            val32  = *(++raw_p) << 24;
            val32 += *(++raw_p) << 16;
            val32 += *(++raw_p) <<  8;
            val32 += *(++raw_p);

            if ( val32 == 0 )                   /* Special case */
            {
                ascii85_p[ascii85_l] = 'z';
                rc = 1;
            }

            else
            {
                ascii85[4] = (char) ((val32 % 85) + 33);
                val32 /= 85;

                ascii85[3] = (char) ((val32 % 85) + 33);
                val32 /= 85;

                ascii85[2] = (char) ((val32 % 85) + 33);
                val32 /= 85;

                ascii85[1] = (char) ((val32 % 85) + 33);
                ascii85[0] = (char) ((val32 / 85) + 33);

                _TIFFmemcpy( &ascii85_p[ascii85_l], ascii85, sizeof(ascii85) );
                rc = sizeof(ascii85);
            }

            ascii85_l += rc;

            if ( (A85BREAKCNTR -= rc) <= 0 )
            {
                ascii85_p[ascii85_l] = '\n';
                ++ascii85_l;
                A85BREAKCNTR = A85BREAKLEN;
            }
        }

        /*
         * Output any straggler bytes:
         */

        if ( raw_l > 0 )
        {
            int             len;                /* Output this many bytes */

            len = raw_l + 1;
            val32 = *++raw_p << 24;             /* Prime the pump */

            if ( --raw_l > 0 )  val32 += *(++raw_p) << 16;
            if ( --raw_l > 0 )  val32 += *(++raw_p) <<  8;

            val32 /= 85;

            ascii85[3] = (char) ((val32 % 85) + 33);
            val32 /= 85;

            ascii85[2] = (char) ((val32 % 85) + 33);
            val32 /= 85;

            ascii85[1] = (char) ((val32 % 85) + 33);
            ascii85[0] = (char) ((val32 / 85) + 33);

            _TIFFmemcpy( &ascii85_p[ascii85_l], ascii85, len );
            ascii85_l += len;
        }
    }

    /*
     * If requested add an ASCII85 End Of Data marker:
     */

    if ( f_eod )
    {
        ascii85_p[ascii85_l++] = '~';
        ascii85_p[ascii85_l++] = '>';
        ascii85_p[ascii85_l++] = '\n';
    }

    return ( ascii85_l );

}   /* Ascii85EncodeBlock() */

#endif	/* EXP_ASCII85ENCODER */

/* vim: set ts=8 sts=8 sw=8 noet: */
