/* pagesel.c -- Page selection mechanism */
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
#include <ctype.h>
#include <string.h>
#include <limits.h>

#include "mdvi.h"
#include "private.h"

char	*program_name = "page";

struct _DviPageSpec {
	DviRange *ranges;
	int	nranges;
};

DviRange *mdvi_parse_range(const char *format, DviRange *limit, int *nitems, char **endptr)
{
	int	quoted;
	int	size;
	int	curr;
	int	done;
	int	lower;
	int	upper;
	int	type;
	char *	cp;
	char *	copy;
	char *	text;
	DviRange one;
	DviRange *range;

	quoted = (*format == '{');
	if(quoted) format++;

	size  = 0;
	curr  = 0;
	range = NULL;
	copy  = mdvi_strdup(format);
	done  = 0;
	lower = 0;
	upper = 0;
	type  = MDVI_RANGE_UNBOUNDED;

	if(limit) {
		switch(limit->type) {
			case MDVI_RANGE_BOUNDED:
				lower = limit->from;
				upper = limit->to;
				break;
			case MDVI_RANGE_UPPER:
				lower = INT_MIN;
				upper = limit->to;
				break;
			case MDVI_RANGE_LOWER:
				lower = limit->from;
				upper = INT_MAX;
				break;
			case MDVI_RANGE_UNBOUNDED:
				lower = INT_MIN;
				upper = INT_MAX;
				break;
		}
		type = limit->type;
	} else {
		lower = INT_MIN;
		upper = INT_MAX;
		type  = MDVI_RANGE_UNBOUNDED;
	}
	one.type = type;
	one.from = lower;
	one.to   = upper;
	one.step = 1;
	for(cp = text = copy; !done; cp++) {
		char	*p;
		int	f, t, s;
		int	ch;
		int	this_type;
		int	lower_given = 0;
		int	upper_given = 0;

		if(*cp == 0 || *cp == '.' || (*cp == '}' && quoted))
			done = 1;
		else if(*cp != ',')
			continue;

		if(text == cp)
			continue;
		ch = *cp;
		*cp = 0;
		f = lower;
		t = upper;
		s = 1;

		p = strchr(text, ':');
		if(p) *p++ = 0;
		if(*text) {
			lower_given = 1;
			f = strtol(text, NULL, 0);
		}
		if(p == NULL) {
			if(lower_given) {
				upper_given = 1;
				t = f; s = 1;
			}
			goto finish;
		}
		text = p;
		p = strchr(text, ':');
		if(p) *p++ = 0;
		if(*text) {
			upper_given = 1;
			t = strtol(text, NULL, 0);
		}
		if(p == NULL)
			goto finish;
		text = p;
		if(*text)
			s = strtol(text, NULL, 0);
finish:
		if(lower_given && upper_given)
			this_type = MDVI_RANGE_BOUNDED;
		else if(lower_given) {
			if(!RANGE_HAS_UPPER(type))
				this_type = MDVI_RANGE_LOWER;
			else
				this_type = MDVI_RANGE_BOUNDED;
			t = upper;
		} else if(upper_given) {
			if(RANGE_HAS_UPPER(one.type)) {
				one.to++;
				this_type = MDVI_RANGE_BOUNDED;
			} else {
				one.to = lower;
				if(!RANGE_HAS_LOWER(type))
					this_type = MDVI_RANGE_UPPER;
				else
					this_type = MDVI_RANGE_BOUNDED;
			}
			f = one.to;
		} else {
			this_type = type;
			f = lower;
			t = upper;
		}
		one.type = this_type;
		one.to   = t;
		one.from = f;
		one.step = s;

		if(curr == size) {
			size += 8;
			range = mdvi_realloc(range, size * sizeof(DviRange));
		}
		memcpy(&range[curr++], &one, sizeof(DviRange));
		*cp = ch;
		text = cp + 1;
	}
	if(done)
		cp--;
	if(quoted && *cp == '}')
		cp++;
	if(endptr)
		*endptr = (char *)format + (cp - copy);
	if(curr && curr < size)
		range = mdvi_realloc(range, curr * sizeof(DviRange));
	*nitems = curr;
	mdvi_free(copy);
	return range;
}

