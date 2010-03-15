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
#ifndef _MDVI_FONTMAP_H
#define _MDVI_FONTMAP_H 1

typedef struct _DviFontMapEnt DviFontMapEnt;
typedef struct _DviEncoding DviEncoding;

typedef struct {
	const char *psname;
	const char *encoding;
	const char *fontfile;
	const char *fullfile;
	const char *fmfile;
	int fmtype;
	long extend;
	long slant;
} DviFontMapInfo;

struct _DviEncoding {
	DviEncoding *next;
	DviEncoding *prev;
	char	*private;
	char	*filename;
	char	*name;
	char	**vector; /* table with exactly 256 strings */
	int	links;
	long	offset;
	DviHashTable nametab;
};

struct _DviFontMapEnt {
	DviFontMapEnt *next;
	DviFontMapEnt *prev;
	char	*private;
	char	*fontname;
	char	*psname;
	char	*encoding;
	char	*encfile;
	char	*fontfile;
	char	*fullfile;
	long	extend;
	long	slant;
};

#define MDVI_FMAP_SLANT(x)	((double)(x)->slant / 10000.0)
#define MDVI_FMAP_EXTEND(x)	((double)(x)->extend / 10000.0)

extern DviEncoding *mdvi_request_encoding __PROTO((const char *));
extern void mdvi_release_encoding __PROTO((DviEncoding *, int));
extern int  mdvi_encode_glyph __PROTO((DviEncoding *, const char *));
extern DviFontMapEnt *mdvi_load_fontmap __PROTO((const char *));
extern void mdvi_install_fontmap __PROTO((DviFontMapEnt *));
extern int  mdvi_load_fontmaps __PROTO((void));
extern int  mdvi_query_fontmap __PROTO((DviFontMapInfo *, const char *));
extern void mdvi_flush_encodings __PROTO((void));
extern void mdvi_flush_fontmaps __PROTO((void));

extern int  mdvi_add_fontmap_file __PROTO((const char *, const char *));

/* PS font maps */
extern int  mdvi_ps_read_fontmap __PROTO((const char *));
extern char *mdvi_ps_find_font __PROTO((const char *));
extern TFMInfo *mdvi_ps_get_metrics __PROTO((const char *));
extern void mdvi_ps_flush_fonts __PROTO((void));

#endif /* _MDVI_FONTMAP_H */
