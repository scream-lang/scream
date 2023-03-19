/*
** $Id: lvm.c $
** Hello virtual machine
** See Copyright Notice in hello.h
*/

#define lvm_c
#define HELLO_CORE

#include "lprefix.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hello.h"

#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lvm.h"

#ifdef HELLO_ETL_ENABLE
#include <chrono>
#endif

#ifdef HELLO_VMDUMP
#include <string>
#include <sstream>
#include "lauxlib.h" // hello_writestring
#include "lopnames.h"
#endif


/* limit for table tag-method chains (to avoid infinite loops) */
#define MAXTAGLOOP	2000


/*
** 'l_intfitsf' checks whether a given integer is in the range that
** can be converted to a float without rounding. Used in comparisons.
*/

/* number of bits in the mantissa of a float */
#define NBM		(l_floatatt(MANT_DIG))

/*
** Check whether some integers may not fit in a float, testing whether
** (maxinteger >> NBM) > 0. (That implies (1 << NBM) <= maxinteger.)
** (The shifts are done in parts, to avoid shifting by more than the size
** of an integer. In a worst case, NBM == 113 for long double and
** sizeof(long) == 32.)
*/
#if ((((HELLO_MAXINTEGER >> (NBM / 4)) >> (NBM / 4)) >> (NBM / 4)) \
    >> (NBM - (3 * (NBM / 4))))  >  0

/* limit for integers that fit in a float */
#define MAXINTFITSF	((hello_Unsigned)1 << NBM)

/* check whether 'i' is in the interval [-MAXINTFITSF, MAXINTFITSF] */
#define l_intfitsf(i)	((MAXINTFITSF + l_castS2U(i)) <= (2 * MAXINTFITSF))

#else  /* all integers fit in a float precisely */

#define l_intfitsf(i)	1

#endif


/*
** Try to convert a value from string to a number value.
** If the value is not a string or is a string not representing
** a valid numeral (or if coercions from strings to numbers
** are disabled via macro 'cvt2num'), do not modify 'result'
** and return 0.
*/
static int l_strton (const TValue *obj, TValue *result) {
  hello_assert(obj != result);
  if (!cvt2num(obj))  /* is object not a string? */
    return 0;
  else
    return (helloO_str2num(svalue(obj), result) == vslen(obj) + 1);
}


/*
** Try to convert a value to a float. The float case is already handled
** by the macro 'tonumber'.
*/
int helloV_tonumber_ (const TValue *obj, hello_Number *n) {
  TValue v;
  if (ttisinteger(obj)) {
    *n = cast_num(ivalue(obj));
    return 1;
  }
  else if (l_strton(obj, &v)) {  /* string coercible to number? */
    *n = nvalue(&v);  /* convert result of 'helloO_str2num' to a float */
    return 1;
  }
  else
    return 0;  /* conversion failed */
}


/*
** try to convert a float to an integer, rounding according to 'mode'.
*/
int helloV_flttointeger (hello_Number n, hello_Integer *p, F2Imod mode) {
  hello_Number f = l_floor(n);
  if (n != f) {  /* not an integral value? */
    if (mode == F2Ieq) return 0;  /* fails if mode demands integral value */
    else if (mode == F2Iceil)  /* needs ceil? */
      f += 1;  /* convert floor to ceil (remember: n != f) */
  }
  return hello_numbertointeger(f, p);
}


/*
** try to convert a value to an integer, rounding according to 'mode',
** without string coercion.
** ("Fast track" handled by macro 'tointegerns'.)
*/
int helloV_tointegerns (const TValue *obj, hello_Integer *p, F2Imod mode) {
  if (ttisfloat(obj))
    return helloV_flttointeger(fltvalue(obj), p, mode);
  else if (ttisinteger(obj)) {
    *p = ivalue(obj);
    return 1;
  }
  else
    return 0;
}


/*
** try to convert a value to an integer.
*/
int helloV_tointeger (const TValue *obj, hello_Integer *p, F2Imod mode) {
  TValue v;
  if (l_strton(obj, &v))  /* does 'obj' point to a numerical string? */
    obj = &v;  /* change it to point to its corresponding number */
  return helloV_tointegerns(obj, p, mode);
}


/*
** Try to convert a 'for' limit to an integer, preserving the semantics
** of the loop. Return true if the loop must not run; otherwise, '*p'
** gets the integer limit.
** (The following explanation assumes a positive step; it is valid for
** negative steps mutatis mutandis.)
** If the limit is an integer or can be converted to an integer,
** rounding down, that is the limit.
** Otherwise, check whether the limit can be converted to a float. If
** the float is too large, clip it to HELLO_MAXINTEGER.  If the float
** is too negative, the loop should not run, because any initial
** integer value is greater than such limit; so, the function returns
** true to signal that. (For this latter case, no integer limit would be
** correct; even a limit of HELLO_MININTEGER would run the loop once for
** an initial value equal to HELLO_MININTEGER.)
*/
static int forlimit (hello_State *L, hello_Integer init, const TValue *lim,
                                   hello_Integer *p, hello_Integer step) {
  if (!helloV_tointeger(lim, p, (step < 0 ? F2Iceil : F2Ifloor))) {
    /* not coercible to in integer */
    hello_Number flim;  /* try to convert to float */
    if (!tonumber(lim, &flim)) /* cannot convert to float? */
      helloG_forerror(L, lim, "limit");
    /* else 'flim' is a float out of integer bounds */
    if (helloi_numlt(0, flim)) {  /* if it is positive, it is too large */
      if (step < 0) return 1;  /* initial value must be less than it */
      *p = HELLO_MAXINTEGER;  /* truncate */
    }
    else {  /* it is less than min integer */
      if (step > 0) return 1;  /* initial value must be greater than it */
      *p = HELLO_MININTEGER;  /* truncate */
    }
  }
  return (step > 0 ? init > *p : init < *p);  /* not to run? */
}


/*
** Prepare a numerical for loop (opcode OP_FORPREP).
** Return true to skip the loop. Otherwise,
** after preparation, stack will be as follows:
**   ra : internal index (safe copy of the control variable)
**   ra + 1 : loop counter (integer loops) or limit (float loops)
**   ra + 2 : step
**   ra + 3 : control variable
*/
static int forprep (hello_State *L, StkId ra) {
  TValue *pinit = s2v(ra);
  TValue *plimit = s2v(ra + 1);
  TValue *pstep = s2v(ra + 2);
  if (ttisinteger(pinit) && ttisinteger(pstep)) { /* integer loop? */
    hello_Integer init = ivalue(pinit);
    hello_Integer step = ivalue(pstep);
    hello_Integer limit;
    if (step == 0)
      helloG_runerror(L, "'for' step is zero");
    setivalue(s2v(ra + 3), init);  /* control variable */
    if (forlimit(L, init, plimit, &limit, step))
      return 1;  /* skip the loop */
    else {  /* prepare loop counter */
      hello_Unsigned count;
      if (step > 0) {  /* ascending loop? */
        count = l_castS2U(limit) - l_castS2U(init);
        if (step != 1)  /* avoid division in the too common case */
          count /= l_castS2U(step);
      }
      else {  /* step < 0; descending loop */
        count = l_castS2U(init) - l_castS2U(limit);
        /* 'step+1' avoids negating 'mininteger' */
        count /= l_castS2U(-(step + 1)) + 1u;
      }
      /* store the counter in place of the limit (which won't be
         needed anymore) */
      setivalue(plimit, l_castU2S(count));
    }
  }
  else {  /* try making all values floats */
    hello_Number init; hello_Number limit; hello_Number step;
    if (l_unlikely(!tonumber(plimit, &limit)))
      helloG_forerror(L, plimit, "limit");
    if (l_unlikely(!tonumber(pstep, &step)))
      helloG_forerror(L, pstep, "step");
    if (l_unlikely(!tonumber(pinit, &init)))
      helloG_forerror(L, pinit, "initial value");
    if (step == 0)
      helloG_runerror(L, "'for' step is zero");
    if (helloi_numlt(0, step) ? helloi_numlt(limit, init)
                            : helloi_numlt(init, limit))
      return 1;  /* skip the loop */
    else {
      /* make sure internal values are all floats */
      setfltvalue(plimit, limit);
      setfltvalue(pstep, step);
      setfltvalue(s2v(ra), init);  /* internal index */
      setfltvalue(s2v(ra + 3), init);  /* control variable */
    }
  }
  return 0;
}


/*
** Execute a step of a float numerical for loop, returning
** true iff the loop must continue. (The integer case is
** written online with opcode OP_FORLOOP, for performance.)
*/
static int floatforloop (StkId ra) {
  hello_Number step = fltvalue(s2v(ra + 2));
  hello_Number limit = fltvalue(s2v(ra + 1));
  hello_Number idx = fltvalue(s2v(ra));  /* internal index */
  idx = helloi_numadd(L, idx, step);  /* increment index */
  if (helloi_numlt(0, step) ? helloi_numle(idx, limit)
                          : helloi_numle(limit, idx)) {
    chgfltvalue(s2v(ra), idx);  /* update internal index */
    setfltvalue(s2v(ra + 3), idx);  /* and control variable */
    return 1;  /* jump back */
  }
  else
    return 0;  /* finish the loop */
}


/*
** Finish the table access 'val = t[key]'.
** if 'slot' is NULL, 't' is not a table; otherwise, 'slot' points to
** t[k] entry (which must be empty).
*/
void helloV_finishget (hello_State *L, const TValue *t, TValue *key, StkId val,
                      const TValue *slot) {
  int loop;  /* counter to avoid infinite loops */
  const TValue *tm;  /* metamethod */
  int isValueString = ttisstring(t) && ttisinteger(key);
  for (loop = 0; loop < MAXTAGLOOP; loop++) {
    if (slot == NULL) {  /* 't' is not a table? */
      hello_assert(!ttistable(t));
      if (isValueString) { /* index for character of string */
        hello_Integer index = ivalue(key);
        if (index < 0) { /* negative index, index from end of string */
          index += vslen(t) + 1;
        }
        if (((hello_Integer)vslen(t) < index) || (index < 1)) { /* invalid index */
          setnilvalue(s2v(val));
          return;
        }
        else { /* index is valid */
          setsvalue(L, s2v(val), helloS_newlstr(L, &tsvalue(t)->contents[index - 1], 1));
          return;
        }
      }
      else {
        tm = helloT_gettmbyobj(L, t, TM_INDEX);
        if (l_unlikely(notm(tm)))
          helloG_typeerror(L, t, "index");  /* no metamethod */
        /* else will try the metamethod */
      }
    }
    else {  /* 't' is a table */
      hello_assert(isempty(slot));
      tm = fasttm(L, hvalue(t)->metatable, TM_INDEX);  /* table's metamethod */
      if (tm == NULL) {  /* no metamethod? */
        setnilvalue(s2v(val));  /* result is nil */
        return;
      }
      /* else will try the metamethod */
    }
    if (ttisfunction(tm)) {  /* is metamethod a function? */
      helloT_callTMres(L, tm, t, key, val);  /* call it */
      return;
    }
    t = tm;  /* else try to access 'tm[key]' */
    if (helloV_fastget(L, t, key, slot, helloH_get)) {  /* fast track? */
      setobj2s(L, val, slot);  /* done */
      return;
    }
    /* else repeat (tail call 'helloV_finishget') */
  }
  helloG_runerror(L, "'__index' chain too long; possible loop");
}


