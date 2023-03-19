/*
** $Id: lmathlib.c $
** Standard mathematical library
** See Copyright Notice in hello.h
*/

#define lmathlib_c
#define HELLO_LIB

#include "lprefix.h"


#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "hello.h"

#include "lauxlib.h"
#include "hellolib.h"


#undef PI
#define PI	(l_mathop(3.141592653589793238462643383279502884))


static int math_abs (hello_State *L) {
  if (hello_isinteger(L, 1)) {
    hello_Integer n = hello_tointeger(L, 1);
    if (n < 0) n = (hello_Integer)(0u - (hello_Unsigned)n);
    hello_pushinteger(L, n);
  }
  else
    hello_pushnumber(L, l_mathop(fabs)(helloL_checknumber(L, 1)));
  return 1;
}

static int math_sin (hello_State *L) {
  hello_pushnumber(L, l_mathop(sin)(helloL_checknumber(L, 1)));
  return 1;
}

static int math_cos (hello_State *L) {
  hello_pushnumber(L, l_mathop(cos)(helloL_checknumber(L, 1)));
  return 1;
}

static int math_tan (hello_State *L) {
  hello_pushnumber(L, l_mathop(tan)(helloL_checknumber(L, 1)));
  return 1;
}

static int math_asin (hello_State *L) {
  hello_pushnumber(L, l_mathop(asin)(helloL_checknumber(L, 1)));
  return 1;
}

static int math_acos (hello_State *L) {
  hello_pushnumber(L, l_mathop(acos)(helloL_checknumber(L, 1)));
  return 1;
}

static int math_atan (hello_State *L) {
  hello_Number y = helloL_checknumber(L, 1);
  hello_Number x = helloL_optnumber(L, 2, 1);
  hello_pushnumber(L, l_mathop(atan2)(y, x));
  return 1;
}


static int math_toint (hello_State *L) {
  int valid;
  hello_Integer n = hello_tointegerx(L, 1, &valid);
  if (l_likely(valid))
    hello_pushinteger(L, n);
  else {
    helloL_checkany(L, 1);
    helloL_pushfail(L);  /* value is not convertible to integer */
  }
  return 1;
}


static void pushnumint (hello_State *L, hello_Number d) {
  hello_Integer n;
  if (hello_numbertointeger(d, &n))  /* does 'd' fit in an integer? */
    hello_pushinteger(L, n);  /* result is integer */
  else
    hello_pushnumber(L, d);  /* result is float */
}


static int math_floor (hello_State *L) {
  if (hello_isinteger(L, 1))
    hello_settop(L, 1);  /* integer is its own floor */
  else {
    hello_Number d = l_mathop(floor)(helloL_checknumber(L, 1));
    pushnumint(L, d);
  }
  return 1;
}


static int math_ceil (hello_State *L) {
  if (hello_isinteger(L, 1))
    hello_settop(L, 1);  /* integer is its own ceil */
  else {
    hello_Number d = l_mathop(ceil)(helloL_checknumber(L, 1));
    pushnumint(L, d);
  }
  return 1;
}


static int math_fmod (hello_State *L) {
  if (hello_isinteger(L, 1) && hello_isinteger(L, 2)) {
    hello_Integer d = hello_tointeger(L, 2);
    if ((hello_Unsigned)d + 1u <= 1u) {  /* special cases: -1 or 0 */
      helloL_argcheck(L, d != 0, 2, "zero");
      hello_pushinteger(L, 0);  /* avoid overflow with 0x80000... / -1 */
    }
    else
      hello_pushinteger(L, hello_tointeger(L, 1) % d);
  }
  else
    hello_pushnumber(L, l_mathop(fmod)(helloL_checknumber(L, 1),
                                     helloL_checknumber(L, 2)));
  return 1;
}


