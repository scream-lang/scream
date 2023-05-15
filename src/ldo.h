#pragma once
/*
** $Id: ldo.h $
** Stack and Call structure of Mask
** See Copyright Notice in mask.h
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
#define maskD_checkstackaux(L,n,pre,pos)  \
	if (l_unlikely(L->stack_last - L->top <= (n))) \
	  { pre; maskD_growstack(L, n, 1); pos; } \
        else { condmovestack(L,pre,pos); }

/* In general, 'pre'/'pos' are empty (nothing to save) */
#define maskD_checkstack(L,n)	maskD_checkstackaux(L,n,(void)0,(void)0)



#define savestack(L,p)		((char *)(p) - (char *)L->stack)
#define restorestack(L,n)	((StkId)((char *)L->stack + (n)))


/* macro to check stack size, preserving 'p' */
#define checkstackp(L,n,p)  \
  maskD_checkstackaux(L, n, \
    ptrdiff_t t__ = savestack(L, p),  /* save 'p' */ \
    p = restorestack(L, t__))  /* 'pos' part: restore 'p' */


/* macro to check stack size and GC, preserving 'p' */
#define checkstackGCp(L,n,p)  \
  maskD_checkstackaux(L, n, \
    ptrdiff_t t__ = savestack(L, p);  /* save 'p' */ \
    maskC_checkGC(L),  /* stack grow uses memory */ \
    p = restorestack(L, t__))  /* 'pos' part: restore 'p' */


/* macro to check stack size and GC */
#define checkstackGC(L,fsize)  \
	maskD_checkstackaux(L, (fsize), maskC_checkGC(L), (void)0)


/* type of protected functions, to be ran by 'runprotected' */
typedef void (*Pfunc) (mask_State *L, void *ud);

MASKI_FUNC void maskD_seterrorobj (mask_State *L, int errcode, StkId oldtop);
MASKI_FUNC int maskD_protectedparser (mask_State *L, ZIO *z, const char *name,
                                                  const char *mode);
MASKI_FUNC void maskD_hook (mask_State *L, int event, int line,
                                        int fTransfer, int nTransfer);
MASKI_FUNC void maskD_hookcall (mask_State *L, CallInfo *ci);
MASKI_FUNC int maskD_pretailcall (mask_State *L, CallInfo *ci, StkId func,
                                              int narg1, int delta);
MASKI_FUNC CallInfo *maskD_precall (mask_State *L, StkId func, int nResults);
MASKI_FUNC void maskD_call (mask_State *L, StkId func, int nResults);
MASKI_FUNC void maskD_callnoyield (mask_State *L, StkId func, int nResults);
MASKI_FUNC StkId maskD_tryfuncTM (mask_State *L, StkId func);
MASKI_FUNC int maskD_closeprotected (mask_State *L, ptrdiff_t level, int status);
MASKI_FUNC int maskD_pcall (mask_State *L, Pfunc func, void *u,
                                        ptrdiff_t oldtop, ptrdiff_t ef);
MASKI_FUNC void maskD_poscall (mask_State *L, CallInfo *ci, int nres);
MASKI_FUNC int maskD_reallocstack (mask_State *L, int newsize, int raiseerror);
MASKI_FUNC int maskD_growstack (mask_State *L, int n, int raiseerror);
MASKI_FUNC void maskD_shrinkstack (mask_State *L);
MASKI_FUNC void maskD_inctop (mask_State *L);

[[noreturn]] MASKI_FUNC void maskD_throw (mask_State *L, int errcode);
MASKI_FUNC int maskD_rawrunprotected (mask_State *L, Pfunc f, void *ud);
