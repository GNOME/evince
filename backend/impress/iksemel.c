/* iksemel (XML parser for Jabber)
** Copyright (C) 2000-2003 Gurer Ozen <madcat@e-kolay.net>
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU Lesser General Public License.
*/

/* minimum sax buffer size */
#define SAX_BUFFER_MIN_SIZE 128

/* sax parser structure plus extra data of dom parser */
#define DEFAULT_DOM_CHUNK_SIZE 256

/* sax parser structure plus extra data of stream parser */
#define DEFAULT_STREAM_CHUNK_SIZE 256

/* iks structure, its data, child iks structures, for stream parsing */
#define DEFAULT_IKS_CHUNK_SIZE 1024

/* iks structure, its data, child iks structures, for file parsing */
#define DEFAULT_DOM_IKS_CHUNK_SIZE 2048

/* rule structure and from/to/id/ns strings */
#define DEFAULT_RULE_CHUNK_SIZE 128

/* file is read by blocks with this size */
#define FILE_IO_BUF_SIZE 4096

/* network receive buffer */
#define NET_IO_BUF_SIZE 4096
/* iksemel (XML parser for Jabber)
** Copyright (C) 2000-2003 Gurer Ozen <madcat@e-kolay.net>
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU Lesser General Public License.
*/

#include <config.h>
#include <errno.h>

#include "common.h"
#include "iksemel.h"

/*****  malloc wrapper  *****/

static void *(*my_malloc_func)(size_t size);
static void (*my_free_func)(void *ptr);

void *
iks_malloc (size_t size)
{
	if (my_malloc_func)
		return my_malloc_func (size);
	else
		return malloc (size);
}

void
iks_free (void *ptr)
{
	if (my_free_func)
		my_free_func (ptr);
	else
		free (ptr);
}

void
iks_set_mem_funcs (void *(*malloc_func)(size_t size), void (*free_func)(void *ptr))
{
	my_malloc_func = malloc_func;
	my_free_func = free_func;
}

/*****  NULL-safe Functions  *****/

char *
iks_strdup (const char *src)
{
	if (src) return strdup(src);
	return NULL;
}

char *
iks_strcat (char *dest, const char *src)
{
	size_t len;

	if (!src) return dest;

	len = strlen (src);
	memcpy (dest, src, len);
	dest[len] = '\0';
	return dest + len;
}

int
iks_strcmp (const char *a, const char *b)
{
	if (!a || !b) return -1;
	return strcmp (a, b);
}

int
iks_strcasecmp (const char *a, const char *b)
{
	if (!a || !b) return -1;
	return strcasecmp (a, b);
}

int
iks_strncmp (const char *a, const char *b, size_t n)
{
	if (!a || !b) return -1;
	return strncmp (a, b, n);
}

int
iks_strncasecmp (const char *a, const char *b, size_t n)
{
	if (!a || !b) return -1;
	return strncasecmp (a, b, n);
}

size_t
iks_strlen (const char *src)
{
	if (!src) return 0;
	return strlen (src);
}

/*****  XML Escaping  *****/

char *
iks_escape (ikstack *s, char *src, size_t len)
{
	char *ret;
	int i, j, nlen;

	if (!src || !s) return NULL;
	if (len == -1) len = strlen (src);

	nlen = len;
	for (i=0; i<len; i++) {
		switch (src[i]) {
		case '&': nlen += 4; break;
		case '<': nlen += 3; break;
		case '>': nlen += 3; break;
		case '\'': nlen += 5; break;
		case '"': nlen += 5; break;
		}
	}
	if (len == nlen) return src;

	ret = iks_stack_alloc (s, nlen + 1);
	if (!ret) return NULL;

	for (i=j=0; i<len; i++) {
		switch (src[i]) {
		case '&': memcpy (&ret[j], "&amp;", 5); j += 5; break;
		case '\'': memcpy (&ret[j], "&apos;", 6); j += 6; break;
		case '"': memcpy (&ret[j], "&quot;", 6); j += 6; break;
		case '<': memcpy (&ret[j], "&lt;", 4); j += 4; break;
		case '>': memcpy (&ret[j], "&gt;", 4); j += 4; break;
		default: ret[j++] = src[i];
		}
	}
	ret[j] = '\0';

	return ret;
}

char *
iks_unescape (ikstack *s, char *src, size_t len)
{
	int i,j;
	char *ret;

	if (!s || !src) return NULL;
	if (!strchr (src, '&')) return src;
	if (len == -1) len = strlen (src);

	ret = iks_stack_alloc (s, len + 1);
	if (!ret) return NULL;

	for (i=j=0; i<len; i++) {
		if (src[i] == '&') {
			i++;
			if (strncmp (&src[i], "amp;", 4) == 0) {
				ret[j] = '&';
				i += 3;
			} else if (strncmp (&src[i], "quot;", 5) == 0) {
				ret[j] = '"';
				i += 4;
			} else if (strncmp (&src[i], "apos;", 5) == 0) {
				ret[j] = '\'';
				i += 4;
			} else if (strncmp (&src[i], "lt;", 3) == 0) {
				ret[j] = '<';
				i += 2;
			} else if (strncmp (&src[i], "gt;", 3) == 0) {
				ret[j] = '>';
				i += 2;
			} else {
				ret[j] = src[--i];
			}
		} else {
			ret[j] = src[i];
		}
		j++;
	}
	ret[j] = '\0';

	return ret;
}
/* iksemel (XML parser for Jabber)
** Copyright (C) 2000-2004 Gurer Ozen <madcat@e-kolay.net>
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU Lesser General Public License.
*/

#include "common.h"
#include "iksemel.h"

struct align_test { char a; double b; };
#define DEFAULT_ALIGNMENT  ((size_t) ((char *) &((struct align_test *) 0)->b - (char *) 0))
#define ALIGN_MASK ( DEFAULT_ALIGNMENT - 1 )
#define MIN_CHUNK_SIZE ( DEFAULT_ALIGNMENT * 8 )
#define MIN_ALLOC_SIZE DEFAULT_ALIGNMENT
#define ALIGN(x) ( (x) + (DEFAULT_ALIGNMENT - ( (x) & ALIGN_MASK)) )

typedef struct ikschunk_struct {
	struct ikschunk_struct *next;
	size_t size;
	size_t used;
	size_t last;
	char data[4];
} ikschunk;

struct ikstack_struct {
	size_t allocated;
	ikschunk *meta;
	ikschunk *data;
};

