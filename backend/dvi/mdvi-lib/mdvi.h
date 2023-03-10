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
#ifndef _MDVI_DVI_H
#define _MDVI_DVI_H 1

#include <stdio.h>
#include <sys/types.h>
#include <math.h>

#include "sysdeps.h"
#include "bitmap.h"
#include "common.h"
#include "defaults.h"
#include "dviopcodes.h"

typedef struct _DviGlyph DviGlyph;
typedef struct _DviDevice DviDevice;
typedef struct _DviFontChar DviFontChar;
typedef struct _DviFontRef DviFontRef;
typedef struct _DviFontInfo DviFontInfo;
typedef struct _DviFont DviFont;
typedef struct _DviState DviState;
typedef struct _DviPageSpec *DviPageSpec;
typedef struct _DviParams DviParams;
typedef struct _DviBuffer DviBuffer;
typedef struct _DviContext DviContext;
typedef struct _DviRange DviRange;
typedef struct _DviColorPair DviColorPair;
typedef struct _DviSection DviSection;
typedef struct _TFMChar TFMChar;
typedef struct _TFMInfo TFMInfo;
typedef struct _DviFontSearch DviFontSearch;
/* this is an opaque type */
typedef struct _DviFontClass DviFontClass;

typedef void (*DviFreeFunc) __PROTO((void *));
typedef void (*DviFree2Func) __PROTO((void *, void *));

typedef Ulong	DviColor;

#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif

typedef enum {
	FALSE	= 0,
	TRUE	= 1
} DviBool;

#include "hash.h"
#include "paper.h"

/*
 * information about a page:
 *   pagenum[0] = offset to BOP
 *   pagenum[1], ..., pagenum[10] = TeX \counters
 */
typedef long	PageNum[11];

/* this structure contains the platform-specific information
 * required to interpret a DVI file */

typedef void (*DviGlyphDraw)	__PROTO((DviContext *context,
				          DviFontChar *glyph,
				          int x, int y));

typedef void (*DviRuleDraw)	__PROTO((DviContext *context,
				          int x, int y,
				          Uint width, Uint height, int fill));

typedef int (*DviColorScale) 	__PROTO((void *device_data,
				         Ulong *pixels,
				         int npixels,
				         Ulong foreground,
				         Ulong background,
				         double gamma,
				         int density));
typedef void *(*DviCreateImage)	__PROTO((void *device_data,
				         Uint width,
				         Uint height,
				         Uint bpp));
typedef void (*DviFreeImage)	__PROTO((void *image));
typedef void (*DviPutPixel)	__PROTO((void *image, int x, int y, Ulong color));
typedef void (*DviImageDone)    __PROTO((void *image));
typedef void (*DviDevDestroy)   __PROTO((void *data));
typedef void (*DviRefresh)      __PROTO((DviContext *dvi, void *device_data));
typedef void (*DviSetColor)	__PROTO((void *device_data, Ulong, Ulong));
typedef void (*DviPSDraw)       __PROTO((DviContext *context,
					 const char *filename,
					 int x, int y,
					 Uint width, Uint height));

struct _DviDevice {
	DviGlyphDraw	draw_glyph;
	DviRuleDraw	draw_rule;
	DviColorScale	alloc_colors;
	DviCreateImage	create_image;
	DviFreeImage	free_image;
	DviPutPixel	put_pixel;
        DviImageDone    image_done;
	DviDevDestroy	dev_destroy;
	DviRefresh	refresh;
	DviSetColor	set_color;
	DviPSDraw       draw_ps;
	void *		device_data;
};

/*
 * Fonts
 */

#include "fontmap.h"

struct _TFMChar {
	Int32	present;
	Int32	advance;	/* advance */
	Int32	height;		/* ascent */
	Int32	depth;		/* descent */
	Int32	left;		/* leftSideBearing */
	Int32	right;		/* rightSideBearing */
};

struct _TFMInfo {
	int	type; /* DviFontAFM, DviFontTFM, DviFontOFM */
	Uint32	checksum;
	Uint32	design;
	int	loc;
	int	hic;
	char	coding[64];
	char	family[64];
	TFMChar *chars;
};

