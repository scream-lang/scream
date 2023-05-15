#pragma once
/*
** $Id: hello.h $
** Hello - A Scripting Language
** Hello.org, PUC-Rio, Brazil 
** See Copyright Notice at the end of this file
*/

#include <stdarg.h>
#include <stddef.h>
#include <string>


#include "helloconf.h"


#define HELLO_VERSION "hello 0.0.1"

#define HELLO_VERSION_MAJOR	"0"
#define HELLO_VERSION_MINOR	"0"
#define HELLO_VERSION_RELEASE	"1"

#define HELLO_VERSION_NUM			504

#define HELLO_VERSION	"Hello " HELLO_VERSION_MAJOR "." HELLO_VERSION_MINOR "." HELLO_VERSION_RELEASE
#define HELLO_RELEASE	HELLO_VERSION "." HELLO_VERSION_RELEASE
#define HELLO_COPYRIGHT	HELLO_VERSION " Copyright (C) 2019 - 2023 Timo Sarkar based on " "Lua 5.4 Copyright (C) 1994-2022 Lua.org, PUC-Rio"
#define HELLO_AUTHORS	"Timo Sarkaar, R. Ierusalimschy, L. H. de Figueiredo, W. Celes"


/* mark for precompiled code ('<esc>Hello') */
#define HELLO_SIGNATURE	"\x1bHello"

/* option for multiple returns in 'hello_pcall' and 'hello_call' */
#define HELLO_MULTRET	(-1)


/*
** Pseudo-indices
** (-HELLOI_MAXSTACK is the minimum valid index; we keep some free empty
** space after that to help overflow detection)
*/
#define HELLO_REGISTRYINDEX	(-HELLOI_MAXSTACK - 1000)
#define hello_upvalueindex(i)	(HELLO_REGISTRYINDEX - (i))


/* thread status */
#define HELLO_OK		0
#define HELLO_YIELD	1
#define HELLO_ERRRUN	2
#define HELLO_ERRSYNTAX	3
#define HELLO_ERRMEM	4
#define HELLO_ERRERR	5


typedef struct hello_State hello_State;


/*
** basic types
*/
#define HELLO_TNONE		(-1)

#define HELLO_TNIL		0
#define HELLO_TBOOLEAN		1
#define HELLO_TLIGHTUSERDATA	2
#define HELLO_TNUMBER		3
#define HELLO_TSTRING		4
#define HELLO_TTABLE		5
#define HELLO_TFUNCTION		6
#define HELLO_TUSERDATA		7
#define HELLO_TTHREAD		8

#define HELLO_NUMTYPES		9



/* minimum Hello stack available to a C function */
#define HELLO_MINSTACK	20


/* predefined values in the registry */
#define HELLO_RIDX_MAINTHREAD	1
#define HELLO_RIDX_GLOBALS	2
#define HELLO_RIDX_LAST		HELLO_RIDX_GLOBALS


/* type of numbers in Hello */
typedef HELLO_NUMBER hello_Number;


/* type for integer functions */
typedef HELLO_INTEGER hello_Integer;

/* unsigned integer type */
typedef HELLO_UNSIGNED hello_Unsigned;

/* type for continuation-function contexts */
typedef HELLO_KCONTEXT hello_KContext;


/*
** Type for C functions registered with Hello
*/
typedef int (*hello_CFunction) (hello_State *L);

/*
** Type for continuation functions
*/
typedef int (*hello_KFunction) (hello_State *L, int status, hello_KContext ctx);


/*
** Type for functions that read/write blocks when loading/dumping Hello chunks
*/
typedef const char * (*hello_Reader) (hello_State *L, void *ud, size_t *sz);

typedef int (*hello_Writer) (hello_State *L, const void *p, size_t sz, void *ud);


/*
** Type for memory-allocation functions
*/
typedef void * (*hello_Alloc) (void *ud, void *ptr, size_t osize, size_t nsize);


