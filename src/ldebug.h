#pragma once
/*
** $Id: ldebug.h $
** Auxiliary functions from Debug Interface module
** See Copyright Notice in hello.h
*/

#include "lstate.h"


#define pcRel(pc, p)	(cast_int((pc) - (p)->code) - 1)


/* Active Hello function (given call info) */
#define ci_func(ci)		(clLvalue(s2v((ci)->func)))


#define resethookcount(L)	(L->hookcount = L->basehookcount)

/*
** mark for entries in 'lineinfo' array that has absolute information in
** 'abslineinfo' array
*/
#define ABSLINEINFO	(-0x80)


/*
** MAXimum number of successive Instructions WiTHout ABSolute line
** information. (A power of two allows fast divisions.)
*/
#if !defined(MAXIWTHABS)
#define MAXIWTHABS	128
#endif


HELLOI_FUNC int helloG_getfuncline (const Proto *f, int pc);
HELLOI_FUNC const char *helloG_findlocal (hello_State *L, CallInfo *ci, int n,
                                                    StkId *pos);
[[noreturn]] HELLOI_FUNC void helloG_typeerror (hello_State *L, const TValue *o,
                                                const char *opname);
[[noreturn]] HELLOI_FUNC void helloG_callerror (hello_State *L, const TValue *o);
[[noreturn]] HELLOI_FUNC void helloG_forerror (hello_State *L, const TValue *o,
                                               const char *what);
[[noreturn]] HELLOI_FUNC void helloG_concaterror (hello_State *L, const TValue *p1,
                                                  const TValue *p2);
[[noreturn]] HELLOI_FUNC void helloG_opinterror (hello_State *L, const TValue *p1,
                                                 const TValue *p2,
                                                 const char *msg);
[[noreturn]] HELLOI_FUNC void helloG_tointerror (hello_State *L, const TValue *p1,
                                                 const TValue *p2);
[[noreturn]] HELLOI_FUNC void helloG_ordererror (hello_State *L, const TValue *p1,
                                                 const TValue *p2);
[[noreturn]] HELLOI_FUNC void helloG_runerror (hello_State *L, const char *fmt, ...);
HELLOI_FUNC const char *helloG_addinfo (hello_State *L, const char *msg,
                                                  TString *src, int line);
[[noreturn]] HELLOI_FUNC void helloG_errormsg (hello_State *L);
HELLOI_FUNC int helloG_traceexec (hello_State *L, const Instruction *pc);
