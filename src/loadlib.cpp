/*
** $Id: loadlib.c $
** Dynamic library loader for Hello
** See Copyright Notice in hello.h
**
** This module contains an implementation of loadlib for Unix systems
** that have dlfcn, an implementation for Windows, and a stub for other
** systems.
*/

#define loadlib_c
#define HELLO_LIB

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hello.h"

#include "lauxlib.h"
#include "hellolib.h"


/*
** HELLO_IGMARK is a mark to ignore all before it when building the
** helloopen_ function name.
*/
#if !defined (HELLO_IGMARK)
#define HELLO_IGMARK		"-"
#endif


/*
** HELLO_CSUBSEP is the character that replaces dots in submodule names
** when searching for a C loader.
** HELLO_LSUBSEP is the character that replaces dots in submodule names
** when searching for a Hello loader.
*/
#if !defined(HELLO_CSUBSEP)
#define HELLO_CSUBSEP		HELLO_DIRSEP
#endif

#if !defined(HELLO_LSUBSEP)
#define HELLO_LSUBSEP		HELLO_DIRSEP
#endif


/* prefix for open functions in C libraries */
#define HELLO_POF		"helloopen_"

/* separator for open functions in C libraries */
#define HELLO_OFSEP	"_"


/*
** key for table in the registry that keeps handles
** for all loaded C libraries
*/
static const char *const CLIBS = "_CLIBS";

#define LIB_FAIL	"open"


#define setprogdir(L)           ((void)0)


/*
** Special type equivalent to '(void*)' for functions in gcc
** (to suppress warnings when converting function pointers)
*/
typedef void (*voidf)(void);


/*
** system-dependent functions
*/

/*
** unload library 'lib'
*/
static void lsys_unloadlib (void *lib);

/*
** load C library in file 'path'. If 'seeglb', load with all names in
** the library global.
** Returns the library; in case of error, returns NULL plus an
** error string in the stack.
*/
static void *lsys_load (hello_State *L, const char *path, int seeglb);

/*
** Try to find a function named 'sym' in library 'lib'.
** Returns the function; in case of error, returns NULL plus an
** error string in the stack.
*/
static hello_CFunction lsys_sym (hello_State *L, void *lib, const char *sym);




#if defined(HELLO_USE_DLOPEN)	/* { */
/*
** {========================================================================
** This is an implementation of loadlib based on the dlfcn interface.
** The dlfcn interface is available in Linux, SunOS, Solaris, IRIX, FreeBSD,
** NetBSD, AIX 4.2, HPUX 11, and  probably most other Unix flavors, at least
** as an emulation layer on top of native functions.
** =========================================================================
*/

#include <dlfcn.h>

/*
** Macro to convert pointer-to-void* to pointer-to-function. This cast
** is undefined according to ISO C, but POSIX assumes that it works.
** (The '__extension__' in gnu compilers is only to avoid warnings.)
*/
#if defined(__GNUC__)
#define cast_func(p) (__extension__ (hello_CFunction)(p))
#else
#define cast_func(p) ((hello_CFunction)(p))
#endif


static void lsys_unloadlib (void *lib) {
  dlclose(lib);
}


static void *lsys_load (hello_State *L, const char *path, int seeglb) {
  void *lib = dlopen(path, RTLD_NOW | (seeglb ? RTLD_GLOBAL : RTLD_LOCAL));
  if (l_unlikely(lib == NULL))
    hello_pushstring(L, dlerror());
  return lib;
}


static hello_CFunction lsys_sym (hello_State *L, void *lib, const char *sym) {
  hello_CFunction f = cast_func(dlsym(lib, sym));
  if (l_unlikely(f == NULL))
    hello_pushstring(L, dlerror());
  return f;
}

/* }====================================================== */



#elif defined(HELLO_DL_DLL)	/* }{ */
/*
** {======================================================================
** This is an implementation of loadlib for Windows using native functions.
** =======================================================================
*/

#include <windows.h>


/*
** optional flags for LoadLibraryEx
*/
#if !defined(HELLO_LLE_FLAGS)
#define HELLO_LLE_FLAGS	0
#endif


#undef setprogdir


