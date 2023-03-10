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

/* postscript specials */

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mdvi.h"
#include "private.h"

typedef struct {
	double	ox;
	double	oy;
	double	bw;
	double	bh;
	double	angle;
} EpsfBox;

#define LLX	0
#define LLY	1
#define URX	2
#define URY	3
#define RWI	4
#define RHI	5
#define HOFF	6
#define VOFF	7
#define HSIZE	8
#define VSIZE	9
#define HSCALE	10
#define VSCALE	11
#define ANGLE	12
#define CLIP	13

void	epsf_special __PROTO((DviContext *dvi, char *prefix, char *arg));

/* Note: the given strings are modified in place */
static char *parse_epsf_special(EpsfBox *box, char **ret,
	char *prefix, char *arg)
{
	static struct {
		char *name;
		int  has_arg;
		char *value;
	} keys[] = {
		{"llx", 1, "0"},
		{"lly", 1, "0"},
		{"urx", 1, "0"},
		{"ury", 1, "0"},
		{"rwi", 1, "0"},
		{"rhi", 1, "0"},
		{"hoffset", 1, "0"},
		{"voffset", 1, "0"},
		{"hsize", 1, "612"},
		{"vsize", 1, "792"},
		{"hscale", 1, "100"},
		{"vscale", 1, "100"},
		{"angle", 1, "0"},
		{"clip", 0, "0"}
	};
#define NKEYS	(sizeof(keys) / sizeof(keys[0]))
	char	*ptr;
	char	*filename;
	double	value[NKEYS];
	Uchar	present[NKEYS];
	Buffer	buffer;
	char	*name;
	int	i;
	double	originx;
	double	originy;
	double	hsize;
	double	vsize;
	double	hscale;
	double	vscale;

	/* this special has the form
	 *   ["]file.ps["] [key=valye]*
	 */

	/* scan the filename */
	while(*arg == ' ' || *arg == '\t')
		arg++;

	/* make a copy of the string */
	ptr = arg;

	if(*ptr == '"')
		for(name = ++ptr; *ptr && *ptr != '"'; ptr++);
	else
		for(name = ptr; *ptr && *ptr != ' ' && *ptr != '\t'; ptr++);
	if(ptr == name)
		return NULL;
	*ptr++ = 0;
	filename = name;

	/* reset values to defaults */
	for(i = 0; i < NKEYS; i++) {
		value[i] = atof(keys[i].value);
		present[i] = 0;
	}

	buff_init(&buffer);
	buff_add(&buffer, "@beginspecial ", 0);

	while(*ptr) {
		const char *keyname;
		char	*val;
		char	*p;

		while(*ptr == ' ' || *ptr == '\t')
			ptr++;
		keyname = ptr;

		/* get the whole key=value pair */
		for(; *ptr && *ptr != ' ' && *ptr != '\t'; ptr++);

		if(*ptr) *ptr++ = 0;
		/* now we shouldn't touch `ptr' anymore */

		/* now work on this pair */
		p = strchr(keyname, '=');
		if(p == NULL)
			val = NULL;
		else {
			*p++ = 0;
			if(*p == '"') {
				val = ++p;
				/* skip until closing quote */
				while(*p && *p != '"')
					p++;
				if(*p != '"')
					mdvi_warning(
					_("%s: malformed value for key `%s'\n"),
						filename, keyname);
			} else
				val = p;
		}

		/* lookup the key */
		for(i = 0; i < NKEYS; i++)
			if(STRCEQ(keys[i].name, keyname))
				break;
		if(i == NKEYS) {
			mdvi_warning(_("%s: unknown key `%s' ignored\n"),
				     filename, keyname);
			continue;
		}
		if(keys[i].has_arg && val == NULL) {
			mdvi_warning(_("%s: no argument for key `%s', using defaults\n"),
				     filename, keyname);
			val = keys[i].value;
		} else if(!keys[i].has_arg && val) {
			mdvi_warning(_("%s: argument `%s' ignored for key `%s'\n"),
				     filename, val, keyname);
			val = NULL;
		}
		if(val)
			value[i] = atof(val);

		/* put the argument */
		buff_add(&buffer, val, 0);
		buff_add(&buffer, " @", 2);
		buff_add(&buffer, keyname, 0);
		buff_add(&buffer, " ", 1);

		/* remember that this option was given */
		present[i] = 0xff;
	}
	buff_add(&buffer, " @setspecial", 0);

	/* now compute the bounding box (code comes from dvips) */
	originx = 0;
	originy = 0;
	hscale = 1;
	vscale = 1;
	hsize = 0;
	vsize = 0;

	if(present[HSIZE])
		hsize = value[HSIZE];
	if(present[VSIZE])
		vsize = value[VSIZE];
	if(present[HOFF])
		originx = value[HOFF];
	if(present[VOFF])
		originy = value[VOFF];
	if(present[HSCALE])
		hscale = value[HSCALE] / 100.0;
	if(present[VSCALE])
		vscale = value[VSCALE] / 100.0;
	if(present[URX] && present[LLX])
		hsize = value[URX] - value[LLX];
	if(present[URY] && present[LLY])
		vsize = value[URY] - value[LLY];
	if(present[RWI] || present[RHI]) {
		if(present[RWI] && !present[RHI])
			hscale = vscale = value[RWI] / (10.0 * hsize);
		else if(present[RHI] && !present[RWI])
			hscale = vscale = value[RHI] / (10.0 * vsize);
		else {
			hscale = value[RWI] / (10.0 * hsize);
			vscale = value[RHI] / (10.0 * vsize);
		}
	}

	box->ox = originx;
	box->oy = originy;
	box->bw = hsize * hscale;
	box->bh = vsize * vscale;
	box->angle = value[ANGLE];

	*ret = buffer.data;

	return filename;
}

