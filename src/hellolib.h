#pragma once
/*
** $Id: hellolib.h $
** Hello standard libraries
** See Copyright Notice in hello.h
*/

#include "hello.h"


/* version suffix for environment variable names */
#define HELLO_VERSUFFIX          "_" HELLO_VERSION_MAJOR "_" HELLO_VERSION_MINOR


HELLOMOD_API int (helloopen_base) (hello_State *L);

#define HELLO_COLIBNAME	"coroutine"
HELLOMOD_API int (helloopen_coroutine) (hello_State *L);

#define HELLO_TABLIBNAME	"table"
HELLOMOD_API int (helloopen_table) (hello_State *L);

#define HELLO_IOLIBNAME	"io"
HELLOMOD_API int (helloopen_io) (hello_State *L);

#define HELLO_OSLIBNAME	"os"
HELLOMOD_API int (helloopen_os) (hello_State *L);

#define HELLO_STRLIBNAME	"string"
HELLOMOD_API int (helloopen_string) (hello_State *L);

#define HELLO_UTF8LIBNAME	"utf8"
HELLOMOD_API int (helloopen_utf8) (hello_State *L);

#define HELLO_MATHLIBNAME	"math"
HELLOMOD_API int (helloopen_math) (hello_State *L);

#define HELLO_DBLIBNAME	"debug"
HELLOMOD_API int (helloopen_debug) (hello_State *L);

#define HELLO_LOADLIBNAME	"package"
HELLOMOD_API int (helloopen_package) (hello_State *L);


HELLOMOD_API int (helloopen_crypto) (hello_State *L);
#ifdef HELLO_USE_SOUP
HELLOMOD_API int (helloopen_json)   (hello_State *L);
HELLOMOD_API int (helloopen_base32) (hello_State *L);
HELLOMOD_API int (helloopen_base58) (hello_State *L);
HELLOMOD_API int (helloopen_base64) (hello_State *L);
#endif

/* open all previous libraries */
HELLOLIB_API void (helloL_openlibs) (hello_State *L);
