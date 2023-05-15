#pragma once
/*
** $Id: lauxlib.h $
** Auxiliary functions for building Mask libraries
** See Copyright Notice in mask.h
*/

#include <stddef.h>
#include <stdio.h>

#ifdef _WIN32
#include <string>
#endif

#include "maskconf.h"
#include "mask.h"


/* global table */
#define MASK_GNAME	"_G"


typedef struct maskL_Buffer maskL_Buffer;


/* extra error code for 'maskL_loadfilex' */
#define MASK_ERRFILE     (MASK_ERRERR+1)


/* key, in the registry, for table of loaded modules */
#define MASK_LOADED_TABLE	"_LOADED"


/* key, in the registry, for table of preloaded loaders */
#define MASK_PRELOAD_TABLE	"_PRELOAD"


typedef struct maskL_Reg {
  const char *name;
  mask_CFunction func;
} maskL_Reg;


#define MASKL_NUMSIZES	(sizeof(mask_Integer)*16 + sizeof(mask_Number))

MASKLIB_API void (maskL_checkversion_) (mask_State *L, mask_Number ver, size_t sz);
#define maskL_checkversion(L)  \
      maskL_checkversion_(L, MASK_VERSION_NUM, MASKL_NUMSIZES)

MASKLIB_API int (maskL_getmetafield) (mask_State *L, int obj, const char *e);
MASKLIB_API int (maskL_callmeta) (mask_State *L, int obj, const char *e);
MASKLIB_API const char *(maskL_tolstring) (mask_State *L, int idx, size_t *len);
[[noreturn]] MASKLIB_API void (maskL_argerror) (mask_State *L, int arg, const char *extramsg);
[[noreturn]] MASKLIB_API void (maskL_typeerror) (mask_State *L, int arg, const char *tname);
MASKLIB_API const char *(maskL_checklstring) (mask_State *L, int arg,
                                                          size_t *l);
MASKLIB_API const char *(maskL_optlstring) (mask_State *L, int arg,
                                          const char *def, size_t *l);
MASKLIB_API mask_Number (maskL_checknumber) (mask_State *L, int arg);
MASKLIB_API mask_Number (maskL_optnumber) (mask_State *L, int arg, mask_Number def);

MASKLIB_API mask_Integer (maskL_checkinteger) (mask_State *L, int arg);
MASKLIB_API mask_Integer (maskL_optinteger) (mask_State *L, int arg,
                                          mask_Integer def);

MASKLIB_API void (maskL_checkstack) (mask_State *L, int sz, const char *msg);
MASKLIB_API void (maskL_checktype) (mask_State *L, int arg, int t);
MASKLIB_API void (maskL_checkany) (mask_State *L, int arg);

MASKLIB_API int   (maskL_newmetatable) (mask_State *L, const char *tname);
MASKLIB_API void  (maskL_setmetatable) (mask_State *L, const char *tname);
MASKLIB_API void *(maskL_testudata) (mask_State *L, int ud, const char *tname);
MASKLIB_API void *(maskL_checkudata) (mask_State *L, int ud, const char *tname);

MASKLIB_API void (maskL_where) (mask_State *L, int lvl);
[[noreturn]] MASKLIB_API void (maskL_error) (mask_State *L, const char *fmt, ...);

MASKLIB_API int (maskL_checkoption) (mask_State *L, int arg, const char *def,
                                   const char *const lst[]);

MASKLIB_API int (maskL_fileresult) (mask_State *L, int stat, const char *fname);
MASKLIB_API int (maskL_execresult) (mask_State *L, int stat);


/* predefined references */
#define MASK_NOREF       (-2)
#define MASK_REFNIL      (-1)

MASKLIB_API int (maskL_ref) (mask_State *L, int t);
MASKLIB_API void (maskL_unref) (mask_State *L, int t, int ref);

#ifdef _WIN32
MASKLIB_API std::wstring maskL_utf8_to_utf16(const char *utf8, size_t utf8_len);
MASKLIB_API std::string maskL_utf16_to_utf8(const wchar_t *utf16, size_t utf16_len);
#endif

MASKLIB_API FILE* (maskL_fopen) (const char *filename, size_t filename_len,
                               const char *mode, size_t mode_len);

MASKLIB_API int (maskL_loadfilex) (mask_State *L, const char *filename,
                                               const char *mode);

#define maskL_loadfile(L,f)	maskL_loadfilex(L,f,NULL)

MASKLIB_API int (maskL_loadbufferx) (mask_State *L, const char *buff, size_t sz,
                                   const char *name, const char *mode);
MASKLIB_API int (maskL_loadstring) (mask_State *L, const char *s);

MASKLIB_API mask_State *(maskL_newstate) (void);

MASKLIB_API mask_Integer (maskL_len) (mask_State *L, int idx);

MASKLIB_API void (maskL_addgsub) (maskL_Buffer *b, const char *s,
                                     const char *p, const char *r);
MASKLIB_API const char *(maskL_gsub) (mask_State *L, const char *s,
                                    const char *p, const char *r);

MASKLIB_API void (maskL_setfuncs) (mask_State *L, const maskL_Reg *l, int nup);

MASKLIB_API int (maskL_getsubtable) (mask_State *L, int idx, const char *fname);

MASKLIB_API void (maskL_traceback) (mask_State *L, mask_State *L1,
                                  const char *msg, int level);

MASKLIB_API void (maskL_requiref) (mask_State *L, const char *modname,
                                 mask_CFunction openf, int glb);

/*
** ===============================================================
** some useful macros
** ===============================================================
*/


#define maskL_newlibtable(L,l)	\
  mask_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)

