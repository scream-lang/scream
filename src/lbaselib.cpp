/*
** $Id: lbaselib.c $
** Basic library
** See Copyright Notice in mask.h
*/

#define lbaselib_c
#define MASK_LIB

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <thread>
#include <chrono>

#include "mask.h"
#include "lprefix.h"
#include "lauxlib.h"
#include "masklib.h"


static int maskB_print (mask_State *L) {
#ifdef MASK_VMDUMP
  if (MASK_VMDUMP_COND(L)) {
    mask_writestring("<OUTPUT> ", 9);
  }
#endif
  int n = mask_gettop(L);  /* number of arguments */
  int i;
  for (i = 1; i <= n; i++) {  /* for each argument */
    size_t l;
    const char *s = maskL_tolstring(L, i, &l);  /* convert it to string */
    if (i > 1)  /* not the first element? */
      mask_writestring("\t", 1);  /* add a tab before it */
    mask_writestring(s, l);  /* print it */
    mask_pop(L, 1);  /* pop result */
  }
  mask_writeline();
  return 0;
}


/*
** Creates a warning with all given arguments.
** Check first for errors; otherwise an error may interrupt
** the composition of a warning, leaving it unfinished.
*/
static int maskB_warn (mask_State *L) {
  int n = mask_gettop(L);  /* number of arguments */
  int i;
  maskL_checkstring(L, 1);  /* at least one argument */
  for (i = 2; i <= n; i++)
    maskL_checkstring(L, i);  /* make sure all arguments are strings */
  for (i = 1; i < n; i++)  /* compose warning */
    mask_warning(L, mask_tostring(L, i), 1);
  mask_warning(L, mask_tostring(L, n), 0);  /* close warning */
  return 0;
}


#define SPACECHARS	" \f\n\r\t\v"

static const char *b_str2int (const char *s, int base, mask_Integer *pn) {
  mask_Unsigned n = 0;
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
  *pn = (mask_Integer)((neg) ? (0u - n) : n);
  return s;
}


static int maskB_tonumber (mask_State *L) {
  if (mask_isnoneornil(L, 2)) {  /* standard conversion? */
    if (mask_type(L, 1) == MASK_TNUMBER) {  /* already a number? */
      mask_settop(L, 1);  /* yes; return it */
      return 1;
    }
    else {
      size_t l;
      const char *s = mask_tolstring(L, 1, &l);
      if (s != NULL && mask_stringtonumber(L, s) == l + 1)
        return 1;  /* successful conversion to number */
      /* else not a number */
      maskL_checkany(L, 1);  /* (but there must be some parameter) */
    }
  }
  else {
    size_t l;
    const char *s;
    mask_Integer n = 0;  /* to avoid warnings */
    mask_Integer base = maskL_checkinteger(L, 2);
    maskL_checktype(L, 1, MASK_TSTRING);  /* no numbers as strings */
    s = mask_tolstring(L, 1, &l);
    maskL_argcheck(L, 2 <= base && base <= 36, 2, "base out of range");
    if (b_str2int(s, (int)base, &n) == s + l) {
      mask_pushinteger(L, n);
      return 1;
    }  /* else not a number */
  }  /* else not a number */
  maskL_pushfail(L);  /* not a number */
  return 1;
}


[[noreturn]] static int maskB_error (mask_State *L) {
  int level = (int)maskL_optinteger(L, 2, 1);
  mask_settop(L, 1);
  if (mask_type(L, 1) == MASK_TSTRING && level > 0) {
    maskL_where(L, level);   /* add extra information */
    mask_pushvalue(L, 1);
    mask_concat(L, 2);
  }
  mask_error(L);
}


static int maskB_getmetatable (mask_State *L) {
  maskL_checkany(L, 1);
  if (!mask_getmetatable(L, 1)) {
    mask_pushnil(L);
    return 1;  /* no metatable */
  }
  maskL_getmetafield(L, 1, "__metatable");
  return 1;  /* returns either __metatable field (if present) or metatable */
}


static int maskB_setmetatable (mask_State *L) {
  int t = mask_type(L, 2);
  maskL_checktype(L, 1, MASK_TTABLE);
  maskL_argexpected(L, t == MASK_TNIL || t == MASK_TTABLE, 2, "nil or table");
  if (l_unlikely(maskL_getmetafield(L, 1, "__metatable") != MASK_TNIL))
    maskL_error(L, "cannot change a protected metatable");
  mask_settop(L, 2);
  mask_setmetatable(L, 1);
  return 1;
}


static int maskB_rawequal (mask_State *L) {
  maskL_checkany(L, 1);
  maskL_checkany(L, 2);
  mask_pushboolean(L, mask_rawequal(L, 1, 2));
  return 1;
}