/*
** Finish a table assignment 't[key] = val'.
** If 'slot' is NULL, 't' is not a table.  Otherwise, 'slot' points
** to the entry 't[key]', or to a value with an absent key if there
** is no such entry.  (The value at 'slot' must be empty, otherwise
** 'helloV_fastget' would have done the job.)
*/
void helloV_finishset (hello_State *L, const TValue *t, TValue *key,
                     TValue *val, const TValue *slot) {
  int loop;  /* counter to avoid infinite loops */
  for (loop = 0; loop < MAXTAGLOOP; loop++) {
    const TValue *tm;  /* '__newindex' metamethod */
    if (slot != NULL) {  /* is 't' a table? */
      Table *h = hvalue(t);  /* save 't' table */
      hello_assert(isempty(slot));  /* slot must be empty */
      tm = fasttm(L, h->metatable, TM_NEWINDEX);  /* get metamethod */
      if (tm == NULL) {  /* no metamethod? */
        helloH_finishset(L, h, key, slot, val);  /* set new value */
        invalidateTMcache(h);
        helloC_barrierback(L, obj2gco(h), val);
        return;
      }
      /* else will try the metamethod */
    }
    else {  /* not a table; check metamethod */
      tm = helloT_gettmbyobj(L, t, TM_NEWINDEX);
      if (l_unlikely(notm(tm)))
        helloG_typeerror(L, t, "index");
    }
    /* try the metamethod */
    if (ttisfunction(tm)) {
      helloT_callTM(L, tm, t, key, val);
      return;
    }
    t = tm;  /* else repeat assignment over 'tm' */
    if (helloV_fastget(L, t, key, slot, helloH_get)) {
      helloV_finishfastset(L, t, slot, val);
      return;  /* done */
    }
    /* else 'return helloV_finishset(L, t, key, val, slot)' (loop) */
  }
  helloG_runerror(L, "'__newindex' chain too long; possible loop");
}


/*
** Compare two strings 'ls' x 'rs', returning an integer less-equal-
** -greater than zero if 'ls' is less-equal-greater than 'rs'.
** The code is a little tricky because it allows '\0' in the strings
** and it uses 'strcoll' (to respect locales) for each segments
** of the strings.
*/
static int l_strcmp (const TString *ls, const TString *rs) {
  const char *l = getstr(ls);
  size_t ll = tsslen(ls);
  const char *r = getstr(rs);
  size_t lr = tsslen(rs);
  for (;;) {  /* for each segment */
    int temp = strcoll(l, r);
    if (temp != 0)  /* not equal? */
      return temp;  /* done */
    else {  /* strings are equal up to a '\0' */
      size_t len = strlen(l);  /* index of first '\0' in both strings */
      if (len == lr)  /* 'rs' is finished? */
        return (len == ll) ? 0 : 1;  /* check 'ls' */
      else if (len == ll)  /* 'ls' is finished? */
        return -1;  /* 'ls' is less than 'rs' ('rs' is not finished) */
      /* both strings longer than 'len'; go on comparing after the '\0' */
      len++;
      l += len; ll -= len; r += len; lr -= len;
    }
  }
}


/*
** Check whether integer 'i' is less than float 'f'. If 'i' has an
** exact representation as a float ('l_intfitsf'), compare numbers as
** floats. Otherwise, use the equivalence 'i < f <=> i < ceil(f)'.
** If 'ceil(f)' is out of integer range, either 'f' is greater than
** all integers or less than all integers.
** (The test with 'l_intfitsf' is only for performance; the else
** case is correct for all values, but it is slow due to the conversion
** from float to int.)
** When 'f' is NaN, comparisons must result in false.
*/
l_sinline int LTintfloat (hello_Integer i, hello_Number f) {
  if (l_intfitsf(i))
    return helloi_numlt(cast_num(i), f);  /* compare them as floats */
  else {  /* i < f <=> i < ceil(f) */
    hello_Integer fi;
    if (helloV_flttointeger(f, &fi, F2Iceil))  /* fi = ceil(f) */
      return i < fi;   /* compare them as integers */
    else  /* 'f' is either greater or less than all integers */
      return f > 0;  /* greater? */
  }
}


/*
** Check whether integer 'i' is less than or equal to float 'f'.
** See comments on previous function.
*/
l_sinline int LEintfloat (hello_Integer i, hello_Number f) {
  if (l_intfitsf(i))
    return helloi_numle(cast_num(i), f);  /* compare them as floats */
  else {  /* i <= f <=> i <= floor(f) */
    hello_Integer fi;
    if (helloV_flttointeger(f, &fi, F2Ifloor))  /* fi = floor(f) */
      return i <= fi;   /* compare them as integers */
    else  /* 'f' is either greater or less than all integers */
      return f > 0;  /* greater? */
  }
}


/*
** Check whether float 'f' is less than integer 'i'.
** See comments on previous function.
*/
l_sinline int LTfloatint (hello_Number f, hello_Integer i) {
  if (l_intfitsf(i))
    return helloi_numlt(f, cast_num(i));  /* compare them as floats */
  else {  /* f < i <=> floor(f) < i */
    hello_Integer fi;
    if (helloV_flttointeger(f, &fi, F2Ifloor))  /* fi = floor(f) */
      return fi < i;   /* compare them as integers */
    else  /* 'f' is either greater or less than all integers */
      return f < 0;  /* less? */
  }
}


/*
** Check whether float 'f' is less than or equal to integer 'i'.
** See comments on previous function.
*/
l_sinline int LEfloatint (hello_Number f, hello_Integer i) {
  if (l_intfitsf(i))
    return helloi_numle(f, cast_num(i));  /* compare them as floats */
  else {  /* f <= i <=> ceil(f) <= i */
    hello_Integer fi;
    if (helloV_flttointeger(f, &fi, F2Iceil))  /* fi = ceil(f) */
      return fi <= i;   /* compare them as integers */
    else  /* 'f' is either greater or less than all integers */
      return f < 0;  /* less? */
  }
}


/*
** Return 'l < r', for numbers.
*/
l_sinline int LTnum (const TValue *l, const TValue *r) {
  hello_assert(ttisnumber(l) && ttisnumber(r));
  if (ttisinteger(l)) {
    hello_Integer li = ivalue(l);
    if (ttisinteger(r))
      return li < ivalue(r);  /* both are integers */
    else  /* 'l' is int and 'r' is float */
      return LTintfloat(li, fltvalue(r));  /* l < r ? */
  }
  else {
    hello_Number lf = fltvalue(l);  /* 'l' must be float */
    if (ttisfloat(r))
      return helloi_numlt(lf, fltvalue(r));  /* both are float */
    else  /* 'l' is float and 'r' is int */
      return LTfloatint(lf, ivalue(r));
  }
}


/*
** Return 'l <= r', for numbers.
*/
l_sinline int LEnum (const TValue *l, const TValue *r) {
  hello_assert(ttisnumber(l) && ttisnumber(r));
  if (ttisinteger(l)) {
    hello_Integer li = ivalue(l);
    if (ttisinteger(r))
      return li <= ivalue(r);  /* both are integers */
    else  /* 'l' is int and 'r' is float */
      return LEintfloat(li, fltvalue(r));  /* l <= r ? */
  }
  else {
    hello_Number lf = fltvalue(l);  /* 'l' must be float */
    if (ttisfloat(r))
      return helloi_numle(lf, fltvalue(r));  /* both are float */
    else  /* 'l' is float and 'r' is int */
      return LEfloatint(lf, ivalue(r));
  }
}


/*
** return 'l < r' for non-numbers.
*/
static int lessthanothers (hello_State *L, const TValue *l, const TValue *r) {
  hello_assert(!ttisnumber(l) || !ttisnumber(r));
  if (ttisstring(l) && ttisstring(r))  /* both are strings? */
    return l_strcmp(tsvalue(l), tsvalue(r)) < 0;
  else
    return helloT_callorderTM(L, l, r, TM_LT);
}


/*
** Main operation less than; return 'l < r'.
*/
int helloV_lessthan (hello_State *L, const TValue *l, const TValue *r) {
  if (ttisnumber(l) && ttisnumber(r))  /* both operands are numbers? */
    return LTnum(l, r);
  else return lessthanothers(L, l, r);
}


/*
** return 'l <= r' for non-numbers.
*/
static int lessequalothers (hello_State *L, const TValue *l, const TValue *r) {
  hello_assert(!ttisnumber(l) || !ttisnumber(r));
  if (ttisstring(l) && ttisstring(r))  /* both are strings? */
    return l_strcmp(tsvalue(l), tsvalue(r)) <= 0;
  else
    return helloT_callorderTM(L, l, r, TM_LE);
}


/*
** Main operation less than or equal to; return 'l <= r'.
*/
int helloV_lessequal (hello_State *L, const TValue *l, const TValue *r) {
  if (ttisnumber(l) && ttisnumber(r))  /* both operands are numbers? */
    return LEnum(l, r);
  else return lessequalothers(L, l, r);
}


/*
** Main operation for equality of Hello values; return 't1 == t2'.
** L == NULL means raw equality (no metamethods)
*/
int helloV_equalobj (hello_State *L, const TValue *t1, const TValue *t2) {
  const TValue *tm;
  if (ttypetag(t1) != ttypetag(t2)) {  /* not the same variant? */
    if (ttype(t1) != ttype(t2) || ttype(t1) != HELLO_TNUMBER)
      return 0;  /* only numbers can be equal with different variants */
    else {  /* two numbers with different variants */
      /* One of them is an integer. If the other does not have an
         integer value, they cannot be equal; otherwise, compare their
         integer values. */
      hello_Integer i1, i2;
      return (helloV_tointegerns(t1, &i1, F2Ieq) &&
              helloV_tointegerns(t2, &i2, F2Ieq) &&
              i1 == i2);
    }
  }
  /* values have same type and same variant */
  switch (ttypetag(t1)) {
    case HELLO_VNIL: case HELLO_VFALSE: case HELLO_VTRUE: return 1;
    case HELLO_VNUMINT: return (ivalue(t1) == ivalue(t2));
    case HELLO_VNUMFLT: return helloi_numeq(fltvalue(t1), fltvalue(t2));
    case HELLO_VLIGHTUSERDATA: return pvalue(t1) == pvalue(t2);
    case HELLO_VLCF: return fvalue(t1) == fvalue(t2);
    case HELLO_VSHRSTR: return eqshrstr(tsvalue(t1), tsvalue(t2));
    case HELLO_VLNGSTR: return helloS_eqlngstr(tsvalue(t1), tsvalue(t2));
    case HELLO_VUSERDATA: {
      if (uvalue(t1) == uvalue(t2)) return 1;
      else if (L == NULL) return 0;
      tm = fasttm(L, uvalue(t1)->metatable, TM_EQ);
      if (tm == NULL)
        tm = fasttm(L, uvalue(t2)->metatable, TM_EQ);
      break;  /* will try TM */
    }
    case HELLO_VTABLE: {
      if (hvalue(t1) == hvalue(t2)) return 1;
      else if (L == NULL) return 0;
      tm = fasttm(L, hvalue(t1)->metatable, TM_EQ);
      if (tm == NULL)
        tm = fasttm(L, hvalue(t2)->metatable, TM_EQ);
      break;  /* will try TM */
    }
    default:
      return gcvalue(t1) == gcvalue(t2);
  }
  if (tm == NULL)  /* no TM? */
    return 0;  /* objects are different */
  else {
    helloT_callTMres(L, tm, t1, t2, L->top);  /* call TM */
    return !l_isfalse(s2v(L->top));
  }
}


