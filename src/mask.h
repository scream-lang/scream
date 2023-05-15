#pragma once
/*
** $Id: mask.h $
** Mask - A Scripting Language
** Mask.org, PUC-Rio, Brazil 
** See Copyright Notice at the end of this file
*/

#include <stdarg.h>
#include <stddef.h>
#include <string>


#include "maskconf.h"


#define MASK_VERSION "mask 0.0.1"

#define MASK_VERSION_MAJOR	"0"
#define MASK_VERSION_MINOR	"0"
#define MASK_VERSION_RELEASE	"1"

#define MASK_VERSION_NUM			504

#define MASK_VERSION	"Mask " MASK_VERSION_MAJOR "." MASK_VERSION_MINOR "." MASK_VERSION_RELEASE
#define MASK_RELEASE	MASK_VERSION "." MASK_VERSION_RELEASE
#define MASK_COPYRIGHT	MASK_VERSION " Copyright (C) 2019 - 2023 Timo Sarkar based on " "Lua 5.4 Copyright (C) 1994-2022 Lua.org, PUC-Rio"
#define MASK_AUTHORS	"Timo Sarkar, R. Ierusalimschy, L. H. de Figueiredo, W. Celes"


/* mark for precompiled code ('<esc>Mask') */
#define MASK_SIGNATURE	"\x1bMask"

/* option for multiple returns in 'mask_pcall' and 'mask_call' */
#define MASK_MULTRET	(-1)


/*
** Pseudo-indices
** (-MASKI_MAXSTACK is the minimum valid index; we keep some free empty
** space after that to help overflow detection)
*/
#define MASK_REGISTRYINDEX	(-MASKI_MAXSTACK - 1000)
#define mask_upvalueindex(i)	(MASK_REGISTRYINDEX - (i))


/* thread status */
#define MASK_OK		0
#define MASK_YIELD	1
#define MASK_ERRRUN	2
#define MASK_ERRSYNTAX	3
#define MASK_ERRMEM	4
#define MASK_ERRERR	5


typedef struct mask_State mask_State;


/*
** basic types
*/
#define MASK_TNONE		(-1)

#define MASK_TNIL		0
#define MASK_TBOOLEAN		1
#define MASK_TLIGHTUSERDATA	2
#define MASK_TNUMBER		3
#define MASK_TSTRING		4
#define MASK_TTABLE		5
#define MASK_TFUNCTION		6
#define MASK_TUSERDATA		7
#define MASK_TTHREAD		8

#define MASK_NUMTYPES		9



/* minimum Mask stack available to a C function */
#define MASK_MINSTACK	20


/* predefined values in the registry */
#define MASK_RIDX_MAINTHREAD	1
#define MASK_RIDX_GLOBALS	2
#define MASK_RIDX_LAST		MASK_RIDX_GLOBALS


/* type of numbers in Mask */
typedef MASK_NUMBER mask_Number;


/* type for integer functions */
typedef MASK_INTEGER mask_Integer;

/* unsigned integer type */
typedef MASK_UNSIGNED mask_Unsigned;

/* type for continuation-function contexts */
typedef MASK_KCONTEXT mask_KContext;


/*
** Type for C functions registered with Mask
*/
typedef int (*mask_CFunction) (mask_State *L);

/*
** Type for continuation functions
*/
typedef int (*mask_KFunction) (mask_State *L, int status, mask_KContext ctx);


/*
** Type for functions that read/write blocks when loading/dumping Mask chunks
*/
typedef const char * (*mask_Reader) (mask_State *L, void *ud, size_t *sz);

typedef int (*mask_Writer) (mask_State *L, const void *p, size_t sz, void *ud);


/*
** Type for memory-allocation functions
*/
typedef void * (*mask_Alloc) (void *ud, void *ptr, size_t osize, size_t nsize);


/*
** Type for warning functions
*/
typedef void (*mask_WarnFunction) (void *ud, const char *msg, int tocont);