static int maskB_rawlen (mask_State *L) {
  int t = mask_type(L, 1);
  maskL_argexpected(L, t == MASK_TTABLE || t == MASK_TSTRING, 1,
                      "table or string");
  mask_pushinteger(L, mask_rawlen(L, 1));
  return 1;
}


static int maskB_rawget (mask_State *L) {
  maskL_checktype(L, 1, MASK_TTABLE);
  maskL_checkany(L, 2);
  mask_settop(L, 2);
  mask_rawget(L, 1);
  return 1;
}

static int maskB_rawset (mask_State *L) {
  maskL_checktype(L, 1, MASK_TTABLE);
  mask_erriffrozen(L, 1);
  maskL_checkany(L, 2);
  maskL_checkany(L, 3);
  mask_settop(L, 3);
  mask_rawset(L, 1);
  return 1;
}


static int pushmode (mask_State *L, int oldmode) {
  if (oldmode == -1)
    maskL_pushfail(L);  /* invalid call to 'mask_gc' */
  else
    mask_pushstring(L, (oldmode == MASK_GCINC) ? "incremental"
                                             : "generational");
  return 1;
}


/*
** check whether call to 'mask_gc' was valid (not inside a finalizer)
*/
#define checkvalres(res) { if (res == -1) break; }

static int maskB_collectgarbage (mask_State *L) {
  static const char *const opts[] = {"stop", "restart", "collect",
    "count", "step", "setpause", "setstepmul",
    "isrunning", "generational", "incremental", NULL};
  static const int optsnum[] = {MASK_GCSTOP, MASK_GCRESTART, MASK_GCCOLLECT,
    MASK_GCCOUNT, MASK_GCSTEP, MASK_GCSETPAUSE, MASK_GCSETSTEPMUL,
    MASK_GCISRUNNING, MASK_GCGEN, MASK_GCINC};
  int o = optsnum[maskL_checkoption(L, 1, "collect", opts)];
  switch (o) {
    case MASK_GCCOUNT: {
      int k = mask_gc(L, o);
      int b = mask_gc(L, MASK_GCCOUNTB);
      checkvalres(k);
      mask_pushnumber(L, (mask_Number)k + ((mask_Number)b/1024));
      return 1;
    }
    case MASK_GCSTEP: {
      int step = (int)maskL_optinteger(L, 2, 0);
      int res = mask_gc(L, o, step);
      checkvalres(res);
      mask_pushboolean(L, res);
      return 1;
    }
    case MASK_GCSETPAUSE:
    case MASK_GCSETSTEPMUL: {
      int p = (int)maskL_optinteger(L, 2, 0);
      int previous = mask_gc(L, o, p);
      checkvalres(previous);
      mask_pushinteger(L, previous);
      return 1;
    }
    case MASK_GCISRUNNING: {
      int res = mask_gc(L, o);
      checkvalres(res);
      mask_pushboolean(L, res);
      return 1;
    }
    case MASK_GCGEN: {
      int minormul = (int)maskL_optinteger(L, 2, 0);
      int majormul = (int)maskL_optinteger(L, 3, 0);
      return pushmode(L, mask_gc(L, o, minormul, majormul));
    }
    case MASK_GCINC: {
      int pause = (int)maskL_optinteger(L, 2, 0);
      int stepmul = (int)maskL_optinteger(L, 3, 0);
      int stepsize = (int)maskL_optinteger(L, 4, 0);
      return pushmode(L, mask_gc(L, o, pause, stepmul, stepsize));
    }
    default: {
      int res = mask_gc(L, o);
      checkvalres(res);
      mask_pushinteger(L, res);
      return 1;
    }
  }
  maskL_pushfail(L);  /* invalid call (inside a finalizer) */
  return 1;
}


static int maskB_type (mask_State *L) {
  int t = mask_type(L, 1);
  maskL_argcheck(L, t != MASK_TNONE, 1, "value expected");
  mask_pushstring(L, mask_typename(L, t));
  return 1;
}

MASKI_FUNC int maskB_next(mask_State *L);
int maskB_next (mask_State *L) {
  maskL_checktype(L, 1, MASK_TTABLE);
  mask_settop(L, 2);  /* create a 2nd argument if there isn't one */
  if (mask_next(L, 1))
    return 2;
  else {
    mask_pushnil(L);
    return 1;
  }
}


static int pairscont (mask_State *L, int status, mask_KContext k) {
  (void)L; (void)status; (void)k;  /* unused */
  return 3;
}

