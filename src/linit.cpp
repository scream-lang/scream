/*
** $Id: linit.c $
** Initialization of libraries for hello.c and other clients
** See Copyright Notice in hello.h
*/


#define linit_c
#define HELLO_LIB

/*
** If you embed Hello in your program and need to open the standard
** libraries, call helloL_openlibs in your program. If you need a
** different set of libraries, copy this file to your project and edit
** it to suit your needs.
**
** You can also *preload* libraries, so that a later 'require' can
** open the library, which is already linked to the application.
** For that, do the following code:
**
**  helloL_getsubtable(L, HELLO_REGISTRYINDEX, HELLO_PRELOAD_TABLE);
**  hello_pushcfunction(L, helloopen_modname);
**  hello_setfield(L, -2, modname);
**  hello_pop(L, 1);  // remove PRELOAD table
*/

#include "lprefix.h"


#include <stddef.h>

#include "hello.h"

#include "hellolib.h"
#include "lauxlib.h"


/*
** these libs are loaded by hello.c and are readily available to any Hello
** program
*/
static const helloL_Reg loadedlibs[] = {
  {HELLO_GNAME, helloopen_base},
  {HELLO_LOADLIBNAME, helloopen_package},
  {HELLO_COLIBNAME, helloopen_coroutine},
  {HELLO_TABLIBNAME, helloopen_table},
  {HELLO_IOLIBNAME, helloopen_io},
  {HELLO_OSLIBNAME, helloopen_os},
  {HELLO_STRLIBNAME, helloopen_string},
  {HELLO_MATHLIBNAME, helloopen_math},
  {HELLO_UTF8LIBNAME, helloopen_utf8},
  {HELLO_DBLIBNAME, helloopen_debug},
  {NULL, NULL}
};


static const helloL_Reg preloadedLibs[] = {
#ifdef HELLO_USE_SOUP
  {"json", helloopen_json},
  {"base32", helloopen_base32},
  {"base58", helloopen_base58},
  {"base64", helloopen_base64},
#endif
  {"crypto", helloopen_crypto},
  {NULL, NULL}
};


HELLOLIB_API void helloL_openlibs (hello_State *L)
{
  const helloL_Reg *lib;

  for (lib = loadedlibs; lib->func; lib++)
  {
    helloL_requiref(L, lib->name, lib->func, 1);
    hello_pop(L, 1);  /* remove lib */
  }

  for (lib = preloadedLibs; lib->func; lib++)
  {
    helloL_getsubtable(L, HELLO_REGISTRYINDEX, HELLO_PRELOAD_TABLE);
    hello_pushcfunction(L, lib->func);
    hello_setfield(L, -2, lib->name);
    hello_pop(L, 1);
  }
}