/*
** next function does not use 'modf', avoiding problems with 'double*'
** (which is not compatible with 'float*') when hello_Number is not
** 'double'.
*/
static int math_modf (hello_State *L) {
  if (hello_isinteger(L ,1)) {
    hello_settop(L, 1);  /* number is its own integer part */
    hello_pushnumber(L, 0);  /* no fractional part */
  }
  else {
    hello_Number n = helloL_checknumber(L, 1);
    /* integer part (rounds toward zero) */
    hello_Number ip = (n < 0) ? l_mathop(ceil)(n) : l_mathop(floor)(n);
    pushnumint(L, ip);
    /* fractional part (test needed for inf/-inf) */
    hello_pushnumber(L, (n == ip) ? l_mathop(0.0) : (n - ip));
  }
  return 2;
}


static int math_sqrt (hello_State *L) {
  hello_pushnumber(L, l_mathop(sqrt)(helloL_checknumber(L, 1)));
  return 1;
}


static int math_ult (hello_State *L) {
  hello_Integer a = helloL_checkinteger(L, 1);
  hello_Integer b = helloL_checkinteger(L, 2);
  hello_pushboolean(L, (hello_Unsigned)a < (hello_Unsigned)b);
  return 1;
}

static int math_log (hello_State *L) {
  hello_Number x = helloL_checknumber(L, 1);
  hello_Number res;
  if (hello_isnoneornil(L, 2))
    res = l_mathop(log)(x);
  else {
    hello_Number base = helloL_checknumber(L, 2);
#if !defined(HELLO_USE_C89)
    if (base == l_mathop(2.0))
      res = l_mathop(log2)(x);
    else
#endif
    if (base == l_mathop(10.0))
      res = l_mathop(log10)(x);
    else
      res = l_mathop(log)(x)/l_mathop(log)(base);
  }
  hello_pushnumber(L, res);
  return 1;
}

static int math_exp (hello_State *L) {
  hello_pushnumber(L, l_mathop(exp)(helloL_checknumber(L, 1)));
  return 1;
}

static int math_deg (hello_State *L) {
  hello_pushnumber(L, helloL_checknumber(L, 1) * (l_mathop(180.0) / PI));
  return 1;
}

static int math_rad (hello_State *L) {
  hello_pushnumber(L, helloL_checknumber(L, 1) * (PI / l_mathop(180.0)));
  return 1;
}


static int math_min (hello_State *L) {
  int n = hello_gettop(L);  /* number of arguments */
  int imin = 1;  /* index of current minimum value */
  int i;
  helloL_argcheck(L, n >= 1, 1, "value expected");
  for (i = 2; i <= n; i++) {
    if (hello_compare(L, i, imin, HELLO_OPLT))
      imin = i;
  }
  hello_pushvalue(L, imin);
  return 1;
}


static int math_max (hello_State *L) {
  int n = hello_gettop(L);  /* number of arguments */
  int imax = 1;  /* index of current maximum value */
  int i;
  helloL_argcheck(L, n >= 1, 1, "value expected");
  for (i = 2; i <= n; i++) {
    if (hello_compare(L, imax, i, HELLO_OPLT))
      imax = i;
  }
  hello_pushvalue(L, imax);
  return 1;
}


static int math_type (hello_State *L) {
  if (hello_type(L, 1) == HELLO_TNUMBER)
    hello_pushstring(L, (hello_isinteger(L, 1)) ? "integer" : "float");
  else {
    helloL_checkany(L, 1);
    helloL_pushfail(L);
  }
  return 1;
}



/*
** {==================================================================
** Pseudo-Random Number Generator based on 'xoshiro256**'.
** ===================================================================
*/

/* number of binary digits in the mantissa of a float */
#define FIGS	l_floatatt(MANT_DIG)

#if FIGS > 64
/* there are only 64 random bits; use them all */
#undef FIGS
#define FIGS	64
#endif


/*
** HELLO_RAND32 forces the use of 32-bit integers in the implementation
** of the PRN generator (mainly for testing).
*/
#if !defined(HELLO_RAND32) && !defined(Rand64)

/* try to find an integer type with at least 64 bits */

#if (ULONG_MAX >> 31 >> 31) >= 3

/* 'long' has at least 64 bits */
#define Rand64		unsigned long

#elif !defined(HELLO_USE_C89) && defined(LLONG_MAX)

