/*
** $Id: lparser.c $
** Hello Parser
** See Copyright Notice in hello.h
*/

#define lparser_c
#define HELLO_CORE

#include "lprefix.h"


#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include <string>
#include <vector>

#include "hello.h"
#include "lcode.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "llex.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "lauxlib.h"

#include "ErrorMessage.hpp"



#define hasmultret(k)		((k) == VCALL || (k) == VVARARG)


/*
** Invokes the hello_writestring macro with a std::string.
*/
#define write_std_string(std_string) hello_writestring(std_string.data(), std_string.size())


/* because all strings are unified by the scanner, the parser
   can use pointer equality for string equality */
#define eqstr(a,b)	((a) == (b))


#define helloO_fmt helloO_pushfstring


/*
** nodes for block list (list of active blocks)
*/
typedef struct BlockCnt {
  struct BlockCnt *previous;  /* chain */
  int breaklist;  /* list of jumps out of this loop */
  int scopeend;  /* delimits the end of this scope, for 'continue' to jump before. */
  int firstlabel;  /* index of first label in this block */
  int firstgoto;  /* index of first pending goto in this block */
  int nactvar;  /* # active locals outside the block */
  lu_byte upval;  /* true if some variable in the block is an upvalue */
  lu_byte isloop;  /* true if 'block' is a loop */
  lu_byte insidetbc;  /* true if inside the scope of a to-be-closed var. */
} BlockCnt;



/*
** prototypes for recursive non-terminal functions
*/
static void statement (LexState *ls, TypeDesc *prop = nullptr);
static void expr (LexState *ls, expdesc *v, TypeDesc *prop = nullptr, bool no_colon = false);


/*
** Throws an exception into Hello, which will promptly close the program.
** This is only called for vital errors, like lexer and/or syntax problems.
*/
[[noreturn]] static void throwerr (LexState *ls, const char *err, const char *here, int line) {
  err = helloG_addinfo(ls->L, err, ls->source, line);
  Hello::ErrorMessage msg{ ls, HRED "syntax error: " BWHT }; // We'll only throw syntax errors if 'throwerr' is called
  msg.addMsg(err)
    .addSrcLine(line)
    .addGenericHere(here)
    .finalizeAndThrow();
}

[[noreturn]] static void throwerr (LexState *ls, const char *err, const char *here) {
  throwerr(ls, err, here, ls->getLineNumber());
}


#ifndef HELLO_NO_PARSER_WARNINGS
// No note.
static void throw_warn (LexState *ls, const char *raw_err, const char *here, int line, WarningType warningType) {
  std::string err(raw_err);
  if (ls->shouldEmitWarning(line, warningType)) {
    Hello::ErrorMessage msg{ ls, helloG_addinfo(ls->L, YEL "warning: " BWHT, ls->source, line) };
    err.append(" [");
    err.append(ls->getWarningConfig().getWarningName(warningType));
    err.push_back(']');
    msg.addMsg(err)
      .addSrcLine(line)
      .addGenericHere(here)
      .finalize();
    hello_warning(ls->L, msg.content.c_str(), 0);
    ls->L->top -= 2; // Hello::Error::finalize & helloG_addinfo
  }
}

// Note.
static void throw_warn(LexState* ls, const char* raw_err, const char* here, const char* note, int line, WarningType warningType) {
  std::string err(raw_err);
  if (ls->shouldEmitWarning(line, warningType)) {
    Hello::ErrorMessage msg{ ls, helloG_addinfo(ls->L, YEL "warning: " BWHT, ls->source, line) };
    err.append(" [");
    err.append(ls->getWarningConfig().getWarningName(warningType));
    err.push_back(']');
    msg.addMsg(err)
      .addSrcLine(line)
      .addGenericHere(here)
      .addNote(note)
      .finalize();
    hello_warning(ls->L, msg.content.c_str(), 0);
    ls->L->top -= 2; // Hello::Error::finalize & helloG_addinfo
  }
}

static void throw_warn(LexState *ls, const char* err, const char *here, WarningType warningType) {
  return throw_warn(ls, err, here, ls->getLineNumber(), warningType);
}

// TO-DO: Warning suppression attribute support for this overload. Don't know where it's used atm.
static void throw_warn(LexState *ls, const char *err, int line, WarningType warningType) {
  if (ls->shouldEmitWarning(line, warningType)) {
    auto msg = helloG_addinfo(ls->L, err, ls->source, line);
    hello_warning(ls->L, msg, 0);
    ls->L->top -= 1; /* remove warning from stack */
  }
}
#endif


/*
** This function will throw an exception and terminate the program.
*/
[[noreturn]] static void error_expected (LexState *ls, int token) {
  switch (token) {
    case '|': {
      throwerr(ls,
        "expected '|' to control parameters.",
        "expected '|' to begin & terminate the lambda's paramater list.");
    }
    case '-': {
      if (helloX_lookahead(ls) == '>') {
        throwerr(ls,
          "impromper lambda definition",
          "expected '->' arrow syntax for lambda expression.");
      }
      goto _default; // Run-through default case, no more work to be done.
    }
    case TK_IN: {
      throwerr(ls,
        "expected 'in' to delimit loop iterator.", "expected 'in' symbol.");
    }
    case TK_DO: {
      throwerr(ls,
        "expected 'do' to establish block.", "you need to append this with the 'do' symbol.");
    }
    case TK_END: {
      throwerr(ls,
        "expected 'end' to terminate block.", "expected 'end' symbol after or on this line.");
    }
    case TK_THEN: {
      throwerr(ls,
        "expected 'then' to delimit condition.", "expected 'then' symbol.");
    }
    case TK_NAME: {
      throwerr(ls,
        "expected an identifier.", "this needs a name.");
    }
    case TK_PCONTINUE: {
      throwerr(ls,
        "expected 'continue' inside a loop.", "this is not within a loop.");
    }
    default: {
      _default:
      throwerr(ls,
        helloO_fmt(ls->L, "%s expected (got %s)",
          helloX_token2str(ls, token), helloX_token2str(ls, ls->t.token)), "this is invalid syntax.");
    }
  }
}


[[noreturn]] static void errorlimit (FuncState *fs, int limit, const char *what) {
  hello_State *L = fs->ls->L;
  const char *msg;
  int line = fs->f->linedefined;
  const char *where = (line == 0)
                      ? "main function"
                      : helloO_pushfstring(L, "function at line %d", line);
  msg = helloO_pushfstring(L, "too many %s (limit is %d) in %s",
                             what, limit, where);
  helloX_syntaxerror(fs->ls, msg);
}


static void checklimit (FuncState *fs, int v, int l, const char *what) {
  if (v > l) errorlimit(fs, l, what);
}


/*
** Test whether next token is 'c'; if so, skip it.
*/
static int testnext (LexState *ls, int c) {
  if (ls->t.token == c) {
    helloX_next(ls);
    return 1;
  }
  else return 0;
}


/*
** Check that next token is 'c'.
*/
static void check (LexState *ls, int c) {
  if (ls->t.token != c) {
    error_expected(ls, c);
  }
}


/*
** Check that next token is 'c' and skip it.
*/
static void checknext (LexState *ls, int c) {
  check(ls, c);
  helloX_next(ls);
}


#define check_condition(ls,c,msg)	{ if (!(c)) helloX_syntaxerror(ls, msg); }


/*
** Check that next token is 'what' and skip it. In case of error,
** raise an error that the expected 'what' should match a 'who'
** in line 'where' (if that is not the current line).
*/
static void check_match (LexState *ls, int what, int who, int where) {
  if (l_unlikely(!testnext(ls, what))) {
    if (where == ls->getLineNumber())  /* all in the same line? */
      error_expected(ls, what);  /* do not need a complex message */
    else {
      if (what == TK_END) {
        std::string msg = "missing 'end' to terminate ";
        msg.append(helloX_token2str(ls, who));
        if (who != TK_BEGIN) {
          msg.append(" block");
        }
        msg.append(" on line ");
        msg.append(std::to_string(where));
        throwerr(ls, msg.c_str(), "this was the last statement.", ls->getLineNumberOfLastNonEmptyLine());
      }
      else {
        Hello::ErrorMessage err{ ls, RED "syntax error: " BWHT }; // Doesn't use throwerr since I replicated old code. Couldn't find problematic code to repro error, so went safe.
        err.addMsg(helloX_token2str(ls, what))
          .addMsg(" expected (to close ")
          .addMsg(helloX_token2str(ls, who))
          .addMsg(" on line ")
          .addMsg(std::to_string(where))
          .addMsg(")")
          .addSrcLine(ls->getLineNumberOfLastNonEmptyLine())
          .addGenericHere()
          .finalizeAndThrow();
      }
    }
  }
}


[[nodiscard]] static bool isnametkn(LexState *ls, bool strict = false) {
  return ls->t.token == TK_NAME || ls->t.IsNarrow() || (!strict && ls->t.IsReservedNonValue());
}


static TString *str_checkname (LexState *ls, bool strict = false) {
  TString *ts;
  if (!isnametkn(ls, strict)) {
    error_expected(ls, TK_NAME);
  }
  ts = ls->t.seminfo.ts;
  helloX_next(ls);
  return ts;
}


static void init_exp (expdesc *e, expkind k, int i) {
  e->f = e->t = NO_JUMP;
  e->k = k;
  e->u.info = i;
}


static void codestring (expdesc *e, TString *s) {
  e->f = e->t = NO_JUMP;
  e->k = VKSTR;
  e->u.strval = s;
}


static void codename (LexState *ls, expdesc *e) {
  codestring(e, str_checkname(ls));
}


/*
** Register a new local variable in the active 'Proto' (for debug
** information).
*/
static int registerlocalvar (LexState *ls, FuncState *fs, TString *varname) {
  Proto *f = fs->f;
  int oldsize = f->sizelocvars;
  helloM_growvector(ls->L, f->locvars, fs->ndebugvars, f->sizelocvars,
                  LocVar, SHRT_MAX, "local variables");
  while (oldsize < f->sizelocvars)
    f->locvars[oldsize++].varname = NULL;
  f->locvars[fs->ndebugvars].varname = varname;
  f->locvars[fs->ndebugvars].startpc = fs->pc;
  helloC_objbarrier(ls->L, f, varname);
  return fs->ndebugvars++;
}


#define new_localvarliteral(ls,v) \
    new_localvar(ls,  \
      helloX_newstring(ls, "" v, (sizeof(v)/sizeof(char)) - 1));


[[nodiscard]] static TypeDesc gettypehint(LexState *ls) noexcept {
  /* TYPEHINT -> [':' Typedesc] */
  if (testnext(ls, ':')) {
    const bool nullable = testnext(ls, '?');
    const char* tname = getstr(str_checkname(ls));
    if (strcmp(tname, "number") == 0)
      return { VT_INT, nullable };
    else if (strcmp(tname, "table") == 0)
      return { VT_TABLE, nullable };
    else if (strcmp(tname, "string") == 0)
      return { VT_STR, nullable };
    else if (strcmp(tname, "boolean") == 0 || strcmp(tname, "bool") == 0)
      return { VT_BOOL, nullable };
    else if (strcmp(tname, "function") == 0)
      return { VT_FUNC, nullable };
    else if (strcmp(tname, "userdata") != 0) {
      helloX_prev(ls);
      throw_warn(ls, "unknown type hint", "the type hinted here is unknown to the parser.", TYPE_MISMATCH);
      helloX_next(ls); // Preserve a6c8e359857644f4311c022f85cf19d85d95c25d
    }
  }
  return VT_DUNNO;
}


static void exp_propagate(LexState* ls, const expdesc& e, TypeDesc& t) noexcept {
  if (e.k == VLOCAL) {
    t = getlocalvardesc(ls->fs, e.u.var.vidx)->vd.prop;
  }
  else if (e.k == VCONST) {
    TValue* val = &ls->dyd->actvar.arr[e.u.info].k;
    switch (ttype(val))
    {
    case HELLO_TNIL: t = VT_NIL; break;
    case HELLO_TBOOLEAN: t = VT_BOOL; break;
    case HELLO_TNUMBER: t = ((ttypetag(val) == HELLO_VNUMINT) ? VT_INT : VT_FLT); break;
    case HELLO_TSTRING: t = VT_STR; break;
    case HELLO_TTABLE: t = VT_TABLE; break;
    case HELLO_TFUNCTION: t = VT_FUNC; break;
    }
  }
}


static void process_assign(LexState* ls, Vardesc* var, const TypeDesc& td, int line) {
#ifndef HELLO_NO_PARSER_WARNINGS
  auto hinted = var->vd.hint.getType() != VT_DUNNO;
  auto knownvalue = td.getType() != VT_DUNNO;
  auto incompatible = !var->vd.hint.isCompatibleWith(td);
  if (hinted && knownvalue && incompatible) {
    const auto hint = var->vd.hint.toString();
    std::string err = var->vd.name->toCpp();
    err.insert(0, "'");
    err.append("' type-hinted as '" + hint);
    err.append("', but assigned a ");
    err.append(td.toString());
    err.append(" value.");
    if (td.getType() == VT_NIL) {  /* Specialize warnings for nullable state incompatibility. */
      throw_warn(ls, "variable type mismatch", err.c_str(), helloO_fmt(ls->L, "try a nilable type hint: '?%s'", hint.c_str()), line, TYPE_MISMATCH);
      ls->L->top--; // helloO_fmt
    }
    else {  /* Throw a generic mismatch warning. */
      throw_warn(ls, "variable type mismatch", err.c_str(), line, TYPE_MISMATCH);
    }
  }
#endif
  var->vd.prop = td; /* propagate type */
}


/*
** Convert 'nvar', a compiler index level, to its corresponding
** register. For that, search for the highest variable below that level
** that is in a register and uses its register index ('ridx') plus one.
*/
static int reglevel (FuncState *fs, int nvar) {
  while (nvar-- > 0) {
    Vardesc *vd = getlocalvardesc(fs, nvar);  /* get previous variable */
    if (vd->vd.kind != RDKCTC)  /* is in a register? */
      return vd->vd.ridx + 1;
  }
  return 0;  /* no variables in registers */
}


/*
** Return the number of variables in the register stack for the given
** function.
*/
int helloY_nvarstack (FuncState *fs) {
  return reglevel(fs, fs->nactvar);
}


/*
** Get the debug-information entry for current variable 'vidx'.
*/
static LocVar *localdebuginfo (FuncState *fs, int vidx) {
  Vardesc *vd = getlocalvardesc(fs, vidx);
  if (vd->vd.kind == RDKCTC)
    return NULL;  /* no debug info. for constants */
  else {
    int idx = vd->vd.pidx;
    hello_assert(idx < fs->ndebugvars);
    return &fs->f->locvars[idx];
  }
}


