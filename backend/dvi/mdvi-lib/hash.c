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
#include "mdvi.h"

/* simple hash tables for MDVI */


struct _DviHashBucket {
	DviHashBucket *next;
	DviHashKey	key;
	Ulong	hvalue;
	void	*data;
};

static Ulong hash_string(DviHashKey key)
{
	Uchar	*p;
	Ulong	h, g;

	for(h = 0, p = (Uchar *)key; *p; p++) {
		h = (h << 4UL) + *p;
		if((g = h & 0xf0000000L) != 0) {
			h ^= (g >> 24UL);
			h ^= g;
		}
	}

	return h;
}

static int hash_compare(DviHashKey k1, DviHashKey k2)
{
	return strcmp((char *)k1, (char *)k2);
}

void	mdvi_hash_init(DviHashTable *hash)
{
	hash->buckets = NULL;
	hash->nbucks  = 0;
	hash->nkeys   = 0;
	hash->hash_func = NULL;
	hash->hash_comp = NULL;
	hash->hash_free = NULL;
}

void	mdvi_hash_create(DviHashTable *hash, int size)
{
	int	i;

	hash->nbucks = size;
	hash->buckets = xnalloc(DviHashBucket *, size);
	for(i = 0; i < size; i++)
		hash->buckets[i] = NULL;
	hash->hash_func = hash_string;
	hash->hash_comp = hash_compare;
	hash->hash_free = NULL;
	hash->nkeys = 0;
}

static DviHashBucket *hash_find(DviHashTable *hash, DviHashKey key)
{
	Ulong	hval;
	DviHashBucket *buck;

	hval = (hash->hash_func(key) % hash->nbucks);

	for(buck = hash->buckets[hval]; buck; buck = buck->next)
		if(hash->hash_comp(buck->key, key) == 0)
			break;
	return buck;
}

/* Neither keys nor data are duplicated */
int	mdvi_hash_add(DviHashTable *hash, DviHashKey key, void *data, int rep)
{
	DviHashBucket *buck = NULL;
	Ulong	hval;

	if(rep != MDVI_HASH_UNCHECKED) {
		buck = hash_find(hash, key);
		if(buck != NULL) {
			if(buck->data == data)
				return 0;
			if(rep == MDVI_HASH_UNIQUE)
				return -1;
			if(hash->hash_free != NULL)
				hash->hash_free(buck->key, buck->data);
		}
	}
	if(buck == NULL) {
		buck = xalloc(DviHashBucket);
		buck->hvalue = hash->hash_func(key);
		hval = (buck->hvalue % hash->nbucks);
		buck->next = hash->buckets[hval];
		hash->buckets[hval] = buck;
		hash->nkeys++;
	}

	/* save key and data */
	buck->key = key;
	buck->data = data;

	return 0;
}

void	*mdvi_hash_lookup(DviHashTable *hash, DviHashKey key)
{
	DviHashBucket *buck = hash_find(hash, key);

	return buck ? buck->data : NULL;
}

static DviHashBucket *hash_remove(DviHashTable *hash, DviHashKey key)
{
	DviHashBucket *buck, *last;
	Ulong	hval;

	hval = hash->hash_func(key);
	hval %= hash->nbucks;

	for(last = NULL, buck = hash->buckets[hval]; buck; buck = buck->next) {
		if(hash->hash_comp(buck->key, key) == 0)
			break;
		last = buck;
	}
	if(buck == NULL)
		return NULL;
	if(last)
		last->next = buck->next;
	else
		hash->buckets[hval] = buck->next;
	hash->nkeys--;
	return buck;
}

void	*mdvi_hash_remove(DviHashTable *hash, DviHashKey key)
{
	DviHashBucket *buck = hash_remove(hash, key);
	void	*data = NULL;

	if(buck) {
		data = buck->data;
		mdvi_free(buck);
	}
	return data;
}

void	*mdvi_hash_remove_ptr(DviHashTable *hash, DviHashKey key)
{
	DviHashBucket *buck, *last;
	Ulong	hval;
	void	*ptr;

	hval = hash->hash_func(key);
	hval %= hash->nbucks;

	for(last = NULL, buck = hash->buckets[hval]; buck; buck = buck->next) {
		if(buck->key == key)
			break;
		last = buck;
	}
	if(buck == NULL)
		return NULL;
	if(last)
		last->next = buck->next;
	else
		hash->buckets[hval] = buck->next;
	hash->nkeys--;
	/* destroy the bucket */
	ptr = buck->data;
	mdvi_free(buck);
	return ptr;
}

int	mdvi_hash_destroy_key(DviHashTable *hash, DviHashKey key)
{
	DviHashBucket *buck = hash_remove(hash, key);

	if(buck == NULL)
		return -1;
	if(hash->hash_free)
		hash->hash_free(buck->key, buck->data);
	mdvi_free(buck);
	return 0;
}

void	mdvi_hash_reset(DviHashTable *hash, int reuse)
{
	int	i;
	DviHashBucket *buck;

	/* remove all keys in the hash table */
	for(i = 0; i < hash->nbucks; i++) {
		for(; (buck = hash->buckets[i]); ) {
			hash->buckets[i] = buck->next;
			if(hash->hash_free)
				hash->hash_free(buck->key, buck->data);
			mdvi_free(buck);
		}
	}
	hash->nkeys = 0;
	if(!reuse && hash->buckets) {
		mdvi_free(hash->buckets);
		hash->buckets = NULL;
		hash->nbucks = 0;
	} /* otherwise, it is left empty, ready to be reused */
}
