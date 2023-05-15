/*
** $Id: loadlib.c $
** Dynamic library loader for Mask
** See Copyright Notice in mask.h
**
** This module contains an implementation of loadlib for Unix systems
** that have dlfcn, an implementation for Windows, and a stub for other
** systems.
*/

#define loadlib_c
#define MASK_LIB

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mask.h"

#include "lauxlib.h"
#include "masklib.h"


/*
** MASK_IGMARK is a mark to ignore all before it when building the
** maskopen_ function name.
*/
#if !defined (MASK_IGMARK)
#define MASK_IGMARK		"-"
#endif


/*
** MASK_CSUBSEP is the character that replaces dots in submodule names
** when searching for a C loader.
** MASK_LSUBSEP is the character that replaces dots in submodule names
** when searching for a Mask loader.
*/
#if !defined(MASK_CSUBSEP)
#define MASK_CSUBSEP		MASK_DIRSEP
#endif

#if !defined(MASK_LSUBSEP)
#define MASK_LSUBSEP		MASK_DIRSEP
#endif


/* prefix for open functions in C libraries */
#define MASK_POF		"maskopen_"

/* separator for open functions in C libraries */
#define MASK_OFSEP	"_"


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
static void *lsys_load (mask_State *L, const char *path, int seeglb);

/*
** Try to find a function named 'sym' in library 'lib'.
** Returns the function; in case of error, returns NULL plus an
** error string in the stack.
*/
static mask_CFunction lsys_sym (mask_State *L, void *lib, const char *sym);




#if defined(MASK_USE_DLOPEN)	/* { */
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
#define cast_func(p) (__extension__ (mask_CFunction)(p))
#else
#define cast_func(p) ((mask_CFunction)(p))
#endif


static void lsys_unloadlib (void *lib) {
  dlclose(lib);
}


static void *lsys_load (mask_State *L, const char *path, int seeglb) {
  void *lib = dlopen(path, RTLD_NOW | (seeglb ? RTLD_GLOBAL : RTLD_LOCAL));
  if (l_unlikely(lib == NULL))
    mask_pushstring(L, dlerror());
  return lib;
}


static mask_CFunction lsys_sym (mask_State *L, void *lib, const char *sym) {
  mask_CFunction f = cast_func(dlsym(lib, sym));
  if (l_unlikely(f == NULL))
    mask_pushstring(L, dlerror());
  return f;
}

/* }====================================================== */



#elif defined(MASK_DL_DLL)	/* }{ */
/*
** {======================================================================
** This is an implementation of loadlib for Windows using native functions.
** =======================================================================
*/

#include <windows.h>


/*
** optional flags for LoadLibraryEx
*/
#if !defined(MASK_LLE_FLAGS)
#define MASK_LLE_FLAGS	0
#endif


#undef setprogdir


/*
** Replace in the path (on the top of the stack) any occurrence
** of MASK_EXEC_DIR with the executable's path.
*/
static void setprogdir (mask_State *L) {
  char buff[MAX_PATH + 1];
  char *lb;
  DWORD nsize = sizeof(buff)/sizeof(char);
  DWORD n = GetModuleFileNameA(NULL, buff, nsize);  /* get exec. name */
  if (n == 0 || n == nsize || (lb = strrchr(buff, '\\')) == NULL)
    maskL_error(L, "unable to get ModuleFileName");
  else {
    *lb = '\0';  /* cut name on the last '\\' to get the path */
    maskL_gsub(L, mask_tostring(L, -1), MASK_EXEC_DIR, buff);
    mask_remove(L, -2);  /* remove original string */
  }
}




static void pusherror (mask_State *L) {
  int error = GetLastError();
  char buffer[128];
  if (FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
      NULL, error, 0, buffer, sizeof(buffer)/sizeof(char), NULL))
    mask_pushstring(L, buffer);
  else
    mask_pushfstring(L, "system error %d\n", error);
}

static void lsys_unloadlib (void *lib) {
  FreeLibrary((HMODULE)lib);
}


static void *lsys_load (mask_State *L, const char *path, int seeglb) {
  HMODULE lib = LoadLibraryExA(path, NULL, MASK_LLE_FLAGS);
  (void)(seeglb);  /* not used: symbols are 'global' by default */
  if (lib == NULL) pusherror(L);
  return lib;
}