/*
** Type for warning functions
*/
typedef void (*hello_WarnFunction) (void *ud, const char *msg, int tocont);




/*
** generic extra include file
*/
#if defined(HELLO_USER_H)
#include HELLO_USER_H
#endif


/*
** RCS ident string
*/
extern const char hello_ident[];


/*
** state manipulation
*/
HELLO_API hello_State *(hello_newstate) (hello_Alloc f, void *ud);
HELLO_API void       (hello_close) (hello_State *L);
HELLO_API hello_State *(hello_newthread) (hello_State *L);
HELLO_API int        (hello_resetthread) (hello_State *L, hello_State *from);

HELLO_API hello_CFunction (hello_atpanic) (hello_State *L, hello_CFunction panicf);


HELLO_API hello_Number (hello_version) (hello_State *L);


/*
** basic stack manipulation
*/
HELLO_API int   (hello_absindex) (hello_State *L, int idx);
HELLO_API int   (hello_gettop) (hello_State *L);
HELLO_API void  (hello_settop) (hello_State *L, int idx);
HELLO_API void  (hello_pushvalue) (hello_State *L, int idx);
HELLO_API void  (hello_rotate) (hello_State *L, int idx, int n);
HELLO_API void  (hello_copy) (hello_State *L, int fromidx, int toidx);
HELLO_API int   (hello_checkstack) (hello_State *L, int n);

HELLO_API void  (hello_xmove) (hello_State *from, hello_State *to, int n);


/*
** access functions (stack -> C)
*/

HELLO_API int             (hello_isnumber) (hello_State *L, int idx);
HELLO_API int             (hello_isstring) (hello_State *L, int idx);
HELLO_API int             (hello_iscfunction) (hello_State *L, int idx);
HELLO_API int             (hello_isinteger) (hello_State *L, int idx);
HELLO_API int             (hello_istrue) (hello_State *L, int idx) noexcept;
HELLO_API int             (hello_isuserdata) (hello_State *L, int idx);
HELLO_API int             (hello_type) (hello_State *L, int idx);
HELLO_API const char     *(hello_typename) (hello_State *L, int tp);

HELLO_API hello_Number      (hello_tonumberx) (hello_State *L, int idx, int *isnum);
HELLO_API hello_Integer     (hello_tointegerx) (hello_State *L, int idx, int *isnum);
HELLO_API int             (hello_toboolean) (hello_State *L, int idx);
HELLO_API const char     *(hello_tolstring) (hello_State *L, int idx, size_t *len);
HELLO_API hello_Unsigned    (hello_rawlen) (hello_State *L, int idx);
HELLO_API hello_CFunction   (hello_tocfunction) (hello_State *L, int idx);
HELLO_API void	       *(hello_touserdata) (hello_State *L, int idx);
HELLO_API hello_State      *(hello_tothread) (hello_State *L, int idx);
HELLO_API const void     *(hello_topointer) (hello_State *L, int idx);


/*
** Comparison and arithmetic functions
*/

#define HELLO_OPADD	0	/* ORDER TM, ORDER OP */
#define HELLO_OPSUB	1
#define HELLO_OPMUL	2
#define HELLO_OPMOD	3
#define HELLO_OPPOW	4
#define HELLO_OPDIV	5
#define HELLO_OPIDIV	6
#define HELLO_OPBAND	7
#define HELLO_OPBOR	8
#define HELLO_OPBXOR	9
#define HELLO_OPSHL	10
#define HELLO_OPSHR	11
#define HELLO_OPUNM	12
#define HELLO_OPBNOT	13

HELLO_API void  (hello_arith) (hello_State *L, int op);

#define HELLO_OPEQ	0
#define HELLO_OPLT	1
#define HELLO_OPLE	2

HELLO_API int   (hello_rawequal) (hello_State *L, int idx1, int idx2);
HELLO_API int   (hello_compare) (hello_State *L, int idx1, int idx2, int op);