struct _DviGlyph {
	short	x, y;	/* origin */
	Uint	w, h;	/* dimensions */
	void	*data;	/* bitmap or XImage */
};

typedef void (*DviFontShrinkFunc)
	__PROTO((DviContext *, DviFont *, DviFontChar *, DviGlyph *));
typedef int (*DviFontLoadFunc) __PROTO((DviParams *, DviFont *));
typedef int (*DviFontGetGlyphFunc) __PROTO((DviParams *, DviFont *, int));
typedef void (*DviFontFreeFunc) __PROTO((DviFont *));
typedef void (*DviFontResetFunc) __PROTO((DviFont *));
typedef char *(*DviFontLookupFunc) __PROTO((const char *, Ushort *, Ushort *));
typedef int (*DviFontEncodeFunc) __PROTO((DviParams *, DviFont *, DviEncoding *));

struct _DviFontInfo {
	char	*name;	/* human-readable format identifying string */
	int	scalable; /* does it support scaling natively? */
	DviFontLoadFunc		load;
	DviFontGetGlyphFunc	getglyph;
	DviFontShrinkFunc 	shrink0;
	DviFontShrinkFunc 	shrink1;
	DviFontFreeFunc		freedata;
	DviFontResetFunc	reset;
	DviFontLookupFunc	lookup;
	int			kpse_type;
	void *			private;
};

struct _DviFontChar {
	Uint32	offset;
	Int16	code;		/* format-dependent, not used by MDVI */
	Int16	width;
	Int16	height;
	Int16	x;
	Int16	y;
	Int32	tfmwidth;
	Ushort	flags;
#ifdef __STRICT_ANSI__
	Ushort	loaded;
	Ushort	missing;
#else
	Ushort	loaded : 1,
		missing : 1;
#endif
	Ulong	fg;
	Ulong	bg;
	BITMAP	*glyph_data;
	/* data for shrunk bitimaps */
	DviGlyph glyph;
	DviGlyph shrunk;
	DviGlyph grey;
};

struct _DviFontRef {
	DviFontRef *next;
	DviFont	*ref;
	Int32	fontid;
};

typedef enum {
	DviFontAny  = -1,
	DviFontPK   = 0,
	DviFontGF   = 1,
	DviFontVF   = 2,
	DviFontTFM  = 3,
	DviFontT1   = 4,
	DviFontTT   = 5,
	DviFontAFM  = 6,
	DviFontOFM  = 7
} DviFontType;

struct _DviFontSearch {
	int	id;
	Ushort	hdpi;
	Ushort	vdpi;
	Ushort	actual_hdpi;
	Ushort	actual_vdpi;
	const char *wanted_name;
	const char *actual_name;
	DviFontClass *curr;
	DviFontInfo  *info;
};

/* this is a kludge, I know */
#define ISVIRTUAL(font)	((font)->search.info->getglyph == NULL)
#define SEARCH_DONE(s)	((s).id < 0)
#define SEARCH_INIT(s, name, h, v) do { \
	(s).id = 0; \
	(s).curr = NULL; \
	(s).hdpi = (h); \
	(s).vdpi = (v); \
	(s).wanted_name = (name); \
	(s).actual_name = NULL; \
	} while(0)

struct _DviFont {
	DviFont *next;
	DviFont *prev;
	int	type;
	Int32	checksum;
	int	hdpi;
	int	vdpi;
	Int32	scale;
	Int32	design;
	FILE	*in;
	char	*fontname;
	char	*filename;
	int	links;
	int	loc;
	int	hic;
	Uint	flags;
	DviFontSearch	search;
	DviFontChar	*chars;
	DviFontRef	*subfonts;
	void	*private;
};

/*
 * Dvi context
 */

typedef enum {
	MDVI_ORIENT_TBLR  = 0,	/* top to bottom, left to right */
	MDVI_ORIENT_TBRL  = 1,	/* top to bottom, right to left */
	MDVI_ORIENT_BTLR  = 2,	/* bottom to top, left to right */
	MDVI_ORIENT_BTRL  = 3,	/* bottom to top, right to left */
	MDVI_ORIENT_RP90  = 4,	/* rotated +90 degrees (counter-clockwise) */
	MDVI_ORIENT_RM90  = 5,	/* rotated -90 degrees (clockwise) */
	MDVI_ORIENT_IRP90 = 6,	/* flip horizontally, then rotate by +90 */
	MDVI_ORIENT_IRM90 = 7	/* rotate by -90, then flip horizontally */
} DviOrientation;

