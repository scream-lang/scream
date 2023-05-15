/*
** $Id: ldblib.c $
** Interface from Mask to its debug API
** See Copyright Notice in mask.h
*/

#define ldblib_c
#define MASK_LIB

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mask.h"

#include "lauxlib.h"
#include "masklib.h"


/*
** The hook table at registry[HOOKKEY] maps threads to their current
** hook function.
*/
static const char *const HOOKKEY = "_HOOKKEY";


/*
** If L1 != L, L1 can be in any state, and therefore there are no
** guarantees about its stack space; any push in L1 must be
** checked.
*/
static void checkstack (mask_State *L, mask_State *L1, int n) {
  if (l_unlikely(L != L1 && !mask_checkstack(L1, n)))
    maskL_error(L, "stack overflow");
}


static int db_getregistry (mask_State *L) {
  mask_pushvalue(L, MASK_REGISTRYINDEX);
  return 1;
}


static int db_getmetatable (mask_State *L) {
  maskL_checkany(L, 1);
  if (!mask_getmetatable(L, 1)) {
    mask_pushnil(L);  /* no metatable */
  }
  return 1;
}


static int db_setmetatable (mask_State *L) {
  int t = mask_type(L, 2);
  maskL_argexpected(L, t == MASK_TNIL || t == MASK_TTABLE, 2, "nil or table");
  mask_settop(L, 2);
  mask_setmetatable(L, 1);
  return 1;  /* return 1st argument */
}


static int db_getuservalue (mask_State *L) {
  int n = (int)maskL_optinteger(L, 2, 1);
  if (mask_type(L, 1) != MASK_TUSERDATA)
    maskL_pushfail(L);
  else if (mask_getiuservalue(L, 1, n) != MASK_TNONE) {
    mask_pushboolean(L, 1);
    return 2;
  }
  return 1;
}


static int db_setuservalue (mask_State *L) {
  int n = (int)maskL_optinteger(L, 3, 1);
  maskL_checktype(L, 1, MASK_TUSERDATA);
  maskL_checkany(L, 2);
  mask_settop(L, 2);
  if (!mask_setiuservalue(L, 1, n))
    maskL_pushfail(L);
  return 1;
}


/*
** Auxiliary function used by several library functions: check for
** an optional thread as function's first argument and set 'arg' with
** 1 if this argument is present (so that functions can skip it to
** access their other arguments)
*/
static mask_State *getthread (mask_State *L, int *arg) {
  if (mask_isthread(L, 1)) {
    *arg = 1;
    return mask_tothread(L, 1);
  }
  else {
    *arg = 0;
    return L;  /* function will operate over current thread */
  }
}


/*
** Variations of 'mask_settable', used by 'db_getinfo' to put results
** from 'mask_getinfo' into result table. Key is always a string;
** value can be a string, an int, or a boolean.
*/
static void settabss (mask_State *L, const char *k, const char *v) {
  mask_pushstring(L, v);
  mask_setfield(L, -2, k);
}

static void settabsi (mask_State *L, const char *k, int v) {
  mask_pushinteger(L, v);
  mask_setfield(L, -2, k);
}

static void settabsb (mask_State *L, const char *k, int v) {
  mask_pushboolean(L, v);
  mask_setfield(L, -2, k);
}


/*
** In function 'db_getinfo', the call to 'mask_getinfo' may push
** results on the stack; later it creates the result table to put
** these objects. Function 'treatstackoption' puts the result from
** 'mask_getinfo' on top of the result table so that it can call
** 'mask_setfield'.
*/
static void treatstackoption (mask_State *L, mask_State *L1, const char *fname) {
  if (L == L1)
    mask_rotate(L, -2, 1);  /* exchange object and table */
  else
    mask_xmove(L1, L, 1);  /* move object to the "main" stack */
  mask_setfield(L, -2, fname);  /* put object into table */
}