/* macro used by 'helloV_concat' to ensure that element at 'o' is a string */
#define tostring(L,o)  \
    (ttisstring(o) || (cvt2str(o) && (helloO_tostring(L, o), 1)))

#define isemptystr(o)	(ttisshrstring(o) && tsvalue(o)->shrlen == 0)

/* copy strings in stack from top - n up to top - 1 to buffer */
static void copy2buff (StkId top, int n, char *buff) {
  size_t tl = 0;  /* size already copied */
  do {
    size_t l = vslen(s2v(top - n));  /* length of string being copied */
    memcpy(buff + tl, svalue(s2v(top - n)), l * sizeof(char));
    tl += l;
  } while (--n > 0);
}


/*
** Main operation for concatenation: concat 'total' values in the stack,
** from 'L->top - total' up to 'L->top - 1'.
*/
void helloV_concat (hello_State *L, int total) {
  if (total == 1)
    return;  /* "all" values already concatenated */
  do {
    StkId top = L->top;
    int n = 2;  /* number of elements handled in this pass (at least 2) */
    if (!(ttisstring(s2v(top - 2)) || cvt2str(s2v(top - 2))) ||
        !tostring(L, s2v(top - 1)))
      helloT_tryconcatTM(L);
    else if (isemptystr(s2v(top - 1)))  /* second operand is empty? */
      cast_void(tostring(L, s2v(top - 2)));  /* result is first operand */
    else if (isemptystr(s2v(top - 2))) {  /* first operand is empty string? */
      setobjs2s(L, top - 2, top - 1);  /* result is second op. */
    }
    else {
      /* at least two non-empty string values; get as many as possible */
      size_t tl = vslen(s2v(top - 1));
      TString *ts;
      /* collect total length and number of strings */
      for (n = 1; n < total && tostring(L, s2v(top - n - 1)); n++) {
        size_t l = vslen(s2v(top - n - 1));
        if (l_unlikely(l >= (MAX_SIZE/sizeof(char)) - tl))
          helloG_runerror(L, "string length overflow");
        tl += l;
      }
      if (tl <= HELLOI_MAXSHORTLEN) {  /* is result a short string? */
        char buff[HELLOI_MAXSHORTLEN];
        copy2buff(top, n, buff);  /* copy strings to buffer */
        ts = helloS_newlstr(L, buff, tl);
      }
      else {  /* long string; copy strings directly to final result */
        ts = helloS_createlngstrobj(L, tl);
        copy2buff(top, n, getstr(ts));
      }
      setsvalue2s(L, top - n, ts);  /* create result */
    }
    total -= n-1;  /* got 'n' strings to create 1 new */
    L->top -= n-1;  /* popped 'n' strings and pushed one */
  } while (total > 1);  /* repeat until only 1 result left */
}


/*
** Main operation 'ra = #rb'.
*/
void helloV_objlen (hello_State *L, StkId ra, const TValue *rb) {
  const TValue *tm;
  switch (ttypetag(rb)) {
    case HELLO_VTABLE: {
      Table *h = hvalue(rb);
      tm = fasttm(L, h->metatable, TM_LEN);
      if (tm) break;  /* metamethod? break switch to call it */
      if (!h->length) h->length = helloH_getn(h);  /* cache length */
      setivalue(s2v(ra), h->length);
      return;
    }
    case HELLO_VSHRSTR: {
      setivalue(s2v(ra), tsvalue(rb)->shrlen);
      return;
    }
    case HELLO_VLNGSTR: {
      setivalue(s2v(ra), tsvalue(rb)->u.lnglen);
      return;
    }
    default: {  /* try metamethod */
      tm = helloT_gettmbyobj(L, rb, TM_LEN);
      if (l_unlikely(notm(tm)))  /* no metamethod? */
        helloG_typeerror(L, rb, "get length of");
      break;
    }
  }
  helloT_callTMres(L, tm, rb, rb, ra);
}


/*
** Integer division; return 'm // n', that is, floor(m/n).
** C division truncates its result (rounds towards zero).
** 'floor(q) == trunc(q)' when 'q >= 0' or when 'q' is integer,
** otherwise 'floor(q) == trunc(q) - 1'.
*/
hello_Integer helloV_idiv (hello_State *L, hello_Integer m, hello_Integer n) {
  if (l_unlikely(l_castS2U(n) + 1u <= 1u)) {  /* special cases: -1 or 0 */
    if (n == 0)
      helloG_runerror(L, "attempt to divide by zero");
    return intop(-, 0, m);   /* n==-1; avoid overflow with 0x80000...//-1 */
  }
  else {
    hello_Integer q = m / n;  /* perform C division */
    if ((m ^ n) < 0 && m % n != 0)  /* 'm/n' would be negative non-integer? */
      q -= 1;  /* correct result for different rounding */
    return q;
  }
}


/*
** Integer modulus; return 'm % n'. (Assume that C '%' with
** negative operands follows C99 behavior. See previous comment
** about helloV_idiv.)
*/
hello_Integer helloV_mod (hello_State *L, hello_Integer m, hello_Integer n) {
  if (l_unlikely(l_castS2U(n) + 1u <= 1u)) {  /* special cases: -1 or 0 */
    if (n == 0)
      helloG_runerror(L, "attempt to perform 'n%%0'");
    return 0;   /* m % -1 == 0; avoid overflow with 0x80000...%-1 */
  }
  else {
    hello_Integer r = m % n;
    if (r != 0 && (r ^ n) < 0)  /* 'm/n' would be non-integer negative? */
      r += n;  /* correct result for different rounding */
    return r;
  }
}


/*
** Float modulus
*/
hello_Number helloV_modf (hello_State *L, hello_Number m, hello_Number n) {
  hello_Number r;
  helloi_nummod(L, m, n, r);
  return r;
}


/* number of bits in an integer */
#define NBITS	cast_int(sizeof(hello_Integer) * CHAR_BIT)


/*
** Shift left operation. (Shift right just negates 'y'.)
*/
hello_Integer helloV_shiftl (hello_Integer x, hello_Integer y) {
  if (y < 0) {  /* shift right? */
    if (y <= -NBITS) return 0;
    else return intop(>>, x, -y);
  }
  else {  /* shift left */
    if (y >= NBITS) return 0;
    else return intop(<<, x, y);
  }
}


/*
** create a new Hello closure, push it in the stack, and initialize
** its upvalues.
*/
static void pushclosure (hello_State *L, Proto *p, UpVal **encup, StkId base,
                         StkId ra) {
  int nup = p->sizeupvalues;
  Upvaldesc *uv = p->upvalues;
  int i;
  LClosure *ncl = helloF_newLclosure(L, nup);
  ncl->p = p;
  setclLvalue2s(L, ra, ncl);  /* anchor new closure in stack */
  for (i = 0; i < nup; i++) {  /* fill in its upvalues */
    if (uv[i].instack)  /* upvalue refers to local variable? */
      ncl->upvals[i] = helloF_findupval(L, base + uv[i].idx);
    else  /* get upvalue from enclosing function */
      ncl->upvals[i] = encup[uv[i].idx];
    helloC_objbarrier(L, ncl, ncl->upvals[i]);
  }
}


/*
** finish execution of an opcode interrupted by a yield
*/
void helloV_finishOp (hello_State *L) {
  CallInfo *ci = L->ci;
  StkId base = ci->func + 1;
  Instruction inst = *(ci->u.l.savedpc - 1);  /* interrupted instruction */
  OpCode op = GET_OPCODE(inst);
  switch (op) {  /* finish its execution */
    case OP_MMBIN: case OP_MMBINI: case OP_MMBINK: {
      setobjs2s(L, base + GETARG_A(*(ci->u.l.savedpc - 2)), --L->top);
      break;
    }
    case OP_UNM: case OP_BNOT: case OP_LEN:
    case OP_GETTABUP: case OP_GETTABLE: case OP_GETI:
    case OP_GETFIELD: case OP_SELF: {
      setobjs2s(L, base + GETARG_A(inst), --L->top);
      break;
    }
    case OP_LT: case OP_LE:
    case OP_LTI: case OP_LEI:
    case OP_GTI: case OP_GEI:
    case OP_EQ: {  /* note that 'OP_EQI'/'OP_EQK' cannot yield */
      int res = !l_isfalse(s2v(L->top - 1));
      L->top--;
#if defined(HELLO_COMPAT_LT_LE)
      if (ci->callstatus & CIST_LEQ) {  /* "<=" using "<" instead? */
        ci->callstatus ^= CIST_LEQ;  /* clear mark */
        res = !res;  /* negate result */
      }
#endif
      hello_assert(GET_OPCODE(*ci->u.l.savedpc) == OP_JMP);
      if (res != GETARG_k(inst))  /* condition failed? */
        ci->u.l.savedpc++;  /* skip jump instruction */
      break;
    }
    case OP_CONCAT: {
      StkId top = L->top - 1;  /* top when 'helloT_tryconcatTM' was called */
      int a = GETARG_A(inst);      /* first element to concatenate */
      int total = cast_int(top - 1 - (base + a));  /* yet to concatenate */
      setobjs2s(L, top - 2, top);  /* put TM result in proper position */
      L->top = top - 1;  /* top is one after last element (at top-2) */
      helloV_concat(L, total);  /* concat them (may yield again) */
      break;
    }
    case OP_CLOSE: {  /* yielded closing variables */
      ci->u.l.savedpc--;  /* repeat instruction to close other vars. */
      break;
    }
    case OP_RETURN: {  /* yielded closing variables */
      StkId ra = base + GETARG_A(inst);
      /* adjust top to signal correct number of returns, in case the
         return is "up to top" ('isIT') */
      L->top = ra + ci->u2.nres;
      /* repeat instruction to close other vars. and complete the return */
      ci->u.l.savedpc--;
      break;
    }
    default: {
      /* only these other opcodes can yield */
      hello_assert(op == OP_TFORCALL || op == OP_CALL ||
           op == OP_TAILCALL || op == OP_SETTABUP || op == OP_SETTABLE ||
           op == OP_SETI || op == OP_SETFIELD);
      break;
    }
  }
}




/*
** {==================================================================
** Macros for arithmetic/bitwise/comparison opcodes in 'helloV_execute'
** ===================================================================
*/

#define l_addi(L,a,b)	intop(+, a, b)
#define l_subi(L,a,b)	intop(-, a, b)
#define l_muli(L,a,b)	intop(*, a, b)
#define l_band(a,b)	intop(&, a, b)
#define l_bor(a,b)	intop(|, a, b)
#define l_bxor(a,b)	intop(^, a, b)

#define l_lti(a,b)	(a < b)
#define l_lei(a,b)	(a <= b)
#define l_gti(a,b)	(a > b)
#define l_gei(a,b)	(a >= b)


/*
** Arithmetic operations with immediate operands. 'iop' is the integer
** operation, 'fop' is the float operation.
*/
#define op_arithI(L,iop,fop) {  \
  savepc(L);  \
  TValue *v1 = vRB(i);  \
  int imm = GETARG_sC(i);  \
  if (ttisinteger(v1)) {  \
    hello_Integer iv1 = ivalue(v1);  \
    pc++; setivalue(s2v(ra), iop(L, iv1, imm));  \
  }  \
  else if (ttisfloat(v1)) {  \
    hello_Number nb = fltvalue(v1);  \
    hello_Number fimm = cast_num(imm);  \
    pc++; setfltvalue(s2v(ra), fop(L, nb, fimm)); \
  }}


