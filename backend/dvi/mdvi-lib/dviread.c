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
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "mdvi.h"
#include "private.h"
#include "color.h"

typedef int (*DviCommand) __PROTO((DviContext *, int));

#define DVICMDDEF(x)	static int x __PROTO((DviContext *, int))

DVICMDDEF(set_char);
DVICMDDEF(set_rule);
DVICMDDEF(no_op);
DVICMDDEF(push);
DVICMDDEF(pop);
DVICMDDEF(move_right);
DVICMDDEF(move_down);
DVICMDDEF(move_w);
DVICMDDEF(move_x);
DVICMDDEF(move_y);
DVICMDDEF(move_z);
DVICMDDEF(sel_font);
DVICMDDEF(sel_fontn);
DVICMDDEF(special);
DVICMDDEF(def_font);
DVICMDDEF(undefined);
DVICMDDEF(unexpected);

static const DviCommand dvi_commands[] = {
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,
	set_char, set_char, set_char, set_char,	/* 0 - 127 */
	set_char, set_char, set_char, set_char, /* 128 - 131 */
	set_rule,				/* 132 */
	set_char, set_char, set_char, set_char, /* 133 - 136 */
	set_rule,				/* 137 */
	no_op,					/* 138 */
	unexpected,				/* 139 (BOP) */
	unexpected,				/* 140 (EOP) */
	push,					/* 141 */
	pop,					/* 142 */
	move_right, move_right, move_right, move_right,	/* 143 - 146 */
	move_w, move_w, move_w, move_w,	move_w,	/* 147 - 151 */
	move_x, move_x, move_x, move_x, move_x,	/* 152 - 156 */
	move_down, move_down, move_down, move_down,	/* 157 - 160 */
	move_y, move_y, move_y, move_y, move_y,	/* 161 - 165 */
	move_z, move_z, move_z, move_z, move_z,	/* 166 - 170 */
	sel_font, sel_font, sel_font, sel_font,
	sel_font, sel_font, sel_font, sel_font,
	sel_font, sel_font, sel_font, sel_font,
	sel_font, sel_font, sel_font, sel_font,
	sel_font, sel_font, sel_font, sel_font,
	sel_font, sel_font, sel_font, sel_font,
	sel_font, sel_font, sel_font, sel_font,
	sel_font, sel_font, sel_font, sel_font,
	sel_font, sel_font, sel_font, sel_font,
	sel_font, sel_font, sel_font, sel_font,
	sel_font, sel_font, sel_font, sel_font,
	sel_font, sel_font, sel_font, sel_font,
	sel_font, sel_font, sel_font, sel_font,
	sel_font, sel_font, sel_font, sel_font,
	sel_font, sel_font, sel_font, sel_font,
	sel_font, sel_font, sel_font, sel_font,	/* 171 - 234 */
	sel_fontn, sel_fontn, sel_fontn, sel_fontn, 	/* 235 - 238 */
	special, special, special, special,	/* 239 - 242 */
	def_font, def_font, def_font, def_font,	/* 243 - 246 */
	unexpected,				/* 247 (PRE) */
	unexpected,				/* 248 (POST) */
	unexpected,				/* 249 (POST_POST) */
	undefined, undefined, undefined,
	undefined, undefined, undefined		/* 250 - 255 */
};

#define DVI_BUFLEN	4096

static int	mdvi_run_macro(DviContext *dvi, Uchar *macro, size_t len);

static void dummy_draw_glyph(DviContext *dvi, DviFontChar *ch, int x, int y)
{
}

static void dummy_draw_rule(DviContext *dvi, int x, int y, Uint w, Uint h, int f)
{
}

static int dummy_alloc_colors(void *a, Ulong *b, int c, Ulong d, Ulong e, double f, int g)
{
	return -1;
}

static void *dummy_create_image(void *a, Uint b, Uint c, Uint d)
{
	return NULL;
}

static void dummy_free_image(void *a)
{
}

static void dummy_dev_destroy(void *a)
{
}

static void dummy_dev_putpixel(void *a, int x, int y, Ulong c)
{
}

static void dummy_dev_refresh(DviContext *a, void *b)
{
}

static void dummy_dev_set_color(void *a, Ulong b, Ulong c)
{
}

/* functions to report errors */
__attribute__((__format__ (__printf__, 2, 3)))
static void dvierr(DviContext *dvi, const char *format, ...)
{
	va_list	ap;

	va_start(ap, format);
	fprintf(stderr, "%s[%d]: Error: ",
		dvi->filename, dvi->currpage);
	vfprintf(stderr, format, ap);
	va_end(ap);
}

__attribute__((__format__ (__printf__, 2, 3)))
static void dviwarn(DviContext *dvi, const char *format, ...)
{
	va_list	ap;

	fprintf(stderr, "%s[%d]: Warning: ",
		dvi->filename, dvi->currpage);
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
}

#define NEEDBYTES(d,n) \
	((d)->buffer.pos + (n) > (d)->buffer.length)

static int get_bytes(DviContext *dvi, size_t n)
{
	/*
	 * caller wants to read `n' bytes from dvi->buffer + dvi->pos.
	 * Make sure there is enough data to satisfy the request
	 */
	if(NEEDBYTES(dvi, n)) {
		size_t	required;
		int	newlen;

		if(dvi->buffer.frozen || dvi->in == NULL || feof(dvi->in)) {
			/* this is EOF */
			dviwarn(dvi, _("unexpected EOF\n"));
			return -1;
		}
		/* get more data */
		if(dvi->buffer.data == NULL) {
			/* first allocation */
			dvi->buffer.size = Max(DVI_BUFLEN, n);
			dvi->buffer.data = (Uchar *)mdvi_malloc(dvi->buffer.size);
			dvi->buffer.length = 0;
			dvi->buffer.frozen = 0;
		} else if(dvi->buffer.pos < dvi->buffer.length) {
			/* move whatever we want to keep */
			dvi->buffer.length -= dvi->buffer.pos;
			memmove(dvi->buffer.data,
				dvi->buffer.data + dvi->buffer.pos,
				dvi->buffer.length);
		} else {
			/* we can discard all the data in this buffer */
			dvi->buffer.length = 0;
		}

		required = n - dvi->buffer.length;
		if(required > dvi->buffer.size - dvi->buffer.length) {
			/* need to allocate more memory */
			dvi->buffer.size = dvi->buffer.length + required + 128;
			dvi->buffer.data = (Uchar *)xresize(dvi->buffer.data,
				char, dvi->buffer.size);
		}
		/* now read into the buffer */
		newlen = fread(dvi->buffer.data + dvi->buffer.length,
			1, dvi->buffer.size - dvi->buffer.length, dvi->in);
		if(newlen == -1) {
			mdvi_error("%s: %s\n", dvi->filename, strerror(errno));
			return -1;
		}
		dvi->buffer.length += newlen;
		dvi->buffer.pos = 0;
	}
	return 0;
}

