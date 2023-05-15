/*
** $Id: lapi.c $
** Mask API
** See Copyright Notice in mask.h
*/

#define lapi_c
#define MASK_CORE

#include "lprefix.h"


#include <limits.h>
#include <stdarg.h>
#include <string.h>

#include "lprefix.h"
#include "mask.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lundump.h"
#include "lvm.h"



const char mask_ident[] =
  "$MaskVersion: " MASK_COPYRIGHT " $"
  "$MaskAuthors: " MASK_AUTHORS " $";



/*
** Test for a valid index (one that is not the 'nilvalue').
** '!ttisnil(o)' implies 'o != &G(L)->nilvalue', so it is not needed.
** However, it covers the most common cases in a faster way.
*/
#define isvalid(L, o)	(!ttisnil(o) || o != &G(L)->nilvalue)


/* test for pseudo index */
#define ispseudo(i)		((i) <= MASK_REGISTRYINDEX)

/* test for upvalue */
#define isupvalue(i)		((i) < MASK_REGISTRYINDEX)


/*
** Convert an acceptable index to a pointer to its respective value.
** Non-valid indices return the special nil value 'G(L)->nilvalue'.
*/
static TValue *index2value (mask_State *L, int idx) {
  CallInfo *ci = L->ci;
  if (idx > 0) {
    StkId o = ci->func + idx;
    api_check(L, idx <= L->ci->top - (ci->func + 1), "unacceptable index");
    if (o >= L->top) return &G(L)->nilvalue;
    else return s2v(o);
  }
  else if (!ispseudo(idx)) {  /* negative index */
    api_check(L, idx != 0 && -idx <= L->top - (ci->func + 1), "invalid index");
    return s2v(L->top + idx);
  }
  else if (idx == MASK_REGISTRYINDEX)
    return &G(L)->l_registry;
  else {  /* upvalues */
    idx = MASK_REGISTRYINDEX - idx;
    api_check(L, idx <= MAXUPVAL + 1, "upvalue index too large");
    if (ttisCclosure(s2v(ci->func))) {  /* C closure? */
      CClosure *func = clCvalue(s2v(ci->func));
      return (idx <= func->nupvalues) ? &func->upvalue[idx-1]
                                      : &G(L)->nilvalue;
    }
    else {  /* light C function or Mask function (through a hook)?) */
      api_check(L, ttislcf(s2v(ci->func)), "caller not a C function");
      return &G(L)->nilvalue;  /* no upvalues */
    }
  }
}



/*
** Convert a valid actual index (not a pseudo-index) to its address.
*/
l_sinline StkId index2stack (mask_State *L, int idx) {
  CallInfo *ci = L->ci;
  if (idx > 0) {
    StkId o = ci->func + idx;
    api_check(L, o < L->top, "invalid index");
    return o;
  }
  else {    /* non-positive index */
    api_check(L, idx != 0 && -idx <= L->top - (ci->func + 1), "invalid index");
    api_check(L, !ispseudo(idx), "invalid index");
    return L->top + idx;
  }
}


MASK_API int mask_checkstack (mask_State *L, int n) {
  int res;
  CallInfo *ci;
  mask_lock(L);
  ci = L->ci;
  api_check(L, n >= 0, "negative 'n'");
  if (L->stack_last - L->top > n)  /* stack large enough? */
    res = 1;  /* yes; check is OK */
  else  /* need to grow stack */
    res = maskD_growstack(L, n, 0);
  if (res && ci->top < L->top + n)
    ci->top = L->top + n;  /* adjust frame top */
  mask_unlock(L);
  return res;
}


MASK_API void mask_xmove (mask_State *from, mask_State *to, int n) {
  int i;
  if (from == to) return;
  mask_lock(to);
  api_checknelems(from, n);
  api_check(from, G(from) == G(to), "moving among independent states");
  api_check(from, to->ci->top - to->top >= n, "stack overflow");
  from->top -= n;
  for (i = 0; i < n; i++) {
    setobjs2s(to, to->top, from->top + i);
    to->top++;  /* stack already checked by previous 'api_check' */
  }
  mask_unlock(to);
}


MASK_API mask_CFunction mask_atpanic (mask_State *L, mask_CFunction panicf) {
  mask_CFunction old;
  mask_lock(L);
  old = G(L)->panic;
  G(L)->panic = panicf;
  mask_unlock(L);
  return old;
}


MASK_API mask_Number mask_version (mask_State *L) {
  UNUSED(L);
  return MASK_VERSION_NUM;
}



/*
** basic stack manipulation
*/


/*
** convert an acceptable stack index into an absolute index
*/
MASK_API int mask_absindex (mask_State *L, int idx) {
  return (idx > 0 || ispseudo(idx))
         ? idx
         : cast_int(L->top - L->ci->func) + idx;
}


MASK_API int mask_gettop (mask_State *L) {
  return cast_int(L->top - (L->ci->func + 1));
}


MASK_API void mask_settop (mask_State *L, int idx) {
  CallInfo *ci;
  StkId func, newtop;
  ptrdiff_t diff;  /* difference for new top */
  mask_lock(L);
  ci = L->ci;
  func = ci->func;
  if (idx >= 0) {
    api_check(L, idx <= ci->top - (func + 1), "new top too large");
    diff = ((func + 1) + idx) - L->top;
    for (; diff > 0; diff--)
      setnilvalue(s2v(L->top++));  /* clear new slots */
  }
  else {
    api_check(L, -(idx+1) <= (L->top - (func + 1)), "invalid new top");
    diff = idx + 1;  /* will "subtract" index (as it is negative) */
  }
  api_check(L, L->tbclist < L->top, "previous pop of an unclosed slot");
  newtop = L->top + diff;
  if (diff < 0 && L->tbclist >= newtop) {
    mask_assert(hastocloseCfunc(ci->nresults));
    newtop = maskF_close(L, newtop, CLOSEKTOP, 0);
  }
  L->top = newtop;  /* correct top only after closing any upvalue */
  mask_unlock(L);
}


