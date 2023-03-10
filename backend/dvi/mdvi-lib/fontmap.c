/* encoding.c - functions to manipulate encodings and fontmaps */
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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>

#include "mdvi.h"
#include "private.h"

#include <kpathsea/expand.h>
#include <kpathsea/pathsearch.h>

typedef struct _DviFontMap DviFontMap;

struct _DviFontMap {
	ListHead entries;
	DviHashTable fonts;
};

typedef struct _PSFontMap {
	struct _PSFontMap *next;
	struct _PSFontMap *prev;
	char	*psname;
	char	*mapname;
	char	*fullname;
} PSFontMap;

/* these variables control PS font maps */
static char *pslibdir = NULL;	/* path where we look for PS font maps */
static char *psfontdir = NULL;	/* PS font search path */
static int psinitialized = 0;	/* did we expand the path already? */

static ListHead psfonts = MDVI_EMPTY_LIST_HEAD;
static DviHashTable pstable = MDVI_EMPTY_HASH_TABLE;

static ListHead fontmaps;
static DviHashTable maptable;
static int fontmaps_loaded = 0;

#define MAP_HASH_SIZE	57
#define ENC_HASH_SIZE	31
#define PSMAP_HASH_SIZE	57

/* this hash table should be big enough to
 * hold (ideally) one glyph name per bucket */
#define ENCNAME_HASH_SIZE	131 /* most TeX fonts have 128 glyphs */

static ListHead	encodings = MDVI_EMPTY_LIST_HEAD;
static DviEncoding *tex_text_encoding = NULL;
static DviEncoding *default_encoding = NULL;

/* we keep two hash tables for encodings: one for their base files (e.g.
 * "8r.enc"), and another one for their names (e.g. "TeXBase1Encoding") */
static DviHashTable enctable = MDVI_EMPTY_HASH_TABLE;
static DviHashTable enctable_file = MDVI_EMPTY_HASH_TABLE;