/* only relative forward seeks are supported by this function */
static int dskip(DviContext *dvi, long offset)
{
	ASSERT(offset > 0);

	if(NEEDBYTES(dvi, offset) && get_bytes(dvi, offset) == -1)
		return -1;
	dvi->buffer.pos += offset;
	return 0;
}

/* DVI I/O functions (note: here `n' must be <= 4) */
static long dsgetn(DviContext *dvi, size_t n)
{
	long	val;

	if(NEEDBYTES(dvi, n) && get_bytes(dvi, n) == -1)
		return -1;
	val = msgetn(dvi->buffer.data + dvi->buffer.pos, n);
	dvi->buffer.pos += n;
	return val;
}

static int dread(DviContext *dvi, char *buffer, size_t len)
{
	if(NEEDBYTES(dvi, len) && get_bytes(dvi, len) == -1)
		return -1;
	memcpy(buffer, dvi->buffer.data + dvi->buffer.pos, len);
	dvi->buffer.pos += len;
	return 0;
}

static long dugetn(DviContext *dvi, size_t n)
{
	long	val;

	if(NEEDBYTES(dvi, n) && get_bytes(dvi, n) == -1)
		return -1;
	val = mugetn(dvi->buffer.data + dvi->buffer.pos, n);
	dvi->buffer.pos += n;
	return val;
}

static long dtell(DviContext *dvi)
{
	return dvi->depth ?
		dvi->buffer.pos :
		ftell(dvi->in) - dvi->buffer.length + dvi->buffer.pos;
}

static void dreset(DviContext *dvi)
{
	if(!dvi->buffer.frozen && dvi->buffer.data)
		mdvi_free(dvi->buffer.data);
	dvi->buffer.data = NULL;
	dvi->buffer.size = 0;
	dvi->buffer.length = 0;
	dvi->buffer.pos = 0;
}

#define dsget1(d)	dsgetn((d), 1)
#define dsget2(d)	dsgetn((d), 2)
#define dsget3(d)	dsgetn((d), 3)
#define dsget4(d)	dsgetn((d), 4)
#define duget1(d)	dugetn((d), 1)
#define duget2(d)	dugetn((d), 2)
#define duget3(d)	dugetn((d), 3)
#define duget4(d)	dugetn((d), 4)

#ifndef NODEBUG
__attribute__((__format__ (__printf__, 4, 5)))
static void dviprint(DviContext *dvi, const char *command, int sub, const char *fmt, ...)
{
	int	i;
	va_list	ap;

	printf("%s: ", dvi->filename);
	for(i = 0; i < dvi->depth; i++)
		printf("  ");
	printf("%4lu: %s", dtell(dvi), command);
	if(sub >= 0) printf("%d", sub);
	if(*fmt) printf(": ");
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}
#define SHOWCMD(x)	\
	if(_mdvi_debug_mask & DBG_OPCODE) do { dviprint x; } while(0)
#else
#define SHOWCMD(x)	do { } while(0)
#endif

int	mdvi_find_tex_page(DviContext *dvi, int tex_page)
{
	int	i;

	for(i = 0; i < dvi->npages; i++)
		if(dvi->pagemap[i][1] == tex_page)
			return i;
	return -1;
}

/* page sorting functions */
static int sort_up(const void *p1, const void *p2)
{
	return ((long *)p1)[1] - ((long *)p2)[1];
}
static int sort_down(const void *p1, const void *p2)
{
	return ((long *)p2)[1] - ((long *)p1)[1];
}
static int sort_random(const void *p1, const void *p2)
{
	return (rand() % 1) ? -1 : 1;
}
static int sort_dvi_up(const void *p1, const void *p2)
{
	return ((long *)p1)[0] - ((long *)p2)[0];
}
static int sort_dvi_down(const void *p1, const void *p2)
{
	return ((long *)p1)[0] - ((long *)p2)[0];
}

void	mdvi_sort_pages(DviContext *dvi, DviPageSort type)
{
	int	(*sortfunc) __PROTO((const void *, const void *));

	switch(type) {
	case MDVI_PAGE_SORT_UP:
		sortfunc = sort_up;
		break;
	case MDVI_PAGE_SORT_DOWN:
		sortfunc = sort_down;
		break;
	case MDVI_PAGE_SORT_RANDOM:
		sortfunc = sort_random;
		break;
	case MDVI_PAGE_SORT_DVI_UP:
		sortfunc = sort_dvi_up;
		break;
	case MDVI_PAGE_SORT_DVI_DOWN:
		sortfunc = sort_dvi_down;
		break;
	case MDVI_PAGE_SORT_NONE:
	default:
		sortfunc = NULL;
		break;
	}

	if(sortfunc)
		qsort(dvi->pagemap, dvi->npages, sizeof(PageNum), sortfunc);
}

static DviFontRef *define_font(DviContext *dvi, int op)
{
	Int32	arg;
	Int32	scale;
	Int32	dsize;
	Int32	checksum;
	int	hdpi;
	int	vdpi;
	int	n;
	char	*name;
	DviFontRef *ref;

	arg = dugetn(dvi, op - DVI_FNT_DEF1 + 1);
	checksum = duget4(dvi);
	scale = duget4(dvi);
	dsize = duget4(dvi);
	hdpi = FROUND(dvi->params.mag * dvi->params.dpi * scale / dsize);
	vdpi = FROUND(dvi->params.mag * dvi->params.vdpi * scale / dsize);
	n = duget1(dvi) + duget1(dvi);
	name = mdvi_malloc(n + 1);
	dread(dvi, name, n);
	name[n] = 0;
	DEBUG((DBG_FONTS, "requesting font %d = `%s' at %.1fpt (%dx%d dpi)\n",
		arg, name, (double)scale / (dvi->params.tfm_conv * 0x100000),
		hdpi, vdpi));
	ref = font_reference(&dvi->params, arg, name, checksum, hdpi, vdpi, scale);
	if(ref == NULL) {
		mdvi_error(_("could not load font `%s'\n"), name);
		mdvi_free(name);
		return NULL;
	}
	mdvi_free(name);
	return ref;
}