/* there is a 'long long' type (which must have at least 64 bits) */
#define Rand64		unsigned long long

#elif (HELLO_MAXUNSIGNED >> 31 >> 31) >= 3

/* 'hello_Integer' has at least 64 bits */
#define Rand64		hello_Unsigned

#endif

#endif


#if defined(Rand64)  /* { */

/*
** Standard implementation, using 64-bit integers.
** If 'Rand64' has more than 64 bits, the extra bits do not interfere
** with the 64 initial bits, except in a right shift. Moreover, the
** final result has to discard the extra bits.
*/

/* avoid using extra bits when needed */
#define trim64(x)	((x) & 0xffffffffffffffffu)


/* rotate left 'x' by 'n' bits */
static Rand64 rotl (Rand64 x, int n) {
  return (x << n) | (trim64(x) >> (64 - n));
}

static Rand64 nextrand (Rand64 *state) {
  Rand64 state0 = state[0];
  Rand64 state1 = state[1];
  Rand64 state2 = state[2] ^ state0;
  Rand64 state3 = state[3] ^ state1;
  Rand64 res = rotl(state1 * 5, 7) * 9;
  state[0] = state0 ^ state3;
  state[1] = state1 ^ state2;
  state[2] = state2 ^ (state1 << 17);
  state[3] = rotl(state3, 45);
  return res;
}


/* must take care to not shift stuff by more than 63 slots */


/*
** Convert bits from a random integer into a float in the
** interval [0,1), getting the higher FIG bits from the
** random unsigned integer and converting that to a float.
*/

/* must throw out the extra (64 - FIGS) bits */
#define shift64_FIG	(64 - FIGS)

/* to scale to [0, 1), multiply by scaleFIG = 2^(-FIGS) */
#define scaleFIG	(l_mathop(0.5) / ((Rand64)1 << (FIGS - 1)))

static hello_Number I2d (Rand64 x) {
  return (hello_Number)(trim64(x) >> shift64_FIG) * scaleFIG;
}

/* convert a 'Rand64' to a 'hello_Unsigned' */
#define I2UInt(x)	((hello_Unsigned)trim64(x))

/* convert a 'hello_Unsigned' to a 'Rand64' */
#define Int2I(x)	((Rand64)(x))


#else	/* no 'Rand64'   }{ */

/* get an integer with at least 32 bits */
#if HELLOI_IS32INT
typedef unsigned int lu_int32;
#else
typedef unsigned long lu_int32;
#endif


/*
** Use two 32-bit integers to represent a 64-bit quantity.
*/
typedef struct Rand64 {
  lu_int32 h;  /* higher half */
  lu_int32 l;  /* lower half */
} Rand64;


/*
** If 'lu_int32' has more than 32 bits, the extra bits do not interfere
** with the 32 initial bits, except in a right shift and comparisons.
** Moreover, the final result has to discard the extra bits.
*/

/* avoid using extra bits when needed */
#define trim32(x)	((x) & 0xffffffffu)


/*
** basic operations on 'Rand64' values
*/

/* build a new Rand64 value */
static Rand64 packI (lu_int32 h, lu_int32 l) {
  Rand64 result;
  result.h = h;
  result.l = l;
  return result;
}

/* return i << n */
static Rand64 Ishl (Rand64 i, int n) {
  hello_assert(n > 0 && n < 32);
  return packI((i.h << n) | (trim32(i.l) >> (32 - n)), i.l << n);
}

/* i1 ^= i2 */
static void Ixor (Rand64 *i1, Rand64 i2) {
  i1->h ^= i2.h;
  i1->l ^= i2.l;
}

/* return i1 + i2 */
static Rand64 Iadd (Rand64 i1, Rand64 i2) {
  Rand64 result = packI(i1.h + i2.h, i1.l + i2.l);
  if (trim32(result.l) < trim32(i1.l))  /* carry? */
    result.h++;
  return result;
}

/* return i * 5 */
static Rand64 times5 (Rand64 i) {
  return Iadd(Ishl(i, 2), i);  /* i * 5 == (i << 2) + i */
}

