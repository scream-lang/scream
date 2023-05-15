/*
** $Id: lstate.c $
** Global State
** See Copyright Notice in mask.h
*/

#define lstate_c
#define MASK_CORE

#include "lprefix.h"


#include <stddef.h>
#include <string.h>

#include "mask.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "llex.h"
#include "lmem.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"



/*
** thread state + extra space
*/
typedef struct LX {
  lu_byte extra_[MASK_EXTRASPACE];
  mask_State l;
} LX;


/*
** Main thread combines a thread state and the global state
*/
typedef struct LG {
  LX l;
  global_State g;
} LG;



#define fromstate(L)	(cast(LX *, cast(lu_byte *, (L)) - offsetof(LX, l)))


/*
** A macro to create a "random" seed when a state is created;
** the seed is used to randomize string hashes.
*/
#if !defined(maski_makeseed)

#include <time.h>

/*
** Compute an initial seed with some level of randomness.
** Rely on Address Space Layout Randomization (if present) and
** current time.
*/
#define addbuff(b,p,e) \
  { size_t t = cast_sizet(e); \
    memcpy(b + p, &t, sizeof(t)); p += sizeof(t); }

static unsigned int maski_makeseed (mask_State *L) {
  char buff[3 * sizeof(size_t)];
  unsigned int h = cast_uint(time(NULL));
  int p = 0;
  addbuff(buff, p, L);  /* heap variable */
  addbuff(buff, p, &h);  /* local variable */
  addbuff(buff, p, &mask_newstate);  /* public function */
  mask_assert(p == sizeof(buff));
  return maskS_hash(buff, p, h);
}

#endif


/*
** set GCdebt to a new value keeping the value (totalbytes + GCdebt)
** invariant (and avoiding underflows in 'totalbytes')
*/
void maskE_setdebt (global_State *g, l_mem debt) {
  l_mem tb = gettotalbytes(g);
  mask_assert(tb > 0);
  if (debt < tb - MAX_LMEM)
    debt = tb - MAX_LMEM;  /* will make 'totalbytes == MAX_LMEM' */
  g->totalbytes = tb - debt;
  g->GCdebt = debt;
}


MASK_API int mask_setcstacklimit (mask_State *L, unsigned int limit) {
  UNUSED(L); UNUSED(limit);
  return MASKI_MAXCCALLS;  /* warning?? */
}


CallInfo *maskE_extendCI (mask_State *L) {
  CallInfo *ci;
  mask_assert(L->ci->next == NULL);
  ci = maskM_new(L, CallInfo);
  mask_assert(L->ci->next == NULL);
  L->ci->next = ci;
  ci->previous = L->ci;
  ci->next = NULL;
  ci->u.l.trap = 0;
  L->nci++;
  return ci;
}


/*
** free all CallInfo structures not in use by a thread
*/
void maskE_freeCI (mask_State *L) {
  CallInfo *ci = L->ci;
  CallInfo *next = ci->next;
  ci->next = NULL;
  while ((ci = next) != NULL) {
    next = ci->next;
    maskM_free(L, ci);
    L->nci--;
  }
}


/*
** free half of the CallInfo structures not in use by a thread,
** keeping the first one.
*/
void maskE_shrinkCI (mask_State *L) {
  CallInfo *ci = L->ci->next;  /* first free CallInfo */
  CallInfo *next;
  if (ci == NULL)
    return;  /* no extra elements */
  while ((next = ci->next) != NULL) {  /* two extra elements? */
    CallInfo *next2 = next->next;  /* next's next */
    ci->next = next2;  /* remove next from the list */
    L->nci--;
    maskM_free(L, next);  /* free next */
    if (next2 == NULL)
      break;  /* no more elements */
    else {
      next2->previous = ci;
      ci = next2;  /* continue */
    }
  }
}


/*
** Called when 'getCcalls(L)' larger or equal to MASKI_MAXCCALLS.
** If equal, raises an overflow error. If value is larger than
** MASKI_MAXCCALLS (which means it is handling an overflow) but
** not much larger, does not report an error (to allow overflow
** handling to work).
*/
void maskE_checkcstack (mask_State *L) {
  if (getCcalls(L) == MASKI_MAXCCALLS)
    maskG_runerror(L, "C stack overflow");
  else if (getCcalls(L) >= (MASKI_MAXCCALLS / 10 * 11))
    maskD_throw(L, MASK_ERRERR);  /* error while handling stack error */
}


MASKI_FUNC void maskE_incCstack (mask_State *L) {
  L->nCcalls++;
  if (l_unlikely(getCcalls(L) >= MASKI_MAXCCALLS))
    maskE_checkcstack(L);
}