/*
** Create a new local variable with the given 'name'. Return its index
** in the function.
*/
static int new_localvar (LexState *ls, TString *name, int line, const TypeDesc& hint = VT_DUNNO) {
  hello_State *L = ls->L;
  FuncState *fs = ls->fs;
  Dyndata *dyd = ls->dyd;
  Vardesc *var;
#ifndef HELLO_NO_PARSER_WARNINGS
  int locals = helloY_nvarstack(fs);
  for (int i = fs->firstlocal; i < locals; i++) {
    Vardesc *desc = getlocalvardesc(fs, i);
    LocVar *local = localdebuginfo(fs, i);
    std::string n = name->toCpp();
    if ((n != "(for state)" && n != "(switch control value)") && (local && local->varname == name)) { // Got a match.
      throw_warn(ls,
        "duplicate local declaration",
          helloO_fmt(L, "this shadows the initial declaration of '%s' on line %d.", name->contents, desc->vd.line), line, VAR_SHADOW);
      L->top--; /* pop result of helloO_fmt */
      break;
    }
  }
#endif
  helloM_growvector(L, dyd->actvar.arr, dyd->actvar.n + 1,
                  dyd->actvar.size, Vardesc, USHRT_MAX, "local variables");
  var = &dyd->actvar.arr[dyd->actvar.n++];
  var->vd.kind = VDKREG;  /* default */
  var->vd.hint = hint;
  var->vd.prop = VT_DUNNO;
  var->vd.name = name;
  var->vd.line = line;
  return dyd->actvar.n - 1 - fs->firstlocal;
}

static int new_localvar (LexState *ls, TString *name, const TypeDesc& hint = VT_DUNNO) {
  return new_localvar(ls, name, ls->getLineNumber(), hint);
}


/*
** Create an expression representing variable 'vidx'
*/
static void init_var (FuncState *fs, expdesc *e, int vidx) {
  e->f = e->t = NO_JUMP;
  e->k = VLOCAL;
  e->u.var.vidx = vidx;
  e->u.var.ridx = getlocalvardesc(fs, vidx)->vd.ridx;
}


/*
** Raises an error if variable described by 'e' is read only
*/
static void check_readonly (LexState *ls, expdesc *e) {
  FuncState *fs = ls->fs;
  TString *varname = NULL;  /* to be set if variable is const */
  switch (e->k) {
    case VCONST: {
      varname = ls->dyd->actvar.arr[e->u.info].vd.name;
      break;
    }
    case VLOCAL: {
      Vardesc *vardesc = getlocalvardesc(fs, e->u.var.vidx);
      if (vardesc->vd.kind != VDKREG)  /* not a regular variable? */
        varname = vardesc->vd.name;
      break;
    }
    case VUPVAL: {
      Upvaldesc *up = &fs->f->upvalues[e->u.info];
      if (up->kind != VDKREG)
        varname = up->name;
      break;
    }
    default:
      return;  /* other cases cannot be read-only */
  }
  if (varname) {
    const char *msg = helloO_fmt(ls->L, "attempt to reassign constant '%s'", getstr(varname));
    const char *here = "this variable is constant, and cannot be reassigned.";
    throwerr(ls, helloO_fmt(ls->L, msg, getstr(varname)), here);
  }
}


/*
** Start the scope for the last 'nvars' created variables.
*/
static void adjustlocalvars (LexState *ls, int nvars) {
  FuncState *fs = ls->fs;
  int reglevel = helloY_nvarstack(fs);
  int i;
  for (i = 0; i < nvars; i++) {
    int vidx = fs->nactvar++;
    Vardesc *var = getlocalvardesc(fs, vidx);
    var->vd.ridx = reglevel++;
    var->vd.pidx = registerlocalvar(ls, fs, var->vd.name);
  }
}


/*
** Close the scope for all variables up to level 'tolevel'.
** (debug info.)
*/
static void removevars (FuncState *fs, int tolevel) {
  fs->ls->dyd->actvar.n -= (fs->nactvar - tolevel);
  while (fs->nactvar > tolevel) {
    LocVar *var = localdebuginfo(fs, --fs->nactvar);
    if (var)  /* does it have debug information? */
      var->endpc = fs->pc;
  }
}


/*
** Search the upvalues of the function 'fs' for one
** with the given 'name'.
*/
static int searchupvalue (FuncState *fs, TString *name) {
  int i;
  Upvaldesc *up = fs->f->upvalues;
  for (i = 0; i < fs->nups; i++) {
    if (eqstr(up[i].name, name)) return i;
  }
  return -1;  /* not found */
}


static Upvaldesc *allocupvalue (FuncState *fs) {
  Proto *f = fs->f;
  int oldsize = f->sizeupvalues;
  checklimit(fs, fs->nups + 1, MAXUPVAL, "upvalues");
  helloM_growvector(fs->ls->L, f->upvalues, fs->nups, f->sizeupvalues,
                  Upvaldesc, MAXUPVAL, "upvalues");
  while (oldsize < f->sizeupvalues)
    f->upvalues[oldsize++].name = NULL;
  return &f->upvalues[fs->nups++];
}


static int newupvalue (FuncState *fs, TString *name, expdesc *v) {
  Upvaldesc *up = allocupvalue(fs);
  FuncState *prev = fs->prev;
  if (v->k == VLOCAL) {
    up->instack = 1;
    up->idx = v->u.var.ridx;
    up->kind = getlocalvardesc(prev, v->u.var.vidx)->vd.kind;
    hello_assert(eqstr(name, getlocalvardesc(prev, v->u.var.vidx)->vd.name));
  }
  else {
    up->instack = 0;
    up->idx = cast_byte(v->u.info);
    up->kind = prev->f->upvalues[v->u.info].kind;
    hello_assert(eqstr(name, prev->f->upvalues[v->u.info].name));
  }
  up->name = name;
  helloC_objbarrier(fs->ls->L, fs->f, name);
  return fs->nups - 1;
}


/*
** Look for an active local variable with the name 'n' in the
** function 'fs'. If found, initialize 'var' with it and return
** its expression kind; otherwise return -1.
*/
static int searchvar (FuncState *fs, TString *n, expdesc *var) {
  int i;
  for (i = fs->nactvar - 1; i >= 0; i--) {
    Vardesc *vd = getlocalvardesc(fs, i);
    if (eqstr(n, vd->vd.name)) {  /* found? */
      if (vd->vd.kind == RDKCTC)  /* compile-time constant? */
        init_exp(var, VCONST, fs->firstlocal + i);
      else  /* real variable */
        init_var(fs, var, i);
      return var->k;
    }
  }
  return -1;  /* not found */
}


/*
** Mark block where variable at given level was defined
** (to emit close instructions later).
*/
static void markupval (FuncState *fs, int level) {
  BlockCnt *bl = fs->bl;
  while (bl->nactvar > level)
    bl = bl->previous;
  bl->upval = 1;
  fs->needclose = 1;
}


/*
** Mark that current block has a to-be-closed variable.
*/
static void marktobeclosed (FuncState *fs) {
  BlockCnt *bl = fs->bl;
  bl->upval = 1;
  bl->insidetbc = 1;
  fs->needclose = 1;
}


/*
** Find a variable with the given name 'n'. If it is an upvalue, add
** this upvalue into all intermediate functions. If it is a global, set
** 'var' as 'void' as a flag.
*/
static void singlevaraux (FuncState *fs, TString *n, expdesc *var, int base) {
  if (fs == NULL)  /* no more levels? */
    init_exp(var, VVOID, 0);  /* default is global */
  else {
    int v = searchvar(fs, n, var);  /* look up locals at current level */
    if (v >= 0) {  /* found? */
      if (v == VLOCAL && !base)
        markupval(fs, var->u.var.vidx);  /* local will be used as an upval */
    }
    else {  /* not found as local at current level; try upvalues */
      int idx = searchupvalue(fs, n);  /* try existing upvalues */
      if (idx < 0) {  /* not found? */
        singlevaraux(fs->prev, n, var, 0);  /* try upper levels */
        if (var->k == VLOCAL || var->k == VUPVAL)  /* local or upvalue? */
          idx  = newupvalue(fs, n, var);  /* will be a new upvalue */
        else  /* it is a global or a constant */
          return;  /* don't need to do anything at this level */
      }
      init_exp(var, VUPVAL, idx);  /* new or old upvalue */
    }
  }
}


inline int gett(LexState *ls) {
  return ls->t.token;
}


/*
** Adjust the number of results from an expression list 'e' with 'nexps'
** expressions to 'nvars' values.
*/
static void adjust_assign (LexState *ls, int nvars, int nexps, expdesc *e) {
  FuncState *fs = ls->fs;
  int needed = nvars - nexps;  /* extra values needed */
  if (hasmultret(e->k)) {  /* last expression has multiple returns? */
    int extra = needed + 1;  /* discount last expression itself */
    if (extra < 0)
      extra = 0;
    helloK_setreturns(fs, e, extra);  /* last exp. provides the difference */
  }
  else {
    if (e->k != VVOID)  /* at least one expression? */
      helloK_exp2nextreg(fs, e);  /* close last expression */
    if (needed > 0)  /* missing values? */
      helloK_nil(fs, fs->freereg, needed);  /* complete with nils */
  }
  if (needed > 0)
    helloK_reserveregs(fs, needed);  /* registers for extra values */
  else  /* adding 'needed' is actually a subtraction */
    fs->freereg += needed;  /* remove extra values */
}


/*
** Find a variable with the given name 'n', handling global variables
** too.
*/
static void singlevarinner (LexState *ls, TString *varname, expdesc *var) {
  FuncState *fs = ls->fs;
  singlevaraux(fs, varname, var, 1);
  if (var->k == VVOID) {  /* global name? */
    expdesc key;
    singlevaraux(fs, ls->envn, var, 1);  /* get environment variable */
    hello_assert(var->k != VVOID);  /* this one must exist */
    codestring(&key, varname);  /* key is variable name */
    helloK_indexed(fs, var, &key);  /* env[varname] */
  }
}

static void singlevar (LexState *ls, expdesc *var) {
  TString *varname = str_checkname(ls);
  if (gett(ls) == TK_WALRUS) {
    helloX_next(ls);
    if (ls->getContext() == PARCTX_CREATE_VARS)
      throwerr(ls, "unexpected ':=' while creating multiple variable", "unexpected ':='");
    if (ls->getContext() == PARCTX_FUNCARGS)
      throwerr(ls, "unexpected ':=' while processing function arguments", "unexpected ':='");
    new_localvar(ls, varname);
    expr(ls, var);
    adjust_assign(ls, 1, 1, var);
    adjustlocalvars(ls, 1);
    return;
  }
  singlevarinner(ls, varname, var);
}


#define enterlevel(ls)	helloE_incCstack(ls->L)


#define leavelevel(ls) ((ls)->L->nCcalls--)


/*
** Generates an error that a goto jumps into the scope of some
** local variable.
*/
[[noreturn]] static void jumpscopeerror (LexState *ls, Labeldesc *gt) {
  const char *varname = getstr(getlocalvardesc(ls->fs, gt->nactvar)->vd.name);
  const char *msg = "<goto %s> at line %d jumps into the scope of local '%s'";
  msg = helloO_pushfstring(ls->L, msg, getstr(gt->name), gt->line, varname);
  helloK_semerror(ls, msg);  /* raise the error */
}


/*
** Solves the goto at index 'g' to given 'label' and removes it
** from the list of pending goto's.
** If it jumps into the scope of some variable, raises an error.
*/
static void solvegoto (LexState *ls, int g, Labeldesc *label) {
  int i;
  Labellist *gl = &ls->dyd->gt;  /* list of goto's */
  Labeldesc *gt = &gl->arr[g];  /* goto to be resolved */
  hello_assert(eqstr(gt->name, label->name));
  if (l_unlikely(gt->nactvar < label->nactvar))  /* enter some scope? */
    jumpscopeerror(ls, gt);
  helloK_patchlist(ls->fs, gt->pc, label->pc);
  for (i = g; i < gl->n - 1; i++)  /* remove goto from pending list */
    gl->arr[i] = gl->arr[i + 1];
  gl->n--;
}


/*
** Search for an active label with the given name.
*/
static Labeldesc *findlabel (LexState *ls, TString *name) {
  int i;
  Dyndata *dyd = ls->dyd;
  /* check labels in current function for a match */
  for (i = ls->fs->firstlabel; i < dyd->label.n; i++) {
    Labeldesc *lb = &dyd->label.arr[i];
    if (eqstr(lb->name, name))  /* correct label? */
      return lb;
  }
  return NULL;  /* label not found */
}


/*
** Adds a new label/goto in the corresponding list.
*/
static int newlabelentry (LexState *ls, Labellist *l, TString *name,
                          int line, int pc) {
  int n = l->n;
  helloM_growvector(ls->L, l->arr, n, l->size,
                  Labeldesc, SHRT_MAX, "labels/gotos");
  l->arr[n].name = name;
  l->arr[n].line = line;
  l->arr[n].nactvar = ls->fs->nactvar;
  l->arr[n].close = 0;
  l->arr[n].pc = pc;
  l->n = n + 1;
  return n;
}


static int newgotoentry (LexState *ls, TString *name, int line, int pc) {
  return newlabelentry(ls, &ls->dyd->gt, name, line, pc);
}


/*
** Solves forward jumps. Check whether new label 'lb' matches any
** pending gotos in current block and solves them. Return true
** if any of the goto's need to close upvalues.
*/
static int solvegotos (LexState *ls, Labeldesc *lb) {
  Labellist *gl = &ls->dyd->gt;
  int i = ls->fs->bl->firstgoto;
  int needsclose = 0;
  while (i < gl->n) {
    if (eqstr(gl->arr[i].name, lb->name)) {
      needsclose |= gl->arr[i].close;
      solvegoto(ls, i, lb);  /* will remove 'i' from the list */
    }
    else
      i++;
  }
  return needsclose;
}


/*
** Create a new label with the given 'name' at the given 'line'.
** 'last' tells whether label is the last non-op statement in its
** block. Solves all pending goto's to this new label and adds
** a close instruction if necessary.
** Returns true iff it added a close instruction.
*/
static int createlabel (LexState *ls, TString *name, int line,
                        int last) {
  FuncState *fs = ls->fs;
  Labellist *ll = &ls->dyd->label;
  int l = newlabelentry(ls, ll, name, line, helloK_getlabel(fs));
  if (last) {  /* label is last no-op statement in the block? */
    /* assume that locals are already out of scope */
    ll->arr[l].nactvar = fs->bl->nactvar;
  }
  if (solvegotos(ls, &ll->arr[l])) {  /* need close? */
    helloK_codeABC(fs, OP_CLOSE, helloY_nvarstack(fs), 0, 0);
    return 1;
  }
  return 0;
}