static ikschunk *
find_space (ikstack *s, ikschunk *c, size_t size)
{
	/* FIXME: dont use *2 after over allocated chunks */
	while (1) {
		if (c->size - c->used >= size) return c;
		if (!c->next) {
			if ((c->size * 2) > size) size = c->size * 2;
			c->next = iks_malloc (sizeof (ikschunk) + size);
			if (!c->next) return NULL;
			s->allocated += sizeof (ikschunk) + size;
			c = c->next;
			c->next = NULL;
			c->size = size;
			c->used = 0;
			c->last = (size_t) -1;
			return c;
		}
		c = c->next;
	}
	return NULL;
}

ikstack *
iks_stack_new (size_t meta_chunk, size_t data_chunk)
{
	ikstack *s;
	size_t len;

	if (meta_chunk < MIN_CHUNK_SIZE) meta_chunk = MIN_CHUNK_SIZE;
	if (meta_chunk & ALIGN_MASK) meta_chunk = ALIGN (meta_chunk);
	if (data_chunk < MIN_CHUNK_SIZE) data_chunk = MIN_CHUNK_SIZE;
	if (data_chunk & ALIGN_MASK) data_chunk = ALIGN (data_chunk);

	len = sizeof (ikstack) + meta_chunk + data_chunk + (sizeof (ikschunk) * 2);
	s = iks_malloc (len);
	if (!s) return NULL;
	s->allocated = len;
	s->meta = (ikschunk *) ((char *) s + sizeof (ikstack));
	s->meta->next = NULL;
	s->meta->size = meta_chunk;
	s->meta->used = 0;
	s->meta->last = (size_t) -1;
	s->data = (ikschunk *) ((char *) s + sizeof (ikstack) + sizeof (ikschunk) + meta_chunk);
	s->data->next = NULL;
	s->data->size = data_chunk;
	s->data->used = 0;
	s->data->last = (size_t) -1;
	return s;
}

void *
iks_stack_alloc (ikstack *s, size_t size)
{
	ikschunk *c;
	void *mem;

	if (size < MIN_ALLOC_SIZE) size = MIN_ALLOC_SIZE;
	if (size & ALIGN_MASK) size = ALIGN (size);

	c = find_space (s, s->meta, size);
	if (!c) return NULL;
	mem = c->data + c->used;
	c->used += size;
	return mem;
}

char *
iks_stack_strdup (ikstack *s, const char *src, size_t len)
{
	ikschunk *c;
	char *dest;

	if (!src) return NULL;
	if (0 == len) len = strlen (src);

	c = find_space (s, s->data, len + 1);
	if (!c) return NULL;
	dest = c->data + c->used;
	c->last = c->used;
	c->used += len + 1;
	memcpy (dest, src, len);
	dest[len] = '\0';
	return dest;
}

char *
iks_stack_strcat (ikstack *s, char *old, size_t old_len, const char *src, size_t src_len)
{
	char *ret;
	ikschunk *c;

	if (!old) {
		return iks_stack_strdup (s, src, src_len);
	}
	if (0 == old_len) old_len = strlen (old);
	if (0 == src_len) src_len = strlen (src);

	for (c = s->data; c; c = c->next) {
		if (c->data + c->last == old) break;
	}
	if (!c) {
		c = find_space (s, s->data, old_len + src_len + 1);
		if (!c) return NULL;
		ret = c->data + c->used;
		c->last = c->used;
		c->used += old_len + src_len + 1;
		memcpy (ret, old, old_len);
		memcpy (ret + old_len, src, src_len);
		ret[old_len + src_len] = '\0';
		return ret;
	}

	if (c->size - c->used > src_len) {
		ret = c->data + c->last;
		memcpy (ret + old_len, src, src_len);
		c->used += src_len;
		ret[old_len + src_len] = '\0';
	} else {
		/* FIXME: decrease c->used before moving string to new place */
		c = find_space (s, s->data, old_len + src_len + 1);
		if (!c) return NULL;
		c->last = c->used;
		ret = c->data + c->used;
		memcpy (ret, old, old_len);
		c->used += old_len;
		memcpy (c->data + c->used, src, src_len);
		c->used += src_len;
		c->data[c->used] = '\0';
		c->used++;
	}
	return ret;
}

void
iks_stack_stat (ikstack *s, size_t *allocated, size_t *used)
{
	ikschunk *c;

	if (allocated) {
		*allocated = s->allocated;
	}
	if (used) {
		*used = 0;
		for (c = s->meta; c; c = c->next) {
			(*used) += c->used;
		}
		for (c = s->data; c; c = c->next) {
			(*used) += c->used;
		}
	}
}

void
iks_stack_delete (ikstack *s)
{
	ikschunk *c, *tmp;

	c = s->meta->next;
	while (c) {
		tmp = c->next;
		iks_free (c);
		c = tmp;
	}
	c = s->data->next;
	while (c) {
		tmp = c->next;
		iks_free (c);
		c = tmp;
	}
	iks_free (s);
}
/* iksemel (XML parser for Jabber)
** Copyright (C) 2000-2004 Gurer Ozen <madcat@e-kolay.net>
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU Lesser General Public License.
*/

#include "common.h"
#include "iksemel.h"

enum cons_e {
	C_CDATA = 0,
	C_TAG_START,
	C_TAG,
	C_TAG_END,
	C_ATTRIBUTE,
	C_ATTRIBUTE_1,
	C_ATTRIBUTE_2,
	C_VALUE,
	C_VALUE_APOS,
	C_VALUE_QUOT,
	C_WHITESPACE,
	C_ENTITY,
	C_COMMENT,
	C_COMMENT_1,
	C_COMMENT_2,
	C_COMMENT_3,
	C_MARKUP,
	C_MARKUP_1,
	C_SECT,
	C_SECT_CDATA,
	C_SECT_CDATA_1,
	C_SECT_CDATA_2,
	C_SECT_CDATA_3,
	C_SECT_CDATA_4,
	C_SECT_CDATA_C,
	C_SECT_CDATA_E,
	C_SECT_CDATA_E2,
	C_PI
};

/* if you add a variable here, dont forget changing iks_parser_reset */
struct iksparser_struct {
	ikstack *s;
	void *user_data;
	iksTagHook *tagHook;
	iksCDataHook *cdataHook;
	iksDeleteHook *deleteHook;
	/* parser context */
	char *stack;
	size_t stack_pos;
	size_t stack_max;

	enum cons_e context;
	enum cons_e oldcontext;

	char *tag_name;
	enum ikstagtype tagtype;

	unsigned int attmax;
	unsigned int attcur;
	int attflag;
	char **atts;
	int valflag;