/*
** generic extra include file
*/
#if defined(MASK_USER_H)
#include MASK_USER_H
#endif


/*
** RCS ident string
*/
extern const char mask_ident[];


/*
** state manipulation
*/
MASK_API mask_State *(mask_newstate) (mask_Alloc f, void *ud);
MASK_API void       (mask_close) (mask_State *L);
MASK_API mask_State *(mask_newthread) (mask_State *L);
MASK_API int        (mask_resetthread) (mask_State *L, mask_State *from);

MASK_API mask_CFunction (mask_atpanic) (mask_State *L, mask_CFunction panicf);


MASK_API mask_Number (mask_version) (mask_State *L);


/*
** basic stack manipulation
*/
MASK_API int   (mask_absindex) (mask_State *L, int idx);
MASK_API int   (mask_gettop) (mask_State *L);
MASK_API void  (mask_settop) (mask_State *L, int idx);
MASK_API void  (mask_pushvalue) (mask_State *L, int idx);
MASK_API void  (mask_rotate) (mask_State *L, int idx, int n);
MASK_API void  (mask_copy) (mask_State *L, int fromidx, int toidx);
MASK_API int   (mask_checkstack) (mask_State *L, int n);

MASK_API void  (mask_xmove) (mask_State *from, mask_State *to, int n);


/*
** access functions (stack -> C)
*/

MASK_API int             (mask_isnumber) (mask_State *L, int idx);
MASK_API int             (mask_isstring) (mask_State *L, int idx);
MASK_API int             (mask_iscfunction) (mask_State *L, int idx);
MASK_API int             (mask_isinteger) (mask_State *L, int idx);
MASK_API int             (mask_istrue) (mask_State *L, int idx) noexcept;
MASK_API int             (mask_isuserdata) (mask_State *L, int idx);
MASK_API int             (mask_type) (mask_State *L, int idx);
MASK_API const char     *(mask_typename) (mask_State *L, int tp);

MASK_API mask_Number      (mask_tonumberx) (mask_State *L, int idx, int *isnum);
MASK_API mask_Integer     (mask_tointegerx) (mask_State *L, int idx, int *isnum);
MASK_API int             (mask_toboolean) (mask_State *L, int idx);
MASK_API const char     *(mask_tolstring) (mask_State *L, int idx, size_t *len);
MASK_API mask_Unsigned    (mask_rawlen) (mask_State *L, int idx);
MASK_API mask_CFunction   (mask_tocfunction) (mask_State *L, int idx);
MASK_API void	       *(mask_touserdata) (mask_State *L, int idx);
MASK_API mask_State      *(mask_tothread) (mask_State *L, int idx);
MASK_API const void     *(mask_topointer) (mask_State *L, int idx);


/*
** Comparison and arithmetic functions
*/

#define MASK_OPADD	0	/* ORDER TM, ORDER OP */
#define MASK_OPSUB	1
#define MASK_OPMUL	2
#define MASK_OPMOD	3
#define MASK_OPPOW	4
#define MASK_OPDIV	5
#define MASK_OPIDIV	6
#define MASK_OPBAND	7
#define MASK_OPBOR	8
#define MASK_OPBXOR	9
#define MASK_OPSHL	10
#define MASK_OPSHR	11
#define MASK_OPUNM	12
#define MASK_OPBNOT	13

MASK_API void  (mask_arith) (mask_State *L, int op);

#define MASK_OPEQ	0
#define MASK_OPLT	1
#define MASK_OPLE	2

MASK_API int   (mask_rawequal) (mask_State *L, int idx1, int idx2);
MASK_API int   (mask_compare) (mask_State *L, int idx1, int idx2, int op);


