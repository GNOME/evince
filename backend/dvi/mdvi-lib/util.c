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
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#include "common.h"
#include "private.h"

static char *const messages[] = {
	_G("Ooops!"),
	_G("Aieeeee!!"),
	_G("Ouch!"),
	_G("Houston, we have a problem"),
	_G("3.. 2.. 1.. BOOM!"),
	_G("I'm history"),
	_G("I'm going down"),
	_G("I smell a rat")
};
#define NMSGS	(sizeof(messages) / sizeof(char *))

static FILE *logfile = NULL;
static int _mdvi_log_level;

int mdvi_set_logfile(const char *filename);
int mdvi_set_logstream(FILE *file);
int mdvi_set_loglevel(int level);

__attribute__((__format__ (__printf__, 3, 0)))
static void vputlog(int level, const char *head, const char *format, va_list ap)
{
	if(logfile != NULL && _mdvi_log_level >= level) {
		if(head != NULL)
			fprintf(logfile, "%s: ", head);
		vfprintf(logfile, format, ap);
	}
}

int mdvi_set_logfile(const char *filename)
{
	FILE	*f = NULL;

	if(filename && (f = fopen(filename, "w")) == NULL)
		return -1;
	if(logfile != NULL && !isatty(fileno(logfile))) {
		fclose(logfile);
		logfile = NULL;
	}
	if(filename)
		logfile = f;
	return 0;
}

int mdvi_set_logstream(FILE *file)
{
	if(logfile && !isatty(fileno(logfile))) {
		fclose(logfile);
		logfile = NULL;
	}
	logfile = file;
	return 0;
}

int mdvi_set_loglevel(int level)
{
	int	old = _mdvi_log_level;

	_mdvi_log_level = level;
	return old;
}

#ifndef NODEBUG
Uint32 _mdvi_debug_mask = 0;

__attribute__((__format__ (__printf__, 2, 3)))
void	__debug(int mask, const char *format, ...)
{
	va_list	ap;

	va_start(ap, format);
	if(_mdvi_debug_mask & mask) {
		if(!DEBUGGING(SILENT)) {
			fprintf(stderr, "Debug: ");
			vfprintf(stderr, format, ap);
			fflush(stderr);
		}
#ifndef __GNUC__
		/* let's be portable */
		va_end(ap);
		va_start(ap, format);
#endif
		vputlog(LOG_DEBUG, "Debug", format, ap);
	}
	va_end(ap);
}
#endif

__attribute__((__format__ (__printf__, 1, 2)))
void	mdvi_message(const char *format, ...)
{
	va_list	ap;

	va_start(ap, format);
	if(_mdvi_log_level >= LOG_INFO) {
		fprintf(stderr, "%s: ", program_name);
		vfprintf(stderr, format, ap);
#ifndef __GNUC__
		va_end(ap);
		va_start(ap, format);
#endif
	}
	vputlog(LOG_INFO, NULL, format, ap);
	va_end(ap);
}

__attribute__((__format__ (__printf__, 1, 2)))
void	mdvi_crash(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	fprintf(stderr, "%s: %s: ",
		program_name,
		gettext(messages[(int)time(NULL) % NMSGS]));
	vfprintf(stderr, format, ap);
#ifndef __GNUC__
	/* let's be portable */
	va_end(ap);
	va_start(ap, format);
#endif
	vputlog(LOG_ERROR, _("Crashing"), format, ap);
	va_end(ap);
	abort();
}

__attribute__((__format__ (__printf__, 1, 2)))
void	mdvi_error(const char *format, ...)
{
	va_list	ap;

	va_start(ap, format);
	fprintf(stderr, _("%s: Error: "), program_name);
	vfprintf(stderr, format, ap);
#ifndef __GNUC__
	/* let's be portable */
	va_end(ap);
	va_start(ap, format);
#endif
	vputlog(LOG_ERROR, _("Error"), format, ap);
	va_end(ap);
}

__attribute__((__format__ (__printf__, 1, 2)))
void	mdvi_warning(const char *format, ...)
{
	va_list	ap;

	va_start(ap, format);
	fprintf(stderr, _("%s: Warning: "), program_name);
	vfprintf(stderr, format, ap);
#ifndef __GNUC__
	/* let's be portable */
	va_end(ap);
	va_start(ap, format);
#endif
	vputlog(LOG_WARN, _("Warning"), format, ap);
	va_end(ap);
}

__attribute__((__format__ (__printf__, 1, 2)))
void	mdvi_fatal(const char *format, ...)
{
	va_list	ap;

	va_start(ap, format);
	fprintf(stderr, _("%s: Fatal: "), program_name);
	vfprintf(stderr, format, ap);
#ifndef __GNUC__
	/* let's be portable */
	va_end(ap);
	va_start(ap, format);
#endif
	vputlog(LOG_ERROR, _("Fatal"), format, ap);
	va_end(ap);
#ifndef NODEBUG
	abort();
#else
	exit(EXIT_FAILURE);
#endif
}

