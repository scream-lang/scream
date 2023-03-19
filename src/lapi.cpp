/*
** $Id: lapi.c $
** Hello API
** See Copyright Notice in hello.h
*/

#define lapi_c
#define HELLO_CORE

#include "lprefix.h"


#include <limits.h>
#include <stdarg.h>
#include <string.h>

#include "lprefix.h"
#include "hello.h"

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



const char hello_ident[] =
  "$HelloVersion: " HELLO_COPYRIGHT " $"
  "$HelloAuthors: " HELLO_AUTHORS " $";



/*
** Test for a valid index (one that is not the 'nilvalue').
** '!ttisnil(o)' implies 'o != &G(L)->nilvalue', so it is not needed.
** However, it covers the most common cases in a faster way.
*/
#define isvalid(L, o)	(!ttisnil(o) || o != &G(L)->nilvalue)


/* test for pseudo index */
#define ispseudo(i)		((i) <= HELLO_REGISTRYINDEX)

/* test for upvalue */
#define isupvalue(i)		((i) < HELLO_REGISTRYINDEX)


/*
** Convert an acceptable index to a pointer to its respective value.
** Non-valid indices return the special nil value 'G(L)->nilvalue'.
*/
static TValue *index2value (hello_State *L, int idx) {
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
  else if (idx == HELLO_REGISTRYINDEX)
    return &G(L)->l_registry;
  else {  /* upvalues */
    idx = HELLO_REGISTRYINDEX - idx;
    api_check(L, idx <= MAXUPVAL + 1, "upvalue index too large");
    if (ttisCclosure(s2v(ci->func))) {  /* C closure? */
      CClosure *func = clCvalue(s2v(ci->func));
      return (idx <= func->nupvalues) ? &func->upvalue[idx-1]
                                      : &G(L)->nilvalue;
    }
    else {  /* light C function or Hello function (through a hook)?) */
      api_check(L, ttislcf(s2v(ci->func)), "caller not a C function");
      return &G(L)->nilvalue;  /* no upvalues */
    }
  }
}



/*
** Convert a valid actual index (not a pseudo-index) to its address.
*/
l_sinline StkId index2stack (hello_State *L, int idx) {
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


HELLO_API int hello_checkstack (hello_State *L, int n) {
  int res;
  CallInfo *ci;
  hello_lock(L);
  ci = L->ci;
  api_check(L, n >= 0, "negative 'n'");
  if (L->stack_last - L->top > n)  /* stack large enough? */
    res = 1;  /* yes; check is OK */
  else  /* need to grow stack */
    res = helloD_growstack(L, n, 0);
  if (res && ci->top < L->top + n)
    ci->top = L->top + n;  /* adjust frame top */
  hello_unlock(L);
  return res;
}


HELLO_API void hello_xmove (hello_State *from, hello_State *to, int n) {
  int i;
  if (from == to) return;
  hello_lock(to);
  api_checknelems(from, n);
  api_check(from, G(from) == G(to), "moving among independent states");
  api_check(from, to->ci->top - to->top >= n, "stack overflow");
  from->top -= n;
  for (i = 0; i < n; i++) {
    setobjs2s(to, to->top, from->top + i);
    to->top++;  /* stack already checked by previous 'api_check' */
  }
  hello_unlock(to);
}


HELLO_API hello_CFunction hello_atpanic (hello_State *L, hello_CFunction panicf) {
  hello_CFunction old;
  hello_lock(L);
  old = G(L)->panic;
  G(L)->panic = panicf;
  hello_unlock(L);
  return old;
}


HELLO_API hello_Number hello_version (hello_State *L) {
  UNUSED(L);
  return HELLO_VERSION_NUM;
}



/*
** basic stack manipulation
*/


/*
** convert an acceptable stack index into an absolute index
*/
HELLO_API int hello_absindex (hello_State *L, int idx) {
  return (idx > 0 || ispseudo(idx))
         ? idx
         : cast_int(L->top - L->ci->func) + idx;
}


HELLO_API int hello_gettop (hello_State *L) {
  return cast_int(L->top - (L->ci->func + 1));
}


HELLO_API void hello_settop (hello_State *L, int idx) {
  CallInfo *ci;
  StkId func, newtop;
  ptrdiff_t diff;  /* difference for new top */
  hello_lock(L);
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
    hello_assert(hastocloseCfunc(ci->nresults));
    newtop = helloF_close(L, newtop, CLOSEKTOP, 0);
  }
  L->top = newtop;  /* correct top only after closing any upvalue */
  hello_unlock(L);
}


HELLO_API void hello_closeslot (hello_State *L, int idx) {
  StkId level;
  hello_lock(L);
  level = index2stack(L, idx);
  api_check(L, hastocloseCfunc(L->ci->nresults) && L->tbclist == level,
     "no variable to close at given level");
  level = helloF_close(L, level, CLOSEKTOP, 0);
  setnilvalue(s2v(level));
  hello_unlock(L);
}