/*
** Calls 'mask_getinfo' and collects all results in a new table.
** L1 needs stack space for an optional input (function) plus
** two optional outputs (function and line table) from function
** 'mask_getinfo'.
*/
static int db_getinfo (mask_State *L) {
  mask_Debug ar;
  int arg;
  mask_State *L1 = getthread(L, &arg);
  const char *options = maskL_optstring(L, arg+2, "flnSrtu");
  checkstack(L, L1, 3);
  maskL_argcheck(L, options[0] != '>', arg + 2, "invalid option '>'");
  if (mask_isfunction(L, arg + 1)) {  /* info about a function? */
    options = mask_pushfstring(L, ">%s", options);  /* add '>' to 'options' */
    mask_pushvalue(L, arg + 1);  /* move function to 'L1' stack */
    mask_xmove(L, L1, 1);
  }
  else {  /* stack level */
    if (!mask_getstack(L1, (int)maskL_checkinteger(L, arg + 1), &ar)) {
      maskL_pushfail(L);  /* level out of range */
      return 1;
    }
  }
  if (!mask_getinfo(L1, options, &ar))
    maskL_argerror(L, arg+2, "invalid option");
  mask_newtable(L);  /* table to collect results */
  if (strchr(options, 'S')) {
    mask_pushlstring(L, ar.source, ar.srclen);
    mask_setfield(L, -2, "source");
    settabss(L, "short_src", ar.short_src);
    settabsi(L, "linedefined", ar.linedefined);
    settabsi(L, "lastlinedefined", ar.lastlinedefined);
    settabss(L, "what", ar.what);
  }
  if (strchr(options, 'l'))
    settabsi(L, "currentline", ar.currentline);
  if (strchr(options, 'u')) {
    settabsi(L, "nups", ar.nups);
    settabsi(L, "nparams", ar.nparams);
    settabsb(L, "isvararg", ar.isvararg);
  }
  if (strchr(options, 'n')) {
    settabss(L, "name", ar.name);
    settabss(L, "namewhat", ar.namewhat);
  }
  if (strchr(options, 'r')) {
    settabsi(L, "ftransfer", ar.ftransfer);
    settabsi(L, "ntransfer", ar.ntransfer);
  }
  if (strchr(options, 't'))
    settabsb(L, "istailcall", ar.istailcall);
  if (strchr(options, 'L'))
    treatstackoption(L, L1, "activelines");
  if (strchr(options, 'f'))
    treatstackoption(L, L1, "func");
  return 1;  /* return table */
}


static int db_getlocal (mask_State *L) {
  int arg;
  mask_State *L1 = getthread(L, &arg);
  int nvar = (int)maskL_checkinteger(L, arg + 2);  /* local-variable index */
  if (mask_isfunction(L, arg + 1)) {  /* function argument? */
    mask_pushvalue(L, arg + 1);  /* push function */
    mask_pushstring(L, mask_getlocal(L, NULL, nvar));  /* push local name */
    return 1;  /* return only name (there is no value) */
  }
  else {  /* stack-level argument */
    mask_Debug ar;
    const char *name;
    int level = (int)maskL_checkinteger(L, arg + 1);
    if (l_unlikely(!mask_getstack(L1, level, &ar)))  /* out of range? */
      maskL_argerror(L, arg+1, "level out of range");
    checkstack(L, L1, 1);
    name = mask_getlocal(L1, &ar, nvar);
    if (name) {
      mask_xmove(L1, L, 1);  /* move local value */
      mask_pushstring(L, name);  /* push name */
      mask_rotate(L, -2, 1);  /* re-order */
      return 2;
    }
    else {
      maskL_pushfail(L);  /* no name (nor value) */
      return 1;
    }
  }
}


static int db_setlocal (mask_State *L) {
  int arg;
  const char *name;
  mask_State *L1 = getthread(L, &arg);
  mask_Debug ar;
  int level = (int)maskL_checkinteger(L, arg + 1);
  int nvar = (int)maskL_checkinteger(L, arg + 2);
  if (l_unlikely(!mask_getstack(L1, level, &ar)))  /* out of range? */
    maskL_argerror(L, arg+1, "level out of range");
  maskL_checkany(L, arg+3);
  mask_settop(L, arg+3);
  checkstack(L, L1, 1);
  mask_xmove(L, L1, 1);
  name = mask_setlocal(L1, &ar, nvar);
  if (name == NULL)
    mask_pop(L1, 1);  /* pop value (if not popped by 'mask_setlocal') */
  mask_pushstring(L, name);
  return 1;
}