void	*mdvi_malloc(size_t nelems)
{
	void	*ptr = malloc(nelems);

	if(ptr == NULL)
		mdvi_fatal(_("out of memory allocating %u bytes\n"),
			   (unsigned)nelems);
	return ptr;
}

void	*mdvi_realloc(void *data, size_t newsize)
{
	void	*ptr;

	if(newsize == 0)
		mdvi_crash(_("attempted to reallocate with zero size\n"));
	ptr = realloc(data, newsize);
	if(ptr == NULL)
		mdvi_fatal(_("failed to reallocate %u bytes\n"), (unsigned)newsize);
	return ptr;
}

void	*mdvi_calloc(size_t nmemb, size_t size)
{
	void	*ptr;

	if(nmemb == 0)
		mdvi_crash(_("attempted to callocate 0 members\n"));
	if(size == 0)
		mdvi_crash(_("attempted to callocate %u members with size 0\n"),
			(unsigned)nmemb);
	ptr = calloc(nmemb, size);
	if(ptr == 0)
		mdvi_fatal(_("failed to allocate %ux%u bytes\n"),
			   (unsigned)nmemb, (unsigned)size);
	return ptr;
}

void	mdvi_free(void *ptr)
{
	if(ptr == NULL)
		mdvi_crash(_("attempted to free NULL pointer\n"));
	free(ptr);
}

char	*mdvi_strdup(const char *string)
{
	int	length;
	char	*ptr;

	length = strlen(string) + 1;
	ptr = (char *)mdvi_malloc(length);
	memcpy(ptr, string, length);
	return ptr;
}

/* `to' should have room for length+1 bytes */
char	*mdvi_strncpy(char *to, const char *from, size_t length)
{
	strncpy(to, from, length);
	to[length] = '\0';
	return to;
}

char	*mdvi_strndup(const char *string, size_t length)
{
	int	n;
	char	*ptr;

	n = strlen(string);
	if(n > length)
		n = length;
	ptr = (char *)mdvi_malloc(n + 1);
	memcpy(ptr, string, n);
	return ptr;
}

void	*mdvi_memdup(const void *data, size_t length)
{
	void	*ptr = mdvi_malloc(length);

	memcpy(ptr, data, length);
	return ptr;
}

char   *mdvi_strrstr (const char *haystack, const char *needle)
{
	size_t i;
	size_t needle_len;
	size_t haystack_len;
	const char *p;

	needle_len = strlen (needle);
	haystack_len = strlen (haystack);

	if (needle_len == 0)
		return NULL;

	if (haystack_len < needle_len)
		return (char *)haystack;

	p = haystack + haystack_len - needle_len;
	while (p >= haystack) {
		for (i = 0; i < needle_len; i++)
			if (p[i] != needle[i])
				goto next;

		return (char *)p;

	next:
		p--;
	}

	return NULL;
}

char   *mdvi_build_path_from_cwd (const char *path)
{
	char  *ptr;
	char  *buf = NULL;
	size_t buf_size = 512, len;

	while (1) {
		buf = mdvi_realloc (buf, buf_size);
		if ((ptr = getcwd (buf, buf_size)) == NULL && errno == ERANGE) {
			buf_size *= 2;
		} else {
			buf = ptr;
			break;
		}
	}

	buf = mdvi_realloc (buf, strlen (buf) + strlen (path) + 2);
	len = strlen (path);
	strcat (buf, "/");
	strncat (buf, path, len);

	return buf;
}

double	unit2pix_factor(const char *spec)
{
	double	val;
	double	factor;
	const char *p, *q;
	static const char *units = "incmmmmtptpcddccspbpftydcs";

	val = 0.0;

	for(p = spec; *p >= '0' && *p <= '9'; p++)
		val = 10.0 * val + (double)(*p - '0');
	if(*p == '.') {
		p++;
		factor = 0.1;
		while(*p && *p >= '0' && *p <= '9') {
			val += (*p++ - '0') * factor;
			factor = factor * 0.1;
		}
	}
	factor = 1.0;
	for(q = units; *q; q += 2) {
		if(p[0] == q[0] && p[1] == q[1])
			break;
	}
	switch((int)(q - units)) {
	/*in*/	case 0:  factor = 1.0; break;
	/*cm*/	case 2:  factor = 1.0 / 2.54; break;
	/*mm*/	case 4:  factor = 1.0 / 25.4; break;
	/*mt*/	case 6:  factor = 1.0 / 0.0254; break;
	/*pt*/	case 8:  factor = 1.0 / 72.27; break;
	/*pc*/	case 10: factor = 12.0 / 72.27; break;
	/*dd*/	case 12: factor = (1238.0 / 1157.0) / 72.27; break;
	/*cc*/	case 14: factor = 12 * (1238.0 / 1157.0) / 72.27; break;
	/*sp*/	case 16: factor = 1.0 / (72.27 * 65536); break;
	/*bp*/	case 18: factor = 1.0 / 72.0; break;
	/*ft*/  case 20: factor = 12.0; break;
	/*yd*/  case 22: factor = 36.0; break;
	/*cs*/	case 24: factor = 1.0 / 72000.0; break;
		default: factor = 1.0;
	}
	return factor * val;
}