static mask_CFunction lsys_sym (mask_State *L, void *lib, const char *sym) {
  mask_CFunction f = (mask_CFunction)(voidf)GetProcAddress((HMODULE)lib, sym);
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


#define DLMSG	"dynamic libraries not enabled; check your Mask installation"


static void lsys_unloadlib (void *lib) {
  (void)(lib);  /* not used */
}


static void *lsys_load (mask_State *L, const char *path, int seeglb) {
  (void)(path); (void)(seeglb);  /* not used */
  mask_pushliteral(L, DLMSG);
  return NULL;
}


static mask_CFunction lsys_sym (mask_State *L, void *lib, const char *sym) {
  (void)(lib); (void)(sym);  /* not used */
  mask_pushliteral(L, DLMSG);
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
** MASK_PATH_VAR and MASK_CPATH_VAR are the names of the environment
** variables that Mask check to set its paths.
*/
#if !defined(MASK_PATH_VAR)
#define MASK_PATH_VAR    "MASK_PATH"
#endif

#if !defined(MASK_CPATH_VAR)
#define MASK_CPATH_VAR   "MASK_CPATH"
#endif



/*
** return registry.MASK_NOENV as a boolean
*/
static int noenv (mask_State *L) {
  int b;
  mask_getfield(L, MASK_REGISTRYINDEX, "MASK_NOENV");
  b = mask_toboolean(L, -1);
  mask_pop(L, 1);  /* remove value */
  return b;
}


/*
** Set a path
*/
static void setpath (mask_State *L, const char *fieldname,
                                   const char *envname,
                                   const char *dft) {
  const char *dftmark;
  const char *nver = mask_pushfstring(L, "%s%s", envname, MASK_VERSUFFIX);
  const char *path = getenv(nver);  /* try versioned name */
  if (path == NULL)  /* no versioned environment variable? */
    path = getenv(envname);  /* try unversioned name */
  if (path == NULL || noenv(L))  /* no environment variable? */
    mask_pushstring(L, dft);  /* use default */
  else if ((dftmark = strstr(path, MASK_PATH_SEP MASK_PATH_SEP)) == NULL)
    mask_pushstring(L, path);  /* nothing to change */
  else {  /* path contains a ";;": insert default path in its place */
    size_t len = strlen(path);
    maskL_Buffer b;
    maskL_buffinit(L, &b);
    if (path < dftmark) {  /* is there a prefix before ';;'? */
      maskL_addlstring(&b, path, dftmark - path);  /* add it */
      maskL_addchar(&b, *MASK_PATH_SEP);
    }
    maskL_addstring(&b, dft);  /* add default */
    if (dftmark < path + len - 2) {  /* is there a suffix after ';;'? */
      maskL_addchar(&b, *MASK_PATH_SEP);
      maskL_addlstring(&b, dftmark + 2, (path + len - 2) - dftmark);
    }
    maskL_pushresult(&b);
  }
  setprogdir(L);
  mask_setfield(L, -3, fieldname);  /* package[fieldname] = path value */
  mask_pop(L, 1);  /* pop versioned variable name ('nver') */
}

/* }================================================================== */


/*
** return registry.CLIBS[path]
*/
static void *checkclib (mask_State *L, const char *path) {
  void *plib;
  mask_getfield(L, MASK_REGISTRYINDEX, CLIBS);
  mask_getfield(L, -1, path);
  plib = mask_touserdata(L, -1);  /* plib = CLIBS[path] */
  mask_pop(L, 2);  /* pop CLIBS table and 'plib' */
  return plib;
}


/*
** registry.CLIBS[path] = plib        -- for queries
** registry.CLIBS[#CLIBS + 1] = plib  -- also keep a list of all libraries
*/
static void addtoclib (mask_State *L, const char *path, void *plib) {
  mask_getfield(L, MASK_REGISTRYINDEX, CLIBS);
  mask_pushlightuserdata(L, plib);
  mask_pushvalue(L, -1);
  mask_setfield(L, -3, path);  /* CLIBS[path] = plib */
  mask_rawseti(L, -2, maskL_len(L, -2) + 1);  /* CLIBS[#CLIBS + 1] = plib */
  mask_pop(L, 1);  /* pop CLIBS table */
}


/*
** __gc tag method for CLIBS table: calls 'lsys_unloadlib' for all lib
** handles in list CLIBS
*/
static int gctm (mask_State *L) {
  mask_Integer n = maskL_len(L, 1);
  for (; n >= 1; n--) {  /* for each handle, in reverse order */
    mask_rawgeti(L, 1, n);  /* get handle CLIBS[n] */
    lsys_unloadlib(mask_touserdata(L, -1));
    mask_pop(L, 1);  /* pop handle */
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
static int lookforfunc (mask_State *L, const char *path, const char *sym) {
  void *reg = checkclib(L, path);  /* check loaded C libraries */
  if (reg == NULL) {  /* must load library? */
    reg = lsys_load(L, path, *sym == '*');  /* global symbols if 'sym'=='*' */
    if (reg == NULL) return ERRLIB;  /* unable to load library */
    addtoclib(L, path, reg);
  }
  if (*sym == '*') {  /* loading only library (no function)? */
    mask_pushboolean(L, 1);  /* return 'true' */
    return 0;  /* no errors */
  }
  else {
    mask_CFunction f = lsys_sym(L, reg, sym);
    if (f == NULL)
      return ERRFUNC;  /* unable to find function */
    mask_pushcfunction(L, f);  /* else create new function */
    return 0;  /* no errors */
  }
}


static int ll_loadlib (mask_State *L) {
  const char *path = maskL_checkstring(L, 1);
  const char *init = maskL_checkstring(L, 2);
  int stat = lookforfunc(L, path, init);
  if (l_likely(stat == 0))  /* no errors? */
    return 1;  /* return the loaded function */
  else {  /* error; error message is on stack top */
    maskL_pushfail(L);
    mask_insert(L, -2);
    mask_pushstring(L, (stat == ERRLIB) ?  LIB_FAIL : "init");
    return 3;  /* return fail, error message, and where */
  }
}



/*
** {======================================================
** 'require' function
** =======================================================
*/


static int readable (const char *filename) {
  FILE *f = maskL_fopen(filename, strlen(filename), "r", sizeof("r") - sizeof(char));  /* try to open file */
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
    *name = *MASK_PATH_SEP;  /* restore separator */
    name++;  /* skip it */
  }
  sep = strchr(name, *MASK_PATH_SEP);  /* find next separator */
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
static void pusherrornotfound (mask_State *L, const char *path) {
  maskL_Buffer b;
  maskL_buffinit(L, &b);
  maskL_addstring(&b, "no file '");
  maskL_addgsub(&b, path, MASK_PATH_SEP, "'\n\tno file '");
  maskL_addstring(&b, "'");
  maskL_pushresult(&b);
}


static const char *searchpath (mask_State *L, const char *name,
                                             const char *path,
                                             const char *sep,
                                             const char *dirsep) {
  maskL_Buffer buff;
  char *pathname;  /* path with name inserted */
  char *endpathname;  /* its end */
  const char *filename;
  /* separator is non-empty and appears in 'name'? */
  if (*sep != '\0' && strchr(name, *sep) != NULL)
    name = maskL_gsub(L, name, sep, dirsep);  /* replace it by 'dirsep' */
  maskL_buffinit(L, &buff);
  /* add path to the buffer, replacing marks ('?') with the file name */
  maskL_addgsub(&buff, path, MASK_PATH_MARK, name);
  maskL_addchar(&buff, '\0');
  pathname = maskL_buffaddr(&buff);  /* writable list of file names */
  endpathname = pathname + maskL_bufflen(&buff) - 1;
  while ((filename = getnextfilename(&pathname, endpathname)) != NULL) {
    if (readable(filename))  /* does file exist and is readable? */
      return mask_pushstring(L, filename);  /* save and return name */
  }
  maskL_pushresult(&buff);  /* push path to create error message */
  pusherrornotfound(L, mask_tostring(L, -1));  /* create error message */
  return NULL;  /* not found */
}


static int ll_searchpath (mask_State *L) {
  const char *f = searchpath(L, maskL_checkstring(L, 1),
                                maskL_checkstring(L, 2),
                                maskL_optstring(L, 3, "."),
                                maskL_optstring(L, 4, MASK_DIRSEP));
  if (f != NULL) return 1;
  else {  /* error message is on top of the stack */
    maskL_pushfail(L);
    mask_insert(L, -2);
    return 2;  /* return fail + error message */
  }
}


static const char *findfile (mask_State *L, const char *name,
                                           const char *pname,
                                           const char *dirsep) {
  const char *path;
  mask_getfield(L, mask_upvalueindex(1), pname);
  path = mask_tostring(L, -1);
  if (l_unlikely(path == NULL))
    maskL_error(L, "'package.%s' must be a string", pname);
  return searchpath(L, name, path, ".", dirsep);
}


static int checkload (mask_State *L, int stat, const char *filename) {
  if (l_likely(stat)) {  /* module loaded successfully? */
    mask_pushstring(L, filename);  /* will be 2nd argument to module */
    return 2;  /* return open function and file name */
  }
  else
    maskL_error(L, "error loading module '%s' from file '%s':\n\t%s",
                          mask_tostring(L, 1), filename, mask_tostring(L, -1));
}


static int searcher_Mask (mask_State *L) {
  const char *filename;
  const char *name = maskL_checkstring(L, 1);
  filename = findfile(L, name, "path", MASK_LSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (maskL_loadfile(L, filename) == MASK_OK), filename);
}


/*
** Try to find a load function for module 'modname' at file 'filename'.
** First, change '.' to '_' in 'modname'; then, if 'modname' has
** the form X-Y (that is, it has an "ignore mark"), build a function
** name "maskopen_X" and look for it. (For compatibility, if that
** fails, it also tries "maskopen_Y".) If there is no ignore mark,
** look for a function named "maskopen_modname".
*/
static int loadfunc (mask_State *L, const char *filename, const char *modname) {
  const char *openfunc;
  const char *mark;
  modname = maskL_gsub(L, modname, ".", MASK_OFSEP);
  mark = strchr(modname, *MASK_IGMARK);
  if (mark) {
    int stat;
    openfunc = mask_pushlstring(L, modname, mark - modname);
    openfunc = mask_pushfstring(L, MASK_POF"%s", openfunc);
    stat = lookforfunc(L, filename, openfunc);
    if (stat != ERRFUNC) return stat;
    modname = mark + 1;  /* else go ahead and try old-style name */
  }
  openfunc = mask_pushfstring(L, MASK_POF"%s", modname);
  return lookforfunc(L, filename, openfunc);
}


static int searcher_C (mask_State *L) {
  const char *name = maskL_checkstring(L, 1);
  const char *filename = findfile(L, name, "cpath", MASK_CSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (loadfunc(L, filename, name) == 0), filename);
}


static int searcher_Croot (mask_State *L) {
  const char *filename;
  const char *name = maskL_checkstring(L, 1);
  const char *p = strchr(name, '.');
  int stat;
  if (p == NULL) return 0;  /* is root */
  mask_pushlstring(L, name, p - name);
  filename = findfile(L, mask_tostring(L, -1), "cpath", MASK_CSUBSEP);
  if (filename == NULL) return 1;  /* root not found */
  if ((stat = loadfunc(L, filename, name)) != 0) {
    if (stat != ERRFUNC)
      return checkload(L, 0, filename);  /* real error */
    else {  /* open function not found */
      mask_pushfstring(L, "no module '%s' in file '%s'", name, filename);
      return 1;
    }
  }
  mask_pushstring(L, filename);  /* will be 2nd argument to module */
  return 2;
}


static int searcher_preload (mask_State *L) {
  const char *name = maskL_checkstring(L, 1);
  mask_getfield(L, MASK_REGISTRYINDEX, MASK_PRELOAD_TABLE);
  if (mask_getfield(L, -1, name) == MASK_TNIL) {  /* not found? */
    mask_pushfstring(L, "no field package.preload['%s']", name);
    return 1;
  }
  else {
    mask_pushliteral(L, ":preload:");
    return 2;
  }
}


static void findloader (mask_State *L, const char *name) {
  int i;
  maskL_Buffer msg;  /* to build error message */
  /* push 'package.searchers' to index 3 in the stack */
  if (l_unlikely(mask_getfield(L, mask_upvalueindex(1), "searchers")
                 != MASK_TTABLE))
    maskL_error(L, "'package.searchers' must be a table");
  maskL_buffinit(L, &msg);
  /*  iterate over available searchers to find a loader */
  for (i = 1; ; i++) {
    maskL_addstring(&msg, "\n\t");  /* error-message prefix */
    if (l_unlikely(mask_rawgeti(L, 3, i) == MASK_TNIL)) {  /* no more searchers? */
      mask_pop(L, 1);  /* remove nil */
      maskL_buffsub(&msg, 2);  /* remove prefix */
      maskL_pushresult(&msg);  /* create error message */
      maskL_error(L, "module '%s' not found:%s", name, mask_tostring(L, -1));
    }
    mask_pushstring(L, name);
    mask_call(L, 1, 2);  /* call it */
    if (mask_isfunction(L, -2))  /* did it find a loader? */
      return;  /* module loader found */
    else if (mask_isstring(L, -2)) {  /* searcher returned error message? */
      mask_pop(L, 1);  /* remove extra return */
      maskL_addvalue(&msg);  /* concatenate error message */
    }
    else {  /* no error message */
      mask_pop(L, 2);  /* remove both returns */
      maskL_buffsub(&msg, 2);  /* remove prefix */
    }
  }
}


static int ll_require (mask_State *L) {
  const char *name = maskL_checkstring(L, 1);
  mask_settop(L, 1);  /* LOADED table will be at index 2 */
  mask_getfield(L, MASK_REGISTRYINDEX, MASK_LOADED_TABLE);
  mask_getfield(L, 2, name);  /* LOADED[name] */
  if (mask_toboolean(L, -1))  /* is it there? */
    return 1;  /* package is already loaded */
  /* else must load package */
  mask_pop(L, 1);  /* remove 'getfield' result */
  findloader(L, name);
  mask_rotate(L, -2, 1);  /* function <-> loader data */
  mask_pushvalue(L, 1);  /* name is 1st argument to module loader */
  mask_pushvalue(L, -3);  /* loader data is 2nd argument */
  /* stack: ...; loader data; loader function; mod. name; loader data */
  mask_call(L, 2, 1);  /* run loader to load module */
  /* stack: ...; loader data; result from loader */
  if (!mask_isnil(L, -1))  /* non-nil return? */
    mask_setfield(L, 2, name);  /* LOADED[name] = returned value */
  else
    mask_pop(L, 1);  /* pop nil */
  if (mask_getfield(L, 2, name) == MASK_TNIL) {   /* module set no value? */
    mask_pushboolean(L, 1);  /* use true as result */
    mask_copy(L, -1, -2);  /* replace loader result */
    mask_setfield(L, 2, name);  /* LOADED[name] = true */
  }
  mask_rotate(L, -2, 1);  /* loader data <-> module result  */
  return 2;  /* return module result and loader data */
}

/* }====================================================== */




static const maskL_Reg pk_funcs[] = {
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


static const maskL_Reg ll_funcs[] = {
  {"require", ll_require},
  {NULL, NULL}
};


static void createsearcherstable (mask_State *L) {
  static const mask_CFunction searchers[] =
    {searcher_preload, searcher_Mask, searcher_C, searcher_Croot, NULL};
  int i;
  /* create 'searchers' table */
  mask_createtable(L, sizeof(searchers)/sizeof(searchers[0]) - 1, 0);
  /* fill it with predefined searchers */
  for (i=0; searchers[i] != NULL; i++) {
    mask_pushvalue(L, -2);  /* set 'package' as upvalue for all searchers */
    mask_pushcclosure(L, searchers[i], 1);
    mask_rawseti(L, -2, i+1);
  }
  mask_setfield(L, -2, "searchers");  /* put it in field 'searchers' */
}


/*
** create table CLIBS to keep track of loaded C libraries,
** setting a finalizer to close all libraries when closing state.
*/
static void createclibstable (mask_State *L) {
  maskL_getsubtable(L, MASK_REGISTRYINDEX, CLIBS);  /* create CLIBS table */
  mask_createtable(L, 0, 1);  /* create metatable for CLIBS */
  mask_pushcfunction(L, gctm);
  mask_setfield(L, -2, "__gc");  /* set finalizer for CLIBS table */
  mask_setmetatable(L, -2);
}


MASKMOD_API int maskopen_package (mask_State *L) {
  createclibstable(L);
  maskL_newlib(L, pk_funcs);  /* create 'package' table */
  createsearcherstable(L);
  /* set paths */
  setpath(L, "path", MASK_PATH_VAR, MASK_PATH_DEFAULT);
  setpath(L, "cpath", MASK_CPATH_VAR, MASK_CPATH_DEFAULT);
  /* store config information */
  mask_pushliteral(L, MASK_DIRSEP "\n" MASK_PATH_SEP "\n" MASK_PATH_MARK "\n"
                     MASK_EXEC_DIR "\n" MASK_IGMARK "\n");
  mask_setfield(L, -2, "config");
  /* set field 'loaded' */
  maskL_getsubtable(L, MASK_REGISTRYINDEX, MASK_LOADED_TABLE);
  mask_setfield(L, -2, "loaded");
  /* set field 'preload' */
  maskL_getsubtable(L, MASK_REGISTRYINDEX, MASK_PRELOAD_TABLE);
  mask_setfield(L, -2, "preload");
  mask_pushglobaltable(L);
  mask_pushvalue(L, -2);  /* set 'package' as upvalue for next lib */
  maskL_setfuncs(L, ll_funcs, 1);  /* open lib into global table */
  mask_pop(L, 1);  /* pop global table */
  return 1;  /* return 'package' table */
}