static void stack_init (mask_State *L1, mask_State *L) {
  int i; CallInfo *ci;
  /* initialize stack array */
  L1->stack = maskM_newvector(L, BASIC_STACK_SIZE + EXTRA_STACK, StackValue);
  L1->tbclist = L1->stack;
  for (i = 0; i < BASIC_STACK_SIZE + EXTRA_STACK; i++)
    setnilvalue(s2v(L1->stack + i));  /* erase new stack */
  L1->top = L1->stack;
  L1->stack_last = L1->stack + BASIC_STACK_SIZE;
  /* initialize first ci */
  ci = &L1->base_ci;
  ci->next = ci->previous = NULL;
  ci->callstatus = CIST_C;
  ci->func = L1->top;
  ci->u.c.k = NULL;
  ci->nresults = 0;
  setnilvalue(s2v(L1->top));  /* 'function' entry for this 'ci' */
  L1->top++;
  ci->top = L1->top + MASK_MINSTACK;
  L1->ci = ci;
}


static void freestack (mask_State *L) {
  if (L->stack == NULL)
    return;  /* stack not completely built yet */
  L->ci = &L->base_ci;  /* free the entire 'ci' list */
  maskE_freeCI(L);
  mask_assert(L->nci == 0);
  maskM_freearray(L, L->stack, stacksize(L) + EXTRA_STACK);  /* free stack */
}


/*
** Create registry table and its predefined values
*/
static void init_registry (mask_State *L, global_State *g) {
  /* create registry */
  Table *registry = maskH_new(L);
  sethvalue(L, &g->l_registry, registry);
  maskH_resize(L, registry, MASK_RIDX_LAST, 0);
  /* registry[MASK_RIDX_MAINTHREAD] = L */
  setthvalue(L, &registry->array[MASK_RIDX_MAINTHREAD - 1], L);
  /* registry[MASK_RIDX_GLOBALS] = new table (table of globals) */
  sethvalue(L, &registry->array[MASK_RIDX_GLOBALS - 1], maskH_new(L));
}


/*
** open parts of the state that may cause memory-allocation errors.
*/
static void f_maskopen (mask_State *L, void *ud) {
  global_State *g = G(L);
  UNUSED(ud);
  stack_init(L, L);  /* init stack */
  init_registry(L, g);
  maskS_init(L);
  maskT_init(L);
  maskX_init(L);
  g->gcstp = 0;  /* allow gc */
  setnilvalue(&g->nilvalue);  /* now state is complete */
  maski_userstateopen(L);
}


/*
** preinitialize a thread with consistent values without allocating
** any memory (to avoid errors)
*/
static void preinit_thread (mask_State *L, global_State *g) {
  G(L) = g;
  L->stack = NULL;
  L->ci = NULL;
  L->nci = 0;
  L->twups = L;  /* thread has no upvalues */
  L->nCcalls = 0;
  L->errorJmp = NULL;
  L->hook = NULL;
  L->hookmask = 0;
  L->basehookcount = 0;
  L->allowhook = 1;
  resethookcount(L);
  L->openupval = NULL;
  L->status = MASK_OK;
  L->errfunc = 0;
  L->oldpc = 0;
}


static void close_state (mask_State *L) {
  global_State *g = G(L);
  if (!completestate(g))  /* closing a partially built state? */
    maskC_freeallobjects(L);  /* just collect its objects */
  else {  /* closing a fully built state */
    L->ci = &L->base_ci;  /* unwind CallInfo list */
    maskD_closeprotected(L, 1, MASK_OK);  /* close all upvalues */
    maskC_freeallobjects(L);  /* collect all objects */
    maski_userstateclose(L);
  }
  maskM_freearray(L, G(L)->strt.hash, G(L)->strt.size);
  freestack(L);
  mask_assert(gettotalbytes(g) == sizeof(LG));
  (*g->frealloc)(g->ud, fromstate(L), sizeof(LG), 0);  /* free main block */
}


MASK_API mask_State *mask_newthread (mask_State *L) {
  global_State *g;
  mask_State *L1;
  mask_lock(L);
  g = G(L);
  maskC_checkGC(L);
  /* create new thread */
  L1 = &cast(LX *, maskM_newobject(L, MASK_TTHREAD, sizeof(LX)))->l;
  L1->marked = maskC_white(g);
  L1->tt = MASK_VTHREAD;
  /* link it on list 'allgc' */
  L1->next = g->allgc;
  g->allgc = obj2gco(L1);
  /* anchor it on L stack */
  setthvalue2s(L, L->top, L1);
  api_incr_top(L);
  preinit_thread(L1, g);
  L1->hookmask = L->hookmask;
  L1->basehookcount = L->basehookcount;
  L1->hook = L->hook;
  resethookcount(L1);
  /* initialize L1 extra space */
  memcpy(mask_getextraspace(L1), mask_getextraspace(g->mainthread),
         MASK_EXTRASPACE);
  maski_userstatethread(L, L1);
  stack_init(L1, L);  /* init stack */
  mask_unlock(L);
  return L1;
}