/*
** Replace in the path (on the top of the stack) any occurrence
** of HELLO_EXEC_DIR with the executable's path.
*/
static void setprogdir (hello_State *L) {
  char buff[MAX_PATH + 1];
  char *lb;
  DWORD nsize = sizeof(buff)/sizeof(char);
  DWORD n = GetModuleFileNameA(NULL, buff, nsize);  /* get exec. name */
  if (n == 0 || n == nsize || (lb = strrchr(buff, '\\')) == NULL)
    helloL_error(L, "unable to get ModuleFileName");
  else {
    *lb = '\0';  /* cut name on the last '\\' to get the path */
    helloL_gsub(L, hello_tostring(L, -1), HELLO_EXEC_DIR, buff);
    hello_remove(L, -2);  /* remove original string */
  }
}




static void pusherror (hello_State *L) {
  int error = GetLastError();
  char buffer[128];
  if (FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
      NULL, error, 0, buffer, sizeof(buffer)/sizeof(char), NULL))
    hello_pushstring(L, buffer);
  else
    hello_pushfstring(L, "system error %d\n", error);
}

static void lsys_unloadlib (void *lib) {
  FreeLibrary((HMODULE)lib);
}


static void *lsys_load (hello_State *L, const char *path, int seeglb) {
  HMODULE lib = LoadLibraryExA(path, NULL, HELLO_LLE_FLAGS);
  (void)(seeglb);  /* not used: symbols are 'global' by default */
  if (lib == NULL) pusherror(L);
  return lib;
}


static hello_CFunction lsys_sym (hello_State *L, void *lib, const char *sym) {
  hello_CFunction f = (hello_CFunction)(voidf)GetProcAddress((HMODULE)lib, sym);
  if (f == NULL) pusherror(L);
  return f;
}

/* }====================================================== */


#else				/* }{ */
/*
** {======================================================
** Fallback for other systems
** =======================================================
*/

#undef LIB_FAIL
#define LIB_FAIL	"absent"


#define DLMSG	"dynamic libraries not enabled; check your Hello installation"


static void lsys_unloadlib (void *lib) {
  (void)(lib);  /* not used */
}


static void *lsys_load (hello_State *L, const char *path, int seeglb) {
  (void)(path); (void)(seeglb);  /* not used */
  hello_pushliteral(L, DLMSG);
  return NULL;
}


static hello_CFunction lsys_sym (hello_State *L, void *lib, const char *sym) {
  (void)(lib); (void)(sym);  /* not used */
  hello_pushliteral(L, DLMSG);
  return NULL;
}

/* }====================================================== */
#endif				/* } */


/*
** {==================================================================
** Set Paths
** ===================================================================
*/

/*
** HELLO_PATH_VAR and HELLO_CPATH_VAR are the names of the environment
** variables that Hello check to set its paths.
*/
#if !defined(HELLO_PATH_VAR)
#define HELLO_PATH_VAR    "HELLO_PATH"
#endif

#if !defined(HELLO_CPATH_VAR)
#define HELLO_CPATH_VAR   "HELLO_CPATH"
#endif



/*
** return registry.HELLO_NOENV as a boolean
*/
static int noenv (hello_State *L) {
  int b;
  hello_getfield(L, HELLO_REGISTRYINDEX, "HELLO_NOENV");
  b = hello_toboolean(L, -1);
  hello_pop(L, 1);  /* remove value */
  return b;
}


/*
** Set a path
*/
static void setpath (hello_State *L, const char *fieldname,
                                   const char *envname,
                                   const char *dft) {
  const char *dftmark;
  const char *nver = hello_pushfstring(L, "%s%s", envname, HELLO_VERSUFFIX);
  const char *path = getenv(nver);  /* try versioned name */
  if (path == NULL)  /* no versioned environment variable? */
    path = getenv(envname);  /* try unversioned name */
  if (path == NULL || noenv(L))  /* no environment variable? */
    hello_pushstring(L, dft);  /* use default */
  else if ((dftmark = strstr(path, HELLO_PATH_SEP HELLO_PATH_SEP)) == NULL)
    hello_pushstring(L, path);  /* nothing to change */
  else {  /* path contains a ";;": insert default path in its place */
    size_t len = strlen(path);
    helloL_Buffer b;
    helloL_buffinit(L, &b);
    if (path < dftmark) {  /* is there a prefix before ';;'? */
      helloL_addlstring(&b, path, dftmark - path);  /* add it */
      helloL_addchar(&b, *HELLO_PATH_SEP);
    }
    helloL_addstring(&b, dft);  /* add default */
    if (dftmark < path + len - 2) {  /* is there a suffix after ';;'? */
      helloL_addchar(&b, *HELLO_PATH_SEP);
      helloL_addlstring(&b, dftmark + 2, (path + len - 2) - dftmark);
    }
    helloL_pushresult(&b);
  }
  setprogdir(L);
  hello_setfield(L, -3, fieldname);  /* package[fieldname] = path value */
  hello_pop(L, 1);  /* pop versioned variable name ('nver') */
}

