#pragma once
/*
** $Id: lvm.h $
** Mask virtual machine
** See Copyright Notice in mask.h
*/

#include "ldo.h"
#include "lobject.h"
#include "ltm.h"


#if !defined(MASK_NOCVTN2S)
#define cvt2str(o)	(ttisnumber(o) || ttisboolean(o))
#else
#define cvt2str(o)	0	/* no conversion from numbers to strings */
#endif


#if !defined(MASK_NOCVTS2N)
#define cvt2num(o)	ttisstring(o)
#else
#define cvt2num(o)	0	/* no conversion from strings to numbers */
#endif


/*
** You can define MASK_FLOORN2I if you want to convert floats to integers
** by flooring them (instead of raising an error if they are not
** integral values)
*/
#if !defined(MASK_FLOORN2I)
#define MASK_FLOORN2I		F2Ieq
#endif


/*
** Rounding modes for float->integer coercion
 */
typedef enum {
  F2Ieq,     /* no rounding; accepts only integral values */
  F2Ifloor,  /* takes the floor of the number */
  F2Iceil    /* takes the ceil of the number */
} F2Imod;


/* convert an object to a float (including string coercion) */
#define tonumber(o,n) \
    (ttisfloat(o) ? (*(n) = fltvalue(o), 1) : maskV_tonumber_(o,n))


/* convert an object to a float (without string coercion) */
#define tonumberns(o,n) \
    (ttisfloat(o) ? ((n) = fltvalue(o), 1) : \
    (ttisinteger(o) ? ((n) = cast_num(ivalue(o)), 1) : 0))


/* convert an object to an integer (including string coercion) */
#define tointeger(o,i) \
  (l_likely(ttisinteger(o)) ? (*(i) = ivalue(o), 1) \
                          : maskV_tointeger(o,i,MASK_FLOORN2I))


/* convert an object to an integer (without string coercion) */
#define tointegerns(o,i) \
  (l_likely(ttisinteger(o)) ? (*(i) = ivalue(o), 1) \
                          : maskV_tointegerns(o,i,MASK_FLOORN2I))


#define intop(op,v1,v2) l_castU2S(l_castS2U(v1) op l_castS2U(v2))

#define maskV_rawequalobj(t1,t2)		maskV_equalobj(NULL,t1,t2)


/*
** fast track for 'gettable': if 't' is a table and 't[k]' is present,
** return 1 with 'slot' pointing to 't[k]' (position of final result).
** Otherwise, return 0 (meaning it will have to check metamethod)
** with 'slot' pointing to an empty 't[k]' (if 't' is a table) or NULL
** (otherwise). 'f' is the raw get function to use.
*/
#define maskV_fastget(L,t,k,slot,f) \
  (!ttistable(t)  \
   ? (slot = NULL, 0)  /* not a table; 'slot' is NULL and result is 0 */  \
   : (slot = f(hvalue(t), k),  /* else, do raw access */  \
      !isempty(slot)))  /* result not empty? */


/*
** Special case of 'maskV_fastget' for integers, inlining the fast case
** of 'maskH_getint'.
*/
#define maskV_fastgeti(L,t,k,slot) \
  (!ttistable(t)  \
   ? (slot = NULL, 0)  /* not a table; 'slot' is NULL and result is 0 */  \
   : (slot = (l_castS2U(k) - 1u < hvalue(t)->alimit) \
              ? &hvalue(t)->array[k - 1] : maskH_getint(hvalue(t), k), \
      !isempty(slot)))  /* result not empty? */


/*
** Finish a fast set operation (when fast get succeeds). In that case,
** 'slot' points to the place to put the value.
*/
#define maskV_finishfastset(L,t,slot,v) \
    { setobj2t(L, cast(TValue *,slot), v); \
      maskC_barrierback(L, gcvalue(t), v); }


/*
** Shift right is the same as shift left with a negative 'y'
*/
#define maskV_shiftr(x,y)	maskV_shiftl(x,intop(-, 0, y))



MASKI_FUNC int maskV_equalobj (mask_State *L, const TValue *t1, const TValue *t2);
MASKI_FUNC int maskV_lessthan (mask_State *L, const TValue *l, const TValue *r);
MASKI_FUNC int maskV_lessequal (mask_State *L, const TValue *l, const TValue *r);
MASKI_FUNC int maskV_tonumber_ (const TValue *obj, mask_Number *n);
MASKI_FUNC int maskV_tointeger (const TValue *obj, mask_Integer *p, F2Imod mode);
MASKI_FUNC int maskV_tointegerns (const TValue *obj, mask_Integer *p,
                                F2Imod mode);
MASKI_FUNC int maskV_flttointeger (mask_Number n, mask_Integer *p, F2Imod mode);
MASKI_FUNC void maskV_finishget (mask_State *L, const TValue *t, TValue *key,
                               StkId val, const TValue *slot);
MASKI_FUNC void maskV_finishset (mask_State *L, const TValue *t, TValue *key,
                               TValue *val, const TValue *slot);
MASKI_FUNC void maskV_finishOp (mask_State *L);
MASKI_FUNC void maskV_execute (mask_State *L, CallInfo *ci);
MASKI_FUNC void maskV_concat (mask_State *L, int total);
MASKI_FUNC mask_Integer maskV_idiv (mask_State *L, mask_Integer x, mask_Integer y);
MASKI_FUNC mask_Integer maskV_mod (mask_State *L, mask_Integer x, mask_Integer y);
MASKI_FUNC mask_Number maskV_modf (mask_State *L, mask_Number x, mask_Number y);
MASKI_FUNC mask_Integer maskV_shiftl (mask_Integer x, mask_Integer y);
MASKI_FUNC void maskV_objlen (mask_State *L, StkId ra, const TValue *rb);