static char *opendvi(const char *name)
{
	int	len;
	char	*file;

	len = strlen(name);
	/* if file ends with .dvi and it exists, that's it */
	if(len >= 4 && STREQ(name+len-4, ".dvi")) {
		DEBUG((DBG_DVI|DBG_FILES, "opendvi: Trying `%s'\n", name));
		if(access(name, R_OK) == 0)
			return mdvi_strdup(name);
	}

	/* try appending .dvi */
	file = mdvi_malloc(len + 5);
	strcpy(file, name);
	strcpy(file+len, ".dvi");
	DEBUG((DBG_DVI|DBG_FILES, "opendvi: Trying `%s'\n", file));
	if(access(file, R_OK) == 0)
		return file;
	/* try the given name */
	file[len] = 0;
	DEBUG((DBG_DVI|DBG_FILES, "opendvi: Trying `%s'\n", file));
	if(access(file, R_OK) == 0)
		return file;
	mdvi_free(file);
	return NULL;
}

int	mdvi_reload(DviContext *dvi, DviParams *np)
{
	DviContext *newdvi;
	DviParams  *pars;

	/* close our file */
	if(dvi->in) {
		fclose(dvi->in);
		dvi->in = NULL;
	}

	pars = np ? np : &dvi->params;
	DEBUG((DBG_DVI, "%s: reloading\n", dvi->filename));

	/* load it again */
	newdvi = mdvi_init_context(pars, dvi->pagesel, dvi->filename);
	if(newdvi == NULL) {
		mdvi_warning(_("could not reload `%s'\n"), dvi->filename);
		return -1;
	}

	/* drop all our font references */
	font_drop_chain(dvi->fonts);
	/* destroy our font map */
	if(dvi->fontmap)
		mdvi_free(dvi->fontmap);
	dvi->currfont = NULL;

	/* and use the ones we just loaded */
	dvi->fonts = newdvi->fonts;
	dvi->fontmap = newdvi->fontmap;
	dvi->nfonts = newdvi->nfonts;

	/* copy the new information */
	dvi->params = newdvi->params;
	dvi->num = newdvi->num;
	dvi->den = newdvi->den;
	dvi->dvimag = newdvi->dvimag;
	dvi->dviconv = newdvi->dviconv;
	dvi->dvivconv = newdvi->dvivconv;
	dvi->modtime = newdvi->modtime;

	if(dvi->fileid) mdvi_free(dvi->fileid);
	dvi->fileid = newdvi->fileid;

	dvi->dvi_page_w = newdvi->dvi_page_w;
	dvi->dvi_page_h = newdvi->dvi_page_h;

	mdvi_free(dvi->pagemap);
	dvi->pagemap = newdvi->pagemap;
	dvi->npages = newdvi->npages;
	if(dvi->currpage > dvi->npages-1)
		dvi->currpage = 0;

	mdvi_free(dvi->stack);
	dvi->stack = newdvi->stack;
	dvi->stacksize = newdvi->stacksize;

	/* remove fonts that are not being used anymore */
	font_free_unused(&dvi->device);

	mdvi_free(newdvi->filename);
	mdvi_free(newdvi);

	DEBUG((DBG_DVI, "%s: reload successful\n", dvi->filename));
	if(dvi->device.refresh)
		dvi->device.refresh(dvi, dvi->device.device_data);

	return 0;
}

/* function to change parameters ia DVI context
 * The DVI context is modified ONLY if this function is successful */
int mdvi_configure(DviContext *dvi, DviParamCode option, ...)
{
	va_list	ap;
	int	reset_all;
	int	reset_font;
	DviParams np;

	va_start(ap, option);

	reset_font = 0;
	reset_all  = 0;
	np = dvi->params; /* structure copy */
	while(option != MDVI_PARAM_LAST) {
		switch(option) {
		case MDVI_SET_DPI:
			np.dpi = np.vdpi = va_arg(ap, Uint);
			reset_all = 1;
			break;
		case MDVI_SET_XDPI:
			np.dpi = va_arg(ap, Uint);
			reset_all = 1;
			break;
		case MDVI_SET_YDPI:
			np.vdpi = va_arg(ap, Uint);
			break;
		case MDVI_SET_SHRINK:
			np.hshrink = np.vshrink = va_arg(ap, Uint);
			reset_font = MDVI_FONTSEL_GREY|MDVI_FONTSEL_BITMAP;
			break;
		case MDVI_SET_XSHRINK:
			np.hshrink = va_arg(ap, Uint);
			reset_font = MDVI_FONTSEL_GREY|MDVI_FONTSEL_BITMAP;
			break;
		case MDVI_SET_YSHRINK:
			np.vshrink = va_arg(ap, Uint);
			reset_font = MDVI_FONTSEL_GREY|MDVI_FONTSEL_BITMAP;
			break;
		case MDVI_SET_ORIENTATION:
			np.orientation = va_arg(ap, DviOrientation);
			reset_font = MDVI_FONTSEL_GLYPH;
			break;
		case MDVI_SET_GAMMA:
			np.gamma = va_arg(ap, double);
			reset_font = MDVI_FONTSEL_GREY;
			break;
		case MDVI_SET_DENSITY:
			np.density = va_arg(ap, Uint);
			reset_font = MDVI_FONTSEL_BITMAP;
			break;
		case MDVI_SET_MAGNIFICATION:
			np.mag = va_arg(ap, double);
			reset_all = 1;
			break;
		case MDVI_SET_DRIFT:
			np.hdrift = np.vdrift = va_arg(ap, int);
			break;
		case MDVI_SET_HDRIFT:
			np.hdrift = va_arg(ap, int);
			break;
		case MDVI_SET_VDRIFT:
			np.vdrift = va_arg(ap, int);
			break;
		case MDVI_SET_FOREGROUND:
			np.fg = va_arg(ap, Ulong);
			reset_font = MDVI_FONTSEL_GREY;
			break;
		case MDVI_SET_BACKGROUND:
			np.bg = va_arg(ap, Ulong);
			reset_font = MDVI_FONTSEL_GREY;
			break;
		default:
			break;
		}
		option = va_arg(ap, DviParamCode);
	}
	va_end(ap);

	/* check that all values make sense */
	if(np.dpi <= 0 || np.vdpi <= 0)
		return -1;
	if(np.mag <= 0.0)
		return -1;
	if(np.hshrink < 1 || np.vshrink < 1)
		return -1;
	if(np.hdrift < 0 || np.vdrift < 0)
		return -1;
	if(np.fg == np.bg)
		return -1;

	/*
	 * If the dpi or the magnification change, we basically have to reload
	 * the DVI file again from scratch.
	 */

	if(reset_all)
		return (mdvi_reload(dvi, &np) == 0);

	if(np.hshrink != dvi->params.hshrink) {
		np.conv = dvi->dviconv;
		if(np.hshrink)
			np.conv /= np.hshrink;
	}
	if(np.vshrink != dvi->params.vshrink) {
		np.vconv = dvi->dvivconv;
		if(np.vshrink)
			np.vconv /= np.vshrink;
	}

	if(reset_font) {
		font_reset_chain_glyphs(&dvi->device, dvi->fonts, reset_font);
	}
	dvi->params = np;
	if((reset_font & MDVI_FONTSEL_GLYPH) && dvi->device.refresh) {
		dvi->device.refresh(dvi, dvi->device.device_data);
		return 0;
	}

	return 1;
}
/*
 * Read the initial data from the DVI file. If something is wrong with the
 * file, we just spit out an error message and refuse to load the file,
 * without giving any details. This makes sense because DVI files are ok
 * 99.99% of the time, and dvitype(1) can be used to check the other 0.01%.
 */