static int maskB_pairs (mask_State *L) {
  maskL_checkany(L, 1);
  if (maskL_getmetafield(L, 1, "__pairs") == MASK_TNIL) {  /* no metamethod? */
    mask_pushcfunction(L, maskB_next);  /* will return generator, */
    mask_pushvalue(L, 1);  /* state, */
    mask_pushnil(L);  /* and initial value */
  }
  else {
    mask_pushvalue(L, 1);  /* argument 'self' to metamethod */
    mask_callk(L, 1, 3, 0, pairscont);  /* get 3 values from metamethod */
  }
  return 3;
}


/*
** Traversal function for 'ipairs'
*/
#define ipairsaux maskB_ipairsaux
MASKI_FUNC int ipairsaux (mask_State *L);
int ipairsaux (mask_State *L) {
  mask_Integer i = maskL_checkinteger(L, 2);
  i = maskL_intop(+, i, 1);
  mask_pushinteger(L, i);
  return (mask_geti(L, 1, i) == MASK_TNIL) ? 1 : 2;
}


/*
** 'ipairs' function. Returns 'ipairsaux', given "table", 0.
** (The given "table" may not be a table.)
*/
static int maskB_ipairs (mask_State *L) {
  maskL_checkany(L, 1);
  mask_pushcfunction(L, ipairsaux);  /* iteration function */
  mask_pushvalue(L, 1);  /* state */
  mask_pushinteger(L, 0);  /* initial value */
  return 3;
}


static int load_aux (mask_State *L, int status, int envidx) {
  if (l_likely(status == MASK_OK)) {
    if (envidx != 0) {  /* 'env' parameter? */
      mask_pushvalue(L, envidx);  /* environment for loaded function */
      if (!mask_setupvalue(L, -2, 1))  /* set it as 1st upvalue */
        mask_pop(L, 1);  /* remove 'env' if not used by previous call */
    }
    return 1;
  }
  else {  /* error (message is on top of the stack) */
    maskL_pushfail(L);
    mask_insert(L, -2);  /* put before error message */
    return 2;  /* return fail plus error message */
  }
}