/* }================================================================== */


/*
** return registry.CLIBS[path]
*/
static void *checkclib (hello_State *L, const char *path) {
  void *plib;
  hello_getfield(L, HELLO_REGISTRYINDEX, CLIBS);
  hello_getfield(L, -1, path);
  plib = hello_touserdata(L, -1);  /* plib = CLIBS[path] */
  hello_pop(L, 2);  /* pop CLIBS table and 'plib' */
  return plib;
}


/*
** registry.CLIBS[path] = plib        -- for queries
** registry.CLIBS[#CLIBS + 1] = plib  -- also keep a list of all libraries
*/
static void addtoclib (hello_State *L, const char *path, void *plib) {
  hello_getfield(L, HELLO_REGISTRYINDEX, CLIBS);
  hello_pushlightuserdata(L, plib);
  hello_pushvalue(L, -1);
  hello_setfield(L, -3, path);  /* CLIBS[path] = plib */
  hello_rawseti(L, -2, helloL_len(L, -2) + 1);  /* CLIBS[#CLIBS + 1] = plib */
  hello_pop(L, 1);  /* pop CLIBS table */
}


/*
** __gc tag method for CLIBS table: calls 'lsys_unloadlib' for all lib
** handles in list CLIBS
*/
static int gctm (hello_State *L) {
  hello_Integer n = helloL_len(L, 1);
  for (; n >= 1; n--) {  /* for each handle, in reverse order */
    hello_rawgeti(L, 1, n);  /* get handle CLIBS[n] */
    lsys_unloadlib(hello_touserdata(L, -1));
    hello_pop(L, 1);  /* pop handle */
  }
  return 0;
}



/* error codes for 'lookforfunc' */
#define ERRLIB		1
#define ERRFUNC		2

/*
** Look for a C function named 'sym' in a dynamically loaded library
** 'path'.
** First, check whether the library is already loaded; if not, try
** to load it.
** Then, if 'sym' is '*', return true (as library has been loaded).
** Otherwise, look for symbol 'sym' in the library and push a
** C function with that symbol.
** Return 0 and 'true' or a function in the stack; in case of
** errors, return an error code and an error message in the stack.
*/
static int lookforfunc (hello_State *L, const char *path, const char *sym) {
  void *reg = checkclib(L, path);  /* check loaded C libraries */
  if (reg == NULL) {  /* must load library? */
    reg = lsys_load(L, path, *sym == '*');  /* global symbols if 'sym'=='*' */
    if (reg == NULL) return ERRLIB;  /* unable to load library */
    addtoclib(L, path, reg);
  }
  if (*sym == '*') {  /* loading only library (no function)? */
    hello_pushboolean(L, 1);  /* return 'true' */
    return 0;  /* no errors */
  }
  else {
    hello_CFunction f = lsys_sym(L, reg, sym);
    if (f == NULL)
      return ERRFUNC;  /* unable to find function */
    hello_pushcfunction(L, f);  /* else create new function */
    return 0;  /* no errors */
  }
}


static int ll_loadlib (hello_State *L) {
  const char *path = helloL_checkstring(L, 1);
  const char *init = helloL_checkstring(L, 2);
  int stat = lookforfunc(L, path, init);
  if (l_likely(stat == 0))  /* no errors? */
    return 1;  /* return the loaded function */
  else {  /* error; error message is on stack top */
    helloL_pushfail(L);
    hello_insert(L, -2);
    hello_pushstring(L, (stat == ERRLIB) ?  LIB_FAIL : "init");
    return 3;  /* return fail, error message, and where */
  }
}



/*
** {======================================================
** 'require' function
** =======================================================
*/


static int readable (const char *filename) {
  FILE *f = helloL_fopen(filename, strlen(filename), "r", sizeof("r") - sizeof(char));  /* try to open file */
  if (f == NULL) return 0;  /* open failed */
  fclose(f);
  return 1;
}