/*
** push functions (C -> stack)
*/
MASK_API void        (mask_pushnil) (mask_State *L);
MASK_API void        (mask_pushnumber) (mask_State *L, mask_Number n);
MASK_API void        (mask_pushinteger) (mask_State *L, mask_Integer n);
MASK_API const char *(mask_pushlstring) (mask_State *L, const char *s, size_t len);
MASK_API const char *(mask_pushstring) (mask_State *L, const char *s);
MASK_API const char *(mask_pushstring) (mask_State* L, const std::string& str);
MASK_API const char *(mask_pushvfstring) (mask_State *L, const char *fmt,
                                                      va_list argp);
MASK_API const char *(mask_pushfstring) (mask_State *L, const char *fmt, ...);
MASK_API void  (mask_pushcclosure) (mask_State *L, mask_CFunction fn, int n);
MASK_API void  (mask_pushboolean) (mask_State *L, int b);
MASK_API void  (mask_pushlightuserdata) (mask_State *L, void *p);
MASK_API int   (mask_pushthread) (mask_State *L);


/*
** get functions (Mask -> stack)
*/
MASK_API int (mask_getglobal) (mask_State *L, const char *name);
MASK_API int (mask_gettable) (mask_State *L, int idx);
MASK_API int (mask_getfield) (mask_State *L, int idx, const char *k);
MASK_API int (mask_geti) (mask_State *L, int idx, mask_Integer n);
MASK_API int (mask_rawget) (mask_State *L, int idx);
MASK_API int (mask_rawgeti) (mask_State *L, int idx, mask_Integer n);
MASK_API int (mask_rawgetp) (mask_State *L, int idx, const void *p);

MASK_API void  (mask_createtable) (mask_State *L, int narr, int nrec);
MASK_API void *(mask_newuserdatauv) (mask_State *L, size_t sz, int nuvalue);
MASK_API int   (mask_getmetatable) (mask_State *L, int objindex);
MASK_API int  (mask_getiuservalue) (mask_State *L, int idx, int n);


/*
** set functions (stack -> Mask)
*/
MASK_API void  (mask_setglobal) (mask_State *L, const char *name);
MASK_API void  (mask_settable) (mask_State *L, int idx);
MASK_API void  (mask_setfield) (mask_State *L, int idx, const char *k);
MASK_API void  (mask_seti) (mask_State *L, int idx, mask_Integer n);
MASK_API void  (mask_rawset) (mask_State *L, int idx);
MASK_API void  (mask_rawseti) (mask_State *L, int idx, mask_Integer n);
MASK_API void  (mask_rawsetp) (mask_State *L, int idx, const void *p);
MASK_API int   (mask_setmetatable) (mask_State *L, int objindex);
MASK_API int   (mask_setiuservalue) (mask_State *L, int idx, int n);
MASK_API void  (mask_setcachelen) (mask_State *L, mask_Unsigned len, int idx);
MASK_API void  (mask_freezetable) (mask_State *L, int idx);
MASK_API int   (mask_istablefrozen) (mask_State *L, int idx);
MASK_API void  (mask_erriffrozen) (mask_State *L, int idx);


/*
** 'load' and 'call' functions (load and run Mask code)
*/
MASK_API void  (mask_callk) (mask_State *L, int nargs, int nresults,
                           mask_KContext ctx, mask_KFunction k);
#define mask_call(L,n,r)		mask_callk(L, (n), (r), 0, NULL)

MASK_API int   (mask_pcallk) (mask_State *L, int nargs, int nresults, int errfunc,
                            mask_KContext ctx, mask_KFunction k);
#define mask_pcall(L,n,r,f)	mask_pcallk(L, (n), (r), (f), 0, NULL)

MASK_API int   (mask_load) (mask_State *L, mask_Reader reader, void *dt,
                          const char *chunkname, const char *mode);

MASK_API int (mask_dump) (mask_State *L, mask_Writer writer, void *data, int strip);


/*
** coroutine functions
*/
MASK_API int  (mask_yieldk)     (mask_State *L, int nresults, mask_KContext ctx,
                               mask_KFunction k);