typedef enum {
	MDVI_PAGE_SORT_UP,	/* up, using \counter0 */
	MDVI_PAGE_SORT_DOWN,	/* down, using \counter0 */
	MDVI_PAGE_SORT_RANDOM,	/* randomly */
	MDVI_PAGE_SORT_DVI_UP,	/* up, by location in DVI file */
	MDVI_PAGE_SORT_DVI_DOWN,	/* down, by location in DVI file */
	MDVI_PAGE_SORT_NONE	/* don't sort */
} DviPageSort;

struct _DviParams {
	double	mag;		/* magnification */
	double	conv;		/* horizontal DVI -> pixel */
	double	vconv;		/* vertical DVI -> pixel */
	double	tfm_conv;	/* TFM -> DVI */
	double	gamma;		/* gamma correction factor */
	Uint	dpi;		/* horizontal resolution */
	Uint	vdpi;		/* vertical resolution */
	int	hshrink;	/* horizontal shrinking factor */
	int	vshrink;	/* vertical shrinking factor */
	Uint	density;	/* pixel density */
	Uint	flags;		/* flags (see MDVI_PARAM macros) */
	int	hdrift;		/* max. horizontal drift */
	int	vdrift;		/* max. vertical drift */
	int	vsmallsp;	/* small vertical space */
	int	thinsp;		/* small horizontal space */
	int	layer;		/* visible layer (for layered DVI files) */
	Ulong	fg;		/* foreground color */
	Ulong	bg;		/* background color */
	DviOrientation	orientation;	/* page orientation */
	int	base_x;
	int	base_y;
};

typedef enum {
	MDVI_PARAM_LAST		= 0,
	MDVI_SET_DPI    	= 1,
	MDVI_SET_XDPI   	= 2,
	MDVI_SET_YDPI   	= 3,
	MDVI_SET_SHRINK		= 4,
	MDVI_SET_XSHRINK	= 5,
	MDVI_SET_YSHRINK	= 6,
	MDVI_SET_GAMMA		= 7,
	MDVI_SET_DENSITY	= 8,
	MDVI_SET_MAGNIFICATION	= 9,
	MDVI_SET_DRIFT		= 10,
	MDVI_SET_HDRIFT		= 11,
	MDVI_SET_VDRIFT		= 12,
	MDVI_SET_ORIENTATION	= 13,
	MDVI_SET_FOREGROUND	= 14,
	MDVI_SET_BACKGROUND	= 15
} DviParamCode;

struct _DviBuffer {
	Uchar	*data;
	size_t	size;		/* allocated size */
	size_t	length;		/* amount of data buffered */
	size_t	pos;		/* current position in buffer */
	int	frozen;		/* can we free this data? */
};

/* DVI registers */
struct _DviState {
	int	h;
	int	v;
	int	hh;
	int	vv;
	int	w;
	int	x;
	int	y;
	int	z;
};

struct _DviColorPair {
	Ulong	fg;
	Ulong	bg;
};

struct _DviContext {
	char	*filename;	/* name of the DVI file */
	FILE	*in;		/* from here we read */
	char	*fileid;	/* from preamble */
	int	npages;		/* number of pages */
	int	currpage;	/* current page (0 based) */
	int	depth;		/* recursion depth */
	DviBuffer buffer;	/* input buffer */
	DviParams params;	/* parameters */
	DviPaper  paper;	/* paper type */
	Int32	num;		/* numerator */
	Int32	den;		/* denominator */
	DviFontRef *fonts;	/* fonts used in this file */
	DviFontRef **fontmap;	/* for faster id lookups */
	DviFontRef *currfont;	/* current font */
	int	nfonts;		/* # of fonts used in this job */
	Int32	dvimag;		/* original magnification */
	double	dviconv;	/* unshrunk scaling factor */
	double	dvivconv;	/* unshrunk scaling factor (vertical) */
	int	dvi_page_w;	/* unscaled page width */
	int	dvi_page_h;	/* unscaled page height */
	Ulong	modtime;	/* file modification time */
	PageNum	*pagemap;	/* page table */
	DviState pos;		/* registers */
	DviPageSpec *pagesel;	/* page selection data */
	int	curr_layer;	/* current layer */
	DviState *stack;	/* DVI stack */
	int	stacksize;	/* stack depth */
	int	stacktop;	/* stack pointer */
	DviDevice device;	/* device-specific routines */
	Ulong	curr_fg;	/* rendering color */
	Ulong	curr_bg;