	unsigned int entpos;
	char entity[8];

	unsigned long nr_bytes;
	unsigned long nr_lines;

	int uni_max;
	int uni_len;
};

iksparser *
iks_sax_new (void *user_data, iksTagHook *tagHook, iksCDataHook *cdataHook)
{
	iksparser *prs;

	prs = iks_malloc (sizeof (iksparser));
	if (NULL == prs) return NULL;
	memset (prs, 0, sizeof (iksparser));
	prs->user_data = user_data;
	prs->tagHook = tagHook;
	prs->cdataHook = cdataHook;
	return prs;
}

iksparser *
iks_sax_extend (ikstack *s, void *user_data, iksTagHook *tagHook, iksCDataHook *cdataHook, iksDeleteHook *deleteHook)
{
	iksparser *prs;

	prs = iks_stack_alloc (s, sizeof (iksparser));
	if (NULL == prs) return NULL;
	memset (prs, 0, sizeof (iksparser));
	prs->s = s;
	prs->user_data = user_data;
	prs->tagHook = tagHook;
	prs->cdataHook = cdataHook;
	prs->deleteHook = deleteHook;
	return prs;
}

ikstack *
iks_parser_stack (iksparser *prs)
{
	return prs->s;
}

void *
iks_user_data (iksparser *prs)
{
	return prs->user_data;
}

unsigned long
iks_nr_bytes (iksparser *prs)
{
	return prs->nr_bytes;
}

unsigned long
iks_nr_lines (iksparser *prs)
{
	return prs->nr_lines;
}

#define IS_WHITESPACE(x) ' ' == (x) || '\t' == (x) || '\r' == (x) || '\n' == (x)
#define NOT_WHITESPACE(x) ' ' != (x) && '\t' != (x) && '\r' != (x) && '\n' != (x)

static int
stack_init (iksparser *prs)
{
	prs->stack = iks_malloc (128);
	if (!prs->stack) return 0;
	prs->stack_max = 128;
	prs->stack_pos = 0;
	return 1;
}

static int
stack_expand (iksparser *prs, int len)
{
	size_t need;
	off_t diff;
	char *tmp;
	need = len - (prs->stack_max - prs->stack_pos);
	if (need < prs->stack_max) {
		need = prs->stack_max * 2;
	} else {
		need = prs->stack_max + (need * 1.2);
	}
	tmp = iks_malloc (need);
	if (!tmp) return 0;
	diff = tmp - prs->stack;
	memcpy (tmp, prs->stack, prs->stack_max);
	iks_free (prs->stack);
	prs->stack = tmp;
	prs->stack_max = need;
	prs->tag_name += diff;
	if (prs->attflag != 0) {
		int i = 0;
		while (i < (prs->attmax * 2)) {
			if (prs->atts[i]) prs->atts[i] += diff;
			i++;
		}
	}
	return 1;
}

#define STACK_INIT \
	if (NULL == prs->stack && 0 == stack_init (prs)) return IKS_NOMEM

#define STACK_PUSH_START (prs->stack + prs->stack_pos)

#define STACK_PUSH(buf,len) \
{ \
	char *sbuf = (buf); \
	size_t slen = (len); \
	if (prs->stack_max - prs->stack_pos <= slen) { \
		if (0 == stack_expand (prs, slen)) return IKS_NOMEM; \
	} \
	memcpy (prs->stack + prs->stack_pos, sbuf, slen); \
	prs->stack_pos += slen; \
}

#define STACK_PUSH_END \
{ \
	if (prs->stack_pos >= prs->stack_max) { \
		if (0 == stack_expand (prs, 1)) return IKS_NOMEM; \
	} \
	prs->stack[prs->stack_pos] = '\0'; \
	prs->stack_pos++; \
}