/*
** Adjust pending gotos to outer level of a block.
*/
static void movegotosout (FuncState *fs, BlockCnt *bl) {
  int i;
  Labellist *gl = &fs->ls->dyd->gt;
  /* correct pending gotos to current block */
  for (i = bl->firstgoto; i < gl->n; i++) {  /* for each pending goto */
    Labeldesc *gt = &gl->arr[i];
    /* leaving a variable scope? */
    if (reglevel(fs, gt->nactvar) > reglevel(fs, bl->nactvar))
      gt->close |= bl->upval;  /* jump may need a close */
    gt->nactvar = bl->nactvar;  /* update goto level */
  }
}


static void enterblock (FuncState *fs, BlockCnt *bl, lu_byte isloop) {
  bl->breaklist = NO_JUMP;
  bl->isloop = isloop;
  bl->scopeend = NO_JUMP;
  bl->nactvar = fs->nactvar;
  bl->firstlabel = fs->ls->dyd->label.n;
  bl->firstgoto = fs->ls->dyd->gt.n;
  bl->upval = 0;
  bl->insidetbc = static_cast<lu_byte>(fs->bl != NULL && fs->bl->insidetbc);
  bl->previous = fs->bl;
  fs->bl = bl;
  hello_assert(fs->freereg == helloY_nvarstack(fs));
}


/*
** generates an error for an undefined 'goto'.
*/
[[noreturn]] static void undefgoto (LexState *ls, Labeldesc *gt) {
  const char *msg;
  if (eqstr(gt->name, helloS_newliteral(ls->L, "break"))) {
    msg = "break outside loop at line %d";
    msg = helloO_pushfstring(ls->L, msg, gt->line);
  }
  else {
    msg = "no visible label '%s' for <goto> at line %d";
    msg = helloO_pushfstring(ls->L, msg, getstr(gt->name), gt->line);
  }
  helloK_semerror(ls, msg);
}


static void leaveblock (FuncState *fs) {
  BlockCnt *bl = fs->bl;
  LexState *ls = fs->ls;
  int hasclose = 0;
  int stklevel = reglevel(fs, bl->nactvar);  /* level outside the block */
  removevars(fs, bl->nactvar);  /* remove block locals */
  hello_assert(bl->nactvar == fs->nactvar);  /* back to level on entry */
  if (bl->isloop)  /* has to fix pending breaks? */
    hasclose = createlabel(ls, helloS_newliteral(ls->L, "break"), 0, 0);
  if (!hasclose && bl->previous && bl->upval)  /* still need a 'close'? */
    helloK_codeABC(fs, OP_CLOSE, stklevel, 0, 0);
  fs->freereg = stklevel;  /* free registers */
  ls->dyd->label.n = bl->firstlabel;  /* remove local labels */
  fs->bl = bl->previous;  /* current block now is previous one */
  if (bl->previous)  /* was it a nested block? */
    movegotosout(fs, bl);  /* update pending gotos to enclosing block */
  else {
    if (bl->firstgoto < ls->dyd->gt.n)  /* still pending gotos? */
      undefgoto(ls, &ls->dyd->gt.arr[bl->firstgoto]);  /* error */
  }
  helloK_patchtohere(fs, bl->breaklist);
  ls->laststat.token = TK_EOS;  /* Prevent unreachable code warnings on blocks that don't explicitly check for TK_END. */
}


/*
** adds a new prototype into list of prototypes
*/
static Proto *addprototype (LexState *ls) {
  Proto *clp;
  hello_State *L = ls->L;
  FuncState *fs = ls->fs;
  Proto *f = fs->f;  /* prototype of current function */
  if (fs->np >= f->sizep) {
    int oldsize = f->sizep;
    helloM_growvector(L, f->p, fs->np, f->sizep, Proto *, MAXARG_Bx, "functions");
    while (oldsize < f->sizep)
      f->p[oldsize++] = NULL;
  }
  f->p[fs->np++] = clp = helloF_newproto(L);
  helloC_objbarrier(L, f, clp);
  return clp;
}


/*
** codes instruction to create new closure in parent function.
** The OP_CLOSURE instruction uses the last available register,
** so that, if it invokes the GC, the GC knows which registers
** are in use at that time.

*/
static void codeclosure (LexState *ls, expdesc *v) {
  FuncState *fs = ls->fs->prev;
  init_exp(v, VRELOC, helloK_codeABx(fs, OP_CLOSURE, 0, fs->np - 1));
  helloK_exp2nextreg(fs, v);  /* fix it at the last register */
}


static void open_func (LexState *ls, FuncState *fs, BlockCnt *bl) {
  Proto *f = fs->f;
  fs->prev = ls->fs;  /* linked list of funcstates */
  fs->ls = ls;
  ls->fs = fs;
  fs->pc = 0;
  fs->previousline = f->linedefined;
  fs->iwthabs = 0;
  fs->lasttarget = 0;
  fs->freereg = 0;
  fs->nk = 0;
  fs->nabslineinfo = 0;
  fs->np = 0;
  fs->nups = 0;
  fs->ndebugvars = 0;
  fs->nactvar = 0;
  fs->needclose = 0;
  fs->firstlocal = ls->dyd->actvar.n;
  fs->firstlabel = ls->dyd->label.n;
  fs->bl = NULL;
  f->source = ls->source;
  helloC_objbarrier(ls->L, f, f->source);
  f->maxstacksize = 2;  /* registers 0/1 are always valid */
  enterblock(fs, bl, 0);
}


static void close_func (LexState *ls) {
  hello_State *L = ls->L;
  FuncState *fs = ls->fs;
  Proto *f = fs->f;
  helloK_ret(fs, helloY_nvarstack(fs), 0);  /* final return */
  leaveblock(fs);
  hello_assert(fs->bl == NULL);
  helloK_finish(fs);
  helloM_shrinkvector(L, f->code, f->sizecode, fs->pc, Instruction);
  helloM_shrinkvector(L, f->lineinfo, f->sizelineinfo, fs->pc, ls_byte);
  helloM_shrinkvector(L, f->abslineinfo, f->sizeabslineinfo,
                       fs->nabslineinfo, AbsLineInfo);
  helloM_shrinkvector(L, f->k, f->sizek, fs->nk, TValue);
  helloM_shrinkvector(L, f->p, f->sizep, fs->np, Proto *);
  helloM_shrinkvector(L, f->locvars, f->sizelocvars, fs->ndebugvars, LocVar);
  helloM_shrinkvector(L, f->upvalues, f->sizeupvalues, fs->nups, Upvaldesc);
  ls->fs = fs->prev;
  helloC_checkGC(L);
}



/*============================================================*/
/* GRAMMAR RULES */
/*============================================================*/


/*
** check whether current token is in the follow set of a block.
** 'until' closes syntactical blocks, but do not close scope,
** so it is handled in separate.
*/
static int block_follow (LexState *ls, int withuntil) {
  switch (ls->t.token) {
    case TK_ELSE: case TK_ELSEIF:
    case TK_END: case TK_EOS:
      return 1;
    case TK_PWHEN:
#ifndef HELLO_COMPATIBLE_WHEN
    case TK_WHEN:
#endif
    case TK_UNTIL: return withuntil;
    default: return 0;
  }
}


static void propagate_return_type(TypeDesc *prop, TypeDesc&& ret) {
  if (prop->getType() != VT_DUNNO) { /* had previous return path(s)? */
    if (prop->getType() == VT_NIL) {
      ret.setNullable();
      *prop = ret;
    }
    else if (ret.getType() == VT_NIL) {
      prop->setNullable();
    }
    else if (!prop->isCompatibleWith(ret)) {
      *prop = VT_MIXED; /* set return type to mixed */
      prop = nullptr; /* and don't update it again */
    }
  }
  else {
    *prop = ret; /* save return type */
  }
}

static void statlist (LexState *ls, TypeDesc *prop = nullptr, bool no_ret_implies_nil = false) {
  /* statlist -> { stat [';'] } */
  bool ret = false;
  while (!block_follow(ls, 1)) {
    ret = (ls->t.token == TK_RETURN);
    TypeDesc p = VT_DUNNO;
    statement(ls, &p);
    if (prop && /* do we need to propagate the return type? */
        p.getType() != VT_DUNNO) { /* is there a return path here? */
      propagate_return_type(prop, p.getType());
    }
    if (ret) break;
  }
  if (prop && /* do we need to propagate the return type? */
      !ret && /* had no return statement? */
      no_ret_implies_nil) { /* does that imply a nil return? */
    propagate_return_type(prop, VT_NIL); /* propagate */
  }
}


/*
** Continue statement. Semantically similar to "goto continue".
** Unlike break, this doesn't use labels. It tracks where to jump via BlockCnt.scopeend;
*/
static void continuestat (LexState *ls, hello_Integer backwards_surplus = 0) {
  auto line = ls->getLineNumber();
  FuncState *fs = ls->fs;
  BlockCnt *bl = fs->bl;
  int upval = 0;
  int foundloops = 0;
  helloX_next(ls); /* skip TK_CONTINUE */
  hello_Integer backwards = 1;
  if (ls->t.token == TK_INT) {
    backwards = ls->t.seminfo.i;
    if (backwards == 0) {
      throwerr(ls, "expected number of blocks to skip, found '0'", "unexpected '0'", line);
    }
    helloX_next(ls);
  }
  backwards += backwards_surplus;
  while (bl) {
    if (!bl->isloop) { /* not a loop, continue search */
      upval |= bl->upval; /* amend upvalues for closing. */
      bl = bl->previous; /* jump back current blocks to find the loop */
    }
    else { /* found a loop */
      if (--backwards == 0) { /* this is our loop */
        break;
      }
      else { /* continue search */
        upval |= bl->upval;
        bl = bl->previous;
        ++foundloops;
      }
    }
  }
  if (bl) {
    if (upval) helloK_codeABC(fs, OP_CLOSE, bl->nactvar, 0, 0); /* close upvalues */
    helloK_concat(fs, &bl->scopeend, helloK_jump(fs));
  }
  else {
    if (foundloops == 0)
      throwerr(ls, "'continue' outside of a loop","'continue' can only be used inside the context of a loop.", line);
    else {
      if (foundloops == 1) {
        throwerr(ls,
          "'continue' argument exceeds the amount of enclosing loops",
            helloO_fmt(ls->L, "there is only 1 enclosing loop.", foundloops));
      }
      else {
        throwerr(ls,
          "'continue' argument exceeds the amount of enclosing loops",
            helloO_fmt(ls->L, "there are only %d enclosing loops.", foundloops));
      }
    }
  }
}


/* Switch logic partially inspired by Paige Marie DePol from the Hello mailing list. */
static void caselist (LexState *ls) {
  while (gett(ls) != TK_CASE
      && gett(ls) != TK_DEFAULT
      && gett(ls) != TK_END
      && gett(ls) != TK_PCASE
      && gett(ls) != TK_PDEFAULT
    ) {
    if (gett(ls) == TK_PCONTINUE
        || gett(ls) == TK_CONTINUE
        ) {
      continuestat(ls, 1);
    }
    else {
      statement(ls);
    }
  }
  ls->laststat.token = TK_EOS;  /* We don't want warnings for trailing control flow statements. */
}


static void fieldsel (LexState *ls, expdesc *v) {
  /* fieldsel -> ['.' | ':'] NAME */
  FuncState *fs = ls->fs;
  expdesc key;
  helloK_exp2anyregup(fs, v);
  helloX_next(ls);  /* skip the dot or colon */
  codename(ls, &key);
  helloK_indexed(fs, v, &key);
}


static void yindex (LexState *ls, expdesc *v) {
  /* index -> '[' expr ']' */
  helloX_next(ls);  /* skip the '[' */
  expr(ls, v);
  helloK_exp2val(ls->fs, v);
  checknext(ls, ']');
}


/*
** {======================================================================
** Rules for Constructors
** =======================================================================
*/


typedef struct ConsControl {
  expdesc v;  /* last list item read */
  expdesc *t;  /* table descriptor */
  int nh;  /* total number of 'record' elements */
  int na;  /* number of array elements already stored */
  int tostore;  /* number of array elements pending to be stored */
} ConsControl;


static void recfield (LexState *ls, ConsControl *cc) {
  /* recfield -> (NAME | '['exp']') = exp */
  FuncState *fs = ls->fs;
  int reg = ls->fs->freereg;
  expdesc tab, key, val;
  if (ls->t.token == TK_NAME) {
    checklimit(fs, cc->nh, MAX_INT, "items in a constructor");
    codename(ls, &key);
  }
  else  /* ls->t.token == '[' */
    yindex(ls, &key);
  cc->nh++;
  checknext(ls, '=');
  tab = *cc->t;
  helloK_indexed(fs, &tab, &key);
  expr(ls, &val);
  helloK_storevar(fs, &tab, &val);
  fs->freereg = reg;  /* free registers */
}

static void prenamedfield(LexState* ls, ConsControl* cc, const char* name) {
  FuncState* fs = ls->fs;
  int reg = ls->fs->freereg;
  expdesc tab, key, val;
  codestring(&key, helloX_newstring(ls, name));
  cc->nh++;
  helloX_next(ls); /* skip name token */
  checknext(ls, '=');
  tab = *cc->t;
  helloK_indexed(fs, &tab, &key);
  expr(ls, &val);
  helloK_storevar(fs, &tab, &val);
  fs->freereg = reg;  /* free registers */
}

static void closelistfield (FuncState *fs, ConsControl *cc) {
  if (cc->v.k == VVOID) return;  /* there is no list item */
  helloK_exp2nextreg(fs, &cc->v);
  cc->v.k = VVOID;
  if (cc->tostore == LFIELDS_PER_FLUSH) {
    helloK_setlist(fs, cc->t->u.info, cc->na, cc->tostore);  /* flush */
    cc->na += cc->tostore;
    cc->tostore = 0;  /* no more items pending */
  }
}