DviContext *mdvi_init_context(DviParams *par, DviPageSpec *spec, const char *file)
{
	FILE	*p;
	Int32	arg;
	int	op;
	long	offset;
	int	n;
	DviContext *dvi;
	char	*filename;
	int	pagecount;

	/*
	 * 1. Open the file and initialize the DVI context
	 */

	filename = opendvi(file);
	if(filename == NULL) {
		perror(file);
		return NULL;
	}
	p = fopen(filename, "rb");
	if(p == NULL) {
		perror(file);
		mdvi_free(filename);
		return NULL;
	}
	dvi = xalloc(DviContext);
	memzero(dvi, sizeof(DviContext));
	dvi->pagemap = NULL;
	dvi->filename = filename;
	dvi->stack = NULL;
	dvi->modtime = get_mtime(fileno(p));
	dvi->buffer.data = NULL;
	dvi->pagesel = spec;
	dvi->in = p; /* now we can use the dget*() functions */

	/*
	 * 2. Read the preamble, extract scaling information, and
	 *    setup the DVI parameters.
	 */

	if(fuget1(p) != DVI_PRE)
		goto bad_dvi;
	if((arg = fuget1(p)) != DVI_ID) {
		mdvi_error(_("%s: unsupported DVI format (version %u)\n"),
			   file, arg);
		goto error; /* jump to the end of this routine,
			     * where we handle errors */
	}
	/* get dimensions */
	dvi->num = fuget4(p);
	dvi->den = fuget4(p);
	dvi->dvimag = fuget4(p);

	/* check that these numbers make sense */
	if(!dvi->num || !dvi->den || !dvi->dvimag)
		goto bad_dvi;

	dvi->params.mag =
		(par->mag > 0 ? par->mag : (double)dvi->dvimag / 1000.0);
	dvi->params.hdrift  = par->hdrift;
	dvi->params.vdrift  = par->vdrift;
	dvi->params.dpi     = par->dpi ? par->dpi : MDVI_DPI;
	dvi->params.vdpi    = par->vdpi ? par->vdpi : par->dpi;
	dvi->params.hshrink = par->hshrink;
	dvi->params.vshrink = par->vshrink;
	dvi->params.density = par->density;
	dvi->params.gamma   = par->gamma;
	dvi->params.conv    = (double)dvi->num / dvi->den;
	dvi->params.conv   *= (dvi->params.dpi / 254000.0) * dvi->params.mag;
	dvi->params.vconv   = (double)dvi->num / dvi->den;
	dvi->params.vconv  *= (dvi->params.vdpi / 254000.0) * dvi->params.mag;
	dvi->params.tfm_conv = (25400000.0 / dvi->num) *
				((double)dvi->den / 473628672) / 16.0;
	dvi->params.flags = par->flags;
	dvi->params.orientation = par->orientation;
	dvi->params.fg = par->fg;
	dvi->params.bg = par->bg;

	/* initialize colors */
	dvi->curr_fg = par->fg;
	dvi->curr_bg = par->bg;
	dvi->color_stack = NULL;
	dvi->color_top = 0;
	dvi->color_size = 0;

	/* pixel conversion factors */
	dvi->dviconv = dvi->params.conv;
	dvi->dvivconv = dvi->params.vconv;
	if(dvi->params.hshrink)
		dvi->params.conv /= dvi->params.hshrink;
	if(dvi->params.vshrink)
		dvi->params.vconv /= dvi->params.vshrink;

	/* get the comment from the preamble */
	n = fuget1(p);
	dvi->fileid = mdvi_malloc(n + 1);
	fread(dvi->fileid, 1, n, p);
	dvi->fileid[n] = 0;
	DEBUG((DBG_DVI, "%s: %s\n", filename, dvi->fileid));

	/*
	 * 3. Read postamble, extract page information (number of
	 *    pages, dimensions) and stack depth.
	 */

	/* jump to the end of the file */
	if(fseek(p, (long)-1, SEEK_END) == -1)
		goto error;
	for(n = 0; (op = fuget1(p)) == DVI_TRAILER; n++)
		if(fseek(p, (long)-2, SEEK_CUR) < 0)
			break;
	if(op != arg || n < 4)
		goto bad_dvi;
	/* get the pointer to postamble */
	fseek(p, (long)-5, SEEK_CUR);
	arg = fuget4(p);
	/* jump to it */
	fseek(p, (long)arg, SEEK_SET);
	if(fuget1(p) != DVI_POST)
		goto bad_dvi;
	offset = fuget4(p);
	if(dvi->num != fuget4(p) || dvi->den != fuget4(p) ||
	   dvi->dvimag != fuget4(p))
		goto bad_dvi;
	dvi->dvi_page_h = fuget4(p);
	dvi->dvi_page_w = fuget4(p);
	dvi->stacksize = fuget2(p);
	dvi->npages = fuget2(p);
	DEBUG((DBG_DVI, "%s: from postamble: stack depth %d, %d page%s\n",
		filename, dvi->stacksize, dvi->npages, dvi->npages > 1 ? "s" : ""));

	/*
	 * 4. Process font definitions.
	 */

	/* process font definitions */
	dvi->nfonts = 0;
	dvi->fontmap = NULL;
	/*
	 * CAREFUL: here we need to use the dvi->buffer, but it might leave the
	 * the file cursor in the wrong position after reading fonts (because of
	 * buffering). It's ok, though, because after the font definitions we read
	 * the page offsets, and we fseek() to the relevant part of the file with
	 * SEEK_SET. Nothing is read after the page offsets.
	 */
	while((op = duget1(dvi)) != DVI_POST_POST) {
		DviFontRef *ref;

		if(op == DVI_NOOP)
			continue;
		else if(op < DVI_FNT_DEF1 || op > DVI_FNT_DEF4)
			goto error;
		ref = define_font(dvi, op);
		if(ref == NULL)
			goto error;
		ref->next = dvi->fonts;
		dvi->fonts = ref;
		dvi->nfonts++;
	}
	/* we don't need the buffer anymore */
	dreset(dvi);

	if(op != DVI_POST_POST)
		goto bad_dvi;
	font_finish_definitions(dvi);
	DEBUG((DBG_DVI, "%s: %d font%s required by this job\n",
		filename, dvi->nfonts, dvi->nfonts > 1 ? "s" : ""));
	dvi->findref = font_find_mapped;

	/*
	 * 5. Build the page map.
	 */

	dvi->pagemap = xnalloc(PageNum, dvi->npages);
	memzero(dvi->pagemap, sizeof(PageNum) * dvi->npages);

	n = dvi->npages - 1;
	pagecount = n;
	while(offset != -1) {
		int	i;
		PageNum page;

		fseek(p, offset, SEEK_SET);
		op = fuget1(p);
		if(op != DVI_BOP || n < 0)
			goto bad_dvi;
		for(i = 1; i <= 10; i++)
			page[i] = fsget4(p);
		page[0] = offset;
		offset = fsget4(p);
		/* check if the page is selected */
		if(spec && mdvi_page_selected(spec, page, n) == 0) {
			DEBUG((DBG_DVI, "Page %d (%ld.%ld.%ld.%ld.%ld.%ld.%ld.%ld.%ld.%ld) ignored by request\n",
			n, page[1], page[2], page[3], page[4], page[5],
			   page[6], page[7], page[8], page[9], page[10]));
		} else {
			memcpy(&dvi->pagemap[pagecount], page, sizeof(PageNum));
			pagecount--;
		}
		n--;
	}
	pagecount++;
	if(pagecount >= dvi->npages) {
		mdvi_error(_("no pages selected\n"));
		goto error;
	}
	if(pagecount) {
		DEBUG((DBG_DVI, "%d of %d pages selected\n",
			dvi->npages - pagecount, dvi->npages));
		dvi->npages -= pagecount;
		memmove(dvi->pagemap, &dvi->pagemap[pagecount],
			dvi->npages * sizeof(PageNum));
	}

	/*
	 * 6. Setup stack, initialize device functions
	 */

	dvi->curr_layer = 0;
	dvi->stack = xnalloc(DviState, dvi->stacksize + 8);

	dvi->device.draw_glyph   = dummy_draw_glyph;
	dvi->device.draw_rule    = dummy_draw_rule;
	dvi->device.alloc_colors = dummy_alloc_colors;
	dvi->device.create_image = dummy_create_image;
	dvi->device.free_image   = dummy_free_image;
	dvi->device.dev_destroy  = dummy_dev_destroy;
	dvi->device.put_pixel    = dummy_dev_putpixel;
	dvi->device.refresh      = dummy_dev_refresh;
	dvi->device.set_color    = dummy_dev_set_color;
	dvi->device.device_data  = NULL;

	DEBUG((DBG_DVI, "%s read successfully\n", filename));
	return dvi;

bad_dvi:
	mdvi_error(_("%s: File corrupted, or not a DVI file\n"), file);
error:
	/* if we came from the font definitions, this will be non-trivial */
	dreset(dvi);
	mdvi_destroy_context(dvi);
	return NULL;
}

