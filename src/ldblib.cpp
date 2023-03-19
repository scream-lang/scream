/*
** $Id: ldblib.c $
** Interface from Hello to its debug API
** See Copyright Notice in hello.h
*/

#define ldblib_c
#define HELLO_LIB

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hello.h"

#include "lauxlib.h"
#include "hellolib.h"


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
static void checkstack (hello_State *L, hello_State *L1, int n) {
  if (l_unlikely(L != L1 && !hello_checkstack(L1, n)))
    helloL_error(L, "stack overflow");
}


static int db_getregistry (hello_State *L) {
  hello_pushvalue(L, HELLO_REGISTRYINDEX);
  return 1;
}


static int db_getmetatable (hello_State *L) {
  helloL_checkany(L, 1);
  if (!hello_getmetatable(L, 1)) {
    hello_pushnil(L);  /* no metatable */
  }
  return 1;
}


static int db_setmetatable (hello_State *L) {
  int t = hello_type(L, 2);
  helloL_argexpected(L, t == HELLO_TNIL || t == HELLO_TTABLE, 2, "nil or table");
  hello_settop(L, 2);
  hello_setmetatable(L, 1);
  return 1;  /* return 1st argument */
}


static int db_getuservalue (hello_State *L) {
  int n = (int)helloL_optinteger(L, 2, 1);
  if (hello_type(L, 1) != HELLO_TUSERDATA)
    helloL_pushfail(L);
  else if (hello_getiuservalue(L, 1, n) != HELLO_TNONE) {
    hello_pushboolean(L, 1);
    return 2;
  }
  return 1;
}


static int db_setuservalue (hello_State *L) {
  int n = (int)helloL_optinteger(L, 3, 1);
  helloL_checktype(L, 1, HELLO_TUSERDATA);
  helloL_checkany(L, 2);
  hello_settop(L, 2);
  if (!hello_setiuservalue(L, 1, n))
    helloL_pushfail(L);
  return 1;
}


/*
** Auxiliary function used by several library functions: check for
** an optional thread as function's first argument and set 'arg' with
** 1 if this argument is present (so that functions can skip it to
** access their other arguments)
*/
static hello_State *getthread (hello_State *L, int *arg) {
  if (hello_isthread(L, 1)) {
    *arg = 1;
    return hello_tothread(L, 1);
  }
  else {
    *arg = 0;
    return L;  /* function will operate over current thread */
  }
}


/*
** Variations of 'hello_settable', used by 'db_getinfo' to put results
** from 'hello_getinfo' into result table. Key is always a string;
** value can be a string, an int, or a boolean.
*/
static void settabss (hello_State *L, const char *k, const char *v) {
  hello_pushstring(L, v);
  hello_setfield(L, -2, k);
}

static void settabsi (hello_State *L, const char *k, int v) {
  hello_pushinteger(L, v);
  hello_setfield(L, -2, k);
}

static void settabsb (hello_State *L, const char *k, int v) {
  hello_pushboolean(L, v);
  hello_setfield(L, -2, k);
}


/*
** In function 'db_getinfo', the call to 'hello_getinfo' may push
** results on the stack; later it creates the result table to put
** these objects. Function 'treatstackoption' puts the result from
** 'hello_getinfo' on top of the result table so that it can call
** 'hello_setfield'.
*/
static void treatstackoption (hello_State *L, hello_State *L1, const char *fname) {
  if (L == L1)
    hello_rotate(L, -2, 1);  /* exchange object and table */
  else
    hello_xmove(L1, L, 1);  /* move object to the "main" stack */
  hello_setfield(L, -2, fname);  /* put object into table */
}


