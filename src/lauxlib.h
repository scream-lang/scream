#pragma once
/*
** $Id: lauxlib.h $
** Auxiliary functions for building Hello libraries
** See Copyright Notice in hello.h
*/

#include <stddef.h>
#include <stdio.h>

#ifdef _WIN32
#include <string>
#endif

#include "helloconf.h"
#include "hello.h"


/* global table */
#define HELLO_GNAME	"_G"


typedef struct helloL_Buffer helloL_Buffer;


/* extra error code for 'helloL_loadfilex' */
#define HELLO_ERRFILE     (HELLO_ERRERR+1)


/* key, in the registry, for table of loaded modules */
#define HELLO_LOADED_TABLE	"_LOADED"


/* key, in the registry, for table of preloaded loaders */
#define HELLO_PRELOAD_TABLE	"_PRELOAD"


typedef struct helloL_Reg {
  const char *name;
  hello_CFunction func;
} helloL_Reg;


#define HELLOL_NUMSIZES	(sizeof(hello_Integer)*16 + sizeof(hello_Number))

HELLOLIB_API void (helloL_checkversion_) (hello_State *L, hello_Number ver, size_t sz);
#define helloL_checkversion(L)  \
      helloL_checkversion_(L, HELLO_VERSION_NUM, HELLOL_NUMSIZES)

HELLOLIB_API int (helloL_getmetafield) (hello_State *L, int obj, const char *e);
HELLOLIB_API int (helloL_callmeta) (hello_State *L, int obj, const char *e);
HELLOLIB_API const char *(helloL_tolstring) (hello_State *L, int idx, size_t *len);
[[noreturn]] HELLOLIB_API void (helloL_argerror) (hello_State *L, int arg, const char *extramsg);
[[noreturn]] HELLOLIB_API void (helloL_typeerror) (hello_State *L, int arg, const char *tname);
HELLOLIB_API const char *(helloL_checklstring) (hello_State *L, int arg,
                                                          size_t *l);
HELLOLIB_API const char *(helloL_optlstring) (hello_State *L, int arg,
                                          const char *def, size_t *l);
HELLOLIB_API hello_Number (helloL_checknumber) (hello_State *L, int arg);
HELLOLIB_API hello_Number (helloL_optnumber) (hello_State *L, int arg, hello_Number def);

HELLOLIB_API hello_Integer (helloL_checkinteger) (hello_State *L, int arg);
HELLOLIB_API hello_Integer (helloL_optinteger) (hello_State *L, int arg,
                                          hello_Integer def);

HELLOLIB_API void (helloL_checkstack) (hello_State *L, int sz, const char *msg);
HELLOLIB_API void (helloL_checktype) (hello_State *L, int arg, int t);
HELLOLIB_API void (helloL_checkany) (hello_State *L, int arg);

HELLOLIB_API int   (helloL_newmetatable) (hello_State *L, const char *tname);
HELLOLIB_API void  (helloL_setmetatable) (hello_State *L, const char *tname);
HELLOLIB_API void *(helloL_testudata) (hello_State *L, int ud, const char *tname);
HELLOLIB_API void *(helloL_checkudata) (hello_State *L, int ud, const char *tname);

HELLOLIB_API void (helloL_where) (hello_State *L, int lvl);
[[noreturn]] HELLOLIB_API void (helloL_error) (hello_State *L, const char *fmt, ...);

HELLOLIB_API int (helloL_checkoption) (hello_State *L, int arg, const char *def,
                                   const char *const lst[]);

HELLOLIB_API int (helloL_fileresult) (hello_State *L, int stat, const char *fname);
HELLOLIB_API int (helloL_execresult) (hello_State *L, int stat);


/* predefined references */
#define HELLO_NOREF       (-2)
#define HELLO_REFNIL      (-1)

HELLOLIB_API int (helloL_ref) (hello_State *L, int t);
HELLOLIB_API void (helloL_unref) (hello_State *L, int t, int ref);

#ifdef _WIN32
HELLOLIB_API std::wstring helloL_utf8_to_utf16(const char *utf8, size_t utf8_len);
HELLOLIB_API std::string helloL_utf16_to_utf8(const wchar_t *utf16, size_t utf16_len);
#endif

HELLOLIB_API FILE* (helloL_fopen) (const char *filename, size_t filename_len,
                               const char *mode, size_t mode_len);

HELLOLIB_API int (helloL_loadfilex) (hello_State *L, const char *filename,
                                               const char *mode);

#define helloL_loadfile(L,f)	helloL_loadfilex(L,f,NULL)

HELLOLIB_API int (helloL_loadbufferx) (hello_State *L, const char *buff, size_t sz,
                                   const char *name, const char *mode);
HELLOLIB_API int (helloL_loadstring) (hello_State *L, const char *s);

HELLOLIB_API hello_State *(helloL_newstate) (void);

HELLOLIB_API hello_Integer (helloL_len) (hello_State *L, int idx);

HELLOLIB_API void (helloL_addgsub) (helloL_Buffer *b, const char *s,
                                     const char *p, const char *r);
HELLOLIB_API const char *(helloL_gsub) (hello_State *L, const char *s,
                                    const char *p, const char *r);

HELLOLIB_API void (helloL_setfuncs) (hello_State *L, const helloL_Reg *l, int nup);

HELLOLIB_API int (helloL_getsubtable) (hello_State *L, int idx, const char *fname);

HELLOLIB_API void (helloL_traceback) (hello_State *L, hello_State *L1,
                                  const char *msg, int level);

HELLOLIB_API void (helloL_requiref) (hello_State *L, const char *modname,
                                 hello_CFunction openf, int glb);

/*
** ===============================================================
** some useful macros
** ===============================================================
*/


#define helloL_newlibtable(L,l)	\
  hello_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)