/*
** Get the next name in '*path' = 'name1;name2;name3;...', changing
** the ending ';' to '\0' to create a zero-terminated string. Return
** NULL when list ends.
*/
static const char *getnextfilename (char **path, char *end) {
  char *sep;
  char *name = *path;
  if (name == end)
    return NULL;  /* no more names */
  else if (*name == '\0') {  /* from previous iteration? */
    *name = *HELLO_PATH_SEP;  /* restore separator */
    name++;  /* skip it */
  }
  sep = strchr(name, *HELLO_PATH_SEP);  /* find next separator */
  if (sep == NULL)  /* separator not found? */
    sep = end;  /* name goes until the end */
  *sep = '\0';  /* finish file name */
  *path = sep;  /* will start next search from here */
  return name;
}


/*
** Given a path such as ";blabla.so;blublu.so", pushes the string
**
** no file 'blabla.so'
**	no file 'blublu.so'
*/
static void pusherrornotfound (hello_State *L, const char *path) {
  helloL_Buffer b;
  helloL_buffinit(L, &b);
  helloL_addstring(&b, "no file '");
  helloL_addgsub(&b, path, HELLO_PATH_SEP, "'\n\tno file '");
  helloL_addstring(&b, "'");
  helloL_pushresult(&b);
}


static const char *searchpath (hello_State *L, const char *name,
                                             const char *path,
                                             const char *sep,
                                             const char *dirsep) {
  helloL_Buffer buff;
  char *pathname;  /* path with name inserted */
  char *endpathname;  /* its end */
  const char *filename;
  /* separator is non-empty and appears in 'name'? */
  if (*sep != '\0' && strchr(name, *sep) != NULL)
    name = helloL_gsub(L, name, sep, dirsep);  /* replace it by 'dirsep' */
  helloL_buffinit(L, &buff);
  /* add path to the buffer, replacing marks ('?') with the file name */
  helloL_addgsub(&buff, path, HELLO_PATH_MARK, name);
  helloL_addchar(&buff, '\0');
  pathname = helloL_buffaddr(&buff);  /* writable list of file names */
  endpathname = pathname + helloL_bufflen(&buff) - 1;
  while ((filename = getnextfilename(&pathname, endpathname)) != NULL) {
    if (readable(filename))  /* does file exist and is readable? */
      return hello_pushstring(L, filename);  /* save and return name */
  }
  helloL_pushresult(&buff);  /* push path to create error message */
  pusherrornotfound(L, hello_tostring(L, -1));  /* create error message */
  return NULL;  /* not found */
}


static int ll_searchpath (hello_State *L) {
  const char *f = searchpath(L, helloL_checkstring(L, 1),
                                helloL_checkstring(L, 2),
                                helloL_optstring(L, 3, "."),
                                helloL_optstring(L, 4, HELLO_DIRSEP));
  if (f != NULL) return 1;
  else {  /* error message is on top of the stack */
    helloL_pushfail(L);
    hello_insert(L, -2);
    return 2;  /* return fail + error message */
  }
}


static const char *findfile (hello_State *L, const char *name,
                                           const char *pname,
                                           const char *dirsep) {
  const char *path;
  hello_getfield(L, hello_upvalueindex(1), pname);
  path = hello_tostring(L, -1);
  if (l_unlikely(path == NULL))
    helloL_error(L, "'package.%s' must be a string", pname);
  return searchpath(L, name, path, ".", dirsep);
}


static int checkload (hello_State *L, int stat, const char *filename) {
  if (l_likely(stat)) {  /* module loaded successfully? */
    hello_pushstring(L, filename);  /* will be 2nd argument to module */
    return 2;  /* return open function and file name */
  }
  else
    helloL_error(L, "error loading module '%s' from file '%s':\n\t%s",
                          hello_tostring(L, 1), filename, hello_tostring(L, -1));
}


static int searcher_Hello (hello_State *L) {
  const char *filename;
  const char *name = helloL_checkstring(L, 1);
  filename = findfile(L, name, "path", HELLO_LSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (helloL_loadfile(L, filename) == HELLO_OK), filename);
}