/* the TeX text encoding, from dvips */
static char *tex_text_vector[256] = {
	"Gamma", "Delta", "Theta", "Lambda", "Xi", "Pi", "Sigma", "Upsilon",
	"Phi", "Psi", "Omega", "arrowup", "arrowdown", "quotesingle",
	"exclamdown", "questiondown", "dotlessi", "dotlessj", "grave",
	"acute", "caron", "breve", "macron", "ring", "cedilla",
	"germandbls", "ae", "oe", "oslash", "AE", "OE", "Oslash", "space",
	"exclam", "quotedbl", "numbersign", "dollar", "percent",
	"ampersand", "quoteright", "parenleft", "parenright", "asterisk",
	"plus", "comma", "hyphen", "period", "slash", "zero", "one", "two",
	"three", "four", "five", "six", "seven", "eight", "nine", "colon",
	"semicolon", "less", "equal", "greater", "question", "at", "A", "B",
	"C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
	"P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
	"bracketleft", "backslash", "bracketright", "circumflex",
	"underscore", "quoteleft", "a", "b", "c", "d", "e", "f", "g", "h",
	"i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u",
	"v", "w", "x", "y", "z", "braceleft", "bar", "braceright", "tilde",
	"dieresis", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static void ps_init_default_paths __PROTO((void));
static int	mdvi_set_default_encoding __PROTO((const char *name));
static int	mdvi_init_fontmaps __PROTO((void));

/*
 * What we do here is allocate one block large enough to hold the entire
 * file (these files are small) minus the leading comments. This is much
 * better than allocating up to 256 tiny strings per encoding vector. */
static int read_encoding(DviEncoding *enc)
{
	FILE	*in;
	int	curr;
	char	*line;
	char	*name;
	char	*next;
	struct stat st;

	ASSERT(enc->private == NULL);

	in = fopen(enc->filename, "rb");
	if(in == NULL) {
		DEBUG((DBG_FMAP, "%s: could not read `%s' (%s)\n",
			enc->name, enc->filename, strerror(errno)));
		return -1;
	}
	if(fstat(fileno(in), &st) < 0) {
		/* should not happen */
		fclose(in);
		return -1;
	}
	st.st_size -= enc->offset;

	/* this will be one big string */
	enc->private = (char *)malloc(st.st_size + 1);
	/* setup the hash table */
	mdvi_hash_create(&enc->nametab, ENCNAME_HASH_SIZE);
	/* setup the encoding vector */
	enc->vector = (char **)mdvi_malloc(256 * sizeof(char *));

	/* jump to the beginning of the interesting part */
	fseek(in, enc->offset, SEEK_SET);
	/* and read everything */
	if(fread(enc->private, st.st_size, 1, in) != 1) {
		fclose(in);
		mdvi_free(enc->private);
		enc->private = NULL;
		return -1;
	}
	/* we don't need this anymore */
	fclose(in);
	curr = 0;

	next = name = NULL;
	DEBUG((DBG_FMAP, "%s: reading encoding vector\n", enc->name));
	for(line = enc->private; *line && curr < 256; line = next) {
		SKIPSP(line);
		if(*line == ']') {
			line++; SKIPSP(line);
			if(STRNEQ(line, "def", 3))
				break;
		}
		name = getword(line, " \t\n", &next);
		if(name == NULL)
			break;
		/* next > line */
		if(*name < ' ')
			continue;
		if(*name == '%') {
			while(*next && *next != '\n')
				next++;
			if(*next) next++; /* skip \n */
			continue;
		}

		/* got a name */
		if(*next) *next++ = 0;

		if(*name == '/')
			name++;
		enc->vector[curr] = name;
		/* add it to the hash table */
		if(!STREQ(name, ".notdef")) {
			mdvi_hash_add(&enc->nametab, MDVI_KEY(name),
				Int2Ptr(curr + 1), MDVI_HASH_REPLACE);
		}
		curr++;
	}
	if(curr == 0) {
		mdvi_hash_reset(&enc->nametab, 0);
		mdvi_free(enc->private);
		mdvi_free(enc);
		return -1;
	}
	while(curr < 256)
		enc->vector[curr++] = NULL;
	return 0;
}

static DviEncoding *find_encoding(const char *name)
{
	return (DviEncoding *)(encodings.count ?
		mdvi_hash_lookup(&enctable, MDVI_KEY(name)) : NULL);
}

static void destroy_encoding(DviEncoding *enc)
{
	if(enc == default_encoding) {
		default_encoding = tex_text_encoding;
		/* now we use reference counts again */
		mdvi_release_encoding(enc, 1);
	}
	if(enc != tex_text_encoding) {
		mdvi_hash_reset(&enc->nametab, 0);
		if(enc->private) {
			mdvi_free(enc->private);
			mdvi_free(enc->vector);
		}
		if(enc->name)
			mdvi_free(enc->name);
		if(enc->filename)
			mdvi_free(enc->filename);
		mdvi_free(enc);
	}
}

/* this is used for the `enctable_file' hash table */
static void file_hash_free(DviHashKey key, void *data)
{
	mdvi_free(key);
}

static DviEncoding *register_encoding(const char *basefile, int replace)
{
	DviEncoding *enc;
	FILE	*in;
	char	*filename;
	char	*name;
	Dstring	input;
	char	*line;
	long	offset;

	DEBUG((DBG_FMAP, "register_encoding(%s)\n", basefile));

	if(encodings.count) {
		enc = mdvi_hash_lookup(&enctable_file, MDVI_KEY(basefile));
		if(enc != NULL) {
			DEBUG((DBG_FMAP, "%s: already there\n", basefile));
			return enc; /* no error */
		}
	}

	/* try our own files first */
	filename = kpse_find_file(basefile,
		kpse_program_text_format, 0);

	/* then try the system-wide ones */
	if(filename == NULL)
		filename = kpse_find_file(basefile,
			kpse_tex_ps_header_format, 0);
	if(filename == NULL)
		filename = kpse_find_file(basefile,
			kpse_dvips_config_format, 0);

	/* finally try the given name */
	if(filename == NULL)
		filename = mdvi_strdup(basefile);

	in = fopen(filename, "rb");
	if(in == NULL) {
		mdvi_free(filename);
		return NULL;
	}

	/* just lookup the name of the encoding */
	name = NULL;
	dstring_init(&input);
	while((line = dgets(&input, in)) != NULL) {
		if(STRNEQ(line, "Encoding=", 9)) {
			name = getword(line + 9, " \t", &line);
			if(*line) *line++ = 0;
			break;
		} else if(*line == '/') {
			char	*label = getword(line + 1, " \t", &line);
			if(*line) {
				*line++ = 0;
				SKIPSP(line);
				if(*line == '[') {
					*line = 0;
					name = label;
					break;
				}
			}
		}
	}
	offset = ftell(in);
	fclose(in);
	if(name == NULL || *name == 0) {
		DEBUG((DBG_FMAP,
			"%s: could not determine name of encoding\n",
			basefile));
		mdvi_free(filename);
		return NULL;
	}

	/* check if the encoding is already there */
	enc = find_encoding(name);
	if(enc == tex_text_encoding) {
		/* A special case: if the vector we found is the static one,
		 * allow the user to override it with an external file */
		listh_remove(&encodings, LIST(enc));
		mdvi_hash_remove(&enctable, MDVI_KEY(enc->name));
		if(enc == default_encoding)
			default_encoding = NULL;
	} else if(enc) {
		/* if the encoding is being used, refuse to remove it */
		if(enc->links) {
			mdvi_free(filename);
			dstring_reset(&input);
			return NULL;
		}
		if(replace) {
			mdvi_hash_remove(&enctable, MDVI_KEY(name));
			mdvi_hash_remove(&enctable_file, MDVI_KEY(basefile));
			listh_remove(&encodings, LIST(enc));
			if(enc == default_encoding) {
				default_encoding = NULL;
				mdvi_release_encoding(enc, 1);
			}
			DEBUG((DBG_FMAP, "%s: overriding encoding\n", name));
			destroy_encoding(enc);
		} else {
			mdvi_free(filename);
			dstring_reset(&input);
			return enc; /* no error */
		}
	}
	enc = xalloc(DviEncoding);
	enc->name = mdvi_strdup(name);
	enc->filename = filename;
	enc->links = 0;
	enc->offset = offset;
	enc->private = NULL;
	enc->vector = NULL;
	mdvi_hash_init(&enc->nametab);
	dstring_reset(&input);
	if(default_encoding == NULL)
		default_encoding = enc;
	mdvi_hash_add(&enctable, MDVI_KEY(enc->name),
		enc, MDVI_HASH_UNCHECKED);
	mdvi_hash_add(&enctable_file, MDVI_KEY(mdvi_strdup(basefile)),
		enc, MDVI_HASH_REPLACE);
	listh_prepend(&encodings, LIST(enc));
	DEBUG((DBG_FMAP, "%s: encoding `%s' registered\n",
		basefile, enc->name));
	return enc;
}

DviEncoding *mdvi_request_encoding(const char *name)
{
	DviEncoding *enc = find_encoding(name);

	if(enc == NULL) {
		DEBUG((DBG_FMAP, "%s: encoding not found, returning default `%s'\n",
			name, default_encoding->name));
		return default_encoding;
	}
	/* we don't keep reference counts for this */
	if(enc == tex_text_encoding)
		return enc;
	if(!enc->private && read_encoding(enc) < 0)
		return NULL;
	enc->links++;

	/* if the hash table is empty, rebuild it */
	if(enc->nametab.nkeys == 0) {
		int	i;

		DEBUG((DBG_FMAP, "%s: rehashing\n", enc->name));
		for(i = 0; i < 256; i++) {
			if(enc->vector[i] == NULL)
				continue;
			mdvi_hash_add(&enc->nametab,
				MDVI_KEY(enc->vector[i]),
				(DviHashKey)Int2Ptr(i),
				MDVI_HASH_REPLACE);
		}
	}
	return enc;
}

void	mdvi_release_encoding(DviEncoding *enc, int should_free)
{
	/* ignore our static encoding */
	if(enc == tex_text_encoding)
		return;
	if(!enc->links || --enc->links > 0 || !should_free)
		return;
	DEBUG((DBG_FMAP, "%s: resetting encoding vector\n", enc->name));
	mdvi_hash_reset(&enc->nametab, 1); /* we'll reuse it */
}

int	mdvi_encode_glyph(DviEncoding *enc, const char *name)
{
	void	*data;

	data = mdvi_hash_lookup(&enc->nametab, MDVI_KEY(name));
	if(data == NULL)
		return -1;
	/* we added +1 to the hashed index just to distinguish
	 * a failed lookup from a zero index. Adjust it now. */
	return (Ptr2Int(data) - 1);
}

/****************
 * Fontmaps     *
 ****************/

static void parse_spec(DviFontMapEnt *ent, char *spec)
{
	char	*arg, *command;

	/* this is a ridiculously simple parser, and recognizes only
	 * things of the form <argument> <command>. Of these, only
	 * command=SlantFont, ExtendFont and ReEncodeFont are handled */
	while(*spec) {
		arg = getword(spec, " \t", &spec);
		if(*spec) *spec++ = 0;
		command = getword(spec, " \t", &spec);
		if(*spec) *spec++ = 0;
		if(!arg || !command)
			continue;
		if(STREQ(command, "SlantFont")) {
			double	x = 10000 * strtod(arg, 0);

			/* SFROUND evaluates arguments twice */
			ent->slant = SFROUND(x);
		} else if(STREQ(command, "ExtendFont")) {
			double	x = 10000 * strtod(arg, 0);

			ent->extend = SFROUND(x);
		} else if(STREQ(command, "ReEncodeFont")) {
			if(ent->encoding)
				mdvi_free(ent->encoding);
			ent->encoding = mdvi_strdup(arg);
		}
	}
}

#if 0
static void print_ent(DviFontMapEnt *ent)
{
	printf("Entry for `%s':\n", ent->fontname);
	printf("  PS name: %s\n", ent->psname ? ent->psname : "(none)");
	printf("  Encoding: %s\n", ent->encoding ? ent->encoding : "(default)");
	printf("  EncFile: %s\n", ent->encfile ? ent->encfile : "(none)");
	printf("  FontFile: %s\n", ent->fontfile ? ent->fontfile : "(same)");
	printf("  Extend: %ld\n", ent->extend);
	printf("  Slant: %ld\n", ent->slant);
}
#endif

DviFontMapEnt	*mdvi_load_fontmap(const char *file)
{
	char	*ptr;
	FILE	*in;
	int	lineno = 1;
	Dstring	input;
	ListHead list;
	DviFontMapEnt *ent;
	DviEncoding	*last_encoding;
	char	*last_encfile;

	ptr = kpse_find_file(file, kpse_program_text_format, 0);
	if(ptr == NULL)
		ptr = kpse_find_file(file, kpse_tex_ps_header_format, 0);
	if(ptr == NULL)
		ptr = kpse_find_file(file, kpse_dvips_config_format, 0);
	if(ptr == NULL)
		in = fopen(file, "rb");
	else {
		in = fopen(ptr, "rb");
		mdvi_free(ptr);
	}
	if(in == NULL)
		return NULL;

	ent = NULL;
	listh_init(&list);
	dstring_init(&input);
	last_encoding = NULL;
	last_encfile  = NULL;

	while((ptr = dgets(&input, in)) != NULL) {
		char	*font_file;
		char	*tex_name;
		char	*ps_name;
		char	*vec_name;
		int	is_encoding;
		DviEncoding *enc;

		lineno++;
		SKIPSP(ptr);

		/* we skip what dvips does */
		if(*ptr <= ' ' || *ptr == '*' || *ptr == '#' ||
		   *ptr == ';' || *ptr == '%')
			continue;

		font_file   = NULL;
		tex_name    = NULL;
		ps_name     = NULL;
		vec_name    = NULL;
		is_encoding = 0;

		if(ent == NULL) {
			ent = xalloc(DviFontMapEnt);
			ent->encoding = NULL;
			ent->slant = 0;
			ent->extend = 0;
		}
		while(*ptr) {
			char	*hdr_name = NULL;

			while(*ptr && *ptr <= ' ')
				ptr++;
			if(*ptr == 0)
				break;
			if(*ptr == '"') {
				char	*str;

				str = getstring(ptr, " \t", &ptr);
				if(*ptr) *ptr++ = 0;
				parse_spec(ent, str);
				continue;
			} else if(*ptr == '<') {
				ptr++;
				if(*ptr == '<')
					ptr++;
				else if(*ptr == '[') {
					is_encoding = 1;
					ptr++;
				}
				SKIPSP(ptr);
				hdr_name = ptr;
			} else if(!tex_name)
				tex_name = ptr;
			else if(!ps_name)
				ps_name = ptr;
			else
				hdr_name = ptr;

			/* get next word */
			getword(ptr, " \t", &ptr);
			if(*ptr) *ptr++ = 0;

			if(hdr_name) {
				const char *ext = file_extension(hdr_name);

				if(is_encoding || (ext && STRCEQ(ext, "enc")))
					vec_name = hdr_name;
				else
					font_file = hdr_name;
			}
		}

		if(tex_name == NULL)
			continue;
		ent->fontname = mdvi_strdup(tex_name);
		ent->psname   = ps_name   ? mdvi_strdup(ps_name)   : NULL;
		ent->fontfile = font_file ? mdvi_strdup(font_file) : NULL;
		ent->encfile  = vec_name  ? mdvi_strdup(vec_name)  : NULL;
		ent->fullfile = NULL;
		enc = NULL; /* we don't have this yet */

		/* if we have an encoding file, register it */
		if(ent->encfile) {
			/* register_encoding is smart enough not to load the
			 * same file twice */
			if(!last_encfile || !STREQ(last_encfile, ent->encfile)) {
				last_encfile  = ent->encfile;
				last_encoding = register_encoding(ent->encfile, 1);
			}
			enc = last_encoding;
		}
		if(ent->encfile && enc){
			if(ent->encoding && !STREQ(ent->encoding, enc->name)) {
				mdvi_warning(
	_("%s: %d: [%s] requested encoding `%s' does not match vector `%s'\n"),
					file, lineno, ent->encfile,
					ent->encoding, enc->name);
			} else if(!ent->encoding)
				ent->encoding = mdvi_strdup(enc->name);
		}

		/* add it to the list */
		/*print_ent(ent);*/
		listh_append(&list, LIST(ent));
		ent = NULL;
	}
	dstring_reset(&input);
	fclose(in);

	return (DviFontMapEnt *)list.head;
}

static void free_ent(DviFontMapEnt *ent)
{
	ASSERT(ent->fontname != NULL);
	mdvi_free(ent->fontname);
	if(ent->psname)
		mdvi_free(ent->psname);
	if(ent->fontfile)
		mdvi_free(ent->fontfile);
	if(ent->encoding)
		mdvi_free(ent->encoding);
	if(ent->encfile)
		mdvi_free(ent->encfile);
	if(ent->fullfile)
		mdvi_free(ent->fullfile);
	mdvi_free(ent);
}

void	mdvi_install_fontmap(DviFontMapEnt *head)
{
	DviFontMapEnt *ent, *next;

	for(ent = head; ent; ent = next) {
		/* add all the entries, overriding old ones */
		DviFontMapEnt *old;

		old = (DviFontMapEnt *)
			mdvi_hash_remove(&maptable, MDVI_KEY(ent->fontname));
		if(old != NULL) {
			DEBUG((DBG_FMAP, "%s: overriding fontmap entry\n",
				old->fontname));
			listh_remove(&fontmaps, LIST(old));
			free_ent(old);
		}
		next = ent->next;
		mdvi_hash_add(&maptable, MDVI_KEY(ent->fontname),
			ent, MDVI_HASH_UNCHECKED);
		listh_append(&fontmaps, LIST(ent));
	}
}

static void init_static_encoding(void)
{
	DviEncoding	*encoding;
	int	i;

	DEBUG((DBG_FMAP, "installing static TeX text encoding\n"));
	encoding = xalloc(DviEncoding);
	encoding->private  = "";
	encoding->filename = "";
	encoding->name     = "TeXTextEncoding";
	encoding->vector   = tex_text_vector;
	encoding->links    = 1;
	encoding->offset   = 0;
	mdvi_hash_create(&encoding->nametab, ENCNAME_HASH_SIZE);
	for(i = 0; i < 256; i++) {
		if(encoding->vector[i]) {
			mdvi_hash_add(&encoding->nametab,
				MDVI_KEY(encoding->vector[i]),
				(DviHashKey)Int2Ptr(i),
				MDVI_HASH_UNCHECKED);
		}
	}
	ASSERT_VALUE(encodings.count, 0);
	mdvi_hash_create(&enctable, ENC_HASH_SIZE);
	mdvi_hash_create(&enctable_file, ENC_HASH_SIZE);
	enctable_file.hash_free = file_hash_free;
	mdvi_hash_add(&enctable, MDVI_KEY(encoding->name),
		encoding, MDVI_HASH_UNCHECKED);
	listh_prepend(&encodings, LIST(encoding));
	tex_text_encoding = encoding;
	default_encoding = tex_text_encoding;
}

static int	mdvi_set_default_encoding(const char *name)
{
	DviEncoding *enc, *old;

	enc = find_encoding(name);
	if(enc == NULL)
		return -1;
	if(enc == default_encoding)
		return 0;
	/* this will read it from file if necessary,
	 * but it can fail if the file is corrupted */
	enc = mdvi_request_encoding(name);
	if(enc == NULL)
		return -1;
	old = default_encoding;
	default_encoding = enc;
	if(old != tex_text_encoding)
		mdvi_release_encoding(old, 1);
	return 0;
}

static int	mdvi_init_fontmaps(void)
{
	char	*file;
	char	*line;
	FILE	*in;
	Dstring	input;
	int	count = 0;
	const char	*config;

	if(fontmaps_loaded)
		return 0;
	/* we will only try this once */
	fontmaps_loaded = 1;

	DEBUG((DBG_FMAP, "reading fontmaps\n"));

	/* make sure the static encoding is there */
	init_static_encoding();

	/* create the fontmap hash table */
	mdvi_hash_create(&maptable, MAP_HASH_SIZE);

	/* get the name of our configuration file */
	config = kpse_cnf_get("mdvi-config");
	if(config == NULL)
		config = MDVI_DEFAULT_CONFIG;
	/* let's ask kpathsea for the file first */
	file = kpse_find_file(config, kpse_program_text_format, 0);
	if(file == NULL)
		in = fopen(config, "rb");
	else {
		in = fopen(file, "rb");
		mdvi_free(file);
	}
	if(in == NULL)
		return -1;
	dstring_init(&input);
	while((line = dgets(&input, in)) != NULL) {
		char	*arg, *map_file;

		SKIPSP(line);
		if(*line < ' ' || *line == '#' || *line == '%')
			continue;
		if(STRNEQ(line, "fontmap", 7)) {
			DviFontMapEnt *ent;

			arg = getstring(line + 7, " \t", &line); *line = 0;
			DEBUG((DBG_FMAP, "%s: loading fontmap\n", arg));
			ent = mdvi_load_fontmap(arg);
			if(ent == NULL) {
				map_file = kpse_find_file(arg, kpse_fontmap_format, 0);
				if (map_file)
					ent = mdvi_load_fontmap(map_file);
			}
			if(ent == NULL)
				mdvi_warning(_("%s: could not load fontmap\n"), arg);
			else {
				DEBUG((DBG_FMAP,
					"%s: installing fontmap\n", arg));
				mdvi_install_fontmap(ent);
				count++;
			}
		} else if(STRNEQ(line, "encoding", 8)) {
			arg = getstring(line + 8, " \t", &line); *line = 0;
			if(arg && *arg)
				register_encoding(arg, 1);
		} else if(STRNEQ(line, "default-encoding", 16)) {
			arg = getstring(line + 16, " \t", &line); *line = 0;
			if(mdvi_set_default_encoding(arg) < 0)
				mdvi_warning(_("%s: could not set as default encoding\n"),
					     arg);
		} else if(STRNEQ(line, "psfontpath", 10)) {
			arg = getstring(line + 11, " \t", &line); *line = 0;
			if(!psinitialized)
				ps_init_default_paths();
			if(psfontdir)
				mdvi_free(psfontdir);
			psfontdir = kpse_path_expand(arg);
		} else if(STRNEQ(line, "pslibpath", 9)) {
			arg = getstring(line + 10, " \t", &line); *line = 0;
			if(!psinitialized)
				ps_init_default_paths();
			if(pslibdir)
				mdvi_free(pslibdir);
			pslibdir = kpse_path_expand(arg);
		} else if(STRNEQ(line, "psfontmap", 9)) {
			arg = getstring(line + 9, " \t", &line); *line = 0;
			if(mdvi_ps_read_fontmap(arg) < 0)
				mdvi_warning("%s: %s: could not read PS fontmap\n",
					     config, arg);
		}
	}
	fclose(in);
	dstring_reset(&input);
	fontmaps_loaded = 1;
	DEBUG((DBG_FMAP, "%d files installed, %d fontmaps\n",
		count, fontmaps.count));
	return count;
}

int	mdvi_query_fontmap(DviFontMapInfo *info, const char *fontname)
{
	DviFontMapEnt *ent;

	if(!fontmaps_loaded && mdvi_init_fontmaps() < 0)
		return -1;
	ent = (DviFontMapEnt *)mdvi_hash_lookup(&maptable, MDVI_KEY(fontname));

	if(ent == NULL)
		return -1;
	info->psname   = ent->psname;
	info->encoding = ent->encoding;
	info->fontfile = ent->fontfile;
	info->extend   = ent->extend;
	info->slant    = ent->slant;
	info->fullfile = ent->fullfile;

	return 0;
}

int	mdvi_add_fontmap_file(const char *name, const char *fullpath)
{
	DviFontMapEnt *ent;

	if(!fontmaps_loaded && mdvi_init_fontmaps() < 0)
		return -1;
	ent = (DviFontMapEnt *)mdvi_hash_lookup(&maptable, MDVI_KEY(name));
	if(ent == NULL)
		return -1;
	if(ent->fullfile)
		mdvi_free(ent->fullfile);
	ent->fullfile = mdvi_strdup(fullpath);
	return 0;
}


void	mdvi_flush_encodings(void)
{
	DviEncoding *enc;

	if(enctable.nbucks == 0)
		return;

	DEBUG((DBG_FMAP, "flushing %d encodings\n", encodings.count));
	/* asked to remove all encodings */
	for(; (enc = (DviEncoding *)encodings.head); ) {
		encodings.head = LIST(enc->next);
		if((enc != tex_text_encoding && enc->links) || enc->links > 1) {
			mdvi_warning(_("encoding vector `%s' is in use\n"),
				     enc->name);
		}
		destroy_encoding(enc);
	}
	/* destroy the static encoding */
	if(tex_text_encoding->nametab.buckets)
		mdvi_hash_reset(&tex_text_encoding->nametab, 0);
	mdvi_hash_reset(&enctable, 0);
	mdvi_hash_reset(&enctable_file, 0);
}

void	mdvi_flush_fontmaps(void)
{
	DviFontMapEnt *ent;

	if(!fontmaps_loaded)
		return;

	DEBUG((DBG_FMAP, "flushing %d fontmaps\n", fontmaps.count));
	for(; (ent = (DviFontMapEnt *)fontmaps.head); ) {
		fontmaps.head = LIST(ent->next);
		free_ent(ent);
	}
	mdvi_hash_reset(&maptable, 0);
	fontmaps_loaded = 0;
}

/* reading of PS fontmaps */

void	ps_init_default_paths(void)
{
	char	*kppath;
	char	*kfpath;

	ASSERT(psinitialized == 0);

	kppath = getenv("GS_LIB");
	kfpath = getenv("GS_FONTPATH");

	if(kppath != NULL)
		pslibdir = kpse_path_expand(kppath);
	if(kfpath != NULL)
		psfontdir = kpse_path_expand(kfpath);

	listh_init(&psfonts);
	mdvi_hash_create(&pstable, PSMAP_HASH_SIZE);
	psinitialized = 1;
}

int	mdvi_ps_read_fontmap(const char *name)
{
	char	*fullname;
	FILE	*in;
	Dstring	dstr;
	char	*line;
	int	count = 0;

	if(!psinitialized)
		ps_init_default_paths();
	if(pslibdir)
		fullname = kpse_path_search(pslibdir, name, 1);
	else
		fullname = (char *)name;
	in = fopen(fullname, "rb");
	if(in == NULL) {
		if(fullname != name)
			mdvi_free(fullname);
		return -1;
	}
	dstring_init(&dstr);

	while((line = dgets(&dstr, in)) != NULL) {
		char	*name;
		char	*mapname;
		const char *ext;
		PSFontMap *ps;

		SKIPSP(line);
		/* we're looking for lines of the form
		 *  /FONT-NAME    (fontfile)
		 *  /FONT-NAME    /FONT-ALIAS
		 */
		if(*line != '/')
			continue;
		name = getword(line + 1, " \t", &line);
		if(*line) *line++ = 0;
		mapname = getword(line, " \t", &line);
		if(*line) *line++ = 0;

		if(!name || !mapname || !*name)
			continue;
		if(*mapname == '(') {
			char	*end;

			mapname++;
			for(end = mapname; *end && *end != ')'; end++);
			*end = 0;
		}
		if(!*mapname)
			continue;
		/* dont add `.gsf' fonts, which require a full blown
		 * PostScript interpreter */
		ext = file_extension(mapname);
		if(ext && STREQ(ext, "gsf")) {
			DEBUG((DBG_FMAP, "(ps) %s: font `%s' ignored\n",
				name, mapname));
			continue;
		}
		ps = (PSFontMap *)mdvi_hash_lookup(&pstable, MDVI_KEY(name));
		if(ps != NULL) {
			if(STREQ(ps->mapname, mapname))
				continue;
			DEBUG((DBG_FMAP,
				"(ps) replacing font `%s' (%s) by `%s'\n",
				name, ps->mapname, mapname));
			mdvi_free(ps->mapname);
			ps->mapname = mdvi_strdup(mapname);
			if(ps->fullname) {
				mdvi_free(ps->fullname);
				ps->fullname = NULL;
			}
		} else {
			DEBUG((DBG_FMAP, "(ps) adding font `%s' as `%s'\n",
				name, mapname));
			ps = xalloc(PSFontMap);
			ps->psname   = mdvi_strdup(name);
			ps->mapname  = mdvi_strdup(mapname);
			ps->fullname = NULL;
			listh_append(&psfonts, LIST(ps));
			mdvi_hash_add(&pstable, MDVI_KEY(ps->psname),
				ps, MDVI_HASH_UNCHECKED);
			count++;
		}
	}
	fclose(in);
	dstring_reset(&dstr);

	DEBUG((DBG_FMAP, "(ps) %s: %d PostScript fonts registered\n",
		fullname, count));
	return 0;
}

void	mdvi_ps_flush_fonts(void)
{
	PSFontMap *map;

	if(!psinitialized)
		return;
	DEBUG((DBG_FMAP, "(ps) flushing PS font map (%d) entries\n",
		psfonts.count));
	mdvi_hash_reset(&pstable, 0);
	for(; (map = (PSFontMap *)psfonts.head); ) {
		psfonts.head = LIST(map->next);
		mdvi_free(map->psname);
		mdvi_free(map->mapname);
		if(map->fullname)
			mdvi_free(map->fullname);
		mdvi_free(map);
	}
	listh_init(&psfonts);
	if(pslibdir) {
		mdvi_free(pslibdir);
		pslibdir = NULL;
	}
	if(psfontdir) {
		mdvi_free(psfontdir);
		psfontdir = NULL;
	}
	psinitialized = 0;
}

char	*mdvi_ps_find_font(const char *psname)
{
	PSFontMap *map, *smap;
	char	*filename;
	int	recursion_limit = 32;

	DEBUG((DBG_FMAP, "(ps) resolving PS font `%s'\n", psname));
	if(!psinitialized)
		return NULL;
	map = (PSFontMap *)mdvi_hash_lookup(&pstable, MDVI_KEY(psname));
	if(map == NULL)
		return NULL;
	if(map->fullname)
		return mdvi_strdup(map->fullname);

	/* is it an alias? */
	smap = map;
	while(recursion_limit-- > 0 && smap && *smap->mapname == '/')
		smap = (PSFontMap *)mdvi_hash_lookup(&pstable,
			MDVI_KEY(smap->mapname + 1));
	if(smap == NULL) {
		if(recursion_limit == 0)
			DEBUG((DBG_FMAP,
				"(ps) %s: possible loop in PS font map\n",
				psname));
		return NULL;
	}

	if(psfontdir)
		filename = kpse_path_search(psfontdir, smap->mapname, 1);
	else if(file_exists(map->mapname))
		filename = mdvi_strdup(map->mapname);
	else
		filename = NULL;
	if(filename)
		map->fullname = mdvi_strdup(filename);

	return filename;
}

/*
 * To get metric info for a font, we proceed as follows:
 *  - We try to find NAME.<tfm,ofm,afm>.
 *  - We query the fontmap for NAME.
 *  - We get back a PSNAME, and use to find the file in the PS font map.
 *  - We get the PSFONT file name, replace its extension by "afm" and
 *  lookup the file in GS's font search path.
 *  - We finally read the data, transform it as specified in our font map,
 *  and return it to the caller. The new data is left in the font metrics
 *  cache, so the next time it will be found at the first step (when we look
 *  up NAME.afm).
 *
 * The name `_ps_' in this function is not meant to imply that it can be
 * used for Type1 fonts only. It should be usable for TrueType fonts as well.
 *
 * The returned metric info is subjected to the same caching mechanism as
 * all the other metric data, as returned by get_font_metrics(). One should
 * not modify the returned data at all, and it should be disposed with
 * free_font_metrics().
 */
TFMInfo *mdvi_ps_get_metrics(const char *fontname)
{
	TFMInfo *info;
	DviFontMapInfo map;
	char	buffer[64]; /* to avoid mallocs */
	char	*psfont;
	char	*basefile;
	char	*afmfile;
	char	*ext;
	int	baselen;
	int	nc;
	TFMChar	*ch;
	double	efactor;
	double	sfactor;

	DEBUG((DBG_FMAP, "(ps) %s: looking for metric data\n", fontname));
	info = get_font_metrics(fontname, DviFontAny, NULL);
	if(info != NULL)
		return info;

	/* query the fontmap */
	if(mdvi_query_fontmap(&map, fontname) < 0 || !map.psname)
		return NULL;

	/* get the PS font */
	psfont = mdvi_ps_find_font(map.psname);
	if(psfont == NULL)
		return NULL;
	DEBUG((DBG_FMAP, "(ps) %s: found as PS font `%s'\n",
		fontname, psfont));
	/* replace its extension */
	basefile = strrchr(psfont, '/');
	if(basefile == NULL)
		basefile = psfont;
	baselen = strlen(basefile);
	ext = strrchr(basefile, '.');
	if(ext != NULL)
		*ext = 0;
	if(baselen + 4 < 64)
		afmfile = &buffer[0];
	else
		afmfile = mdvi_malloc(baselen + 5);
	strcpy(afmfile, basefile);
	strcpy(afmfile + baselen, ".afm");
	/* we don't need this anymore */
	mdvi_free(psfont);
	DEBUG((DBG_FMAP, "(ps) %s: looking for `%s'\n",
		fontname, afmfile));
	/* lookup the file */
	psfont = kpse_path_search(psfontdir, afmfile, 1);
	/* don't need this anymore */
	if(afmfile != &buffer[0])
		mdvi_free(afmfile);
	if(psfont != NULL) {
		info = get_font_metrics(fontname, DviFontAFM, psfont);
		mdvi_free(psfont);
	} else
		info = NULL;
	if(info == NULL || (!map.extend && !map.slant))
		return info;

	/*
	 * transform the data as prescribed -- keep in mind that `info'
	 * points to CACHED data, so we're modifying the metric cache
	 * in place.
	 */

#define DROUND(x)	((x) >= 0 ? floor((x) + 0.5) : ceil((x) - 0.5))
#define TRANSFORM(x,y)	DROUND(efactor * (x) + sfactor * (y))

	efactor = (double)map.extend / 10000.0;
	sfactor = (double)map.slant / 10000.0;
	DEBUG((DBG_FMAP, "(ps) %s: applying extend=%f, slant=%f\n",
		efactor, sfactor));

	nc = info->hic - info->loc + 1;
	for(ch = info->chars; ch < info->chars + nc; ch++) {
		/* the AFM bounding box is:
		 *    wx = ch->advance
		 *   llx = ch->left
		 *   lly = -ch->depth
		 *   urx = ch->right
		 *   ury = ch->height
		 * what we do here is transform wx, llx, and urx by
		 *         newX = efactor * oldX + sfactor * oldY
		 * where for `wx' oldY = 0. Also, these numbers are all in
		 * TFM units (i.e. TFM's fix-words, which is just the actual
		 * number times 2^20, no need to do anything to it).
		 */
		if(ch->present) {
			ch->advance = TRANSFORM(ch->advance, 0);
			ch->left    = TRANSFORM(ch->left, -ch->depth);
			ch->right   = TRANSFORM(ch->right, ch->height);
		}
	}

	return info;
}
