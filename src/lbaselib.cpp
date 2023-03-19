/*
** $Id: lbaselib.c $
** Basic library
** See Copyright Notice in hello.h
*/

#define lbaselib_c
#define HELLO_LIB

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <thread>
#include <chrono>

#include "hello.h"
#include "lprefix.h"
#include "lauxlib.h"
#include "hellolib.h"


static int helloB_print (hello_State *L) {
#ifdef HELLO_VMDUMP
  if (HELLO_VMDUMP_COND(L)) {
    hello_writestring("<OUTPUT> ", 9);
  }
#endif
  int n = hello_gettop(L);  /* number of arguments */
  int i;
  for (i = 1; i <= n; i++) {  /* for each argument */
    size_t l;
    const char *s = helloL_tolstring(L, i, &l);  /* convert it to string */
    if (i > 1)  /* not the first element? */
      hello_writestring("\t", 1);  /* add a tab before it */
    hello_writestring(s, l);  /* print it */
    hello_pop(L, 1);  /* pop result */
  }
  hello_writeline();
  return 0;
}


/*
** Creates a warning with all given arguments.
** Check first for errors; otherwise an error may interrupt
** the composition of a warning, leaving it unfinished.
*/
static int helloB_warn (hello_State *L) {
  int n = hello_gettop(L);  /* number of arguments */
  int i;
  helloL_checkstring(L, 1);  /* at least one argument */
  for (i = 2; i <= n; i++)
    helloL_checkstring(L, i);  /* make sure all arguments are strings */
  for (i = 1; i < n; i++)  /* compose warning */
    hello_warning(L, hello_tostring(L, i), 1);
  hello_warning(L, hello_tostring(L, n), 0);  /* close warning */
  return 0;
}


#define SPACECHARS	" \f\n\r\t\v"

static const char *b_str2int (const char *s, int base, hello_Integer *pn) {
  hello_Unsigned n = 0;
  int neg = 0;
  s += strspn(s, SPACECHARS);  /* skip initial spaces */
  if (*s == '-') { s++; neg = 1; }  /* handle sign */
  else if (*s == '+') s++;
  if (!isalnum((unsigned char)*s))  /* no digit? */
    return NULL;
  do {
    int digit = (isdigit((unsigned char)*s)) ? *s - '0'
                   : (toupper((unsigned char)*s) - 'A') + 10;
    if (digit >= base) return NULL;  /* invalid numeral */
    n = n * base + digit;
    s++;
  } while (isalnum((unsigned char)*s));
  s += strspn(s, SPACECHARS);  /* skip trailing spaces */
  *pn = (hello_Integer)((neg) ? (0u - n) : n);
  return s;
}


static int helloB_tonumber (hello_State *L) {
  if (hello_isnoneornil(L, 2)) {  /* standard conversion? */
    if (hello_type(L, 1) == HELLO_TNUMBER) {  /* already a number? */
      hello_settop(L, 1);  /* yes; return it */
      return 1;
    }
    else {
      size_t l;
      const char *s = hello_tolstring(L, 1, &l);
      if (s != NULL && hello_stringtonumber(L, s) == l + 1)
        return 1;  /* successful conversion to number */
      /* else not a number */
      helloL_checkany(L, 1);  /* (but there must be some parameter) */
    }
  }
  else {
    size_t l;
    const char *s;
    hello_Integer n = 0;  /* to avoid warnings */
    hello_Integer base = helloL_checkinteger(L, 2);
    helloL_checktype(L, 1, HELLO_TSTRING);  /* no numbers as strings */
    s = hello_tolstring(L, 1, &l);
    helloL_argcheck(L, 2 <= base && base <= 36, 2, "base out of range");
    if (b_str2int(s, (int)base, &n) == s + l) {
      hello_pushinteger(L, n);
      return 1;
    }  /* else not a number */
  }  /* else not a number */
  helloL_pushfail(L);  /* not a number */
  return 1;
}


