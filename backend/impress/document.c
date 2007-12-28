/* imposter (OO.org Impress viewer)
** Copyright (C) 2003-2005 Gurer Ozen
** This code is free software; you can redistribute it and/or
** modify it under the terms of GNU General Public License.
*/

#include <config.h>
#include "common.h"
#include "internal.h"

static iks *
_imp_load_xml(ImpDoc *doc, const char *xmlfile)
{
	int e;
	iks *x;

	x = zip_load_xml (doc->zfile, xmlfile, &e);
	return x;
}

ImpDoc *
imp_open(const char *filename, int *err)
{
	ImpDoc *doc;
	int e;

	doc = calloc(1, sizeof(ImpDoc));
	if (!doc) {
		*err = IMP_NOMEM;
		return NULL;
	}

	doc->stack = iks_stack_new(sizeof(ImpPage) * 32, 0);
	if (!doc->stack) {
		*err = IMP_NOMEM;
		imp_close(doc);
		return NULL;
	}

	doc->zfile = zip_open(filename, &e);
	if (e) {
		*err = IMP_NOTZIP;
		imp_close(doc);
		return NULL;
	}

	doc->content = _imp_load_xml(doc, "content.xml");
	doc->styles = _imp_load_xml(doc, "styles.xml");
	doc->meta = _imp_load_xml(doc, "meta.xml");

	if (!doc->content || !doc->styles) {
		*err = IMP_BADDOC;
		imp_close(doc);
		return NULL;
	}

	e = _imp_oo13_load(doc);
	if (e && e != IMP_NOTIMP) {
		*err = e;
		imp_close(doc);
		return NULL;
	}

	if (e == IMP_NOTIMP) {
		e = _imp_oasis_load(doc);
		if (e) {
			*err = e;
			imp_close(doc);
			return NULL;
		}
	}

	return doc;
}

int
imp_nr_pages(ImpDoc *doc)
{
	return doc->nr_pages;
}

ImpPage *
imp_get_page(ImpDoc *doc, int page_no)
{
	if (page_no == IMP_LAST_PAGE) {
		return doc->last_page;
	} else {
		ImpPage *page;
		if (page_no < 0 || page_no > doc->nr_pages) return NULL;
		for (page = doc->pages; page_no; --page_no) {
			page = page->next;
		}
		return page;
	}
}

ImpPage *
imp_next_page(ImpPage *page)
{
	return page->next;
}

ImpPage *
imp_prev_page(ImpPage *page)
{
	return page->prev;
}

int
imp_get_page_no(ImpPage *page)
{
	return page->nr;
}

const char *
imp_get_page_name(ImpPage *page)
{
	return page->name;
}

void *
imp_get_xml(ImpDoc *doc, const char *filename)
{
	if (strcmp(filename, "content.xml") == 0)
		return doc->content;
	else if (strcmp(filename, "styles.xml") == 0)
		return doc->styles;
	else if (strcmp(filename, "meta.xml") == 0)
		return doc->meta;
	else
		return NULL;
}

void
imp_close(ImpDoc *doc)
{
	if (doc->stack) iks_stack_delete(doc->stack);
	if (doc->zfile) zip_close(doc->zfile);
	free(doc);
}
