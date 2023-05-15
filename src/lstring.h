#pragma once
/*
** $Id: lstring.h $
** String table (keep all strings handled by Mask)
** See Copyright Notice in mask.h
*/

#include "lgc.h"
#include "lobject.h"
#include "lstate.h"


/*
** Memory-allocation error message must be preallocated (it cannot
** be created after memory is exhausted)
*/
#define MEMERRMSG       "not enough memory"


/*
** Size of a TString: Size of the header plus space for the string
** itself (including final '\0').
*/
#define sizelstring(l)  (offsetof(TString, contents) + ((l) + 1) * sizeof(char))

#define maskS_newliteral(L, s)	(maskS_newlstr(L, "" s, \
                                 (sizeof(s)/sizeof(char))-1))


/*
** test whether a string is a reserved word
*/
#define isreserved(s)	((s)->tt == MASK_VSHRSTR && (s)->extra > 0)


/*
** equality for short strings, which are always internalized
*/
#define eqshrstr(a,b)	check_exp((a)->tt == MASK_VSHRSTR, (a) == (b))


MASKI_FUNC unsigned int maskS_hash (const char *str, size_t l, unsigned int seed);
MASKI_FUNC unsigned int maskS_hashlongstr (TString *ts);
MASKI_FUNC int maskS_eqlngstr (TString *a, TString *b);
MASKI_FUNC void maskS_resize (mask_State *L, int newsize);
MASKI_FUNC void maskS_clearcache (global_State *g);
MASKI_FUNC void maskS_init (mask_State *L);
MASKI_FUNC void maskS_remove (mask_State *L, TString *ts);
MASKI_FUNC Udata *maskS_newudata (mask_State *L, size_t s, int nuvalue);
MASKI_FUNC TString *maskS_newlstr (mask_State *L, const char *str, size_t l);
MASKI_FUNC TString *maskS_new (mask_State *L, const char *str);
MASKI_FUNC TString *maskS_createlngstrobj (mask_State *L, size_t l);
