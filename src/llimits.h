#pragma once
/*
** $Id: llimits.h $
** Limits, basic types, and some other 'installation-dependent' definitions
** See Copyright Notice in mask.h
*/

#include <limits.h>
#include <stddef.h>


#include "mask.h"


/*
** 'lu_mem' and 'l_mem' are unsigned/signed integers big enough to count
** the total memory used by Mask (in bytes). Usually, 'size_t' and
** 'ptrdiff_t' should work, but we use 'long' for 16-bit machines.
*/
#if defined(MASKI_MEM)		/* { external definitions? */
typedef MASKI_UMEM lu_mem;
typedef MASKI_MEM l_mem;
#elif MASKI_IS32INT	/* }{ */
typedef size_t lu_mem;
typedef ptrdiff_t l_mem;
#else  /* 16-bit ints */	/* }{ */
typedef unsigned long lu_mem;
typedef long l_mem;
#endif				/* } */


/* chars used as small naturals (so that 'char' is reserved for characters) */
typedef unsigned char lu_byte;
typedef signed char ls_byte;


/* maximum value for size_t */
#define MAX_SIZET	((size_t)(~(size_t)0))

/* maximum size visible for Mask (must be representable in a mask_Integer) */
#define MAX_SIZE	(sizeof(size_t) < sizeof(mask_Integer) ? MAX_SIZET \
                          : (size_t)(MASK_MAXINTEGER))


#define MAX_LUMEM	((lu_mem)(~(lu_mem)0))

#define MAX_LMEM	((l_mem)(MAX_LUMEM >> 1))


#define MAX_INT		INT_MAX  /* maximum value of an int */


/*
** floor of the log2 of the maximum signed value for integral type 't'.
** (That is, maximum 'n' such that '2^n' fits in the given signed type.)
*/
#define log2maxs(t)	(sizeof(t) * 8 - 2)


/*
** test whether an unsigned value is a power of 2 (or zero)
*/
#define maskispow2(x)	(((x) & ((x) - 1)) == 0)


/* number of chars of a literal string without the ending \0 */
#define LL(x)   (sizeof(x)/sizeof(char) - 1)


/*
** conversion of pointer to unsigned integer:
** this is for hashing only; there is no problem if the integer
** cannot hold the whole pointer value
*/
#define point2uint(p)	((unsigned int)((size_t)(p) & UINT_MAX))



/* types of 'usual argument conversions' for mask_Number and mask_Integer */
typedef MASKI_UACNUMBER l_uacNumber;
typedef MASKI_UACINT l_uacInt;


/*
** Internal assertions for in-house debugging
*/
#if defined MASKI_ASSERT
#undef NDEBUG
#include <assert.h>
#define mask_assert(c)           assert(c)
#endif

#if defined(mask_assert)
#define check_exp(c,e)		(mask_assert(c), (e))
/* to avoid problems with conditions too long */
#define mask_longassert(c)	((c) ? (void)0 : mask_assert(0))
#else
#define mask_assert(c)		((void)0)
#define check_exp(c,e)		(e)
#define mask_longassert(c)	((void)0)
#endif

/*
** assertion for checking API calls
*/
#if !defined(maski_apicheck)
#define maski_apicheck(l,e)	((void)l, mask_assert(e))
#endif

#define api_check(l,e,msg)	maski_apicheck(l,(e) && msg)


/* macro to avoid warnings about unused variables */
#if !defined(UNUSED)
#define UNUSED(x)	((void)(x))
#endif


/* type casts (a macro highlights casts in the code) */
#define cast(t, exp)	((t)(exp))

#define cast_void(i)	cast(void, (i))
#define cast_voidp(i)	cast(void *, (i))
#define cast_num(i)	cast(mask_Number, (i))
#define cast_int(i)	cast(int, (i))
#define cast_uint(i)	cast(unsigned int, (i))
#define cast_byte(i)	cast(lu_byte, (i))
#define cast_uchar(i)	cast(unsigned char, (i))
#define cast_char(i)	cast(char, (i))
#define cast_charp(i)	cast(char *, (i))
#define cast_sizet(i)	cast(size_t, (i))


/* cast a signed mask_Integer to mask_Unsigned */
#if !defined(l_castS2U)
#define l_castS2U(i)	((mask_Unsigned)(i))
#endif

/*
** cast a mask_Unsigned to a signed mask_Integer; this cast is
** not strict ISO C, but two-complement architectures should
** work fine.
*/
#if !defined(l_castU2S)
#define l_castU2S(i)	((mask_Integer)(i))
#endif


/*
** Inline functions
*/
#if !defined(MASK_USE_C89)
#define l_inline	inline
#elif defined(__GNUC__)
#define l_inline	__inline__
#else
#define l_inline	/* empty */
#endif

#define l_sinline	static l_inline


/*
** type for virtual-machine instructions;
** must be an unsigned with (at least) 4 bytes (see details in lopcodes.h)
*/
#if MASKI_IS32INT
typedef unsigned int l_uint32;
#else
typedef unsigned long l_uint32;
#endif

typedef l_uint32 Instruction;