[[noreturn]] static int helloB_error (hello_State *L) {
  int level = (int)helloL_optinteger(L, 2, 1);
  hello_settop(L, 1);
  if (hello_type(L, 1) == HELLO_TSTRING && level > 0) {
    helloL_where(L, level);   /* add extra information */
    hello_pushvalue(L, 1);
    hello_concat(L, 2);
  }
  hello_error(L);
}


static int helloB_getmetatable (hello_State *L) {
  helloL_checkany(L, 1);
  if (!hello_getmetatable(L, 1)) {
    hello_pushnil(L);
    return 1;  /* no metatable */
  }
  helloL_getmetafield(L, 1, "__metatable");
  return 1;  /* returns either __metatable field (if present) or metatable */
}


static int helloB_setmetatable (hello_State *L) {
  int t = hello_type(L, 2);
  helloL_checktype(L, 1, HELLO_TTABLE);
  helloL_argexpected(L, t == HELLO_TNIL || t == HELLO_TTABLE, 2, "nil or table");
  if (l_unlikely(helloL_getmetafield(L, 1, "__metatable") != HELLO_TNIL))
    helloL_error(L, "cannot change a protected metatable");
  hello_settop(L, 2);
  hello_setmetatable(L, 1);
  return 1;
}


static int helloB_rawequal (hello_State *L) {
  helloL_checkany(L, 1);
  helloL_checkany(L, 2);
  hello_pushboolean(L, hello_rawequal(L, 1, 2));
  return 1;
}


static int helloB_rawlen (hello_State *L) {
  int t = hello_type(L, 1);
  helloL_argexpected(L, t == HELLO_TTABLE || t == HELLO_TSTRING, 1,
                      "table or string");
  hello_pushinteger(L, hello_rawlen(L, 1));
  return 1;
}


static int helloB_rawget (hello_State *L) {
  helloL_checktype(L, 1, HELLO_TTABLE);
  helloL_checkany(L, 2);
  hello_settop(L, 2);
  hello_rawget(L, 1);
  return 1;
}

static int helloB_rawset (hello_State *L) {
  helloL_checktype(L, 1, HELLO_TTABLE);
  hello_erriffrozen(L, 1);
  helloL_checkany(L, 2);
  helloL_checkany(L, 3);
  hello_settop(L, 3);
  hello_rawset(L, 1);
  return 1;
}


static int pushmode (hello_State *L, int oldmode) {
  if (oldmode == -1)
    helloL_pushfail(L);  /* invalid call to 'hello_gc' */
  else
    hello_pushstring(L, (oldmode == HELLO_GCINC) ? "incremental"
                                             : "generational");
  return 1;
}


/*
** check whether call to 'hello_gc' was valid (not inside a finalizer)
*/
#define checkvalres(res) { if (res == -1) break; }

static int helloB_collectgarbage (hello_State *L) {
  static const char *const opts[] = {"stop", "restart", "collect",
    "count", "step", "setpause", "setstepmul",
    "isrunning", "generational", "incremental", NULL};
  static const int optsnum[] = {HELLO_GCSTOP, HELLO_GCRESTART, HELLO_GCCOLLECT,
    HELLO_GCCOUNT, HELLO_GCSTEP, HELLO_GCSETPAUSE, HELLO_GCSETSTEPMUL,
    HELLO_GCISRUNNING, HELLO_GCGEN, HELLO_GCINC};
  int o = optsnum[helloL_checkoption(L, 1, "collect", opts)];
  switch (o) {
    case HELLO_GCCOUNT: {
      int k = hello_gc(L, o);
      int b = hello_gc(L, HELLO_GCCOUNTB);
      checkvalres(k);
      hello_pushnumber(L, (hello_Number)k + ((hello_Number)b/1024));
      return 1;
    }
    case HELLO_GCSTEP: {
      int step = (int)helloL_optinteger(L, 2, 0);
      int res = hello_gc(L, o, step);
      checkvalres(res);
      hello_pushboolean(L, res);
      return 1;
    }
    case HELLO_GCSETPAUSE:
    case HELLO_GCSETSTEPMUL: {
      int p = (int)helloL_optinteger(L, 2, 0);
      int previous = hello_gc(L, o, p);
      checkvalres(previous);
      hello_pushinteger(L, previous);
      return 1;
    }
    case HELLO_GCISRUNNING: {
      int res = hello_gc(L, o);
      checkvalres(res);
      hello_pushboolean(L, res);
      return 1;
    }
    case HELLO_GCGEN: {
      int minormul = (int)helloL_optinteger(L, 2, 0);
      int majormul = (int)helloL_optinteger(L, 3, 0);
      return pushmode(L, hello_gc(L, o, minormul, majormul));
    }
    case HELLO_GCINC: {
      int pause = (int)helloL_optinteger(L, 2, 0);
      int stepmul = (int)helloL_optinteger(L, 3, 0);
      int stepsize = (int)helloL_optinteger(L, 4, 0);
      return pushmode(L, hello_gc(L, o, pause, stepmul, stepsize));
    }
    default: {
      int res = hello_gc(L, o);
      checkvalres(res);
      hello_pushinteger(L, res);
      return 1;
    }
  }
  helloL_pushfail(L);  /* invalid call (inside a finalizer) */
  return 1;
}


