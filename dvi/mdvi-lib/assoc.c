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

#include "mdvi.h"
#include "assoc.h"
#include "hash.h"

typedef struct {
	void	*data;
	DviFree2Func free_func;
} DviAssoc;

#define MDVI_ASSOC_SIZE	31

static void assoc_free(DviHashKey key, void *ptr)
{
	DviAssoc *assoc = (DviAssoc *)ptr;

	DEBUG((4, "Destroying association `%s'\n", (char *)key));	
	if(assoc->free_func)
		assoc->free_func(key, assoc->data);
	xfree(assoc);
}

int	mdvi_assoc_put(DviContext *dvi, char *key, void *data, DviFree2Func f)
{
	DviAssoc *assoc;
	int	ok;
		
	if(dvi->assoc.buckets == NULL) {
		mdvi_hash_create(&dvi->assoc, MDVI_ASSOC_SIZE);
		dvi->assoc.hash_free = assoc_free;
	}
	assoc = xalloc(DviAssoc);
	assoc->data = data;
	assoc->free_func = f;

	ok = mdvi_hash_add(&dvi->assoc, MDVI_KEY(key), 
		assoc, MDVI_HASH_UNIQUE);
	if(ok < 0) {
		xfree(assoc);
		return -1;
	}
	return 0;
}

void	*mdvi_assoc_get(DviContext *dvi, char *key)
{
	DviAssoc *assoc;
	
	if(dvi->assoc.buckets == NULL)
		return NULL;
	assoc = (DviAssoc *)mdvi_hash_lookup(&dvi->assoc, MDVI_KEY(key));
	return assoc ? assoc->data : NULL;
}

void	*mdvi_assoc_del(DviContext *dvi, char *key)
{
	DviAssoc *assoc;
	void	*ptr;
	
	if(dvi->assoc.buckets == NULL)
		return NULL;
	assoc = mdvi_hash_remove(&dvi->assoc, MDVI_KEY(key));
	if(assoc == NULL)
		return NULL;
	ptr = assoc->data;
	xfree(assoc);
	return ptr;
}

void	mdvi_assoc_free(DviContext *dvi, char *key)
{
	if(dvi->assoc.buckets) {
		/* this will call `assoc_free' */
		mdvi_hash_destroy_key(&dvi->assoc, MDVI_KEY(key));
	}
}

void	mdvi_assoc_flush(DviContext *dvi)
{
	if(dvi->assoc.buckets)
		mdvi_hash_reset(&dvi->assoc, 0);
}
