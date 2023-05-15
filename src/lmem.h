#pragma once
/*
** $Id: lmem.h $
** Interface to Memory Manager
** See Copyright Notice in mask.h
*/

#include <stddef.h>

#include "llimits.h"
#include "mask.h"


#define maskM_error(L)	maskD_throw(L, MASK_ERRMEM)


/*
** This macro tests whether it is safe to multiply 'n' by the size of
** type 't' without overflows. Because 'e' is always constant, it avoids
** the runtime division MAX_SIZET/(e).
** (The macro is somewhat complex to avoid warnings:  The 'sizeof'
** comparison avoids a runtime comparison when overflow cannot occur.
** The compiler should be able to optimize the real test by itself, but
** when it does it, it may give a warning about "comparison is always
** false due to limited range of data type"; the +1 tricks the compiler,
** avoiding this warning but also this optimization.)
*/
#define maskM_testsize(n,e)  \
    (sizeof(n) >= sizeof(size_t) && cast_sizet((n)) + 1 > MAX_SIZET/(e))

#define maskM_checksize(L,n,e)  \
    (maskM_testsize(n,e) ? maskM_toobig(L) : cast_void(0))


/*
** Computes the minimum between 'n' and 'MAX_SIZET/sizeof(t)', so that
** the result is not larger than 'n' and cannot overflow a 'size_t'
** when multiplied by the size of type 't'. (Assumes that 'n' is an
** 'int' or 'unsigned int' and that 'int' is not larger than 'size_t'.)
*/
#define maskM_limitN(n,t)  \
  ((cast_sizet(n) <= MAX_SIZET/sizeof(t)) ? (n) :  \
     cast_uint((MAX_SIZET/sizeof(t))))


/*
** Arrays of chars do not need any test
*/
#define maskM_reallocvchar(L,b,on,n)  \
  cast_charp(maskM_saferealloc_(L, (b), (on)*sizeof(char), (n)*sizeof(char)))

#define maskM_freemem(L, b, s)	maskM_free_(L, (b), (s))
#define maskM_free(L, b)		maskM_free_(L, (b), sizeof(*(b)))
#define maskM_freearray(L, b, n)   maskM_free_(L, (b), (n)*sizeof(*(b)))

#define maskM_new(L,t)		cast(t*, maskM_malloc_(L, sizeof(t), 0))
#define maskM_newvector(L,n,t)	cast(t*, maskM_malloc_(L, (n)*sizeof(t), 0))
#define maskM_newvectorchecked(L,n,t) \
  (maskM_checksize(L,n,sizeof(t)), maskM_newvector(L,n,t))

#define maskM_newobject(L,tag,s)	maskM_malloc_(L, (s), tag)

#define maskM_growvector(L,v,nelems,size,t,limit,e) \
    ((v)=cast(t *, maskM_growaux_(L,v,nelems,&(size),sizeof(t), \
                         maskM_limitN(limit,t),e)))

#define maskM_reallocvector(L, v,oldn,n,t) \
   (cast(t *, maskM_realloc_(L, v, cast_sizet(oldn) * sizeof(t), \
                                  cast_sizet(n) * sizeof(t))))

#define maskM_shrinkvector(L,v,size,fs,t) \
   ((v)=cast(t *, maskM_shrinkvector_(L, v, &(size), fs, sizeof(t))))

[[noreturn]] MASKI_FUNC void maskM_toobig (mask_State *L);

/* not to be called directly */
MASKI_FUNC void *maskM_realloc_ (mask_State *L, void *block, size_t oldsize,
                                                          size_t size);
MASKI_FUNC void *maskM_saferealloc_ (mask_State *L, void *block, size_t oldsize,
                                                              size_t size);
MASKI_FUNC void maskM_free_ (mask_State *L, void *block, size_t osize);
MASKI_FUNC void *maskM_growaux_ (mask_State *L, void *block, int nelems,
                               int *size, int size_elem, int limit,
                               const char *what);
MASKI_FUNC void *maskM_shrinkvector_ (mask_State *L, void *block, int *nelem,
                                    int final_n, int size_elem);
MASKI_FUNC void *maskM_malloc_ (mask_State *L, size_t size, int tag);