static int helloB_type (hello_State *L) {
  int t = hello_type(L, 1);
  helloL_argcheck(L, t != HELLO_TNONE, 1, "value expected");
  hello_pushstring(L, hello_typename(L, t));
  return 1;
}

HELLOI_FUNC int helloB_next(hello_State *L);
int helloB_next (hello_State *L) {
  helloL_checktype(L, 1, HELLO_TTABLE);
  hello_settop(L, 2);  /* create a 2nd argument if there isn't one */
  if (hello_next(L, 1))
    return 2;
  else {
    hello_pushnil(L);
    return 1;
  }
}


static int pairscont (hello_State *L, int status, hello_KContext k) {
  (void)L; (void)status; (void)k;  /* unused */
  return 3;
}

static int helloB_pairs (hello_State *L) {
  helloL_checkany(L, 1);
  if (helloL_getmetafield(L, 1, "__pairs") == HELLO_TNIL) {  /* no metamethod? */
    hello_pushcfunction(L, helloB_next);  /* will return generator, */
    hello_pushvalue(L, 1);  /* state, */
    hello_pushnil(L);  /* and initial value */
  }
  else {
    hello_pushvalue(L, 1);  /* argument 'self' to metamethod */
    hello_callk(L, 1, 3, 0, pairscont);  /* get 3 values from metamethod */
  }
  return 3;
}


/*
** Traversal function for 'ipairs'
*/
#define ipairsaux helloB_ipairsaux
HELLOI_FUNC int ipairsaux (hello_State *L);
int ipairsaux (hello_State *L) {
  hello_Integer i = helloL_checkinteger(L, 2);
  i = helloL_intop(+, i, 1);
  hello_pushinteger(L, i);
  return (hello_geti(L, 1, i) == HELLO_TNIL) ? 1 : 2;
}


/*
** 'ipairs' function. Returns 'ipairsaux', given "table", 0.
** (The given "table" may not be a table.)
*/
static int helloB_ipairs (hello_State *L) {
  helloL_checkany(L, 1);
  hello_pushcfunction(L, ipairsaux);  /* iteration function */
  hello_pushvalue(L, 1);  /* state */
  hello_pushinteger(L, 0);  /* initial value */
  return 3;
}


static int load_aux (hello_State *L, int status, int envidx) {
  if (l_likely(status == HELLO_OK)) {
    if (envidx != 0) {  /* 'env' parameter? */
      hello_pushvalue(L, envidx);  /* environment for loaded function */
      if (!hello_setupvalue(L, -2, 1))  /* set it as 1st upvalue */
        hello_pop(L, 1);  /* remove 'env' if not used by previous call */
    }
    return 1;
  }
  else {  /* error (message is on top of the stack) */
    helloL_pushfail(L);
    hello_insert(L, -2);  /* put before error message */
    return 2;  /* return fail plus error message */
  }
}