static void lastlistfield (FuncState *fs, ConsControl *cc) {
  if (cc->tostore == 0) return;
  if (hasmultret(cc->v.k)) {
    helloK_setmultret(fs, &cc->v);
    helloK_setlist(fs, cc->t->u.info, cc->na, HELLO_MULTRET);
    cc->na--;  /* do not count last expression (unknown number of elements) */
  }
  else {
    if (cc->v.k != VVOID)
      helloK_exp2nextreg(fs, &cc->v);
    helloK_setlist(fs, cc->t->u.info, cc->na, cc->tostore);
  }
  cc->na += cc->tostore;
}


static void listfield (LexState *ls, ConsControl *cc) {
  /* listfield -> exp */
  expr(ls, &cc->v);
  cc->tostore++;
}


static void body (LexState *ls, expdesc *e, int ismethod, int line, TypeDesc *prop = nullptr);
static void funcfield (LexState *ls, struct ConsControl *cc) {
  /* funcfield -> function NAME funcargs */
  FuncState *fs = ls->fs;
  int reg = ls->fs->freereg;
  expdesc tab, key, val;
  cc->nh++;
  helloX_next(ls); /* skip TK_FUNCTION */
  codename(ls, &key);
  tab = *cc->t;
  helloK_indexed(fs, &tab, &key);
  body(ls, &val, true, ls->getLineNumber());
  helloK_storevar(fs, &tab, &val);
  fs->freereg = reg;  /* free registers */
}


static void field (LexState *ls, ConsControl *cc) {
  /* field -> listfield | recfield | funcfield */
  switch(ls->t.token) {
    case TK_NAME: {  /* may be 'listfield' or 'recfield' */
      if (helloX_lookahead(ls) != '=')  /* expression? */
        listfield(ls, cc);
      else
        recfield(ls, cc);
      break;
    }
    case '[': {
      recfield(ls, cc);
      break;
    }
    case TK_FUNCTION: {
      if (helloX_lookahead(ls) == '(') {
        listfield(ls, cc);
      }
      else {
        funcfield(ls, cc);
      }
      break;
    }
    default: {
      if (ls->t.IsReservedNonValue()) {
        prenamedfield(ls, cc, helloX_reserved2str(ls->t.token));
      } else {
        listfield(ls, cc);
      }
      break;
    }
  }
}


static void constructor (LexState *ls, expdesc *t) {
  /* constructor -> '{' [ field { sep field } [sep] ] '}'
     sep -> ',' | ';' */
  FuncState *fs = ls->fs;
  int line = ls->getLineNumber();
  int pc = helloK_codeABC(fs, OP_NEWTABLE, 0, 0, 0);
  ConsControl cc;
  helloK_code(fs, 0);  /* space for extra arg. */
  cc.na = cc.nh = cc.tostore = 0;
  cc.t = t;
  init_exp(t, VNONRELOC, fs->freereg);  /* table will be at stack top */
  helloK_reserveregs(fs, 1);
  init_exp(&cc.v, VVOID, 0);  /* no value (yet) */
  checknext(ls, '{');
  do {
    hello_assert(cc.v.k == VVOID || cc.tostore > 0);
    if (ls->t.token == '}') break;
    closelistfield(fs, &cc);
    field(ls, &cc);
  } while (testnext(ls, ',') || testnext(ls, ';'));
  check_match(ls, '}', '{', line);
  lastlistfield(fs, &cc);
  helloK_settablesize(fs, pc, t->u.info, cc.na, cc.nh);
}

/* }====================================================================== */


static void setvararg (FuncState *fs, int nparams) {
  fs->f->is_vararg = 1;
  helloK_codeABC(fs, OP_VARARGPREP, nparams, 0, 0);
}


static void simpleexp (LexState *ls, expdesc *v, bool no_colon = false, TypeDesc *prop = nullptr);
static void simpleexp_with_unary_support (LexState *ls, expdesc *v) {
  if (testnext(ls, '-')) { /* Negative constant? */
    check(ls, TK_INT);
    init_exp(v, VKINT, 0);
    v->u.ival = (ls->t.seminfo.i * -1);
    helloX_next(ls);
  }
  else {
    testnext(ls, '+'); /* support pseudo-unary '+' */
    simpleexp(ls, v, true);
  }
}


static void parlist (LexState *ls, std::vector<expdesc>* fallbacks = nullptr) {
  /* parlist -> [ {NAME ','} (NAME | '...') ] */
  FuncState *fs = ls->fs;
  Proto *f = fs->f;
  int nparams = 0;
  int isvararg = 0;
  if (ls->t.token != ')' && ls->t.token != '|') {  /* is 'parlist' not empty? */
    do {
      if (isnametkn(ls, true)) {
        auto parname = str_checkname(ls, true);
        auto parhint = gettypehint(ls);
        new_localvar(ls, parname, parhint);
        if (fallbacks) {
          expdesc* parfallback = &fallbacks->emplace_back(expdesc{});
          if (testnext(ls, '=')) {
            simpleexp_with_unary_support(ls, parfallback);
            if (!vkisconst(parfallback->k)) {
              helloX_syntaxerror(ls, "parameter fallback value must be a compile-time constant");
            }
          }
        }
        nparams++;
      }
      else if (ls->t.token == TK_DOTS) {
        helloX_next(ls);
        isvararg = 1;
      }
      else helloX_syntaxerror(ls, "<name> or '...' expected");
    } while (!isvararg && testnext(ls, ','));
  }
  adjustlocalvars(ls, nparams);
  f->numparams = cast_byte(fs->nactvar);
  if (isvararg)
    setvararg(fs, f->numparams);  /* declared vararg */
  helloK_reserveregs(fs, fs->nactvar);  /* reserve registers for parameters */
}


static void body (LexState *ls, expdesc *e, int ismethod, int line, TypeDesc *prop) {
  /* body ->  '(' parlist ')' block END */
  ls->pushContext(PARCTX_BODY);
  FuncState new_fs;
  BlockCnt bl;
  new_fs.f = addprototype(ls);
  new_fs.f->linedefined = line;
  open_func(ls, &new_fs, &bl);
  checknext(ls, '(');
  if (ismethod) {
    new_localvarliteral(ls, "self");  /* create 'self' parameter */
    adjustlocalvars(ls, 1);
  }
  std::vector<expdesc> fallbacks{};
  parlist(ls, &fallbacks);
  int fallback_idx = 0;
  for (auto& fallback : fallbacks) {
    if (fallback.k != VVOID) {
      Vardesc *vd = getlocalvardesc(ls->fs, fallback_idx);
      expdesc lv;
      singlevaraux(ls->fs, vd->vd.name, &lv, 1);
      expdesc lcond = lv;
      helloK_goifnil(ls->fs, &lcond);
      helloK_storevar(ls->fs, &lv, &fallback);
      helloK_patchtohere(ls->fs, lcond.t);
    }
    ++fallback_idx;
  }
  checknext(ls, ')');
  TypeDesc rethint = gettypehint(ls);
  TypeDesc p = VT_DUNNO;
  statlist(ls, &p, true);
#ifndef HELLO_NO_PARSER_WARNINGS
  if (rethint.getType() != VT_DUNNO && /* has type hint for return type? */
      p.getType() != VT_DUNNO && /* return type is known? */
      !rethint.isCompatibleWith(p)) { /* incompatible? */
    std::string err = "function was hinted to return ";
    err.append(rethint.toString());
    err.append(" but actually returns ");
    err.append(p.toString());
    throw_warn(ls, err.c_str(), line, TYPE_MISMATCH);
  }
#endif
  if (prop) { /* propagate type of function */
    *prop = VT_FUNC;
    prop->proto = new_fs.f;
    prop->retn = p.primitive;
    int vidx = new_fs.firstlocal;
    for (lu_byte i = 0; i != prop->getNumTypedParams(); ++i) {
      prop->params[i] = ls->dyd->actvar.arr[vidx].vd.hint.primitive;
      ++vidx;
    }
  }
  new_fs.f->lastlinedefined = ls->getLineNumber();
  check_match(ls, TK_END, TK_FUNCTION, line);
  codeclosure(ls, e);
  close_func(ls);
  ls->popContext(PARCTX_BODY);
}


/*
** Lambda implementation.
** Shorthands lambda expressions into `function (...) return ... end`.
** The '|' token was chosen because it's not commonly used as an unary operator in programming.
** The '->' arrow syntax looked more visually appealing than a colon. It also plays along with common lambda tokens.
*/
static void lambdabody (LexState *ls, expdesc *e, int line) {
  FuncState new_fs;
  BlockCnt bl;
  new_fs.f = addprototype(ls);
  new_fs.f->linedefined = line;
  open_func(ls, &new_fs, &bl);
  checknext(ls, '|');
  parlist(ls);
  checknext(ls, '|');
  checknext(ls, '-');
  checknext(ls, '>');
  expr(ls, e);
  helloK_ret(&new_fs, helloK_exp2anyreg(&new_fs, e), 1);
  new_fs.f->lastlinedefined = ls->getLineNumber();
  codeclosure(ls, e);
  close_func(ls);
}


static void expr_propagate(LexState *ls, expdesc* v, TypeDesc& td) {
  expr(ls, v, &td);
  exp_propagate(ls, *v, td);
}

static int explist (LexState *ls, expdesc *v, std::vector<TypeDesc>& prop) {
  /* explist -> expr { ',' expr } */
  int n = 1;  /* at least one expression */
  expr_propagate(ls, v, prop.emplace_back(VT_DUNNO));
  while (testnext(ls, ',')) {
    helloK_exp2nextreg(ls->fs, v);
    expr_propagate(ls, v, prop.emplace_back(VT_DUNNO));
    n++;
  }
  return n;
}

static int explist (LexState *ls, expdesc *v, TypeDesc *prop = nullptr) {
  /* explist -> expr { ',' expr } */
  int n = 1;  /* at least one expression */
  expr(ls, v, prop);
  while (testnext(ls, ',')) {
    helloK_exp2nextreg(ls->fs, v);
    expr(ls, v);
    n++;
  }
  return n;
}

static void funcargs (LexState *ls, expdesc *f, int line, TypeDesc *funcdesc = nullptr) {
  ls->pushContext(PARCTX_FUNCARGS);
  FuncState *fs = ls->fs;
  expdesc args;
  std::vector<TypeDesc> argdescs;
  int base, nparams;
  switch (ls->t.token) {
    case '(': {  /* funcargs -> '(' [ explist ] ')' */
      helloX_next(ls);
      if (ls->t.token == ')')  /* arg list is empty? */
        args.k = VVOID;
      else {
        explist(ls, &args, argdescs);
        if (hasmultret(args.k))
          helloK_setmultret(fs, &args);
      }
      check_match(ls, ')', '(', line);
      break;
    }
    case '{': {  /* funcargs -> constructor */
      argdescs = { TypeDesc{ VT_TABLE } };
      constructor(ls, &args);
      break;
    }
    case TK_STRING: {  /* funcargs -> STRING */
      argdescs = { TypeDesc{ VT_STR } };
      codestring(&args, ls->t.seminfo.ts);
      helloX_next(ls);  /* must use 'seminfo' before 'next' */
      break;
    }
    default: {
      helloX_syntaxerror(ls, "function arguments expected");
    }
  }
#ifndef HELLO_NO_PARSER_WARNINGS
  if (funcdesc) {
    for (lu_byte i = 0; i != funcdesc->getNumTypedParams(); ++i) {
      const PrimitiveType& param_hint = funcdesc->params[i];
      if (param_hint.getType() == VT_DUNNO)
        continue; /* skip parameters without type hint */
      TypeDesc arg = VT_NIL;
      if (i < (int)argdescs.size()) {
        arg = argdescs.at(i);
        if (arg.getType() == VT_DUNNO)
          continue; /* skip arguments without propagated type */
      }
      if (!param_hint.isCompatibleWith(arg.primitive)) {
        std::string err = "Function's ";;
        err.append(funcdesc->proto->locvars[i].varname->contents, funcdesc->proto->locvars[i].varname->size());
        err.append(" parameter was type-hinted as ");
        err.append(param_hint.toString());
        err.append(" but provided with ");
        err.append(arg.toString());
        throw_warn(ls, err.c_str(), "argument type mismatch", line, TYPE_MISMATCH);
      }
    }
    const auto expected = funcdesc->getNumParams();
    const auto received = (int)argdescs.size();
    if (!funcdesc->proto->is_vararg && expected < received) {  /* Too many arguments? */
      auto suffix = expected == 1 ? "" : "s"; // Omit plural suffixes when the noun is singular.
      throw_warn(ls,
        "too many arguments",
          helloO_fmt(ls->L, "expected %d argument%s, got %d.", expected, suffix, received), EXCESSIVE_ARGUMENTS);
      --ls->L->top;
    }
  }
#endif
  hello_assert(f->k == VNONRELOC);
  base = f->u.info;  /* base register for call */
  if (hasmultret(args.k))
    nparams = HELLO_MULTRET;  /* open call */
  else {
    if (args.k != VVOID)
      helloK_exp2nextreg(fs, &args);  /* close last argument */
    nparams = fs->freereg - (base+1);
  }
  init_exp(f, VCALL, helloK_codeABC(fs, OP_CALL, base, nparams+1, 2));
  helloK_fixline(fs, line);
  fs->freereg = base+1;  /* call remove function and arguments and leaves
                            (unless changed) one result */
  ls->popContext(PARCTX_FUNCARGS);
}




/*
** {======================================================================
** Expression parsing
** =======================================================================
*/


/*
** Safe navigation is entirely accredited to SvenOlsen.
** http://hello-users.org/wiki/SvenOlsen
*/
static void safe_navigation(LexState *ls, expdesc *v) {
  FuncState *fs = ls->fs;
  helloK_exp2nextreg(fs, v);
  helloK_codeABC(fs, OP_TEST, v->u.info, NO_REG, 0 );
  {
    int old_free = fs->freereg;             
    int vreg = v->u.info;
    int j = helloK_jump(fs);
    expdesc key;
    switch(ls->t.token) {
      case '[': {
        helloX_next(ls);  /* skip the '[' */
        if (ls->t.token == '-') {
          expr(ls, &key);
          switch (key.k) {
            case VKINT: {
              key.u.ival *= -1;
              break;
            }
            case VKFLT: {
              key.u.nval *= -1;
              break;
            }
            default: {
              throwerr(ls, "unexpected symbol during navigation.", "unary '-' on non-numeral type.");
            }
          }
        }
        else expr(ls, &key);
        checknext(ls, ']');
        helloK_indexed(fs, v, &key);
        break; 
      }       
      case '.': {
        helloX_next(ls);
        codename(ls, &key);
        helloK_indexed(fs, v, &key);
        break;
      }
      default: {
        helloX_syntaxerror(ls, "unexpected symbol");
      }
    }
    helloK_exp2nextreg(fs, v);
    fs->freereg = old_free;
    if (v->u.info != vreg) {
      helloK_codeABC(fs, OP_MOVE, vreg, v->u.info, 0);
      v->u.info = vreg;
    }
    helloK_patchtohere(fs, j);
  }
}


