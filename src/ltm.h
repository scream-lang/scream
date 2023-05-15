#pragma once
/*
** $Id: ltm.h $
** Tag methods
** See Copyright Notice in mask.h
*/

#include "lobject.h"


/*
* WARNING: if you change the order of this enumeration,
* grep "ORDER TM" and "ORDER OP"
*/
typedef enum {
  TM_INDEX,
  TM_NEWINDEX,
  TM_GC,
  TM_MODE,
  TM_LEN,
  TM_EQ,  /* last tag method with fast access */
  TM_ADD,
  TM_SUB,
  TM_MUL,
  TM_MOD,
  TM_POW,
  TM_DIV,
  TM_IDIV,
  TM_BAND,
  TM_BOR,
  TM_BXOR,
  TM_SHL,
  TM_SHR,
  TM_UNM,
  TM_BNOT,
  TM_LT,
  TM_LE,
  TM_CONCAT,
  TM_CALL,
  TM_CLOSE,
  TM_N		/* number of elements in the enum */
} TMS;


static const char *const maskT_eventname[] = {  /* ORDER TM */
  "__index", "__newindex",
  "__gc", "__mode", "__len", "__eq",
  "__add", "__sub", "__mul", "__mod", "__pow",
  "__div", "__idiv",
  "__band", "__bor", "__bxor", "__shl", "__shr",
  "__unm", "__bnot", "__lt", "__le",
  "__concat", "__call", "__close"
};


/*
** Mask with 1 in all fast-access methods. A 1 in any of these bits
** in the flag of a (meta)table means the metatable does not have the
** corresponding metamethod field. (Bit 7 of the flag is used for
** 'isrealasize'.)
*/
#define maskflags	(~(~0u << (TM_EQ + 1)))


/*
** Test whether there is no tagmethod.
** (Because tagmethods use raw accesses, the result may be an "empty" nil.)
*/
#define notm(tm)	ttisnil(tm)


#define gfasttm(g,et,e) ((et) == NULL ? NULL : \
  ((et)->flags & (1u<<(e))) ? NULL : maskT_gettm(et, e, (g)->tmname[e]))

#define fasttm(l,et,e)	gfasttm(G(l), et, e)

#define ttypename(x)	maskT_typenames_[(x) + 1]

MASKI_DDEC(const char *const maskT_typenames_[MASK_TOTALTYPES];)


MASKI_FUNC const char *maskT_objtypename (mask_State *L, const TValue *o);

MASKI_FUNC const TValue *maskT_gettm (Table *events, TMS event, TString *ename);
MASKI_FUNC const TValue *maskT_gettmbyobj (mask_State *L, const TValue *o,
                                                       TMS event);
MASKI_FUNC void maskT_init (mask_State *L);

MASKI_FUNC void maskT_callTM (mask_State *L, const TValue *f, const TValue *p1,
                            const TValue *p2, const TValue *p3);
MASKI_FUNC void maskT_callTMres (mask_State *L, const TValue *f,
                            const TValue *p1, const TValue *p2, StkId p3);
MASKI_FUNC void maskT_trybinTM (mask_State *L, const TValue *p1, const TValue *p2,
                              StkId res, TMS event);
MASKI_FUNC void maskT_tryconcatTM (mask_State *L);
MASKI_FUNC void maskT_trybinassocTM (mask_State *L, const TValue *p1,
       const TValue *p2, int inv, StkId res, TMS event);
MASKI_FUNC void maskT_trybiniTM (mask_State *L, const TValue *p1, mask_Integer i2,
                               int inv, StkId res, TMS event);
MASKI_FUNC int maskT_callorderTM (mask_State *L, const TValue *p1,
                                const TValue *p2, TMS event);
MASKI_FUNC int maskT_callorderiTM (mask_State *L, const TValue *p1, int v2,
                                 int inv, int isfloat, TMS event);

MASKI_FUNC void maskT_adjustvarargs (mask_State *L, int nfixparams,
                                   struct CallInfo *ci, const Proto *p);
MASKI_FUNC void maskT_getvarargs (mask_State *L, struct CallInfo *ci,
                                              StkId where, int wanted);