void	mdvi_destroy_context(DviContext *dvi)
{
	if(dvi->device.dev_destroy)
		dvi->device.dev_destroy(dvi->device.device_data);
	/* release all fonts */
	if(dvi->fonts) {
		font_drop_chain(dvi->fonts);
		font_free_unused(&dvi->device);
	}
	if(dvi->fontmap)
		mdvi_free(dvi->fontmap);
	if(dvi->filename)
		mdvi_free(dvi->filename);
	if(dvi->stack)
		mdvi_free(dvi->stack);
	if(dvi->pagemap)
		mdvi_free(dvi->pagemap);
	if(dvi->fileid)
		mdvi_free(dvi->fileid);
	if(dvi->in)
		fclose(dvi->in);
	if(dvi->buffer.data && !dvi->buffer.frozen)
		mdvi_free(dvi->buffer.data);
	if(dvi->color_stack)
		mdvi_free(dvi->color_stack);

	mdvi_free(dvi);
}

void	mdvi_setpage(DviContext *dvi, int pageno)
{
	if(pageno < 0)
		pageno = 0;
	if(pageno > dvi->npages-1)
		pageno = dvi->npages - 1;
	dvi->currpage = pageno;
}

static int	mdvi_run_macro(DviContext *dvi, Uchar *macro, size_t len)
{
	DviFontRef *curr, *fonts;
	DviBuffer saved_buffer;
	FILE	*saved_file;
	int	opcode;
	int	oldtop;

	dvi->depth++;
	push(dvi, DVI_PUSH);
	dvi->pos.w = 0;
	dvi->pos.x = 0;
	dvi->pos.y = 0;
	dvi->pos.z = 0;

	/* save our state */
	curr = dvi->currfont;
	fonts = dvi->fonts;
	saved_buffer = dvi->buffer;
	saved_file = dvi->in;
	dvi->currfont = curr->ref->subfonts;
	dvi->fonts = curr->ref->subfonts;
	dvi->buffer.data = macro;
	dvi->buffer.pos = 0;
	dvi->buffer.length = len;
	dvi->buffer.frozen = 1;
	dvi->in = NULL;
	oldtop = dvi->stacktop;

	/* execute commands */
	while((opcode = duget1(dvi)) != DVI_EOP) {
		if(dvi_commands[opcode](dvi, opcode) < 0)
			break;
	}
	if(opcode != DVI_EOP)
		dviwarn(dvi, _("%s: vf macro had errors\n"),
			curr->ref->fontname);
	if(dvi->stacktop != oldtop)
		dviwarn(dvi, _("%s: stack not empty after vf macro\n"),
			curr->ref->fontname);

	/* restore things */
	pop(dvi, DVI_POP);
	dvi->currfont = curr;
	dvi->fonts = fonts;
	dvi->buffer = saved_buffer;
	dvi->in = saved_file;
	dvi->depth--;

	return (opcode != DVI_EOP ? -1 : 0);
}

