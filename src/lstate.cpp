/*
** $Id: lstate.c $
** Global State
** See Copyright Notice in hello.h
*/

#define lstate_c
#define HELLO_CORE

#include "lprefix.h"


#include <stddef.h>
#include <string.h>

#include "hello.h"

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
  lu_byte extra_[HELLO_EXTRASPACE];
  hello_State l;
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
#if !defined(helloi_makeseed)

#include <time.h>

/*
** Compute an initial seed with some level of randomness.
** Rely on Address Space Layout Randomization (if present) and
** current time.
*/
#define addbuff(b,p,e) \
  { size_t t = cast_sizet(e); \
    memcpy(b + p, &t, sizeof(t)); p += sizeof(t); }

static unsigned int helloi_makeseed (hello_State *L) {
  char buff[3 * sizeof(size_t)];
  unsigned int h = cast_uint(time(NULL));
  int p = 0;
  addbuff(buff, p, L);  /* heap variable */
  addbuff(buff, p, &h);  /* local variable */
  addbuff(buff, p, &hello_newstate);  /* public function */
  hello_assert(p == sizeof(buff));
  return helloS_hash(buff, p, h);
}

#endif


/*
** set GCdebt to a new value keeping the value (totalbytes + GCdebt)
** invariant (and avoiding underflows in 'totalbytes')
*/
void helloE_setdebt (global_State *g, l_mem debt) {
  l_mem tb = gettotalbytes(g);
  hello_assert(tb > 0);
  if (debt < tb - MAX_LMEM)
    debt = tb - MAX_LMEM;  /* will make 'totalbytes == MAX_LMEM' */
  g->totalbytes = tb - debt;
  g->GCdebt = debt;
}


HELLO_API int hello_setcstacklimit (hello_State *L, unsigned int limit) {
  UNUSED(L); UNUSED(limit);
  return HELLOI_MAXCCALLS;  /* warning?? */
}