MASK_API void mask_closeslot (mask_State *L, int idx) {
  StkId level;
  mask_lock(L);
  level = index2stack(L, idx);
  api_check(L, hastocloseCfunc(L->ci->nresults) && L->tbclist == level,
     "no variable to close at given level");
  level = maskF_close(L, level, CLOSEKTOP, 0);
  setnilvalue(s2v(level));
  mask_unlock(L);
}


/*
** Reverse the stack segment from 'from' to 'to'
** (auxiliary to 'mask_rotate')
** Note that we move(copy) only the value inside the stack.
** (We do not move additional fields that may exist.)
*/
l_sinline void reverse (mask_State *L, StkId from, StkId to) {
  for (; from < to; from++, to--) {
    TValue temp;
    setobj(L, &temp, s2v(from));
    setobjs2s(L, from, to);
    setobj2s(L, to, &temp);
  }
}


/*
** Let x = AB, where A is a prefix of length 'n'. Then,
** rotate x n == BA. But BA == (A^r . B^r)^r.
*/
MASK_API void mask_rotate (mask_State *L, int idx, int n) {
  StkId p, t, m;
  mask_lock(L);
  t = L->top - 1;  /* end of stack segment being rotated */
  p = index2stack(L, idx);  /* start of segment */
  api_check(L, (n >= 0 ? n : -n) <= (t - p + 1), "invalid 'n'");
  m = (n >= 0 ? t - n : p - n - 1);  /* end of prefix */
  reverse(L, p, m);  /* reverse the prefix with length 'n' */
  reverse(L, m + 1, t);  /* reverse the suffix */
  reverse(L, p, t);  /* reverse the entire segment */
  mask_unlock(L);
}


MASK_API void mask_copy (mask_State *L, int fromidx, int toidx) {
  TValue *fr, *to;
  mask_lock(L);
  fr = index2value(L, fromidx);
  to = index2value(L, toidx);
  api_check(L, isvalid(L, to), "invalid index");
  setobj(L, to, fr);
  if (isupvalue(toidx))  /* function upvalue? */
    maskC_barrier(L, clCvalue(s2v(L->ci->func)), fr);
  /* MASK_REGISTRYINDEX does not need gc barrier
     (collector revisits it before finishing collection) */
  mask_unlock(L);
}


MASK_API void mask_pushvalue (mask_State *L, int idx) {
  mask_lock(L);
  setobj2s(L, L->top, index2value(L, idx));
  api_incr_top(L);
  mask_unlock(L);
}



/*
** access functions (stack -> C)
*/


MASK_API int mask_type (mask_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (isvalid(L, o) ? ttype(o) : MASK_TNONE);
}


MASK_API const char *mask_typename (mask_State *L, int t) {
  UNUSED(L);
  api_check(L, MASK_TNONE <= t && t < MASK_NUMTYPES, "invalid type");
  return ttypename(t);
}


MASK_API int mask_iscfunction (mask_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttislcf(o) || (ttisCclosure(o)));
}


MASK_API int mask_isinteger (mask_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return ttisinteger(o);
}


MASK_API int mask_isnumber (mask_State *L, int idx) {
  mask_Number n;
  const TValue *o = index2value(L, idx);
  return tonumber(o, &n);
}


MASK_API int mask_isstring (mask_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttisstring(o) || cvt2str(o));
}


MASK_API int mask_isuserdata (mask_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttisfulluserdata(o) || ttislightuserdata(o));
}


MASK_API int mask_rawequal (mask_State *L, int index1, int index2) {
  const TValue *o1 = index2value(L, index1);
  const TValue *o2 = index2value(L, index2);
  return (isvalid(L, o1) && isvalid(L, o2)) ? maskV_rawequalobj(o1, o2) : 0;
}


MASK_API void mask_arith (mask_State *L, int op) {
  mask_lock(L);
  if (op != MASK_OPUNM && op != MASK_OPBNOT)
    api_checknelems(L, 2);  /* all other operations expect two operands */
  else {  /* for unary operations, add fake 2nd operand */
    api_checknelems(L, 1);
    setobjs2s(L, L->top, L->top - 1);
    api_incr_top(L);
  }
  /* first operand at top - 2, second at top - 1; result go to top - 2 */
  maskO_arith(L, op, s2v(L->top - 2), s2v(L->top - 1), L->top - 2);
  L->top--;  /* remove second operand */
  mask_unlock(L);
}


MASK_API int mask_compare (mask_State *L, int index1, int index2, int op) {
  const TValue *o1;
  const TValue *o2;
  int i = 0;
  mask_lock(L);  /* may call tag method */
  o1 = index2value(L, index1);
  o2 = index2value(L, index2);
  if (isvalid(L, o1) && isvalid(L, o2)) {
    switch (op) {
      case MASK_OPEQ: i = maskV_equalobj(L, o1, o2); break;
      case MASK_OPLT: i = maskV_lessthan(L, o1, o2); break;
      case MASK_OPLE: i = maskV_lessequal(L, o1, o2); break;
      default: api_check(L, 0, "invalid option");
    }
  }
  mask_unlock(L);
  return i;
}


