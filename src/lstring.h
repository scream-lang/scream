#pragma once
/*
** $Id: lstring.h $
** String table (keep all strings handled by Hello)
** See Copyright Notice in hello.h
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

#define helloS_newliteral(L, s)	(helloS_newlstr(L, "" s, \
                                 (sizeof(s)/sizeof(char))-1))


/*
** test whether a string is a reserved word
*/
#define isreserved(s)	((s)->tt == HELLO_VSHRSTR && (s)->extra > 0)


/*
** equality for short strings, which are always internalized
*/
#define eqshrstr(a,b)	check_exp((a)->tt == HELLO_VSHRSTR, (a) == (b))


HELLOI_FUNC unsigned int helloS_hash (const char *str, size_t l, unsigned int seed);
HELLOI_FUNC unsigned int helloS_hashlongstr (TString *ts);
HELLOI_FUNC int helloS_eqlngstr (TString *a, TString *b);
HELLOI_FUNC void helloS_resize (hello_State *L, int newsize);
HELLOI_FUNC void helloS_clearcache (global_State *g);
HELLOI_FUNC void helloS_init (hello_State *L);
HELLOI_FUNC void helloS_remove (hello_State *L, TString *ts);
HELLOI_FUNC Udata *helloS_newudata (hello_State *L, size_t s, int nuvalue);
HELLOI_FUNC TString *helloS_newlstr (hello_State *L, const char *str, size_t l);
HELLOI_FUNC TString *helloS_new (hello_State *L, const char *str);
HELLOI_FUNC TString *helloS_createlngstrobj (hello_State *L, size_t l);