struct StringChain {
  LexState* ls;
  expdesc* v;

  StringChain(LexState* ls, expdesc* v) noexcept
    : ls(ls), v(v)
  {
    enterlevel(ls);
    v->k = VVOID;
  }

  ~StringChain() noexcept {
    if (v->k == VVOID) { /* ensure we produce at least an empty string */
      codestring(v, helloS_new(ls->L, ""));
    }
    leavelevel(ls);
  }

  void add(const char* str) noexcept {
    if (v->k == VVOID) { /* first chain entry? */
      codestring(v, helloS_new(ls->L, str));
    } else {
      helloK_infix(ls->fs, OPR_CONCAT, v);
      expdesc v2;
      codestring(&v2, helloS_new(ls->L, str));
      helloK_posfix(ls->fs, OPR_CONCAT, v, &v2, ls->getLineNumber());
    }
  }

  void addVar(const char* varname) noexcept {
    if (v->k == VVOID) { /* first chain entry? */
      singlevarinner(ls, helloS_new(ls->L, varname), v);
    } else {
      helloK_infix(ls->fs, OPR_CONCAT, v);
      expdesc v2;
      singlevarinner(ls, helloS_new(ls->L, varname), &v2);
      helloK_posfix(ls->fs, OPR_CONCAT, v, &v2, ls->getLineNumber());
    }
  }
};


static void fstring (LexState *ls, expdesc *v) {
  helloX_next(ls); /* skip '$' */

  check(ls, TK_STRING);
  auto str = ls->t.seminfo.ts->toCpp();

  StringChain sc(ls, v);
  for (size_t i = 0;; ) {
    size_t del = str.find('{', i);
    auto chunk = str.substr(i, del - i);
    if (!chunk.empty()) {
      sc.add(chunk.c_str());
    }
    if (del == std::string::npos) {
      break;
    }
    ++del;
    size_t del2 = str.find('}', del);
    if (del2 == std::string::npos) {
      helloX_syntaxerror(ls, "Improper $-string with unterminated varname");
      break;
    }
    auto varname_len = (del2 - del);
    auto varname = str.substr(del, varname_len);
    del += varname_len + 1;
    sc.addVar(varname.c_str());
    i = del;
  }

  helloX_next(ls); /* skip string */
}


static void primaryexp (LexState *ls, expdesc *v) {
  /* primaryexp -> NAME | '(' expr ')' */
  if (isnametkn(ls)) {
    singlevar(ls, v);
    return;
  }
  switch (ls->t.token) {
    case '(': {
      int line = ls->getLineNumber();
      helloX_next(ls);
      expr(ls, v);
      check_match(ls, ')', '(', line);
      helloK_dischargevars(ls->fs, v);
      return;
    }
    case '}':
    case '{': { // Unfinished table constructors.
       if (ls->t.token == '{') {
         throwerr(ls, "unfinished table constructor", "did you mean to close with '}'?");
       }
       else {
         throwerr(ls, "unfinished table constructor", "did you mean to enter with '{'?");
       }
       return;
    }
    case '|': { // Potentially mistyped lambda expression. People may confuse '->' with '=>'.
      while (testnext(ls, '|') || testnext(ls, TK_NAME) || testnext(ls, ','));
      throwerr(ls, "unexpected symbol", "impromper or stranded lambda expression.");
      return;
    }
    case '$': {
      fstring(ls, v);
      return;
    }
    default: {
      const char *token = helloX_token2str(ls, ls->t.token);
      throwerr(ls, helloO_fmt(ls->L, "unexpected symbol near %s", token), "unexpected symbol.");
    }
  }
}


static void suffixedexp (LexState *ls, expdesc *v, bool no_colon = false, TypeDesc *prop = nullptr) {
  /* suffixedexp ->
       primaryexp { '.' NAME | '[' exp ']' | ':' NAME funcargs | funcargs } */
  FuncState *fs = ls->fs;
  int line = ls->getLineNumber();
  primaryexp(ls, v);
  for (;;) {
    switch (ls->t.token) {
      case '?': {  /* safe navigation or ternary */
        helloX_next(ls); /* skip '?' */
        if (gett(ls) != '[' && gett(ls) != '.') {
          /* it's a ternary but we have to deal with that later */
          helloX_prev(ls); /* unskip '?' */
          return; /* back to primaryexp */
        }
        safe_navigation(ls, v);
        break;
      }
      case '.': {  /* fieldsel */
        fieldsel(ls, v);
        break;
      }
      case '[': {  /* '[' exp ']' */
        expdesc key;
        helloK_exp2anyregup(fs, v);
        yindex(ls, &key);
        helloK_indexed(fs, v, &key);
        break;
      }
      case ':': {  /* ':' NAME funcargs */
        if (no_colon) {
          return;
        }
        expdesc key;
        helloX_next(ls);
        codename(ls, &key);
        helloK_self(fs, v, &key);
        funcargs(ls, v, line);
        break;
      }
      case '(': case TK_STRING: case '{': {  /* funcargs */
        TypeDesc* funcdesc = nullptr;
        if (v->k == VLOCAL) {
          auto fvar = getlocalvardesc(ls->fs, v->u.var.vidx);
          if (fvar->vd.prop.getType() == VT_FUNC) { /* just in case... */
            funcdesc = &fvar->vd.prop;
            if (prop) { /* propagate return type */
              *prop = fvar->vd.prop.retn;
            }
          }
        }
        helloK_exp2nextreg(fs, v);
        funcargs(ls, v, line, funcdesc);
        break;
      }
      default: return;
    }
  }
}


int cond (LexState *ls);
static void ifexpr (LexState *ls, expdesc *v) {
  /*
  ** Patch published by Ryota Hirose.
  */
  FuncState *fs = ls->fs;
  int condition;
  int escape = NO_JUMP;
  int reg;
  helloX_next(ls);			
  condition = cond(ls);
  checknext(ls, TK_THEN);
  expr(ls, v);					
  reg = helloK_exp2anyreg(fs, v);			
  helloK_concat(fs, &escape, helloK_jump(fs));
  helloK_patchtohere(fs, condition);
  checknext(ls, TK_ELSE);
  expr(ls, v);
  helloK_exp2reg(fs, v, reg);
  helloK_patchtohere(fs, escape);
}


static void simpleexp (LexState *ls, expdesc *v, bool no_colon, TypeDesc *prop) {
  /* simpleexp -> FLT | INT | STRING | NIL | TRUE | FALSE | ... |
                  constructor | FUNCTION body | suffixedexp */
  switch (ls->t.token) {
    case TK_FLT: {
      if (prop) *prop = VT_FLT;
      init_exp(v, VKFLT, 0);
      v->u.nval = ls->t.seminfo.r;
      break;
    }
    case TK_INT: {
      if (prop) *prop = VT_INT;
      init_exp(v, VKINT, 0);
      v->u.ival = ls->t.seminfo.i;
      break;
    }
    case TK_STRING: {
      if (prop) *prop = VT_STR;
      codestring(v, ls->t.seminfo.ts);
      break;
    }
    case TK_NIL: {
      if (prop) *prop = VT_NIL;
      init_exp(v, VNIL, 0);
      break;
    }
    case TK_TRUE: {
      if (prop) *prop = VT_BOOL;
      init_exp(v, VTRUE, 0);
      break;
    }
    case TK_FALSE: {
      if (prop) *prop = VT_BOOL;
      init_exp(v, VFALSE, 0);
      break;
    }
    case TK_DOTS: {  /* vararg */
      FuncState *fs = ls->fs;
      check_condition(ls, fs->f->is_vararg,
                      "cannot use '...' outside a vararg function");
      init_exp(v, VVARARG, helloK_codeABC(fs, OP_VARARG, 0, 0, 1));
      break;
    }
    case '{': {  /* constructor */
      if (prop) *prop = VT_TABLE;
      constructor(ls, v);
      return;
    }
    case TK_FUNCTION: {
      helloX_next(ls);
      body(ls, v, 0, ls->getLineNumber(), prop);
      return;
    }
    case '|': {
      lambdabody(ls, v, ls->getLineNumber());
      return;
    }
    default: {
      suffixedexp(ls, v, no_colon, prop);
      return;
    }
  }
  helloX_next(ls);
  if (!no_colon && testnext(ls, ':')) {
    expdesc key;
    codename(ls, &key);
    helloK_self(ls->fs, v, &key);
    funcargs(ls, v, ls->getLineNumber());
  }
}


static void inexpr (LexState *ls, expdesc *v) {
  expdesc v2;
  checknext(ls, TK_IN);
  expr(ls, &v2);
  helloK_exp2nextreg(ls->fs, v);
  helloK_exp2nextreg(ls->fs, &v2);
  helloK_codeABC(ls->fs, OP_IN, v->u.info, v2.u.info, 0);
  helloK_storevar(ls->fs, v, v);
}


static UnOpr getunopr (int op) {
  switch (op) {
    case TK_NOT: return OPR_NOT;
    case '-': return OPR_MINUS;
    case '~': return OPR_BNOT;
    case '#': return OPR_LEN;
    default: return OPR_NOUNOPR;
  }
}


static BinOpr getbinopr (int op) {
  switch (op) {
    case '+': return OPR_ADD;
    case '-': return OPR_SUB;
    case '*': return OPR_MUL;
    case '%': return OPR_MOD;
    case '^': return OPR_POW;
    case '/': return OPR_DIV;
    case TK_IDIV: return OPR_IDIV;
    case '&': return OPR_BAND;
    case '|': return OPR_BOR;
    case '~': return OPR_BXOR;
    case TK_SHL: return OPR_SHL;
    case TK_SHR: return OPR_SHR;
    case TK_CONCAT: return OPR_CONCAT;
    case TK_NE: return OPR_NE;
    case TK_EQ: return OPR_EQ;
    case '<': return OPR_LT;
    case TK_LE: return OPR_LE;
    case '>': return OPR_GT;
    case TK_GE: return OPR_GE;
    case TK_AND: return OPR_AND;
    case TK_OR: return OPR_OR;
    case TK_COAL: return OPR_COAL;
    case TK_POW: return OPR_POW;  /* '**' operator support */
    default: return OPR_NOBINOPR;
  }
}


static void prefixplusplus(LexState *ls, expdesc* v) {
  int line = ls->getLineNumber();
  helloX_next(ls); /* skip second '+' */
  singlevar(ls, v); /* variable name */
  FuncState *fs = ls->fs;
  expdesc e = *v, v2;
  if (v->k != VLOCAL) {  /* complex lvalue, use a temporary register. linear perf incr. with complexity of lvalue */
    helloK_reserveregs(fs, fs->freereg-fs->nactvar);
    enterlevel(ls);
    helloK_infix(fs, OPR_ADD, &e);
    init_exp(&v2, VKINT, 0);
    v2.u.ival = 1;
    helloK_posfix(fs, OPR_ADD, &e, &v2, line);
    leavelevel(ls);
    helloK_exp2nextreg(fs, &e);
    helloK_setoneret(ls->fs, &e);
    helloK_storevar(ls->fs, v, &e);
  }
  else {  /* simple lvalue; a local. directly change value (~20% speedup vs temporary register) */
    enterlevel(ls);
    helloK_infix(fs, OPR_ADD, &e);
    init_exp(&v2, VKINT, 0);
    v2.u.ival = 1;
    helloK_posfix(fs, OPR_ADD, &e, &v2, line);
    leavelevel(ls);
    helloK_setoneret(ls->fs, &e);
    helloK_storevar(ls->fs, v, &e);
  }
}


/*
** Priority table for binary operators.
*/
static const struct {
  lu_byte left;  /* left priority for each binary operator */
  lu_byte right; /* right priority */
} priority[] = {  /* ORDER OPR */
   {10, 10}, {10, 10},           /* '+' '-' */
   {11, 11}, {11, 11},           /* '*' '%' */
   {14, 13},                  /* '^' (right associative) */
   {11, 11}, {11, 11},           /* '/' '//' */
   {6, 6}, {4, 4}, {5, 5},   /* '&' '|' '~' */
   {7, 7}, {7, 7},           /* '<<' '>>' */
   {9, 8},                   /* '..' (right associative) */
   {3, 3}, {3, 3}, {3, 3},   /* ==, <, <= */
   {3, 3}, {3, 3}, {3, 3},   /* ~=, >, >= */
   {2, 2}, {1, 1}, {1, 1}    /* and, or, ?? */
};

#define UNARY_PRIORITY	12  /* priority for unary operators */


/*
** subexpr -> (simpleexp | unop subexpr) { binop subexpr }
** where 'binop' is any binary operator with a priority higher than 'limit'
*/
static BinOpr subexpr (LexState *ls, expdesc *v, int limit, TypeDesc *prop = nullptr, bool no_colon = false) {
  BinOpr op;
  UnOpr uop;
  enterlevel(ls);
  uop = getunopr(ls->t.token);
  if (uop != OPR_NOUNOPR) {  /* prefix (unary) operator? */
    int line = ls->getLineNumber();
    helloX_next(ls);  /* skip operator */
    subexpr(ls, v, UNARY_PRIORITY);
    helloK_prefix(ls->fs, uop, v, line);
  }
  else if (ls->t.token == TK_IF) ifexpr(ls, v);
  else if (ls->t.token == '+') {
    int line = ls->getLineNumber();
    helloX_next(ls); /* skip '+' */
    if (ls->t.token == '+') { /* '++' ? */
      prefixplusplus(ls, v);
    }
    else {
      /* support pseudo-unary '+' by implying '0 + subexpr' */
      init_exp(v, VKINT, 0);
      v->u.ival = 0;
      helloK_infix(ls->fs, OPR_ADD, v);

      expdesc v2;
      subexpr(ls, &v2, priority[OPR_ADD].right);
      helloK_posfix(ls->fs, OPR_ADD, v, &v2, line);
    }
  }
  else {
    simpleexp(ls, v, no_colon, prop);
    if (ls->t.token == TK_IN) {
      inexpr(ls, v);
      if (prop) *prop = VT_BOOL;
    }
  }
  /* expand while operators have priorities higher than 'limit' */
  op = getbinopr(ls->t.token);
  while (op != OPR_NOBINOPR && priority[op].left > limit) {
    expdesc v2;
    BinOpr nextop;
    int line = ls->getLineNumber();
    helloX_next(ls);  /* skip operator */
    helloK_infix(ls->fs, op, v);
    /* read sub-expression with higher priority */
    nextop = subexpr(ls, &v2, priority[op].right);
    helloK_posfix(ls->fs, op, v, &v2, line);
    op = nextop;
  }
  leavelevel(ls);
  return op;  /* return first untreated operator */
}


