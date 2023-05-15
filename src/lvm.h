#pragma once
/*
** $Id: lvm.h $
** Hello virtual machine
** See Copyright Notice in hello.h
*/

#include "ldo.h"
#include "lobject.h"
#include "ltm.h"


#if !defined(HELLO_NOCVTN2S)
#define cvt2str(o)	(ttisnumber(o) || ttisboolean(o))
#else
#define cvt2str(o)	0	/* no conversion from numbers to strings */
#endif


#if !defined(HELLO_NOCVTS2N)
#define cvt2num(o)	ttisstring(o)
#else
#define cvt2num(o)	0	/* no conversion from strings to numbers */
#endif


/*
** You can define HELLO_FLOORN2I if you want to convert floats to integers
** by flooring them (instead of raising an error if they are not
** integral values)
*/
#if !defined(HELLO_FLOORN2I)
#define HELLO_FLOORN2I		F2Ieq
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
    (ttisfloat(o) ? (*(n) = fltvalue(o), 1) : helloV_tonumber_(o,n))


/* convert an object to a float (without string coercion) */
#define tonumberns(o,n) \
    (ttisfloat(o) ? ((n) = fltvalue(o), 1) : \
    (ttisinteger(o) ? ((n) = cast_num(ivalue(o)), 1) : 0))


/* convert an object to an integer (including string coercion) */
#define tointeger(o,i) \
  (l_likely(ttisinteger(o)) ? (*(i) = ivalue(o), 1) \
                          : helloV_tointeger(o,i,HELLO_FLOORN2I))


/* convert an object to an integer (without string coercion) */
#define tointegerns(o,i) \
  (l_likely(ttisinteger(o)) ? (*(i) = ivalue(o), 1) \
                          : helloV_tointegerns(o,i,HELLO_FLOORN2I))


#define intop(op,v1,v2) l_castU2S(l_castS2U(v1) op l_castS2U(v2))

#define helloV_rawequalobj(t1,t2)		helloV_equalobj(NULL,t1,t2)


/*
** fast track for 'gettable': if 't' is a table and 't[k]' is present,
** return 1 with 'slot' pointing to 't[k]' (position of final result).
** Otherwise, return 0 (meaning it will have to check metamethod)
** with 'slot' pointing to an empty 't[k]' (if 't' is a table) or NULL
** (otherwise). 'f' is the raw get function to use.
*/
#define helloV_fastget(L,t,k,slot,f) \
  (!ttistable(t)  \
   ? (slot = NULL, 0)  /* not a table; 'slot' is NULL and result is 0 */  \
   : (slot = f(hvalue(t), k),  /* else, do raw access */  \
      !isempty(slot)))  /* result not empty? */


/*
** Special case of 'helloV_fastget' for integers, inlining the fast case
** of 'helloH_getint'.
*/
#define helloV_fastgeti(L,t,k,slot) \
  (!ttistable(t)  \
   ? (slot = NULL, 0)  /* not a table; 'slot' is NULL and result is 0 */  \
   : (slot = (l_castS2U(k) - 1u < hvalue(t)->alimit) \
              ? &hvalue(t)->array[k - 1] : helloH_getint(hvalue(t), k), \
      !isempty(slot)))  /* result not empty? */


/*
** Finish a fast set operation (when fast get succeeds). In that case,
** 'slot' points to the place to put the value.
*/
#define helloV_finishfastset(L,t,slot,v) \
    { setobj2t(L, cast(TValue *,slot), v); \
      helloC_barrierback(L, gcvalue(t), v); }


/*
** Shift right is the same as shift left with a negative 'y'
*/
#define helloV_shiftr(x,y)	helloV_shiftl(x,intop(-, 0, y))



HELLOI_FUNC int helloV_equalobj (hello_State *L, const TValue *t1, const TValue *t2);
HELLOI_FUNC int helloV_lessthan (hello_State *L, const TValue *l, const TValue *r);
HELLOI_FUNC int helloV_lessequal (hello_State *L, const TValue *l, const TValue *r);
HELLOI_FUNC int helloV_tonumber_ (const TValue *obj, hello_Number *n);
HELLOI_FUNC int helloV_tointeger (const TValue *obj, hello_Integer *p, F2Imod mode);
HELLOI_FUNC int helloV_tointegerns (const TValue *obj, hello_Integer *p,
                                F2Imod mode);
HELLOI_FUNC int helloV_flttointeger (hello_Number n, hello_Integer *p, F2Imod mode);
HELLOI_FUNC void helloV_finishget (hello_State *L, const TValue *t, TValue *key,
                               StkId val, const TValue *slot);
HELLOI_FUNC void helloV_finishset (hello_State *L, const TValue *t, TValue *key,
                               TValue *val, const TValue *slot);
HELLOI_FUNC void helloV_finishOp (hello_State *L);
HELLOI_FUNC void helloV_execute (hello_State *L, CallInfo *ci);
HELLOI_FUNC void helloV_concat (hello_State *L, int total);
HELLOI_FUNC hello_Integer helloV_idiv (hello_State *L, hello_Integer x, hello_Integer y);
HELLOI_FUNC hello_Integer helloV_mod (hello_State *L, hello_Integer x, hello_Integer y);
HELLOI_FUNC hello_Number helloV_modf (hello_State *L, hello_Number x, hello_Number y);
HELLOI_FUNC hello_Integer helloV_shiftl (hello_Integer x, hello_Integer y);
HELLOI_FUNC void helloV_objlen (hello_State *L, StkId ra, const TValue *rb);