	DviColorPair *color_stack;
	int	color_top;
	int	color_size;

	DviFontRef *(*findref) __PROTO((DviContext *, Int32));
	void	*user_data;	/* client data attached to this context */
};

typedef enum {
	MDVI_RANGE_BOUNDED,	/* range is finite */
	MDVI_RANGE_LOWER,	/* range has a lower bound */
	MDVI_RANGE_UPPER,	/* range has an upper bound */
	MDVI_RANGE_UNBOUNDED	/* range has no bounds at all */
} DviRangeType;

struct _DviRange {
	DviRangeType type;	/* one of the above */
	int	from;		/* lower bound */
	int	to;		/* upper bound */
	int	step;		/* step */
};


typedef void (*DviSpecialHandler)
	__PROTO((DviContext *dvi, const char *prefix, const char *arg));

#define RANGE_HAS_LOWER(x) \
	((x) == MDVI_RANGE_BOUNDED || (x) == MDVI_RANGE_LOWER)
#define RANGE_HAS_UPPER(x) \
	((x) == MDVI_RANGE_BOUNDED || (x) == MDVI_RANGE_UPPER)

/*
 * Macros and prototypes
 */

#define MDVI_PARAM_ANTIALIASED	1
#define MDVI_PARAM_MONO		2
#define MDVI_PARAM_CHARBOXES	4
#define MDVI_PARAM_SHOWUNDEF	8
#define MDVI_PARAM_DELAYFONTS	16

/*
 * The FALLBACK priority class is reserved for font formats that
 * contain no glyph information and are to be used as a last
 * resort (e.g. TFM, AFM)
 */
#define MDVI_FONTPRIO_FALLBACK	-3
#define MDVI_FONTPRIO_LOWEST	-2
#define MDVI_FONTPRIO_LOW	-1
#define MDVI_FONTPRIO_NORMAL	0
#define MDVI_FONTPRIO_HIGH	1
#define MDVI_FONTPRIO_HIGHEST	2

#define MDVI_FONT_ENCODED	(1 << 0)

#define MDVI_GLYPH_EMPTY	((void *)1)
/* does the glyph have a non-empty bitmap/image? */
#define MDVI_GLYPH_NONEMPTY(x)	((x) && (x) != MDVI_GLYPH_EMPTY)
/* has the glyph been loaded from disk? */
#define MDVI_GLYPH_UNSET(x)	((x) == NULL)
/* do we have only a bounding box for this glyph? */
#define MDVI_GLYPH_ISEMPTY(x)	((x) == MDVI_GLYPH_EMPTY)

#define MDVI_ENABLED(d,x)	((d)->params.flags & (x))
#define MDVI_DISABLED(d,x)	!MDVI_ENABLED((d), (x))

#define MDVI_LASTPAGE(d)	((d)->npages - 1)
#define MDVI_NPAGES(d)		(d)->npages
#define MDVI_VALIDPAGE(d,p)	((p) >= 0 && (p) <= MDVI_LASTPAGE(d))
#define MDVI_FLAGS(d)		(d)->params.flags
#define MDVI_SHRINK_FROM_DPI(d)	Max(1, (d) / 75)
#define MDVI_CURRFG(d)		(d)->curr_fg
#define MDVI_CURRBG(d)		(d)->curr_bg

#define pixel_round(d,v)	(int)((d)->params.conv * (v) + 0.5)
#define vpixel_round(d,v)	(int)((d)->params.vconv * (v) + 0.5)
#define rule_round(d,v)		(int)((d)->params.conv * (v) + 0.99999) /*9999999)*/
#define vrule_round(d,v)	(int)((d)->params.vconv * (v) + 0.99999)

