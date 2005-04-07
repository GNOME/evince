#ifndef MDVI_HASH
#define MDVI_HASH

/* Hash tables */


typedef struct _DviHashBucket DviHashBucket;
typedef struct _DviHashTable DviHashTable;

/*
 * Hash tables
 */

typedef Uchar	*DviHashKey;
#define MDVI_KEY(x)	((DviHashKey)(x))

typedef Ulong	(*DviHashFunc) __PROTO((DviHashKey key));
typedef int	(*DviHashComp) __PROTO((DviHashKey key1, DviHashKey key2));
typedef void	(*DviHashFree) __PROTO((DviHashKey key, void *data));


struct _DviHashTable {
	DviHashBucket	**buckets;
	int	nbucks;
	int	nkeys;
	DviHashFunc hash_func;
	DviHashComp hash_comp;
	DviHashFree hash_free;
};
#define MDVI_EMPTY_HASH_TABLE {NULL, 0, 0, NULL, NULL, NULL}

#define MDVI_HASH_REPLACE	0
#define MDVI_HASH_UNIQUE	1
#define MDVI_HASH_UNCHECKED	2

extern void mdvi_hash_init __PROTO((DviHashTable *));
extern void mdvi_hash_create __PROTO((DviHashTable *, int));
extern int  mdvi_hash_add __PROTO((DviHashTable *, DviHashKey, void *, int));
extern int  mdvi_hash_destroy_key __PROTO((DviHashTable *, DviHashKey));
extern void mdvi_hash_reset __PROTO((DviHashTable *, int));
extern void *mdvi_hash_lookup __PROTO((DviHashTable *, DviHashKey));
extern void *mdvi_hash_remove __PROTO((DviHashTable *, DviHashKey));
extern void *mdvi_hash_remove_ptr __PROTO((DviHashTable *, DviHashKey));

#define mdvi_hash_flush(h)	mdvi_hash_reset((h), 1)
#define mdvi_hash_destroy(h)	mdvi_hash_reset((h), 0)

#endif

