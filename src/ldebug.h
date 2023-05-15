#pragma once
/*
** $Id: ldebug.h $
** Auxiliary functions from Debug Interface module
** See Copyright Notice in mask.h
*/

#include "lstate.h"


#define pcRel(pc, p)	(cast_int((pc) - (p)->code) - 1)


/* Active Mask function (given call info) */
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


MASKI_FUNC int maskG_getfuncline (const Proto *f, int pc);
MASKI_FUNC const char *maskG_findlocal (mask_State *L, CallInfo *ci, int n,
                                                    StkId *pos);
[[noreturn]] MASKI_FUNC void maskG_typeerror (mask_State *L, const TValue *o,
                                                const char *opname);
[[noreturn]] MASKI_FUNC void maskG_callerror (mask_State *L, const TValue *o);
[[noreturn]] MASKI_FUNC void maskG_forerror (mask_State *L, const TValue *o,
                                               const char *what);
[[noreturn]] MASKI_FUNC void maskG_concaterror (mask_State *L, const TValue *p1,
                                                  const TValue *p2);
[[noreturn]] MASKI_FUNC void maskG_opinterror (mask_State *L, const TValue *p1,
                                                 const TValue *p2,
                                                 const char *msg);
[[noreturn]] MASKI_FUNC void maskG_tointerror (mask_State *L, const TValue *p1,
                                                 const TValue *p2);
[[noreturn]] MASKI_FUNC void maskG_ordererror (mask_State *L, const TValue *p1,
                                                 const TValue *p2);
[[noreturn]] MASKI_FUNC void maskG_runerror (mask_State *L, const char *fmt, ...);
MASKI_FUNC const char *maskG_addinfo (mask_State *L, const char *msg,
                                                  TString *src, int line);
[[noreturn]] MASKI_FUNC void maskG_errormsg (mask_State *L);
MASKI_FUNC int maskG_traceexec (mask_State *L, const Instruction *pc);
