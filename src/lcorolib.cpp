/*
** $Id: lcorolib.c $
** Coroutine Library
** See Copyright Notice in hello.h
*/

#define lcorolib_c
#define HELLO_LIB

#include "lprefix.h"


#include <stdlib.h>

#include "hello.h"

#include "lauxlib.h"
#include "hellolib.h"


static hello_State *getco (hello_State *L) {
  hello_State *co = hello_tothread(L, 1);
  helloL_argexpected(L, co, 1, "thread");
  return co;
}


/*
** Resumes a coroutine. Returns the number of results for non-error
** cases or -1 for errors.
*/
static int auxresume (hello_State *L, hello_State *co, int narg) {
  int status, nres;
  if (l_unlikely(!hello_checkstack(co, narg))) {
    hello_pushliteral(L, "too many arguments to resume");
    return -1;  /* error flag */
  }
  hello_xmove(L, co, narg);
  status = hello_resume(co, L, narg, &nres);
  if (l_likely(status == HELLO_OK || status == HELLO_YIELD)) {
    if (l_unlikely(!hello_checkstack(L, nres + 1))) {
      hello_pop(co, nres);  /* remove results anyway */
      hello_pushliteral(L, "too many results to resume");
      return -1;  /* error flag */
    }
    hello_xmove(co, L, nres);  /* move yielded values */
    return nres;
  }
  else {
    hello_xmove(co, L, 1);  /* move error message */
    return -1;  /* error flag */
  }
}


static int helloB_coresume (hello_State *L) {
  hello_State *co = getco(L);
  int r;
  r = auxresume(L, co, hello_gettop(L) - 1);
  if (l_unlikely(r < 0)) {
    hello_pushboolean(L, 0);
    hello_insert(L, -2);
    return 2;  /* return false + error message */
  }
  else {
    hello_pushboolean(L, 1);
    hello_insert(L, -(r + 1));
    return r + 1;  /* return true + 'resume' returns */
  }
}


static int helloB_auxwrap (hello_State *L) {
  hello_State *co = hello_tothread(L, hello_upvalueindex(1));
  int r = auxresume(L, co, hello_gettop(L));
  if (l_unlikely(r < 0)) {  /* error? */
    int stat = hello_status(co);
    if (stat != HELLO_OK && stat != HELLO_YIELD) {  /* error in the coroutine? */
      stat = hello_resetthread(co, L);  /* close its tbc variables */
      hello_assert(stat != HELLO_OK);
      hello_xmove(co, L, 1);  /* move error message to the caller */
    }
    if (stat != HELLO_ERRMEM &&  /* not a memory error and ... */
        hello_type(L, -1) == HELLO_TSTRING) {  /* ... error object is a string? */
      helloL_where(L, 1);  /* add extra info, if available */
      hello_insert(L, -2);
      hello_concat(L, 2);
    }
    hello_error(L);  /* propagate error */
  }
  return r;
}


static int helloB_cocreate (hello_State *L) {
  hello_State *NL;
  helloL_checktype(L, 1, HELLO_TFUNCTION);
  NL = hello_newthread(L);
  hello_pushvalue(L, 1);  /* move function to top */
  hello_xmove(L, NL, 1);  /* move function from L to NL */
  return 1;
}


static int helloB_cowrap (hello_State *L) {
  helloB_cocreate(L);
  hello_pushcclosure(L, helloB_auxwrap, 1);
  return 1;
}


static int helloB_yield (hello_State *L) {
  return hello_yield(L, hello_gettop(L));
}


#define COS_RUN		0
#define COS_DEAD	1
#define COS_YIELD	2
#define COS_NORM	3


static const char *const statname[] =
  {"running", "dead", "suspended", "normal"};


static int auxstatus (hello_State *L, hello_State *co) {
  if (L == co) return COS_RUN;
  else {
    switch (hello_status(co)) {
      case HELLO_YIELD:
        return COS_YIELD;
      case HELLO_OK: {
        hello_Debug ar;
        if (hello_getstack(co, 0, &ar))  /* does it have frames? */
          return COS_NORM;  /* it is running */
        else if (hello_gettop(co) == 0)
            return COS_DEAD;
        else
          return COS_YIELD;  /* initial state */
      }
      default:  /* some error occurred */
        return COS_DEAD;
    }
  }
}


static int helloB_costatus (hello_State *L) {
  hello_State *co = getco(L);
  hello_pushstring(L, statname[auxstatus(L, co)]);
  return 1;
}


static int helloB_yieldable (hello_State *L) {
  hello_State *co = hello_isnone(L, 1) ? L : getco(L);
  hello_pushboolean(L, hello_isyieldable(co));
  return 1;
}


static int helloB_corunning (hello_State *L) {
  int ismain = hello_pushthread(L);
  hello_pushboolean(L, ismain);
  return 2;
}


static int helloB_close (hello_State *L) {
  hello_State *co = getco(L);
  int status = auxstatus(L, co);
  switch (status) {
    case COS_DEAD: case COS_YIELD: {
      status = hello_resetthread(co, L);
      if (status == HELLO_OK) {
        hello_pushboolean(L, 1);
        return 1;
      }
      else {
        hello_pushboolean(L, 0);
        hello_xmove(co, L, 1);  /* move error message */
        return 2;
      }
    }
    default:  /* normal or running coroutine */
      helloL_error(L, "cannot close a %s coroutine", statname[status]);
  }
}


static const helloL_Reg co_funcs[] = {
  {"create", helloB_cocreate},
  {"resume", helloB_coresume},
  {"running", helloB_corunning},
  {"status", helloB_costatus},
  {"wrap", helloB_cowrap},
  {"yield", helloB_yield},
  {"isyieldable", helloB_yieldable},
  {"close", helloB_close},
  {NULL, NULL}
};



HELLOMOD_API int helloopen_coroutine (hello_State *L) {
  helloL_newlib(L, co_funcs);
  return 1;
}