static enum ikserror
sax_core (iksparser *prs, char *buf, int len)
{
	enum ikserror err;
	int pos = 0, old = 0, re, stack_old = -1;
	unsigned char c;

	while (pos < len) {
		re = 0;
		c = buf[pos];
		if (0 == c || 0xFE == c || 0xFF == c) return IKS_BADXML;
		if (prs->uni_max) {
			if ((c & 0xC0) != 0x80) return IKS_BADXML;
			prs->uni_len++;
			if (prs->uni_len == prs->uni_max) prs->uni_max = 0;
			goto cont;
		} else {
			if (c & 0x80) {
				unsigned char mask;
				if ((c & 0x60) == 0x40) {
					prs->uni_max = 2;
					mask = 0x1F;
				} else if ((c & 0x70) == 0x60) {
					prs->uni_max = 3;
					mask = 0x0F;
				} else if ((c & 0x78) == 0x70) {
					prs->uni_max = 4;
					mask = 0x07;
				} else if ((c & 0x7C) == 0x78) {
					prs->uni_max = 5;
					mask = 0x03;
				} else if ((c & 0x7E) == 0x7C) {
					prs->uni_max = 6;
					mask = 0x01;
				} else {
					return IKS_BADXML;
				}
				if ((c & mask) == 0) return IKS_BADXML;
				prs->uni_len = 1;
				if (stack_old == -1) stack_old = pos;
				goto cont;
			}
		}

		switch (prs->context) {
			case C_CDATA:
				if ('&' == c) {
					if (old < pos && prs->cdataHook) {
						err = prs->cdataHook (prs->user_data, &buf[old], pos - old);
						if (IKS_OK != err) return err;
					}
					prs->context = C_ENTITY;
					prs->entpos = 0;
					break;
				}
				if ('<' == c) {
					if (old < pos && prs->cdataHook) {
						err = prs->cdataHook (prs->user_data, &buf[old], pos - old);
						if (IKS_OK != err) return err;
					}
					STACK_INIT;
					prs->tag_name = STACK_PUSH_START;
					if (!prs->tag_name) return IKS_NOMEM;
					prs->context = C_TAG_START;
				}
				break;

			case C_TAG_START:
				prs->context = C_TAG;
				if ('/' == c) {
					prs->tagtype = IKS_CLOSE;
					break;
				}
				if ('?' == c) {
					prs->context = C_PI;
					break;
				}
				if ('!' == c) {
					prs->context = C_MARKUP;
					break;
				}
				prs->tagtype = IKS_OPEN;
				stack_old = pos;
				break;

			case C_TAG:
				if (IS_WHITESPACE(c)) {
					if (IKS_CLOSE == prs->tagtype)
						prs->oldcontext = C_TAG_END;
					else
						prs->oldcontext = C_ATTRIBUTE;
					prs->context = C_WHITESPACE;
					if (stack_old != -1) STACK_PUSH (buf + stack_old, pos - stack_old);
					stack_old = -1;
					STACK_PUSH_END;
					break;
				}
				if ('/' == c) {
					if (IKS_CLOSE == prs->tagtype) return IKS_BADXML;
					prs->tagtype = IKS_SINGLE;
					prs->context = C_TAG_END;
					if (stack_old != -1) STACK_PUSH (buf + stack_old, pos - stack_old);
					stack_old = -1;
					STACK_PUSH_END;
					break;
				}
				if ('>' == c) {
					prs->context = C_TAG_END;
					if (stack_old != -1) STACK_PUSH (buf + stack_old, pos - stack_old);
					stack_old = -1;
					STACK_PUSH_END;
					re = 1;
				}
				if (stack_old == -1) stack_old = pos;
				break;

			case C_TAG_END:
				if (c != '>') return IKS_BADXML;
				if (prs->tagHook) {
					char **tmp;
					if (prs->attcur == 0) tmp = NULL; else tmp = prs->atts;
					err = prs->tagHook (prs->user_data, prs->tag_name, tmp, prs->tagtype);
					if (IKS_OK != err) return err;
				}
				prs->stack_pos = 0;
				stack_old = -1;
				prs->attcur = 0;
				prs->attflag = 0;
				prs->context = C_CDATA;
				old = pos + 1;
				break;

			case C_ATTRIBUTE:
				if ('/' == c) {
					prs->tagtype = IKS_SINGLE;
					prs->context = C_TAG_END;
					break;
				}
				if ('>' == c) {
					prs->context = C_TAG_END;
					re = 1;
					break;
				}
				if (!prs->atts) {
					prs->attmax = 12;
					prs->atts = iks_malloc (sizeof(char *) * 2 * 12);
					if (!prs->atts) return IKS_NOMEM;
					memset (prs->atts, 0, sizeof(char *) * 2 * 12);
					prs->attcur = 0;
				} else {
					if (prs->attcur >= (prs->attmax * 2)) {
						void *tmp;
						prs->attmax += 12;
						tmp = iks_malloc (sizeof(char *) * (2 * prs->attmax + 1));
						if (!tmp) return IKS_NOMEM;
						memset (tmp, 0, sizeof(char *) * (2 * prs->attmax + 1));
						memcpy (tmp, prs->atts, sizeof(char *) * prs->attcur);
						iks_free (prs->atts);
						prs->atts = tmp;
					}
				}
				prs->attflag = 1;
				prs->atts[prs->attcur] = STACK_PUSH_START;
				stack_old = pos;
				prs->context = C_ATTRIBUTE_1;
				break;

			case C_ATTRIBUTE_1:
				if ('=' == c) {
					if (stack_old != -1) STACK_PUSH (buf + stack_old, pos - stack_old);
					stack_old = -1;
					STACK_PUSH_END;
					prs->context = C_VALUE;
					break;
				}
				if (stack_old == -1) stack_old = pos;
				break;

			case C_ATTRIBUTE_2:
				if ('/' == c) {
					prs->tagtype = IKS_SINGLE;
					prs->atts[prs->attcur] = NULL;
					prs->context = C_TAG_END;
					break;
				}
				if ('>' == c) {
					prs->atts[prs->attcur] = NULL;
					prs->context = C_TAG_END;
					re = 1;
					break;
				}
				prs->context = C_ATTRIBUTE;
				re = 1;
				break;

			case C_VALUE:
				prs->atts[prs->attcur + 1] = STACK_PUSH_START;
				if ('\'' == c) {
					prs->context = C_VALUE_APOS;
					break;
				}
				if ('"' == c) {
					prs->context = C_VALUE_QUOT;
					break;
				}
				return IKS_BADXML;

			case C_VALUE_APOS:
				if ('\'' == c) {
					if (stack_old != -1) STACK_PUSH (buf + stack_old, pos - stack_old);
					stack_old = -1;
					STACK_PUSH_END;
					prs->oldcontext = C_ATTRIBUTE_2;
					prs->context = C_WHITESPACE;
					prs->attcur += 2;
				}
				if (stack_old == -1) stack_old = pos;
				break;

			case C_VALUE_QUOT:
				if ('"' == c) {
					if (stack_old != -1) STACK_PUSH (buf + stack_old, pos - stack_old);
					stack_old = -1;
					STACK_PUSH_END;
					prs->oldcontext = C_ATTRIBUTE_2;
					prs->context = C_WHITESPACE;
					prs->attcur += 2;
				}
				if (stack_old == -1) stack_old = pos;
				break;

			case C_WHITESPACE:
				if (NOT_WHITESPACE(c)) {
					prs->context = prs->oldcontext;
					re = 1;
				}
				break;

			case C_ENTITY:
				if (';' == c) {
					char hede[2];
					char t = '?';
					prs->entity[prs->entpos] = '\0';
					if (strcmp(prs->entity, "amp") == 0)
						t = '&';
					else if (strcmp(prs->entity, "quot") == 0)
						t = '"';
					else if (strcmp(prs->entity, "apos") == 0)
						t = '\'';
					else if (strcmp(prs->entity, "lt") == 0)
						t = '<';
					else if (strcmp(prs->entity, "gt") == 0)
						t = '>';
					old = pos + 1;
					hede[0] = t;
					if (prs->cdataHook) {
						err = prs->cdataHook (prs->user_data, &hede[0], 1);
						if (IKS_OK != err) return err;
					}
					prs->context = C_CDATA;
				} else {
					prs->entity[prs->entpos++] = buf[pos];
					if (prs->entpos > 7) return IKS_BADXML;
				}
				break;

			case C_COMMENT:
				if ('-' != c) return IKS_BADXML;
				prs->context = C_COMMENT_1;
				break;

			case C_COMMENT_1:
				if ('-' == c) prs->context = C_COMMENT_2;
				break;

			case C_COMMENT_2:
				if ('-' == c)
					prs->context = C_COMMENT_3;
				else
					prs->context = C_COMMENT_1;
				break;

			case C_COMMENT_3:
				if ('>' != c) return IKS_BADXML;
				prs->context = C_CDATA;
				old = pos + 1;
				break;

			case C_MARKUP:
				if ('[' == c) {
					prs->context = C_SECT;
					break;
				}
				if ('-' == c) {
					prs->context = C_COMMENT;
					break;
				}
				prs->context = C_MARKUP_1;

			case C_MARKUP_1:
				if ('>' == c) {
					old = pos + 1;
					prs->context = C_CDATA;
				}
				break;

			case C_SECT:
				if ('C' == c) {
					prs->context = C_SECT_CDATA;
					break;
				}
				return IKS_BADXML;

			case C_SECT_CDATA:
				if ('D' != c) return IKS_BADXML;
				prs->context = C_SECT_CDATA_1;
				break;

			case C_SECT_CDATA_1:
				if ('A' != c) return IKS_BADXML;
				prs->context = C_SECT_CDATA_2;
				break;

			case C_SECT_CDATA_2:
				if ('T' != c) return IKS_BADXML;
				prs->context = C_SECT_CDATA_3;
				break;

			case C_SECT_CDATA_3:
				if ('A' != c) return IKS_BADXML;
				prs->context = C_SECT_CDATA_4;
				break;

			case C_SECT_CDATA_4:
				if ('[' != c) return IKS_BADXML;
				old = pos + 1;
				prs->context = C_SECT_CDATA_C;
				break;

			case C_SECT_CDATA_C:
				if (']' == c) {
					prs->context = C_SECT_CDATA_E;
					if (prs->cdataHook && old < pos) {
						err = prs->cdataHook (prs->user_data, &buf[old], pos - old);
						if (IKS_OK != err) return err;
					}
				}
				break;

			case C_SECT_CDATA_E:
				if (']' == c) {
					prs->context = C_SECT_CDATA_E2;
				} else {
					if (prs->cdataHook) {
						err = prs->cdataHook (prs->user_data, "]", 1);
						if (IKS_OK != err) return err;
					}
					old = pos;
					prs->context = C_SECT_CDATA_C;
				}
				break;

			case C_SECT_CDATA_E2:
				if ('>' == c) {
					old = pos + 1;
					prs->context = C_CDATA;
				} else {
					if (prs->cdataHook) {
						err = prs->cdataHook (prs->user_data, "]]", 2);
						if (IKS_OK != err) return err;
					}
					old = pos;
					prs->context = C_SECT_CDATA_C;
				}
				break;

			case C_PI:
				old = pos + 1;
				if ('>' == c) prs->context = C_CDATA;
				break;
		}
cont:
		if (0 == re) {
			pos++;
			prs->nr_bytes++;
			if ('\n' == c) prs->nr_lines++;
		}
	}

	if (stack_old != -1)
		STACK_PUSH (buf + stack_old, pos - stack_old);

	err = IKS_OK;
	if (prs->cdataHook && (prs->context == C_CDATA || prs->context == C_SECT_CDATA_C) && old < pos)
		err = prs->cdataHook (prs->user_data, &buf[old], pos - old);
	return err;
}