static int helloB_loadfile (hello_State *L) {
  const char *fname = helloL_optstring(L, 1, NULL);
  const char *mode = helloL_optstring(L, 2, NULL);
  int env = (!hello_isnone(L, 3) ? 3 : 0);  /* 'env' index or 0 if no 'env' */
  int status = helloL_loadfilex(L, fname, mode);
  return load_aux(L, status, env);
}


/*
** {======================================================
** Generic Read function
** =======================================================
*/


/*
** reserved slot, above all arguments, to hold a copy of the returned
** string to avoid it being collected while parsed. 'load' has four
** optional arguments (chunk, source name, mode, and environment).
*/
#define RESERVEDSLOT	5


/*
** Reader for generic 'load' function: 'hello_load' uses the
** stack for internal stuff, so the reader cannot change the
** stack top. Instead, it keeps its resulting string in a
** reserved slot inside the stack.
*/
static const char *generic_reader (hello_State *L, void *ud, size_t *size) {
  (void)(ud);  /* not used */
  helloL_checkstack(L, 2, "too many nested functions");
  hello_pushvalue(L, 1);  /* get function */
  hello_call(L, 0, 1);  /* call it */
  if (hello_isnil(L, -1)) {
    hello_pop(L, 1);  /* pop result */
    *size = 0;
    return NULL;
  }
  else if (l_unlikely(!hello_isstring(L, -1)))
    helloL_error(L, "reader function must return a string");
  hello_replace(L, RESERVEDSLOT);  /* save string in reserved slot */
  return hello_tolstring(L, RESERVEDSLOT, size);
}


static int helloB_load (hello_State *L) {
  int status;
  size_t l;
  const char *s = hello_tolstring(L, 1, &l);
  const char *mode = helloL_optstring(L, 3, "bt");
  int env = (!hello_isnone(L, 4) ? 4 : 0);  /* 'env' index or 0 if no 'env' */
  if (s != NULL) {  /* loading a string? */
    const char *chunkname = helloL_optstring(L, 2, s);
    status = helloL_loadbufferx(L, s, l, chunkname, mode);
  }
  else {  /* loading from a reader function */
    const char *chunkname = helloL_optstring(L, 2, "=(load)");
    helloL_checktype(L, 1, HELLO_TFUNCTION);
    hello_settop(L, RESERVEDSLOT);  /* create reserved slot */
    status = hello_load(L, generic_reader, NULL, chunkname, mode);
  }
  return load_aux(L, status, env);
}

/* }====================================================== */


static int dofilecont (hello_State *L, int d1, hello_KContext d2) {
  (void)d1;  (void)d2;  /* only to match 'hello_Kfunction' prototype */
  return hello_gettop(L) - 1;
}


static int helloB_dofile (hello_State *L) {
  const char *fname = helloL_optstring(L, 1, NULL);
  hello_settop(L, 1);
  if (l_unlikely(helloL_loadfile(L, fname) != HELLO_OK))
    hello_error(L);
  hello_callk(L, 0, HELLO_MULTRET, 0, dofilecont);
  return dofilecont(L, 0, 0);
}


static int helloB_assert (hello_State *L) {
  if (l_likely(hello_toboolean(L, 1)))  /* condition is true? */
    return hello_gettop(L);  /* return all arguments */
  else {  /* error */
    helloL_checkany(L, 1);  /* there must be a condition */
    hello_remove(L, 1);  /* remove it */
    hello_pushliteral(L, "assertion failed!");  /* default message */
    hello_settop(L, 1);  /* leave only message (default if no other one) */
    helloB_error(L);  /* call 'error' */
  }
}


static int helloB_select (hello_State *L) {
  int n = hello_gettop(L);
  if (hello_type(L, 1) == HELLO_TSTRING && *hello_tostring(L, 1) == '#') {
    hello_pushinteger(L, n-1);
    return 1;
  }
  else {
    hello_Integer i = helloL_checkinteger(L, 1);
    if (i < 0) i = n + i;
    else if (i > n) i = n;
    helloL_argcheck(L, 1 <= i, 1, "index out of range");
    return n - (int)i;
  }
}


