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
#include <stdlib.h>
#include <string.h>

#include "common.h"

long	fsgetn(FILE *p, size_t n)
{
	long	v;

	v = fgetbyte(p);
	if(v & 0x80)
		v -= 0x100;
	while(--n > 0)
		v = (v << 8) | fgetbyte(p);
	return v;
}

Ulong	fugetn(FILE *p, size_t n)
{
	Ulong	v;

	v = fgetbyte(p);
	while(--n > 0)
		v = (v << 8) | fgetbyte(p);
	return v;
}

long	msgetn(const Uchar *p, size_t n)
{
	long	v = (long)*p++;

	if(v & 0x80)
		v -= 0x100;
	while(--n > 0)
		v = (v << 8) | *p++;
	return v;
}

Ulong	mugetn(const Uchar *p, size_t n)
{
	Ulong	v = (Ulong)*p++;

	while(--n > 0)
		v = (v << 8) | *p++;
	return v;
}

char	*read_string(FILE *in, int s, char *buffer, size_t len)
{
	int	n;
	char	*str;

	n = fugetn(in, s ? s : 1);
	if((str = buffer) == NULL || n + 1 > len)
		str = mdvi_malloc(n + 1);
	if(fread(str, 1, n, in) != n) {
		if(str != buffer) mdvi_free(str);
		return NULL;
	}
	str[n] = 0;
	return str;
}

size_t	read_bcpl(FILE *in, char *buffer, size_t maxlen, size_t wanted)
{
	size_t	i;

	i = (int)fuget1(in);
	if(maxlen && i > maxlen)
		i = maxlen;
	if(fread(buffer, i, 1, in) != 1)
		return -1;
	buffer[i] = '\0';
	while(wanted-- > i)
		(void)fgetc(in);
	return i;
}

char	*read_alloc_bcpl(FILE *in, size_t maxlen, size_t *size)
{
	size_t	i;
	char	*buffer;

	i = (size_t)fuget1(in);
	if(maxlen && i > maxlen)
		i = maxlen;
	buffer = (char *)malloc(i + 1);
	if(buffer == NULL)
		return NULL;
	if(fread(buffer, i, 1, in) != 1) {
		free(buffer);
		return NULL;
	}
	buffer[i] = '\0';
	if(size) *size = i;
	return buffer;
}

/* buffers */

void	buff_free(Buffer *buf)
{
	if(buf->data)
		mdvi_free(buf->data);
	buff_init(buf);
}

void	buff_init(Buffer *buf)
{
	buf->data = NULL;
	buf->size = 0;
	buf->length = 0;
}

size_t	buff_add(Buffer *buf, const char *data, size_t len)
{
	if(!len && data)
		len = strlen(data);
	if(buf->length + len + 1 > buf->size) {
		buf->size = buf->length + len + 256;
		buf->data = mdvi_realloc(buf->data, buf->size);
	}
	memcpy(buf->data + buf->length, data, len);
	buf->length += len;
	return buf->length;
}

char	*buff_gets(Buffer *buf, size_t *length)
{
	char	*ptr;
	char	*ret;
	size_t	len;

	ptr = strchr(buf->data, '\n');
	if(ptr == NULL)
		return NULL;
	ptr++; /* include newline */
	len = ptr - buf->data;
	ret = mdvi_malloc(len + 1);
	if(len > 0) {
		memcpy(ret, buf->data, len);
		memmove(buf->data, buf->data + len, buf->length - len);
		buf->length -= len;
	}
	ret[len] = 0;
	if(length) *length = len;
	return ret;
}

