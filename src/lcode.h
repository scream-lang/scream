#pragma once
/*
** $Id: lcode.h $
** Code generator for Hello
** See Copyright Notice in hello.h
*/

#include "llex.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"


/*
** Marks the end of a patch list. It is an invalid value both as an absolute
** address, and as a list link (would link an element to itself).
*/
#define NO_JUMP (-1)


/*
** grep "ORDER OPR" if you change these enums  (ORDER OP)
*/
typedef enum BinOpr {
  /* arithmetic operators */
  OPR_ADD, OPR_SUB, OPR_MUL, OPR_MOD, OPR_POW,
  OPR_DIV, OPR_IDIV,
  /* bitwise operators */
  OPR_BAND, OPR_BOR, OPR_BXOR,
  OPR_SHL, OPR_SHR,
  /* string operator */
  OPR_CONCAT,
  /* comparison operators */
  OPR_EQ, OPR_LT, OPR_LE,
  OPR_NE, OPR_GT, OPR_GE,
  /* logical operators */
  OPR_AND, OPR_OR, OPR_COAL,
  OPR_NOBINOPR,
} BinOpr;

/* true if operation is foldable (that is, it is arithmetic or bitwise) */
#define foldbinop(op)	((op) <= OPR_SHR)


#define helloK_codeABC(fs,o,a,b,c)	helloK_codeABCk(fs,o,a,b,c,0)


typedef enum UnOpr { OPR_MINUS, OPR_BNOT, OPR_NOT, OPR_LEN, OPR_NOUNOPR } UnOpr;


/* get (pointer to) instruction of given 'expdesc' */
#define getinstruction(fs,e)	((fs)->f->code[(e)->u.info])


#define helloK_setmultret(fs,e)	helloK_setreturns(fs, e, HELLO_MULTRET)

#define helloK_jumpto(fs,t)	helloK_patchlist(fs, helloK_jump(fs), t)

HELLOI_FUNC int helloK_code (FuncState *fs, Instruction i);
HELLOI_FUNC int helloK_codeABx (FuncState *fs, OpCode o, int A, unsigned int Bx);
HELLOI_FUNC int helloK_codeAsBx (FuncState *fs, OpCode o, int A, int Bx);
HELLOI_FUNC int helloK_codeABCk (FuncState *fs, OpCode o, int A,
                                            int B, int C, int k);
HELLOI_FUNC int helloK_isKint (expdesc *e);
HELLOI_FUNC int helloK_exp2const (FuncState *fs, const expdesc *e, TValue *v);
HELLOI_FUNC void helloK_fixline (FuncState *fs, int line);
HELLOI_FUNC void helloK_nil (FuncState *fs, int from, int n);
HELLOI_FUNC void helloK_reserveregs (FuncState *fs, int n);
HELLOI_FUNC void helloK_checkstack (FuncState *fs, int n);
HELLOI_FUNC void helloK_int (FuncState *fs, int reg, hello_Integer n);
HELLOI_FUNC void helloK_dischargevars (FuncState *fs, expdesc *e);
HELLOI_FUNC int helloK_exp2anyreg (FuncState *fs, expdesc *e);
HELLOI_FUNC void helloK_exp2anyregup (FuncState *fs, expdesc *e);
HELLOI_FUNC void helloK_exp2nextreg (FuncState *fs, expdesc *e);
HELLOI_FUNC void helloK_exp2val (FuncState *fs, expdesc *e);
HELLOI_FUNC int helloK_exp2RK (FuncState *fs, expdesc *e);
HELLOI_FUNC void helloK_self (FuncState *fs, expdesc *e, expdesc *key);
HELLOI_FUNC void helloK_indexed (FuncState *fs, expdesc *t, expdesc *k);
HELLOI_FUNC bool helloK_isalwaytrue (expdesc *e);
HELLOI_FUNC void helloK_goifnil (FuncState *fs, expdesc *e);
HELLOI_FUNC void helloK_goiftrue (FuncState *fs, expdesc *e);
HELLOI_FUNC void helloK_goiffalse (FuncState *fs, expdesc *e);
HELLOI_FUNC void helloK_storevar (FuncState *fs, expdesc *var, expdesc *e);
HELLOI_FUNC void helloK_setreturns (FuncState *fs, expdesc *e, int nresults);
HELLOI_FUNC void helloK_setoneret (FuncState *fs, expdesc *e);
HELLOI_FUNC int helloK_jump (FuncState *fs);
HELLOI_FUNC void helloK_ret (FuncState *fs, int first, int nret);
HELLOI_FUNC void helloK_patchlist (FuncState *fs, int list, int target);
HELLOI_FUNC void helloK_patchtohere (FuncState *fs, int list);
HELLOI_FUNC void helloK_concat (FuncState *fs, int *l1, int l2);
HELLOI_FUNC int helloK_getlabel (FuncState *fs);
HELLOI_FUNC void helloK_prefix (FuncState *fs, UnOpr op, expdesc *v, int line);
HELLOI_FUNC void helloK_infix (FuncState *fs, BinOpr op, expdesc *v);
HELLOI_FUNC void helloK_posfix (FuncState *fs, BinOpr op, expdesc *v1,
                            expdesc *v2, int line);
HELLOI_FUNC void helloK_settablesize (FuncState *fs, int pc,
                                  int ra, int asize, int hsize);
HELLOI_FUNC void helloK_setlist (FuncState *fs, int base, int nelems, int tostore);
HELLOI_FUNC void helloK_finish (FuncState *fs);
[[noreturn]] HELLOI_FUNC void helloK_semerror (LexState *ls, const char *msg);
HELLOI_FUNC void helloK_exp2reg (FuncState *fs, expdesc *e, int reg);