int	unit2pix(int dpi, const char *spec)
{
	double	factor = unit2pix_factor(spec);

	return (int)(factor * dpi + 0.5);
}

Ulong	get_mtime(int fd)
{
	struct stat st;

	if(fstat(fd, &st) == 0)
		return (Ulong)st.st_mtime;
	return 0;
}

char	*xstradd(char *dest, size_t *size, size_t n, const char *src, size_t m)
{
	if(m == 0)
		m = strlen(src);
	if(n + m >= *size) {
		dest = mdvi_realloc(dest, n + m + 1);
		*size = n + m + 1;
	}
	memcpy(dest + n, src, m);
	dest[n + m] = 0;
	return dest;
}

char	*getword(char *string, const char *delim, char **end)
{
	char *ptr;
	char *word;

	/* skip leading delimiters */
	for(ptr = string; *ptr && strchr(delim, *ptr); ptr++);

	if(*ptr == 0)
		return NULL;
	word = ptr++;
	/* skip non-delimiters */
	while(*ptr && !strchr(delim, *ptr))
		ptr++;
	*end = (char *)ptr;
	return word;
}

char	*getstring(char *string, const char *delim, char **end)
{
	char *ptr;
	char *word;
	int	quoted = 0;

	/* skip leading delimiters */
	for(ptr = string; *ptr && strchr(delim, *ptr); ptr++);

	if(ptr == NULL)
		return NULL;
	quoted = (*ptr == '"');
	if(quoted)
		for(word = ++ptr; *ptr && *ptr != '"'; ptr++);
	else
		for(word = ptr; *ptr && !strchr(delim, *ptr); ptr++);
	*end = (char *)ptr;
	return word;
}

static long pow2(size_t n)
{
	long	x = 8; /* don't bother allocating less than this */

	while(x < n)
		x <<= 1L;
	return x;
}

void	dstring_init(Dstring *dstr)
{
	dstr->data = NULL;
	dstr->size = 0;
	dstr->length = 0;
}

int	dstring_append(Dstring *dstr, const char *string, int len)
{
	if(len < 0)
		len = strlen(string);
	if(len) {
		if(dstr->length + len >= dstr->size) {
			dstr->size = pow2(dstr->length + len + 1);
			dstr->data = mdvi_realloc(dstr->data, dstr->size);
		}
		memcpy(dstr->data + dstr->length, string, len);
		dstr->length += len;
		dstr->data[dstr->length] = 0;
	} else if(dstr->size == 0) {
		ASSERT(dstr->data == NULL);
		dstr->size = 8;
		dstr->data = mdvi_malloc(8);
		dstr->data[0] = 0;
	}

	return dstr->length;
}

int	dstring_copy(Dstring *dstr, int pos, const char *string, int len)
{
	ASSERT(pos >= 0);
	if(len < 0)
		len = strlen(string);
	if(len) {
		if(pos + len >= dstr->length) {
			dstr->length = pos;
			return dstring_append(dstr, string, len);
		}
		memcpy(dstr->data + pos, string, len);
	}
	return dstr->length;
}

int	dstring_insert(Dstring *dstr, int pos, const char *string, int len)
{
	ASSERT(pos >= 0);
	if(pos == dstr->length)
		return dstring_append(dstr, string, len);
	if(len < 0)
		len = strlen(string);
	if(len) {
		if(dstr->length + len >= dstr->size) {
			dstr->size = pow2(dstr->length + len + 1);
			dstr->data = mdvi_realloc(dstr->data, dstr->size);
		}
		/* make room */
		memmove(dstr->data + pos, dstr->data + pos + len, len);
		/* now copy */
		memcpy(dstr->data + pos, string, len);
		dstr->length += len;
		dstr->data[dstr->length] = 0;
	}
	return dstr->length;
}

int	dstring_new(Dstring *dstr, const char *string, int len)
{
	if(len < 0)
		len = strlen(string);
	if(len) {
		dstr->size = pow2(len + 1);
		dstr->data = mdvi_malloc(dstr->size * len);
		memcpy(dstr->data, string, len);
	} else
		dstring_init(dstr);
	return dstr->length;
}

void	dstring_reset(Dstring *dstr)
{
	if(dstr->data)
		mdvi_free(dstr->data);
	dstring_init(dstr);
}