int
iks_parse (iksparser *prs, const char *data, size_t len, int finish)
{
	if (!data) return IKS_OK;
	if (len == 0) len = strlen (data);
	return sax_core (prs, (char *) data, len);
}

void
iks_parser_reset (iksparser *prs)
{
	if (prs->deleteHook) prs->deleteHook (prs->user_data);
	prs->stack_pos = 0;
	prs->context = 0;
	prs->oldcontext = 0;
	prs->tagtype = 0;
	prs->attcur = 0;
	prs->attflag = 0;
	prs->valflag = 0;
	prs->entpos = 0;
	prs->nr_bytes = 0;
	prs->nr_lines = 0;
	prs->uni_max = 0;
	prs->uni_len = 0;
}

void
iks_parser_delete (iksparser *prs)
{
	if (prs->deleteHook) prs->deleteHook (prs->user_data);
	if (prs->stack) iks_free (prs->stack);
	if (prs->atts) iks_free (prs->atts);
	if (prs->s) iks_stack_delete (prs->s); else iks_free (prs);
}
/* iksemel (XML parser for Jabber)
** Copyright (C) 2000-2004 Gurer Ozen <madcat@e-kolay.net>
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU Lesser General Public License.
*/

#include "common.h"
#include "iksemel.h"

#define IKS_COMMON \
	struct iks_struct *next, *prev; \
	struct iks_struct *parent; \
	enum ikstype type; \
	ikstack *s

struct iks_struct {
	IKS_COMMON;
};

struct iks_tag {
	IKS_COMMON;
	struct iks_struct *children, *last_child;
	struct iks_struct *attribs, *last_attrib;
	char *name;
};

#define IKS_TAG_NAME(x) ((struct iks_tag *) (x) )->name
#define IKS_TAG_CHILDREN(x) ((struct iks_tag *) (x) )->children
#define IKS_TAG_LAST_CHILD(x) ((struct iks_tag *) (x) )->last_child
#define IKS_TAG_ATTRIBS(x) ((struct iks_tag *) (x) )->attribs
#define IKS_TAG_LAST_ATTRIB(x) ((struct iks_tag *) (x) )->last_attrib

struct iks_cdata {
	IKS_COMMON;
	char *cdata;
	size_t len;
};

#define IKS_CDATA_CDATA(x) ((struct iks_cdata *) (x) )->cdata
#define IKS_CDATA_LEN(x) ((struct iks_cdata *) (x) )->len

struct iks_attrib {
	IKS_COMMON;
	char *name;
	char *value;
};

#define IKS_ATTRIB_NAME(x) ((struct iks_attrib *) (x) )->name
#define IKS_ATTRIB_VALUE(x) ((struct iks_attrib *) (x) )->value

/*****  Node Creating & Deleting  *****/

iks *
iks_new (const char *name)
{
	ikstack *s;
	iks *x;

	s = iks_stack_new (sizeof (struct iks_tag) * 6, 256);
	if (!s) return NULL;
	x = iks_new_within (name, s);
	if (!x) {
		iks_stack_delete (s);
		return NULL;
	}
	return x;
}

iks *
iks_new_within (const char *name, ikstack *s)
{
	iks *x;
	size_t len;

	if (name) len = sizeof (struct iks_tag); else len = sizeof (struct iks_cdata);
	x = iks_stack_alloc (s, len);
	if (!x) return NULL;
	memset (x, 0, len);
	x->s = s;
	x->type = IKS_TAG;
	if (name) {
		IKS_TAG_NAME (x) = iks_stack_strdup (s, name, 0);
		if (!IKS_TAG_NAME (x)) return NULL;
	}
	return x;
}