MASK_API int  (mask_resume)     (mask_State *L, mask_State *from, int narg,
                               int *nres);
MASK_API int  (mask_status)     (mask_State *L);
MASK_API int (mask_isyieldable) (mask_State *L);

#define mask_yield(L,n)		mask_yieldk(L, (n), 0, NULL)


/*
** Warning-related functions
*/
MASK_API void (mask_setwarnf) (mask_State *L, mask_WarnFunction f, void *ud);
MASK_API void (mask_warning)  (mask_State *L, const char *msg, int tocont);


/*
** garbage-collection function and options
*/

#define MASK_GCSTOP		0
#define MASK_GCRESTART		1
#define MASK_GCCOLLECT		2
#define MASK_GCCOUNT		3
#define MASK_GCCOUNTB		4
#define MASK_GCSTEP		5
#define MASK_GCSETPAUSE		6
#define MASK_GCSETSTEPMUL	7
#define MASK_GCISRUNNING		9
#define MASK_GCGEN		10
#define MASK_GCINC		11

MASK_API int (mask_gc) (mask_State *L, int what, ...);


/*
** miscellaneous functions
*/

[[noreturn]] MASK_API void   (mask_error) (mask_State *L);

MASK_API int   (mask_next) (mask_State *L, int idx);

MASK_API void  (mask_concat) (mask_State *L, int n);
MASK_API void  (mask_len)    (mask_State *L, int idx);

MASK_API size_t   (mask_stringtonumber) (mask_State *L, const char *s);

MASK_API mask_Alloc (mask_getallocf) (mask_State *L, void **ud);
MASK_API void      (mask_setallocf) (mask_State *L, mask_Alloc f, void *ud);

MASK_API void (mask_toclose) (mask_State *L, int idx);
MASK_API void (mask_closeslot) (mask_State *L, int idx);


/*
** {==============================================================
** some useful macros
** ===============================================================
*/

#define mask_getextraspace(L)	((void *)((char *)(L) - MASK_EXTRASPACE))

#define mask_tonumber(L,i)	mask_tonumberx(L,(i),NULL)
#define mask_tointeger(L,i)	mask_tointegerx(L,(i),NULL)

#define mask_pop(L,n)		mask_settop(L, -(n)-1)

#define mask_newtable(L)		mask_createtable(L, 0, 0)

#define mask_register(L,n,f) (mask_pushcfunction(L, (f)), mask_setglobal(L, (n)))

#define mask_pushcfunction(L,f)	mask_pushcclosure(L, (f), 0)

#define mask_isfunction(L,n)	(mask_type(L, (n)) == MASK_TFUNCTION)
#define mask_istable(L,n)	(mask_type(L, (n)) == MASK_TTABLE)
#define mask_islightuserdata(L,n)	(mask_type(L, (n)) == MASK_TLIGHTUSERDATA)
#define mask_isnil(L,n)		(mask_type(L, (n)) == MASK_TNIL)
#define mask_isboolean(L,n)	(mask_type(L, (n)) == MASK_TBOOLEAN)
#define mask_isthread(L,n)	(mask_type(L, (n)) == MASK_TTHREAD)
#define mask_isnone(L,n)		(mask_type(L, (n)) == MASK_TNONE)
#define mask_isnoneornil(L, n)	(mask_type(L, (n)) <= 0)

#define mask_pushliteral(L, s)	mask_pushstring(L, "" s)

#define mask_pushglobaltable(L)  \
    ((void)mask_rawgeti(L, MASK_REGISTRYINDEX, MASK_RIDX_GLOBALS))

#define mask_tostring(L,i)	mask_tolstring(L, (i), NULL)


#define mask_insert(L,idx)	mask_rotate(L, (idx), 1)

#define mask_remove(L,idx)	(mask_rotate(L, (idx), -1), mask_pop(L, 1))

#define mask_replace(L,idx)	(mask_copy(L, -1, (idx)), mask_pop(L, 1))

