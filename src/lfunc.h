#pragma once
/*
** $Id: lfunc.h $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in hello.h
*/

#include "lobject.h"


#define sizeCclosure(n)	(cast_int(offsetof(CClosure, upvalue)) + \
                         cast_int(sizeof(TValue)) * (n))

#define sizeLclosure(n)	(cast_int(offsetof(LClosure, upvals)) + \
                         cast_int(sizeof(TValue *)) * (n))


/* test whether thread is in 'twups' list */
#define isintwups(L)	(L->twups != L)


/*
** maximum number of upvalues in a closure (both C and Hello). (Value
** must fit in a VM register.)
*/
#define MAXUPVAL	255


#define upisopen(up)	((up)->v != &(up)->u.value)


#define uplevel(up)	check_exp(upisopen(up), cast(StkId, (up)->v))


/*
** maximum number of misses before giving up the cache of closures
** in prototypes
*/
#define MAXMISS		10



/* special status to close upvalues preserving the top of the stack */
#define CLOSEKTOP	(-1)


HELLOI_FUNC Proto *helloF_newproto (hello_State *L);
HELLOI_FUNC CClosure *helloF_newCclosure (hello_State *L, int nupvals);
HELLOI_FUNC LClosure *helloF_newLclosure (hello_State *L, int nupvals);
HELLOI_FUNC void helloF_initupvals (hello_State *L, LClosure *cl);
HELLOI_FUNC UpVal *helloF_findupval (hello_State *L, StkId level);
HELLOI_FUNC void helloF_newtbcupval (hello_State *L, StkId level);
HELLOI_FUNC void helloF_closeupval (hello_State *L, StkId level);
HELLOI_FUNC StkId helloF_close (hello_State *L, StkId level, int status, int yy);
HELLOI_FUNC void helloF_unlinkupval (UpVal *uv);
HELLOI_FUNC void helloF_freeproto (hello_State *L, Proto *f);
HELLOI_FUNC const char *helloF_getlocalname (const Proto *func, int local_number,
                                         int pc);