iks *
iks_insert (iks *x, const char *name)
{
	iks *y;

	if (!x) return NULL;

	y = iks_new_within (name, x->s);
	if (!y) return NULL;
	y->parent = x;
	if (!IKS_TAG_CHILDREN (x)) IKS_TAG_CHILDREN (x) = y;
	if (IKS_TAG_LAST_CHILD (x)) {
		IKS_TAG_LAST_CHILD (x)->next = y;
		y->prev = IKS_TAG_LAST_CHILD (x);
	}
	IKS_TAG_LAST_CHILD (x) = y;
	return y;
}

iks *
iks_insert_cdata (iks *x, const char *data, size_t len)
{
	iks *y;

	if(!x || !data) return NULL;
	if(len == 0) len = strlen (data);

	y = IKS_TAG_LAST_CHILD (x);
	if (y && y->type == IKS_CDATA) {
		IKS_CDATA_CDATA (y) = iks_stack_strcat (x->s, IKS_CDATA_CDATA (y), IKS_CDATA_LEN (y), data, len);
		IKS_CDATA_LEN (y) += len;
	} else {
		y = iks_insert (x, NULL);
		if (!y) return NULL;
		y->type = IKS_CDATA;
		IKS_CDATA_CDATA (y) = iks_stack_strdup (x->s, data, len);
		if (!IKS_CDATA_CDATA (y)) return NULL;
		IKS_CDATA_LEN (y) = len;
	}
	return y;
}

iks *
iks_insert_attrib (iks *x, const char *name, const char *value)
{
	iks *y;
	size_t len;

	if (!x) return NULL;

	y = IKS_TAG_ATTRIBS (x);
	while (y) {
		if (strcmp (name, IKS_ATTRIB_NAME (y)) == 0) break;
		y = y->next;
	}
	if (NULL == y) {
		if (!value) return NULL;
		y = iks_stack_alloc (x->s, sizeof (struct iks_attrib));
		if (!y) return NULL;
		memset (y, 0, sizeof (struct iks_attrib));
		y->type = IKS_ATTRIBUTE;
		IKS_ATTRIB_NAME (y) = iks_stack_strdup (x->s, name, 0);
		y->parent = x;
		if (!IKS_TAG_ATTRIBS (x)) IKS_TAG_ATTRIBS (x) = y;
		if (IKS_TAG_LAST_ATTRIB (x)) {
			IKS_TAG_LAST_ATTRIB (x)->next = y;
			y->prev = IKS_TAG_LAST_ATTRIB (x);
		}
		IKS_TAG_LAST_ATTRIB (x) = y;
	}

	if (value) {
		len = strlen (value);
		IKS_ATTRIB_VALUE (y) = iks_stack_strdup (x->s, value, len);
		if (!IKS_ATTRIB_VALUE (y)) return NULL;
	} else {
		if (y->next) y->next->prev = y->prev;
		if (y->prev) y->prev->next = y->next;
		if (IKS_TAG_ATTRIBS (x) == y) IKS_TAG_ATTRIBS (x) = y->next;
		if (IKS_TAG_LAST_ATTRIB (x) == y) IKS_TAG_LAST_ATTRIB (x) = y->prev;
	}

	return y;
}

iks *
iks_insert_node (iks *x, iks *y)
{
	y->parent = x;
	if (!IKS_TAG_CHILDREN (x)) IKS_TAG_CHILDREN (x) = y;
	if (IKS_TAG_LAST_CHILD (x)) {
		IKS_TAG_LAST_CHILD (x)->next = y;
		y->prev = IKS_TAG_LAST_CHILD (x);
	}
	IKS_TAG_LAST_CHILD (x) = y;
	return y;
}

void
iks_hide (iks *x)
{
	iks *y;

	if (!x) return;

	if (x->prev) x->prev->next = x->next;
	if (x->next) x->next->prev = x->prev;
	y = x->parent;
	if (y) {
		if (IKS_TAG_CHILDREN (y) == x) IKS_TAG_CHILDREN (y) = x->next;
		if (IKS_TAG_LAST_CHILD (y) == x) IKS_TAG_LAST_CHILD (y) = x->prev;
	}
}

void
iks_delete (iks *x)
{
	if (x) iks_stack_delete (x->s);
}

/*****  Node Traversing  *****/

iks *
iks_next (iks *x)
{
	if (x) return x->next;
	return NULL;
}

iks *
iks_next_tag (iks *x)
{
	if (x) {
		while (1) {
			x = x->next;
			if (NULL == x) break;
			if (IKS_TAG == x->type) return x;
		}
	}
	return NULL;
}

iks *
iks_prev (iks *x)
{
	if (x) return x->prev;
	return NULL;
}

iks *
iks_prev_tag (iks *x)
{
	if (x) {
		while (1) {
			x = x->prev;
			if (NULL == x) break;
			if (IKS_TAG == x->type) return x;
		}
	}
	return NULL;
}

iks *
iks_parent (iks *x)
{
	if (x) return x->parent;
	return NULL;
}

iks *
iks_root (iks *x)
{
	if (x) {
		while (x->parent)
			x = x->parent;
	}
	return x;
}

iks *
iks_child (iks *x)
{
	if (x) return IKS_TAG_CHILDREN (x);
	return NULL;
}

iks *
iks_first_tag (iks *x)
{
	if (x) {
		x = IKS_TAG_CHILDREN (x);
		while (x) {
			if (IKS_TAG == x->type) return x;
			x = x->next;
		}
	}
	return NULL;
}

iks *
iks_attrib (iks *x)
{
	if (x) return IKS_TAG_ATTRIBS (x);
	return NULL;
}

iks *
iks_find (iks *x, const char *name)
{
	iks *y;

	if (!x) return NULL;
	y = IKS_TAG_CHILDREN (x);
	while (y) {
		if (IKS_TAG == y->type && IKS_TAG_NAME (y) && strcmp (IKS_TAG_NAME (y), name) == 0) return y;
		y = y->next;
	}
	return NULL;
}

char *
iks_find_cdata (iks *x, const char *name)
{
	iks *y;

	y = iks_find (x, name);
	if (!y) return NULL;
	y = IKS_TAG_CHILDREN (y);
	if (!y || IKS_CDATA != y->type) return NULL;
	return IKS_CDATA_CDATA (y);
}

char *
iks_find_attrib (iks *x, const char *name)
{
	iks *y;

	if (!x) return NULL;

	y = IKS_TAG_ATTRIBS (x);
	while (y) {
		if (IKS_ATTRIB_NAME (y) && strcmp (IKS_ATTRIB_NAME (y), name) == 0)
			return IKS_ATTRIB_VALUE (y);
		y = y->next;
	}
	return NULL;
}