MASK_API size_t mask_stringtonumber (mask_State *L, const char *s) {
  size_t sz = maskO_str2num(s, s2v(L->top));
  if (sz != 0)
    api_incr_top(L);
  return sz;
}


MASK_API mask_Number mask_tonumberx (mask_State *L, int idx, int *pisnum) {
  mask_Number n = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tonumber(o, &n);
  if (pisnum)
    *pisnum = isnum;
  return n;
}


MASK_API mask_Integer mask_tointegerx (mask_State *L, int idx, int *pisnum) {
  mask_Integer res = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tointeger(o, &res);
  if (pisnum)
    *pisnum = isnum;
  return res;
}


MASK_API int mask_toboolean (mask_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return !l_isfalse(o);
}


MASK_API int mask_istrue(mask_State *L, int idx) noexcept
{
  const TValue *o = index2value(L, idx);
  return ttistrue(o);
}


MASK_API const char *mask_tolstring (mask_State *L, int idx, size_t *len) {
  TValue *o;
  mask_lock(L);
  o = index2value(L, idx);
  if (!ttisstring(o)) {
    if (!cvt2str(o)) {  /* not convertible? */
      if (len != NULL) *len = 0;
      mask_unlock(L);
      return NULL;
    }
    maskO_tostring(L, o);
    maskC_checkGC(L);
    o = index2value(L, idx);  /* previous call may reallocate the stack */
  }
  if (len != NULL)
    *len = vslen(o);
  mask_unlock(L);
  return svalue(o);
}


MASK_API mask_Unsigned mask_rawlen (mask_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o)) {
    case MASK_VSHRSTR: return tsvalue(o)->shrlen;
    case MASK_VLNGSTR: return tsvalue(o)->u.lnglen;
    case MASK_VUSERDATA: return uvalue(o)->len;
    case MASK_VTABLE: return maskH_getn(hvalue(o));
    default: return 0;
  }
}


MASK_API mask_CFunction mask_tocfunction (mask_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  if (ttislcf(o)) return fvalue(o);
  else if (ttisCclosure(o))
    return clCvalue(o)->f;
  else return NULL;  /* not a C function */
}


l_sinline void *touserdata (const TValue *o) {
  switch (ttype(o)) {
    case MASK_TUSERDATA: return getudatamem(uvalue(o));
    case MASK_TLIGHTUSERDATA: return pvalue(o);
    default: return NULL;
  }
}


MASK_API void *mask_touserdata (mask_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return touserdata(o);
}


MASK_API mask_State *mask_tothread (mask_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (!ttisthread(o)) ? NULL : thvalue(o);
}


/*
** Returns a pointer to the internal representation of an object.
** Note that ANSI C does not allow the conversion of a pointer to
** function to a 'void*', so the conversion here goes through
** a 'size_t'. (As the returned pointer is only informative, this
** conversion should not be a problem.)
*/
MASK_API const void *mask_topointer (mask_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o)) {
    case MASK_VLCF: return cast_voidp(cast_sizet(fvalue(o)));
    case MASK_VUSERDATA: case MASK_VLIGHTUSERDATA:
      return touserdata(o);
    default: {
      if (iscollectable(o))
        return gcvalue(o);
      else
        return NULL;
    }
  }
}



/*
** push functions (C -> stack)
*/


MASK_API void mask_pushnil (mask_State *L) {
  mask_lock(L);
  setnilvalue(s2v(L->top));
  api_incr_top(L);
  mask_unlock(L);
}


MASK_API void mask_pushnumber (mask_State *L, mask_Number n) {
  mask_lock(L);
  setfltvalue(s2v(L->top), n);
  api_incr_top(L);
  mask_unlock(L);
}


MASK_API void mask_pushinteger (mask_State *L, mask_Integer n) {
  mask_lock(L);
  setivalue(s2v(L->top), n);
  api_incr_top(L);
  mask_unlock(L);
}


/*
** Pushes on the stack a string with given length. Avoid using 's' when
** 'len' == 0 (as 's' can be NULL in that case), due to later use of
** 'memcmp' and 'memcpy'.
*/
MASK_API const char *mask_pushlstring (mask_State *L, const char *s, size_t len) {
  TString *ts;
  mask_lock(L);
  ts = (len == 0) ? maskS_new(L, "") : maskS_newlstr(L, s, len);
  setsvalue2s(L, L->top, ts);
  api_incr_top(L);
  maskC_checkGC(L);
  mask_unlock(L);
  return getstr(ts);
}


MASK_API const char *mask_pushstring (mask_State *L, const char *s) {
  mask_lock(L);
  if (s == NULL)
    setnilvalue(s2v(L->top));
  else {
    TString *ts;
    ts = maskS_new(L, s);
    setsvalue2s(L, L->top, ts);
    s = getstr(ts);  /* internal copy's address */
  }
  api_incr_top(L);
  maskC_checkGC(L);
  mask_unlock(L);
  return s;
}

MASK_API const char* mask_pushstring(mask_State* L, const std::string& str) {
  return mask_pushstring(L, str.c_str());
}


MASK_API const char *mask_pushvfstring (mask_State *L, const char *fmt,
                                      va_list argp) {
  const char *ret;
  mask_lock(L);
  ret = maskO_pushvfstring(L, fmt, argp);
  maskC_checkGC(L);
  mask_unlock(L);
  return ret;
}


MASK_API const char *mask_pushfstring (mask_State *L, const char *fmt, ...) {
  const char *ret;
  va_list argp;
  mask_lock(L);
  va_start(argp, fmt);
  ret = maskO_pushvfstring(L, fmt, argp);
  va_end(argp);
  maskC_checkGC(L);
  mask_unlock(L);
  return ret;
}


