/*
** $Id: lcorolib.c $
** Coroutine Library
** See Copyright Notice in mask.h
*/

#define lcorolib_c
#define MASK_LIB

#include "lprefix.h"


#include <stdlib.h>

#include "mask.h"

#include "lauxlib.h"
#include "masklib.h"


static mask_State *getco (mask_State *L) {
  mask_State *co = mask_tothread(L, 1);
  maskL_argexpected(L, co, 1, "thread");
  return co;
}


/*
** Resumes a coroutine. Returns the number of results for non-error
** cases or -1 for errors.
*/
static int auxresume (mask_State *L, mask_State *co, int narg) {
  int status, nres;
  if (l_unlikely(!mask_checkstack(co, narg))) {
    mask_pushliteral(L, "too many arguments to resume");
    return -1;  /* error flag */
  }
  mask_xmove(L, co, narg);
  status = mask_resume(co, L, narg, &nres);
  if (l_likely(status == MASK_OK || status == MASK_YIELD)) {
    if (l_unlikely(!mask_checkstack(L, nres + 1))) {
      mask_pop(co, nres);  /* remove results anyway */
      mask_pushliteral(L, "too many results to resume");
      return -1;  /* error flag */
    }
    mask_xmove(co, L, nres);  /* move yielded values */
    return nres;
  }
  else {
    mask_xmove(co, L, 1);  /* move error message */
    return -1;  /* error flag */
  }
}


static int maskB_coresume (mask_State *L) {
  mask_State *co = getco(L);
  int r;
  r = auxresume(L, co, mask_gettop(L) - 1);
  if (l_unlikely(r < 0)) {
    mask_pushboolean(L, 0);
    mask_insert(L, -2);
    return 2;  /* return false + error message */
  }
  else {
    mask_pushboolean(L, 1);
    mask_insert(L, -(r + 1));
    return r + 1;  /* return true + 'resume' returns */
  }
}


static int maskB_auxwrap (mask_State *L) {
  mask_State *co = mask_tothread(L, mask_upvalueindex(1));
  int r = auxresume(L, co, mask_gettop(L));
  if (l_unlikely(r < 0)) {  /* error? */
    int stat = mask_status(co);
    if (stat != MASK_OK && stat != MASK_YIELD) {  /* error in the coroutine? */
      stat = mask_resetthread(co, L);  /* close its tbc variables */
      mask_assert(stat != MASK_OK);
      mask_xmove(co, L, 1);  /* move error message to the caller */
    }
    if (stat != MASK_ERRMEM &&  /* not a memory error and ... */
        mask_type(L, -1) == MASK_TSTRING) {  /* ... error object is a string? */
      maskL_where(L, 1);  /* add extra info, if available */
      mask_insert(L, -2);
      mask_concat(L, 2);
    }
    mask_error(L);  /* propagate error */
  }
  return r;
}


static int maskB_cocreate (mask_State *L) {
  mask_State *NL;
  maskL_checktype(L, 1, MASK_TFUNCTION);
  NL = mask_newthread(L);
  mask_pushvalue(L, 1);  /* move function to top */
  mask_xmove(L, NL, 1);  /* move function from L to NL */
  return 1;
}


static int maskB_cowrap (mask_State *L) {
  maskB_cocreate(L);
  mask_pushcclosure(L, maskB_auxwrap, 1);
  return 1;
}


static int maskB_yield (mask_State *L) {
  return mask_yield(L, mask_gettop(L));
}


#define COS_RUN		0
#define COS_DEAD	1
#define COS_YIELD	2
#define COS_NORM	3


static const char *const statname[] =
  {"running", "dead", "suspended", "normal"};


static int auxstatus (mask_State *L, mask_State *co) {
  if (L == co) return COS_RUN;
  else {
    switch (mask_status(co)) {
      case MASK_YIELD:
        return COS_YIELD;
      case MASK_OK: {
        mask_Debug ar;
        if (mask_getstack(co, 0, &ar))  /* does it have frames? */
          return COS_NORM;  /* it is running */
        else if (mask_gettop(co) == 0)
            return COS_DEAD;
        else
          return COS_YIELD;  /* initial state */
      }
      default:  /* some error occurred */
        return COS_DEAD;
    }
  }
}


static int maskB_costatus (mask_State *L) {
  mask_State *co = getco(L);
  mask_pushstring(L, statname[auxstatus(L, co)]);
  return 1;
}


static int maskB_yieldable (mask_State *L) {
  mask_State *co = mask_isnone(L, 1) ? L : getco(L);
  mask_pushboolean(L, mask_isyieldable(co));
  return 1;
}


static int maskB_corunning (mask_State *L) {
  int ismain = mask_pushthread(L);
  mask_pushboolean(L, ismain);
  return 2;
}


static int maskB_close (mask_State *L) {
  mask_State *co = getco(L);
  int status = auxstatus(L, co);
  switch (status) {
    case COS_DEAD: case COS_YIELD: {
      status = mask_resetthread(co, L);
      if (status == MASK_OK) {
        mask_pushboolean(L, 1);
        return 1;
      }
      else {
        mask_pushboolean(L, 0);
        mask_xmove(co, L, 1);  /* move error message */
        return 2;
      }
    }
    default:  /* normal or running coroutine */
      maskL_error(L, "cannot close a %s coroutine", statname[status]);
  }
}


static const maskL_Reg co_funcs[] = {
  {"create", maskB_cocreate},
  {"resume", maskB_coresume},
  {"running", maskB_corunning},
  {"status", maskB_costatus},
  {"wrap", maskB_cowrap},
  {"yield", maskB_yield},
  {"isyieldable", maskB_yieldable},
  {"close", maskB_close},
  {NULL, NULL}
};



MASKMOD_API int maskopen_coroutine (mask_State *L) {
  maskL_newlib(L, co_funcs);
  return 1;
}