iks *
iks_find_with_attrib (iks *x, const char *tagname, const char *attrname, const char *value)
{
	iks *y;

	if (NULL == x) return NULL;

	if (tagname) {
		for (y = IKS_TAG_CHILDREN (x); y; y = y->next) {
			if (IKS_TAG == y->type
				&& strcmp (IKS_TAG_NAME (y), tagname) == 0
				&& iks_strcmp (iks_find_attrib (y, attrname), value) == 0) {
					return y;
			}
		}
	} else {
		for (y = IKS_TAG_CHILDREN (x); y; y = y->next) {
			if (IKS_TAG == y->type
				&& iks_strcmp (iks_find_attrib (y, attrname), value) == 0) {
					return y;
			}
		}
	}
	return NULL;
}

/*****  Node Information  *****/

ikstack *
iks_stack (iks *x)
{
	if (x) return x->s;
	return NULL;
}

enum ikstype
iks_type (iks *x)
{
	if (x) return x->type;
	return IKS_NONE;
}

char *
iks_name (iks *x)
{
	if (x) {
		if (IKS_TAG == x->type)
			return IKS_TAG_NAME (x);
		else
			return IKS_ATTRIB_NAME (x);
	}
	return NULL;
}

char *
iks_cdata (iks *x)
{
	if (x) {
		if (IKS_CDATA == x->type)
			return IKS_CDATA_CDATA (x);
		else
			return IKS_ATTRIB_VALUE (x);
	}
	return NULL;
}

size_t
iks_cdata_size (iks *x)
{
	if (x) return IKS_CDATA_LEN (x);
	return 0;
}

int
iks_has_children (iks *x)
{
	if (x && IKS_TAG == x->type && IKS_TAG_CHILDREN (x)) return 1;
	return 0;
}

int
iks_has_attribs (iks *x)
{
	if (x && IKS_TAG == x->type && IKS_TAG_ATTRIBS (x)) return 1;
	return 0;
}

/*****  Serializing  *****/

static size_t
escape_size (char *src, size_t len)
{
	size_t sz;
	char c;
	int i;

	sz = 0;
	for (i = 0; i < len; i++) {
		c = src[i];
		switch (c) {
			case '&': sz += 5; break;
			case '\'': sz += 6; break;
			case '"': sz += 6; break;
			case '<': sz += 4; break;
			case '>': sz += 4; break;
			default: sz++; break;
		}
	}
	return sz;
}

static char *
my_strcat (char *dest, char *src, size_t len)
{
	if (0 == len) len = strlen (src);
	memcpy (dest, src, len);
	return dest + len;
}

static char *
escape (char *dest, char *src, size_t len)
{
	char c;
	int i;
	int j = 0;

	for (i = 0; i < len; i++) {
		c = src[i];
		if ('&' == c || '<' == c || '>' == c || '\'' == c || '"' == c) {
			if (i - j > 0) dest = my_strcat (dest, src + j, i - j);
			j = i + 1;
			switch (c) {
			case '&': dest = my_strcat (dest, "&amp;", 5); break;
			case '\'': dest = my_strcat (dest, "&apos;", 6); break;
			case '"': dest = my_strcat (dest, "&quot;", 6); break;
			case '<': dest = my_strcat (dest, "&lt;", 4); break;
			case '>': dest = my_strcat (dest, "&gt;", 4); break;
			}
		}
	}
	if (i - j > 0) dest = my_strcat (dest, src + j, i - j);
	return dest;
}

char *
iks_string (ikstack *s, iks *x)
{
	size_t size;
	int level, dir;
	iks *y, *z;
	char *ret, *t;

	if (!x) return NULL;

	if (x->type == IKS_CDATA) {
		if (s) {
			return iks_stack_strdup (s, IKS_CDATA_CDATA (x), IKS_CDATA_LEN (x));
		} else {
			ret = iks_malloc (IKS_CDATA_LEN (x));
			memcpy (ret, IKS_CDATA_CDATA (x), IKS_CDATA_LEN (x));
			return ret;
		}
	}

	size = 0;
	level = 0;
	dir = 0;
	y = x;
	while (1) {
		if (dir==0) {
			if (y->type == IKS_TAG) {
				size++;
				size += strlen (IKS_TAG_NAME (y));
				for (z = IKS_TAG_ATTRIBS (y); z; z = z->next) {
					size += 4 + strlen (IKS_ATTRIB_NAME (z))
						+ escape_size (IKS_ATTRIB_VALUE (z), strlen (IKS_ATTRIB_VALUE (z)));
				}
				if (IKS_TAG_CHILDREN (y)) {
					size++;
					y = IKS_TAG_CHILDREN (y);
					level++;
					continue;
				} else {
					size += 2;
				}
			} else {
				size += escape_size (IKS_CDATA_CDATA (y), IKS_CDATA_LEN (y));
			}
		}
		z = y->next;
		if (z) {
			if (0 == level) {
				if (IKS_TAG_CHILDREN (y)) size += 3 + strlen (IKS_TAG_NAME (y));
				break;
			}
			y = z;
			dir = 0;
		} else {
			y = y->parent;
			level--;
			if (level >= 0) size += 3 + strlen (IKS_TAG_NAME (y));
			if (level < 1) break;
			dir = 1;
		}
	}

	if (s) ret = iks_stack_alloc (s, size + 1);
	else ret = iks_malloc (size + 1);

	if (!ret) return NULL;

	t = ret;
	level = 0;
	dir = 0;
	while (1) {
		if (dir==0) {
			if (x->type == IKS_TAG) {
				*t++ = '<';
				t = my_strcat (t, IKS_TAG_NAME (x), 0);
				y = IKS_TAG_ATTRIBS (x);
				while (y) {
					*t++ = ' ';
					t = my_strcat (t, IKS_ATTRIB_NAME (y), 0);
					*t++ = '=';
					*t++ = '\'';
					t = escape (t, IKS_ATTRIB_VALUE (y), strlen (IKS_ATTRIB_VALUE (y)));
					*t++ = '\'';
					y = y->next;
				}
				if (IKS_TAG_CHILDREN (x)) {
					*t++ = '>';
					x = IKS_TAG_CHILDREN (x);
					level++;
					continue;
				} else {
					*t++ = '/';
					*t++ = '>';
				}
			} else {
				t = escape (t, IKS_CDATA_CDATA (x), IKS_CDATA_LEN (x));
			}
		}
		y = x->next;
		if (y) {
			if (0 == level) {
				if (IKS_TAG_CHILDREN (x)) {
					*t++ = '<';
					*t++ = '/';
					t = my_strcat (t, IKS_TAG_NAME (x), 0);
					*t++ = '>';
				}
				break;
			}
			x = y;
			dir = 0;
		} else {
			x = x->parent;
			level--;
			if (level >= 0) {
					*t++ = '<';
					*t++ = '/';
					t = my_strcat (t, IKS_TAG_NAME (x), 0);
					*t++ = '>';
				}
			if (level < 1) break;
			dir = 1;
		}
	}
	*t = '\0';

	return ret;
}

