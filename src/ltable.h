#pragma once
/*
** $Id: ltable.h $
** Mask tables (hash)
** See Copyright Notice in mask.h
*/

#include "lobject.h"


#define gnode(t,i)	(&(t)->node[i])
#define gval(n)		(&(n)->i_val)
#define gnext(n)	((n)->u.next)


/*
** Clear all bits of fast-access metamethods, which means that the table
** may have any of these metamethods. (First access that fails after the
** clearing will set the bit again.)
*/
#define invalidateTMcache(t)	((t)->flags &= ~maskflags)


/* true when 't' is using 'dummynode' as its hash part */
#define isdummy(t)		((t)->lastfree == NULL)


/* allocated size for hash nodes */
#define allocsizenode(t)	(isdummy(t) ? 0 : sizenode(t))


/* returns the Node, given the value of a table entry */
#define nodefromval(v)	cast(Node *, (v))


MASKI_FUNC const TValue *maskH_getint (Table *t, mask_Integer key);
MASKI_FUNC void maskH_setint (mask_State *L, Table *t, mask_Integer key,
                                                    TValue *value);
MASKI_FUNC const TValue *maskH_getshortstr (Table *t, TString *key);
MASKI_FUNC const TValue *maskH_getstr (Table *t, TString *key);
MASKI_FUNC const TValue *maskH_get (Table *t, const TValue *key);
MASKI_FUNC void maskH_newkey (mask_State *L, Table *t, const TValue *key,
                                                    TValue *value);
MASKI_FUNC void maskH_set (mask_State *L, Table *t, const TValue *key,
                                                 TValue *value);
MASKI_FUNC void maskH_finishset (mask_State *L, Table *t, const TValue *key,
                                       const TValue *slot, TValue *value);
MASKI_FUNC Table *maskH_new (mask_State *L);
MASKI_FUNC void maskH_resize (mask_State *L, Table *t, unsigned int nasize,
                                                    unsigned int nhsize);
MASKI_FUNC void maskH_resizearray (mask_State *L, Table *t, unsigned int nasize);
MASKI_FUNC void maskH_free (mask_State *L, Table *t);
MASKI_FUNC int maskH_next (mask_State *L, Table *t, StkId key);
MASKI_FUNC mask_Unsigned maskH_getn (Table *t);
MASKI_FUNC unsigned int maskH_realasize (const Table *t);


#if defined(MASK_DEBUG)
MASKI_FUNC Node *maskH_mainposition (const Table *t, const TValue *key);
#endif