/*
** get (if 'get' is true) or set an upvalue from a closure
*/
static int auxupvalue (mask_State *L, int get) {
  const char *name;
  int n = (int)maskL_checkinteger(L, 2);  /* upvalue index */
  maskL_checktype(L, 1, MASK_TFUNCTION);  /* closure */
  name = get ? mask_getupvalue(L, 1, n) : mask_setupvalue(L, 1, n);
  if (name == NULL) return 0;
  mask_pushstring(L, name);
  mask_insert(L, -(get+1));  /* no-op if get is false */
  return get + 1;
}


static int db_getupvalue (mask_State *L) {
  return auxupvalue(L, 1);
}


static int db_setupvalue (mask_State *L) {
  maskL_checkany(L, 3);
  return auxupvalue(L, 0);
}


/*
** Check whether a given upvalue from a given closure exists and
** returns its index
*/
static void *checkupval (mask_State *L, int argf, int argnup, int *pnup) {
  void *id;
  int nup = (int)maskL_checkinteger(L, argnup);  /* upvalue index */
  maskL_checktype(L, argf, MASK_TFUNCTION);  /* closure */
  id = mask_upvalueid(L, argf, nup);
  if (pnup) {
    maskL_argcheck(L, id != NULL, argnup, "invalid upvalue index");
    *pnup = nup;
  }
  return id;
}


static int db_upvalueid (mask_State *L) {
  void *id = checkupval(L, 1, 2, NULL);
  if (id != NULL)
    mask_pushlightuserdata(L, id);
  else
    maskL_pushfail(L);
  return 1;
}


static int db_upvaluejoin (mask_State *L) {
  int n1, n2;
  checkupval(L, 1, 2, &n1);
  checkupval(L, 3, 4, &n2);
  maskL_argcheck(L, !mask_iscfunction(L, 1), 1, "Mask function expected");
  maskL_argcheck(L, !mask_iscfunction(L, 3), 3, "Mask function expected");
  mask_upvaluejoin(L, 1, n1, 3, n2);
  return 0;
}


/*
** Call hook function registered at hook table for the current
** thread (if there is one)
*/
static void hookf (mask_State *L, mask_Debug *ar) {
  static const char *const hooknames[] =
    {"call", "return", "line", "count", "tail call"};
  mask_getfield(L, MASK_REGISTRYINDEX, HOOKKEY);
  mask_pushthread(L);
  if (mask_rawget(L, -2) == MASK_TFUNCTION) {  /* is there a hook function? */
    mask_pushstring(L, hooknames[(int)ar->event]);  /* push event name */
    if (ar->currentline >= 0)
      mask_pushinteger(L, ar->currentline);  /* push current line */
    else mask_pushnil(L);
    mask_assert(mask_getinfo(L, "lS", ar));
    mask_call(L, 2, 0);  /* call hook function */
  }
}


/*
** Convert a string mask (for 'sethook') into a bit mask
*/
static int makemask (const char *smask, int count) {
  int mask = 0;
  if (strchr(smask, 'c')) mask |= MASK_MASKCALL;
  if (strchr(smask, 'r')) mask |= MASK_MASKRET;
  if (strchr(smask, 'l')) mask |= MASK_MASKLINE;
  if (count > 0) mask |= MASK_MASKCOUNT;
  return mask;
}


/*
** Convert a bit mask (for 'gethook') into a string mask
*/
static char *unmakemask (int mask, char *smask) {
  int i = 0;
  if (mask & MASK_MASKCALL) smask[i++] = 'c';
  if (mask & MASK_MASKRET) smask[i++] = 'r';
  if (mask & MASK_MASKLINE) smask[i++] = 'l';
  smask[i] = '\0';
  return smask;
}