MASK_API void mask_pushcclosure (mask_State *L, mask_CFunction fn, int n) {
  mask_lock(L);
  if (n == 0) {
    setfvalue(s2v(L->top), fn);
    api_incr_top(L);
  }
  else {
    CClosure *cl;
    api_checknelems(L, n);
    api_check(L, n <= MAXUPVAL, "upvalue index too large");
    cl = maskF_newCclosure(L, n);
    cl->f = fn;
    L->top -= n;
    while (n--) {
      setobj2n(L, &cl->upvalue[n], s2v(L->top + n));
      /* does not need barrier because closure is white */
      mask_assert(iswhite(cl));
    }
    setclCvalue(L, s2v(L->top), cl);
    api_incr_top(L);
    maskC_checkGC(L);
  }
  mask_unlock(L);
}


MASK_API void mask_pushboolean (mask_State *L, int b) {
  mask_lock(L);
  if (b)
    setbtvalue(s2v(L->top));
  else
    setbfvalue(s2v(L->top));
  api_incr_top(L);
  mask_unlock(L);
}


MASK_API void mask_pushlightuserdata (mask_State *L, void *p) {
  mask_lock(L);
  setpvalue(s2v(L->top), p);
  api_incr_top(L);
  mask_unlock(L);
}


MASK_API int mask_pushthread (mask_State *L) {
  mask_lock(L);
  setthvalue(L, s2v(L->top), L);
  api_incr_top(L);
  mask_unlock(L);
  return (G(L)->mainthread == L);
}



/*
** get functions (Mask -> stack)
*/


l_sinline int auxgetstr (mask_State *L, const TValue *t, const char *k) {
  const TValue *slot;
  TString *str = maskS_new(L, k);
  if (maskV_fastget(L, t, str, slot, maskH_getstr)) {
    setobj2s(L, L->top, slot);
    api_incr_top(L);
  }
  else {
    setsvalue2s(L, L->top, str);
    api_incr_top(L);
    maskV_finishget(L, t, s2v(L->top - 1), L->top - 1, slot);
  }
  mask_unlock(L);
  return ttype(s2v(L->top - 1));
}


/*
** Get the global table in the registry. Since all predefined
** indices in the registry were inserted right when the registry
** was created and never removed, they must always be in the array
** part of the registry.
*/
#define getGtable(L)  \
    (&hvalue(&G(L)->l_registry)->array[MASK_RIDX_GLOBALS - 1])


MASK_API int mask_getglobal (mask_State *L, const char *name) {
  const TValue *G;
  mask_lock(L);
  G = getGtable(L);
  return auxgetstr(L, G, name);
}


MASK_API int mask_gettable (mask_State *L, int idx) {
  const TValue *slot;
  TValue *t;
  mask_lock(L);
  t = index2value(L, idx);
  if (maskV_fastget(L, t, s2v(L->top - 1), slot, maskH_get)) {
    setobj2s(L, L->top - 1, slot);
  }
  else
    maskV_finishget(L, t, s2v(L->top - 1), L->top - 1, slot);
  mask_unlock(L);
  return ttype(s2v(L->top - 1));
}


MASK_API int mask_getfield (mask_State *L, int idx, const char *k) {
  mask_lock(L);
  return auxgetstr(L, index2value(L, idx), k);
}


MASK_API int mask_geti (mask_State *L, int idx, mask_Integer n) {
  TValue *t;
  const TValue *slot;
  mask_lock(L);
  t = index2value(L, idx);
  if (maskV_fastgeti(L, t, n, slot)) {
    setobj2s(L, L->top, slot);
  }
  else {
    TValue aux;
    setivalue(&aux, n);
    maskV_finishget(L, t, &aux, L->top, slot);
  }
  api_incr_top(L);
  mask_unlock(L);
  return ttype(s2v(L->top - 1));
}


l_sinline int finishrawget (mask_State *L, const TValue *val) {
  if (isempty(val))  /* avoid copying empty items to the stack */
    setnilvalue(s2v(L->top));
  else
    setobj2s(L, L->top, val);
  api_incr_top(L);
  mask_unlock(L);
  return ttype(s2v(L->top - 1));
}


static Table *gettable (mask_State *L, int idx) {
  TValue *t = index2value(L, idx);
  api_check(L, ttistable(t), "table expected");
  return hvalue(t);
}


MASK_API int mask_rawget (mask_State *L, int idx) {
  Table *t;
  const TValue *val;
  mask_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  val = maskH_get(t, s2v(L->top - 1));
  L->top--;  /* remove key */
  return finishrawget(L, val);
}


MASK_API int mask_rawgeti (mask_State *L, int idx, mask_Integer n) {
  Table *t;
  mask_lock(L);
  t = gettable(L, idx);
  return finishrawget(L, maskH_getint(t, n));
}


MASK_API int mask_rawgetp (mask_State *L, int idx, const void *p) {
  Table *t;
  TValue k;
  mask_lock(L);
  t = gettable(L, idx);
  setpvalue(&k, cast_voidp(p));
  return finishrawget(L, maskH_get(t, &k));
}


MASK_API void mask_createtable (mask_State *L, int narray, int nrec) {
  Table *t;
  mask_lock(L);
  t = maskH_new(L);
  sethvalue2s(L, L->top, t);
  api_incr_top(L);
  if (narray > 0 || nrec > 0)
    maskH_resize(L, t, narray, nrec);
  maskC_checkGC(L);
  mask_unlock(L);
}