/*
** Auxiliary function for arithmetic operations over floats and others
** with two register operands.
*/
#define op_arithf_aux(L,v1,v2,fop) {  \
  hello_Number n1; hello_Number n2;  \
  if (tonumberns(v1, n1) && tonumberns(v2, n2)) {  \
    pc++; setfltvalue(s2v(ra), fop(L, n1, n2));  \
  }}


/*
** Arithmetic operations over floats and others with register operands.
*/
#define op_arithf(L,fop) {  \
  savepc(L);  \
  TValue *v1 = vRB(i);  \
  TValue *v2 = vRC(i);  \
  op_arithf_aux(L, v1, v2, fop); }


/*
** Arithmetic operations with K operands for floats.
*/
#define op_arithfK(L,fop) {  \
  savepc(L);  \
  TValue *v1 = vRB(i);  \
  TValue *v2 = KC(i); hello_assert(ttisnumber(v2));  \
  op_arithf_aux(L, v1, v2, fop); }


/*
** Arithmetic operations over integers and floats.
*/
#define op_arith_aux(L,v1,v2,iop,fop) {  \
  if (ttisinteger(v1) && ttisinteger(v2)) {  \
    hello_Integer i1 = ivalue(v1); hello_Integer i2 = ivalue(v2);  \
    pc++; setivalue(s2v(ra), iop(L, i1, i2));  \
  }  \
  else op_arithf_aux(L, v1, v2, fop); }


/*
** Arithmetic operations with register operands.
*/
#define op_arith(L,iop,fop) {  \
  savepc(L);  \
  TValue *v1 = vRB(i);  \
  TValue *v2 = vRC(i);  \
  op_arith_aux(L, v1, v2, iop, fop); }


/*
** Arithmetic operations with K operands.
*/
#define op_arithK(L,iop,fop) {  \
  savepc(L);  \
  TValue *v1 = vRB(i);  \
  TValue *v2 = KC(i); hello_assert(ttisnumber(v2));  \
  op_arith_aux(L, v1, v2, iop, fop); }


/*
** Bitwise operations with constant operand.
*/
#define op_bitwiseK(L,op) {  \
  savepc(L);  \
  TValue *v1 = vRB(i);  \
  TValue *v2 = KC(i);  \
  hello_Integer i1;  \
  hello_Integer i2 = ivalue(v2);  \
  if (tointegerns(v1, &i1)) {  \
    pc++; setivalue(s2v(ra), op(i1, i2));  \
  }}


/*
** Bitwise operations with register operands.
*/
#define op_bitwise(L,op) {  \
  savepc(L);  \
  TValue *v1 = vRB(i);  \
  TValue *v2 = vRC(i);  \
  hello_Integer i1; hello_Integer i2;  \
  if (tointegerns(v1, &i1) && tointegerns(v2, &i2)) {  \
    pc++; setivalue(s2v(ra), op(i1, i2));  \
  }}


/*
** Order operations with register operands. 'opn' actually works
** for all numbers, but the fast track improves performance for
** integers.
*/
#define op_order(L,opi,opn,other) {  \
        int cond;  \
        TValue *rb = vRB(i);  \
        if (ttisinteger(s2v(ra)) && ttisinteger(rb)) {  \
          hello_Integer ia = ivalue(s2v(ra));  \
          hello_Integer ib = ivalue(rb);  \
          cond = opi(ia, ib);  \
        }  \
        else if (ttisnumber(s2v(ra)) && ttisnumber(rb))  \
          cond = opn(s2v(ra), rb);  \
        else  \
          Protect(cond = other(L, s2v(ra), rb));  \
        docondjump(); }


/*
** Order operations with immediate operand. (Immediate operand is
** always small enough to have an exact representation as a float.)
*/
#define op_orderI(L,opi,opf,inv,tm) {  \
        int cond;  \
        int im = GETARG_sB(i);  \
        if (ttisinteger(s2v(ra)))  \
          cond = opi(ivalue(s2v(ra)), im);  \
        else if (ttisfloat(s2v(ra))) {  \
          hello_Number fa = fltvalue(s2v(ra));  \
          hello_Number fim = cast_num(im);  \
          cond = opf(fa, fim);  \
        }  \
        else {  \
          int isf = GETARG_C(i);  \
          Protect(cond = helloT_callorderiTM(L, s2v(ra), im, inv, isf, tm));  \
        }  \
        docondjump(); }

/* }================================================================== */


/*
** {==================================================================
** Function 'helloV_execute': main interpreter loop
** ===================================================================
*/

/*
** some macros for common tasks in 'helloV_execute'
*/


#define RA(i)	(base+GETARG_A(i))
#define RB(i)	(base+GETARG_B(i))
#define vRB(i)	s2v(RB(i))
#define KB(i)	(k+GETARG_B(i))
#define RC(i)	(base+GETARG_C(i))
#define vRC(i)	s2v(RC(i))
#define KC(i)	(k+GETARG_C(i))
#define RKC(i)	((TESTARG_k(i)) ? k + GETARG_C(i) : s2v(base + GETARG_C(i)))



#define updatetrap(ci)  (trap = ci->u.l.trap)

#define updatebase(ci)	(base = ci->func + 1)


#define updatestack(ci)  \
    { if (l_unlikely(trap)) { updatebase(ci); ra = RA(i); } }


/*
** Execute a jump instruction. The 'updatetrap' allows signals to stop
** tight loops. (Without it, the local copy of 'trap' could never change.)
*/
#define dojump(ci,i,e)	{ pc += GETARG_sJ(i) + e; updatetrap(ci); }


/* for test instructions, execute the jump instruction that follows it */
#define donextjump(ci)	{ Instruction ni = *pc; dojump(ci, ni, 1); }

/*
** do a conditional jump: skip next instruction if 'cond' is not what
** was expected (parameter 'k'), else do next instruction, which must
** be a jump.
*/
#define docondjump()	if (cond != GETARG_k(i)) pc++; else donextjump(ci);


/*
** Correct global 'pc'.
*/
#define savepc(L)	(ci->u.l.savedpc = pc)


/*
** Whenever code can raise errors, the global 'pc' and the global
** 'top' must be correct to report occasional errors.
*/
#define savestate(L,ci)		(savepc(L), L->top = ci->top)


/*
** Protect code that, in general, can raise errors, reallocate the
** stack, and change the hooks.
*/
#define Protect(exp)  (savestate(L,ci), (exp), updatetrap(ci))

/* special version that does not change the top */
#define ProtectNT(exp)  (savepc(L), (exp), updatetrap(ci))

/*
** Protect code that can only raise errors. (That is, it cannot change
** the stack or hooks.)
*/
#define halfProtect(exp)  (savestate(L,ci), (exp))

/* 'c' is the limit of live values in the stack */
#define checkGC(L,c)  \
    { helloC_condGC(L, (savepc(L), L->top = (c)), \
                         updatetrap(ci)); \
           helloi_threadyield(L); }


/* fetch an instruction and prepare its execution */
#define vmfetch()	{ \
  if (l_unlikely(trap)) {  /* stack reallocation or hooks? */ \
    trap = helloG_traceexec(L, pc);  /* handle hooks */ \
    updatebase(ci);  /* correct stack */ \
  } \
  i = *(pc++); \
  ra = RA(i); /* WARNING: any stack reallocation invalidates 'ra' */ \
}

#define vmdispatch(o)	switch(o)
#define vmcase(l)	case l:
#define vmbreak		break


/*
** Optimization:
**     Discovered by Xmilia Hermit, Jun 7, 2022.
**     "It works by checking in the OP_TFORPREP opcode for the pairs and ipairs
**      case and use the to-be-closed slot for an index variable in the pairs
**      and a marker in the ipairs case.
**      This allows to easily check in the OP_TFORCALL opcode for the index or
**      marker and use the pairs or ipairs fast path or fall back to the default
**      path in other cases.
**      Furthermore, the patch can not be observed (except for a speedup). In
**      case of debug.getlocal it will return nil instead of the special values
**      and deoptimize the loop in case any of the state variables are modified
**      with debug.setlocal."
**
** Speed-ups upwards of 5x have been observed on my machine.
** Furthermore, this optimzation is safe. It only occurs when the TBC variable is ignored.
*/
HELLOI_FUNC int helloB_next (hello_State *L);
HELLOI_FUNC int helloB_ipairsaux (hello_State *L);


#ifdef HELLO_VMDUMP
#include <vector>

[[nodiscard]] static std::string stringify_ttype(const TValue* t) noexcept
{
  std::ostringstream str { };

  switch (ttype(t))
  {
    case HELLO_TNIL:
      str << "nil";
      break;
    case HELLO_TBOOLEAN: 
      str << "boolean";
      break;
    case HELLO_TLIGHTUSERDATA: 
      str << "lightuserdata";
      break;
    case HELLO_TNUMBER:
      str << "number";
      break;
    case HELLO_TSTRING:
      str << "string";
      break;
    case HELLO_TTABLE:
      str << "table";
      break;
    case HELLO_TFUNCTION:
      str << "function";
      break;
    case HELLO_TUSERDATA:
      str << "userdata";
      break;
    case HELLO_TTHREAD:
      str << "thread";
      break;
    default:
      str << "unknown";
  }
  
  str << "-";
  str << (void*)t;

  return str.str();
}


inline void padUntilGoal(std::string& s, const size_t goal) noexcept
{
  while (s.length() < goal) s += " ";
}


[[nodiscard]] static std::string stringify_tvalue(const TValue* o) noexcept
{
  std::string str { };

  switch (ttype(o))
  {
    case HELLO_TSTRING:
      str.push_back('"');
      str.append(svalue(o));
      str.push_back('"');
      break;
    case HELLO_TNUMBER:
      if (ttisinteger(o))
        str += std::to_string(ivalue(o));
      else
        str += std::to_string(nvalue(o));
      break;
    case HELLO_TBOOLEAN:
      str += ttistrue(o) ? "true" : "false";
      break;
    default:
      str += stringify_ttype(o);
  }

  return str;
}

// The compiler didn't like including 'lopcodes.h' in 'helloconf.h'.
static const std::vector<OpCode> ignoreOps = { vmDumpIgnore };
static const std::vector<OpCode> allowOps = { vmDumpAllow };

#ifdef HELLO_VMDUMP_WHITELIST
#define vmDumpInit() \
  bool ignore = true; \
  OpCode opcode = GET_OPCODE(i); \
  std::string tmp; \
  for (size_t idx = 0; idx < allowOps.size(); idx++) \
    if (allowOps.at(idx) == opcode) { tmp = opnames[opcode]; padUntilGoal(tmp, 11); ignore = false; break; }
#else
#define vmDumpInit() \
  bool ignore = false; \
  OpCode opcode = GET_OPCODE(i); \
  std::string tmp; \
  for (size_t idx = 0; idx < ignoreOps.size(); idx++) \
    if (ignoreOps.at(idx) == opcode) { ignore = true; break; } \
  if (!ignore) { tmp = opnames[opcode]; padUntilGoal(tmp, 11); }
#endif
  