/*
** push functions (C -> stack)
*/
HELLO_API void        (hello_pushnil) (hello_State *L);
HELLO_API void        (hello_pushnumber) (hello_State *L, hello_Number n);
HELLO_API void        (hello_pushinteger) (hello_State *L, hello_Integer n);
HELLO_API const char *(hello_pushlstring) (hello_State *L, const char *s, size_t len);
HELLO_API const char *(hello_pushstring) (hello_State *L, const char *s);
HELLO_API const char *(hello_pushstring) (hello_State* L, const std::string& str);
HELLO_API const char *(hello_pushvfstring) (hello_State *L, const char *fmt,
                                                      va_list argp);
HELLO_API const char *(hello_pushfstring) (hello_State *L, const char *fmt, ...);
HELLO_API void  (hello_pushcclosure) (hello_State *L, hello_CFunction fn, int n);
HELLO_API void  (hello_pushboolean) (hello_State *L, int b);
HELLO_API void  (hello_pushlightuserdata) (hello_State *L, void *p);
HELLO_API int   (hello_pushthread) (hello_State *L);


/*
** get functions (Hello -> stack)
*/
HELLO_API int (hello_getglobal) (hello_State *L, const char *name);
HELLO_API int (hello_gettable) (hello_State *L, int idx);
HELLO_API int (hello_getfield) (hello_State *L, int idx, const char *k);
HELLO_API int (hello_geti) (hello_State *L, int idx, hello_Integer n);
HELLO_API int (hello_rawget) (hello_State *L, int idx);
HELLO_API int (hello_rawgeti) (hello_State *L, int idx, hello_Integer n);
HELLO_API int (hello_rawgetp) (hello_State *L, int idx, const void *p);

HELLO_API void  (hello_createtable) (hello_State *L, int narr, int nrec);
HELLO_API void *(hello_newuserdatauv) (hello_State *L, size_t sz, int nuvalue);
HELLO_API int   (hello_getmetatable) (hello_State *L, int objindex);
HELLO_API int  (hello_getiuservalue) (hello_State *L, int idx, int n);


/*
** set functions (stack -> Hello)
*/
HELLO_API void  (hello_setglobal) (hello_State *L, const char *name);
HELLO_API void  (hello_settable) (hello_State *L, int idx);
HELLO_API void  (hello_setfield) (hello_State *L, int idx, const char *k);
HELLO_API void  (hello_seti) (hello_State *L, int idx, hello_Integer n);
HELLO_API void  (hello_rawset) (hello_State *L, int idx);
HELLO_API void  (hello_rawseti) (hello_State *L, int idx, hello_Integer n);
HELLO_API void  (hello_rawsetp) (hello_State *L, int idx, const void *p);
HELLO_API int   (hello_setmetatable) (hello_State *L, int objindex);
HELLO_API int   (hello_setiuservalue) (hello_State *L, int idx, int n);
HELLO_API void  (hello_setcachelen) (hello_State *L, hello_Unsigned len, int idx);
HELLO_API void  (hello_freezetable) (hello_State *L, int idx);
HELLO_API int   (hello_istablefrozen) (hello_State *L, int idx);
HELLO_API void  (hello_erriffrozen) (hello_State *L, int idx);


/*
** 'load' and 'call' functions (load and run Hello code)
*/
HELLO_API void  (hello_callk) (hello_State *L, int nargs, int nresults,
                           hello_KContext ctx, hello_KFunction k);
#define hello_call(L,n,r)		hello_callk(L, (n), (r), 0, NULL)

HELLO_API int   (hello_pcallk) (hello_State *L, int nargs, int nresults, int errfunc,
                            hello_KContext ctx, hello_KFunction k);
#define hello_pcall(L,n,r,f)	hello_pcallk(L, (n), (r), (f), 0, NULL)

HELLO_API int   (hello_load) (hello_State *L, hello_Reader reader, void *dt,
                          const char *chunkname, const char *mode);

