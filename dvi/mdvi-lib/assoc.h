#ifndef MDVI_ASSOC_H
#define MDVI_ASSOC_H

/* Associations */
extern int	mdvi_assoc_put 
		__PROTO((DviContext *, char *, void *, DviFree2Func));
extern void *	mdvi_assoc_get __PROTO((DviContext *, char *));
extern void *	mdvi_assoc_del __PROTO((DviContext *, char *));
extern void	mdvi_assoc_free __PROTO((DviContext *, char *));
extern void	mdvi_assoc_flush __PROTO((DviContext *));


#endif