/*
** Reverse the stack segment from 'from' to 'to'
** (auxiliary to 'hello_rotate')
** Note that we move(copy) only the value inside the stack.
** (We do not move additional fields that may exist.)
*/
l_sinline void reverse (hello_State *L, StkId from, StkId to) {
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
HELLO_API void hello_rotate (hello_State *L, int idx, int n) {
  StkId p, t, m;
  hello_lock(L);
  t = L->top - 1;  /* end of stack segment being rotated */
  p = index2stack(L, idx);  /* start of segment */
  api_check(L, (n >= 0 ? n : -n) <= (t - p + 1), "invalid 'n'");
  m = (n >= 0 ? t - n : p - n - 1);  /* end of prefix */
  reverse(L, p, m);  /* reverse the prefix with length 'n' */
  reverse(L, m + 1, t);  /* reverse the suffix */
  reverse(L, p, t);  /* reverse the entire segment */
  hello_unlock(L);
}


HELLO_API void hello_copy (hello_State *L, int fromidx, int toidx) {
  TValue *fr, *to;
  hello_lock(L);
  fr = index2value(L, fromidx);
  to = index2value(L, toidx);
  api_check(L, isvalid(L, to), "invalid index");
  setobj(L, to, fr);
  if (isupvalue(toidx))  /* function upvalue? */
    helloC_barrier(L, clCvalue(s2v(L->ci->func)), fr);
  /* HELLO_REGISTRYINDEX does not need gc barrier
     (collector revisits it before finishing collection) */
  hello_unlock(L);
}


HELLO_API void hello_pushvalue (hello_State *L, int idx) {
  hello_lock(L);
  setobj2s(L, L->top, index2value(L, idx));
  api_incr_top(L);
  hello_unlock(L);
}



/*
** access functions (stack -> C)
*/


HELLO_API int hello_type (hello_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (isvalid(L, o) ? ttype(o) : HELLO_TNONE);
}


HELLO_API const char *hello_typename (hello_State *L, int t) {
  UNUSED(L);
  api_check(L, HELLO_TNONE <= t && t < HELLO_NUMTYPES, "invalid type");
  return ttypename(t);
}


HELLO_API int hello_iscfunction (hello_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttislcf(o) || (ttisCclosure(o)));
}


HELLO_API int hello_isinteger (hello_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return ttisinteger(o);
}


HELLO_API int hello_isnumber (hello_State *L, int idx) {
  hello_Number n;
  const TValue *o = index2value(L, idx);
  return tonumber(o, &n);
}


HELLO_API int hello_isstring (hello_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttisstring(o) || cvt2str(o));
}


HELLO_API int hello_isuserdata (hello_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (ttisfulluserdata(o) || ttislightuserdata(o));
}


HELLO_API int hello_rawequal (hello_State *L, int index1, int index2) {
  const TValue *o1 = index2value(L, index1);
  const TValue *o2 = index2value(L, index2);
  return (isvalid(L, o1) && isvalid(L, o2)) ? helloV_rawequalobj(o1, o2) : 0;
}


HELLO_API void hello_arith (hello_State *L, int op) {
  hello_lock(L);
  if (op != HELLO_OPUNM && op != HELLO_OPBNOT)
    api_checknelems(L, 2);  /* all other operations expect two operands */
  else {  /* for unary operations, add fake 2nd operand */
    api_checknelems(L, 1);
    setobjs2s(L, L->top, L->top - 1);
    api_incr_top(L);
  }
  /* first operand at top - 2, second at top - 1; result go to top - 2 */
  helloO_arith(L, op, s2v(L->top - 2), s2v(L->top - 1), L->top - 2);
  L->top--;  /* remove second operand */
  hello_unlock(L);
}


HELLO_API int hello_compare (hello_State *L, int index1, int index2, int op) {
  const TValue *o1;
  const TValue *o2;
  int i = 0;
  hello_lock(L);  /* may call tag method */
  o1 = index2value(L, index1);
  o2 = index2value(L, index2);
  if (isvalid(L, o1) && isvalid(L, o2)) {
    switch (op) {
      case HELLO_OPEQ: i = helloV_equalobj(L, o1, o2); break;
      case HELLO_OPLT: i = helloV_lessthan(L, o1, o2); break;
      case HELLO_OPLE: i = helloV_lessequal(L, o1, o2); break;
      default: api_check(L, 0, "invalid option");
    }
  }
  hello_unlock(L);
  return i;
}


HELLO_API size_t hello_stringtonumber (hello_State *L, const char *s) {
  size_t sz = helloO_str2num(s, s2v(L->top));
  if (sz != 0)
    api_incr_top(L);
  return sz;
}


HELLO_API hello_Number hello_tonumberx (hello_State *L, int idx, int *pisnum) {
  hello_Number n = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tonumber(o, &n);
  if (pisnum)
    *pisnum = isnum;
  return n;
}


HELLO_API hello_Integer hello_tointegerx (hello_State *L, int idx, int *pisnum) {
  hello_Integer res = 0;
  const TValue *o = index2value(L, idx);
  int isnum = tointeger(o, &res);
  if (pisnum)
    *pisnum = isnum;
  return res;
}


HELLO_API int hello_toboolean (hello_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return !l_isfalse(o);
}


HELLO_API int hello_istrue(hello_State *L, int idx) noexcept
{
  const TValue *o = index2value(L, idx);
  return ttistrue(o);
}