HELLO_API int (hello_dump) (hello_State *L, hello_Writer writer, void *data, int strip);


/*
** coroutine functions
*/
HELLO_API int  (hello_yieldk)     (hello_State *L, int nresults, hello_KContext ctx,
                               hello_KFunction k);
HELLO_API int  (hello_resume)     (hello_State *L, hello_State *from, int narg,
                               int *nres);
HELLO_API int  (hello_status)     (hello_State *L);
HELLO_API int (hello_isyieldable) (hello_State *L);

#define hello_yield(L,n)		hello_yieldk(L, (n), 0, NULL)


/*
** Warning-related functions
*/
HELLO_API void (hello_setwarnf) (hello_State *L, hello_WarnFunction f, void *ud);
HELLO_API void (hello_warning)  (hello_State *L, const char *msg, int tocont);


/*
** garbage-collection function and options
*/

#define HELLO_GCSTOP		0
#define HELLO_GCRESTART		1
#define HELLO_GCCOLLECT		2
#define HELLO_GCCOUNT		3
#define HELLO_GCCOUNTB		4
#define HELLO_GCSTEP		5
#define HELLO_GCSETPAUSE		6
#define HELLO_GCSETSTEPMUL	7
#define HELLO_GCISRUNNING		9
#define HELLO_GCGEN		10
#define HELLO_GCINC		11

HELLO_API int (hello_gc) (hello_State *L, int what, ...);


/*
** miscellaneous functions
*/

[[noreturn]] HELLO_API void   (hello_error) (hello_State *L);

HELLO_API int   (hello_next) (hello_State *L, int idx);

HELLO_API void  (hello_concat) (hello_State *L, int n);
HELLO_API void  (hello_len)    (hello_State *L, int idx);

HELLO_API size_t   (hello_stringtonumber) (hello_State *L, const char *s);

HELLO_API hello_Alloc (hello_getallocf) (hello_State *L, void **ud);
HELLO_API void      (hello_setallocf) (hello_State *L, hello_Alloc f, void *ud);

HELLO_API void (hello_toclose) (hello_State *L, int idx);
HELLO_API void (hello_closeslot) (hello_State *L, int idx);


/*
** {==============================================================
** some useful macros
** ===============================================================
*/

#define hello_getextraspace(L)	((void *)((char *)(L) - HELLO_EXTRASPACE))

#define hello_tonumber(L,i)	hello_tonumberx(L,(i),NULL)
#define hello_tointeger(L,i)	hello_tointegerx(L,(i),NULL)

#define hello_pop(L,n)		hello_settop(L, -(n)-1)

#define hello_newtable(L)		hello_createtable(L, 0, 0)

#define hello_register(L,n,f) (hello_pushcfunction(L, (f)), hello_setglobal(L, (n)))

#define hello_pushcfunction(L,f)	hello_pushcclosure(L, (f), 0)

#define hello_isfunction(L,n)	(hello_type(L, (n)) == HELLO_TFUNCTION)
#define hello_istable(L,n)	(hello_type(L, (n)) == HELLO_TTABLE)
#define hello_islightuserdata(L,n)	(hello_type(L, (n)) == HELLO_TLIGHTUSERDATA)
#define hello_isnil(L,n)		(hello_type(L, (n)) == HELLO_TNIL)
#define hello_isboolean(L,n)	(hello_type(L, (n)) == HELLO_TBOOLEAN)
#define hello_isthread(L,n)	(hello_type(L, (n)) == HELLO_TTHREAD)
#define hello_isnone(L,n)		(hello_type(L, (n)) == HELLO_TNONE)
#define hello_isnoneornil(L, n)	(hello_type(L, (n)) <= 0)

#define hello_pushliteral(L, s)	hello_pushstring(L, "" s)