MASK_API int mask_getmetatable (mask_State *L, int objindex) {
  const TValue *obj;
  Table *mt;
  int res = 0;
  mask_lock(L);
  obj = index2value(L, objindex);
  switch (ttype(obj)) {
    case MASK_TTABLE:
      mt = hvalue(obj)->metatable;
      break;
    case MASK_TUSERDATA:
      mt = uvalue(obj)->metatable;
      break;
    default:
      mt = G(L)->mt[ttype(obj)];
      break;
  }
  if (mt != NULL) {
    sethvalue2s(L, L->top, mt);
    api_incr_top(L);
    res = 1;
  }
  mask_unlock(L);
  return res;
}


MASK_API int mask_getiuservalue (mask_State *L, int idx, int n) {
  TValue *o;
  int t;
  mask_lock(L);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (n <= 0 || n > uvalue(o)->nuvalue) {
    setnilvalue(s2v(L->top));
    t = MASK_TNONE;
  }
  else {
    setobj2s(L, L->top, &uvalue(o)->uv[n - 1].uv);
    t = ttype(s2v(L->top));
  }
  api_incr_top(L);
  mask_unlock(L);
  return t;
}


/*
** set functions (stack -> Mask)
*/

/*
** t[k] = value at the top of the stack (where 'k' is a string)
*/
static void auxsetstr (mask_State *L, const TValue *t, const char *k) {
  const TValue *slot;
  TString *str = maskS_new(L, k);
  api_checknelems(L, 1);
  if (maskV_fastget(L, t, str, slot, maskH_getstr)) {
    maskV_finishfastset(L, t, slot, s2v(L->top - 1));
    L->top--;  /* pop value */
  }
  else {
    setsvalue2s(L, L->top, str);  /* push 'str' (to make it a TValue) */
    api_incr_top(L);
    maskV_finishset(L, t, s2v(L->top - 1), s2v(L->top - 2), slot);
    L->top -= 2;  /* pop value and key */
  }
  mask_unlock(L);  /* lock done by caller */
}


MASK_API void mask_setglobal (mask_State *L, const char *name) {
  const TValue *G;
  mask_lock(L);  /* unlock done in 'auxsetstr' */
  G = getGtable(L);
  auxsetstr(L, G, name);
}


MASK_API void mask_settable (mask_State *L, int idx) {
  TValue *t;
  const TValue *slot;
  mask_lock(L);
  api_checknelems(L, 2);
  t = index2value(L, idx);
  if (maskV_fastget(L, t, s2v(L->top - 2), slot, maskH_get)) {
    maskV_finishfastset(L, t, slot, s2v(L->top - 1));
  }
  else
    maskV_finishset(L, t, s2v(L->top - 2), s2v(L->top - 1), slot);
  L->top -= 2;  /* pop index and value */
  mask_unlock(L);
}


MASK_API void mask_setfield (mask_State *L, int idx, const char *k) {
  mask_lock(L);  /* unlock done in 'auxsetstr' */
  auxsetstr(L, index2value(L, idx), k);
}


MASK_API void mask_seti (mask_State *L, int idx, mask_Integer n) {
  TValue *t;
  const TValue *slot;
  mask_lock(L);
  api_checknelems(L, 1);
  t = index2value(L, idx);
  if (ttistable(t)) {
    Table *tab = hvalue(t);
    if (tab->isfrozen) maskG_runerror(L, "attempt to modify frozen table.");
    tab->length = 0;
  } 
  if (maskV_fastgeti(L, t, n, slot)) {
    maskV_finishfastset(L, t, slot, s2v(L->top - 1));
  }
  else {
    TValue aux;
    setivalue(&aux, n);
    maskV_finishset(L, t, &aux, s2v(L->top - 1), slot);
  }
  L->top--;  /* pop value */
  mask_unlock(L);
}


static void aux_rawset (mask_State *L, int idx, TValue *key, int n) {
  Table *t;
  mask_lock(L);
  api_checknelems(L, n);
  t = gettable(L, idx);
  maskH_set(L, t, key, s2v(L->top - 1));
  t->length = 0; // Reset length cache.
  invalidateTMcache(t);
  maskC_barrierback(L, obj2gco(t), s2v(L->top - 1));
  L->top -= n;
  mask_unlock(L);
}


MASK_API void mask_rawset (mask_State *L, int idx) {
  aux_rawset(L, idx, s2v(L->top - 2), 2);
}


MASK_API void mask_rawsetp (mask_State *L, int idx, const void *p) {
  TValue k;
  setpvalue(&k, cast_voidp(p));
  aux_rawset(L, idx, &k, 1);
}


MASK_API void mask_rawseti (mask_State *L, int idx, mask_Integer n) {
  Table *t;
  mask_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  t->length = 0; // Reset length cache.
  maskH_setint(L, t, n, s2v(L->top - 1));
  maskC_barrierback(L, obj2gco(t), s2v(L->top - 1));
  L->top--;
  mask_unlock(L);
}


MASK_API void mask_setcachelen (mask_State *L, mask_Unsigned len, int idx) {
  Table *t;
  mask_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  t->length = len;
  mask_unlock(L);
}


MASK_API void mask_freezetable (mask_State *L, int idx) {
  Table *t;
  mask_lock(L);
  t = gettable(L, idx);
  if (t) {
    t->isfrozen = true;
    if (!t->length) t->length = maskH_getn(t); // May as well if modification is no longer permitted.
  }
  mask_unlock(L);
}


MASK_API int mask_istablefrozen (mask_State *L, int idx) {
  mask_lock(L);
  Table *t = gettable(L, idx);
  mask_unlock(L);
  return t ? t->isfrozen : false;
}


