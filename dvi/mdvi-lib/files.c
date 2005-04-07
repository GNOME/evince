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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "common.h"

char	*dgets(Dstring *dstr, FILE *in)
{
	char	buffer[256];
	
	dstr->length = 0;
	if(feof(in))
		return NULL;
	while(fgets(buffer, 256, in) != NULL) {
		int	len = strlen(buffer);
		
 		if(buffer[len-1] == '\n') {
			dstring_append(dstr, buffer, len - 1);
			break;
		}
		dstring_append(dstr, buffer, len);
	}
	if(dstr->data)
		dstr->data[dstr->length] = 0;
	return dstr->data;
}

/* some simple helper functions to manipulate file names */

const char *file_basename(const char *filename)
{
	const char *ptr = strrchr(filename, '/');
	
	return (ptr ? ptr + 1 : filename);
}

const char *file_extension(const char *filename)
{
	const char *ptr = strchr(file_basename(filename), '.');
	
	return (ptr ? ptr + 1 : NULL);
}

int	file_readable(const char *filename)
{
	int	status = (access(filename, R_OK) == 0);
	
	DEBUG((DBG_FILES, "file_redable(%s) -> %s\n",
		filename, status ? "Yes" : "No"));
	return status;
}

int	file_exists(const char *filename)
{
	int	status = (access(filename, F_OK) == 0);
	
	DEBUG((DBG_FILES, "file_exists(%s) -> %s\n",
		filename, status ? "Yes" : "No"));
	return status;
}

static char *xstrchre(const char *string, int c)
{
	const char *ptr;
	
	for(ptr = string; *ptr && *ptr != c; ptr++);
	return (char *)ptr;
}

char	*find_in_path(const char *filename, const char *path)
{
	const char *p, *q;
	char	*try;
	int	len;
	
	/* if the file is readable as given, return it */
	if(file_readable(filename))
		return xstrdup(filename);
	
	/* worst case scenario */
	try = xmalloc(strlen(path) + strlen(filename) + 2);
	try[0] = 0;

	for(p = path; *p; p = q) {
		q = xstrchre(p, ':');
		len = q - p;
		xstrncpy(try, p, len);
		try[len] = '/';
		strcpy(try + len + 1, filename);
		if(file_readable(try))
			break;
		if(*q) q++;
	}

	if(*p)
		return try;
	else {
		xfree(try);
		return NULL;
	}
}

char	*read_into_core(const char *file, size_t *size)
{
	FILE	*in;
	struct stat st;
	char	*ptr;
	
	in = fopen(file, "r");
	if(in == NULL)
		return NULL;
	if(fstat(fileno(in), &st) < 0) {
		/* I don't think this is possible, but who knows */
		fclose(in);
		return NULL;
	}
	if(st.st_size == 0) {
		warning("%s: file is empty\n", file);
		fclose(in);
		return NULL;
	}
	ptr = xmalloc(st.st_size + 1);
	if(fread(ptr, st.st_size, 1, in) != 1) {
		fclose(in);
		xfree(ptr);
		return NULL;
	}
	fclose(in);
	ptr[st.st_size] = 0;
	if(size) *size = st.st_size;
	return ptr;
}