#define hello_pushglobaltable(L)  \
    ((void)hello_rawgeti(L, HELLO_REGISTRYINDEX, HELLO_RIDX_GLOBALS))

#define hello_tostring(L,i)	hello_tolstring(L, (i), NULL)


#define hello_insert(L,idx)	hello_rotate(L, (idx), 1)

#define hello_remove(L,idx)	(hello_rotate(L, (idx), -1), hello_pop(L, 1))

#define hello_replace(L,idx)	(hello_copy(L, -1, (idx)), hello_pop(L, 1))

/* }============================================================== */


/*
** {==============================================================
** compatibility macros
** ===============================================================
*/
#if defined(HELLO_COMPAT_APIINTCASTS)

#define hello_pushunsigned(L,n)	hello_pushinteger(L, (hello_Integer)(n))
#define hello_tounsignedx(L,i,is)	((hello_Unsigned)hello_tointegerx(L,i,is))
#define hello_tounsigned(L,i)	hello_tounsignedx(L,(i),NULL)

#endif

#define hello_newuserdata(L,s)	hello_newuserdatauv(L,s,1)
#define hello_getuservalue(L,idx)	hello_getiuservalue(L,idx,1)
#define hello_setuservalue(L,idx)	hello_setiuservalue(L,idx,1)

#define HELLO_NUMTAGS		HELLO_NUMTYPES

/* }============================================================== */

/*
** {======================================================================
** Debug API
** =======================================================================
*/


/*
** Event codes
*/
#define HELLO_HOOKCALL	0
#define HELLO_HOOKRET	1
#define HELLO_HOOKLINE	2
#define HELLO_HOOKCOUNT	3
#define HELLO_HOOKTAILCALL 4


/*
** Event masks
*/
#define HELLO_MASKCALL	(1 << HELLO_HOOKCALL)
#define HELLO_MASKRET	(1 << HELLO_HOOKRET)
#define HELLO_MASKLINE	(1 << HELLO_HOOKLINE)
#define HELLO_MASKCOUNT	(1 << HELLO_HOOKCOUNT)

typedef struct hello_Debug hello_Debug;  /* activation record */


/* Functions to be called by the debugger in specific events */
typedef void (*hello_Hook) (hello_State *L, hello_Debug *ar);


HELLO_API int (hello_getstack) (hello_State *L, int level, hello_Debug *ar);
HELLO_API int (hello_getinfo) (hello_State *L, const char *what, hello_Debug *ar);
HELLO_API const char *(hello_getlocal) (hello_State *L, const hello_Debug *ar, int n);
HELLO_API const char *(hello_setlocal) (hello_State *L, const hello_Debug *ar, int n);
HELLO_API const char *(hello_getupvalue) (hello_State *L, int funcindex, int n);
HELLO_API const char *(hello_setupvalue) (hello_State *L, int funcindex, int n);

HELLO_API void *(hello_upvalueid) (hello_State *L, int fidx, int n);
HELLO_API void  (hello_upvaluejoin) (hello_State *L, int fidx1, int n1,
                                               int fidx2, int n2);

HELLO_API void (hello_sethook) (hello_State *L, hello_Hook func, int mask, int count);
HELLO_API hello_Hook (hello_gethook) (hello_State *L);
HELLO_API int (hello_gethookmask) (hello_State *L);
HELLO_API int (hello_gethookcount) (hello_State *L);

HELLO_API int (hello_setcstacklimit) (hello_State *L, unsigned int limit);

struct hello_Debug {
  int event;
  const char *name;	/* (n) */
  const char *namewhat;	/* (n) 'global', 'local', 'field', 'method' */
  const char *what;	/* (S) 'Hello', 'C', 'main', 'tail' */
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
  char short_src[HELLO_IDSIZE]; /* (S) */
  /* private part */
  struct CallInfo *i_ci;  /* active function */
};

/* }====================================================================== */


/******************************************************************************
* Copyright (C) 1994-2022 Hello.org, PUC-Rio.
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