void maskE_freethread (mask_State *L, mask_State *L1) {
  LX *l = fromstate(L1);
  maskF_closeupval(L1, L1->stack);  /* close all upvalues */
  mask_assert(L1->openupval == NULL);
  maski_userstatefree(L, L1);
  freestack(L1);
  maskM_free(L, l);
}


int maskE_resetthread (mask_State *L, int status) {
  CallInfo *ci = L->ci = &L->base_ci;  /* unwind CallInfo list */
  setnilvalue(s2v(L->stack));  /* 'function' entry for basic 'ci' */
  ci->func = L->stack;
  ci->callstatus = CIST_C;
  if (status == MASK_YIELD)
    status = MASK_OK;
  L->status = MASK_OK;  /* so it can run __close metamethods */
  status = maskD_closeprotected(L, 1, status);
  if (status != MASK_OK)  /* errors? */
    maskD_seterrorobj(L, status, L->stack + 1);
  else
    L->top = L->stack + 1;
  ci->top = L->top + MASK_MINSTACK;
  maskD_reallocstack(L, cast_int(ci->top - L->stack), 0);
  return status;
}


MASK_API int mask_resetthread (mask_State *L, mask_State *from) {
  int status;
  mask_lock(L);
  L->nCcalls = (from) ? getCcalls(from) : 0;
  status = maskE_resetthread(L, L->status);
  mask_unlock(L);
  return status;
}


MASK_API mask_State *mask_newstate (mask_Alloc f, void *ud) {
  int i;
  mask_State *L;
  global_State *g;
  LG *l = cast(LG *, (*f)(ud, NULL, MASK_TTHREAD, sizeof(LG)));
  if (l == NULL) return NULL;
  L = &l->l.l;
  g = &l->g;
  L->tt = MASK_VTHREAD;
  g->currentwhite = bitmask(WHITE0BIT);
  L->marked = maskC_white(g);
  preinit_thread(L, g);
  g->allgc = obj2gco(L);  /* by now, only object is the main thread */
  L->next = NULL;
  incnny(L);  /* main thread is always non yieldable */
  g->frealloc = f;
  g->ud = ud;
  g->warnf = NULL;
  g->ud_warn = NULL;
  g->mainthread = L;
  g->seed = maski_makeseed(L);
  g->gcstp = GCSTPGC;  /* no GC while building state */
  g->strt.size = g->strt.nuse = 0;
  g->strt.hash = NULL;
  setnilvalue(&g->l_registry);
  g->panic = NULL;
  g->gcstate = GCSpause;
  g->gckind = KGC_INC;
  g->gcstopem = 0;
  g->gcemergency = 0;
  g->finobj = g->tobefnz = g->fixedgc = NULL;
  g->firstold1 = g->survival = g->old1 = g->reallyold = NULL;
  g->finobjsur = g->finobjold1 = g->finobjrold = NULL;
  g->sweepgc = NULL;
  g->gray = g->grayagain = NULL;
  g->weak = g->ephemeron = g->allweak = NULL;
  g->twups = NULL;
  g->totalbytes = sizeof(LG);
  g->GCdebt = 0;
  g->lastatomic = 0;
  setivalue(&g->nilvalue, 0);  /* to signal that state is not yet built */
  setgcparam(g->gcpause, MASKI_GCPAUSE);
  setgcparam(g->gcstepmul, MASKI_GCMUL);
  g->gcstepsize = MASKI_GCSTEPSIZE;
  setgcparam(g->genmajormul, MASKI_GENMAJORMUL);
  g->genminormul = MASKI_GENMINORMUL;
  for (i=0; i < MASK_NUMTAGS; i++) g->mt[i] = NULL;
  if (maskD_rawrunprotected(L, f_maskopen, NULL) != MASK_OK) {
    /* memory allocation error: free partial state */
    close_state(L);
    L = NULL;
  }
  return L;
}


MASK_API void mask_close (mask_State *L) {
  mask_lock(L);
  L = G(L)->mainthread;  /* only the main thread can be closed */
  close_state(L);
}


void maskE_warning (mask_State *L, const char *msg, int tocont) {
  mask_WarnFunction wf = G(L)->warnf;
  if (wf != NULL)
    wf(G(L)->ud_warn, msg, tocont);
}


/*
** Generate a warning from an error message
*/
void maskE_warnerror (mask_State *L, const char *where) {
  TValue *errobj = s2v(L->top - 1);  /* error object */
  const char *msg = (ttisstring(errobj))
                  ? svalue(errobj)
                  : "error object is not a string";
  /* produce "warning: error in %s (%s)" (where, msg) */
  maskE_warning(L, "warning: error in ", 1);
  maskE_warning(L, where, 1);
  maskE_warning(L, " (", 1);
  maskE_warning(L, msg, 1);
  maskE_warning(L, ")", 0);
}