/* }============================================================== */


/*
** {==============================================================
** compatibility macros
** ===============================================================
*/
#if defined(MASK_COMPAT_APIINTCASTS)

#define mask_pushunsigned(L,n)	mask_pushinteger(L, (mask_Integer)(n))
#define mask_tounsignedx(L,i,is)	((mask_Unsigned)mask_tointegerx(L,i,is))
#define mask_tounsigned(L,i)	mask_tounsignedx(L,(i),NULL)

#endif

#define mask_newuserdata(L,s)	mask_newuserdatauv(L,s,1)
#define mask_getuservalue(L,idx)	mask_getiuservalue(L,idx,1)
#define mask_setuservalue(L,idx)	mask_setiuservalue(L,idx,1)

#define MASK_NUMTAGS		MASK_NUMTYPES

/* }============================================================== */

/*
** {======================================================================
** Debug API
** =======================================================================
*/


/*
** Event codes
*/
#define MASK_HOOKCALL	0
#define MASK_HOOKRET	1
#define MASK_HOOKLINE	2
#define MASK_HOOKCOUNT	3
#define MASK_HOOKTAILCALL 4


/*
** Event masks
*/
#define MASK_MASKCALL	(1 << MASK_HOOKCALL)
#define MASK_MASKRET	(1 << MASK_HOOKRET)
#define MASK_MASKLINE	(1 << MASK_HOOKLINE)
#define MASK_MASKCOUNT	(1 << MASK_HOOKCOUNT)

typedef struct mask_Debug mask_Debug;  /* activation record */


/* Functions to be called by the debugger in specific events */
typedef void (*mask_Hook) (mask_State *L, mask_Debug *ar);


MASK_API int (mask_getstack) (mask_State *L, int level, mask_Debug *ar);
MASK_API int (mask_getinfo) (mask_State *L, const char *what, mask_Debug *ar);
MASK_API const char *(mask_getlocal) (mask_State *L, const mask_Debug *ar, int n);
MASK_API const char *(mask_setlocal) (mask_State *L, const mask_Debug *ar, int n);
MASK_API const char *(mask_getupvalue) (mask_State *L, int funcindex, int n);
MASK_API const char *(mask_setupvalue) (mask_State *L, int funcindex, int n);

MASK_API void *(mask_upvalueid) (mask_State *L, int fidx, int n);
MASK_API void  (mask_upvaluejoin) (mask_State *L, int fidx1, int n1,
                                               int fidx2, int n2);

MASK_API void (mask_sethook) (mask_State *L, mask_Hook func, int mask, int count);
MASK_API mask_Hook (mask_gethook) (mask_State *L);
MASK_API int (mask_gethookmask) (mask_State *L);
MASK_API int (mask_gethookcount) (mask_State *L);

MASK_API int (mask_setcstacklimit) (mask_State *L, unsigned int limit);

struct mask_Debug {
  int event;
  const char *name;	/* (n) */
  const char *namewhat;	/* (n) 'global', 'local', 'field', 'method' */
  const char *what;	/* (S) 'Mask', 'C', 'main', 'tail' */
  const char *source;	/* (S) */
  size_t srclen;	/* (S) */
  int currentline;	/* (l) */
  int linedefined;	/* (S) */
  int lastlinedefined;	/* (S) */
  unsigned char nups;	/* (u) number of upvalues */
  unsigned char nparams;/* (u) number of parameters */
  char isvararg;        /* (u) */
  char istailcall;	/* (t) */
  unsigned short ftransfer;   /* (r) index of first value transferred */
  unsigned short ntransfer;   /* (r) number of transferred values */
  char short_src[MASK_IDSIZE]; /* (S) */
  /* private part */
  struct CallInfo *i_ci;  /* active function */
};

/* }====================================================================== */


/******************************************************************************
* Copyright (C) 1994-2022 Mask.org, PUC-Rio.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/