HELLO_API const char *hello_tolstring (hello_State *L, int idx, size_t *len) {
  TValue *o;
  hello_lock(L);
  o = index2value(L, idx);
  if (!ttisstring(o)) {
    if (!cvt2str(o)) {  /* not convertible? */
      if (len != NULL) *len = 0;
      hello_unlock(L);
      return NULL;
    }
    helloO_tostring(L, o);
    helloC_checkGC(L);
    o = index2value(L, idx);  /* previous call may reallocate the stack */
  }
  if (len != NULL)
    *len = vslen(o);
  hello_unlock(L);
  return svalue(o);
}


HELLO_API hello_Unsigned hello_rawlen (hello_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o)) {
    case HELLO_VSHRSTR: return tsvalue(o)->shrlen;
    case HELLO_VLNGSTR: return tsvalue(o)->u.lnglen;
    case HELLO_VUSERDATA: return uvalue(o)->len;
    case HELLO_VTABLE: return helloH_getn(hvalue(o));
    default: return 0;
  }
}


HELLO_API hello_CFunction hello_tocfunction (hello_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  if (ttislcf(o)) return fvalue(o);
  else if (ttisCclosure(o))
    return clCvalue(o)->f;
  else return NULL;  /* not a C function */
}


l_sinline void *touserdata (const TValue *o) {
  switch (ttype(o)) {
    case HELLO_TUSERDATA: return getudatamem(uvalue(o));
    case HELLO_TLIGHTUSERDATA: return pvalue(o);
    default: return NULL;
  }
}


HELLO_API void *hello_touserdata (hello_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return touserdata(o);
}