DviPageSpec *mdvi_parse_page_spec(const char *format)
{
	/*
	 * a page specification looks like this:
	 *   '{'RANGE_SPEC'}'         for a DVI spec
	 *   '{'RANGE_SPEC'}' '.' ... for a TeX spec
	 */
	DviPageSpec *spec;
	DviRange *range;
	int	count;
	int	i;
	char	*ptr = NULL;

	spec = xnalloc(struct _DviPageSpec *, 11);
	for(i = 0; i < 11; i++)
		spec[i] = NULL;

	/* check what kind of spec we're parsing */
	if(*format != '*') {
		range = mdvi_parse_range(format, NULL, &count, &ptr);
		if(ptr == format) {
			if(range) mdvi_free(range);
			mdvi_error(_("invalid page specification `%s'\n"), format);
			return NULL;
		}
	} else
		range = NULL;

	if(*format == 'D' || *format == 'd' || *ptr != '.')
		i = 0;
	else
		i = 1;

	if(range) {
		spec[i] = xalloc(struct _DviPageSpec);
		spec[i]->ranges = range;
		spec[i]->nranges = count;
	} else
		spec[i] = NULL;

	if(*ptr != '.') {
		if(*ptr)
			mdvi_warning(_("garbage after DVI page specification ignored\n"));
		return spec;
	}

	for(i++; *ptr == '.' && i <= 10; i++) {
		ptr++;
		if(*ptr == '*') {
			ptr++;
			range = NULL;
		} else {
			char	*end;

			range = mdvi_parse_range(ptr, NULL, &count, &end);
			if(end == ptr) {
				if(range) mdvi_free(range);
				range = NULL;
			} else
				ptr = end;
		}
		if(range != NULL) {
			spec[i] = xalloc(struct _DviPageSpec);
			spec[i]->ranges = range;
			spec[i]->nranges = count;
		} else
			spec[i] = NULL;
	}

	if(i > 10)
		mdvi_warning(_("more than 10 counters in page specification\n"));
	else if(*ptr)
		mdvi_warning(_("garbage after TeX page specification ignored\n"));

	return spec;
}

/* returns non-zero if the given page is included by `spec' */
int	mdvi_page_selected(DviPageSpec *spec, PageNum page, int dvipage)
{
	int	i;
	int	not_found;

	if(spec == NULL)
		return 1;
	if(spec[0]) {
		not_found = mdvi_in_range(spec[0]->ranges,
			spec[0]->nranges, dvipage);
		if(not_found < 0)
			return 0;
	}
	for(i = 1; i <= 10; i++) {
		if(spec[i] == NULL)
			continue;
		not_found = mdvi_in_range(spec[i]->ranges,
			spec[i]->nranges, (int)page[i]);
		if(not_found < 0)
			return 0;
	}
	return 1;
}

void	mdvi_free_page_spec(DviPageSpec *spec)
{
	int	i;

	for(i = 0; i < 11; i++)
		if(spec[i]) {
			mdvi_free(spec[i]->ranges);
			mdvi_free(spec[i]);
		}
	mdvi_free(spec);
}

int	mdvi_in_range(DviRange *range, int nitems, int value)
{
	DviRange *r;

	for(r = range; r < range + nitems; r++) {
		int	cond;

		switch(r->type) {
		case MDVI_RANGE_BOUNDED:
			if(value == r->from)
				return (r - range);
			if(r->step < 0)
				cond = (value <= r->from) && (value >= r->to);
			else
				cond = (value <= r->to) && (value >= r->from);
			if(cond && ((value - r->from) % r->step) == 0)
				return (r - range);
			break;
		case MDVI_RANGE_LOWER:
			if(value == r->from)
				return (r - range);
			if(r->step < 0)
				cond = (value < r->from);
			else
				cond = (value > r->from);
			if(cond && ((value - r->from) % r->step) == 0)
				return (r - range);
			break;
		case MDVI_RANGE_UPPER:
			if(value == r->to)
				return (r - range);
			if(r->step < 0)
				cond = (value > r->to);
			else
				cond = (value < r->to);
			if(cond && ((value - r->to) % r->step) == 0)
				return (r - range);
			break;
		case MDVI_RANGE_UNBOUNDED:
			if((value % r->step) == 0)
				return (r - range);
			break;
		}
	}
	return -1;
}

