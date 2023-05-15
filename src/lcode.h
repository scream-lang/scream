#pragma once
/*
** $Id: lcode.h $
** Code generator for Mask
** See Copyright Notice in mask.h
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


#define maskK_codeABC(fs,o,a,b,c)	maskK_codeABCk(fs,o,a,b,c,0)


typedef enum UnOpr { OPR_MINUS, OPR_BNOT, OPR_NOT, OPR_LEN, OPR_NOUNOPR } UnOpr;


/* get (pointer to) instruction of given 'expdesc' */
#define getinstruction(fs,e)	((fs)->f->code[(e)->u.info])


#define maskK_setmultret(fs,e)	maskK_setreturns(fs, e, MASK_MULTRET)

#define maskK_jumpto(fs,t)	maskK_patchlist(fs, maskK_jump(fs), t)

MASKI_FUNC int maskK_code (FuncState *fs, Instruction i);
MASKI_FUNC int maskK_codeABx (FuncState *fs, OpCode o, int A, unsigned int Bx);
MASKI_FUNC int maskK_codeAsBx (FuncState *fs, OpCode o, int A, int Bx);
MASKI_FUNC int maskK_codeABCk (FuncState *fs, OpCode o, int A,
                                            int B, int C, int k);
MASKI_FUNC int maskK_isKint (expdesc *e);
MASKI_FUNC int maskK_exp2const (FuncState *fs, const expdesc *e, TValue *v);
MASKI_FUNC void maskK_fixline (FuncState *fs, int line);
MASKI_FUNC void maskK_nil (FuncState *fs, int from, int n);
MASKI_FUNC void maskK_reserveregs (FuncState *fs, int n);
MASKI_FUNC void maskK_checkstack (FuncState *fs, int n);
MASKI_FUNC void maskK_int (FuncState *fs, int reg, mask_Integer n);
MASKI_FUNC void maskK_dischargevars (FuncState *fs, expdesc *e);
MASKI_FUNC int maskK_exp2anyreg (FuncState *fs, expdesc *e);
MASKI_FUNC void maskK_exp2anyregup (FuncState *fs, expdesc *e);
MASKI_FUNC void maskK_exp2nextreg (FuncState *fs, expdesc *e);
MASKI_FUNC void maskK_exp2val (FuncState *fs, expdesc *e);
MASKI_FUNC int maskK_exp2RK (FuncState *fs, expdesc *e);
MASKI_FUNC void maskK_self (FuncState *fs, expdesc *e, expdesc *key);
MASKI_FUNC void maskK_indexed (FuncState *fs, expdesc *t, expdesc *k);
MASKI_FUNC bool maskK_isalwaytrue (expdesc *e);
MASKI_FUNC void maskK_goifnil (FuncState *fs, expdesc *e);
MASKI_FUNC void maskK_goiftrue (FuncState *fs, expdesc *e);
MASKI_FUNC void maskK_goiffalse (FuncState *fs, expdesc *e);
MASKI_FUNC void maskK_storevar (FuncState *fs, expdesc *var, expdesc *e);
MASKI_FUNC void maskK_setreturns (FuncState *fs, expdesc *e, int nresults);
MASKI_FUNC void maskK_setoneret (FuncState *fs, expdesc *e);
MASKI_FUNC int maskK_jump (FuncState *fs);
MASKI_FUNC void maskK_ret (FuncState *fs, int first, int nret);
MASKI_FUNC void maskK_patchlist (FuncState *fs, int list, int target);
MASKI_FUNC void maskK_patchtohere (FuncState *fs, int list);
MASKI_FUNC void maskK_concat (FuncState *fs, int *l1, int l2);
MASKI_FUNC int maskK_getlabel (FuncState *fs);
MASKI_FUNC void maskK_prefix (FuncState *fs, UnOpr op, expdesc *v, int line);
MASKI_FUNC void maskK_infix (FuncState *fs, BinOpr op, expdesc *v);
MASKI_FUNC void maskK_posfix (FuncState *fs, BinOpr op, expdesc *v1,
                            expdesc *v2, int line);
MASKI_FUNC void maskK_settablesize (FuncState *fs, int pc,
                                  int ra, int asize, int hsize);
MASKI_FUNC void maskK_setlist (FuncState *fs, int base, int nelems, int tostore);
MASKI_FUNC void maskK_finish (FuncState *fs);
[[noreturn]] MASKI_FUNC void maskK_semerror (LexState *ls, const char *msg);
MASKI_FUNC void maskK_exp2reg (FuncState *fs, expdesc *e, int reg);
