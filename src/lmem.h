#pragma once
/*
** $Id: lmem.h $
** Interface to Memory Manager
** See Copyright Notice in hello.h
*/

#include <stddef.h>

#include "llimits.h"
#include "hello.h"


#define helloM_error(L)	helloD_throw(L, HELLO_ERRMEM)


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
#define helloM_testsize(n,e)  \
    (sizeof(n) >= sizeof(size_t) && cast_sizet((n)) + 1 > MAX_SIZET/(e))

#define helloM_checksize(L,n,e)  \
    (helloM_testsize(n,e) ? helloM_toobig(L) : cast_void(0))


/*
** Computes the minimum between 'n' and 'MAX_SIZET/sizeof(t)', so that
** the result is not larger than 'n' and cannot overflow a 'size_t'
** when multiplied by the size of type 't'. (Assumes that 'n' is an
** 'int' or 'unsigned int' and that 'int' is not larger than 'size_t'.)
*/
#define helloM_limitN(n,t)  \
  ((cast_sizet(n) <= MAX_SIZET/sizeof(t)) ? (n) :  \
     cast_uint((MAX_SIZET/sizeof(t))))


/*
** Arrays of chars do not need any test
*/
#define helloM_reallocvchar(L,b,on,n)  \
  cast_charp(helloM_saferealloc_(L, (b), (on)*sizeof(char), (n)*sizeof(char)))

#define helloM_freemem(L, b, s)	helloM_free_(L, (b), (s))
#define helloM_free(L, b)		helloM_free_(L, (b), sizeof(*(b)))
#define helloM_freearray(L, b, n)   helloM_free_(L, (b), (n)*sizeof(*(b)))

#define helloM_new(L,t)		cast(t*, helloM_malloc_(L, sizeof(t), 0))
#define helloM_newvector(L,n,t)	cast(t*, helloM_malloc_(L, (n)*sizeof(t), 0))
#define helloM_newvectorchecked(L,n,t) \
  (helloM_checksize(L,n,sizeof(t)), helloM_newvector(L,n,t))

#define helloM_newobject(L,tag,s)	helloM_malloc_(L, (s), tag)

#define helloM_growvector(L,v,nelems,size,t,limit,e) \
    ((v)=cast(t *, helloM_growaux_(L,v,nelems,&(size),sizeof(t), \
                         helloM_limitN(limit,t),e)))

#define helloM_reallocvector(L, v,oldn,n,t) \
   (cast(t *, helloM_realloc_(L, v, cast_sizet(oldn) * sizeof(t), \
                                  cast_sizet(n) * sizeof(t))))

#define helloM_shrinkvector(L,v,size,fs,t) \
   ((v)=cast(t *, helloM_shrinkvector_(L, v, &(size), fs, sizeof(t))))

[[noreturn]] HELLOI_FUNC void helloM_toobig (hello_State *L);

/* not to be called directly */
HELLOI_FUNC void *helloM_realloc_ (hello_State *L, void *block, size_t oldsize,
                                                          size_t size);
HELLOI_FUNC void *helloM_saferealloc_ (hello_State *L, void *block, size_t oldsize,
                                                              size_t size);
HELLOI_FUNC void helloM_free_ (hello_State *L, void *block, size_t osize);
HELLOI_FUNC void *helloM_growaux_ (hello_State *L, void *block, int nelems,
                               int *size, int size_elem, int limit,
                               const char *what);
HELLOI_FUNC void *helloM_shrinkvector_ (hello_State *L, void *block, int *nelem,
                                    int final_n, int size_elem);
HELLOI_FUNC void *helloM_malloc_ (hello_State *L, size_t size, int tag);