/*
** Calls 'hello_getinfo' and collects all results in a new table.
** L1 needs stack space for an optional input (function) plus
** two optional outputs (function and line table) from function
** 'hello_getinfo'.
*/
static int db_getinfo (hello_State *L) {
  hello_Debug ar;
  int arg;
  hello_State *L1 = getthread(L, &arg);
  const char *options = helloL_optstring(L, arg+2, "flnSrtu");
  checkstack(L, L1, 3);
  helloL_argcheck(L, options[0] != '>', arg + 2, "invalid option '>'");
  if (hello_isfunction(L, arg + 1)) {  /* info about a function? */
    options = hello_pushfstring(L, ">%s", options);  /* add '>' to 'options' */
    hello_pushvalue(L, arg + 1);  /* move function to 'L1' stack */
    hello_xmove(L, L1, 1);
  }
  else {  /* stack level */
    if (!hello_getstack(L1, (int)helloL_checkinteger(L, arg + 1), &ar)) {
      helloL_pushfail(L);  /* level out of range */
      return 1;
    }
  }
  if (!hello_getinfo(L1, options, &ar))
    helloL_argerror(L, arg+2, "invalid option");
  hello_newtable(L);  /* table to collect results */
  if (strchr(options, 'S')) {
    hello_pushlstring(L, ar.source, ar.srclen);
    hello_setfield(L, -2, "source");
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


static int db_getlocal (hello_State *L) {
  int arg;
  hello_State *L1 = getthread(L, &arg);
  int nvar = (int)helloL_checkinteger(L, arg + 2);  /* local-variable index */
  if (hello_isfunction(L, arg + 1)) {  /* function argument? */
    hello_pushvalue(L, arg + 1);  /* push function */
    hello_pushstring(L, hello_getlocal(L, NULL, nvar));  /* push local name */
    return 1;  /* return only name (there is no value) */
  }
  else {  /* stack-level argument */
    hello_Debug ar;
    const char *name;
    int level = (int)helloL_checkinteger(L, arg + 1);
    if (l_unlikely(!hello_getstack(L1, level, &ar)))  /* out of range? */
      helloL_argerror(L, arg+1, "level out of range");
    checkstack(L, L1, 1);
    name = hello_getlocal(L1, &ar, nvar);
    if (name) {
      hello_xmove(L1, L, 1);  /* move local value */
      hello_pushstring(L, name);  /* push name */
      hello_rotate(L, -2, 1);  /* re-order */
      return 2;
    }
    else {
      helloL_pushfail(L);  /* no name (nor value) */
      return 1;
    }
  }
}


static int db_setlocal (hello_State *L) {
  int arg;
  const char *name;
  hello_State *L1 = getthread(L, &arg);
  hello_Debug ar;
  int level = (int)helloL_checkinteger(L, arg + 1);
  int nvar = (int)helloL_checkinteger(L, arg + 2);
  if (l_unlikely(!hello_getstack(L1, level, &ar)))  /* out of range? */
    helloL_argerror(L, arg+1, "level out of range");
  helloL_checkany(L, arg+3);
  hello_settop(L, arg+3);
  checkstack(L, L1, 1);
  hello_xmove(L, L1, 1);
  name = hello_setlocal(L1, &ar, nvar);
  if (name == NULL)
    hello_pop(L1, 1);  /* pop value (if not popped by 'hello_setlocal') */
  hello_pushstring(L, name);
  return 1;
}


/*
** get (if 'get' is true) or set an upvalue from a closure
*/
static int auxupvalue (hello_State *L, int get) {
  const char *name;
  int n = (int)helloL_checkinteger(L, 2);  /* upvalue index */
  helloL_checktype(L, 1, HELLO_TFUNCTION);  /* closure */
  name = get ? hello_getupvalue(L, 1, n) : hello_setupvalue(L, 1, n);
  if (name == NULL) return 0;
  hello_pushstring(L, name);
  hello_insert(L, -(get+1));  /* no-op if get is false */
  return get + 1;
}


static int db_getupvalue (hello_State *L) {
  return auxupvalue(L, 1);
}


static int db_setupvalue (hello_State *L) {
  helloL_checkany(L, 3);
  return auxupvalue(L, 0);
}


/*
** Check whether a given upvalue from a given closure exists and
** returns its index
*/
static void *checkupval (hello_State *L, int argf, int argnup, int *pnup) {
  void *id;
  int nup = (int)helloL_checkinteger(L, argnup);  /* upvalue index */
  helloL_checktype(L, argf, HELLO_TFUNCTION);  /* closure */
  id = hello_upvalueid(L, argf, nup);
  if (pnup) {
    helloL_argcheck(L, id != NULL, argnup, "invalid upvalue index");
    *pnup = nup;
  }
  return id;
}


static int db_upvalueid (hello_State *L) {
  void *id = checkupval(L, 1, 2, NULL);
  if (id != NULL)
    hello_pushlightuserdata(L, id);
  else
    helloL_pushfail(L);
  return 1;
}


static int db_upvaluejoin (hello_State *L) {
  int n1, n2;
  checkupval(L, 1, 2, &n1);
  checkupval(L, 3, 4, &n2);
  helloL_argcheck(L, !hello_iscfunction(L, 1), 1, "Hello function expected");
  helloL_argcheck(L, !hello_iscfunction(L, 3), 3, "Hello function expected");
  hello_upvaluejoin(L, 1, n1, 3, n2);
  return 0;
}


/*
** Call hook function registered at hook table for the current
** thread (if there is one)
*/
static void hookf (hello_State *L, hello_Debug *ar) {
  static const char *const hooknames[] =
    {"call", "return", "line", "count", "tail call"};
  hello_getfield(L, HELLO_REGISTRYINDEX, HOOKKEY);
  hello_pushthread(L);
  if (hello_rawget(L, -2) == HELLO_TFUNCTION) {  /* is there a hook function? */
    hello_pushstring(L, hooknames[(int)ar->event]);  /* push event name */
    if (ar->currentline >= 0)
      hello_pushinteger(L, ar->currentline);  /* push current line */
    else hello_pushnil(L);
    hello_assert(hello_getinfo(L, "lS", ar));
    hello_call(L, 2, 0);  /* call hook function */
  }
}


/*
** Convert a string mask (for 'sethook') into a bit mask
*/
static int makemask (const char *smask, int count) {
  int mask = 0;
  if (strchr(smask, 'c')) mask |= HELLO_MASKCALL;
  if (strchr(smask, 'r')) mask |= HELLO_MASKRET;
  if (strchr(smask, 'l')) mask |= HELLO_MASKLINE;
  if (count > 0) mask |= HELLO_MASKCOUNT;
  return mask;
}


/*
** Convert a bit mask (for 'gethook') into a string mask
*/
static char *unmakemask (int mask, char *smask) {
  int i = 0;
  if (mask & HELLO_MASKCALL) smask[i++] = 'c';
  if (mask & HELLO_MASKRET) smask[i++] = 'r';
  if (mask & HELLO_MASKLINE) smask[i++] = 'l';
  smask[i] = '\0';
  return smask;
}


static int db_sethook (hello_State *L) {
  int arg, mask, count;
  hello_Hook func;
  hello_State *L1 = getthread(L, &arg);
  if (hello_isnoneornil(L, arg+1)) {  /* no hook? */
    hello_settop(L, arg+1);
    func = NULL; mask = 0; count = 0;  /* turn off hooks */
  }
  else {
    const char *smask = helloL_checkstring(L, arg+2);
    helloL_checktype(L, arg+1, HELLO_TFUNCTION);
    count = (int)helloL_optinteger(L, arg + 3, 0);
    func = hookf; mask = makemask(smask, count);
  }
  if (!helloL_getsubtable(L, HELLO_REGISTRYINDEX, HOOKKEY)) {
    /* table just created; initialize it */
    hello_pushliteral(L, "k");
    hello_setfield(L, -2, "__mode");  /** hooktable.__mode = "k" */
    hello_pushvalue(L, -1);
    hello_setmetatable(L, -2);  /* metatable(hooktable) = hooktable */
  }
  checkstack(L, L1, 1);
  hello_pushthread(L1); hello_xmove(L1, L, 1);  /* key (thread) */
  hello_pushvalue(L, arg + 1);  /* value (hook function) */
  hello_rawset(L, -3);  /* hooktable[L1] = new Hello hook */
  hello_sethook(L1, func, mask, count);
  return 0;
}


static int db_gethook (hello_State *L) {
  int arg;
  hello_State *L1 = getthread(L, &arg);
  char buff[5];
  int mask = hello_gethookmask(L1);
  hello_Hook hook = hello_gethook(L1);
  if (hook == NULL) {  /* no hook? */
    helloL_pushfail(L);
    return 1;
  }
  else if (hook != hookf)  /* external hook? */
    hello_pushliteral(L, "external hook");
  else {  /* hook table must exist */
    hello_getfield(L, HELLO_REGISTRYINDEX, HOOKKEY);
    checkstack(L, L1, 1);
    hello_pushthread(L1); hello_xmove(L1, L, 1);
    hello_rawget(L, -2);   /* 1st result = hooktable[L1] */
    hello_remove(L, -2);  /* remove hook table */
  }
  hello_pushstring(L, unmakemask(mask, buff));  /* 2nd result = mask */
  hello_pushinteger(L, hello_gethookcount(L1));  /* 3rd result = count */
  return 3;
}


static int db_debug (hello_State *L) {
  for (;;) {
    char buffer[250];
    hello_writestringerror("%s", "hello_debug> ");
    if (fgets(buffer, sizeof(buffer), stdin) == NULL ||
        strcmp(buffer, "cont\n") == 0)
      return 0;
    if (helloL_loadbuffer(L, buffer, strlen(buffer), "=(debug command)") ||
        hello_pcall(L, 0, 0, 0))
      hello_writestringerror("%s\n", helloL_tolstring(L, -1, NULL));
    hello_settop(L, 0);  /* remove eventual returns */
  }
}


static int db_traceback (hello_State *L) {
  int arg;
  hello_State *L1 = getthread(L, &arg);
  const char *msg = hello_tostring(L, arg + 1);
  if (msg == NULL && !hello_isnoneornil(L, arg + 1))  /* non-string 'msg'? */
    hello_pushvalue(L, arg + 1);  /* return it untouched */
  else {
    int level = (int)helloL_optinteger(L, arg + 2, (L == L1) ? 1 : 0);
    helloL_traceback(L, L1, msg, level);
  }
  return 1;
}


static int db_setcstacklimit (hello_State *L) {
  int limit = (int)helloL_checkinteger(L, 1);
  int res = hello_setcstacklimit(L, limit);
  hello_pushinteger(L, res);
  return 1;
}


static const helloL_Reg dblib[] = {
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


HELLOMOD_API int helloopen_debug (hello_State *L) {
  helloL_newlib(L, dblib);
  return 1;
}