extern int	mdvi_reload __PROTO((DviContext *, DviParams *));
extern void	mdvi_setpage __PROTO((DviContext *, int));
extern int  	mdvi_dopage __PROTO((DviContext *, int));
extern void 	mdvi_shrink_glyph __PROTO((DviContext *, DviFont *, DviFontChar *, DviGlyph *));
extern void	mdvi_shrink_box __PROTO((DviContext *, DviFont *, DviFontChar *, DviGlyph *));
extern void 	mdvi_shrink_glyph_grey __PROTO((DviContext *, DviFont *, DviFontChar *, DviGlyph *));
extern int	mdvi_find_tex_page __PROTO((DviContext *, int));
extern int	mdvi_configure __PROTO((DviContext *, DviParamCode, ...));

extern int	get_tfm_chars __PROTO((DviParams *, DviFont *, TFMInfo *, int));
extern int 	tfm_load_file __PROTO((const char *, TFMInfo *));
extern int	afm_load_file __PROTO((const char *, TFMInfo *));
extern TFMInfo *get_font_metrics __PROTO((const char *, int, const char *));
extern char    *lookup_font_metrics __PROTO((const char *, int *));
extern void	free_font_metrics __PROTO((TFMInfo *));
extern void	flush_font_metrics __PROTO((void));

#define get_metrics(name)	get_font_metrics((name), DviFontAny, NULL)

extern void	mdvi_sort_pages __PROTO((DviContext *, DviPageSort));

extern void mdvi_init_kpathsea __PROTO((const char *, const char *, const char *, int, const char *));

extern DviContext* mdvi_init_context __PROTO((DviParams *, DviPageSpec *, const char *));
extern void 	mdvi_destroy_context __PROTO((DviContext *));

/* helper macros that call mdvi_configure() */
#define mdvi_config_one(d,x,y)	mdvi_configure((d), (x), (y), MDVI_PARAM_LAST)
#define mdvi_set_dpi(d,x)	mdvi_config_one((d), MDVI_SET_DPI, (x))
#define mdvi_set_xdpi(d,x)	mdvi_config_one((d), MDVI_SET_XDPI, (x))
#define mdvi_set_ydpi(d,x)	mdvi_config_one((d), MDVI_SET_YDPI, (x))
#define mdvi_set_hshrink(d,h)	mdvi_config_one((d), MDVI_SET_XSHRINK, (h))
#define mdvi_set_vshrink(d,h)	mdvi_config_one((d), MDVI_SET_YSHRINK, (h))
#define mdvi_set_gamma(d,g)	mdvi_config_one((d), MDVI_SET_GAMMA, (g))
#define mdvi_set_density(d,x)	mdvi_config_one((d), MDVI_SET_DENSITY, (x))
#define mdvi_set_drift(d,x)	mdvi_config_one((d), MDVI_SET_DRIFT, (x))
#define mdvi_set_hdrift(d,h)	mdvi_config_one((d), MDVI_SET_HDRIFT, (h))
#define mdvi_set_vdrift(d,v)	mdvi_config_one((d), MDVI_SET_VDRIFT, (v))
#define mdvi_set_mag(d,m) \
	mdvi_config_one((d), MDVI_SET_MAGNIFICATION, (m))
#define mdvi_set_foreground(d,x) \
	mdvi_config_one((d), MDVI_SET_FOREGROUND, (x))
#define mdvi_set_background(d,x) \
	mdvi_config_one((d), MDVI_SET_BACKGROUND, (x))
#define mdvi_set_orientation(d,x) \
	mdvi_config_one((d), MDVI_SET_ORIENTATION, (x))
#define mdvi_set_shrink(d,h,v)	\
	mdvi_configure((d), MDVI_SET_XSHRINK, (h), \
	MDVI_SET_YSHRINK, (v), MDVI_PARAM_LAST)

