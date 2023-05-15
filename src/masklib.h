#pragma once
/*
** $Id: masklib.h $
** Mask standard libraries
** See Copyright Notice in mask.h
*/

#include "mask.h"


/* version suffix for environment variable names */
#define MASK_VERSUFFIX          "_" MASK_VERSION_MAJOR "_" MASK_VERSION_MINOR


MASKMOD_API int (maskopen_base) (mask_State *L);

#define MASK_COLIBNAME	"coroutine"
MASKMOD_API int (maskopen_coroutine) (mask_State *L);

#define MASK_TABLIBNAME	"table"
MASKMOD_API int (maskopen_table) (mask_State *L);

#define MASK_IOLIBNAME	"io"
MASKMOD_API int (maskopen_io) (mask_State *L);

#define MASK_OSLIBNAME	"os"
MASKMOD_API int (maskopen_os) (mask_State *L);

#define MASK_STRLIBNAME	"string"
MASKMOD_API int (maskopen_string) (mask_State *L);

#define MASK_UTF8LIBNAME	"utf8"
MASKMOD_API int (maskopen_utf8) (mask_State *L);

#define MASK_MATHLIBNAME	"math"
MASKMOD_API int (maskopen_math) (mask_State *L);

#define MASK_DBLIBNAME	"debug"
MASKMOD_API int (maskopen_debug) (mask_State *L);

#define MASK_LOADLIBNAME	"package"
MASKMOD_API int (maskopen_package) (mask_State *L);


MASKMOD_API int (maskopen_crypto) (mask_State *L);
#ifdef MASK_USE_SOUP
MASKMOD_API int (maskopen_json)   (mask_State *L);
MASKMOD_API int (maskopen_base32) (mask_State *L);
MASKMOD_API int (maskopen_base58) (mask_State *L);
MASKMOD_API int (maskopen_base64) (mask_State *L);
#endif

/* open all previous libraries */
MASKLIB_API void (maskL_openlibs) (mask_State *L);