MASK_API void mask_erriffrozen (mask_State *L, int idx) {
  mask_lock(L);
  if (mask_istablefrozen(L, idx)) maskG_runerror(L, "attempt to modify frozen table.");
  mask_unlock(L);
}


MASK_API int mask_setmetatable (mask_State *L, int objindex) {
  TValue *obj;
  Table *mt;
  mask_lock(L);
  api_checknelems(L, 1);
  obj = index2value(L, objindex);
  if (ttisnil(s2v(L->top - 1)))
    mt = NULL;
  else {
    api_check(L, ttistable(s2v(L->top - 1)), "table expected");
    mt = hvalue(s2v(L->top - 1));
  }
  switch (ttype(obj)) {
    case MASK_TTABLE: {
      hvalue(obj)->metatable = mt;
      if (mt) {
        maskC_objbarrier(L, gcvalue(obj), mt);
        maskC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    case MASK_TUSERDATA: {
      uvalue(obj)->metatable = mt;
      if (mt) {
        maskC_objbarrier(L, uvalue(obj), mt);
        maskC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    default: {
      G(L)->mt[ttype(obj)] = mt;
      break;
    }
  }
  L->top--;
  mask_unlock(L);
  return 1;
}


MASK_API int mask_setiuservalue (mask_State *L, int idx, int n) {
  TValue *o;
  int res;
  mask_lock(L);
  api_checknelems(L, 1);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (!(cast_uint(n) - 1u < cast_uint(uvalue(o)->nuvalue)))
    res = 0;  /* 'n' not in [1, uvalue(o)->nuvalue] */
  else {
    setobj(L, &uvalue(o)->uv[n - 1].uv, s2v(L->top - 1));
    maskC_barrierback(L, gcvalue(o), s2v(L->top - 1));
    res = 1;
  }
  L->top--;
  mask_unlock(L);
  return res;
}


/*
** 'load' and 'call' functions (run Mask code)
*/


#define checkresults(L,na,nr) \
     api_check(L, (nr) == MASK_MULTRET || (L->ci->top - L->top >= (nr) - (na)), \
    "results from function overflow current stack size")


MASK_API void mask_callk (mask_State *L, int nargs, int nresults,
                        mask_KContext ctx, mask_KFunction k) {
  StkId func;
  mask_lock(L);
  api_check(L, k == NULL || !isMask(L->ci),
    "cannot use continuations inside hooks");
  api_checknelems(L, nargs+1);
  api_check(L, L->status == MASK_OK, "cannot do calls on non-normal thread");
  checkresults(L, nargs, nresults);
  func = L->top - (nargs+1);
  if (k != NULL && yieldable(L)) {  /* need to prepare continuation? */
    L->ci->u.c.k = k;  /* save continuation */
    L->ci->u.c.ctx = ctx;  /* save context */
    maskD_call(L, func, nresults);  /* do the call */
  }
  else  /* no continuation or no yieldable */
    maskD_callnoyield(L, func, nresults);  /* just do the call */
  adjustresults(L, nresults);
  mask_unlock(L);
}



/*
** Execute a protected call.
*/
struct CallS {  /* data to 'f_call' */
  StkId func;
  int nresults;
};


static void f_call (mask_State *L, void *ud) {
  struct CallS *c = cast(struct CallS *, ud);
  maskD_callnoyield(L, c->func, c->nresults);
}



MASK_API int mask_pcallk (mask_State *L, int nargs, int nresults, int errfunc,
                        mask_KContext ctx, mask_KFunction k) {
  struct CallS c;
  int status;
  ptrdiff_t func;
  mask_lock(L);
  api_check(L, k == NULL || !isMask(L->ci),
    "cannot use continuations inside hooks");
  api_checknelems(L, nargs+1);
  api_check(L, L->status == MASK_OK, "cannot do calls on non-normal thread");
  checkresults(L, nargs, nresults);
  if (errfunc == 0)
    func = 0;
  else {
    StkId o = index2stack(L, errfunc);
    api_check(L, ttisfunction(s2v(o)), "error handler must be a function");
    func = savestack(L, o);
  }
  c.func = L->top - (nargs+1);  /* function to be called */
  if (k == NULL || !yieldable(L)) {  /* no continuation or no yieldable? */
    c.nresults = nresults;  /* do a 'conventional' protected call */
    status = maskD_pcall(L, f_call, &c, savestack(L, c.func), func);
  }
  else {  /* prepare continuation (call is already protected by 'resume') */
    CallInfo *ci = L->ci;
    ci->u.c.k = k;  /* save continuation */
    ci->u.c.ctx = ctx;  /* save context */
    /* save information for error recovery */
    ci->u2.funcidx = cast_int(savestack(L, c.func));
    ci->u.c.old_errfunc = L->errfunc;
    L->errfunc = func;
    setoah(ci->callstatus, L->allowhook);  /* save value of 'allowhook' */
    ci->callstatus |= CIST_YPCALL;  /* function can do error recovery */
    maskD_call(L, c.func, nresults);  /* do the call */
    ci->callstatus &= ~CIST_YPCALL;
    L->errfunc = ci->u.c.old_errfunc;
    status = MASK_OK;  /* if it is here, there were no errors */
  }
  adjustresults(L, nresults);
  mask_unlock(L);
  return status;
}


MASK_API int mask_load (mask_State *L, mask_Reader reader, void *data,
                      const char *chunkname, const char *mode) {
  ZIO z;
  int status;
  mask_lock(L);
  if (!chunkname) chunkname = "?";
  maskZ_init(L, &z, reader, data);
  status = maskD_protectedparser(L, &z, chunkname, mode);
  if (status == MASK_OK) {  /* no errors? */
    LClosure *f = clLvalue(s2v(L->top - 1));  /* get newly created function */
    if (f->nupvalues >= 1) {  /* does it have an upvalue? */
      /* get global table from registry */
      const TValue *gt = getGtable(L);
      /* set global table as 1st upvalue of 'f' (may be MASK_ENV) */
      setobj(L, f->upvals[0]->v, gt);
      maskC_barrier(L, f->upvals[0], gt);
    }
  }
  mask_unlock(L);
  return status;
}


MASK_API int mask_dump (mask_State *L, mask_Writer writer, void *data, int strip) {
  int status;
  TValue *o;
  mask_lock(L);
  api_checknelems(L, 1);
  o = s2v(L->top - 1);
  if (isLfunction(o))
    status = maskU_dump(L, getproto(o), writer, data, strip);
  else
    status = 1;
  mask_unlock(L);
  return status;
}


MASK_API int mask_status (mask_State *L) {
  return L->status;
}


/*
** Garbage-collection function
*/
MASK_API int mask_gc (mask_State *L, int what, ...) {
  va_list argp;
  int res = 0;
  global_State *g = G(L);
  if (g->gcstp & GCSTPGC)  /* internal stop? */
    return -1;  /* all options are invalid when stopped */
  mask_lock(L);
  va_start(argp, what);
  switch (what) {
    case MASK_GCSTOP: {
      g->gcstp = GCSTPUSR;  /* stopped by the user */
      break;
    }
    case MASK_GCRESTART: {
      maskE_setdebt(g, 0);
      g->gcstp = 0;  /* (GCSTPGC must be already zero here) */
      break;
    }
    case MASK_GCCOLLECT: {
      maskC_fullgc(L, 0);
      break;
    }
    case MASK_GCCOUNT: {
      /* GC values are expressed in Kbytes: #bytes/2^10 */
      res = cast_int(gettotalbytes(g) >> 10);
      break;
    }
    case MASK_GCCOUNTB: {
      res = cast_int(gettotalbytes(g) & 0x3ff);
      break;
    }
    case MASK_GCSTEP: {
      int data = va_arg(argp, int);
      l_mem debt = 1;  /* =1 to signal that it did an actual step */
      lu_byte oldstp = g->gcstp;
      g->gcstp = 0;  /* allow GC to run (GCSTPGC must be zero here) */
      if (data == 0) {
        maskE_setdebt(g, 0);  /* do a basic step */
        maskC_step(L);
      }
      else {  /* add 'data' to total debt */
        debt = cast(l_mem, data) * 1024 + g->GCdebt;
        maskE_setdebt(g, debt);
        maskC_checkGC(L);
      }
      g->gcstp = oldstp;  /* restore previous state */
      if (debt > 0 && g->gcstate == GCSpause)  /* end of cycle? */
        res = 1;  /* signal it */
      break;
    }
    case MASK_GCSETPAUSE: {
      int data = va_arg(argp, int);
      res = getgcparam(g->gcpause);
      setgcparam(g->gcpause, data);
      break;
    }
    case MASK_GCSETSTEPMUL: {
      int data = va_arg(argp, int);
      res = getgcparam(g->gcstepmul);
      setgcparam(g->gcstepmul, data);
      break;
    }
    case MASK_GCISRUNNING: {
      res = gcrunning(g);
      break;
    }
    case MASK_GCGEN: {
      int minormul = va_arg(argp, int);
      int majormul = va_arg(argp, int);
      res = isdecGCmodegen(g) ? MASK_GCGEN : MASK_GCINC;
      if (minormul != 0)
        g->genminormul = minormul;
      if (majormul != 0)
        setgcparam(g->genmajormul, majormul);
      maskC_changemode(L, KGC_GEN);
      break;
    }
    case MASK_GCINC: {
      int pause = va_arg(argp, int);
      int stepmul = va_arg(argp, int);
      int stepsize = va_arg(argp, int);
      res = isdecGCmodegen(g) ? MASK_GCGEN : MASK_GCINC;
      if (pause != 0)
        setgcparam(g->gcpause, pause);
      if (stepmul != 0)
        setgcparam(g->gcstepmul, stepmul);
      if (stepsize != 0)
        g->gcstepsize = stepsize;
      maskC_changemode(L, KGC_INC);
      break;
    }
    default: res = -1;  /* invalid option */
  }
  va_end(argp);
  mask_unlock(L);
  return res;
}



/*
** miscellaneous functions
*/


MASK_API void mask_error (mask_State *L) {
  TValue *errobj;
  mask_lock(L);
  errobj = s2v(L->top - 1);
  api_checknelems(L, 1);
  /* error object is the memory error message? */
  if (ttisshrstring(errobj) && eqshrstr(tsvalue(errobj), G(L)->memerrmsg))
    maskM_error(L);  /* raise a memory error */
  else
    maskG_errormsg(L);  /* raise a regular error */
}


MASK_API int mask_next (mask_State *L, int idx) {
  Table *t;
  int more;
  mask_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  more = maskH_next(L, t, L->top - 1);
  if (more) {
    api_incr_top(L);
  }
  else  /* no more elements */
    L->top -= 1;  /* remove key */
  mask_unlock(L);
  return more;
}


MASK_API void mask_toclose (mask_State *L, int idx) {
  int nresults;
  StkId o;
  mask_lock(L);
  o = index2stack(L, idx);
  nresults = L->ci->nresults;
  api_check(L, L->tbclist < o, "given index below or equal a marked one");
  maskF_newtbcupval(L, o);  /* create new to-be-closed upvalue */
  if (!hastocloseCfunc(nresults))  /* function not marked yet? */
    L->ci->nresults = codeNresults(nresults);  /* mark it */
  mask_assert(hastocloseCfunc(L->ci->nresults));
  mask_unlock(L);
}


MASK_API void mask_concat (mask_State *L, int n) {
  mask_lock(L);
  api_checknelems(L, n);
  if (n > 0)
    maskV_concat(L, n);
  else {  /* nothing to concatenate */
    setsvalue2s(L, L->top, maskS_newlstr(L, "", 0));  /* push empty string */
    api_incr_top(L);
  }
  maskC_checkGC(L);
  mask_unlock(L);
}


MASK_API void mask_len (mask_State *L, int idx) {
  TValue *t;
  mask_lock(L);
  t = index2value(L, idx);
  maskV_objlen(L, L->top, t);
  api_incr_top(L);
  mask_unlock(L);
}


MASK_API mask_Alloc mask_getallocf (mask_State *L, void **ud) {
  mask_Alloc f;
  mask_lock(L);
  if (ud) *ud = G(L)->ud;
  f = G(L)->frealloc;
  mask_unlock(L);
  return f;
}


MASK_API void mask_setallocf (mask_State *L, mask_Alloc f, void *ud) {
  mask_lock(L);
  G(L)->ud = ud;
  G(L)->frealloc = f;
  mask_unlock(L);
}


void mask_setwarnf (mask_State *L, mask_WarnFunction f, void *ud) {
  mask_lock(L);
  G(L)->ud_warn = ud;
  G(L)->warnf = f;
  mask_unlock(L);
}


void mask_warning (mask_State *L, const char *msg, int tocont) {
  mask_lock(L);
  maskE_warning(L, msg, tocont);
  mask_unlock(L);
}



MASK_API void *mask_newuserdatauv (mask_State *L, size_t size, int nuvalue) {
  Udata *u;
  mask_lock(L);
  api_check(L, 0 <= nuvalue && nuvalue < USHRT_MAX, "invalid value");
  u = maskS_newudata(L, size, nuvalue);
  setuvalue(L, s2v(L->top), u);
  api_incr_top(L);
  maskC_checkGC(L);
  mask_unlock(L);
  return getudatamem(u);
}



static const char *aux_upvalue (TValue *fi, int n, TValue **val,
                                GCObject **owner) {
  switch (ttypetag(fi)) {
    case MASK_VCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      if (!(cast_uint(n) - 1u < cast_uint(f->nupvalues)))
        return NULL;  /* 'n' not in [1, f->nupvalues] */
      *val = &f->upvalue[n-1];
      if (owner) *owner = obj2gco(f);
      return "";
    }
    case MASK_VLCL: {  /* Mask closure */
      LClosure *f = clLvalue(fi);
      TString *name;
      Proto *p = f->p;
      if (!(cast_uint(n) - 1u  < cast_uint(p->sizeupvalues)))
        return NULL;  /* 'n' not in [1, p->sizeupvalues] */
      *val = f->upvals[n-1]->v;
      if (owner) *owner = obj2gco(f->upvals[n - 1]);
      name = p->upvalues[n-1].name;
      return (name == NULL) ? "(no name)" : getstr(name);
    }
    default: return NULL;  /* not a closure */
  }
}


MASK_API const char *mask_getupvalue (mask_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  mask_lock(L);
  name = aux_upvalue(index2value(L, funcindex), n, &val, NULL);
  if (name) {
    setobj2s(L, L->top, val);
    api_incr_top(L);
  }
  mask_unlock(L);
  return name;
}


MASK_API const char *mask_setupvalue (mask_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  GCObject *owner = NULL;  /* to avoid warnings */
  TValue *fi;
  mask_lock(L);
  fi = index2value(L, funcindex);
  api_checknelems(L, 1);
  name = aux_upvalue(fi, n, &val, &owner);
  if (name) {
    L->top--;
    setobj(L, val, s2v(L->top));
    maskC_barrier(L, owner, val);
  }
  mask_unlock(L);
  return name;
}


static UpVal **getupvalref (mask_State *L, int fidx, int n, LClosure **pf) {
  static const UpVal *const nullup = NULL;
  LClosure *f;
  TValue *fi = index2value(L, fidx);
  api_check(L, ttisLclosure(fi), "Mask function expected");
  f = clLvalue(fi);
  if (pf) *pf = f;
  if (1 <= n && n <= f->p->sizeupvalues)
    return &f->upvals[n - 1];  /* get its upvalue pointer */
  else
    return (UpVal**)&nullup;
}


MASK_API void *mask_upvalueid (mask_State *L, int fidx, int n) {
  TValue *fi = index2value(L, fidx);
  switch (ttypetag(fi)) {
    case MASK_VLCL: {  /* mask closure */
      return *getupvalref(L, fidx, n, NULL);
    }
    case MASK_VCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      if (1 <= n && n <= f->nupvalues)
        return &f->upvalue[n - 1];
      /* else */
    }  /* FALLTHROUGH */
    case MASK_VLCF:
      return NULL;  /* light C functions have no upvalues */
    default: {
      api_check(L, 0, "function expected");
      return NULL;
    }
  }
}


MASK_API void mask_upvaluejoin (mask_State *L, int fidx1, int n1,
                                            int fidx2, int n2) {
  LClosure *f1;
  UpVal **up1 = getupvalref(L, fidx1, n1, &f1);
  UpVal **up2 = getupvalref(L, fidx2, n2, NULL);
  api_check(L, *up1 != NULL && *up2 != NULL, "invalid upvalue index");
  *up1 = *up2;
  maskC_objbarrier(L, f1, *up1);
}