extern DviRange* mdvi_parse_range __PROTO((const char *, DviRange *, int *, char **));
extern DviPageSpec* mdvi_parse_page_spec __PROTO((const char *));
extern void mdvi_free_page_spec __PROTO((DviPageSpec *));
extern int mdvi_in_range __PROTO((DviRange *, int, int));
extern int mdvi_range_length __PROTO((DviRange *, int));
extern int mdvi_page_selected __PROTO((DviPageSpec *, PageNum, int));

/* Specials */
extern int mdvi_register_special __PROTO((
	const char *label,
	const char *prefix,
	const char *regex,
	DviSpecialHandler handler,
	int replace));
extern int mdvi_unregister_special __PROTO((const char *prefix));
extern int mdvi_do_special __PROTO((DviContext *dvi, char *dvi_special));
extern void mdvi_flush_specials __PROTO((void));

/* Fonts */

#define MDVI_FONTSEL_BITMAP	(1 << 0)
#define MDVI_FONTSEL_GREY	(1 << 1)
#define MDVI_FONTSEL_GLYPH	(1 << 2)

#define FONTCHAR(font, code)	\
	(((code) < font->loc || (code) > font->hic || !(font)->chars) ? \
		NULL : &font->chars[(code) - (font)->loc])
#define FONT_GLYPH_COUNT(font) ((font)->hic - (font)->loc + 1)

#define glyph_present(x) ((x) && (x)->offset)

/* create a reference to a font */
extern DviFontRef *font_reference __PROTO((DviParams *params,
                                           Int32 dvi_id,
                                           const char *font_name,
                                           Int32 checksum,
                                           int xdpi,
                                           int ydpi,
                                           Int32 scale_factor));

/* drop a reference to a font */
extern void font_drop_one __PROTO((DviFontRef *));

/* drop a chain of references */
extern void font_drop_chain __PROTO((DviFontRef *));

/* destroy selected information for a glyph */
extern void font_reset_one_glyph __PROTO((DviDevice *, DviFontChar *, int));

/* destroy selected information for all glyphs in a font */
extern void font_reset_font_glyphs __PROTO((DviDevice *, DviFont *, int));

/* same for a chain of font references */
extern void font_reset_chain_glyphs __PROTO((DviDevice *, DviFontRef *, int));

extern void font_finish_definitions __PROTO((DviContext *));

/* lookup an id # in a reference chain */
extern DviFontRef* font_find_flat __PROTO((DviContext *, Int32));
extern DviFontRef* font_find_mapped __PROTO((DviContext *, Int32));

/* called to reopen (or rewind) a font file */
extern int font_reopen __PROTO((DviFont *));

/* reads a glyph from a font, and makes all necessary transformations */
extern DviFontChar* font_get_glyph __PROTO((DviContext *, DviFont *, int));

/* transform a glyph according to the given orientation */
extern void font_transform_glyph __PROTO((DviOrientation, DviGlyph *));

/* destroy all fonts that are not being used, returns number of fonts freed */
extern int font_free_unused __PROTO((DviDevice *));

#define font_free_glyph(dev, font, code) \
	font_reset_one_glyph((dev), \
	FONTCHAR((font), (code)), MDVI_FONTSEL_GLYPH)

extern int mdvi_encode_font __PROTO((DviParams *, DviFont *));


/* font lookup functions */
extern int mdvi_register_font_type __PROTO((DviFontInfo *, int));
extern char **mdvi_list_font_class __PROTO((int));
extern int mdvi_get_font_classes __PROTO((void));
extern int mdvi_unregister_font_type __PROTO((const char *, int));
extern char *mdvi_lookup_font __PROTO((DviFontSearch *));
extern DviFont *mdvi_add_font __PROTO((const char *, Int32, int, int, Int32));
extern int mdvi_font_retry __PROTO((DviParams *, DviFont *));

/* Miscellaneous */

extern int mdvi_set_logfile __PROTO((const char *));
extern int mdvi_set_logstream __PROTO((FILE *));
extern int mdvi_set_loglevel __PROTO((int));

#define mdvi_stop_logging(x) mdvi_set_logstream(NULL)

/* this will check the environment and then `texmf.cnf' for
 * the given name changed to lowercase, and `_' changed to `-' */
extern char* mdvi_getenv __PROTO((const char *));

#endif /* _MDVI_DVI_H */