static void expr (LexState *ls, expdesc *v, TypeDesc *prop, bool no_colon) {
  subexpr(ls, v, 0, prop, no_colon);
  if (testnext(ls, '?')) { /* ternary expression? */
    int escape = NO_JUMP;
    v->normaliseFalse();
    helloK_goiftrue(ls->fs, v);
    int condition = v->f;
    expr(ls, v, nullptr, true);
    auto fs = ls->fs;
    auto reg = helloK_exp2anyreg(fs, v);
    helloK_concat(fs, &escape, helloK_jump(fs));
    helloK_patchtohere(fs, condition);
    checknext(ls, ':');
    expr(ls, v);
    helloK_exp2reg(fs, v, reg);
    helloK_patchtohere(fs, escape);
    if (prop) *prop = VT_MIXED; /* reset propagated type */
  }
}

/* }==================================================================== */



/*
** {======================================================================
** Rules for Statements
** =======================================================================
*/


static void block (LexState *ls) {
  /* block -> statlist */
  FuncState *fs = ls->fs;
  BlockCnt bl;
  enterblock(fs, &bl, 0);
  statlist(ls);
  leaveblock(fs);
}


/*
** structure to chain all variables in the left-hand side of an
** assignment
*/
struct LHS_assign {
  struct LHS_assign *prev, *next; /* previous & next lhs objects */
  expdesc v;  /* variable (global, local, upvalue, or indexed) */
};


/*
** check whether, in an assignment to an upvalue/local variable, the
** upvalue/local variable is begin used in a previous assignment to a
** table. If so, save original upvalue/local value in a safe place and
** use this safe copy in the previous assignment.
*/
static void check_conflict (LexState *ls, struct LHS_assign *lh, expdesc *v) {
  FuncState *fs = ls->fs;
  int extra = fs->freereg;  /* eventual position to save local variable */
  int conflict = 0;
  for (; lh; lh = lh->prev) {  /* check all previous assignments */
    if (vkisindexed(lh->v.k)) {  /* assignment to table field? */
      if (lh->v.k == VINDEXUP) {  /* is table an upvalue? */
        if (v->k == VUPVAL && lh->v.u.ind.t == v->u.info) {
          conflict = 1;  /* table is the upvalue being assigned now */
          lh->v.k = VINDEXSTR;
          lh->v.u.ind.t = extra;  /* assignment will use safe copy */
        }
      }
      else {  /* table is a register */
        if (v->k == VLOCAL && lh->v.u.ind.t == v->u.var.ridx) {
          conflict = 1;  /* table is the local being assigned now */
          lh->v.u.ind.t = extra;  /* assignment will use safe copy */
        }
        /* is index the local being assigned? */
        if (lh->v.k == VINDEXED && v->k == VLOCAL &&
            lh->v.u.ind.idx == v->u.var.ridx) {
          conflict = 1;
          lh->v.u.ind.idx = extra;  /* previous assignment will use safe copy */
        }
      }
    }
  }
  if (conflict) {
    /* copy upvalue/local value to a temporary (in position 'extra') */
    if (v->k == VLOCAL)
      helloK_codeABC(fs, OP_MOVE, extra, v->u.var.ridx, 0);
    else
      helloK_codeABC(fs, OP_GETUPVAL, extra, v->u.info, 0);
    helloK_reserveregs(fs, 1);
  }
}

/*
  gets the supported binary compound operation (if any)
  gives OPR_NOBINOPR if the operation does not have compound support.
  returns a status (0 false, 1 true) and takes a pointer to set.
  this allows for seamless conditional implementation, avoiding a getcompoundop call for every Hello assignment.
*/
static int getcompoundop (hello_Integer i, BinOpr *op) {
  switch (i) {
    case TK_CCAT: {
      *op = OPR_CONCAT;
      return 1;       /* concatenation */
    }
    case TK_CADD: {
      *op = OPR_ADD;  /* addition */
      return 1;
    }
    case TK_CSUB: {
      *op = OPR_SUB;  /* subtraction */
      return 1;
    }
    case TK_CMUL: {
      *op = OPR_MUL;  /* multiplication */
      return 1;
    }
    case TK_CMOD: {
      *op = OPR_MOD;  /* modulo */
      return 1;
    }
    case TK_CDIV: {
      *op = OPR_DIV;  /* float division */
      return 1;
    }
    case TK_CPOW: {
      *op = OPR_POW;  /* power */
      return 1;
    }
    case TK_CIDIV: {
      *op = OPR_IDIV;  /* integer division */
      return 1;
    }
    case TK_CBOR: {
      *op = OPR_BOR;  /* bitwise OR */
      return 1;
    }
    case TK_CBAND: {
      *op = OPR_BAND;  /* bitwise AND */
      return 1;
    }
    case TK_CBXOR: {
      *op = OPR_BXOR;  /* bitwise XOR */
      return 1;
    }
    case TK_CSHL: {
      *op = OPR_SHL;  /* shift left */
      return 1; 
    }
    case TK_CSHR: {
      *op = OPR_SHR;  /* shift right */
      return 1;
    }
    case TK_COAL: {
      *op = OPR_COAL;
      return 1;
    }
    default: {
      *op = OPR_NOBINOPR;
      return 0;
    }
  }
}

/* 
  compound assignment function
  determines the binary operation to perform depending on lexer state tokens (ls->lasttoken)
  resets the lexer state token
  reserves N registers (where N = local variables on stack)
  preforms binary operation and assignment
*/ 
static void compoundassign(LexState *ls, expdesc* v, BinOpr op) {
  helloX_next(ls);
  int line = ls->getLineNumber();
  FuncState *fs = ls->fs;
  expdesc e = *v, v2;
  if (v->k != VLOCAL) {  /* complex lvalue, use a temporary register. linear perf incr. with complexity of lvalue */
    helloK_reserveregs(fs, fs->freereg-fs->nactvar);
    enterlevel(ls);
    helloK_infix(fs, op, &e);
    expr(ls, &v2);
    helloK_posfix(fs, op, &e, &v2, line);
    leavelevel(ls);
    helloK_exp2nextreg(fs, &e);
    helloK_setoneret(ls->fs, &e);
    helloK_storevar(ls->fs, v, &e);
  }
  else {  /* simple lvalue; a local. directly change value (~20% speedup vs temporary register) */
    enterlevel(ls);
    helloK_infix(fs, op, &e);
    expr(ls, &v2);
    helloK_posfix(fs, op, &e, &v2, line);
    leavelevel(ls);
    helloK_setoneret(ls->fs, &e);
    helloK_storevar(ls->fs, v, &e);
  }
}

/*
  assignment function
  handles every Hello assignment
  special cases for compound operators via lexer state tokens (ls->t.seminfo.i)
*/
static void restassign (LexState *ls, struct LHS_assign *lh, int nvars) {
  int line = ls->getLineNumber(); /* in case we need to emit a warning */
  expdesc e;
  check_condition(ls, vkisvar(lh->v.k), "syntax error");
  check_readonly(ls, &lh->v);
  if (testnext(ls, ',')) {  /* restassign -> ',' suffixedexp restassign */
    struct LHS_assign nv;
    nv.prev = lh;
    nv.next = NULL;
    lh->next = &nv;
    suffixedexp(ls, &nv.v);
    if (!vkisindexed(nv.v.k))
      check_conflict(ls, lh, &nv.v);
    enterlevel(ls);  /* control recursion depth */
    restassign(ls, &nv, nvars+1);
    leavelevel(ls);
  }
  else {  /* restassign -> '=' explist */
    BinOpr op;  /* binary operation from lexer state */
    if (getcompoundop(ls->t.seminfo.i, &op) != 0) {  /* is there a saved binop? */
      check_condition(ls, nvars == 1, "unsupported tuple assignment");
      compoundassign(ls, &lh->v, op);  /* perform binop & assignment */
      return;  /* avoid default */
    }
    else if (testnext(ls, '=')) { /* no requested binop, continue */
      TypeDesc prop = VT_DUNNO;
      ParserContext ctx = ((nvars == 1) ? PARCTX_CREATE_VAR : PARCTX_CREATE_VARS);
      ls->pushContext(ctx);
      int nexps = explist(ls, &e, &prop);
      ls->popContext(ctx);
      if (nexps != nvars)
        adjust_assign(ls, nvars, nexps, &e);
      else {
        helloK_setoneret(ls->fs, &e);  /* close last expression */
        if (lh->v.k == VLOCAL) { /* assigning to a local variable? */
          exp_propagate(ls, e, prop);
          process_assign(ls, getlocalvardesc(ls->fs, lh->v.u.var.vidx), prop, line);
        }
        helloK_storevar(ls->fs, &lh->v, &e);
        return;  /* avoid default */
      }
    }
  }
  init_exp(&e, VNONRELOC, ls->fs->freereg-1);  /* default assignment */
  helloK_storevar(ls->fs, &lh->v, &e);
}

int cond (LexState *ls) {
  /* cond -> exp */
  expdesc v;
  expr(ls, &v);  /* read condition */
  v.normaliseFalse();
  helloK_goiftrue(ls->fs, &v);
  return v.f;
}


static void lgoto(LexState *ls, TString *name) {
  FuncState *fs = ls->fs;
  int line = ls->getLineNumber();
  Labeldesc *lb = findlabel(ls, name);
  if (lb == NULL)  /* no label? */
    /* forward jump; will be resolved when the label is declared */
    newgotoentry(ls, name, line, helloK_jump(fs));
  else {  /* found a label */
    /* backward jump; will be resolved here */
    int lblevel = reglevel(fs, lb->nactvar);  /* label level */
    if (helloY_nvarstack(fs) > lblevel)  /* leaving the scope of a variable? */
      helloK_codeABC(fs, OP_CLOSE, lblevel, 0, 0);
    /* create jump and link it to the label */
    helloK_patchlist(fs, helloK_jump(fs), lb->pc);
  }
}

static void gotostat (LexState *ls) {
  lgoto(ls, str_checkname(ls));
}


/*
** Break statement. Very similiar to `continue` usage, but it jumps slightly more forward.
**
** Implementation Detail:
**   Unlike normal Hello, it has been reverted from a label implementation back into a mix between a label & patchlist implementation.
**   This allows reusage of the existing "continue" implementation, which has been time-tested extensively by now.
*/
static void breakstat (LexState *ls) {
  auto line = ls->getLineNumber();
  FuncState *fs = ls->fs;
  BlockCnt *bl = fs->bl;
  int upval = 0;
  helloX_next(ls); /* skip TK_BREAK */
  hello_Integer backwards = 1;
  if (ls->t.token == TK_INT) {
    backwards = ls->t.seminfo.i;
    if (backwards == 0) {
      throwerr(ls, "expected number of blocks to skip, found '0'", "unexpected '0'", line);
    }
    helloX_next(ls);
  }
  while (bl) {
    if (!bl->isloop) { /* not a loop, continue search */
      upval |= bl->upval; /* amend upvalues for closing. */
      bl = bl->previous; /* jump back current blocks to find the loop */
    }
    else { /* found a loop */
      if (--backwards == 0) { /* this is our loop */
        break;
      }
      else { /* continue search */
        upval |= bl->upval;
        bl = bl->previous;
      }
    };
  }
  if (bl) {
    if (upval) helloK_codeABC(fs, OP_CLOSE, bl->nactvar, 0, 0); /* close upvalues */
    helloK_concat(fs, &bl->breaklist, helloK_jump(fs));
  }
  else {
    throwerr(ls, "break can't skip that many blocks", "try a smaller number", line);
  }
}


// Test the next token to see if it's either 'token1' or 'token2'.
inline bool testnext2 (LexState *ls, int token1, int token2) {
  return testnext(ls, token1) || testnext(ls, token2);
}


static void casecond(LexState* ls, int case_line, expdesc& lcase) {
  if (testnext(ls, '-')) { // Probably a negative constant.
    simpleexp(ls, &lcase, true);
    switch (lcase.k) {
      case VKINT:
        lcase.u.ival *= -1;
        break;
  
      case VKFLT:
        lcase.u.nval *= -1;
        break;
  
      default: // Why is there a unary '-' on a non-numeral type?
        throwerr(ls, "unexpected symbol in 'case' expression.", "unary '-' on non-numeral type.");
    }
  }
  else {
    testnext(ls, '+'); /* support pseudo-unary '+' */
    simpleexp(ls, &lcase, true);
    if (!vkisconst(lcase.k) && lcase.k != VLOCAL) {
      throwerr(ls, "malformed 'case' expression.", "expression must be compile-time constant.", case_line);
    }
  }
  checknext(ls, ':');
}