#define vmDumpAddA() if (!ignore) { tmp += std::to_string(GETARG_A(i)); tmp += " "; }
#define vmDumpAddB() if (!ignore) { tmp += std::to_string(GETARG_B(i)); tmp += " "; }
#define vmDumpAddC() if (!ignore) { tmp += std::to_string(GETARG_C(i)); tmp += " "; }
#define vmDumpAdd(o) if (!ignore) { tmp += std::to_string(o);           tmp += " "; }
#define vmDumpOut(c) if (!ignore) { padUntilGoal(tmp, 20); std::stringstream cs; cs << c; tmp.append(cs.str()); hello_writestring(tmp.data(), tmp.size()); hello_writeline(); }
#else
#define vmDumpInit()
#define vmDumpAddA()
#define vmDumpAddB()
#define vmDumpAddC()
#define vmDumpAdd(o)
#define vmDumpOut(c)
#endif  /* HELLO_VMDUMP */


#if !defined(__GNUC__) && defined(HELLO_FORCE_JUMPTABLE)
#include "ljumptab.h"
#endif

void helloV_execute (hello_State *L, CallInfo *ci) {
  LClosure *cl;
  TValue *k;
  StkId base;
  const Instruction *pc;
  int trap;
#ifdef HELLO_ILP_ENABLE
  int sequentialJumps = 0;
#endif
#ifdef HELLO_ETL_ENABLE
  std::time_t deadline = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()
                         + HELLO_ETL_NANOS;
#endif
#if defined(__GNUC__)
#include "ljumptabgcc.h"
#endif
 startfunc:
  trap = L->hookmask;
 returning:  /* trap already set */
  cl = clLvalue(s2v(ci->func));
  k = cl->p->k;
  pc = ci->u.l.savedpc;
  if (l_unlikely(trap)) {
    if (pc == cl->p->code) {  /* first instruction (not resuming)? */
      if (cl->p->is_vararg)
        trap = 0;  /* hooks will start after VARARGPREP instruction */
      else  /* check 'call' hook */
        helloD_hookcall(L, ci);
    }
    ci->u.l.trap = 1;  /* assume trap is on, for now */
  }
  base = ci->func + 1;
  /* main loop of interpreter */
  for (;;) {
    Instruction i;  /* instruction being executed */
    StkId ra;  /* instruction's A register */
    vmfetch();
    hello_assert(isIT(i) || (cast_void(L->top = base), 1));
    vmdispatch (GET_OPCODE(i)) {
      vmcase(OP_MOVE) {
        setobjs2s(L, ra, RB(i));
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpOut ("; move " << stringify_tvalue(vRB(i)) << " into R(A)");
        vmbreak;
      }
      vmcase(OP_LOADI) {
        hello_Integer b = GETARG_sBx(i);
        setivalue(s2v(ra), b);
        vmDumpInit();
        vmDumpAddA();
        vmDumpAdd (GETARG_sBx(i));
        vmDumpOut ("; push " << b);
        vmbreak;
      }
      vmcase(OP_LOADF) {
        int b = GETARG_sBx(i);
        setfltvalue(s2v(ra), cast_num(b));
        vmDumpInit();
        vmDumpAddA();
        vmDumpAdd (GETARG_sBx(i));
        vmDumpOut ("; push " << b << ".0");
        vmbreak;
      }
      vmcase(OP_LOADK) {
        TValue *rb = k + GETARG_Bx(i);
        setobj2s(L, ra, rb);
        vmDumpInit();
        vmDumpAddA();
        vmDumpAdd (GETARG_Bx(i));
        vmDumpOut ("; push " << stringify_tvalue(rb));
        vmbreak;
      }
      vmcase(OP_LOADKX) {
        TValue *rb;
        rb = k + GETARG_Ax(*pc); pc++;
        setobj2s(L, ra, rb);
        vmDumpInit();
        vmDumpAdd (GETARG_Ax(*pc));
        vmDumpOut ("; push " << stringify_tvalue(rb) << " & skip next opcode");
        vmbreak;
      }
      vmcase(OP_LOADFALSE) {
        setbfvalue(s2v(ra));
        vmDumpInit();
        vmDumpAddA();
        vmDumpOut ("; push false");
        vmbreak;
      }
      vmcase(OP_LFALSESKIP) {
        setbfvalue(s2v(ra));
        pc++;  /* skip next instruction */
        vmDumpInit();
        vmDumpAddA();
        vmDumpOut ("; push false & skip next opcode");
        vmbreak;
      }
      vmcase(OP_LOADTRUE) {
        setbtvalue(s2v(ra));
        vmDumpInit();
        vmDumpAddA();
        vmDumpOut ("; push true");
        vmbreak;
      }
      vmcase(OP_LOADNIL) {
        int b = GETARG_B(i);
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpOut ("; push " << b << " nil value(s)");
        do {
          setnilvalue(s2v(ra++));
        } while (b--);
        vmbreak;
      }
      vmcase(OP_GETUPVAL) {
        int b = GETARG_B(i);
        setobj2s(L, ra, cl->upvals[b]->v);
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpOut ("; push " << stringify_tvalue(cl->upvals[b]->v));
        vmbreak;
      }
      vmcase(OP_SETUPVAL) {
        UpVal *uv = cl->upvals[GETARG_B(i)];
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpOut ("; old=" << stringify_tvalue(uv->v) << " new=" << stringify_tvalue(s2v(ra)));
        setobj(L, uv->v, s2v(ra));
        helloC_barrier(L, uv, s2v(ra));
        vmbreak;
      }
      vmcase(OP_GETTABUP) {
        const TValue *slot;
        TValue *upval = cl->upvals[GETARG_B(i)]->v;
        TValue *rc = KC(i);
        TString *key = tsvalue(rc);  /* key must be a string */
        if (helloV_fastget(L, upval, key, slot, helloH_getshortstr)) {
          setobj2s(L, ra, slot);
        }
        else
          Protect(helloV_finishget(L, upval, rc, ra, slot));
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; push T['" << getstr(key) << "'] for " << stringify_tvalue(s2v(ra)) << " (T=" << stringify_tvalue(upval) << ")");
        vmbreak;
      }
      vmcase(OP_GETTABLE) {
        const TValue *slot;
        TValue *rb = vRB(i);
        TValue *rc = vRC(i);
        hello_Unsigned n;
        if (ttisinteger(rc)  /* fast track for integers? */
            ? (cast_void(n = ivalue(rc)), helloV_fastgeti(L, rb, n, slot))
            : helloV_fastget(L, rb, rc, slot, helloH_get)) {
          setobj2s(L, ra, slot);
        }
        else
          Protect(helloV_finishget(L, rb, rc, ra, slot));
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; push T[" << stringify_tvalue(rc) << "] where T=" << stringify_tvalue(rb))
        vmbreak;
      }
      vmcase(OP_GETI) {
        const TValue *slot;
        TValue *rb = vRB(i);
        int c = GETARG_C(i);
        if (helloV_fastgeti(L, rb, c, slot)) {
          setobj2s(L, ra, slot);
        }
        else {
          TValue key;
          setivalue(&key, c);
          Protect(helloV_finishget(L, rb, &key, ra, slot));
        }
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; push T[" << c << "] for " << stringify_tvalue(rb));
        vmbreak;
      }
      vmcase(OP_GETFIELD) {
        const TValue *slot;
        TValue *rb = vRB(i);
        TValue *rc = KC(i);
        TString *key = tsvalue(rc);  /* key must be a string */
        if (helloV_fastget(L, rb, key, slot, helloH_getshortstr)) {
          setobj2s(L, ra, slot);
        }
        else
          Protect(helloV_finishget(L, rb, rc, ra, slot));
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; push T['" << getstr(key) << "'] for " << stringify_tvalue(rb));
        vmbreak;
      }
      vmcase(OP_SETTABUP) {
        const TValue *slot;
        TValue *upval = cl->upvals[GETARG_A(i)]->v;
        TValue *rb = KB(i);
        TValue *rc = RKC(i);
        TString *key = tsvalue(rb);  /* key must be a string */
        if (ttistable(upval)) {  // R(A) may not be a table.
          Table *t = hvalue(upval);
          t->length = 0;
          if (t->isfrozen) {
            savepc(L);
            helloG_runerror(L, "attempt to modify frozen table.");
          }
        }
        if (helloV_fastget(L, upval, key, slot, helloH_getshortstr)) {
          helloV_finishfastset(L, upval, slot, rc);
        }
        else {
          Protect(helloV_finishset(L, upval, rb, rc, slot));
        }
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; T['" << getstr(key) << "'] = " << stringify_tvalue(rc) << " (T=" << stringify_tvalue(upval) << ")");
        vmbreak;
      }
      vmcase(OP_SETTABLE) {
        const TValue *slot;
        TValue *rb = vRB(i);  /* key (table is in 'ra') */
        TValue *rc = RKC(i);  /* value */
        hello_Unsigned n;
        if (ttistable(s2v(ra))) {
          Table *t = hvalue(s2v(ra));
          t->length = 0; // Reset length cache.
          if (t->isfrozen) {
            savepc(L);
            helloG_runerror(L, "attempt to modify frozen table.");
          }
        }
        if (ttisinteger(rb)  /* fast track for integers? */
            ? (cast_void(n = ivalue(rb)), helloV_fastgeti(L, s2v(ra), n, slot))
            : helloV_fastget(L, s2v(ra), rb, slot, helloH_get)) {
          helloV_finishfastset(L, s2v(ra), slot, rc);
        }
        else
          Protect(helloV_finishset(L, s2v(ra), rb, rc, slot));
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; T[" << stringify_tvalue(rb) << "] = " << stringify_tvalue(rc) << " (T=" << stringify_tvalue(s2v(ra)) << ")");
        vmbreak;
      }
      vmcase(OP_SETI) {
        const TValue *slot;
        int c = GETARG_B(i);
        TValue *rc = RKC(i);
        if (ttistable(s2v(ra))) {
          Table *t = hvalue(s2v(ra));
          if (t->isfrozen) {
            savepc(L);
            helloG_runerror(L, "attempt to modify frozen table.");
          }
          t->length = 0; // Reset length cache.
        }
        if (helloV_fastgeti(L, s2v(ra), c, slot)) {
          helloV_finishfastset(L, s2v(ra), slot, rc);
        }
        else {
          TValue key;
          setivalue(&key, c);
          Protect(helloV_finishset(L, s2v(ra), &key, rc, slot));
        }
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; T[" << c << "] = " << stringify_tvalue(rc) << " (T=" << stringify_tvalue(s2v(ra)) << ")");
        vmbreak;
      }
      vmcase(OP_SETFIELD) {
        const TValue *slot;
        TValue *rb = KB(i);
        TValue *rc = RKC(i);
        TString *key = tsvalue(rb);  /* key must be a string */
        if (ttistable(s2v(ra)) && hvalue(s2v(ra))->isfrozen) {
          savepc(L);
          helloG_runerror(L, "attempt to modify frozen table.");
        }
        if (helloV_fastget(L, s2v(ra), key, slot, helloH_getshortstr)) {
          helloV_finishfastset(L, s2v(ra), slot, rc);
        }
        else
          Protect(helloV_finishset(L, s2v(ra), rb, rc, slot));
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; T['" << getstr(key) << "'] = " << stringify_tvalue(rc) << " (T=" << stringify_tvalue(s2v(ra)) << ")");
        vmbreak;
      }
      vmcase(OP_NEWTABLE) {
        int b = GETARG_B(i);  /* log2(hash size) + 1 */
        int c = GETARG_C(i);  /* array size */
        Table *t;
        if (b > 0)
          b = 1 << (b - 1);  /* size is 2^(b - 1) */
        hello_assert((!TESTARG_k(i)) == (GETARG_Ax(*pc) == 0));
        if (TESTARG_k(i))  /* non-zero extra argument? */
          c += GETARG_Ax(*pc) * (MAXARG_C + 1);  /* add it to size */
        pc++;  /* skip extra argument */
        L->top = ra + 1;  /* correct top in case of emergency GC */
        t = helloH_new(L);  /* memory allocation */
        sethvalue2s(L, ra, t);
        if (b != 0 || c != 0)
          helloH_resize(L, t, c, b);  /* idem */
        checkGC(L, ra + 1);
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; arraysize=" << c << " hashsize=" << b);
        vmbreak;
      }
      vmcase(OP_SELF) {
        const TValue *slot;
        TValue *rb = vRB(i);
        TValue *rc = RKC(i);
        TString *key = tsvalue(rc);  /* key must be a string */
        setobj2s(L, ra + 1, rb);
        if (helloV_fastget(L, rb, key, slot, helloH_getstr)) {
          setobj2s(L, ra, slot);
        }
        else
          Protect(helloV_finishget(L, rb, rc, ra, slot));
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; push self to call '" << getstr(key) << "'");
        vmbreak;
      }
      vmcase(OP_ADDI) {
        vmDumpInit();
        vmDumpAddA();
        vmDumpAdd (GETARG_sC(i));
        vmDumpOut ("; push " << stringify_tvalue(vRB(i)) << " + " << GETARG_sC(i));
        op_arithI(L, l_addi, helloi_numadd);
        vmbreak;
      }
      vmcase(OP_ADDK) {
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; push " << stringify_tvalue(vRB(i)) << " + " << stringify_tvalue(KC(i)));
        op_arithK(L, l_addi, helloi_numadd);
        vmbreak;
      }
      vmcase(OP_SUBK) {
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; push " << stringify_tvalue(vRB(i)) << " - " << stringify_tvalue(KC(i)));
        op_arithK(L, l_subi, helloi_numsub);
        vmbreak;
      }
      vmcase(OP_MULK) {
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; push " << stringify_tvalue(vRB(i)) << " * " << stringify_tvalue(KC(i)));
        op_arithK(L, l_muli, helloi_nummul);
        vmbreak;
      }
      vmcase(OP_MODK) {
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; push " << stringify_tvalue(vRB(i)) << " % " << stringify_tvalue(KC(i)));
        op_arithK(L, helloV_mod, helloV_modf);
        vmbreak;
      }
      vmcase(OP_POWK) {
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; push " << stringify_tvalue(vRB(i)) << " ^ " << stringify_tvalue(KC(i)));
        op_arithfK(L, helloi_numpow);
        vmbreak;
      }
      vmcase(OP_DIVK) {
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; push " << stringify_tvalue(vRB(i)) << " / " << stringify_tvalue(KC(i)));
        op_arithfK(L, helloi_numdiv);
        vmbreak;
      }
      vmcase(OP_IDIVK) {
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; push " << stringify_tvalue(vRB(i)) << " // " << stringify_tvalue(KC(i)));
        op_arithK(L, helloV_idiv, helloi_numidiv);
        vmbreak;
      }
      vmcase(OP_BANDK) {
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; push " << stringify_tvalue(vRB(i)) << " & " << stringify_tvalue(KC(i)));
        op_bitwiseK(L, l_band);
        vmbreak;
      }
      vmcase(OP_BORK) {
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; push " << stringify_tvalue(vRB(i)) << " | " << stringify_tvalue(KC(i)));
        op_bitwiseK(L, l_bor);
        vmbreak;
      }
      vmcase(OP_BXORK) {
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; push " << stringify_tvalue(vRB(i)) << " ~ " << stringify_tvalue(KC(i)));
        op_bitwiseK(L, l_bxor);
        vmbreak;
      }
      vmcase(OP_SHRI) {
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAdd (GETARG_sC(i));
        vmDumpOut ("; push " << stringify_tvalue(vRB(i)) << " >> " << GETARG_sC(i));
        TValue *rb = vRB(i);
        int ic = GETARG_sC(i);
        hello_Integer ib;
        if (tointegerns(rb, &ib)) {
          pc++; setivalue(s2v(ra), helloV_shiftl(ib, -ic));
        }
        vmbreak;
      }
      vmcase(OP_SHLI) {
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; push " << GETARG_sC(i) << " << " << stringify_tvalue(vRB(i)));
        TValue *rb = vRB(i);
        int ic = GETARG_sC(i);
        hello_Integer ib;
        if (tointegerns(rb, &ib)) {
          pc++; setivalue(s2v(ra), helloV_shiftl(ic, ib));
        }
        vmbreak;
      }
      vmcase(OP_ADD) {
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; push " << stringify_tvalue(vRB(i)) << " + " << stringify_tvalue(vRC(i)));
        op_arith(L, l_addi, helloi_numadd);
        vmbreak;
      }
      vmcase(OP_SUB) {
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; push " << stringify_tvalue(vRB(i)) << " - " << stringify_tvalue(vRC(i)));
        op_arith(L, l_subi, helloi_numsub);
        vmbreak;
      }
      vmcase(OP_MUL) {
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; push " << stringify_tvalue(vRB(i)) << " * " << stringify_tvalue(vRC(i)));
        op_arith(L, l_muli, helloi_nummul);
        vmbreak;
      }
      vmcase(OP_MOD) {
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; push " << stringify_tvalue(vRB(i)) << " % " << stringify_tvalue(vRC(i)));
        op_arith(L, helloV_mod, helloV_modf);
        vmbreak;
      }
      vmcase(OP_POW) {
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; push " << stringify_tvalue(vRB(i)) << " ^ " << stringify_tvalue(vRC(i)));
        op_arithf(L, helloi_numpow);
        vmbreak;
      }
      vmcase(OP_DIV) {  /* float division (always with floats) */
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; push " << stringify_tvalue(vRB(i)) << " / " << stringify_tvalue(vRC(i)));
        op_arithf(L, helloi_numdiv);
        vmbreak;
      }
      vmcase(OP_IDIV) {  /* floor division */
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; push " << stringify_tvalue(vRB(i)) << " // " << stringify_tvalue(vRC(i)));
        op_arith(L, helloV_idiv, helloi_numidiv);
        vmbreak;
      }
      vmcase(OP_BAND) {
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; push " << stringify_tvalue(vRB(i)) << " & " << stringify_tvalue(vRC(i)));
        op_bitwise(L, l_band);
        vmbreak;
      }
      vmcase(OP_BOR) {
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; push " << stringify_tvalue(vRB(i)) << " | " << stringify_tvalue(vRC(i)));
        op_bitwise(L, l_bor);
        vmbreak;
      }
      vmcase(OP_BXOR) {
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; push " << stringify_tvalue(vRB(i)) << " ~ " << stringify_tvalue(vRC(i)));
        op_bitwise(L, l_bxor);
        vmbreak;
      }
      vmcase(OP_SHR) {
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; push " << stringify_tvalue(vRB(i)) << " >> " << stringify_tvalue(vRC(i)));
        op_bitwise(L, helloV_shiftr);
        vmbreak;
      }
      vmcase(OP_SHL) {
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; push " << stringify_tvalue(vRB(i)) << " << " << stringify_tvalue(vRC(i)));
        op_bitwise(L, helloV_shiftl);
        vmbreak;
      }
      vmcase(OP_MMBIN) {
        Instruction pi = *(pc - 2);  /* original arith. expression */
        TValue *rb = vRB(i);
        TMS tm = (TMS)GETARG_C(i);
        StkId result = RA(pi);
        hello_assert(OP_ADD <= GET_OPCODE(pi) && GET_OPCODE(pi) <= OP_SHR);
        Protect(helloT_trybinTM(L, s2v(ra), rb, result, tm));
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; call " << helloT_eventname[tm] << " over " << stringify_tvalue(s2v(ra)) << " & " << stringify_tvalue(rb));
        vmbreak;
      }
      vmcase(OP_MMBINI) {
        Instruction pi = *(pc - 2);  /* original arith. expression */
        int imm = GETARG_sB(i);
        TMS tm = (TMS)GETARG_C(i);
        int flip = GETARG_k(i);
        StkId result = RA(pi);
        Protect(helloT_trybiniTM(L, s2v(ra), imm, flip, result, tm));
        vmDumpInit();
        vmDumpAddA();
        vmDumpAdd (GETARG_sB(i));
        vmDumpAddC();
        vmDumpOut ("; call " << helloT_eventname[tm] << " over " << stringify_tvalue(s2v(ra)) << " & " << imm);
        vmbreak;
      }
      vmcase(OP_MMBINK) {
        Instruction pi = *(pc - 2);  /* original arith. expression */
        TValue *imm = KB(i);
        TMS tm = (TMS)GETARG_C(i);
        int flip = GETARG_k(i);
        StkId result = RA(pi);
        Protect(helloT_trybinassocTM(L, s2v(ra), imm, flip, result, tm));
        vmDumpInit();
        vmDumpAddA();
        vmDumpAdd (GETARG_sB(i));
        vmDumpAddC();
        vmDumpOut ("; call " << helloT_eventname[tm] << " over " << stringify_tvalue(s2v(ra)) << " & " << stringify_tvalue(KB(i)) << " (k=" << GETARG_k(i) << ")");
        vmbreak;
      }
      vmcase(OP_UNM) {
        TValue *rb = vRB(i);
        hello_Number nb;
        if (ttisinteger(rb)) {
          hello_Integer ib = ivalue(rb);
          setivalue(s2v(ra), intop(-, 0, ib));
        }
        else if (tonumberns(rb, nb)) {
          setfltvalue(s2v(ra), helloi_numunm(L, nb));
        }
        else
          Protect(helloT_trybinTM(L, rb, rb, ra, TM_UNM));
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpOut ("; push -(" << stringify_tvalue(rb) << ")");
        vmbreak;
      }
      vmcase(OP_BNOT) {
        TValue *rb = vRB(i);
        hello_Integer ib;
        if (tointegerns(rb, &ib)) {
          setivalue(s2v(ra), intop(^, ~l_castS2U(0), ib));
        }
        else
          Protect(helloT_trybinTM(L, rb, rb, ra, TM_BNOT));
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpOut ("; push ~(" << stringify_tvalue(rb) << ")");
        vmbreak;
      }
      vmcase(OP_NOT) {
        TValue *rb = vRB(i);
        if (l_isfalse(rb))
          setbtvalue(s2v(ra));
        else
          setbfvalue(s2v(ra));
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpOut ("; push " << stringify_tvalue(s2v(ra)) << " (operand=" << stringify_tvalue(rb) << ")");
        vmbreak;
      }
      vmcase(OP_LEN) {
        Protect(helloV_objlen(L, ra, vRB(i)));
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpOut ("; push #(" << stringify_tvalue(vRB(i)) << ") (value=" << stringify_tvalue(s2v(ra)) << ")");
        vmbreak;
      }
      vmcase(OP_CONCAT) {
        int n = GETARG_B(i);  /* number of elements to concatenate */
        L->top = ra + n;  /* mark the end of concat operands */
        ProtectNT(helloV_concat(L, n));
        checkGC(L, L->top); /* 'helloV_concat' ensures correct top */
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpOut ("; concat the last " << n << " elements on the stack");
        vmbreak;
      }
      vmcase(OP_CLOSE) {
        vmDumpInit();
        vmDumpAddA();
        vmDumpOut ("; close all upvalues with an ID >= " << ivalue(s2v(ra)));
        Protect(helloF_close(L, ra, HELLO_OK, 1));
        vmbreak;
      }
      vmcase(OP_TBC) {
        /* create new to-be-closed upvalue */
        halfProtect(helloF_newtbcupval(L, ra));
        vmDumpInit();
        vmDumpAddA();
        vmDumpOut ("; turn R(A) into a TBC upvalue");
        vmbreak;
      }
      vmcase(OP_JMP) {
        int offset = GETARG_sJ(i);
#ifdef HELLO_ILP_ENABLE
        if (offset <= 0) {
          sequentialJumps++;
        }
        else sequentialJumps = 0;
        if (sequentialJumps >= HELLO_ILP_MAX_ITERATIONS) {
          sequentialJumps = 0;
#ifndef HELLO_ILP_SILENT_BREAK
          helloG_runerror(L, "infinite loop detected (exceeded max iterations: %d)", HELLO_ILP_MAX_ITERATIONS);
#endif
          vmbreak;
        }
#endif // HELLO_ILP_ENABLE
        pc += offset;
        updatetrap(ci);
        vmDumpInit();
        vmDumpAdd (offset);
        vmDumpOut ("; offset=" << offset << " newpc=" << pc);
        vmbreak;
      }
      vmcase(OP_EQ) {
        int cond;
        TValue *rb = vRB(i);
        Protect(cond = helloV_equalobj(L, s2v(ra), rb));
        docondjump();
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAdd (GETARG_k(i));
        vmDumpOut ("; " << stringify_tvalue(s2v(ra)) << " == " << stringify_tvalue(rb));
        vmbreak;
      }
      vmcase(OP_LT) {
        op_order(L, l_lti, LTnum, lessthanothers);
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAdd (GETARG_k(i));
        vmDumpOut ("; " << stringify_tvalue(s2v(ra)) << " < " << stringify_tvalue(vRB(i)));
        vmbreak;
      }
      vmcase(OP_LE) {
        op_order(L, l_lei, LEnum, lessequalothers);
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAdd (GETARG_k(i));
        vmDumpOut ("; " << stringify_tvalue(s2v(ra)) << " <= " << stringify_tvalue(vRB(i)));
        vmbreak;
      }
      vmcase(OP_EQK) {
        TValue *rb = KB(i);
        /* basic types do not use '__eq'; we can use raw equality */
        int cond = helloV_rawequalobj(s2v(ra), rb);
        docondjump();
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAdd (GETARG_k(i));
        vmDumpOut ("; " << stringify_tvalue(s2v(ra)) << " == " << stringify_tvalue(rb));
        vmbreak;
      }
      vmcase(OP_EQI) {
        int cond;
        int im = GETARG_sB(i);
        if (ttisinteger(s2v(ra)))
          cond = (ivalue(s2v(ra)) == im);
        else if (ttisfloat(s2v(ra)))
          cond = helloi_numeq(fltvalue(s2v(ra)), cast_num(im));
        else
          cond = 0;  /* other types cannot be equal to a number */
        docondjump();
        vmDumpInit();
        vmDumpAddA();
        vmDumpAdd (GETARG_sB(i));
        vmDumpAdd (GETARG_k(i));
        vmDumpOut ("; " << stringify_tvalue(s2v(ra)) << " == " << im);
        vmbreak;
      }
      vmcase(OP_LTI) {
        op_orderI(L, l_lti, helloi_numlt, 0, TM_LT);
        vmDumpInit();
        vmDumpAddA();
        vmDumpAdd (GETARG_sB(i));
        vmDumpAdd (GETARG_k(i));
        vmDumpOut ("; " << stringify_tvalue(s2v(ra)) << " <= " << GETARG_sB(i));
        vmbreak;
      }
      vmcase(OP_LEI) {
        op_orderI(L, l_lei, helloi_numle, 0, TM_LE);
        vmDumpInit();
        vmDumpAddA();
        vmDumpAdd (GETARG_sB(i));
        vmDumpAdd (GETARG_k(i));
        vmDumpOut ("; " << stringify_tvalue(s2v(ra)) << " < " << GETARG_sB(i));
        vmbreak;
      }
      vmcase(OP_GTI) {
        op_orderI(L, l_gti, helloi_numgt, 1, TM_LT);
        vmDumpInit();
        vmDumpAddA();
        vmDumpAdd (GETARG_sB(i));
        vmDumpAdd (GETARG_k(i));
        vmDumpOut ("; " << stringify_tvalue(s2v(ra)) << " > " << GETARG_sB(i));
        vmbreak;
      }
      vmcase(OP_GEI) {
        op_orderI(L, l_gei, helloi_numge, 1, TM_LE);
        vmDumpInit();
        vmDumpAddA();
        vmDumpAdd (GETARG_sB(i));
        vmDumpAdd (GETARG_k(i));
        vmDumpOut ("; " << stringify_tvalue(s2v(ra)) << " >= " << GETARG_sB(i));
        vmbreak;
      }
      vmcase(OP_TEST) {
        int cond = !l_isfalse(s2v(ra));
#ifdef HELLO_ILP_ENABLE
        int offset = GETARG_sJ(i);
        if (offset <= 0) {
          sequentialJumps++;
        }
        else sequentialJumps = 0;
        if (sequentialJumps >= HELLO_ILP_MAX_ITERATIONS) {
          sequentialJumps = 0;
#ifndef HELLO_ILP_SILENT_BREAK
          helloG_runerror(L, "infinite loop detected (exceeded max iterations: %d)", HELLO_ILP_MAX_ITERATIONS);
#endif
          vmbreak;
        }
#endif // HELLO_ILP_ENABLE
        docondjump();
        vmDumpInit();
        vmDumpAddA();
        vmDumpAdd (GETARG_k(i));
        vmDumpOut (";");
        vmbreak;
      }
      vmcase(OP_TESTSET) {
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpAdd (GETARG_k(i));
        if (GETARG_C(i) == NULL_COALESCE) { /* R(C) is used as an identifier, as it was previously unused. */
          TValue *rb = vRB(i);
          if (ttisnil(rb)) {
            pc++;
            vmDumpOut("; null coalesce, no assignment");
          }
          else {
            setobj2s(L, ra, rb);
            donextjump(ci);
            vmDumpOut("; null coalesce, push/assign " << stringify_tvalue(rb));
          }
        }
        else {
          TValue *rb = vRB(i);
          if (l_isfalse(rb) == GETARG_k(i))
          {
            pc++;
            vmDumpOut("; no assignment");
          }
          else {
            setobj2s(L, ra, rb);
            donextjump(ci);
            vmDumpOut("; push/assign " << stringify_tvalue(rb));
          }
        }
        vmbreak;
      }
      vmcase(OP_CALL) {
        CallInfo *newci;
        int b = GETARG_B(i);
        int nresults = GETARG_C(i) - 1;
        if (b != 0)  /* fixed number of arguments? */
          L->top = ra + b;  /* top signals number of arguments */
        /* else previous instruction set top */
        savepc(L);  /* in case of errors */
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
#ifdef HELLO_ILP_HOOK_FUNCTION
        if (fvalue(s2v(ra)) == HELLO_ILP_HOOK_FUNCTION) sequentialJumps = 0;
#endif
        if ((newci = helloD_precall(L, ra, nresults)) == NULL)  /* C call; nothing else to be done */
        {
          updatetrap(ci);
          vmDumpOut("; call cfunc (nresults=" << nresults << " nparams=" << nresults << ")");
        }
        else  /* Hello call: run function in this same C frame */
        {
          vmDumpOut("; call lfunc (nresults=" << nresults << " nparams=" << nresults << ")");
          ci = newci;
          goto startfunc;
        }
        vmbreak;
      }
      vmcase(OP_TAILCALL) {
        int b = GETARG_B(i);  /* number of arguments + 1 (function) */
        int n;  /* number of results when calling a C function */
        int nparams1 = GETARG_C(i);
        /* delta is virtual 'func' - real 'func' (vararg functions) */
        int delta = (nparams1) ? ci->u.l.nextraargs + nparams1 : 0;
        if (b != 0)
          L->top = ra + b;
        else  /* previous instruction set top */
          b = cast_int(L->top - ra);
        savepc(ci);  /* several calls here can raise errors */
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        if (TESTARG_k(i)) {
          helloF_closeupval(L, base);  /* close upvalues from current call */
          hello_assert(L->tbclist < base);  /* no pending tbc variables */
          hello_assert(base == ci->func + 1);
        }
        if ((n = helloD_pretailcall(L, ci, ra, b, delta)) < 0)
        {
          vmDumpOut("; tailcall lfunc (nresults=" << n << " nparams=" << b - 1 << ")");
          goto startfunc;  /* execute the callee */
        }
        else
        {
          vmDumpOut("; tailcall cfunc (nresults=" << n << " nparams=" << b - 1 << ")");
          ci->func -= delta;  /* restore 'func' (if vararg) */
          helloD_poscall(L, ci, n);  /* finish caller */
          updatetrap(ci);  /* 'helloD_poscall' can change hooks */
          goto ret;  /* caller returns after the tail call */
        }
      }
      vmcase(OP_RETURN) {
        int n = GETARG_B(i) - 1;  /* number of results */
        int nparams1 = GETARG_C(i);
        if (n < 0)  /* not fixed? */
          n = cast_int(L->top - ra);  /* get what is available */
        savepc(ci);
        if (TESTARG_k(i)) {  /* may there be open upvalues? */
          ci->u2.nres = n;  /* save number of returns */
          if (L->top < ci->top)
            L->top = ci->top;
          helloF_close(L, base, CLOSEKTOP, 1);
          updatetrap(ci);
          updatestack(ci);
        }
        if (nparams1)  /* vararg function? */
          ci->func -= ci->u.l.nextraargs + nparams1;
        L->top = ra + n;  /* set call for 'helloD_poscall' */
        helloD_poscall(L, ci, n);
        updatetrap(ci);  /* 'helloD_poscall' can change hooks */
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; return " << n << " value(s)");
        goto ret;
      }
      vmcase(OP_RETURN0) {
        if (l_unlikely(L->hookmask)) {
          L->top = ra;
          savepc(ci);
          helloD_poscall(L, ci, 0);  /* no hurry... */
          trap = 1;
        }
        else {  /* do the 'poscall' here */
          int nres;
          L->ci = ci->previous;  /* back to caller */
          L->top = base - 1;
          for (nres = ci->nresults; l_unlikely(nres > 0); nres--)
            setnilvalue(s2v(L->top++));  /* all results are nil */
        }
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; return nothing");
        goto ret;
      }
      vmcase(OP_RETURN1) {
#ifdef HELLO_VMDUMP
        if (true)  /* Jump to label 'ret' crosses initalization with vmDumpInit. */
        {
          vmDumpInit();
          vmDumpAddA();
          vmDumpAddB();
          vmDumpAddC();
          vmDumpOut ("; return " << stringify_tvalue(s2v(ra)));
        }
#endif
        if (l_unlikely(L->hookmask)) {
          L->top = ra + 1;
          savepc(ci);
          helloD_poscall(L, ci, 1);  /* no hurry... */
          trap = 1;
        }
        else {  /* do the 'poscall' here */
          int nres = ci->nresults;
          L->ci = ci->previous;  /* back to caller */
          if (nres == 0)
            L->top = base - 1;  /* asked for no results */
          else {
            setobjs2s(L, base - 1, ra);  /* at least this result */
            L->top = base;
            for (; l_unlikely(nres > 1); nres--)
              setnilvalue(s2v(L->top++));  /* complete missing results */
          }
        }
       ret:  /* return from a Hello function */
        if (ci->callstatus & CIST_FRESH)
          return;  /* end this frame */
        else {
          ci = ci->previous;
          goto returning;  /* continue running caller in this frame */
        }
      }
      vmcase(OP_FORLOOP) {
        if (ttisinteger(s2v(ra + 2))) {  /* integer loop? */
          hello_Unsigned count = l_castS2U(ivalue(s2v(ra + 1)));
          if (count > 0) {  /* still more iterations? */
            hello_Integer step = ivalue(s2v(ra + 2));
            hello_Integer idx = ivalue(s2v(ra));  /* internal index */
            chgivalue(s2v(ra + 1), count - 1);  /* update counter */
            idx = intop(+, idx, step);  /* add step to index */
            chgivalue(s2v(ra), idx);  /* update internal index */
            setivalue(s2v(ra + 3), idx);  /* and control variable */
            pc -= GETARG_Bx(i);  /* jump back */
          }
        }
        else if (floatforloop(ra))  /* float loop */
          pc -= GETARG_Bx(i);  /* jump back */
        updatetrap(ci);  /* allows a signal to break the loop */
        vmDumpInit();
        vmDumpAddA();
        vmDumpAdd (GETARG_Bx(i));
        vmDumpOut ("; update loop: curr=" << stringify_tvalue(s2v(ra + 3)) << " step=" << stringify_tvalue(s2v(ra + 2)));
        vmbreak;
      }
      vmcase(OP_FORPREP) {
        vmDumpInit();
        vmDumpAddA();
        vmDumpAdd (GETARG_Bx(i));
        savestate(L, ci);  /* in case of errors */
        if (forprep(L, ra))
        {
          pc += GETARG_Bx(i) + 1;  /* skip the loop */
          vmDumpOut("; this loop is skipped");
        }
#ifdef HELLO_VMDUMP
        else
        {
          vmDumpOut("; prepare loop");
        }
#endif
        vmbreak;
      }
      vmcase(OP_TFORPREP) {
        vmDumpInit();
        vmDumpAddA();
        vmDumpAdd (GETARG_Bx(i));
        vmDumpOut (";");
        const Instruction* callpc = pc + GETARG_Bx(i);
        i = *callpc;
        if ((!ttisfunction(s2v(ra)))) {
          setobjs2s(L, ra + 1, ra);
          setfvalue(s2v(ra), helloB_next);
        }
        if (ttypetag(s2v(ra)) == HELLO_VLCF
              && ttistable(s2v(ra+1))
              && ttisnil(s2v(ra+3))
              && !trap
              && (GETARG_C(i) == 1 || GETARG_C(i) == 2)
        ) {
          if (fvalue(s2v(ra)) == helloB_next && ttisnil(s2v(ra + 2))) {
            settt_(s2v(ra + 3), HELLO_VITER);
            val_(s2v(ra + 3)).it = 0;
          } else if (fvalue(s2v(ra)) == helloB_ipairsaux && ttisinteger(s2v(ra + 2))) {
            settt_(s2v(ra + 3), HELLO_VITERI);
          } else {
            /* create to-be-closed upvalue (if needed) */
            halfProtect(helloF_newtbcupval(L, ra + 3));
          }
        } else {
          /* create to-be-closed upvalue (if needed) */
          halfProtect(helloF_newtbcupval(L, ra + 3));
        }
        pc = callpc + 1;
        hello_assert(GET_OPCODE(i) == OP_TFORCALL && ra == RA(i));
        goto l_tforcall;
      }
      vmcase(OP_TFORCALL) {
#ifdef HELLO_VMDUMP
        if (true)
        {
          vmDumpInit();
          vmDumpAddA();
          vmDumpAddC();
          vmDumpOut (";");
        }
#endif
       l_tforcall:
        /* 'ra' has the iterator function, 'ra + 1' has the state,
           'ra + 2' has the control variable, and 'ra + 3' has the
           to-be-closed variable. The call will use the stack after
           these values (starting at 'ra + 4')
        */
        if (helloi_likely(ttypetag(s2v(ra + 3)) == HELLO_VITER)) {
          if (helloi_likely(!trap)) {
            Table *t = hvalue(s2v(ra + 1));
            unsigned int idx = val_(s2v(ra + 3)).it;
            unsigned int asize = helloH_realasize(t);

            i = *(pc++);  /* go to next instruction */
            hello_assert(GET_OPCODE(i) == OP_TFORLOOP && ra == RA(i));

            for (; idx < asize; idx++) {  /* try first array part */
              if (helloi_likely(!isempty(&t->array[idx]))) {  /* a non-empty entry? */
                setivalue(s2v(ra + 4), idx + 1);
                setobj2s(L, ra + 5, &t->array[idx]);
                goto l_tforcall_found;
              }
            }
            for (idx -= asize; cast_int(idx) < sizenode(t); idx++) {  /* hash part */
              Node *n = gnode(t, idx);
              if (helloi_likely(!isempty(gval(n)))) {  /* a non-empty entry? */
                getnodekey(L, s2v(ra + 4), n);
                setobj2s(L, ra + 5, gval(n));
                idx += asize;
                goto l_tforcall_found;
              }
            }
            vmbreak;
           l_tforcall_found:
            val_(s2v(ra + 3)).it = idx + 1;
            setobjs2s(L, ra + 2, ra + 4);  /* save control variable */
            pc -= GETARG_Bx(i);  /* jump back */
            vmbreak;
          }
          setnilvalue(s2v(ra + 3));
        } else if (helloi_likely(ttypetag(s2v(ra + 3)) == HELLO_VITERI)) {
          if (helloi_likely(!trap)) {
            /* No check for type as HELLO_VITERI is removed in case of debug setlocal. */
            Table *t = hvalue(s2v(ra + 1));
            hello_Integer n = ivalue(s2v(ra + 2));
            const TValue *slot;
            n = intop(+, n, 1);
            slot = helloi_likely(l_castS2U(n) - 1 < t->alimit) ? &t->array[n - 1] : helloH_getint(t, n);
            if (helloi_likely(!isempty(slot))) {
              setobj2s(L, ra + 5, slot);
              chgivalue(s2v(ra + 2), n);
              i = *(pc++);  /* go to next instruction */
              hello_assert(GET_OPCODE(i) == OP_TFORLOOP && ra == RA(i));
              setobjs2s(L, ra + 4, ra + 2);  /* save control variable */
              pc -= GETARG_Bx(i);  /* jump back */
              vmbreak;
            }
          } else {
            setnilvalue(s2v(ra + 3));
          }
        }
        /* push function, state, and control variable */
        memcpy(ra + 4, ra, 3 * sizeof(*ra));
        L->top = ra + 4 + 3;
        ProtectNT(helloD_call(L, ra + 4, GETARG_C(i)));  /* do the call */
        updatestack(ci);  /* stack may have changed */
        i = *(pc++);  /* go to next instruction */
        hello_assert(GET_OPCODE(i) == OP_TFORLOOP && ra == RA(i));
        goto l_tforloop;
      }
      vmcase(OP_TFORLOOP) {
#ifdef HELLO_VMDUMP
        if (true)
        {
          vmDumpInit();
          vmDumpAddA();
          vmDumpAdd (GETARG_Bx(i));
          vmDumpOut (";");
        }
#endif
        l_tforloop:
        if (!ttisnil(s2v(ra + 4))) {  /* continue loop? */
          setobjs2s(L, ra + 2, ra + 4);  /* save control variable */
          pc -= GETARG_Bx(i);  /* jump back */
        }
        vmbreak;
      }
      vmcase(OP_SETLIST) {
        int n = GETARG_B(i);
        unsigned int last = GETARG_C(i);
        Table *h = hvalue(s2v(ra));
        if (n == 0)
          n = cast_int(L->top - ra) - 1;  /* get up to the top */
        else
          L->top = ci->top;  /* correct top in case of emergency GC */
        last += n;
        if (TESTARG_k(i)) {
          last += GETARG_Ax(*pc) * (MAXARG_C + 1);
          pc++;
        }
        if (last > helloH_realasize(h))  /* needs more space? */
          helloH_resizearray(L, h, last);  /* preallocate it at once */
#ifdef HELLO_VMDUMP
        std::string rep;
#endif
        for (; n > 0; n--) {
          TValue *val = s2v(ra + n);
          setobj2t(L, &h->array[last - 1], val);
#ifdef HELLO_VMDUMP
          rep.insert(0, stringify_tvalue(val) + "; ");
#endif
          last--;
          helloC_barrierback(L, obj2gco(h), val);
        }
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; { " << rep << "}");
        vmbreak;
      }
      vmcase(OP_CLOSURE) {
        Proto *p = cl->p->p[GETARG_Bx(i)];
        halfProtect(pushclosure(L, p, cl->upvals, base, ra));
        checkGC(L, ra + 1);
        vmDumpInit();
        vmDumpAddA();
        vmDumpAdd (GETARG_Bx(i));
        vmDumpOut (";");
        vmbreak;
      }
      vmcase(OP_VARARG) {
        int n = GETARG_C(i) - 1;  /* required results */
        Protect(helloT_getvarargs(L, ci, ra, n));
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddC();
        vmDumpOut ("; get " << n << " varargs");
        vmbreak;
      }
      vmcase(OP_VARARGPREP) {
        ProtectNT(helloT_adjustvarargs(L, GETARG_A(i), ci, cl->p));
        if (l_unlikely(trap)) {  /* previous "Protect" updated trap */
          helloD_hookcall(L, ci);
          L->oldpc = 1;  /* next opcode will be seen as a "new" line */
        }
        updatebase(ci);  /* function has new base after adjustment */
        vmDumpInit();
        vmDumpAddA();
        vmDumpOut ("; prepare for " << GETARG_A(i) << " varargs");
        vmbreak;
      }
      vmcase(OP_EXTRAARG) {
        hello_assert(0);
        vmDumpInit();
        vmDumpAddA();
        vmDumpOut (";");
        vmbreak;
      }
      vmcase(OP_IN) {
        TValue *a = s2v(RA(i));
        TValue *b = vRB(i);
#ifdef HELLO_VMDUMP
        std::string old = stringify_tvalue(a);  /* RA will be changed below. */
#endif
        if (ttisstring(a) && ttisstring(b)) {
          if (strstr(svalue(b), svalue(a)) != nullptr)
            setbtvalue(s2v(ra));
          else
            setbfvalue(s2v(ra));
        } else {
          /* fetch table key */
          const TValue* slot;
          hello_Integer n;
          if (ttisinteger(a)
              ? (cast_void(n = ivalue(a)), helloV_fastgeti(L, b, n, slot))
              : helloV_fastget(L, b, a, slot, helloH_get)) {
            setobj2s(L, ra, slot);
          }
          else
            Protect(helloV_finishget(L, b, a, ra, slot));
          /* check if nil */
          if (ttisnil(s2v(ra)))
            setbfvalue(s2v(ra));
          else
            setbtvalue(s2v(ra));
        }
        vmDumpInit();
        vmDumpAddA();
        vmDumpAddB();
        vmDumpAddC();
        vmDumpOut ("; " << old << " in " << stringify_tvalue(b) << " (" << stringify_tvalue(s2v(ra)) << ")");
        vmbreak;
      }
      vmcase(NUM_OPCODES) {
        vmbreak;
      }
    }
#ifdef HELLO_ETL_ENABLE
    if (deadline < std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()) {
      HELLO_ETL_TIMESUP
      return;
    }
#endif
  }
}

/* }================================================================== */