#define maskL_newlib(L,l)  \
  (maskL_checkversion(L), maskL_newlibtable(L,l), maskL_setfuncs(L,l,0))

#define maskL_argcheck(L, cond,arg,extramsg)	if (maski_unlikely(!(cond))) { maskL_argerror(L, (arg), (extramsg)); }

#define maskL_argexpected(L,cond,arg,tname) if (maski_unlikely(!(cond))) { maskL_typeerror(L, (arg), (tname)); }

#define maskL_checkstring(L,n)	(maskL_checklstring(L, (n), NULL))
#define maskL_optstring(L,n,d)	(maskL_optlstring(L, (n), (d), NULL))

#define maskL_typename(L,i)	mask_typename(L, mask_type(L,(i)))

#define maskL_dofile(L, fn) \
    (maskL_loadfile(L, fn) || mask_pcall(L, 0, MASK_MULTRET, 0))

#define maskL_dostring(L, s) \
    (maskL_loadstring(L, s) || mask_pcall(L, 0, MASK_MULTRET, 0))

#define maskL_getmetatable(L,n)	(mask_getfield(L, MASK_REGISTRYINDEX, (n)))

#define maskL_opt(L,f,n,d)	(mask_isnoneornil(L,(n)) ? (d) : f(L,(n)))

#define maskL_loadbuffer(L,s,sz,n)	maskL_loadbufferx(L,s,sz,n,NULL)


/*
** Perform arithmetic operations on mask_Integer values with wrap-around
** semantics, as the Mask core does.
*/
#define maskL_intop(op,v1,v2)  \
    ((mask_Integer)((mask_Unsigned)(v1) op (mask_Unsigned)(v2)))


/* push the value used to represent failure/error */
#define maskL_pushfail(L)	mask_pushnil(L)


/*
** Internal assertions for in-house debugging
*/
#if !defined(mask_assert)

#if defined MASKI_ASSERT
  #include <assert.h>
  #define mask_assert(c)		assert(c)
#else
  #define mask_assert(c)		((void)0)
#endif

#endif



/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

struct maskL_Buffer {
  char *b;  /* buffer address */
  size_t size;  /* buffer size */
  size_t n;  /* number of characters in buffer */
  mask_State *L;
  union {
    MASKI_MAXALIGN;  /* ensure maximum alignment for buffer */
    char b[MASKL_BUFFERSIZE];  /* initial buffer */
  } init;
};


#define maskL_bufflen(bf)	((bf)->n)
#define maskL_buffaddr(bf)	((bf)->b)


#define maskL_addchar(B,c) \
  ((void)((B)->n < (B)->size || maskL_prepbuffsize((B), 1)), \
   ((B)->b[(B)->n++] = (c)))

#define maskL_addsize(B,s)	((B)->n += (s))

#define maskL_buffsub(B,s)	((B)->n -= (s))

MASKLIB_API void (maskL_buffinit) (mask_State *L, maskL_Buffer *B);
MASKLIB_API char *(maskL_prepbuffsize) (maskL_Buffer *B, size_t sz);
MASKLIB_API void (maskL_addlstring) (maskL_Buffer *B, const char *s, size_t l);
MASKLIB_API void (maskL_addstring) (maskL_Buffer *B, const char *s);
MASKLIB_API void (maskL_addvalue) (maskL_Buffer *B);
MASKLIB_API void (maskL_pushresult) (maskL_Buffer *B);
MASKLIB_API void (maskL_pushresultsize) (maskL_Buffer *B, size_t sz);
MASKLIB_API char *(maskL_buffinitsize) (mask_State *L, maskL_Buffer *B, size_t sz);

#define maskL_prepbuffer(B)	maskL_prepbuffsize(B, MASKL_BUFFERSIZE)

/* }====================================================== */



/*
** {======================================================
** File handles for IO library
** =======================================================
*/

/*
** A file handle is a userdata with metatable 'MASK_FILEHANDLE' and
** initial structure 'maskL_Stream' (it may contain other fields
** after that initial structure).
*/

#define MASK_FILEHANDLE          "FILE*"


typedef struct maskL_Stream {
  FILE *f;  /* stream (NULL for incompletely created streams) */
  mask_CFunction closef;  /* to close stream (NULL for closed streams) */
} maskL_Stream;

/* }====================================================== */

/*
** {==================================================================
** "Abstraction Layer" for basic report of messages and errors
** ===================================================================
*/

/* print a string */
#if !defined(mask_writestring)
#define mask_writestring(s,l)   fwrite((s), sizeof(char), (l), stdout)
#endif

/* print a newline and flush the output */
#if !defined(mask_writeline)
#define mask_writeline()        (mask_writestring("\n", 1), fflush(stdout))
#endif

/* print an error message */
#if !defined(mask_writestringerror)
#define mask_writestringerror(s,p) \
        (fprintf(stderr, (s), (p)), fflush(stderr))
#endif

/* }================================================================== */


/*
** {============================================================
** Compatibility with deprecated conversions
** =============================================================
*/
#if defined(MASK_COMPAT_APIINTCASTS)

#define maskL_checkunsigned(L,a)	((mask_Unsigned)maskL_checkinteger(L,a))
#define maskL_optunsigned(L,a,d)	\
    ((mask_Unsigned)maskL_optinteger(L,a,(mask_Integer)(d)))

#define maskL_checkint(L,n)	((int)maskL_checkinteger(L, (n)))
#define maskL_optint(L,n,d)	((int)maskL_optinteger(L, (n), (d)))

#define maskL_checklong(L,n)	((long)maskL_checkinteger(L, (n)))
#define maskL_optlong(L,n,d)	((long)maskL_optinteger(L, (n), (d)))

#endif
/* }============================================================ */
