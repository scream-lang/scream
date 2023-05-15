#pragma once
/*
** $Id: ltm.h $
** Tag methods
** See Copyright Notice in hello.h
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


static const char *const helloT_eventname[] = {  /* ORDER TM */
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
  ((et)->flags & (1u<<(e))) ? NULL : helloT_gettm(et, e, (g)->tmname[e]))

#define fasttm(l,et,e)	gfasttm(G(l), et, e)

#define ttypename(x)	helloT_typenames_[(x) + 1]

HELLOI_DDEC(const char *const helloT_typenames_[HELLO_TOTALTYPES];)


HELLOI_FUNC const char *helloT_objtypename (hello_State *L, const TValue *o);

HELLOI_FUNC const TValue *helloT_gettm (Table *events, TMS event, TString *ename);
HELLOI_FUNC const TValue *helloT_gettmbyobj (hello_State *L, const TValue *o,
                                                       TMS event);
HELLOI_FUNC void helloT_init (hello_State *L);

HELLOI_FUNC void helloT_callTM (hello_State *L, const TValue *f, const TValue *p1,
                            const TValue *p2, const TValue *p3);
HELLOI_FUNC void helloT_callTMres (hello_State *L, const TValue *f,
                            const TValue *p1, const TValue *p2, StkId p3);
HELLOI_FUNC void helloT_trybinTM (hello_State *L, const TValue *p1, const TValue *p2,
                              StkId res, TMS event);
HELLOI_FUNC void helloT_tryconcatTM (hello_State *L);
HELLOI_FUNC void helloT_trybinassocTM (hello_State *L, const TValue *p1,
       const TValue *p2, int inv, StkId res, TMS event);
HELLOI_FUNC void helloT_trybiniTM (hello_State *L, const TValue *p1, hello_Integer i2,
                               int inv, StkId res, TMS event);
HELLOI_FUNC int helloT_callorderTM (hello_State *L, const TValue *p1,
                                const TValue *p2, TMS event);
HELLOI_FUNC int helloT_callorderiTM (hello_State *L, const TValue *p1, int v2,
                                 int inv, int isfloat, TMS event);

HELLOI_FUNC void helloT_adjustvarargs (hello_State *L, int nfixparams,
                                   struct CallInfo *ci, const Proto *p);
HELLOI_FUNC void helloT_getvarargs (hello_State *L, struct CallInfo *ci,
                                              StkId where, int wanted);