void	epsf_special(DviContext *dvi, char *prefix, char *arg)
{
	char	*file;
	char	*special;
	char    *psfile;
	char    *tmp;
	EpsfBox	box = {0, 0, 0, 0};
	int	x, y;
	int	w, h;
	double	xf, vf;
	struct stat buf;

	file = parse_epsf_special(&box, &special, prefix, arg);
	if (file != NULL)
		mdvi_free (special);

	xf = dvi->params.dpi * dvi->params.mag / (72.0 * dvi->params.hshrink);
	vf = dvi->params.vdpi * dvi->params.mag / (72.0 * dvi->params.vshrink);
	w = FROUND(box.bw * xf);
	h = FROUND(box.bh * vf);
	x = FROUND(box.ox * xf) + dvi->pos.hh;
	y = FROUND(box.oy * vf) + dvi->pos.vv - h + 1;

	if (!file || !dvi->device.draw_ps) {
		dvi->device.draw_rule (dvi, x, y, w, h, 0);
		return;
	}

	if (file[0] == '/') { /* Absolute path */
		if (stat (file, &buf) == 0)
			dvi->device.draw_ps (dvi, file, x, y, w, h);
		else
			dvi->device.draw_rule (dvi, x, y, w, h, 0);
		return;
	}

	tmp = mdvi_strrstr (dvi->filename, "/");
	if (tmp) { /* Document directory */
		int path_len = strlen (dvi->filename) - strlen (tmp + 1);
		int file_len = strlen (file);

		psfile = mdvi_malloc (path_len + file_len + 1);
		psfile[0] = '\0';
		strncat (psfile, dvi->filename, path_len);
		strncat (psfile, file, file_len);

		if (stat (psfile, &buf) == 0) {
			dvi->device.draw_ps (dvi, psfile, x, y, w, h);
			mdvi_free (psfile);

			return;
		}

		mdvi_free (psfile);
	}

	psfile = mdvi_build_path_from_cwd (file);
	if (stat (psfile, &buf) == 0) { /* Current working dir */
		dvi->device.draw_ps (dvi, psfile, x, y, w, h);
		mdvi_free (psfile);

		return;
	}

	mdvi_free (psfile);

	psfile = kpse_find_pict (file);
	if (psfile) { /* kpse */
		dvi->device.draw_ps (dvi, psfile, x, y, w, h);
	} else {
		dvi->device.draw_rule(dvi, x, y, w, h, 0);
	}

	free (psfile);
}