/*****  Copying  *****/

iks *
iks_copy_within (iks *x, ikstack *s)
{
	int level=0, dir=0;
	iks *copy = NULL;
	iks *cur = NULL;
	iks *y;

	while (1) {
		if (dir == 0) {
			if (x->type == IKS_TAG) {
				if (copy == NULL) {
					copy = iks_new_within (IKS_TAG_NAME (x), s);
					cur = copy;
				} else {
					cur = iks_insert (cur, IKS_TAG_NAME (x));
				}
				for (y = IKS_TAG_ATTRIBS (x); y; y = y->next) {
					iks_insert_attrib (cur, IKS_ATTRIB_NAME (y), IKS_ATTRIB_VALUE (y));
				}
				if (IKS_TAG_CHILDREN (x)) {
					x = IKS_TAG_CHILDREN (x);
					level++;
					continue;
				} else {
					cur = cur->parent;
				}
			} else {
				iks_insert_cdata (cur, IKS_CDATA_CDATA (x), IKS_CDATA_LEN (x));
			}
		}
		y = x->next;
		if (y) {
			if (0 == level) break;
			x = y;
			dir = 0;
		} else {
			if (level < 2) break;
			level--;
			x = x->parent;
			cur = cur->parent;
			dir = 1;
		}
	}
	return copy;
}

iks *
iks_copy (iks *x)
{
	return iks_copy_within (x, iks_stack_new (sizeof (struct iks_tag) * 6, 256));
}
/* iksemel (XML parser for Jabber)
** Copyright (C) 2000-2003 Gurer Ozen <madcat@e-kolay.net>
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU Lesser General Public License.
*/

#include "common.h"
#include "iksemel.h"

struct dom_data {
	iks **iksptr;
	iks *current;
	size_t chunk_size;
};

static int
tagHook (struct dom_data *data, char *name, char **atts, int type)
{
	iks *x;

	if (IKS_OPEN == type || IKS_SINGLE == type) {
		if (data->current) {
			x = iks_insert (data->current, name);
		} else {
			ikstack *s;
			s = iks_stack_new (data->chunk_size, data->chunk_size);
			x = iks_new_within (name, s);
		}
		if (atts) {
			int i=0;
			while (atts[i]) {
				iks_insert_attrib (x, atts[i], atts[i+1]);
				i += 2;
			}
		}
		data->current = x;
	}
	if (IKS_CLOSE == type || IKS_SINGLE == type) {
		x = iks_parent (data->current);
		if (x)
			data->current = x;
		else {
			*(data->iksptr) = data->current;
			data->current = NULL;
		}
	}
	return IKS_OK;
}

static int
cdataHook (struct dom_data *data, char *cdata, size_t len)
{
	if (data->current) iks_insert_cdata (data->current, cdata, len);
	return IKS_OK;
}

static void
deleteHook (struct dom_data *data)
{
	if (data->current) iks_delete (data->current);
	data->current = NULL;
}

iksparser *
iks_dom_new (iks **iksptr)
{
	ikstack *s;
	struct dom_data *data;

	*iksptr = NULL;
	s = iks_stack_new (DEFAULT_DOM_CHUNK_SIZE, 0);
	if (!s) return NULL;
	data = iks_stack_alloc (s, sizeof (struct dom_data));
	data->iksptr = iksptr;
	data->current = NULL;
	data->chunk_size = DEFAULT_DOM_IKS_CHUNK_SIZE;
	return iks_sax_extend (s, data, (iksTagHook *) tagHook, (iksCDataHook *) cdataHook, (iksDeleteHook *) deleteHook);
}

void
iks_set_size_hint (iksparser *prs, size_t approx_size)
{
	size_t cs;
	struct dom_data *data = iks_user_data (prs);

	cs = approx_size / 10;
	if (cs < DEFAULT_DOM_IKS_CHUNK_SIZE) cs = DEFAULT_DOM_IKS_CHUNK_SIZE;
	data->chunk_size = cs;
}

iks *
iks_tree (const char *xml_str, size_t len, int *err)
{
	iksparser *prs;
	iks *x;
	int e;

	if (0 == len) len = strlen (xml_str);
	prs = iks_dom_new (&x);
	if (!prs) {
		if (err) *err = IKS_NOMEM;
		return NULL;
	}
	e = iks_parse (prs, xml_str, len, 1);
	if (err) *err = e;
	iks_parser_delete (prs);
	return x;
}

int
iks_load (const char *fname, iks **xptr)
{
	iksparser *prs;
	char *buf;
	FILE *f;
	int len, done = 0;
	int ret;

	*xptr = NULL;

	buf = iks_malloc (FILE_IO_BUF_SIZE);
	if (!buf) return IKS_NOMEM;
	ret = IKS_NOMEM;
	prs = iks_dom_new (xptr);
	if (prs) {
		f = fopen (fname, "r");
		if (f) {
			while (0 == done) {
				len = fread (buf, 1, FILE_IO_BUF_SIZE, f);
				if (len < FILE_IO_BUF_SIZE) {
					if (0 == feof (f)) {
						ret = IKS_FILE_RWERR;
						len = 0;
					}
					done = 1;
				}
				if (len > 0) {
					int e;
					e = iks_parse (prs, buf, len, done);
					if (IKS_OK != e) {
						ret = e;
						break;
					}
					if (done) ret = IKS_OK;
				}
			}
			fclose (f);
		} else {
			if (ENOENT == errno) ret = IKS_FILE_NOFILE;
			else ret = IKS_FILE_NOACCESS;
		}
		iks_parser_delete (prs);
	}
	iks_free (buf);
	return ret;
}

int
iks_save (const char *fname, iks *x)
{
	FILE *f;
	char *data;
	int ret;

	ret = IKS_NOMEM;
	data = iks_string (NULL, x);
	if (data) {
		ret = IKS_FILE_NOACCESS;
		f = fopen (fname, "w");
		if (f) {
			ret = IKS_FILE_RWERR;
			if (fputs (data, f) >= 0) ret = IKS_OK;
			fclose (f);
		}
		iks_free (data);
	}
	return ret;
}