/*
** Maximum length for short strings, that is, strings that are
** internalized. (Cannot be smaller than reserved words or tags for
** metamethods, as these strings must be internalized;
** #("function") = 8, #("__newindex") = 10.)
*/
#if !defined(MASKI_MAXSHORTLEN)
#define MASKI_MAXSHORTLEN	40
#endif


/*
** Initial size for the string table (must be power of 2).
** The Mask core alone registers ~50 strings (reserved words +
** metaevent keys + a few others). Libraries would typically add
** a few dozens more.
*/
#if !defined(MINSTRTABSIZE)
#define MINSTRTABSIZE	128
#endif


/*
** Size of cache for strings in the API. 'N' is the number of
** sets (better be a prime) and "M" is the size of each set (M == 1
** makes a direct cache.)
*/
#if !defined(STRCACHE_N)
#define STRCACHE_N		53
#define STRCACHE_M		2
#endif


/* minimum size for string buffer */
#if !defined(MASK_MINBUFFER)
#define MASK_MINBUFFER	32
#endif


/*
** Maximum depth for nested C calls, syntactical nested non-terminals,
** and other features implemented through recursion in C. (Value must
** fit in a 16-bit unsigned integer. It must also be compatible with
** the size of the C stack.)
*/
#if !defined(MASKI_MAXCCALLS)
#define MASKI_MAXCCALLS		200
#endif


/*
** macros that are executed whenever program enters the Mask core
** ('mask_lock') and leaves the core ('mask_unlock')
*/
#if !defined(mask_lock)
#define mask_lock(L)	((void) 0)
#define mask_unlock(L)	((void) 0)
#endif

/*
** macro executed during Mask functions at points where the
** function can yield.
*/
#if !defined(maski_threadyield)
#define maski_threadyield(L)	{mask_unlock(L); mask_lock(L);}
#endif


/*
** these macros allow user-specific actions when a thread is
** created/deleted/resumed/yielded.
*/
#if !defined(maski_userstateopen)
#define maski_userstateopen(L)		((void)L)
#endif

#if !defined(maski_userstateclose)
#define maski_userstateclose(L)		((void)L)
#endif

#if !defined(maski_userstatethread)
#define maski_userstatethread(L,L1)	((void)L)
#endif

#if !defined(maski_userstatefree)
#define maski_userstatefree(L,L1)	((void)L)
#endif

#if !defined(maski_userstateresume)
#define maski_userstateresume(L,n)	((void)L)
#endif

#if !defined(maski_userstateyield)
#define maski_userstateyield(L,n)	((void)L)
#endif



/*
** The maski_num* macros define the primitive operations over numbers.
*/

/* floor division (defined as 'floor(a/b)') */
#if !defined(maski_numidiv)
#define maski_numidiv(L,a,b)     ((void)L, l_floor(maski_numdiv(L,a,b)))
#endif

/* float division */
#if !defined(maski_numdiv)
#define maski_numdiv(L,a,b)      ((a)/(b))
#endif

/*
** modulo: defined as 'a - floor(a/b)*b'; the direct computation
** using this definition has several problems with rounding errors,
** so it is better to use 'fmod'. 'fmod' gives the result of
** 'a - trunc(a/b)*b', and therefore must be corrected when
** 'trunc(a/b) ~= floor(a/b)'. That happens when the division has a
** non-integer negative result: non-integer result is equivalent to
** a non-zero remainder 'm'; negative result is equivalent to 'a' and
** 'b' with different signs, or 'm' and 'b' with different signs
** (as the result 'm' of 'fmod' has the same sign of 'a').
*/
#if !defined(maski_nummod)
#define maski_nummod(L,a,b,m)  \
  { (void)L; (m) = l_mathop(fmod)(a,b); \
    if (((m) > 0) ? (b) < 0 : ((m) < 0 && (b) > 0)) (m) += (b); }
#endif

/* exponentiation */
#if !defined(maski_numpow)
#define maski_numpow(L,a,b)  \
  ((void)L, (b == 2) ? (a)*(a) : l_mathop(pow)(a,b))
#endif

/* the others are quite standard operations */
#if !defined(maski_numadd)
#define maski_numadd(L,a,b)      ((a)+(b))
#define maski_numsub(L,a,b)      ((a)-(b))
#define maski_nummul(L,a,b)      ((a)*(b))
#define maski_numunm(L,a)        (-(a))
#define maski_numeq(a,b)         ((a)==(b))
#define maski_numlt(a,b)         ((a)<(b))
#define maski_numle(a,b)         ((a)<=(b))
#define maski_numgt(a,b)         ((a)>(b))
#define maski_numge(a,b)         ((a)>=(b))
#define maski_numisnan(a)        (!maski_numeq((a), (a)))
#endif





/*
** macro to control inclusion of some hard tests on stack reallocation
*/
#if !defined(HARDSTACKTESTS)
#define condmovestack(L,pre,pos)	((void)0)
#else
/* realloc stack keeping its size */
#define condmovestack(L,pre,pos)  \
  { int sz_ = stacksize(L); pre; maskD_reallocstack((L), sz_, 0); pos; }
#endif

#if !defined(HARDMEMTESTS)
#define condchangemem(L,pre,pos)	((void)0)
#else
#define condchangemem(L,pre,pos)  \
    { if (gcrunning(G(L))) { pre; maskC_fullgc(L, 0); pos; } }
#endif