HELLO_API hello_State *hello_tothread (hello_State *L, int idx) {
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
HELLO_API const void *hello_topointer (hello_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (ttypetag(o)) {
    case HELLO_VLCF: return cast_voidp(cast_sizet(fvalue(o)));
    case HELLO_VUSERDATA: case HELLO_VLIGHTUSERDATA:
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


HELLO_API void hello_pushnil (hello_State *L) {
  hello_lock(L);
  setnilvalue(s2v(L->top));
  api_incr_top(L);
  hello_unlock(L);
}


HELLO_API void hello_pushnumber (hello_State *L, hello_Number n) {
  hello_lock(L);
  setfltvalue(s2v(L->top), n);
  api_incr_top(L);
  hello_unlock(L);
}


HELLO_API void hello_pushinteger (hello_State *L, hello_Integer n) {
  hello_lock(L);
  setivalue(s2v(L->top), n);
  api_incr_top(L);
  hello_unlock(L);
}


/*
** Pushes on the stack a string with given length. Avoid using 's' when
** 'len' == 0 (as 's' can be NULL in that case), due to later use of
** 'memcmp' and 'memcpy'.
*/
HELLO_API const char *hello_pushlstring (hello_State *L, const char *s, size_t len) {
  TString *ts;
  hello_lock(L);
  ts = (len == 0) ? helloS_new(L, "") : helloS_newlstr(L, s, len);
  setsvalue2s(L, L->top, ts);
  api_incr_top(L);
  helloC_checkGC(L);
  hello_unlock(L);
  return getstr(ts);
}


HELLO_API const char *hello_pushstring (hello_State *L, const char *s) {
  hello_lock(L);
  if (s == NULL)
    setnilvalue(s2v(L->top));
  else {
    TString *ts;
    ts = helloS_new(L, s);
    setsvalue2s(L, L->top, ts);
    s = getstr(ts);  /* internal copy's address */
  }
  api_incr_top(L);
  helloC_checkGC(L);
  hello_unlock(L);
  return s;
}

HELLO_API const char* hello_pushstring(hello_State* L, const std::string& str) {
  return hello_pushstring(L, str.c_str());
}


HELLO_API const char *hello_pushvfstring (hello_State *L, const char *fmt,
                                      va_list argp) {
  const char *ret;
  hello_lock(L);
  ret = helloO_pushvfstring(L, fmt, argp);
  helloC_checkGC(L);
  hello_unlock(L);
  return ret;
}


HELLO_API const char *hello_pushfstring (hello_State *L, const char *fmt, ...) {
  const char *ret;
  va_list argp;
  hello_lock(L);
  va_start(argp, fmt);
  ret = helloO_pushvfstring(L, fmt, argp);
  va_end(argp);
  helloC_checkGC(L);
  hello_unlock(L);
  return ret;
}


HELLO_API void hello_pushcclosure (hello_State *L, hello_CFunction fn, int n) {
  hello_lock(L);
  if (n == 0) {
    setfvalue(s2v(L->top), fn);
    api_incr_top(L);
  }
  else {
    CClosure *cl;
    api_checknelems(L, n);
    api_check(L, n <= MAXUPVAL, "upvalue index too large");
    cl = helloF_newCclosure(L, n);
    cl->f = fn;
    L->top -= n;
    while (n--) {
      setobj2n(L, &cl->upvalue[n], s2v(L->top + n));
      /* does not need barrier because closure is white */
      hello_assert(iswhite(cl));
    }
    setclCvalue(L, s2v(L->top), cl);
    api_incr_top(L);
    helloC_checkGC(L);
  }
  hello_unlock(L);
}


HELLO_API void hello_pushboolean (hello_State *L, int b) {
  hello_lock(L);
  if (b)
    setbtvalue(s2v(L->top));
  else
    setbfvalue(s2v(L->top));
  api_incr_top(L);
  hello_unlock(L);
}


HELLO_API void hello_pushlightuserdata (hello_State *L, void *p) {
  hello_lock(L);
  setpvalue(s2v(L->top), p);
  api_incr_top(L);
  hello_unlock(L);
}


HELLO_API int hello_pushthread (hello_State *L) {
  hello_lock(L);
  setthvalue(L, s2v(L->top), L);
  api_incr_top(L);
  hello_unlock(L);
  return (G(L)->mainthread == L);
}



/*
** get functions (Hello -> stack)
*/


l_sinline int auxgetstr (hello_State *L, const TValue *t, const char *k) {
  const TValue *slot;
  TString *str = helloS_new(L, k);
  if (helloV_fastget(L, t, str, slot, helloH_getstr)) {
    setobj2s(L, L->top, slot);
    api_incr_top(L);
  }
  else {
    setsvalue2s(L, L->top, str);
    api_incr_top(L);
    helloV_finishget(L, t, s2v(L->top - 1), L->top - 1, slot);
  }
  hello_unlock(L);
  return ttype(s2v(L->top - 1));
}


/*
** Get the global table in the registry. Since all predefined
** indices in the registry were inserted right when the registry
** was created and never removed, they must always be in the array
** part of the registry.
*/
#define getGtable(L)  \
    (&hvalue(&G(L)->l_registry)->array[HELLO_RIDX_GLOBALS - 1])


HELLO_API int hello_getglobal (hello_State *L, const char *name) {
  const TValue *G;
  hello_lock(L);
  G = getGtable(L);
  return auxgetstr(L, G, name);
}


HELLO_API int hello_gettable (hello_State *L, int idx) {
  const TValue *slot;
  TValue *t;
  hello_lock(L);
  t = index2value(L, idx);
  if (helloV_fastget(L, t, s2v(L->top - 1), slot, helloH_get)) {
    setobj2s(L, L->top - 1, slot);
  }
  else
    helloV_finishget(L, t, s2v(L->top - 1), L->top - 1, slot);
  hello_unlock(L);
  return ttype(s2v(L->top - 1));
}


HELLO_API int hello_getfield (hello_State *L, int idx, const char *k) {
  hello_lock(L);
  return auxgetstr(L, index2value(L, idx), k);
}


HELLO_API int hello_geti (hello_State *L, int idx, hello_Integer n) {
  TValue *t;
  const TValue *slot;
  hello_lock(L);
  t = index2value(L, idx);
  if (helloV_fastgeti(L, t, n, slot)) {
    setobj2s(L, L->top, slot);
  }
  else {
    TValue aux;
    setivalue(&aux, n);
    helloV_finishget(L, t, &aux, L->top, slot);
  }
  api_incr_top(L);
  hello_unlock(L);
  return ttype(s2v(L->top - 1));
}


l_sinline int finishrawget (hello_State *L, const TValue *val) {
  if (isempty(val))  /* avoid copying empty items to the stack */
    setnilvalue(s2v(L->top));
  else
    setobj2s(L, L->top, val);
  api_incr_top(L);
  hello_unlock(L);
  return ttype(s2v(L->top - 1));
}


static Table *gettable (hello_State *L, int idx) {
  TValue *t = index2value(L, idx);
  api_check(L, ttistable(t), "table expected");
  return hvalue(t);
}


HELLO_API int hello_rawget (hello_State *L, int idx) {
  Table *t;
  const TValue *val;
  hello_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  val = helloH_get(t, s2v(L->top - 1));
  L->top--;  /* remove key */
  return finishrawget(L, val);
}


HELLO_API int hello_rawgeti (hello_State *L, int idx, hello_Integer n) {
  Table *t;
  hello_lock(L);
  t = gettable(L, idx);
  return finishrawget(L, helloH_getint(t, n));
}


HELLO_API int hello_rawgetp (hello_State *L, int idx, const void *p) {
  Table *t;
  TValue k;
  hello_lock(L);
  t = gettable(L, idx);
  setpvalue(&k, cast_voidp(p));
  return finishrawget(L, helloH_get(t, &k));
}


HELLO_API void hello_createtable (hello_State *L, int narray, int nrec) {
  Table *t;
  hello_lock(L);
  t = helloH_new(L);
  sethvalue2s(L, L->top, t);
  api_incr_top(L);
  if (narray > 0 || nrec > 0)
    helloH_resize(L, t, narray, nrec);
  helloC_checkGC(L);
  hello_unlock(L);
}


HELLO_API int hello_getmetatable (hello_State *L, int objindex) {
  const TValue *obj;
  Table *mt;
  int res = 0;
  hello_lock(L);
  obj = index2value(L, objindex);
  switch (ttype(obj)) {
    case HELLO_TTABLE:
      mt = hvalue(obj)->metatable;
      break;
    case HELLO_TUSERDATA:
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
  hello_unlock(L);
  return res;
}


HELLO_API int hello_getiuservalue (hello_State *L, int idx, int n) {
  TValue *o;
  int t;
  hello_lock(L);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (n <= 0 || n > uvalue(o)->nuvalue) {
    setnilvalue(s2v(L->top));
    t = HELLO_TNONE;
  }
  else {
    setobj2s(L, L->top, &uvalue(o)->uv[n - 1].uv);
    t = ttype(s2v(L->top));
  }
  api_incr_top(L);
  hello_unlock(L);
  return t;
}


/*
** set functions (stack -> Hello)
*/

/*
** t[k] = value at the top of the stack (where 'k' is a string)
*/
static void auxsetstr (hello_State *L, const TValue *t, const char *k) {
  const TValue *slot;
  TString *str = helloS_new(L, k);
  api_checknelems(L, 1);
  if (helloV_fastget(L, t, str, slot, helloH_getstr)) {
    helloV_finishfastset(L, t, slot, s2v(L->top - 1));
    L->top--;  /* pop value */
  }
  else {
    setsvalue2s(L, L->top, str);  /* push 'str' (to make it a TValue) */
    api_incr_top(L);
    helloV_finishset(L, t, s2v(L->top - 1), s2v(L->top - 2), slot);
    L->top -= 2;  /* pop value and key */
  }
  hello_unlock(L);  /* lock done by caller */
}


HELLO_API void hello_setglobal (hello_State *L, const char *name) {
  const TValue *G;
  hello_lock(L);  /* unlock done in 'auxsetstr' */
  G = getGtable(L);
  auxsetstr(L, G, name);
}


HELLO_API void hello_settable (hello_State *L, int idx) {
  TValue *t;
  const TValue *slot;
  hello_lock(L);
  api_checknelems(L, 2);
  t = index2value(L, idx);
  if (helloV_fastget(L, t, s2v(L->top - 2), slot, helloH_get)) {
    helloV_finishfastset(L, t, slot, s2v(L->top - 1));
  }
  else
    helloV_finishset(L, t, s2v(L->top - 2), s2v(L->top - 1), slot);
  L->top -= 2;  /* pop index and value */
  hello_unlock(L);
}


HELLO_API void hello_setfield (hello_State *L, int idx, const char *k) {
  hello_lock(L);  /* unlock done in 'auxsetstr' */
  auxsetstr(L, index2value(L, idx), k);
}


HELLO_API void hello_seti (hello_State *L, int idx, hello_Integer n) {
  TValue *t;
  const TValue *slot;
  hello_lock(L);
  api_checknelems(L, 1);
  t = index2value(L, idx);
  if (ttistable(t)) {
    Table *tab = hvalue(t);
    if (tab->isfrozen) helloG_runerror(L, "attempt to modify frozen table.");
    tab->length = 0;
  } 
  if (helloV_fastgeti(L, t, n, slot)) {
    helloV_finishfastset(L, t, slot, s2v(L->top - 1));
  }
  else {
    TValue aux;
    setivalue(&aux, n);
    helloV_finishset(L, t, &aux, s2v(L->top - 1), slot);
  }
  L->top--;  /* pop value */
  hello_unlock(L);
}


static void aux_rawset (hello_State *L, int idx, TValue *key, int n) {
  Table *t;
  hello_lock(L);
  api_checknelems(L, n);
  t = gettable(L, idx);
  helloH_set(L, t, key, s2v(L->top - 1));
  t->length = 0; // Reset length cache.
  invalidateTMcache(t);
  helloC_barrierback(L, obj2gco(t), s2v(L->top - 1));
  L->top -= n;
  hello_unlock(L);
}


HELLO_API void hello_rawset (hello_State *L, int idx) {
  aux_rawset(L, idx, s2v(L->top - 2), 2);
}


HELLO_API void hello_rawsetp (hello_State *L, int idx, const void *p) {
  TValue k;
  setpvalue(&k, cast_voidp(p));
  aux_rawset(L, idx, &k, 1);
}


HELLO_API void hello_rawseti (hello_State *L, int idx, hello_Integer n) {
  Table *t;
  hello_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  t->length = 0; // Reset length cache.
  helloH_setint(L, t, n, s2v(L->top - 1));
  helloC_barrierback(L, obj2gco(t), s2v(L->top - 1));
  L->top--;
  hello_unlock(L);
}


HELLO_API void hello_setcachelen (hello_State *L, hello_Unsigned len, int idx) {
  Table *t;
  hello_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  t->length = len;
  hello_unlock(L);
}


HELLO_API void hello_freezetable (hello_State *L, int idx) {
  Table *t;
  hello_lock(L);
  t = gettable(L, idx);
  if (t) {
    t->isfrozen = true;
    if (!t->length) t->length = helloH_getn(t); // May as well if modification is no longer permitted.
  }
  hello_unlock(L);
}


HELLO_API int hello_istablefrozen (hello_State *L, int idx) {
  hello_lock(L);
  Table *t = gettable(L, idx);
  hello_unlock(L);
  return t ? t->isfrozen : false;
}


HELLO_API void hello_erriffrozen (hello_State *L, int idx) {
  hello_lock(L);
  if (hello_istablefrozen(L, idx)) helloG_runerror(L, "attempt to modify frozen table.");
  hello_unlock(L);
}


HELLO_API int hello_setmetatable (hello_State *L, int objindex) {
  TValue *obj;
  Table *mt;
  hello_lock(L);
  api_checknelems(L, 1);
  obj = index2value(L, objindex);
  if (ttisnil(s2v(L->top - 1)))
    mt = NULL;
  else {
    api_check(L, ttistable(s2v(L->top - 1)), "table expected");
    mt = hvalue(s2v(L->top - 1));
  }
  switch (ttype(obj)) {
    case HELLO_TTABLE: {
      hvalue(obj)->metatable = mt;
      if (mt) {
        helloC_objbarrier(L, gcvalue(obj), mt);
        helloC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    case HELLO_TUSERDATA: {
      uvalue(obj)->metatable = mt;
      if (mt) {
        helloC_objbarrier(L, uvalue(obj), mt);
        helloC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    default: {
      G(L)->mt[ttype(obj)] = mt;
      break;
    }
  }
  L->top--;
  hello_unlock(L);
  return 1;
}


HELLO_API int hello_setiuservalue (hello_State *L, int idx, int n) {
  TValue *o;
  int res;
  hello_lock(L);
  api_checknelems(L, 1);
  o = index2value(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  if (!(cast_uint(n) - 1u < cast_uint(uvalue(o)->nuvalue)))
    res = 0;  /* 'n' not in [1, uvalue(o)->nuvalue] */
  else {
    setobj(L, &uvalue(o)->uv[n - 1].uv, s2v(L->top - 1));
    helloC_barrierback(L, gcvalue(o), s2v(L->top - 1));
    res = 1;
  }
  L->top--;
  hello_unlock(L);
  return res;
}


/*
** 'load' and 'call' functions (run Hello code)
*/


#define checkresults(L,na,nr) \
     api_check(L, (nr) == HELLO_MULTRET || (L->ci->top - L->top >= (nr) - (na)), \
    "results from function overflow current stack size")


HELLO_API void hello_callk (hello_State *L, int nargs, int nresults,
                        hello_KContext ctx, hello_KFunction k) {
  StkId func;
  hello_lock(L);
  api_check(L, k == NULL || !isHello(L->ci),
    "cannot use continuations inside hooks");
  api_checknelems(L, nargs+1);
  api_check(L, L->status == HELLO_OK, "cannot do calls on non-normal thread");
  checkresults(L, nargs, nresults);
  func = L->top - (nargs+1);
  if (k != NULL && yieldable(L)) {  /* need to prepare continuation? */
    L->ci->u.c.k = k;  /* save continuation */
    L->ci->u.c.ctx = ctx;  /* save context */
    helloD_call(L, func, nresults);  /* do the call */
  }
  else  /* no continuation or no yieldable */
    helloD_callnoyield(L, func, nresults);  /* just do the call */
  adjustresults(L, nresults);
  hello_unlock(L);
}



/*
** Execute a protected call.
*/
struct CallS {  /* data to 'f_call' */
  StkId func;
  int nresults;
};


static void f_call (hello_State *L, void *ud) {
  struct CallS *c = cast(struct CallS *, ud);
  helloD_callnoyield(L, c->func, c->nresults);
}



HELLO_API int hello_pcallk (hello_State *L, int nargs, int nresults, int errfunc,
                        hello_KContext ctx, hello_KFunction k) {
  struct CallS c;
  int status;
  ptrdiff_t func;
  hello_lock(L);
  api_check(L, k == NULL || !isHello(L->ci),
    "cannot use continuations inside hooks");
  api_checknelems(L, nargs+1);
  api_check(L, L->status == HELLO_OK, "cannot do calls on non-normal thread");
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
    status = helloD_pcall(L, f_call, &c, savestack(L, c.func), func);
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
    helloD_call(L, c.func, nresults);  /* do the call */
    ci->callstatus &= ~CIST_YPCALL;
    L->errfunc = ci->u.c.old_errfunc;
    status = HELLO_OK;  /* if it is here, there were no errors */
  }
  adjustresults(L, nresults);
  hello_unlock(L);
  return status;
}


HELLO_API int hello_load (hello_State *L, hello_Reader reader, void *data,
                      const char *chunkname, const char *mode) {
  ZIO z;
  int status;
  hello_lock(L);
  if (!chunkname) chunkname = "?";
  helloZ_init(L, &z, reader, data);
  status = helloD_protectedparser(L, &z, chunkname, mode);
  if (status == HELLO_OK) {  /* no errors? */
    LClosure *f = clLvalue(s2v(L->top - 1));  /* get newly created function */
    if (f->nupvalues >= 1) {  /* does it have an upvalue? */
      /* get global table from registry */
      const TValue *gt = getGtable(L);
      /* set global table as 1st upvalue of 'f' (may be HELLO_ENV) */
      setobj(L, f->upvals[0]->v, gt);
      helloC_barrier(L, f->upvals[0], gt);
    }
  }
  hello_unlock(L);
  return status;
}


HELLO_API int hello_dump (hello_State *L, hello_Writer writer, void *data, int strip) {
  int status;
  TValue *o;
  hello_lock(L);
  api_checknelems(L, 1);
  o = s2v(L->top - 1);
  if (isLfunction(o))
    status = helloU_dump(L, getproto(o), writer, data, strip);
  else
    status = 1;
  hello_unlock(L);
  return status;
}


HELLO_API int hello_status (hello_State *L) {
  return L->status;
}


/*
** Garbage-collection function
*/
HELLO_API int hello_gc (hello_State *L, int what, ...) {
  va_list argp;
  int res = 0;
  global_State *g = G(L);
  if (g->gcstp & GCSTPGC)  /* internal stop? */
    return -1;  /* all options are invalid when stopped */
  hello_lock(L);
  va_start(argp, what);
  switch (what) {
    case HELLO_GCSTOP: {
      g->gcstp = GCSTPUSR;  /* stopped by the user */
      break;
    }
    case HELLO_GCRESTART: {
      helloE_setdebt(g, 0);
      g->gcstp = 0;  /* (GCSTPGC must be already zero here) */
      break;
    }
    case HELLO_GCCOLLECT: {
      helloC_fullgc(L, 0);
      break;
    }
    case HELLO_GCCOUNT: {
      /* GC values are expressed in Kbytes: #bytes/2^10 */
      res = cast_int(gettotalbytes(g) >> 10);
      break;
    }
    case HELLO_GCCOUNTB: {
      res = cast_int(gettotalbytes(g) & 0x3ff);
      break;
    }
    case HELLO_GCSTEP: {
      int data = va_arg(argp, int);
      l_mem debt = 1;  /* =1 to signal that it did an actual step */
      lu_byte oldstp = g->gcstp;
      g->gcstp = 0;  /* allow GC to run (GCSTPGC must be zero here) */
      if (data == 0) {
        helloE_setdebt(g, 0);  /* do a basic step */
        helloC_step(L);
      }
      else {  /* add 'data' to total debt */
        debt = cast(l_mem, data) * 1024 + g->GCdebt;
        helloE_setdebt(g, debt);
        helloC_checkGC(L);
      }
      g->gcstp = oldstp;  /* restore previous state */
      if (debt > 0 && g->gcstate == GCSpause)  /* end of cycle? */
        res = 1;  /* signal it */
      break;
    }
    case HELLO_GCSETPAUSE: {
      int data = va_arg(argp, int);
      res = getgcparam(g->gcpause);
      setgcparam(g->gcpause, data);
      break;
    }
    case HELLO_GCSETSTEPMUL: {
      int data = va_arg(argp, int);
      res = getgcparam(g->gcstepmul);
      setgcparam(g->gcstepmul, data);
      break;
    }
    case HELLO_GCISRUNNING: {
      res = gcrunning(g);
      break;
    }
    case HELLO_GCGEN: {
      int minormul = va_arg(argp, int);
      int majormul = va_arg(argp, int);
      res = isdecGCmodegen(g) ? HELLO_GCGEN : HELLO_GCINC;
      if (minormul != 0)
        g->genminormul = minormul;
      if (majormul != 0)
        setgcparam(g->genmajormul, majormul);
      helloC_changemode(L, KGC_GEN);
      break;
    }
    case HELLO_GCINC: {
      int pause = va_arg(argp, int);
      int stepmul = va_arg(argp, int);
      int stepsize = va_arg(argp, int);
      res = isdecGCmodegen(g) ? HELLO_GCGEN : HELLO_GCINC;
      if (pause != 0)
        setgcparam(g->gcpause, pause);
      if (stepmul != 0)
        setgcparam(g->gcstepmul, stepmul);
      if (stepsize != 0)
        g->gcstepsize = stepsize;
      helloC_changemode(L, KGC_INC);
      break;
    }
    default: res = -1;  /* invalid option */
  }
  va_end(argp);
  hello_unlock(L);
  return res;
}



/*
** miscellaneous functions
*/


HELLO_API void hello_error (hello_State *L) {
  TValue *errobj;
  hello_lock(L);
  errobj = s2v(L->top - 1);
  api_checknelems(L, 1);
  /* error object is the memory error message? */
  if (ttisshrstring(errobj) && eqshrstr(tsvalue(errobj), G(L)->memerrmsg))
    helloM_error(L);  /* raise a memory error */
  else
    helloG_errormsg(L);  /* raise a regular error */
}


HELLO_API int hello_next (hello_State *L, int idx) {
  Table *t;
  int more;
  hello_lock(L);
  api_checknelems(L, 1);
  t = gettable(L, idx);
  more = helloH_next(L, t, L->top - 1);
  if (more) {
    api_incr_top(L);
  }
  else  /* no more elements */
    L->top -= 1;  /* remove key */
  hello_unlock(L);
  return more;
}


HELLO_API void hello_toclose (hello_State *L, int idx) {
  int nresults;
  StkId o;
  hello_lock(L);
  o = index2stack(L, idx);
  nresults = L->ci->nresults;
  api_check(L, L->tbclist < o, "given index below or equal a marked one");
  helloF_newtbcupval(L, o);  /* create new to-be-closed upvalue */
  if (!hastocloseCfunc(nresults))  /* function not marked yet? */
    L->ci->nresults = codeNresults(nresults);  /* mark it */
  hello_assert(hastocloseCfunc(L->ci->nresults));
  hello_unlock(L);
}


HELLO_API void hello_concat (hello_State *L, int n) {
  hello_lock(L);
  api_checknelems(L, n);
  if (n > 0)
    helloV_concat(L, n);
  else {  /* nothing to concatenate */
    setsvalue2s(L, L->top, helloS_newlstr(L, "", 0));  /* push empty string */
    api_incr_top(L);
  }
  helloC_checkGC(L);
  hello_unlock(L);
}


HELLO_API void hello_len (hello_State *L, int idx) {
  TValue *t;
  hello_lock(L);
  t = index2value(L, idx);
  helloV_objlen(L, L->top, t);
  api_incr_top(L);
  hello_unlock(L);
}


HELLO_API hello_Alloc hello_getallocf (hello_State *L, void **ud) {
  hello_Alloc f;
  hello_lock(L);
  if (ud) *ud = G(L)->ud;
  f = G(L)->frealloc;
  hello_unlock(L);
  return f;
}


HELLO_API void hello_setallocf (hello_State *L, hello_Alloc f, void *ud) {
  hello_lock(L);
  G(L)->ud = ud;
  G(L)->frealloc = f;
  hello_unlock(L);
}


void hello_setwarnf (hello_State *L, hello_WarnFunction f, void *ud) {
  hello_lock(L);
  G(L)->ud_warn = ud;
  G(L)->warnf = f;
  hello_unlock(L);
}


void hello_warning (hello_State *L, const char *msg, int tocont) {
  hello_lock(L);
  helloE_warning(L, msg, tocont);
  hello_unlock(L);
}



HELLO_API void *hello_newuserdatauv (hello_State *L, size_t size, int nuvalue) {
  Udata *u;
  hello_lock(L);
  api_check(L, 0 <= nuvalue && nuvalue < USHRT_MAX, "invalid value");
  u = helloS_newudata(L, size, nuvalue);
  setuvalue(L, s2v(L->top), u);
  api_incr_top(L);
  helloC_checkGC(L);
  hello_unlock(L);
  return getudatamem(u);
}



static const char *aux_upvalue (TValue *fi, int n, TValue **val,
                                GCObject **owner) {
  switch (ttypetag(fi)) {
    case HELLO_VCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      if (!(cast_uint(n) - 1u < cast_uint(f->nupvalues)))
        return NULL;  /* 'n' not in [1, f->nupvalues] */
      *val = &f->upvalue[n-1];
      if (owner) *owner = obj2gco(f);
      return "";
    }
    case HELLO_VLCL: {  /* Hello closure */
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


HELLO_API const char *hello_getupvalue (hello_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  hello_lock(L);
  name = aux_upvalue(index2value(L, funcindex), n, &val, NULL);
  if (name) {
    setobj2s(L, L->top, val);
    api_incr_top(L);
  }
  hello_unlock(L);
  return name;
}


HELLO_API const char *hello_setupvalue (hello_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  GCObject *owner = NULL;  /* to avoid warnings */
  TValue *fi;
  hello_lock(L);
  fi = index2value(L, funcindex);
  api_checknelems(L, 1);
  name = aux_upvalue(fi, n, &val, &owner);
  if (name) {
    L->top--;
    setobj(L, val, s2v(L->top));
    helloC_barrier(L, owner, val);
  }
  hello_unlock(L);
  return name;
}


static UpVal **getupvalref (hello_State *L, int fidx, int n, LClosure **pf) {
  static const UpVal *const nullup = NULL;
  LClosure *f;
  TValue *fi = index2value(L, fidx);
  api_check(L, ttisLclosure(fi), "Hello function expected");
  f = clLvalue(fi);
  if (pf) *pf = f;
  if (1 <= n && n <= f->p->sizeupvalues)
    return &f->upvals[n - 1];  /* get its upvalue pointer */
  else
    return (UpVal**)&nullup;
}


HELLO_API void *hello_upvalueid (hello_State *L, int fidx, int n) {
  TValue *fi = index2value(L, fidx);
  switch (ttypetag(fi)) {
    case HELLO_VLCL: {  /* hello closure */
      return *getupvalref(L, fidx, n, NULL);
    }
    case HELLO_VCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      if (1 <= n && n <= f->nupvalues)
        return &f->upvalue[n - 1];
      /* else */
    }  /* FALLTHROUGH */
    case HELLO_VLCF:
      return NULL;  /* light C functions have no upvalues */
    default: {
      api_check(L, 0, "function expected");
      return NULL;
    }
  }
}


HELLO_API void hello_upvaluejoin (hello_State *L, int fidx1, int n1,
                                            int fidx2, int n2) {
  LClosure *f1;
  UpVal **up1 = getupvalref(L, fidx1, n1, &f1);
  UpVal **up2 = getupvalref(L, fidx2, n2, NULL);
  api_check(L, *up1 != NULL && *up2 != NULL, "invalid upvalue index");
  *up1 = *up2;
  helloC_objbarrier(L, f1, *up1);
}