int	mdvi_dopage(DviContext *dvi, int pageno)
{
	int	op;
	int	ppi;
	int	reloaded = 0;

again:
	if(dvi->in == NULL) {
		/* try reopening the file */
		dvi->in = fopen(dvi->filename, "rb");
		if(dvi->in == NULL) {
			mdvi_warning(_("%s: could not reopen file (%s)\n"),
				     dvi->filename,
				     strerror(errno));
			return -1;
		}
		DEBUG((DBG_FILES, "reopen(%s) -> Ok\n", dvi->filename));
	}

	/* check if we need to reload the file */
	if(!reloaded && get_mtime(fileno(dvi->in)) > dvi->modtime) {
		mdvi_reload(dvi, &dvi->params);
		/* we have to reopen the file, again */
		reloaded = 1;
		goto again;
	}

	if(pageno < 0 || pageno > dvi->npages-1) {
		mdvi_error(_("%s: page %d out of range\n"),
			   dvi->filename, pageno);
		return -1;
	}

	fseek(dvi->in, (long)dvi->pagemap[pageno][0], SEEK_SET);
	if((op = fuget1(dvi->in)) != DVI_BOP) {
		mdvi_error(_("%s: bad offset at page %d\n"),
			   dvi->filename, pageno+1);
		return -1;
	}

	/* skip bop */
	fseek(dvi->in, (long)44, SEEK_CUR);

	/* reset state */
	dvi->currfont = NULL;
	memzero(&dvi->pos, sizeof(DviState));
	dvi->stacktop = 0;
	dvi->currpage = pageno;
	dvi->curr_layer = 0;

	if(dvi->buffer.data && !dvi->buffer.frozen)
		mdvi_free(dvi->buffer.data);

	/* reset our buffer */
	dvi->buffer.data   = NULL;
	dvi->buffer.length = 0;
	dvi->buffer.pos    = 0;
	dvi->buffer.frozen = 0;

#if 0 /* make colors survive page breaks */
	/* reset color stack */
	mdvi_reset_color(dvi);
#endif

	/* set max horizontal and vertical drift (from dvips) */
	if(dvi->params.hdrift < 0) {
		ppi = dvi->params.dpi / dvi->params.hshrink; /* shrunk pixels per inch */
		if(ppi < 600)
			dvi->params.hdrift = ppi / 100;
		else if(ppi < 1200)
			dvi->params.hdrift = ppi / 200;
		else
			dvi->params.hdrift = ppi / 400;
	}
	if(dvi->params.vdrift < 0) {
		ppi = dvi->params.vdpi / dvi->params.vshrink; /* shrunk pixels per inch */
		if(ppi < 600)
			dvi->params.vdrift = ppi / 100;
		else if(ppi < 1200)
			dvi->params.vdrift = ppi / 200;
		else
			dvi->params.vdrift = ppi / 400;
	}

	dvi->params.thinsp   = FROUND(0.025 * dvi->params.dpi / dvi->params.conv);
	dvi->params.vsmallsp = FROUND(0.025 * dvi->params.vdpi / dvi->params.vconv);

	/* execute all the commands in the page */
	while((op = duget1(dvi)) != DVI_EOP) {
		if(dvi_commands[op](dvi, op) < 0)
			break;
	}

	fflush(stdout);
	fflush(stderr);
	if(op != DVI_EOP)
		return -1;
	if(dvi->stacktop)
		dviwarn(dvi, _("stack not empty at end of page\n"));
	return 0;
}

static inline int move_vertical(DviContext *dvi, int amount)
{
	int	rvv;

	dvi->pos.v += amount;
	rvv = vpixel_round(dvi, dvi->pos.v);
	if(!dvi->params.vdrift)
		return rvv;
	if(amount > dvi->params.vsmallsp || amount <= -dvi->params.vsmallsp)
		return rvv;
	else {
		int	newvv;

		newvv = dvi->pos.vv + vpixel_round(dvi, amount);
		if(rvv - newvv > dvi->params.vdrift)
			return rvv - dvi->params.vdrift;
		else if(newvv - rvv > dvi->params.vdrift)
			return rvv + dvi->params.vdrift;
		else
			return newvv;
	}
}

static inline int move_horizontal(DviContext *dvi, int amount)
{
	int	rhh;

	dvi->pos.h += amount;
	rhh = pixel_round(dvi, dvi->pos.h);
	if(!dvi->params.hdrift)
		return rhh;
	else if(amount > dvi->params.thinsp || amount <= -6 * dvi->params.thinsp)
		return rhh;
	else {
		int	newhh;

		newhh = dvi->pos.hh + pixel_round(dvi, amount);
		if(rhh - newhh > dvi->params.hdrift)
			return rhh - dvi->params.hdrift;
		else if(newhh - rhh > dvi->params.hdrift)
			return rhh + dvi->params.hdrift;
		else
			return newhh;
	}
}

static inline void fix_after_horizontal(DviContext *dvi)
{
	int	rhh;

	rhh = pixel_round(dvi, dvi->pos.h);
	if(!dvi->params.hdrift)
		dvi->pos.hh = rhh;
	else if(rhh - dvi->pos.hh > dvi->params.hdrift)
		dvi->pos.hh = rhh - dvi->params.hdrift;
	else if(dvi->pos.hh - rhh > dvi->params.hdrift)
		dvi->pos.hh = rhh + dvi->params.hdrift;
}

/* commands */

#define DBGSUM(a,b,c) \
	(a), (b) > 0 ? '+' : '-', \
	(b) > 0 ? (b) : -(b), (c)

static void draw_shrink_rule (DviContext *dvi, int x, int y, Uint w, Uint h, int f)
{
	Ulong fg, bg;

	fg = dvi->curr_fg;
	bg = dvi->curr_bg;

	mdvi_push_color (dvi, fg, bg);
	dvi->device.draw_rule(dvi, x, y, w, h, f);
	mdvi_pop_color (dvi);

	return;
}

/*
 * The only commands that actually draw something are:
 *   set_char, set_rule
 */