/*
** Try to find a load function for module 'modname' at file 'filename'.
** First, change '.' to '_' in 'modname'; then, if 'modname' has
** the form X-Y (that is, it has an "ignore mark"), build a function
** name "helloopen_X" and look for it. (For compatibility, if that
** fails, it also tries "helloopen_Y".) If there is no ignore mark,
** look for a function named "helloopen_modname".
*/
static int loadfunc (hello_State *L, const char *filename, const char *modname) {
  const char *openfunc;
  const char *mark;
  modname = helloL_gsub(L, modname, ".", HELLO_OFSEP);
  mark = strchr(modname, *HELLO_IGMARK);
  if (mark) {
    int stat;
    openfunc = hello_pushlstring(L, modname, mark - modname);
    openfunc = hello_pushfstring(L, HELLO_POF"%s", openfunc);
    stat = lookforfunc(L, filename, openfunc);
    if (stat != ERRFUNC) return stat;
    modname = mark + 1;  /* else go ahead and try old-style name */
  }
  openfunc = hello_pushfstring(L, HELLO_POF"%s", modname);
  return lookforfunc(L, filename, openfunc);
}


static int searcher_C (hello_State *L) {
  const char *name = helloL_checkstring(L, 1);
  const char *filename = findfile(L, name, "cpath", HELLO_CSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (loadfunc(L, filename, name) == 0), filename);
}


static int searcher_Croot (hello_State *L) {
  const char *filename;
  const char *name = helloL_checkstring(L, 1);
  const char *p = strchr(name, '.');
  int stat;
  if (p == NULL) return 0;  /* is root */
  hello_pushlstring(L, name, p - name);
  filename = findfile(L, hello_tostring(L, -1), "cpath", HELLO_CSUBSEP);
  if (filename == NULL) return 1;  /* root not found */
  if ((stat = loadfunc(L, filename, name)) != 0) {
    if (stat != ERRFUNC)
      return checkload(L, 0, filename);  /* real error */
    else {  /* open function not found */
      hello_pushfstring(L, "no module '%s' in file '%s'", name, filename);
      return 1;
    }
  }
  hello_pushstring(L, filename);  /* will be 2nd argument to module */
  return 2;
}


static int searcher_preload (hello_State *L) {
  const char *name = helloL_checkstring(L, 1);
  hello_getfield(L, HELLO_REGISTRYINDEX, HELLO_PRELOAD_TABLE);
  if (hello_getfield(L, -1, name) == HELLO_TNIL) {  /* not found? */
    hello_pushfstring(L, "no field package.preload['%s']", name);
    return 1;
  }
  else {
    hello_pushliteral(L, ":preload:");
    return 2;
  }
}


static void findloader (hello_State *L, const char *name) {
  int i;
  helloL_Buffer msg;  /* to build error message */
  /* push 'package.searchers' to index 3 in the stack */
  if (l_unlikely(hello_getfield(L, hello_upvalueindex(1), "searchers")
                 != HELLO_TTABLE))
    helloL_error(L, "'package.searchers' must be a table");
  helloL_buffinit(L, &msg);
  /*  iterate over available searchers to find a loader */
  for (i = 1; ; i++) {
    helloL_addstring(&msg, "\n\t");  /* error-message prefix */
    if (l_unlikely(hello_rawgeti(L, 3, i) == HELLO_TNIL)) {  /* no more searchers? */
      hello_pop(L, 1);  /* remove nil */
      helloL_buffsub(&msg, 2);  /* remove prefix */
      helloL_pushresult(&msg);  /* create error message */
      helloL_error(L, "module '%s' not found:%s", name, hello_tostring(L, -1));
    }
    hello_pushstring(L, name);
    hello_call(L, 1, 2);  /* call it */
    if (hello_isfunction(L, -2))  /* did it find a loader? */
      return;  /* module loader found */
    else if (hello_isstring(L, -2)) {  /* searcher returned error message? */
      hello_pop(L, 1);  /* remove extra return */
      helloL_addvalue(&msg);  /* concatenate error message */
    }
    else {  /* no error message */
      hello_pop(L, 2);  /* remove both returns */
      helloL_buffsub(&msg, 2);  /* remove prefix */
    }
  }
}