CallInfo *helloE_extendCI (hello_State *L) {
  CallInfo *ci;
  hello_assert(L->ci->next == NULL);
  ci = helloM_new(L, CallInfo);
  hello_assert(L->ci->next == NULL);
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
void helloE_freeCI (hello_State *L) {
  CallInfo *ci = L->ci;
  CallInfo *next = ci->next;
  ci->next = NULL;
  while ((ci = next) != NULL) {
    next = ci->next;
    helloM_free(L, ci);
    L->nci--;
  }
}


/*
** free half of the CallInfo structures not in use by a thread,
** keeping the first one.
*/
void helloE_shrinkCI (hello_State *L) {
  CallInfo *ci = L->ci->next;  /* first free CallInfo */
  CallInfo *next;
  if (ci == NULL)
    return;  /* no extra elements */
  while ((next = ci->next) != NULL) {  /* two extra elements? */
    CallInfo *next2 = next->next;  /* next's next */
    ci->next = next2;  /* remove next from the list */
    L->nci--;
    helloM_free(L, next);  /* free next */
    if (next2 == NULL)
      break;  /* no more elements */
    else {
      next2->previous = ci;
      ci = next2;  /* continue */
    }
  }
}


/*
** Called when 'getCcalls(L)' larger or equal to HELLOI_MAXCCALLS.
** If equal, raises an overflow error. If value is larger than
** HELLOI_MAXCCALLS (which means it is handling an overflow) but
** not much larger, does not report an error (to allow overflow
** handling to work).
*/
void helloE_checkcstack (hello_State *L) {
  if (getCcalls(L) == HELLOI_MAXCCALLS)
    helloG_runerror(L, "C stack overflow");
  else if (getCcalls(L) >= (HELLOI_MAXCCALLS / 10 * 11))
    helloD_throw(L, HELLO_ERRERR);  /* error while handling stack error */
}


HELLOI_FUNC void helloE_incCstack (hello_State *L) {
  L->nCcalls++;
  if (l_unlikely(getCcalls(L) >= HELLOI_MAXCCALLS))
    helloE_checkcstack(L);
}


static void stack_init (hello_State *L1, hello_State *L) {
  int i; CallInfo *ci;
  /* initialize stack array */
  L1->stack = helloM_newvector(L, BASIC_STACK_SIZE + EXTRA_STACK, StackValue);
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
  ci->top = L1->top + HELLO_MINSTACK;
  L1->ci = ci;
}


static void freestack (hello_State *L) {
  if (L->stack == NULL)
    return;  /* stack not completely built yet */
  L->ci = &L->base_ci;  /* free the entire 'ci' list */
  helloE_freeCI(L);
  hello_assert(L->nci == 0);
  helloM_freearray(L, L->stack, stacksize(L) + EXTRA_STACK);  /* free stack */
}


/*
** Create registry table and its predefined values
*/
static void init_registry (hello_State *L, global_State *g) {
  /* create registry */
  Table *registry = helloH_new(L);
  sethvalue(L, &g->l_registry, registry);
  helloH_resize(L, registry, HELLO_RIDX_LAST, 0);
  /* registry[HELLO_RIDX_MAINTHREAD] = L */
  setthvalue(L, &registry->array[HELLO_RIDX_MAINTHREAD - 1], L);
  /* registry[HELLO_RIDX_GLOBALS] = new table (table of globals) */
  sethvalue(L, &registry->array[HELLO_RIDX_GLOBALS - 1], helloH_new(L));
}


/*
** open parts of the state that may cause memory-allocation errors.
*/
static void f_helloopen (hello_State *L, void *ud) {
  global_State *g = G(L);
  UNUSED(ud);
  stack_init(L, L);  /* init stack */
  init_registry(L, g);
  helloS_init(L);
  helloT_init(L);
  helloX_init(L);
  g->gcstp = 0;  /* allow gc */
  setnilvalue(&g->nilvalue);  /* now state is complete */
  helloi_userstateopen(L);
}


/*
** preinitialize a thread with consistent values without allocating
** any memory (to avoid errors)
*/
static void preinit_thread (hello_State *L, global_State *g) {
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
  L->status = HELLO_OK;
  L->errfunc = 0;
  L->oldpc = 0;
}


static void close_state (hello_State *L) {
  global_State *g = G(L);
  if (!completestate(g))  /* closing a partially built state? */
    helloC_freeallobjects(L);  /* just collect its objects */
  else {  /* closing a fully built state */
    L->ci = &L->base_ci;  /* unwind CallInfo list */
    helloD_closeprotected(L, 1, HELLO_OK);  /* close all upvalues */
    helloC_freeallobjects(L);  /* collect all objects */
    helloi_userstateclose(L);
  }
  helloM_freearray(L, G(L)->strt.hash, G(L)->strt.size);
  freestack(L);
  hello_assert(gettotalbytes(g) == sizeof(LG));
  (*g->frealloc)(g->ud, fromstate(L), sizeof(LG), 0);  /* free main block */
}


HELLO_API hello_State *hello_newthread (hello_State *L) {
  global_State *g;
  hello_State *L1;
  hello_lock(L);
  g = G(L);
  helloC_checkGC(L);
  /* create new thread */
  L1 = &cast(LX *, helloM_newobject(L, HELLO_TTHREAD, sizeof(LX)))->l;
  L1->marked = helloC_white(g);
  L1->tt = HELLO_VTHREAD;
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
  memcpy(hello_getextraspace(L1), hello_getextraspace(g->mainthread),
         HELLO_EXTRASPACE);
  helloi_userstatethread(L, L1);
  stack_init(L1, L);  /* init stack */
  hello_unlock(L);
  return L1;
}


void helloE_freethread (hello_State *L, hello_State *L1) {
  LX *l = fromstate(L1);
  helloF_closeupval(L1, L1->stack);  /* close all upvalues */
  hello_assert(L1->openupval == NULL);
  helloi_userstatefree(L, L1);
  freestack(L1);
  helloM_free(L, l);
}


int helloE_resetthread (hello_State *L, int status) {
  CallInfo *ci = L->ci = &L->base_ci;  /* unwind CallInfo list */
  setnilvalue(s2v(L->stack));  /* 'function' entry for basic 'ci' */
  ci->func = L->stack;
  ci->callstatus = CIST_C;
  if (status == HELLO_YIELD)
    status = HELLO_OK;
  L->status = HELLO_OK;  /* so it can run __close metamethods */
  status = helloD_closeprotected(L, 1, status);
  if (status != HELLO_OK)  /* errors? */
    helloD_seterrorobj(L, status, L->stack + 1);
  else
    L->top = L->stack + 1;
  ci->top = L->top + HELLO_MINSTACK;
  helloD_reallocstack(L, cast_int(ci->top - L->stack), 0);
  return status;
}


HELLO_API int hello_resetthread (hello_State *L, hello_State *from) {
  int status;
  hello_lock(L);
  L->nCcalls = (from) ? getCcalls(from) : 0;
  status = helloE_resetthread(L, L->status);
  hello_unlock(L);
  return status;
}


HELLO_API hello_State *hello_newstate (hello_Alloc f, void *ud) {
  int i;
  hello_State *L;
  global_State *g;
  LG *l = cast(LG *, (*f)(ud, NULL, HELLO_TTHREAD, sizeof(LG)));
  if (l == NULL) return NULL;
  L = &l->l.l;
  g = &l->g;
  L->tt = HELLO_VTHREAD;
  g->currentwhite = bitmask(WHITE0BIT);
  L->marked = helloC_white(g);
  preinit_thread(L, g);
  g->allgc = obj2gco(L);  /* by now, only object is the main thread */
  L->next = NULL;
  incnny(L);  /* main thread is always non yieldable */
  g->frealloc = f;
  g->ud = ud;
  g->warnf = NULL;
  g->ud_warn = NULL;
  g->mainthread = L;
  g->seed = helloi_makeseed(L);
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
  setgcparam(g->gcpause, HELLOI_GCPAUSE);
  setgcparam(g->gcstepmul, HELLOI_GCMUL);
  g->gcstepsize = HELLOI_GCSTEPSIZE;
  setgcparam(g->genmajormul, HELLOI_GENMAJORMUL);
  g->genminormul = HELLOI_GENMINORMUL;
  for (i=0; i < HELLO_NUMTAGS; i++) g->mt[i] = NULL;
  if (helloD_rawrunprotected(L, f_helloopen, NULL) != HELLO_OK) {
    /* memory allocation error: free partial state */
    close_state(L);
    L = NULL;
  }
  return L;
}


HELLO_API void hello_close (hello_State *L) {
  hello_lock(L);
  L = G(L)->mainthread;  /* only the main thread can be closed */
  close_state(L);
}


void helloE_warning (hello_State *L, const char *msg, int tocont) {
  hello_WarnFunction wf = G(L)->warnf;
  if (wf != NULL)
    wf(G(L)->ud_warn, msg, tocont);
}


/*
** Generate a warning from an error message
*/
void helloE_warnerror (hello_State *L, const char *where) {
  TValue *errobj = s2v(L->top - 1);  /* error object */
  const char *msg = (ttisstring(errobj))
                  ? svalue(errobj)
                  : "error object is not a string";
  /* produce "warning: error in %s (%s)" (where, msg) */
  helloE_warning(L, "warning: error in ", 1);
  helloE_warning(L, where, 1);
  helloE_warning(L, " (", 1);
  helloE_warning(L, msg, 1);
  helloE_warning(L, ")", 0);
}