/* return i * 9 */
static Rand64 times9 (Rand64 i) {
  return Iadd(Ishl(i, 3), i);  /* i * 9 == (i << 3) + i */
}

/* return 'i' rotated left 'n' bits */
static Rand64 rotl (Rand64 i, int n) {
  hello_assert(n > 0 && n < 32);
  return packI((i.h << n) | (trim32(i.l) >> (32 - n)),
               (trim32(i.h) >> (32 - n)) | (i.l << n));
}

/* for offsets larger than 32, rotate right by 64 - offset */
static Rand64 rotl1 (Rand64 i, int n) {
  hello_assert(n > 32 && n < 64);
  n = 64 - n;
  return packI((trim32(i.h) >> n) | (i.l << (32 - n)),
               (i.h << (32 - n)) | (trim32(i.l) >> n));
}

/*
** implementation of 'xoshiro256**' algorithm on 'Rand64' values
*/
static Rand64 nextrand (Rand64 *state) {
  Rand64 res = times9(rotl(times5(state[1]), 7));
  Rand64 t = Ishl(state[1], 17);
  Ixor(&state[2], state[0]);
  Ixor(&state[3], state[1]);
  Ixor(&state[1], state[2]);
  Ixor(&state[0], state[3]);
  Ixor(&state[2], t);
  state[3] = rotl1(state[3], 45);
  return res;
}


/*
** Converts a 'Rand64' into a float.
*/

/* an unsigned 1 with proper type */
#define UONE		((lu_int32)1)


#if FIGS <= 32

/* 2^(-FIGS) */
#define scaleFIG       (l_mathop(0.5) / (UONE << (FIGS - 1)))

/*
** get up to 32 bits from higher half, shifting right to
** throw out the extra bits.
*/
static hello_Number I2d (Rand64 x) {
  hello_Number h = (hello_Number)(trim32(x.h) >> (32 - FIGS));
  return h * scaleFIG;
}

#else	/* 32 < FIGS <= 64 */

/* must take care to not shift stuff by more than 31 slots */

/* 2^(-FIGS) = 1.0 / 2^30 / 2^3 / 2^(FIGS-33) */
#define scaleFIG  \
    (l_mathop(1.0) / (UONE << 30) / l_mathop(8.0) / (UONE << (FIGS - 33)))

/*
** use FIGS - 32 bits from lower half, throwing out the other
** (32 - (FIGS - 32)) = (64 - FIGS) bits
*/
#define shiftLOW	(64 - FIGS)

/*
** higher 32 bits go after those (FIGS - 32) bits: shiftHI = 2^(FIGS - 32)
*/
#define shiftHI		((hello_Number)(UONE << (FIGS - 33)) * l_mathop(2.0))


static hello_Number I2d (Rand64 x) {
  hello_Number h = (hello_Number)trim32(x.h) * shiftHI;
  hello_Number l = (hello_Number)(trim32(x.l) >> shiftLOW);
  return (h + l) * scaleFIG;
}

#endif


/* convert a 'Rand64' to a 'hello_Unsigned' */
static hello_Unsigned I2UInt (Rand64 x) {
  return ((hello_Unsigned)trim32(x.h) << 31 << 1) | (hello_Unsigned)trim32(x.l);
}

/* convert a 'hello_Unsigned' to a 'Rand64' */
static Rand64 Int2I (hello_Unsigned n) {
  return packI((lu_int32)(n >> 31 >> 1), (lu_int32)n);
}

#endif  /* } */


/*
** A state uses four 'Rand64' values.
*/
typedef struct {
  Rand64 s[4];
} RanState;


