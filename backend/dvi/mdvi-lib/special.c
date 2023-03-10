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
#include <ctype.h>
#include <string.h>

#include "mdvi.h"
#include "private.h"

#if defined(WITH_REGEX_SPECIALS) && defined(HAVE_REGEX_H)
#include <regex.h>
#endif

typedef struct _DviSpecial {
	struct _DviSpecial *next;
	struct _DviSpecial *prev;
	char	*label;
	char	*prefix;
	size_t	plen;
#ifdef WITH_REGEX_SPECIALS
	regex_t	reg;
	int	has_reg;
#endif
	DviSpecialHandler handler;
} DviSpecial;

static ListHead specials = {NULL, NULL, 0};

#define SPECIAL(x)	\
	void x __PROTO((DviContext *, const char *, const char *))

static SPECIAL(sp_layer);
extern SPECIAL(epsf_special);
extern SPECIAL(do_color_special);

static struct {
	char	*label;
	char	*prefix;
	char	*regex;
	DviSpecialHandler handler;
} builtins[] = {
	{"Layers", "layer", NULL, sp_layer},
	{"EPSF", "psfile", NULL, epsf_special}
};
#define NSPECIALS	(sizeof(builtins) / sizeof(builtins[0]))
static int registered_builtins = 0;

static void register_builtin_specials(void)
{
	int	i;

	ASSERT(registered_builtins == 0);
	registered_builtins = 1;
	for(i = 0; i < NSPECIALS; i++)
		mdvi_register_special(
			builtins[i].label,
			builtins[i].prefix,
			builtins[i].regex,
			builtins[i].handler,
			1 /* replace if exists */);
}

static DviSpecial *find_special_prefix(const char *prefix)
{
	DviSpecial *sp;

	/* should have a hash table here, but I'm so lazy */
	for(sp = (DviSpecial *)specials.head; sp; sp = sp->next) {
		if(STRCEQ(sp->prefix, prefix))
			break;
	}
	return sp;
}

int	mdvi_register_special(const char *label, const char *prefix,
	const char *regex, DviSpecialHandler handler, int replace)
{
	DviSpecial *sp;
	int	newsp = 0;

	if(!registered_builtins)
		register_builtin_specials();

	sp = find_special_prefix(prefix);
	if(sp == NULL) {
		sp = xalloc(DviSpecial);
		sp->prefix = mdvi_strdup(prefix);
		newsp = 1;
	} else if(!replace)
		return -1;
	else {
		mdvi_free(sp->label);
		sp->label = NULL;
	}

#ifdef WITH_REGEX_SPECIALS
	if(!newsp && sp->has_reg) {
		regfree(&sp->reg);
		sp->has_reg = 0;
	}
	if(regex && regcomp(&sp->reg, regex, REG_NOSUB) != 0) {
		if(newsp) {
			mdvi_free(sp->prefix);
			mdvi_free(sp);
		}
		return -1;
	}
	sp->has_reg = (regex != NULL);
#endif
	sp->handler = handler;
	sp->label = mdvi_strdup(label);
	sp->plen = strlen(prefix);
	if(newsp)
		listh_prepend(&specials, LIST(sp));
	DEBUG((DBG_SPECIAL,
		"New \\special handler `%s' with prefix `%s'\n",
		label, prefix));
	return 0;
}

int	mdvi_unregister_special(const char *prefix)
{
	DviSpecial *sp;

	sp = find_special_prefix(prefix);
	if(sp == NULL)
		return -1;
	mdvi_free(sp->prefix);
#ifdef WITH_REGEX_SPECIALS
	if(sp->has_reg)
		regfree(&sp->reg);
#endif
	listh_remove(&specials, LIST(sp));
	mdvi_free(sp);
	return 0;
}

#define IS_PREFIX_DELIMITER(x)	(strchr(" \t\n:=", (x)) != NULL)

int	mdvi_do_special(DviContext *dvi, char *string)
{
	char	*prefix;
	char 	*ptr;
	DviSpecial *sp;

	if(!registered_builtins) {
	}

	if(!string || !*string)
		return 0;

	/* skip leading spaces */
	while(*string && isspace(*string))
		string++;

	DEBUG((DBG_SPECIAL, "Looking for a handler for `%s'\n", string));

	/* now try to find a match */
	ptr = string;
	for(sp = (DviSpecial *)specials.head; sp; sp = sp->next) {
#ifdef WITH_REGEX_SPECIALS
		if(sp->has_reg && !regexec(&sp->reg, ptr, 0, 0, 0))
			break;
#endif
		/* check the prefix */
		if(STRNCEQ(sp->prefix, ptr, sp->plen)) {
			ptr += sp->plen;
			break;
		}
	}

	if(sp == NULL) {
		DEBUG((DBG_SPECIAL, "None found\n"));
		return -1;
	}

	/* extract the prefix */
	if(ptr == string) {
		prefix = NULL;
		DEBUG((DBG_SPECIAL,
			"REGEX match with `%s' (arg `%s')\n",
			sp->label, ptr));
	} else {
		if(*ptr) *ptr++ = 0;
		prefix = string;
		DEBUG((DBG_SPECIAL,
			"PREFIX match with `%s' (prefix `%s', arg `%s')\n",
			sp->label, prefix, ptr));
	}

	/* invoke the handler */
	sp->handler(dvi, prefix, ptr);

	return 0;
}

void	mdvi_flush_specials(void)
{
	DviSpecial *sp, *list;


	for(list = (DviSpecial *)specials.head; (sp = list); ) {
		list = sp->next;
		if(sp->prefix) mdvi_free(sp->prefix);
		if(sp->label) mdvi_free(sp->label);
#ifdef WITH_REGEX_SPECIALS
		if(sp->has_reg)
			regfree(&sp->reg);
#endif
		mdvi_free(sp);
	}
	specials.head = NULL;
	specials.tail = NULL;
	specials.count = 0;
}

/* some builtin specials */

void	sp_layer(DviContext *dvi, const char *prefix, const char *arg)
{
	if(STREQ("push", arg))
		dvi->curr_layer++;
	else if(STREQ("pop", arg)) {
		if(dvi->curr_layer)
			dvi->curr_layer--;
		else
			mdvi_warning(_("%s: tried to pop top level layer\n"),
				     dvi->filename);
	} else if(STREQ("reset", arg))
		dvi->curr_layer = 0;
	DEBUG((DBG_SPECIAL, "Layer level: %d\n", dvi->curr_layer));
}