static int ll_require (hello_State *L) {
  const char *name = helloL_checkstring(L, 1);
  hello_settop(L, 1);  /* LOADED table will be at index 2 */
  hello_getfield(L, HELLO_REGISTRYINDEX, HELLO_LOADED_TABLE);
  hello_getfield(L, 2, name);  /* LOADED[name] */
  if (hello_toboolean(L, -1))  /* is it there? */
    return 1;  /* package is already loaded */
  /* else must load package */
  hello_pop(L, 1);  /* remove 'getfield' result */
  findloader(L, name);
  hello_rotate(L, -2, 1);  /* function <-> loader data */
  hello_pushvalue(L, 1);  /* name is 1st argument to module loader */
  hello_pushvalue(L, -3);  /* loader data is 2nd argument */
  /* stack: ...; loader data; loader function; mod. name; loader data */
  hello_call(L, 2, 1);  /* run loader to load module */
  /* stack: ...; loader data; result from loader */
  if (!hello_isnil(L, -1))  /* non-nil return? */
    hello_setfield(L, 2, name);  /* LOADED[name] = returned value */
  else
    hello_pop(L, 1);  /* pop nil */
  if (hello_getfield(L, 2, name) == HELLO_TNIL) {   /* module set no value? */
    hello_pushboolean(L, 1);  /* use true as result */
    hello_copy(L, -1, -2);  /* replace loader result */
    hello_setfield(L, 2, name);  /* LOADED[name] = true */
  }
  hello_rotate(L, -2, 1);  /* loader data <-> module result  */
  return 2;  /* return module result and loader data */
}

/* }====================================================== */




static const helloL_Reg pk_funcs[] = {
  {"loadlib", ll_loadlib},
  {"searchpath", ll_searchpath},
  /* placeholders */
  {"preload", NULL},
  {"cpath", NULL},
  {"path", NULL},
  {"searchers", NULL},
  {"loaded", NULL},
  {NULL, NULL}
};


static const helloL_Reg ll_funcs[] = {
  {"require", ll_require},
  {NULL, NULL}
};


static void createsearcherstable (hello_State *L) {
  static const hello_CFunction searchers[] =
    {searcher_preload, searcher_Hello, searcher_C, searcher_Croot, NULL};
  int i;
  /* create 'searchers' table */
  hello_createtable(L, sizeof(searchers)/sizeof(searchers[0]) - 1, 0);
  /* fill it with predefined searchers */
  for (i=0; searchers[i] != NULL; i++) {
    hello_pushvalue(L, -2);  /* set 'package' as upvalue for all searchers */
    hello_pushcclosure(L, searchers[i], 1);
    hello_rawseti(L, -2, i+1);
  }
  hello_setfield(L, -2, "searchers");  /* put it in field 'searchers' */
}


/*
** create table CLIBS to keep track of loaded C libraries,
** setting a finalizer to close all libraries when closing state.
*/
static void createclibstable (hello_State *L) {
  helloL_getsubtable(L, HELLO_REGISTRYINDEX, CLIBS);  /* create CLIBS table */
  hello_createtable(L, 0, 1);  /* create metatable for CLIBS */
  hello_pushcfunction(L, gctm);
  hello_setfield(L, -2, "__gc");  /* set finalizer for CLIBS table */
  hello_setmetatable(L, -2);
}


HELLOMOD_API int helloopen_package (hello_State *L) {
  createclibstable(L);
  helloL_newlib(L, pk_funcs);  /* create 'package' table */
  createsearcherstable(L);
  /* set paths */
  setpath(L, "path", HELLO_PATH_VAR, HELLO_PATH_DEFAULT);
  setpath(L, "cpath", HELLO_CPATH_VAR, HELLO_CPATH_DEFAULT);
  /* store config information */
  hello_pushliteral(L, HELLO_DIRSEP "\n" HELLO_PATH_SEP "\n" HELLO_PATH_MARK "\n"
                     HELLO_EXEC_DIR "\n" HELLO_IGMARK "\n");
  hello_setfield(L, -2, "config");
  /* set field 'loaded' */
  helloL_getsubtable(L, HELLO_REGISTRYINDEX, HELLO_LOADED_TABLE);
  hello_setfield(L, -2, "loaded");
  /* set field 'preload' */
  helloL_getsubtable(L, HELLO_REGISTRYINDEX, HELLO_PRELOAD_TABLE);
  hello_setfield(L, -2, "preload");
  hello_pushglobaltable(L);
  hello_pushvalue(L, -2);  /* set 'package' as upvalue for next lib */
  helloL_setfuncs(L, ll_funcs, 1);  /* open lib into global table */
  hello_pop(L, 1);  /* pop global table */
  return 1;  /* return 'package' table */
}