/*
** Project the random integer 'ran' into the interval [0, n].
** Because 'ran' has 2^B possible values, the projection can only be
** uniform when the size of the interval is a power of 2 (exact
** division). Otherwise, to get a uniform projection into [0, n], we
** first compute 'lim', the smallest Mersenne number not smaller than
** 'n'. We then project 'ran' into the interval [0, lim].  If the result
** is inside [0, n], we are done. Otherwise, we try with another 'ran',
** until we have a result inside the interval.
*/
static hello_Unsigned project (hello_Unsigned ran, hello_Unsigned n,
                             RanState *state) {
  if ((n & (n + 1)) == 0)  /* is 'n + 1' a power of 2? */
    return ran & n;  /* no bias */
  else {
    hello_Unsigned lim = n;
    /* compute the smallest (2^b - 1) not smaller than 'n' */
    lim |= (lim >> 1);
    lim |= (lim >> 2);
    lim |= (lim >> 4);
    lim |= (lim >> 8);
    lim |= (lim >> 16);
#if (HELLO_MAXUNSIGNED >> 31) >= 3
    lim |= (lim >> 32);  /* integer type has more than 32 bits */
#endif
    hello_assert((lim & (lim + 1)) == 0  /* 'lim + 1' is a power of 2, */
      && lim >= n  /* not smaller than 'n', */
      && (lim >> 1) < n);  /* and it is the smallest one */
    while ((ran &= lim) > n)  /* project 'ran' into [0..lim] */
      ran = I2UInt(nextrand(state->s));  /* not inside [0..n]? try again */
    return ran;
  }
}


static int math_random (hello_State *L) {
  hello_Integer low, up;
  hello_Unsigned p;
  RanState *state = (RanState *)hello_touserdata(L, hello_upvalueindex(1));
  Rand64 rv = nextrand(state->s);  /* next pseudo-random value */
  switch (hello_gettop(L)) {  /* check number of arguments */
    case 0: {  /* no arguments */
      hello_pushnumber(L, I2d(rv));  /* float between 0 and 1 */
      return 1;
    }
    case 1: {  /* only upper limit */
      low = 1;
      up = helloL_checkinteger(L, 1);
      if (up == 0) {  /* single 0 as argument? */
        hello_pushinteger(L, I2UInt(rv));  /* full random integer */
        return 1;
      }
      break;
    }
    case 2: {  /* lower and upper limits */
      low = helloL_checkinteger(L, 1);
      up = helloL_checkinteger(L, 2);
      break;
    }
    default: helloL_error(L, "wrong number of arguments");
  }
  /* random integer in the interval [low, up] */
  helloL_argcheck(L, low <= up, 1, "interval is empty");
  /* project random integer into the interval [0, up - low] */
  p = project(I2UInt(rv), (hello_Unsigned)up - (hello_Unsigned)low, state);
  hello_pushinteger(L, p + (hello_Unsigned)low);
  return 1;
}


static void setseed (hello_State *L, Rand64 *state,
                     hello_Unsigned n1, hello_Unsigned n2) {
  int i;
  state[0] = Int2I(n1);
  state[1] = Int2I(0xff);  /* avoid a zero state */
  state[2] = Int2I(n2);
  state[3] = Int2I(0);
  for (i = 0; i < 16; i++)
    nextrand(state);  /* discard initial values to "spread" seed */
  hello_pushinteger(L, n1);
  hello_pushinteger(L, n2);
}


/*
** Set a "random" seed. To get some randomness, use the current time
** and the address of 'L' (in case the machine does address space layout
** randomization).
*/
static void randseed (hello_State *L, RanState *state) {
  hello_Unsigned seed1 = (hello_Unsigned)time(NULL);
  hello_Unsigned seed2 = (hello_Unsigned)(size_t)L;
  setseed(L, state->s, seed1, seed2);
}


static int math_randomseed (hello_State *L) {
  RanState *state = (RanState *)hello_touserdata(L, hello_upvalueindex(1));
  if (hello_isnone(L, 1)) {
    randseed(L, state);
  }
  else {
    hello_Integer n1 = helloL_checkinteger(L, 1);
    hello_Integer n2 = helloL_optinteger(L, 2, 0);
    setseed(L, state->s, n1, n2);
  }
  return 2;  /* return seeds */
}


static const helloL_Reg randfuncs[] = {
  {"random", math_random},
  {"randomseed", math_randomseed},
  {NULL, NULL}
};