static void draw_box(DviContext *dvi, DviFontChar *ch)
{
	DviGlyph *glyph = NULL;
	int	x, y, w, h;

	if(!MDVI_GLYPH_UNSET(ch->shrunk.data))
		glyph = &ch->shrunk;
	else if(!MDVI_GLYPH_UNSET(ch->grey.data))
		glyph = &ch->grey;
	else if(!MDVI_GLYPH_UNSET(ch->glyph.data))
		glyph = &ch->glyph;
	if(glyph == NULL)
		return;
	x = glyph->x;
	y = glyph->y;
	w = glyph->w;
	h = glyph->h;
	/* this is bad -- we have to undo the orientation */
	switch(dvi->params.orientation) {
	case MDVI_ORIENT_TBLR:
		break;
	case MDVI_ORIENT_TBRL:
		x = w - x;
		break;
	case MDVI_ORIENT_BTLR:
		y = h - y;
		break;
	case MDVI_ORIENT_BTRL:
		x = w - x;
		y = h - y;
		break;
	case MDVI_ORIENT_RP90:
		SWAPINT(w, h);
		SWAPINT(x, y);
		x = w - x;
		break;
	case MDVI_ORIENT_RM90:
		SWAPINT(w, h);
		SWAPINT(x, y);
		y = h - y;
		break;
	case MDVI_ORIENT_IRP90:
		SWAPINT(w, h);
		SWAPINT(x, y);
		break;
	case MDVI_ORIENT_IRM90:
		SWAPINT(w, h);
		SWAPINT(x, y);
		x = w - x;
		y = h - y;
		break;
	}

	draw_shrink_rule(dvi, dvi->pos.hh - x, dvi->pos.vv - y, w, h, 1);
}

int	set_char(DviContext *dvi, int opcode)
{
	int	num;
	int	h;
	int	hh;
	DviFontChar *ch;
	DviFont	*font;

	if(opcode < 128)
		num = opcode;
	else
		num = dugetn(dvi, opcode - DVI_SET1 + 1);
	if(dvi->currfont == NULL) {
		dvierr(dvi, _("no default font set yet\n"));
		return -1;
	}
	font = dvi->currfont->ref;
	ch = font_get_glyph(dvi, font, num);
	if(ch == NULL || ch->missing) {
		/* try to display something anyway */
		ch = FONTCHAR(font, num);
		if(!glyph_present(ch)) {
			dviwarn(dvi,
			_("requested character %d does not exist in `%s'\n"),
				num, font->fontname);
			return 0;
		}
		draw_box(dvi, ch);
	} else if(dvi->curr_layer <= dvi->params.layer) {
		if(ISVIRTUAL(font))
			mdvi_run_macro(dvi, (Uchar *)font->private +
				ch->offset, ch->width);
		else if(ch->width && ch->height)
			dvi->device.draw_glyph(dvi, ch,
				dvi->pos.hh, dvi->pos.vv);
	}
	if(opcode >= DVI_PUT1 && opcode <= DVI_PUT4) {
		SHOWCMD((dvi, "putchar", opcode - DVI_PUT1 + 1,
			"char %d (%s)\n",
			num, dvi->currfont->ref->fontname));
	} else {
		h = dvi->pos.h + ch->tfmwidth;
		hh = dvi->pos.hh + pixel_round(dvi, ch->tfmwidth);
		SHOWCMD((dvi, "setchar", num, "(%d,%d) h:=%d%c%ld=%d, hh:=%d (%s)\n",
			dvi->pos.hh, dvi->pos.vv,
			DBGSUM(dvi->pos.h, (long) ch->tfmwidth, h), hh,
			font->fontname));
		dvi->pos.h  = h;
		dvi->pos.hh = hh;
		fix_after_horizontal(dvi);
	}

	return 0;
}

int	set_rule(DviContext *dvi, int opcode)
{
	Int32	a, b;
	int	h, w;

	a = dsget4(dvi);
	b = dsget4(dvi); w = rule_round(dvi, b);
	if(a > 0 && b > 0) {
		h = vrule_round(dvi, a);
		SHOWCMD((dvi, opcode == DVI_SET_RULE ? "setrule" : "putrule", -1,
			"width %ld, height %ld (%dx%d pixels)\n",
			(long) b, (long) a, w, h));
		/* the `draw' functions expect the origin to be at the top left
		 * corner of the rule, not the bottom left, as in DVI files */
		if(dvi->curr_layer <= dvi->params.layer) {
			draw_shrink_rule(dvi,
					 dvi->pos.hh, dvi->pos.vv - h + 1, w, h, 1);
		}
	} else {
		SHOWCMD((dvi, opcode == DVI_SET_RULE ? "setrule" : "putrule", -1,
			"(moving left only, by %ld)\n", (long) b));
	}

	if(opcode == DVI_SET_RULE) {
		dvi->pos.h  += b;
		dvi->pos.hh += w;
		fix_after_horizontal(dvi);
	}
	return 0;
}

int	no_op(DviContext *dvi, int opcode)
{
	SHOWCMD((dvi, "noop", -1, "\n"));
	return 0;
}

int	push(DviContext *dvi, int opcode)
{
	if(dvi->stacktop == dvi->stacksize) {
		if(!dvi->depth)
			dviwarn(dvi, _("enlarging stack\n"));
		dvi->stacksize += 8;
		dvi->stack = xresize(dvi->stack,
			DviState, dvi->stacksize);
	}
	memcpy(&dvi->stack[dvi->stacktop], &dvi->pos, sizeof(DviState));
	SHOWCMD((dvi, "push", -1,
		"level %d: (h=%d,v=%d,w=%d,x=%d,y=%d,z=%d,hh=%d,vv=%d)\n",
		dvi->stacktop,
		dvi->pos.h, dvi->pos.v, dvi->pos.w, dvi->pos.x,
		dvi->pos.y, dvi->pos.z, dvi->pos.hh, dvi->pos.vv));
	dvi->stacktop++;
	return 0;
}

int	pop(DviContext *dvi, int opcode)
{
	if(dvi->stacktop == 0) {
		dvierr(dvi, _("stack underflow\n"));
		return -1;
	}
	memcpy(&dvi->pos, &dvi->stack[dvi->stacktop-1], sizeof(DviState));
	SHOWCMD((dvi, "pop", -1,
		"level %d: (h=%d,v=%d,w=%d,x=%d,y=%d,z=%d,hh=%d,vv=%d)\n",
		dvi->stacktop,
		dvi->pos.h, dvi->pos.v, dvi->pos.w, dvi->pos.x,
		dvi->pos.y, dvi->pos.z, dvi->pos.hh, dvi->pos.vv));
	dvi->stacktop--;
	return 0;
}

