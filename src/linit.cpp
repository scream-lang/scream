/*
** $Id: linit.c $
** Initialization of libraries for mask.c and other clients
** See Copyright Notice in mask.h
*/


#define linit_c
#define MASK_LIB

/*
** If you embed Mask in your program and need to open the standard
** libraries, call maskL_openlibs in your program. If you need a
** different set of libraries, copy this file to your project and edit
** it to suit your needs.
**
** You can also *preload* libraries, so that a later 'require' can
** open the library, which is already linked to the application.
** For that, do the following code:
**
**  maskL_getsubtable(L, MASK_REGISTRYINDEX, MASK_PRELOAD_TABLE);
**  mask_pushcfunction(L, maskopen_modname);
**  mask_setfield(L, -2, modname);
**  mask_pop(L, 1);  // remove PRELOAD table
*/

#include "lprefix.h"


#include <stddef.h>

#include "mask.h"

#include "masklib.h"
#include "lauxlib.h"


/*
** these libs are loaded by mask.c and are readily available to any Mask
** program
*/
static const maskL_Reg loadedlibs[] = {
  {MASK_GNAME, maskopen_base},
  {MASK_LOADLIBNAME, maskopen_package},
  {MASK_COLIBNAME, maskopen_coroutine},
  {MASK_TABLIBNAME, maskopen_table},
  {MASK_IOLIBNAME, maskopen_io},
  {MASK_OSLIBNAME, maskopen_os},
  {MASK_STRLIBNAME, maskopen_string},
  {MASK_MATHLIBNAME, maskopen_math},
  {MASK_UTF8LIBNAME, maskopen_utf8},
  {MASK_DBLIBNAME, maskopen_debug},
  {NULL, NULL}
};


static const maskL_Reg preloadedLibs[] = {
#ifdef MASK_USE_SOUP
  {"json", maskopen_json},
  {"base32", maskopen_base32},
  {"base58", maskopen_base58},
  {"base64", maskopen_base64},
#endif
  {"crypto", maskopen_crypto},
  {NULL, NULL}
};


MASKLIB_API void maskL_openlibs (mask_State *L)
{
  const maskL_Reg *lib;

  for (lib = loadedlibs; lib->func; lib++)
  {
    maskL_requiref(L, lib->name, lib->func, 1);
    mask_pop(L, 1);  /* remove lib */
  }

  for (lib = preloadedLibs; lib->func; lib++)
  {
    maskL_getsubtable(L, MASK_REGISTRYINDEX, MASK_PRELOAD_TABLE);
    mask_pushcfunction(L, lib->func);
    mask_setfield(L, -2, lib->name);
    mask_pop(L, 1);
  }
}

