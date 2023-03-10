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

#include "common.h"
#include "mdvi.h"
#include "private.h"

static const DviPaperSpec papers[] = {
	{"ISO", 0, 0},
	{"4A0", "1682mm", "2378mm"},
	{"2A0", "1189mm", "1682mm"},
	{"A0", "841mm", "1189mm"},
	{"A1", "594mm", "841mm"},
	{"A2", "420mm", "594mm"},
	{"A3", "297mm", "420mm"},
	{"A4", "210mm", "297mm"},
	{"A5", "148mm", "210mm"},
	{"A6", "105mm", "148mm"},
	{"A7", "74mm", "105mm"},
	{"A8", "52mm", "74mm"},
	{"A9", "37mm", "52mm"},
	{"A10", "26mm", "37mm"},
	{"B0", "1000mm", "1414mm"},
	{"B1", "707mm", "1000mm"},
	{"B2", "500mm", "707mm"},
	{"B3", "353mm", "500mm"},
	{"B4", "250mm", "353mm"},
	{"B5", "176mm", "250mm"},
	{"B6", "125mm", "176mm"},
	{"B7", "88mm", "125mm"},
	{"B8", "62mm", "88mm"},
	{"B9", "44mm", "62mm"},
	{"B10", "31mm", "44mm"},
	{"C0", "917mm", "1297mm"},
	{"C1", "648mm", "917mm"},
	{"C2", "458mm", "648mm"},
	{"C3", "324mm", "458mm"},
	{"C4", "229mm", "324mm"},
	{"C5", "162mm", "229mm"},
	{"C6", "114mm", "162mm"},
	{"C7", "81mm", "114mm"},
	{"C8", "57mm", "81mm"},
	{"C9", "40mm", "57mm"},
	{"C10", "28mm", "40mm"},
	{"US", 0, 0},
	{"archA", "9in", "12in"},
	{"archB", "12in", "18in"},
	{"archC", "18in", "24in"},
	{"archD", "24in", "36in"},
	{"archE", "36in", "48in"},
	{"executive", "7.5in", "10in"},
	{"flsa", "8.5in", "13in"},
	{"flse", "8.5in", "13in"},
	{"halfletter", "5.5in", "8.5in"},
	{"letter", "8.5in", "11in"},
	{"legal", "8.5in", "14in"},
	{"ledger", "17in", "11in"},
	{"note", "7.5in", "10in"},
	{"tabloid", "11in", "17in"},
	{"statement", "5.5in", "8.5in"},
	{0, 0, 0}
};

static DviPaperClass str2class(const char *name)
{
	if(STRCEQ(name, "ISO"))
		return MDVI_PAPER_CLASS_ISO;
	else if(STRCEQ(name, "US"))
		return MDVI_PAPER_CLASS_US;
	return MDVI_PAPER_CLASS_CUSTOM;
}

int	mdvi_get_paper_size(const char *name, DviPaper *paper)
{
	const DviPaperSpec *sp;
	double	a, b;
	char	c, d, e, f;
	char	buf[32];

	paper->pclass = MDVI_PAPER_CLASS_CUSTOM;
	if(sscanf(name, "%lfx%lf%c%c", &a, &b, &c, &d) == 4) {
		sprintf(buf, "%12.16f%c%c", a, c, d);
		paper->inches_wide = unit2pix_factor(buf);
		sprintf(buf, "%12.16f%c%c", b, c, d);
		paper->inches_tall = unit2pix_factor(buf);
		paper->name = _("custom");
		return 0;
	} else if(sscanf(name, "%lf%c%c,%lf%c%c", &a, &c, &d, &b, &e, &f) == 6) {
		sprintf(buf, "%12.16f%c%c", a, c, d);
		paper->inches_wide = unit2pix_factor(buf);
		sprintf(buf, "%12.16f%c%c", b, e, f);
		paper->inches_tall = unit2pix_factor(buf);
		paper->name = _("custom");
		return 0;
	}

	for(sp = &papers[0]; sp->name; sp++) {
		if(!sp->width || !sp->height) {
			paper->pclass = str2class(sp->name);
			continue;
		}
		if(strcasecmp(sp->name, name) == 0) {
			paper->inches_wide = unit2pix_factor(sp->width);
			paper->inches_tall = unit2pix_factor(sp->height);
			paper->name = sp->name;
			return 0;
		}
	}
	return -1;
}

DviPaperSpec *mdvi_get_paper_specs(DviPaperClass pclass)
{
	int	i;
	int	first, count;
	DviPaperSpec *spec, *ptr;

	first = -1;
	count = 0;
	if(pclass == MDVI_PAPER_CLASS_ANY ||
	   pclass == MDVI_PAPER_CLASS_CUSTOM) {
	   	first = 0;
		count = (sizeof(papers) / sizeof(papers[0])) - 3;
	} else for(i = 0; papers[i].name; i++) {
		if(papers[i].width == NULL) {
			if(str2class(papers[i].name) == pclass)
				first = i;
			else if(first >= 0)
				break;
		} else if(first >= 0)
			count++;
	}
	ptr = spec = xnalloc(DviPaperSpec, count + 1);
	for(i = first; papers[i].name&& count > 0; i++) {
		if(papers[i].width) {
			ptr->name = papers[i].name;
			ptr->width = papers[i].width;
			ptr->height = papers[i].height;
			ptr++;
			count--;
		}
	}
	ptr->name = NULL;
	ptr->width = NULL;
	ptr->height = NULL;

	return spec;
}

void	mdvi_free_paper_specs(DviPaperSpec *spec)
{
	mdvi_free(spec);
}
