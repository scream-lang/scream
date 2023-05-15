#pragma once
/*
** $Id: ldo.h $
** Stack and Call structure of Hello
** See Copyright Notice in hello.h
*/

#include "lobject.h"
#include "lstate.h"
#include "lzio.h"


/*
** Macro to check stack size and grow stack if needed.  Parameters
** 'pre'/'pos' allow the macro to preserve a pointer into the
** stack across reallocations, doing the work only when needed.
** It also allows the running of one GC step when the stack is
** reallocated.
** 'condmovestack' is used in heavy tests to force a stack reallocation
** at every check.
*/
#define helloD_checkstackaux(L,n,pre,pos)  \
	if (l_unlikely(L->stack_last - L->top <= (n))) \
	  { pre; helloD_growstack(L, n, 1); pos; } \
        else { condmovestack(L,pre,pos); }

/* In general, 'pre'/'pos' are empty (nothing to save) */
#define helloD_checkstack(L,n)	helloD_checkstackaux(L,n,(void)0,(void)0)



#define savestack(L,p)		((char *)(p) - (char *)L->stack)
#define restorestack(L,n)	((StkId)((char *)L->stack + (n)))


/* macro to check stack size, preserving 'p' */
#define checkstackp(L,n,p)  \
  helloD_checkstackaux(L, n, \
    ptrdiff_t t__ = savestack(L, p),  /* save 'p' */ \
    p = restorestack(L, t__))  /* 'pos' part: restore 'p' */


/* macro to check stack size and GC, preserving 'p' */
#define checkstackGCp(L,n,p)  \
  helloD_checkstackaux(L, n, \
    ptrdiff_t t__ = savestack(L, p);  /* save 'p' */ \
    helloC_checkGC(L),  /* stack grow uses memory */ \
    p = restorestack(L, t__))  /* 'pos' part: restore 'p' */


/* macro to check stack size and GC */
#define checkstackGC(L,fsize)  \
	helloD_checkstackaux(L, (fsize), helloC_checkGC(L), (void)0)


/* type of protected functions, to be ran by 'runprotected' */
typedef void (*Pfunc) (hello_State *L, void *ud);

HELLOI_FUNC void helloD_seterrorobj (hello_State *L, int errcode, StkId oldtop);
HELLOI_FUNC int helloD_protectedparser (hello_State *L, ZIO *z, const char *name,
                                                  const char *mode);
HELLOI_FUNC void helloD_hook (hello_State *L, int event, int line,
                                        int fTransfer, int nTransfer);
HELLOI_FUNC void helloD_hookcall (hello_State *L, CallInfo *ci);
HELLOI_FUNC int helloD_pretailcall (hello_State *L, CallInfo *ci, StkId func,
                                              int narg1, int delta);
HELLOI_FUNC CallInfo *helloD_precall (hello_State *L, StkId func, int nResults);
HELLOI_FUNC void helloD_call (hello_State *L, StkId func, int nResults);
HELLOI_FUNC void helloD_callnoyield (hello_State *L, StkId func, int nResults);
HELLOI_FUNC StkId helloD_tryfuncTM (hello_State *L, StkId func);
HELLOI_FUNC int helloD_closeprotected (hello_State *L, ptrdiff_t level, int status);
HELLOI_FUNC int helloD_pcall (hello_State *L, Pfunc func, void *u,
                                        ptrdiff_t oldtop, ptrdiff_t ef);
HELLOI_FUNC void helloD_poscall (hello_State *L, CallInfo *ci, int nres);
HELLOI_FUNC int helloD_reallocstack (hello_State *L, int newsize, int raiseerror);
HELLOI_FUNC int helloD_growstack (hello_State *L, int n, int raiseerror);
HELLOI_FUNC void helloD_shrinkstack (hello_State *L);
HELLOI_FUNC void helloD_inctop (hello_State *L);

[[noreturn]] HELLOI_FUNC void helloD_throw (hello_State *L, int errcode);
HELLOI_FUNC int helloD_rawrunprotected (hello_State *L, Pfunc f, void *ud);
