#pragma once
/*
** $Id: ltable.h $
** Hello tables (hash)
** See Copyright Notice in hello.h
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


HELLOI_FUNC const TValue *helloH_getint (Table *t, hello_Integer key);
HELLOI_FUNC void helloH_setint (hello_State *L, Table *t, hello_Integer key,
                                                    TValue *value);
HELLOI_FUNC const TValue *helloH_getshortstr (Table *t, TString *key);
HELLOI_FUNC const TValue *helloH_getstr (Table *t, TString *key);
HELLOI_FUNC const TValue *helloH_get (Table *t, const TValue *key);
HELLOI_FUNC void helloH_newkey (hello_State *L, Table *t, const TValue *key,
                                                    TValue *value);
HELLOI_FUNC void helloH_set (hello_State *L, Table *t, const TValue *key,
                                                 TValue *value);
HELLOI_FUNC void helloH_finishset (hello_State *L, Table *t, const TValue *key,
                                       const TValue *slot, TValue *value);
HELLOI_FUNC Table *helloH_new (hello_State *L);
HELLOI_FUNC void helloH_resize (hello_State *L, Table *t, unsigned int nasize,
                                                    unsigned int nhsize);
HELLOI_FUNC void helloH_resizearray (hello_State *L, Table *t, unsigned int nasize);
HELLOI_FUNC void helloH_free (hello_State *L, Table *t);
HELLOI_FUNC int helloH_next (hello_State *L, Table *t, StkId key);
HELLOI_FUNC hello_Unsigned helloH_getn (Table *t);
HELLOI_FUNC unsigned int helloH_realasize (const Table *t);


#if defined(HELLO_DEBUG)
HELLOI_FUNC Node *helloH_mainposition (const Table *t, const TValue *key);
#endif