static int maskB_loadfile (mask_State *L) {
  const char *fname = maskL_optstring(L, 1, NULL);
  const char *mode = maskL_optstring(L, 2, NULL);
  int env = (!mask_isnone(L, 3) ? 3 : 0);  /* 'env' index or 0 if no 'env' */
  int status = maskL_loadfilex(L, fname, mode);
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
** Reader for generic 'load' function: 'mask_load' uses the
** stack for internal stuff, so the reader cannot change the
** stack top. Instead, it keeps its resulting string in a
** reserved slot inside the stack.
*/
static const char *generic_reader (mask_State *L, void *ud, size_t *size) {
  (void)(ud);  /* not used */
  maskL_checkstack(L, 2, "too many nested functions");
  mask_pushvalue(L, 1);  /* get function */
  mask_call(L, 0, 1);  /* call it */
  if (mask_isnil(L, -1)) {
    mask_pop(L, 1);  /* pop result */
    *size = 0;
    return NULL;
  }
  else if (l_unlikely(!mask_isstring(L, -1)))
    maskL_error(L, "reader function must return a string");
  mask_replace(L, RESERVEDSLOT);  /* save string in reserved slot */
  return mask_tolstring(L, RESERVEDSLOT, size);
}


static int maskB_load (mask_State *L) {
  int status;
  size_t l;
  const char *s = mask_tolstring(L, 1, &l);
  const char *mode = maskL_optstring(L, 3, "bt");
  int env = (!mask_isnone(L, 4) ? 4 : 0);  /* 'env' index or 0 if no 'env' */
  if (s != NULL) {  /* loading a string? */
    const char *chunkname = maskL_optstring(L, 2, s);
    status = maskL_loadbufferx(L, s, l, chunkname, mode);
  }
  else {  /* loading from a reader function */
    const char *chunkname = maskL_optstring(L, 2, "=(load)");
    maskL_checktype(L, 1, MASK_TFUNCTION);
    mask_settop(L, RESERVEDSLOT);  /* create reserved slot */
    status = mask_load(L, generic_reader, NULL, chunkname, mode);
  }
  return load_aux(L, status, env);
}

/* }====================================================== */


static int dofilecont (mask_State *L, int d1, mask_KContext d2) {
  (void)d1;  (void)d2;  /* only to match 'mask_Kfunction' prototype */
  return mask_gettop(L) - 1;
}


static int maskB_dofile (mask_State *L) {
  const char *fname = maskL_optstring(L, 1, NULL);
  mask_settop(L, 1);
  if (l_unlikely(maskL_loadfile(L, fname) != MASK_OK))
    mask_error(L);
  mask_callk(L, 0, MASK_MULTRET, 0, dofilecont);
  return dofilecont(L, 0, 0);
}


static int maskB_assert (mask_State *L) {
  if (l_likely(mask_toboolean(L, 1)))  /* condition is true? */
    return mask_gettop(L);  /* return all arguments */
  else {  /* error */
    maskL_checkany(L, 1);  /* there must be a condition */
    mask_remove(L, 1);  /* remove it */
    mask_pushliteral(L, "assertion failed!");  /* default message */
    mask_settop(L, 1);  /* leave only message (default if no other one) */
    maskB_error(L);  /* call 'error' */
  }
}


static int maskB_select (mask_State *L) {
  int n = mask_gettop(L);
  if (mask_type(L, 1) == MASK_TSTRING && *mask_tostring(L, 1) == '#') {
    mask_pushinteger(L, n-1);
    return 1;
  }
  else {
    mask_Integer i = maskL_checkinteger(L, 1);
    if (i < 0) i = n + i;
    else if (i > n) i = n;
    maskL_argcheck(L, 1 <= i, 1, "index out of range");
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
static int finishpcall (mask_State *L, int status, mask_KContext extra) {
  if (l_unlikely(status != MASK_OK && status != MASK_YIELD)) {  /* error? */
    mask_pushboolean(L, 0);  /* first result (false) */
    mask_pushvalue(L, -2);  /* error message */
    return 2;  /* return false, msg */
  }
  else
    return mask_gettop(L) - (int)extra;  /* return all results */
}


static int maskB_pcall (mask_State *L) {
  int status;
  maskL_checkany(L, 1);
  mask_pushboolean(L, 1);  /* first result if no errors */
  mask_insert(L, 1);  /* put it in place */
  status = mask_pcallk(L, mask_gettop(L) - 2, MASK_MULTRET, 0, 0, finishpcall);
  return finishpcall(L, status, 0);
}


/*
** Do a protected call with error handling. After 'mask_rotate', the
** stack will have <f, err, true, f, [args...]>; so, the function passes
** 2 to 'finishpcall' to skip the 2 first values when returning results.
*/
static int maskB_xpcall (mask_State *L) {
  int status;
  int n = mask_gettop(L);
  maskL_checktype(L, 2, MASK_TFUNCTION);  /* check error function */
  mask_pushboolean(L, 1);  /* first result */
  mask_pushvalue(L, 1);  /* function */
  mask_rotate(L, 3, 2);  /* move them below function's arguments */
  status = mask_pcallk(L, n - 2, MASK_MULTRET, 2, 2, finishpcall);
  return finishpcall(L, status, 2);
}


static int maskB_tostring (mask_State *L) {
  maskL_checkany(L, 1);
  maskL_tolstring(L, 1, NULL);
  return 1;
}


static int maskB_newuserdata (mask_State *L) {
  mask_newuserdata(L, 0);
  mask_newtable(L);
  mask_setmetatable(L, -2);
  return 1;
}


static const maskL_Reg base_funcs[] = {
  {"newuserdata", maskB_newuserdata},
  {"assert", maskB_assert},
  {"collectgarbage", maskB_collectgarbage},
  {"dofile", maskB_dofile},
  {"error", maskB_error},
  {"getmetatable", maskB_getmetatable},
  {"ipairs", maskB_ipairs},
  {"loadfile", maskB_loadfile},
  {"load", maskB_load},
  {"next", maskB_next},
  {"pairs", maskB_pairs},
  {"pcall", maskB_pcall},
  {"print", maskB_print},
  {"warn", maskB_warn},
  {"rawequal", maskB_rawequal},
  {"rawlen", maskB_rawlen},
  {"rawget", maskB_rawget},
  {"rawset", maskB_rawset},
  {"select", maskB_select},
  {"setmetatable", maskB_setmetatable},
  {"tonumber", maskB_tonumber},
  {"tostring", maskB_tostring},
  {"type", maskB_type},
  {"xpcall", maskB_xpcall},
  /* placeholders */
  {MASK_GNAME, NULL},
  {"_VERSION", NULL},
  {NULL, NULL}
};


MASKMOD_API int maskopen_base (mask_State *L) {
  /* open lib into global table */
  mask_pushglobaltable(L);
  maskL_setfuncs(L, base_funcs, 0);
  /* set global _G */
  mask_pushvalue(L, -1);
  mask_setfield(L, -2, MASK_GNAME);
  /* set global _VERSION */
  mask_pushliteral(L, MASK_VERSION);
  mask_setfield(L, -2, "_VERSION");
  /* set global _PVERSION */
  mask_pushliteral(L, MASK_VERSION);
  mask_setfield(L, -2, "_PVERSION");
  /* set global _PSOUP */
#ifdef MASK_USE_SOUP
  mask_pushboolean(L, true);
#else
  mask_pushboolean(L, false);
#endif
  mask_setfield(L, -2, "_PSOUP");
  return 1;
}