int	mdvi_range_length(DviRange *range, int nitems)
{
	int	count = 0;
	DviRange *r;

	for(r = range; r < range + nitems; r++) {
		int	n;

		if(r->type != MDVI_RANGE_BOUNDED)
			return -2;
		n = (r->to - r->from) / r->step;
		if(n < 0)
			n = 0;
		count += n + 1;
	}
	return count;
}

#ifdef TEST

void	print_range(DviRange *range)
{
	switch(range->type) {
	case MDVI_RANGE_BOUNDED:
		printf("From %d to %d, step %d\n",
			range->from, range->to, range->step);
		break;
	case MDVI_RANGE_LOWER:
		printf("From %d, step %d\n",
			range->from, range->step);
		break;
	case MDVI_RANGE_UPPER:
		printf("From %d, step -%d\n",
			range->to, range->step);
		break;
	case MDVI_RANGE_UNBOUNDED:
		printf("From 0, step %d and %d\n",
			range->step, -range->step);
		break;
	}
}

int main()
{
#if 0
	char	buf[256];
	DviRange limit;

	limit.from = 0;
	limit.to = 100;
	limit.step = 2;
	limit.type = MDVI_RANGE_UNBOUNDED;
	while(1) {
		DviRange *range;
		char	*end;
		int	count;
		int	i;

		printf("Range> "); fflush(stdout);
		if(fgets(buf, 256, stdin) == NULL)
			break;
		if(buf[strlen(buf)-1] == '\n')
			buf[strlen(buf)-1] = 0;
		if(buf[0] == 0)
			continue;
		end = NULL;
		range = mdvi_parse_range(buf, &limit, &count, &end);
		if(range == NULL) {
			printf("range is empty\n");
			continue;
		}

		for(i = 0; i < count; i++) {
			printf("Range %d (%d elements):\n",
				i, mdvi_range_length(&range[i], 1));
			print_range(&range[i]);
		}
		if(end && *end)
			printf("Tail: [%s]\n", end);
		printf("range has %d elements\n",
			mdvi_range_length(range, count));
#if 1
		while(1) {
			int	v;

			printf("Value: "); fflush(stdout);
			if(fgets(buf, 256, stdin) == NULL)
				break;
			if(buf[strlen(buf)-1] == '\n')
				buf[strlen(buf)-1] = 0;
			if(buf[0] == 0)
				break;
			v = atoi(buf);
			i = mdvi_in_range(range, count, v);
			if(i == -1)
				printf("%d not in range\n", v);
			else {
				printf("%d in range: ", v);
				print_range(&range[i]);
			}
		}
#endif
		if(range) mdvi_free(range);
	}
#else
	DviPageSpec *spec;
	char	buf[256];

	while(1) {
		printf("Spec> "); fflush(stdout);
		if(fgets(buf, 256, stdin) == NULL)
			break;
		if(buf[strlen(buf)-1] == '\n')
			buf[strlen(buf)-1] = 0;
		if(buf[0] == 0)
			continue;
		spec = mdvi_parse_page_spec(buf);
		if(spec == NULL)
			printf("no spec parsed\n");
		else {
			int	i;

			printf("spec = ");
			for(i = 0; i < 11; i++) {
				printf("Counter %d:\n", i);
				if(spec[i]) {
					int	k;

					for(k = 0; k < spec[i]->nranges; k++)
						print_range(&spec[i]->ranges[k]);
				} else
					printf("\t*\n");
			}
			mdvi_free_page_spec(spec);
		}
	}
#endif
	exit(0);

}
#endif /* TEST */