static void switchstat (LexState *ls, int line) {
  int switchToken = gett(ls);
  helloX_next(ls); // Skip switch statement.
  testnext(ls, '(');

  FuncState* fs = ls->fs;
  BlockCnt sbl;
  enterblock(fs, &sbl, 1);

  expdesc crtl, save, first;
  expr(ls, &crtl);
  helloK_exp2nextreg(ls->fs, &crtl);
  init_exp(&save, VLOCAL, crtl.u.info);
  testnext(ls, ')');
  checknext(ls, TK_DO);
  new_localvarliteral(ls, "(switch control value)"); // Save control value into a local.
  adjustlocalvars(ls, 1);

  TString* const begin_switch = helloS_newliteral(ls->L, "hello_begin_switch");
  TString* const end_switch = helloS_newliteral(ls->L, "hello_end_switch");
  TString* default_case = nullptr;

  if (gett(ls) == TK_CASE || gett(ls) == TK_PCASE) {
    int case_line = ls->getLineNumber();

    if (gett(ls) == TK_PCASE) {
      throw_warn(ls, "'hello_case' is deprecated", "use 'case' instead", WT_DEPRECATED);
    }
    helloX_next(ls); /* Skip 'case' */

    first = save;

    expdesc lcase;
    casecond(ls, case_line, lcase);

    helloK_infix(fs, OPR_NE, &first);
    helloK_posfix(fs, OPR_NE, &first, &lcase, ls->getLineNumber());

    caselist(ls);
  }
  else {
    first.k = VVOID;
    newgotoentry(ls, begin_switch, ls->getLineNumber(), helloK_jump(fs)); // goto begin_switch
  }

  std::vector<std::pair<expdesc, int>> cases{};

  while (gett(ls) != TK_END) {
    auto case_line = ls->getLineNumber();
    if (gett(ls) == TK_DEFAULT || gett(ls) == TK_PDEFAULT) {
      if (gett(ls) == TK_PDEFAULT) {
        throw_warn(ls, "'hello_default' is deprecated", "use 'default' instead", WT_DEPRECATED);
      }
      helloX_next(ls); /* Skip 'default' */

      checknext(ls, ':');
      if (default_case != nullptr)
        throwerr(ls, "switch statement already has a default case", "second default case", case_line);
      default_case = helloS_newliteral(ls->L, "hello_default_case");
      createlabel(ls, default_case, ls->getLineNumber(), block_follow(ls, 0));
      caselist(ls);
    }
    else {
      if (!testnext2(ls, TK_CASE, TK_PCASE)) {
        error_expected(ls, TK_CASE);
      }
      casecond(ls, case_line, cases.emplace_back(std::pair<expdesc, int>{ expdesc{}, helloK_getlabel(fs) }).first);
      caselist(ls);
    }
  }

  /* handle possible fallthrough, don't loop infinitely */
  newgotoentry(ls, end_switch, ls->getLineNumber(), helloK_jump(fs)); // goto end_switch

  if (first.k != VVOID) {
    helloK_patchtohere(fs, first.u.info);
  }
  else {
    createlabel(ls, begin_switch, ls->getLineNumber(), block_follow(ls, 0)); // ::begin_switch::
  }

  expdesc test;
  for (auto& c : cases) {
    test = save;
    helloK_infix(fs, OPR_EQ, &test);
    helloK_posfix(fs, OPR_EQ, &test, &c.first, ls->getLineNumber());
    helloK_patchlist(fs, test.u.info, c.second);
  }

  if (default_case != nullptr)
    lgoto(ls, default_case);

  createlabel(ls, end_switch, ls->getLineNumber(), block_follow(ls, 0)); // ::end_switch::

  check_match(ls, TK_END, switchToken, line);
  leaveblock(fs);
}


static void enumstat (LexState *ls) {
  /* enumstat -> ENUM [NAME] BEGIN NAME ['=' INT] { ',' NAME ['=' INT] } END */

  helloX_next(ls); /* skip 'enum' */

  if (gett(ls) != TK_BEGIN) { /* enum has name? */
    helloX_next(ls); /* skip name */
  }

  const auto line_begin = ls->getLineNumber();
  checknext(ls, TK_BEGIN); /* ensure we have 'begin' */

  hello_Integer i = 1;
  while (gett(ls) == TK_NAME) {
    auto vidx = new_localvar(ls, str_checkname(ls, true), ls->getLineNumber());
    auto var = getlocalvardesc(ls->fs, vidx);
    if (testnext(ls, '=')) {
      expdesc v;
      simpleexp_with_unary_support(ls, &v);
      if (v.k == VCONST) { /* compile-time constant? */
        TValue* k = &ls->dyd->actvar.arr[v.u.info].k;
        if (ttype(k) == HELLO_TNUMBER && ttisinteger(k)) { /* integer value? */
          init_exp(&v, VKINT, 0);
          v.u.ival = ivalue(k);
        }
      }
      if (v.k != VKINT) { /* assert expdesc kind */
        throwerr(ls, "expected integer constant", "unexpected expression type");
      }
      i = v.u.ival;
    }
    var->vd.kind = RDKCTC;
    setivalue(&var->k, i++);
    ls->fs->nactvar++;
    if (gett(ls) != ',') break;
    helloX_next(ls);
  }

  check_match(ls, TK_END, TK_BEGIN, line_begin);
}


/*
** Check whether there is already a label with the given 'name'.
*/
static void checkrepeated (LexState *ls, TString *name) {
  Labeldesc *lb = findlabel(ls, name);
  if (l_unlikely(lb != NULL)) {  /* already defined? */
    const char *msg = "label '%s' already defined on line %d";
    msg = helloO_pushfstring(ls->L, msg, getstr(name), lb->line);
    helloK_semerror(ls, msg);  /* error */
  }
}


static void labelstat (LexState *ls, TString *name, int line) {
  /* label -> '::' NAME '::' */
  checknext(ls, TK_DBCOLON);  /* skip double colon */
  while (ls->t.token == ';' || ls->t.token == TK_DBCOLON)
    statement(ls);  /* skip other no-op statements */
  checkrepeated(ls, name);  /* check for repeated labels */
  createlabel(ls, name, line, block_follow(ls, 0));
}


static void whilestat (LexState *ls, int line) {
  /* whilestat -> WHILE cond DO block END */
  FuncState *fs = ls->fs;
  int whileinit;
  int condexit;
  BlockCnt bl;
  helloX_next(ls);  /* skip WHILE */
  whileinit = helloK_getlabel(fs);
  condexit = cond(ls);
  enterblock(fs, &bl, 1);
  checknext(ls, TK_DO);
  block(ls);
  helloK_jumpto(fs, whileinit);
  helloK_patchlist(fs, bl.scopeend, whileinit);
  check_match(ls, TK_END, TK_WHILE, line);
  leaveblock(fs);
  helloK_patchtohere(fs, condexit);  /* false conditions finish the loop */
}


static void repeatstat (LexState *ls) {
  /* repeatstat -> REPEAT block ( UNTIL | WHEN ) cond */
  int condexit;
  FuncState *fs = ls->fs;
  int repeat_init = helloK_getlabel(fs);
  BlockCnt bl1, bl2;
  enterblock(fs, &bl1, 1);  /* loop block */
  enterblock(fs, &bl2, 0);  /* scope block */
  helloX_next(ls);  /* skip REPEAT */
  statlist(ls);
  helloK_patchtohere(fs, bl1.scopeend);
  if (testnext(ls, TK_UNTIL)) {
    condexit = cond(ls);  /* read condition (inside scope block) */
#ifdef HELLO_COMPATIBLE_WHEN
  } else if (testnext(ls, TK_PWHEN)) {
#else
  } else if (testnext2(ls, TK_PWHEN, TK_WHEN)) {
#endif
    expdesc v;
    expr(ls, &v);  /* read condition */
    v.normaliseFalse();
    helloK_goiffalse(ls->fs, &v);
    condexit = v.t;
  }
  else {
    error_expected(ls, TK_UNTIL);
  }
  leaveblock(fs);  /* finish scope */
  if (bl2.upval) {  /* upvalues? */
    int exit = helloK_jump(fs);  /* normal exit must jump over fix */
    helloK_patchtohere(fs, condexit);  /* repetition must close upvalues */
    helloK_codeABC(fs, OP_CLOSE, reglevel(fs, bl2.nactvar), 0, 0);
    condexit = helloK_jump(fs);  /* repeat after closing upvalues */
    helloK_patchtohere(fs, exit);  /* normal exit comes to here */
  }
  helloK_patchlist(fs, condexit, repeat_init);  /* close the loop */
  leaveblock(fs);  /* finish loop */
}


/*
** Read an expression and generate code to put its results in next
** stack slot.
**
*/
static void exp1 (LexState *ls) {
  expdesc e;
  expr(ls, &e);
  helloK_exp2nextreg(ls->fs, &e);
  hello_assert(e.k == VNONRELOC);
}


/*
** Fix for instruction at position 'pc' to jump to 'dest'.
** (Jump addresses are relative in Hello). 'back' true means
** a back jump.
*/
static void fixforjump (FuncState *fs, int pc, int dest, int back) {
  Instruction *jmp = &fs->f->code[pc];
  int offset = dest - (pc + 1);
  if (back)
    offset = -offset;
  if (l_unlikely(offset > MAXARG_Bx))
    helloX_syntaxerror(fs->ls, "control structure too long");
  SETARG_Bx(*jmp, offset);
}


/*
** Generate code for a 'for' loop.
*/
static void forbody (LexState *ls, int base, int line, int nvars, int isgen) {
  /* forbody -> DO block */
  static const OpCode forprep[2] = {OP_FORPREP, OP_TFORPREP};
  static const OpCode forloop[2] = {OP_FORLOOP, OP_TFORLOOP};
  BlockCnt bl;
  FuncState *fs = ls->fs;
  int prep, endfor;
  checknext(ls, TK_DO);
  prep = helloK_codeABx(fs, forprep[isgen], base, 0);
  enterblock(fs, &bl, 0);  /* scope for declared variables */
  adjustlocalvars(ls, nvars);
  helloK_reserveregs(fs, nvars);
  block(ls);
  leaveblock(fs);  /* end of scope for declared variables */
  fixforjump(fs, prep, helloK_getlabel(fs), 0);
  helloK_patchtohere(fs, bl.previous->scopeend);
  if (isgen) {  /* generic for? */
    helloK_codeABC(fs, OP_TFORCALL, base, 0, nvars);
    helloK_fixline(fs, line);
  }
  endfor = helloK_codeABx(fs, forloop[isgen], base, 0);
  fixforjump(fs, endfor, prep + 1, 1);
  helloK_fixline(fs, line);
}


static void fornum (LexState *ls, TString *varname, int line) {
  /* fornum -> NAME = exp,exp[,exp] forbody */
  FuncState *fs = ls->fs;
  int base = fs->freereg;
  new_localvarliteral(ls, "(for state)");
  new_localvarliteral(ls, "(for state)");
  new_localvarliteral(ls, "(for state)");
  new_localvar(ls, varname);
  checknext(ls, '=');
  exp1(ls);  /* initial value */
  checknext(ls, ',');
  exp1(ls);  /* limit */
  if (testnext(ls, ','))
    exp1(ls);  /* optional step */
  else {  /* default step = 1 */
    helloK_int(fs, fs->freereg, 1);
    helloK_reserveregs(fs, 1);
  }
  adjustlocalvars(ls, 3);  /* control variables */
  forbody(ls, base, line, 1, 0);
}


static void forlist (LexState *ls, TString *indexname) {
  /* forlist -> NAME {,NAME} IN explist forbody */
  FuncState *fs = ls->fs;
  expdesc e;
  int nvars = 5;  /* gen, state, control, toclose, 'indexname' */
  int line;
  int base = fs->freereg;
  /* create control variables */
  new_localvarliteral(ls, "(for state)");
  new_localvarliteral(ls, "(for state)");
  new_localvarliteral(ls, "(for state)");
  new_localvarliteral(ls, "(for state)");
  /* create declared variables */
  new_localvar(ls, indexname);
  while (testnext(ls, ',')) {
    new_localvar(ls, str_checkname(ls));
    nvars++;
  }
  checknext(ls, TK_IN);
  line = ls->getLineNumber();
  adjust_assign(ls, 4, explist(ls, &e), &e);
  adjustlocalvars(ls, 4);  /* control variables */
  marktobeclosed(fs);  /* last control var. must be closed */
  helloK_checkstack(fs, 3);  /* extra space to call generator */
  forbody(ls, base, line, nvars - 4, 1);
}


static void forvlist (LexState *ls, TString *valname) {
  /* forvlist -> explist AS NAME forbody */
  FuncState *fs = ls->fs;
  expdesc e;
  int nvars = 5;  /* gen, state, control, toclose, 'indexname' */
  int line;
  int base = fs->freereg;
  /* create control variables */
  new_localvarliteral(ls, "(for state)");
  new_localvarliteral(ls, "(for state)");
  new_localvarliteral(ls, "(for state)");
  new_localvarliteral(ls, "(for state)");
  /* create variable for key */
  new_localvar(ls, helloS_newliteral(ls->L, "(for state)"));
  /* create variable for value */
  new_localvar(ls, valname);
  nvars++;

  line = ls->getLineNumber();
  adjust_assign(ls, 4, explist(ls, &e), &e);
  adjustlocalvars(ls, 4);  /* control variables */
  marktobeclosed(fs);  /* last control var. must be closed */
  helloK_checkstack(fs, 3);  /* extra space to call generator */

  checknext(ls, TK_AS);
  helloX_next(ls); /* skip valname */

  forbody(ls, base, line, nvars - 4, 1);
}


static void forstat (LexState *ls, int line) {
  /* forstat -> FOR (fornum | forlist) END */
  FuncState *fs = ls->fs;
  TString *varname = nullptr;
  BlockCnt bl;
  enterblock(fs, &bl, 1);  /* scope for loop and control variables */
  helloX_next(ls);  /* skip 'for' */

  /* determine if this is a for-as loop */
  auto sp = helloX_getpos(ls);
  for (; ls->t.token != TK_IN && ls->t.token != TK_DO && ls->t.token != TK_EOS; helloX_next(ls)) {
    if (ls->t.token == TK_AS) {
      helloX_next(ls);
      varname = str_checkname(ls);
      break;
    }
  }
  helloX_setpos(ls, sp);

  if (varname == nullptr) {
    varname = str_checkname(ls);  /* first variable name */
    switch (ls->t.token) {
      case '=': {
        fornum(ls, varname, line);
        break;
      }
      case ',': case TK_IN: {
        forlist(ls, varname);
        break;
      }
      default: {
        helloX_syntaxerror(ls, "'=' or 'in' expected");
      }
    }
  }
  else {
    forvlist(ls, varname);
  }
  check_match(ls, TK_END, TK_FOR, line);
  leaveblock(fs);  /* loop scope ('break' jumps to this point) */
}


static void test_then_block (LexState *ls, int *escapelist, TypeDesc *prop) {
  /* test_then_block -> [IF | ELSEIF] cond THEN block */
  BlockCnt bl;
  FuncState *fs = ls->fs;
  expdesc v;
  int jf;  /* instruction to skip 'then' code (if condition is false) */
  helloX_next(ls);  /* skip IF or ELSEIF */
  expr(ls, &v);  /* read condition */
  if (v.k == VNIL || v.k == VFALSE)
    throw_warn(ls, "unreachable code", "this condition will never be truthy.", ls->getLineNumber(), UNREACHABLE_CODE);
  checknext(ls, TK_THEN);
  if (ls->t.token == TK_BREAK && helloX_lookahead(ls) != TK_INT) {  /* 'if x then break' and not 'if x then break int' ? */
    ls->laststat.token = TK_BREAK;
    int line = ls->getLineNumber();
    helloK_goiffalse(ls->fs, &v);  /* will jump if condition is true */
    helloX_next(ls);  /* skip 'break' */
    enterblock(fs, &bl, 0);  /* must enter block before 'goto' */
    newgotoentry(ls, helloS_newliteral(ls->L, "break"), line, v.t);
    while (testnext(ls, ';')) {}  /* skip semicolons */
    if (block_follow(ls, 0)) {  /* jump is the entire block? */
      leaveblock(fs);
      return;  /* and that is it */
    }
    else  /* must skip over 'then' part if condition is false */
      jf = helloK_jump(fs);
  }
  else {  /* regular case (not a break) */
    helloK_goiftrue(ls->fs, &v);  /* skip over block if condition is false */
    enterblock(fs, &bl, 0);
    jf = v.f;
  }
  statlist(ls, prop);  /* 'then' part */
  leaveblock(fs);
  if (ls->t.token == TK_ELSE ||
      ls->t.token == TK_ELSEIF)  /* followed by 'else'/'elseif'? */
    helloK_concat(fs, escapelist, helloK_jump(fs));  /* must jump over it */
  helloK_patchtohere(fs, jf);
}