/*
** Continuation function for 'pcall' and 'xpcall'. Both functions
** already pushed a 'true' before doing the call, so in case of success
** 'finishpcall' only has to return everything in the stack minus
** 'extra' values (where 'extra' is exactly the number of items to be
** ignored).
*/
static int finishpcall (hello_State *L, int status, hello_KContext extra) {
  if (l_unlikely(status != HELLO_OK && status != HELLO_YIELD)) {  /* error? */
    hello_pushboolean(L, 0);  /* first result (false) */
    hello_pushvalue(L, -2);  /* error message */
    return 2;  /* return false, msg */
  }
  else
    return hello_gettop(L) - (int)extra;  /* return all results */
}


static int helloB_pcall (hello_State *L) {
  int status;
  helloL_checkany(L, 1);
  hello_pushboolean(L, 1);  /* first result if no errors */
  hello_insert(L, 1);  /* put it in place */
  status = hello_pcallk(L, hello_gettop(L) - 2, HELLO_MULTRET, 0, 0, finishpcall);
  return finishpcall(L, status, 0);
}


/*
** Do a protected call with error handling. After 'hello_rotate', the
** stack will have <f, err, true, f, [args...]>; so, the function passes
** 2 to 'finishpcall' to skip the 2 first values when returning results.
*/
static int helloB_xpcall (hello_State *L) {
  int status;
  int n = hello_gettop(L);
  helloL_checktype(L, 2, HELLO_TFUNCTION);  /* check error function */
  hello_pushboolean(L, 1);  /* first result */
  hello_pushvalue(L, 1);  /* function */
  hello_rotate(L, 3, 2);  /* move them below function's arguments */
  status = hello_pcallk(L, n - 2, HELLO_MULTRET, 2, 2, finishpcall);
  return finishpcall(L, status, 2);
}


static int helloB_tostring (hello_State *L) {
  helloL_checkany(L, 1);
  helloL_tolstring(L, 1, NULL);
  return 1;
}


static int helloB_newuserdata (hello_State *L) {
  hello_newuserdata(L, 0);
  hello_newtable(L);
  hello_setmetatable(L, -2);
  return 1;
}


static const helloL_Reg base_funcs[] = {
  {"newuserdata", helloB_newuserdata},
  {"assert", helloB_assert},
  {"collectgarbage", helloB_collectgarbage},
  {"dofile", helloB_dofile},
  {"error", helloB_error},
  {"getmetatable", helloB_getmetatable},
  {"ipairs", helloB_ipairs},
  {"loadfile", helloB_loadfile},
  {"load", helloB_load},
  {"next", helloB_next},
  {"pairs", helloB_pairs},
  {"pcall", helloB_pcall},
  {"print", helloB_print},
  {"warn", helloB_warn},
  {"rawequal", helloB_rawequal},
  {"rawlen", helloB_rawlen},
  {"rawget", helloB_rawget},
  {"rawset", helloB_rawset},
  {"select", helloB_select},
  {"setmetatable", helloB_setmetatable},
  {"tonumber", helloB_tonumber},
  {"tostring", helloB_tostring},
  {"type", helloB_type},
  {"xpcall", helloB_xpcall},
  /* placeholders */
  {HELLO_GNAME, NULL},
  {"_VERSION", NULL},
  {NULL, NULL}
};


HELLOMOD_API int helloopen_base (hello_State *L) {
  /* open lib into global table */
  hello_pushglobaltable(L);
  helloL_setfuncs(L, base_funcs, 0);
  /* set global _G */
  hello_pushvalue(L, -1);
  hello_setfield(L, -2, HELLO_GNAME);
  /* set global _VERSION */
  hello_pushliteral(L, HELLO_VERSION);
  hello_setfield(L, -2, "_VERSION");
  /* set global _PVERSION */
  hello_pushliteral(L, HELLO_VERSION);
  hello_setfield(L, -2, "_PVERSION");
  /* set global _PSOUP */
#ifdef HELLO_USE_SOUP
  hello_pushboolean(L, true);
#else
  hello_pushboolean(L, false);
#endif
  hello_setfield(L, -2, "_PSOUP");
  return 1;
}