/*
** Register the random functions and initialize their state.
*/
static void setrandfunc (hello_State *L) {
  RanState *state = (RanState *)hello_newuserdatauv(L, sizeof(RanState), 0);
  randseed(L, state);  /* initialize with a "random" seed */
  hello_pop(L, 2);  /* remove pushed seeds */
  helloL_setfuncs(L, randfuncs, 1);

  // Provide "rand" as an alias to "random"
  hello_getfield(L, -1, "random");
  hello_setfield(L, -2, "rand");
}

/* }================================================================== */


/*
** {==================================================================
** Deprecated functions (for compatibility only)
** ===================================================================
*/
#if defined(HELLO_COMPAT_MATHLIB)

static int math_cosh (hello_State *L) {
  hello_pushnumber(L, l_mathop(cosh)(helloL_checknumber(L, 1)));
  return 1;
}

static int math_sinh (hello_State *L) {
  hello_pushnumber(L, l_mathop(sinh)(helloL_checknumber(L, 1)));
  return 1;
}

static int math_tanh (hello_State *L) {
  hello_pushnumber(L, l_mathop(tanh)(helloL_checknumber(L, 1)));
  return 1;
}

static int math_pow (hello_State *L) {
  hello_Number x = helloL_checknumber(L, 1);
  hello_Number y = helloL_checknumber(L, 2);
  hello_pushnumber(L, l_mathop(pow)(x, y));
  return 1;
}

static int math_frexp (hello_State *L) {
  int e;
  hello_pushnumber(L, l_mathop(frexp)(helloL_checknumber(L, 1), &e));
  hello_pushinteger(L, e);
  return 2;
}

static int math_ldexp (hello_State *L) {
  hello_Number x = helloL_checknumber(L, 1);
  int ep = (int)helloL_checkinteger(L, 2);
  hello_pushnumber(L, l_mathop(ldexp)(x, ep));
  return 1;
}

static int math_log10 (hello_State *L) {
  hello_pushnumber(L, l_mathop(log10)(helloL_checknumber(L, 1)));
  return 1;
}

#endif
/* }================================================================== */



static const helloL_Reg mathlib[] = {
  {"abs",   math_abs},
  {"acos",  math_acos},
  {"asin",  math_asin},
  {"atan",  math_atan},
  {"ceil",  math_ceil},
  {"cos",   math_cos},
  {"deg",   math_deg},
  {"exp",   math_exp},
  {"tointeger", math_toint},
  {"floor", math_floor},
  {"fmod",   math_fmod},
  {"ult",   math_ult},
  {"log",   math_log},
  {"max",   math_max},
  {"min",   math_min},
  {"modf",   math_modf},
  {"rad",   math_rad},
  {"sin",   math_sin},
  {"sqrt",  math_sqrt},
  {"tan",   math_tan},
  {"type", math_type},
#if defined(HELLO_COMPAT_MATHLIB)
  {"atan2", math_atan},
  {"cosh",   math_cosh},
  {"sinh",   math_sinh},
  {"tanh",   math_tanh},
  {"pow",   math_pow},
  {"frexp", math_frexp},
  {"ldexp", math_ldexp},
  {"log10", math_log10},
#endif
  /* placeholders */
  {"random", NULL},
  {"randomseed", NULL},
  {"pi", NULL},
  {"huge", NULL},
  {"maxinteger", NULL},
  {"mininteger", NULL},
  {NULL, NULL}
};


/*
** Open math library
*/
HELLOMOD_API int helloopen_math (hello_State *L) {
  helloL_newlib(L, mathlib);
  hello_pushnumber(L, PI);
  hello_setfield(L, -2, "pi");
  hello_pushnumber(L, (hello_Number)HUGE_VAL);
  hello_setfield(L, -2, "huge");
  hello_pushinteger(L, HELLO_MAXINTEGER);
  hello_setfield(L, -2, "maxinteger");
  hello_pushinteger(L, HELLO_MININTEGER);
  hello_setfield(L, -2, "mininteger");
  setrandfunc(L);
  return 1;
}