static void ifstat (LexState *ls, int line, TypeDesc *prop = nullptr) {
  /* ifstat -> IF cond THEN block {ELSEIF cond THEN block} [ELSE block] END */
  FuncState *fs = ls->fs;
  int escapelist = NO_JUMP;  /* exit list for finished parts */
  test_then_block(ls, &escapelist, prop);  /* IF cond THEN block */
  while (ls->t.token == TK_ELSEIF)
    test_then_block(ls, &escapelist, prop);  /* ELSEIF cond THEN block */
  if (testnext(ls, TK_ELSE))
    block(ls);  /* 'else' part */
  check_match(ls, TK_END, TK_IF, line);
  helloK_patchtohere(fs, escapelist);  /* patch escape list to 'if' end */
}


static void localfunc (LexState *ls) {
  expdesc b;
  FuncState *fs = ls->fs;
  int fvar = fs->nactvar;  /* function's variable index */
  new_localvar(ls, str_checkname(ls, true));  /* new local variable */
  adjustlocalvars(ls, 1);  /* enter its scope */
  TypeDesc funcdesc;
  body(ls, &b, 0, ls->getLineNumber(), &funcdesc);  /* function created in next register */
  getlocalvardesc(fs, fvar)->vd.prop = std::move(funcdesc);
  /* debug information will only see the variable after this point! */
  localdebuginfo(fs, fvar)->startpc = fs->pc;
}


static int getlocalattribute (LexState *ls) {
  /* ATTRIB -> ['<' Name '>'] */
  if (testnext(ls, '<')) {
    const char *attr = getstr(str_checkname(ls));
    checknext(ls, '>');
    if (strcmp(attr, "const") == 0)
      return RDKCONST;  /* read-only variable */
    else if (strcmp(attr, "close") == 0)
      return RDKTOCLOSE;  /* to-be-closed variable */
    else {
      helloX_prev(ls); // back to '>'
      helloX_prev(ls); // back to attribute
      helloK_semerror(ls,
        helloO_pushfstring(ls->L, "unknown attribute '%s'", attr));
    }
  }
  return VDKREG;  /* regular variable */
}


static void checktoclose (FuncState *fs, int level) {
  if (level != -1) {  /* is there a to-be-closed variable? */
    marktobeclosed(fs);
    helloK_codeABC(fs, OP_TBC, reglevel(fs, level), 0, 0);
  }
}


static void localstat (LexState *ls) {
  /* stat -> LOCAL NAME ATTRIB { ',' NAME ATTRIB } ['=' explist] */
  FuncState *fs = ls->fs;
  int toclose = -1;  /* index of to-be-closed variable (if any) */
  Vardesc *var;  /* last variable */
  int vidx, kind;  /* index and kind of last variable */
  TypeDesc hint = VT_DUNNO;
  int nvars = 0;
  int nexps;
  expdesc e;
  int line = ls->getLineNumber(); /* in case we need to emit a warning */
  size_t starting_tidx = ls->tidx; /* code snippets on tuple assignments can have inaccurate line readings because the parser skips lines until it can close the statement */
  do {
    vidx = new_localvar(ls, str_checkname(ls, true), line);
    hint = gettypehint(ls);
    kind = getlocalattribute(ls);
    var = getlocalvardesc(fs, vidx);
    var->vd.kind = kind;
    var->vd.hint = hint;
    if (kind == RDKTOCLOSE) {  /* to-be-closed? */
      if (toclose != -1) { /* one already present? */
        helloX_setpos(ls, starting_tidx);
        helloK_semerror(ls, "multiple to-be-closed variables in local list");
      }
      toclose = fs->nactvar + nvars;
    }
    nvars++;
  } while (testnext(ls, ','));
  std::vector<TypeDesc> tds;
  if (testnext(ls, '=')) {
    ParserContext ctx = ((nvars == 1) ? PARCTX_CREATE_VAR : PARCTX_CREATE_VARS);
    ls->pushContext(ctx);
    nexps = explist(ls, &e, tds);
    ls->popContext(ctx);
  }
  else {
    e.k = VVOID;
    nexps = 0;
    process_assign(ls, var, VT_NIL, line);
  }
  if (nvars == nexps) { /* no adjustments? */
    if (var->vd.kind == RDKCONST &&  /* last variable is const? */
        helloK_exp2const(fs, &e, &var->k)) {  /* compile-time constant? */
      var->vd.kind = RDKCTC;  /* variable is a compile-time constant */
      adjustlocalvars(ls, nvars - 1);  /* exclude last variable */
      fs->nactvar++;  /* but count it */
    }
    else {
      vidx = vidx - nvars + 1;
      for (TypeDesc& td : tds) {
        exp_propagate(ls, e, td);
        process_assign(ls, getlocalvardesc(fs, vidx), td, line);
        ++vidx;
      }
      adjust_assign(ls, nvars, nexps, &e);
      adjustlocalvars(ls, nvars);
    }
  }
  else {
    adjust_assign(ls, nvars, nexps, &e);
    adjustlocalvars(ls, nvars);
  }
  checktoclose(fs, toclose);
}


static int funcname (LexState *ls, expdesc *v) {
  /* funcname -> NAME {fieldsel} [':' NAME] */
  int ismethod = 0;
  singlevar(ls, v);
  while (ls->t.token == '.')
    fieldsel(ls, v);
  if (ls->t.token == ':') {
    ismethod = 1;
    fieldsel(ls, v);
  }
  return ismethod;
}


static void funcstat (LexState *ls, int line) {
  /* funcstat -> FUNCTION funcname body */
  int ismethod;
  expdesc v, b;
  helloX_next(ls);  /* skip FUNCTION */
  ismethod = funcname(ls, &v);
  body(ls, &b, ismethod, line);
  check_readonly(ls, &v);
  helloK_storevar(ls->fs, &v, &b);
  helloK_fixline(ls->fs, line);  /* definition "happens" in the first line */
}


static void exprstat (LexState *ls) {
  /* stat -> func | assignment */
  FuncState *fs = ls->fs;
  struct LHS_assign v;
  suffixedexp(ls, &v.v);
  if (ls->t.token == '=' || ls->t.token == ',') { /* stat -> assignment ? */
    v.prev = NULL;
    restassign(ls, &v, 1);
  }
  else {  /* stat -> func */
    Instruction *inst;
    check_condition(ls, v.v.k == VCALL, "syntax error");
    inst = &getinstruction(fs, &v.v);
    SETARG_C(*inst, 1);  /* call statement uses no results */
  }
}


static void retstat (LexState *ls, TypeDesc *prop) {
  /* stat -> RETURN [explist] [';'] */
  FuncState *fs = ls->fs;
  expdesc e;
  int nret;  /* number of values being returned */
  int first = helloY_nvarstack(fs);  /* first slot to be returned */
  if (block_follow(ls, 1) || ls->t.token == ';'
    || ls->t.token == TK_CASE || ls->t.token == TK_DEFAULT
    || ls->t.token == TK_PCASE || ls->t.token == TK_PDEFAULT
  ) {
    nret = 0;  /* return no values */
    if (prop) *prop = VT_NIL;
  }
  else {
    nret = explist(ls, &e, prop);  /* optional return values */
    if (hasmultret(e.k)) {
      helloK_setmultret(fs, &e);
      if (e.k == VCALL && nret == 1 && !fs->bl->insidetbc) {  /* tail call? */
        SET_OPCODE(getinstruction(fs,&e), OP_TAILCALL);
        hello_assert(GETARG_A(getinstruction(fs,&e)) == helloY_nvarstack(fs));
      }
      nret = HELLO_MULTRET;  /* return all values */
    }
    else {
      if (nret == 1)  /* only one single value? */
        first = helloK_exp2anyreg(fs, &e);  /* can use original slot */
      else {  /* values must go to the top of the stack */
        helloK_exp2nextreg(fs, &e);
        hello_assert(nret == fs->freereg - first);
      }
    }
  }
  helloK_ret(fs, first, nret);
  testnext(ls, ';');  /* skip optional semicolon */
}


static void statement (LexState *ls, TypeDesc *prop) {
  int line = ls->getLineNumber();
  if (ls->laststat.IsEscapingToken() ||
     (ls->laststat.Is(TK_GOTO) && !ls->findWithinLine(line, helloX_lookbehind(ls).seminfo.ts->toCpp()))) /* Don't warn if this statement is the goto's label. */
  {
    throw_warn(ls,
      "unreachable code",
        helloO_fmt(ls->L, "this code comes after an escaping %s statement.", helloX_token2str(ls, ls->laststat.token)), UNREACHABLE_CODE);
    ls->L->top -= 2;
  }
  ls->laststat.token = ls->t.token;
  enterlevel(ls);
  switch (ls->t.token) {
    case ';': {  /* stat -> ';' (empty statement) */
      helloX_next(ls);  /* skip ';' */
      break;
    }
    case TK_IF: {  /* stat -> ifstat */
      ifstat(ls, line, prop);
      break;
    }
    case TK_WHILE: {  /* stat -> whilestat */
      whilestat(ls, line);
      break;
    }
    case TK_DO: {  /* stat -> DO block END */
      helloX_next(ls);  /* skip DO */
      block(ls);
      check_match(ls, TK_END, TK_DO, line);
      break;
    }
    case TK_FOR: {  /* stat -> forstat */
      forstat(ls, line);
      break;
    }
    case TK_REPEAT: {  /* stat -> repeatstat */
      repeatstat(ls);
      break;
    }
    case TK_FUNCTION: {  /* stat -> funcstat */
      funcstat(ls, line);
      break;
    }
    case TK_LOCAL: {  /* stat -> localstat */
      helloX_next(ls);  /* skip LOCAL */
      if (testnext(ls, TK_FUNCTION))  /* local function? */
        localfunc(ls);
      else
        localstat(ls);
      break;
    }
    case TK_DBCOLON: {  /* stat -> label */
      helloX_next(ls);  /* skip double colon */
      labelstat(ls, str_checkname(ls), line);
      break;
    }
    case TK_RETURN: {  /* stat -> retstat */
      helloX_next(ls);  /* skip RETURN */
      retstat(ls, prop);
      break;
    }
    case TK_BREAK: {  /* stat -> breakstat */
      breakstat(ls);
      break;
    }
#ifndef HELLO_COMPATIBLE_CONTINUE
    case TK_CONTINUE:
#endif
    case TK_PCONTINUE: {
      continuestat(ls);
      break;
    }
    case TK_GOTO: {  /* stat -> 'goto' NAME */
      helloX_next(ls);  /* skip 'goto' */
      gotostat(ls);
      break;
    }
    case TK_CASE:
    case TK_PCASE: {
      throwerr(ls, "inappropriate 'case' statement.", "outside of 'switch' block.");
    }
    case TK_DEFAULT:
    case TK_PDEFAULT: {
      throwerr(ls, "inappropriate 'default' statement.", "outside of 'switch' block.");
    }
#ifndef HELLO_COMPATIBLE_SWITCH
    case TK_SWITCH:
#endif
    case TK_PSWITCH: {
      switchstat(ls, line);
      break;
    }
#ifndef HELLO_COMPATIBLE_ENUM
    case TK_ENUM:
#endif
    case TK_PENUM: {
      enumstat(ls);
      break;
    }
    default: {  /* stat -> func | assignment */
      exprstat(ls);
      break;
    }
  }
  hello_assert(ls->fs->f->maxstacksize >= ls->fs->freereg &&
             ls->fs->freereg >= helloY_nvarstack(ls->fs));
  ls->fs->freereg = helloY_nvarstack(ls->fs);  /* free registers */
  leavelevel(ls);
}

/* }====================================================================== */


/*
** compiles the main function, which is a regular vararg function with an
** upvalue named HELLO_ENV
*/
static void mainfunc (LexState *ls, FuncState *fs) {
  BlockCnt bl;
  Upvaldesc *env;
  open_func(ls, fs, &bl);
  setvararg(fs, 0);  /* main function is always declared vararg */
  env = allocupvalue(fs);  /* ...set environment upvalue */
  env->instack = 1;
  env->idx = 0;
  env->kind = VDKREG;
  env->name = ls->envn;
  helloC_objbarrier(ls->L, fs->f, env->name);
  helloX_next(ls);  /* read first token */
  statlist(ls);  /* parse main body */
  check(ls, TK_EOS);
  close_func(ls);
}


LClosure *helloY_parser (hello_State *L, ZIO *z, Mbuffer *buff,
                       Dyndata *dyd, const char *name, int firstchar) {
  LexState lexstate;
  FuncState funcstate;
  LClosure *cl = helloF_newLclosure(L, 1);  /* create main closure */
  setclLvalue2s(L, L->top, cl);  /* anchor it (to avoid being collected) */
  helloD_inctop(L);
  lexstate.h = helloH_new(L);  /* create table for scanner */
  sethvalue2s(L, L->top, lexstate.h);  /* anchor it */
  helloD_inctop(L);
  funcstate.f = cl->p = helloF_newproto(L);
  helloC_objbarrier(L, cl, cl->p);
  funcstate.f->source = helloS_new(L, name);  /* create and anchor TString */
  helloC_objbarrier(L, funcstate.f, funcstate.f->source);
  lexstate.buff = buff;
  lexstate.dyd = dyd;
  dyd->actvar.n = dyd->gt.n = dyd->label.n = 0;
  helloX_setinput(L, &lexstate, z, funcstate.f->source, firstchar);
  mainfunc(&lexstate, &funcstate);
  hello_assert(!funcstate.prev && funcstate.nups == 1 && !lexstate.fs);
  /* all scopes should be correctly finished */
  hello_assert(dyd->actvar.n == 0 && dyd->gt.n == 0 && dyd->label.n == 0);
  L->top--;  /* remove scanner's table */
  return cl;  /* closure is on the stack, too */
}