static int db_sethook (mask_State *L) {
  int arg, mask, count;
  mask_Hook func;
  mask_State *L1 = getthread(L, &arg);
  if (mask_isnoneornil(L, arg+1)) {  /* no hook? */
    mask_settop(L, arg+1);
    func = NULL; mask = 0; count = 0;  /* turn off hooks */
  }
  else {
    const char *smask = maskL_checkstring(L, arg+2);
    maskL_checktype(L, arg+1, MASK_TFUNCTION);
    count = (int)maskL_optinteger(L, arg + 3, 0);
    func = hookf; mask = makemask(smask, count);
  }
  if (!maskL_getsubtable(L, MASK_REGISTRYINDEX, HOOKKEY)) {
    /* table just created; initialize it */
    mask_pushliteral(L, "k");
    mask_setfield(L, -2, "__mode");  /** hooktable.__mode = "k" */
    mask_pushvalue(L, -1);
    mask_setmetatable(L, -2);  /* metatable(hooktable) = hooktable */
  }
  checkstack(L, L1, 1);
  mask_pushthread(L1); mask_xmove(L1, L, 1);  /* key (thread) */
  mask_pushvalue(L, arg + 1);  /* value (hook function) */
  mask_rawset(L, -3);  /* hooktable[L1] = new Mask hook */
  mask_sethook(L1, func, mask, count);
  return 0;
}


static int db_gethook (mask_State *L) {
  int arg;
  mask_State *L1 = getthread(L, &arg);
  char buff[5];
  int mask = mask_gethookmask(L1);
  mask_Hook hook = mask_gethook(L1);
  if (hook == NULL) {  /* no hook? */
    maskL_pushfail(L);
    return 1;
  }
  else if (hook != hookf)  /* external hook? */
    mask_pushliteral(L, "external hook");
  else {  /* hook table must exist */
    mask_getfield(L, MASK_REGISTRYINDEX, HOOKKEY);
    checkstack(L, L1, 1);
    mask_pushthread(L1); mask_xmove(L1, L, 1);
    mask_rawget(L, -2);   /* 1st result = hooktable[L1] */
    mask_remove(L, -2);  /* remove hook table */
  }
  mask_pushstring(L, unmakemask(mask, buff));  /* 2nd result = mask */
  mask_pushinteger(L, mask_gethookcount(L1));  /* 3rd result = count */
  return 3;
}


static int db_debug (mask_State *L) {
  for (;;) {
    char buffer[250];
    mask_writestringerror("%s", "mask_debug> ");
    if (fgets(buffer, sizeof(buffer), stdin) == NULL ||
        strcmp(buffer, "cont\n") == 0)
      return 0;
    if (maskL_loadbuffer(L, buffer, strlen(buffer), "=(debug command)") ||
        mask_pcall(L, 0, 0, 0))
      mask_writestringerror("%s\n", maskL_tolstring(L, -1, NULL));
    mask_settop(L, 0);  /* remove eventual returns */
  }
}


static int db_traceback (mask_State *L) {
  int arg;
  mask_State *L1 = getthread(L, &arg);
  const char *msg = mask_tostring(L, arg + 1);
  if (msg == NULL && !mask_isnoneornil(L, arg + 1))  /* non-string 'msg'? */
    mask_pushvalue(L, arg + 1);  /* return it untouched */
  else {
    int level = (int)maskL_optinteger(L, arg + 2, (L == L1) ? 1 : 0);
    maskL_traceback(L, L1, msg, level);
  }
  return 1;
}


static int db_setcstacklimit (mask_State *L) {
  int limit = (int)maskL_checkinteger(L, 1);
  int res = mask_setcstacklimit(L, limit);
  mask_pushinteger(L, res);
  return 1;
}


static const maskL_Reg dblib[] = {
  {"debug", db_debug},
  {"getuservalue", db_getuservalue},
  {"gethook", db_gethook},
  {"getinfo", db_getinfo},
  {"getlocal", db_getlocal},
  {"getregistry", db_getregistry},
  {"getmetatable", db_getmetatable},
  {"getupvalue", db_getupvalue},
  {"upvaluejoin", db_upvaluejoin},
  {"upvalueid", db_upvalueid},
  {"setuservalue", db_setuservalue},
  {"sethook", db_sethook},
  {"setlocal", db_setlocal},
  {"setmetatable", db_setmetatable},
  {"setupvalue", db_setupvalue},
  {"traceback", db_traceback},
  {"setcstacklimit", db_setcstacklimit},
  {NULL, NULL}
};


MASKMOD_API int maskopen_debug (mask_State *L) {
  maskL_newlib(L, dblib);
  return 1;
}

