#pragma once
/*
** $Id: lfunc.h $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in mask.h
*/

#include "lobject.h"


#define sizeCclosure(n)	(cast_int(offsetof(CClosure, upvalue)) + \
                         cast_int(sizeof(TValue)) * (n))

#define sizeLclosure(n)	(cast_int(offsetof(LClosure, upvals)) + \
                         cast_int(sizeof(TValue *)) * (n))


/* test whether thread is in 'twups' list */
#define isintwups(L)	(L->twups != L)


/*
** maximum number of upvalues in a closure (both C and Mask). (Value
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


MASKI_FUNC Proto *maskF_newproto (mask_State *L);
MASKI_FUNC CClosure *maskF_newCclosure (mask_State *L, int nupvals);
MASKI_FUNC LClosure *maskF_newLclosure (mask_State *L, int nupvals);
MASKI_FUNC void maskF_initupvals (mask_State *L, LClosure *cl);
MASKI_FUNC UpVal *maskF_findupval (mask_State *L, StkId level);
MASKI_FUNC void maskF_newtbcupval (mask_State *L, StkId level);
MASKI_FUNC void maskF_closeupval (mask_State *L, StkId level);
MASKI_FUNC StkId maskF_close (mask_State *L, StkId level, int status, int yy);
MASKI_FUNC void maskF_unlinkupval (UpVal *uv);
MASKI_FUNC void maskF_freeproto (mask_State *L, Proto *f);
MASKI_FUNC const char *maskF_getlocalname (const Proto *func, int local_number,
                                         int pc);