int	move_right(DviContext *dvi, int opcode)
{
	Int32	arg;
	int	h, hh;

	arg = dsgetn(dvi, opcode - DVI_RIGHT1 + 1);
	h = dvi->pos.h;
	hh = move_horizontal(dvi, arg);
	SHOWCMD((dvi, "right", opcode - DVI_RIGHT1 + 1,
		"%ld h:=%d%c%ld=%d, hh:=%d\n",
		(long) arg, DBGSUM(h, (long) arg, dvi->pos.h), hh));
	dvi->pos.hh = hh;
	return 0;
}

int	move_down(DviContext *dvi, int opcode)
{
	Int32	arg;
	int	v, vv;

	arg = dsgetn(dvi, opcode - DVI_DOWN1 + 1);
	v = dvi->pos.v;
	vv = move_vertical(dvi, arg);
	SHOWCMD((dvi, "down", opcode - DVI_DOWN1 + 1,
		"%ld v:=%d%c%ld=%d, vv:=%d\n",
		(long) arg, DBGSUM(v, (long) arg, dvi->pos.v), vv));
	dvi->pos.vv = vv;
	return 0;
}

int	move_w(DviContext *dvi, int opcode)
{
	int	h, hh;

	if(opcode != DVI_W0)
		dvi->pos.w = dsgetn(dvi, opcode - DVI_W0);
	h = dvi->pos.h;
	hh = move_horizontal(dvi, dvi->pos.w);
	SHOWCMD((dvi, "w", opcode - DVI_W0,
		"%d h:=%d%c%d=%d, hh:=%d\n",
		dvi->pos.w, DBGSUM(h, dvi->pos.w, dvi->pos.h), hh));
	dvi->pos.hh = hh;
	return 0;
}

int	move_x(DviContext *dvi, int opcode)
{
	int	h, hh;

	if(opcode != DVI_X0)
		dvi->pos.x = dsgetn(dvi, opcode - DVI_X0);
	h = dvi->pos.h;
	hh = move_horizontal(dvi, dvi->pos.x);
	SHOWCMD((dvi, "x", opcode - DVI_X0,
		"%d h:=%d%c%d=%d, hh:=%d\n",
		dvi->pos.x, DBGSUM(h, dvi->pos.x, dvi->pos.h), hh));
	dvi->pos.hh = hh;
	return 0;
}

int	move_y(DviContext *dvi, int opcode)
{
	int	v, vv;

	if(opcode != DVI_Y0)
		dvi->pos.y = dsgetn(dvi, opcode - DVI_Y0);
	v = dvi->pos.v;
	vv = move_vertical(dvi, dvi->pos.y);
	SHOWCMD((dvi, "y", opcode - DVI_Y0,
		"%d h:=%d%c%d=%d, hh:=%d\n",
		dvi->pos.y, DBGSUM(v, dvi->pos.y, dvi->pos.v), vv));
	dvi->pos.vv = vv;
	return 0;
}

int	move_z(DviContext *dvi, int opcode)
{
	int	v, vv;

	if(opcode != DVI_Z0)
		dvi->pos.z = dsgetn(dvi, opcode - DVI_Z0);
	v = dvi->pos.v;
	vv = move_vertical(dvi, dvi->pos.z);
	SHOWCMD((dvi, "z", opcode - DVI_Z0,
		"%d h:=%d%c%d=%d, hh:=%d\n",
		dvi->pos.z, DBGSUM(v, dvi->pos.z, dvi->pos.v), vv));
	dvi->pos.vv = vv;
	return 0;
}

int	sel_font(DviContext *dvi, int opcode)
{
	DviFontRef *ref;
	int	ndx;

	ndx = opcode - DVI_FNT_NUM0;
	if(dvi->depth)
		ref = font_find_flat(dvi, ndx);
	else
		ref = dvi->findref(dvi, ndx);
	if(ref == NULL) {
		dvierr(dvi, _("font %d is not defined\n"),
			opcode - DVI_FNT_NUM0);
		return -1;
	}
	SHOWCMD((dvi, "fntnum", opcode - DVI_FNT_NUM0,
		"current font is %s\n",
		ref->ref->fontname));
	dvi->currfont = ref;
	return 0;
}

int	sel_fontn(DviContext *dvi, int opcode)
{
	Int32	arg;
	DviFontRef *ref;

	arg = dugetn(dvi, opcode - DVI_FNT1 + 1);
	if(dvi->depth)
		ref = font_find_flat(dvi, arg);
	else
		ref = dvi->findref(dvi, arg);
	if(ref == NULL) {
		dvierr(dvi, _("font %ld is not defined\n"), (long) arg);
		return -1;
	}
	SHOWCMD((dvi, "fnt", opcode - DVI_FNT1 + 1,
		"current font is %s (id %ld)\n",
		ref->ref->fontname, (long) arg));
	dvi->currfont = ref;
	return 0;
}

int	special(DviContext *dvi, int opcode)
{
	char	*s;
	Int32	arg;

	arg = dugetn(dvi, opcode - DVI_XXX1 + 1);
	if (arg <= 0) {
		dvierr(dvi, _("malformed special length\n"));
		return -1;
	}
	s = mdvi_malloc(arg + 1);
	dread(dvi, s, arg);
	s[arg] = 0;
	mdvi_do_special(dvi, s);
	SHOWCMD((dvi, "XXXX", opcode - DVI_XXX1 + 1,
		"[%s]", s));
	mdvi_free(s);
	return 0;
}

int	def_font(DviContext *dvi, int opcode)
{
	DviFontRef *ref;
	Int32	arg;

	arg = dugetn(dvi, opcode - DVI_FNT_DEF1 + 1);
	if(dvi->depth)
		ref = font_find_flat(dvi, arg);
	else
		ref = dvi->findref(dvi, arg);
	/* skip the rest */
	dskip(dvi, 12);
	dskip(dvi, duget1(dvi) + duget1(dvi));
	if(ref == NULL) {
		dvierr(dvi, _("font %ld is not defined in postamble\n"), (long) arg);
		return -1;
	}
	SHOWCMD((dvi, "fntdef", opcode - DVI_FNT_DEF1 + 1,
		"%ld -> %s (%d links)\n",
		(long) ref->fontid, ref->ref->fontname,
		ref->ref->links));
	return 0;
}

int	unexpected(DviContext *dvi, int opcode)
{
	dvierr(dvi, _("unexpected opcode %d\n"), opcode);
	return -1;
}

int	undefined(DviContext *dvi, int opcode)
{
	dvierr(dvi, _("undefined opcode %d\n"), opcode);
	return -1;
}