#define helloL_newlib(L,l)  \
  (helloL_checkversion(L), helloL_newlibtable(L,l), helloL_setfuncs(L,l,0))

#define helloL_argcheck(L, cond,arg,extramsg)	if (helloi_unlikely(!(cond))) { helloL_argerror(L, (arg), (extramsg)); }

#define helloL_argexpected(L,cond,arg,tname) if (helloi_unlikely(!(cond))) { helloL_typeerror(L, (arg), (tname)); }

#define helloL_checkstring(L,n)	(helloL_checklstring(L, (n), NULL))
#define helloL_optstring(L,n,d)	(helloL_optlstring(L, (n), (d), NULL))

#define helloL_typename(L,i)	hello_typename(L, hello_type(L,(i)))

#define helloL_dofile(L, fn) \
    (helloL_loadfile(L, fn) || hello_pcall(L, 0, HELLO_MULTRET, 0))

#define helloL_dostring(L, s) \
    (helloL_loadstring(L, s) || hello_pcall(L, 0, HELLO_MULTRET, 0))

#define helloL_getmetatable(L,n)	(hello_getfield(L, HELLO_REGISTRYINDEX, (n)))

#define helloL_opt(L,f,n,d)	(hello_isnoneornil(L,(n)) ? (d) : f(L,(n)))

#define helloL_loadbuffer(L,s,sz,n)	helloL_loadbufferx(L,s,sz,n,NULL)


/*
** Perform arithmetic operations on hello_Integer values with wrap-around
** semantics, as the Hello core does.
*/
#define helloL_intop(op,v1,v2)  \
    ((hello_Integer)((hello_Unsigned)(v1) op (hello_Unsigned)(v2)))


/* push the value used to represent failure/error */
#define helloL_pushfail(L)	hello_pushnil(L)


/*
** Internal assertions for in-house debugging
*/
#if !defined(hello_assert)

#if defined HELLOI_ASSERT
  #include <assert.h>
  #define hello_assert(c)		assert(c)
#else
  #define hello_assert(c)		((void)0)
#endif

#endif



/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

struct helloL_Buffer {
  char *b;  /* buffer address */
  size_t size;  /* buffer size */
  size_t n;  /* number of characters in buffer */
  hello_State *L;
  union {
    HELLOI_MAXALIGN;  /* ensure maximum alignment for buffer */
    char b[HELLOL_BUFFERSIZE];  /* initial buffer */
  } init;
};


#define helloL_bufflen(bf)	((bf)->n)
#define helloL_buffaddr(bf)	((bf)->b)


#define helloL_addchar(B,c) \
  ((void)((B)->n < (B)->size || helloL_prepbuffsize((B), 1)), \
   ((B)->b[(B)->n++] = (c)))

#define helloL_addsize(B,s)	((B)->n += (s))

#define helloL_buffsub(B,s)	((B)->n -= (s))

HELLOLIB_API void (helloL_buffinit) (hello_State *L, helloL_Buffer *B);
HELLOLIB_API char *(helloL_prepbuffsize) (helloL_Buffer *B, size_t sz);
HELLOLIB_API void (helloL_addlstring) (helloL_Buffer *B, const char *s, size_t l);
HELLOLIB_API void (helloL_addstring) (helloL_Buffer *B, const char *s);
HELLOLIB_API void (helloL_addvalue) (helloL_Buffer *B);
HELLOLIB_API void (helloL_pushresult) (helloL_Buffer *B);
HELLOLIB_API void (helloL_pushresultsize) (helloL_Buffer *B, size_t sz);
HELLOLIB_API char *(helloL_buffinitsize) (hello_State *L, helloL_Buffer *B, size_t sz);

#define helloL_prepbuffer(B)	helloL_prepbuffsize(B, HELLOL_BUFFERSIZE)

/* }====================================================== */



/*
** {======================================================
** File handles for IO library
** =======================================================
*/

/*
** A file handle is a userdata with metatable 'HELLO_FILEHANDLE' and
** initial structure 'helloL_Stream' (it may contain other fields
** after that initial structure).
*/

#define HELLO_FILEHANDLE          "FILE*"


typedef struct helloL_Stream {
  FILE *f;  /* stream (NULL for incompletely created streams) */
  hello_CFunction closef;  /* to close stream (NULL for closed streams) */
} helloL_Stream;

/* }====================================================== */

/*
** {==================================================================
** "Abstraction Layer" for basic report of messages and errors
** ===================================================================
*/

/* print a string */
#if !defined(hello_writestring)
#define hello_writestring(s,l)   fwrite((s), sizeof(char), (l), stdout)
#endif

/* print a newline and flush the output */
#if !defined(hello_writeline)
#define hello_writeline()        (hello_writestring("\n", 1), fflush(stdout))
#endif

/* print an error message */
#if !defined(hello_writestringerror)
#define hello_writestringerror(s,p) \
        (fprintf(stderr, (s), (p)), fflush(stderr))
#endif

/* }================================================================== */


/*
** {============================================================
** Compatibility with deprecated conversions
** =============================================================
*/
#if defined(HELLO_COMPAT_APIINTCASTS)

#define helloL_checkunsigned(L,a)	((hello_Unsigned)helloL_checkinteger(L,a))
#define helloL_optunsigned(L,a,d)	\
    ((hello_Unsigned)helloL_optinteger(L,a,(hello_Integer)(d)))

#define helloL_checkint(L,n)	((int)helloL_checkinteger(L, (n)))
#define helloL_optint(L,n,d)	((int)helloL_optinteger(L, (n), (d)))

#define helloL_checklong(L,n)	((long)helloL_checkinteger(L, (n)))
#define helloL_optlong(L,n,d)	((long)helloL_optinteger(L, (n), (d)))

#endif
/* }============================================================ */
