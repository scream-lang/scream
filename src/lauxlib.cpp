/*
** $Id: lauxlib.c $
** Auxiliary functions for building Mask libraries
** See Copyright Notice in mask.h
*/

#define lauxlib_c
#define MASK_LIB

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <Windows.h>
#endif


/*
** This file uses only the official API of Mask.
** Any function declared here could be written as an application function.
*/

#include "mask.h"
#include "lprefix.h"
#include "lauxlib.h"


#if !defined(MAX_SIZET)
/* maximum value for size_t */
#define MAX_SIZET	((size_t)(~(size_t)0))
#endif


/*
** {======================================================
** Traceback
** =======================================================
*/


#define LEVELS1	10	/* size of the first part of the stack */
#define LEVELS2	11	/* size of the second part of the stack */



/*
** Search for 'objidx' in table at index -1. ('objidx' must be an
** absolute index.) Return 1 + string at top if it found a good name.
*/
static int findfield (mask_State *L, int objidx, int level) {
  if (level == 0 || !mask_istable(L, -1))
    return 0;  /* not found */
  mask_pushnil(L);  /* start 'next' loop */
  while (mask_next(L, -2)) {  /* for each pair in table */
    if (mask_type(L, -2) == MASK_TSTRING) {  /* ignore non-string keys */
      if (mask_rawequal(L, objidx, -1)) {  /* found object? */
        mask_pop(L, 1);  /* remove value (but keep name) */
        return 1;
      }
      else if (findfield(L, objidx, level - 1)) {  /* try recursively */
        /* stack: lib_name, lib_table, field_name (top) */
        mask_pushliteral(L, ".");  /* place '.' between the two names */
        mask_replace(L, -3);  /* (in the slot occupied by table) */
        mask_concat(L, 3);  /* lib_name.field_name */
        return 1;
      }
    }
    mask_pop(L, 1);  /* remove value */
  }
  return 0;  /* not found */
}


/*
** Search for a name for a function in all loaded modules
*/
static int pushglobalfuncname (mask_State *L, mask_Debug *ar) {
  int top = mask_gettop(L);
  mask_getinfo(L, "f", ar);  /* push function */
  mask_getfield(L, MASK_REGISTRYINDEX, MASK_LOADED_TABLE);
  if (findfield(L, top + 1, 2)) {
    const char *name = mask_tostring(L, -1);
    if (strncmp(name, MASK_GNAME ".", 3) == 0) {  /* name start with '_G.'? */
      mask_pushstring(L, name + 3);  /* push name without prefix */
      mask_remove(L, -2);  /* remove original name */
    }
    mask_copy(L, -1, top + 1);  /* copy name to proper place */
    mask_settop(L, top + 1);  /* remove table "loaded" and name copy */
    return 1;
  }
  else {
    mask_settop(L, top);  /* remove function and global table */
    return 0;
  }
}


static void pushfuncname (mask_State *L, mask_Debug *ar) {
  if (pushglobalfuncname(L, ar)) {  /* try first a global name */
    mask_pushfstring(L, "function '%s'", mask_tostring(L, -1));
    mask_remove(L, -2);  /* remove name */
  }
  else if (*ar->namewhat != '\0')  /* is there a name from code? */
    mask_pushfstring(L, "%s '%s'", ar->namewhat, ar->name);  /* use it */
  else if (*ar->what == 'm')  /* main? */
      mask_pushliteral(L, "main chunk");
  else if (*ar->what != 'C')  /* for Mask functions, use <file:line> */
    mask_pushfstring(L, "function <%s:%d>", ar->short_src, ar->linedefined);
  else  /* nothing left... */
    mask_pushliteral(L, "?");
}


static int lastlevel (mask_State *L) {
  mask_Debug ar;
  int li = 1, le = 1;
  /* find an upper bound */
  while (mask_getstack(L, le, &ar)) { li = le; le *= 2; }
  /* do a binary search */
  while (li < le) {
    int m = (li + le)/2;
    if (mask_getstack(L, m, &ar)) li = m + 1;
    else le = m;
  }
  return le - 1;
}


MASKLIB_API void maskL_traceback (mask_State *L, mask_State *L1,
                                const char *msg, int level) {
  maskL_Buffer b;
  mask_Debug ar;
  int last = lastlevel(L1);
  int limit2show = (last - level > LEVELS1 + LEVELS2) ? LEVELS1 : -1;
  maskL_buffinit(L, &b);
  if (msg) {
    maskL_addstring(&b, msg);
    maskL_addchar(&b, '\n');
  }
  maskL_addstring(&b, "stack traceback:");
  while (mask_getstack(L1, level++, &ar)) {
    if (limit2show-- == 0) {  /* too many levels? */
      int n = last - level - LEVELS2 + 1;  /* number of levels to skip */
      mask_pushfstring(L, "\n\t...\t(skipping %d levels)", n);
      maskL_addvalue(&b);  /* add warning about skip */
      level += n;  /* and skip to last levels */
    }
    else {
      mask_getinfo(L1, "Slnt", &ar);
      if (ar.currentline <= 0)
        mask_pushfstring(L, "\n\t%s: in ", ar.short_src);
      else
        mask_pushfstring(L, "\n\t%s:%d: in ", ar.short_src, ar.currentline);
      maskL_addvalue(&b);
      pushfuncname(L, &ar);
      maskL_addvalue(&b);
      if (ar.istailcall)
        maskL_addstring(&b, "\n\t(...tail calls...)");
    }
  }
  maskL_pushresult(&b);
}

/* }====================================================== */


/*
** {======================================================
** Error-report functions
** =======================================================
*/

MASKLIB_API void maskL_argerror (mask_State *L, int arg, const char *extramsg) {
  mask_Debug ar;
  if (!mask_getstack(L, 0, &ar))  /* no stack frame? */
    maskL_error(L, "bad argument #%d (%s)", arg, extramsg);
  mask_getinfo(L, "n", &ar);
  if (strcmp(ar.namewhat, "method") == 0) {
    arg--;  /* do not count 'self' */
    if (arg == 0)  /* error is in the self argument itself? */
      maskL_error(L, "calling '%s' on bad self (%s)",
                           ar.name, extramsg);
  }
  if (ar.name == NULL)
    ar.name = (pushglobalfuncname(L, &ar)) ? mask_tostring(L, -1) : "?";
  maskL_error(L, "bad argument #%d to '%s' (%s)",
                        arg, ar.name, extramsg);
}


MASKLIB_API void maskL_typeerror (mask_State *L, int arg, const char *tname) {
  const char *msg;
  const char *typearg;  /* name for the type of the actual argument */
  if (maskL_getmetafield(L, arg, "__name") == MASK_TSTRING)
    typearg = mask_tostring(L, -1);  /* use the given type name */
  else if (mask_type(L, arg) == MASK_TLIGHTUSERDATA)
    typearg = "light userdata";  /* special name for messages */
  else
    typearg = maskL_typename(L, arg);  /* standard name */
  msg = mask_pushfstring(L, "%s expected, got %s", tname, typearg);
  maskL_argerror(L, arg, msg);
}


[[noreturn]] static void tag_error (mask_State *L, int arg, int tag) {
  maskL_typeerror(L, arg, mask_typename(L, tag));
}


/*
** The use of 'mask_pushfstring' ensures this function does not
** need reserved stack space when called.
*/
MASKLIB_API void maskL_where (mask_State *L, int level) {
  mask_Debug ar;
  if (mask_getstack(L, level, &ar)) {  /* check function at level */
    mask_getinfo(L, "Sl", &ar);  /* get info about it */
    if (ar.currentline > 0) {  /* is there info? */
      mask_pushfstring(L, "%s:%d: ", ar.short_src, ar.currentline);
      return;
    }
  }
  mask_pushfstring(L, "");  /* else, no information available... */
}


/*
** Again, the use of 'mask_pushvfstring' ensures this function does
** not need reserved stack space when called. (At worst, it generates
** an error with "stack overflow" instead of the given message.)
*/
MASKLIB_API void maskL_error (mask_State *L, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  maskL_where(L, 1);
  mask_pushvfstring(L, fmt, argp);
  va_end(argp);
  mask_concat(L, 2);
  mask_error(L);
}


MASKLIB_API int maskL_fileresult (mask_State *L, int stat, const char *fname) {
  int en = errno;  /* calls to Mask API may change this value */
  if (stat) {
    mask_pushboolean(L, 1);
    return 1;
  }
  else {
    maskL_pushfail(L);
    if (fname)
      mask_pushfstring(L, "%s: %s", fname, strerror(en));
    else
      mask_pushstring(L, strerror(en));
    mask_pushinteger(L, en);
    return 3;
  }
}


#if !defined(l_inspectstat)	/* { */

#if defined(MASK_USE_POSIX)

#include <sys/wait.h>

/*
** use appropriate macros to interpret 'pclose' return status
*/
#define l_inspectstat(stat,what)  \
   if (WIFEXITED(stat)) { stat = WEXITSTATUS(stat); } \
   else if (WIFSIGNALED(stat)) { stat = WTERMSIG(stat); what = "signal"; }

#else

#define l_inspectstat(stat,what)  /* no op */

#endif

#endif				/* } */


MASKLIB_API int maskL_execresult (mask_State *L, int stat) {
  if (stat != 0 && errno != 0)  /* error with an 'errno'? */
    return maskL_fileresult(L, 0, NULL);
  else {
    const char *what = "exit";  /* type of termination */
    l_inspectstat(stat, what);  /* interpret result */
    if (*what == 'e' && stat == 0)  /* successful termination? */
      mask_pushboolean(L, 1);
    else
      maskL_pushfail(L);
    mask_pushstring(L, what);
    mask_pushinteger(L, stat);
    return 3;  /* return true/fail,what,code */
  }
}

/* }====================================================== */



/*
** {======================================================
** Userdata's metatable manipulation
** =======================================================
*/

MASKLIB_API int maskL_newmetatable (mask_State *L, const char *tname) {
  if (maskL_getmetatable(L, tname) != MASK_TNIL)  /* name already in use? */
    return 0;  /* leave previous value on top, but return 0 */
  mask_pop(L, 1);
  mask_createtable(L, 0, 2);  /* create metatable */
  mask_pushstring(L, tname);
  mask_setfield(L, -2, "__name");  /* metatable.__name = tname */
  mask_pushvalue(L, -1);
  mask_setfield(L, MASK_REGISTRYINDEX, tname);  /* registry.name = metatable */
  return 1;
}


MASKLIB_API void maskL_setmetatable (mask_State *L, const char *tname) {
  maskL_getmetatable(L, tname);
  mask_setmetatable(L, -2);
}


MASKLIB_API void *maskL_testudata (mask_State *L, int ud, const char *tname) {
  void *p = mask_touserdata(L, ud);
  if (p != NULL) {  /* value is a userdata? */
    if (mask_getmetatable(L, ud)) {  /* does it have a metatable? */
      maskL_getmetatable(L, tname);  /* get correct metatable */
      if (!mask_rawequal(L, -1, -2))  /* not the same? */
        p = NULL;  /* value is a userdata with wrong metatable */
      mask_pop(L, 2);  /* remove both metatables */
      return p;
    }
  }
  return NULL;  /* value is not a userdata with a metatable */
}


MASKLIB_API void *maskL_checkudata (mask_State *L, int ud, const char *tname) {
  void *p = maskL_testudata(L, ud, tname);
  maskL_argexpected(L, p != NULL, ud, tname);
  return p;
}

/* }====================================================== */


/*
** {======================================================
** Argument check functions
** =======================================================
*/

MASKLIB_API int maskL_checkoption (mask_State *L, int arg, const char *def,
                                 const char *const lst[]) {
  const char *name = (def) ? maskL_optstring(L, arg, def) :
                             maskL_checkstring(L, arg);
  int i;
  for (i=0; lst[i]; i++)
    if (strcmp(lst[i], name) == 0)
      return i;
  maskL_argerror(L, arg, mask_pushfstring(L, "invalid option '%s'", name));
}


/*
** Ensures the stack has at least 'space' extra slots, raising an error
** if it cannot fulfill the request. (The error handling needs a few
** extra slots to format the error message. In case of an error without
** this extra space, Mask will generate the same 'stack overflow' error,
** but without 'msg'.)
*/
MASKLIB_API void maskL_checkstack (mask_State *L, int space, const char *msg) {
  if (l_unlikely(!mask_checkstack(L, space))) {
    if (msg)
      maskL_error(L, "stack overflow (%s)", msg);
    else
      maskL_error(L, "stack overflow");
  }
}


MASKLIB_API void maskL_checktype (mask_State *L, int arg, int t) {
  if (l_unlikely(mask_type(L, arg) != t))
    tag_error(L, arg, t);
}


MASKLIB_API void maskL_checkany (mask_State *L, int arg) {
  if (l_unlikely(mask_type(L, arg) == MASK_TNONE))
    maskL_argerror(L, arg, "value expected");
}


MASKLIB_API const char *maskL_checklstring (mask_State *L, int arg, size_t *len) {
  const char *s = mask_tolstring(L, arg, len);
  if (l_unlikely(!s)) tag_error(L, arg, MASK_TSTRING);
  return s;
}


MASKLIB_API const char *maskL_optlstring (mask_State *L, int arg,
                                        const char *def, size_t *len) {
  if (mask_isnoneornil(L, arg)) {
    if (len)
      *len = (def ? strlen(def) : 0);
    return def;
  }
  else return maskL_checklstring(L, arg, len);
}


MASKLIB_API mask_Number maskL_checknumber (mask_State *L, int arg) {
  int isnum;
  mask_Number d = mask_tonumberx(L, arg, &isnum);
  if (l_unlikely(!isnum))
    tag_error(L, arg, MASK_TNUMBER);
  return d;
}


MASKLIB_API mask_Number maskL_optnumber (mask_State *L, int arg, mask_Number def) {
  return maskL_opt(L, maskL_checknumber, arg, def);
}


[[noreturn]] static void interror (mask_State *L, int arg) {
  if (mask_isnumber(L, arg))
    maskL_argerror(L, arg, "number has no integer representation");
  else
    tag_error(L, arg, MASK_TNUMBER);
}


MASKLIB_API mask_Integer maskL_checkinteger (mask_State *L, int arg) {
  int isnum;
  mask_Integer d = mask_tointegerx(L, arg, &isnum);
  if (l_unlikely(!isnum)) {
    interror(L, arg);
  }
  return d;
}


MASKLIB_API mask_Integer maskL_optinteger (mask_State *L, int arg,
                                                      mask_Integer def) {
  return maskL_opt(L, maskL_checkinteger, arg, def);
}

/* }====================================================== */


/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

/* userdata to box arbitrary data */
typedef struct UBox {
  void *box;
  size_t bsize;
} UBox;


static void *resizebox (mask_State *L, int idx, size_t newsize) {
  void *ud;
  mask_Alloc allocf = mask_getallocf(L, &ud);
  UBox *box = (UBox *)mask_touserdata(L, idx);
  void *temp = allocf(ud, box->box, box->bsize, newsize);
  if (l_unlikely(temp == NULL && newsize > 0)) {  /* allocation error? */
    mask_pushliteral(L, "not enough memory");
    mask_error(L);  /* raise a memory error */
  }
  box->box = temp;
  box->bsize = newsize;
  return temp;
}


static int boxgc (mask_State *L) {
  resizebox(L, 1, 0);
  return 0;
}


static const maskL_Reg boxmt[] = {  /* box metamethods */
  {"__gc", boxgc},
  {"__close", boxgc},
  {NULL, NULL}
};


static void newbox (mask_State *L) {
  UBox *box = (UBox *)mask_newuserdatauv(L, sizeof(UBox), 0);
  box->box = NULL;
  box->bsize = 0;
  if (maskL_newmetatable(L, "_UBOX*"))  /* creating metatable? */
    maskL_setfuncs(L, boxmt, 0);  /* set its metamethods */
  mask_setmetatable(L, -2);
}


/*
** check whether buffer is using a userdata on the stack as a temporary
** buffer
*/
#define buffonstack(B)	((B)->b != (B)->init.b)


/*
** Whenever buffer is accessed, slot 'idx' must either be a box (which
** cannot be NULL) or it is a placeholder for the buffer.
*/
#define checkbufferlevel(B,idx)  \
  mask_assert(buffonstack(B) ? mask_touserdata(B->L, idx) != NULL  \
                            : mask_touserdata(B->L, idx) == (void*)B)


/*
** Compute new size for buffer 'B', enough to accommodate extra 'sz'
** bytes. (The test for "not big enough" also gets the case when the
** computation of 'newsize' overflows.)
*/
static size_t newbuffsize (maskL_Buffer *B, size_t sz) {
  size_t newsize = (B->size / 2) * 3;  /* buffer size * 1.5 */
  if (l_unlikely(MAX_SIZET - sz < B->n))  /* overflow in (B->n + sz)? */
    maskL_error(B->L, "buffer too large");
  if (newsize < B->n + sz)  /* not big enough? */
    newsize = B->n + sz;
  return newsize;
}


/*
** Returns a pointer to a free area with at least 'sz' bytes in buffer
** 'B'. 'boxidx' is the relative position in the stack where is the
** buffer's box or its placeholder.
*/
static char *prepbuffsize (maskL_Buffer *B, size_t sz, int boxidx) {
  checkbufferlevel(B, boxidx);
  if (B->size - B->n >= sz)  /* enough space? */
    return B->b + B->n;
  else {
    mask_State *L = B->L;
    char *newbuff;
    size_t newsize = newbuffsize(B, sz);
    /* create larger buffer */
    if (buffonstack(B))  /* buffer already has a box? */
      newbuff = (char *)resizebox(L, boxidx, newsize);  /* resize it */
    else {  /* no box yet */
      mask_remove(L, boxidx);  /* remove placeholder */
      newbox(L);  /* create a new box */
      mask_insert(L, boxidx);  /* move box to its intended position */
      mask_toclose(L, boxidx);
      newbuff = (char *)resizebox(L, boxidx, newsize);
      memcpy(newbuff, B->b, B->n * sizeof(char));  /* copy original content */
    }
    B->b = newbuff;
    B->size = newsize;
    return newbuff + B->n;
  }
}

/*
** returns a pointer to a free area with at least 'sz' bytes
*/
MASKLIB_API char *maskL_prepbuffsize (maskL_Buffer *B, size_t sz) {
  return prepbuffsize(B, sz, -1);
}


MASKLIB_API void maskL_addlstring (maskL_Buffer *B, const char *s, size_t l) {
  if (l > 0) {  /* avoid 'memcpy' when 's' can be NULL */
    char *b = prepbuffsize(B, l, -1);
    memcpy(b, s, l * sizeof(char));
    maskL_addsize(B, l);
  }
}


MASKLIB_API void maskL_addstring (maskL_Buffer *B, const char *s) {
  maskL_addlstring(B, s, strlen(s));
}


MASKLIB_API void maskL_pushresult (maskL_Buffer *B) {
  mask_State *L = B->L;
  checkbufferlevel(B, -1);
  mask_pushlstring(L, B->b, B->n);
  if (buffonstack(B))
    mask_closeslot(L, -2);  /* close the box */
  mask_remove(L, -2);  /* remove box or placeholder from the stack */
}


MASKLIB_API void maskL_pushresultsize (maskL_Buffer *B, size_t sz) {
  maskL_addsize(B, sz);
  maskL_pushresult(B);
}


/*
** 'maskL_addvalue' is the only function in the Buffer system where the
** box (if existent) is not on the top of the stack. So, instead of
** calling 'maskL_addlstring', it replicates the code using -2 as the
** last argument to 'prepbuffsize', signaling that the box is (or will
** be) below the string being added to the buffer. (Box creation can
** trigger an emergency GC, so we should not remove the string from the
** stack before we have the space guaranteed.)
*/
MASKLIB_API void maskL_addvalue (maskL_Buffer *B) {
  mask_State *L = B->L;
  size_t len;
  const char *s = mask_tolstring(L, -1, &len);
  char *b = prepbuffsize(B, len, -2);
  memcpy(b, s, len * sizeof(char));
  maskL_addsize(B, len);
  mask_pop(L, 1);  /* pop string */
}


MASKLIB_API void maskL_buffinit (mask_State *L, maskL_Buffer *B) {
  B->L = L;
  B->b = B->init.b;
  B->n = 0;
  B->size = MASKL_BUFFERSIZE;
  mask_pushlightuserdata(L, (void*)B);  /* push placeholder */
}


MASKLIB_API char *maskL_buffinitsize (mask_State *L, maskL_Buffer *B, size_t sz) {
  maskL_buffinit(L, B);
  return prepbuffsize(B, sz, -1);
}

/* }====================================================== */


/*
** {======================================================
** Reference system
** =======================================================
*/

/* index of free-list header (after the predefined values) */
#define freelist	(MASK_RIDX_LAST + 1)

/*
** The previously freed references form a linked list:
** t[freelist] is the index of a first free index, or zero if list is
** empty; t[t[freelist]] is the index of the second element; etc.
*/
MASKLIB_API int maskL_ref (mask_State *L, int t) {
  int ref;
  if (mask_isnil(L, -1)) {
    mask_pop(L, 1);  /* remove from stack */
    return MASK_REFNIL;  /* 'nil' has a unique fixed reference */
  }
  t = mask_absindex(L, t);
  if (mask_rawgeti(L, t, freelist) == MASK_TNIL) {  /* first access? */
    ref = 0;  /* list is empty */
    mask_pushinteger(L, 0);  /* initialize as an empty list */
    mask_rawseti(L, t, freelist);  /* ref = t[freelist] = 0 */
  }
  else {  /* already initialized */
    mask_assert(mask_isinteger(L, -1));
    ref = (int)mask_tointeger(L, -1);  /* ref = t[freelist] */
  }
  mask_pop(L, 1);  /* remove element from stack */
  if (ref != 0) {  /* any free element? */
    mask_rawgeti(L, t, ref);  /* remove it from list */
    mask_rawseti(L, t, freelist);  /* (t[freelist] = t[ref]) */
  }
  else  /* no free elements */
    ref = (int)mask_rawlen(L, t) + 1;  /* get a new reference */
  mask_rawseti(L, t, ref);
  return ref;
}


MASKLIB_API void maskL_unref (mask_State *L, int t, int ref) {
  if (ref >= 0) {
    t = mask_absindex(L, t);
    mask_rawgeti(L, t, freelist);
    mask_assert(mask_isinteger(L, -1));
    mask_rawseti(L, t, ref);  /* t[ref] = t[freelist] */
    mask_pushinteger(L, ref);
    mask_rawseti(L, t, freelist);  /* t[freelist] = ref */
  }
}

/* }====================================================== */


/*
** {======================================================
** Load functions
** =======================================================
*/

typedef struct LoadF {
  int n;  /* number of pre-read characters */
  FILE *f;  /* file being read */
  char buff[BUFSIZ];  /* area for reading file */
} LoadF;


static const char *getF (mask_State *L, void *ud, size_t *size) {
  LoadF *lf = (LoadF *)ud;
  (void)L;  /* not used */
  if (lf->n > 0) {  /* are there pre-read characters to be read? */
    *size = lf->n;  /* return them (chars already in buffer) */
    lf->n = 0;  /* no more pre-read characters */
  }
  else {  /* read a block from file */
    /* 'fread' can return > 0 *and* set the EOF flag. If next call to
       'getF' called 'fread', it might still wait for user input.
       The next check avoids this problem. */
    if (feof(lf->f)) return NULL;
    *size = fread(lf->buff, 1, sizeof(lf->buff), lf->f);  /* read block */
  }
  return lf->buff;
}


static int errfile (mask_State *L, const char *what, int fnameindex) {
  const char *serr = strerror(errno);
  const char *filename = mask_tostring(L, fnameindex) + 1;
  mask_pushfstring(L, "cannot %s %s: %s", what, filename, serr);
  mask_remove(L, fnameindex);
  return MASK_ERRFILE;
}


/*
** Skip an optional BOM at the start of a stream. If there is an
** incomplete BOM (the first character is correct but the rest is
** not), returns the first character anyway to force an error
** (as no chunk can start with 0xEF).
*/
static int skipBOM (FILE *f) {
  int c = getc(f);  /* read first character */
  if (c == 0xEF && getc(f) == 0xBB && getc(f) == 0xBF)  /* correct BOM? */
    return getc(f);  /* ignore BOM and return next char */
  else  /* no (valid) BOM */
    return c;  /* return first character */
}


#ifdef _WIN32
std::wstring maskL_utf8_to_utf16(const char *utf8, size_t utf8_len) {
  std::wstring utf16;
  const int sizeRequired = MultiByteToWideChar(CP_UTF8, 0, utf8, (int)utf8_len, nullptr, 0);
  if (l_likely(sizeRequired != 0)) {
    utf16 = std::wstring(sizeRequired, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8, (int)utf8_len, utf16.data(), sizeRequired);
  }
  return utf16;
}

std::string maskL_utf16_to_utf8(const wchar_t *utf16, size_t utf16_len) {
  std::string utf8;
  const int sizeRequired = WideCharToMultiByte(CP_UTF8, 0, utf16, (int)utf16_len, nullptr, 0, 0, 0);
  if (l_likely(sizeRequired != 0)) {
    utf8 = std::string(sizeRequired, 0);
    WideCharToMultiByte(CP_UTF8, 0, utf16, (int)utf16_len, utf8.data(), sizeRequired, 0, 0);
  }
  return utf8;
}
#endif


MASKLIB_API FILE* (maskL_fopen) (const char *filename, size_t filename_len,
                               const char *mode, size_t mode_len) {
#ifdef _WIN32
  // From what I could gather online, UTF-8 is the path encoding convention on *nix systems,
  // so I've ultimately decided that we should just "fix" the fact that Windows doesn't use UTF-8.
  std::wstring wfilename = maskL_utf8_to_utf16(filename, filename_len);
  std::wstring wmode = maskL_utf8_to_utf16(mode, mode_len);
  return _wfopen(wfilename.c_str(), wmode.c_str());
#else
  return fopen(filename, mode);
#endif
}


/*
** reads the first character of file 'f' and skips an optional BOM mark
** in its beginning plus its first line if it starts with '#'. Returns
** true if it skipped the first line.  In any case, '*cp' has the
** first "valid" character of the file (after the optional BOM and
** a first-line comment).
*/
static int skipcomment (FILE *f, int *cp) {
  int c = *cp = skipBOM(f);
  if (c == '#') {  /* first line is a comment (Unix exec. file)? */
    do {  /* skip first line */
      c = getc(f);
    } while (c != EOF && c != '\n');
    *cp = getc(f);  /* next character after comment, if present */
    return 1;  /* there was a comment */
  }
  else return 0;  /* no comment */
}

#ifdef MASK_LOADFILE_HOOK
extern "C" bool MASK_LOADFILE_HOOK(const char* filename);
#endif

MASKLIB_API int maskL_loadfilex (mask_State *L, const char *filename,
                                             const char *mode) {
#ifdef MASK_LOADFILE_HOOK
  if (!MASK_LOADFILE_HOOK(filename)) {
    mask_pushfstring(L, "%s failed content moderation policy", filename);
    return MASK_ERRFILE;
  }
#endif
  LoadF lf;
  int status, readstatus;
  int c;
  int fnameindex = mask_gettop(L) + 1;  /* index of filename on the stack */
  if (filename == NULL) {
    mask_pushliteral(L, "=stdin");
    lf.f = stdin;
  }
  else {
    mask_pushfstring(L, "@%s", filename);
    lf.f = maskL_fopen(filename, strlen(filename), "r", sizeof("r") - sizeof(char));
    if (lf.f == NULL) return errfile(L, "open", fnameindex);
  }
  lf.n = 0;
  if (skipcomment(lf.f, &c))  /* read initial portion */
    lf.buff[lf.n++] = '\n';  /* add newline to correct line numbers */
  if (c == MASK_SIGNATURE[0]) {  /* binary file? */
    lf.n = 0;  /* remove possible newline */
    if (filename) {  /* "real" file? */
      lf.f = freopen(filename, "rb", lf.f);  /* reopen in binary mode */
      if (lf.f == NULL) return errfile(L, "reopen", fnameindex);
      skipcomment(lf.f, &c);  /* re-read initial portion */
    }
  }
  if (c != EOF)
    lf.buff[lf.n++] = c;  /* 'c' is the first character of the stream */
  status = mask_load(L, getF, &lf, mask_tostring(L, -1), mode);
  readstatus = ferror(lf.f);
  if (filename) fclose(lf.f);  /* close file (even in case of errors) */
  if (readstatus) {
    mask_settop(L, fnameindex);  /* ignore results from 'mask_load' */
    return errfile(L, "read", fnameindex);
  }
  mask_remove(L, fnameindex);
  return status;
}


typedef struct LoadS {
  const char *s;
  size_t size;
} LoadS;


static const char *getS (mask_State *L, void *ud, size_t *size) {
  LoadS *ls = (LoadS *)ud;
  (void)L;  /* not used */
  if (ls->size == 0) return NULL;
  *size = ls->size;
  ls->size = 0;
  return ls->s;
}


MASKLIB_API int maskL_loadbufferx (mask_State *L, const char *buff, size_t size,
                                 const char *name, const char *mode) {
  LoadS ls;
  ls.s = buff;
  ls.size = size;
  return mask_load(L, getS, &ls, name, mode);
}


MASKLIB_API int maskL_loadstring (mask_State *L, const char *s) {
  return maskL_loadbuffer(L, s, strlen(s), s);
}

/* }====================================================== */



MASKLIB_API int maskL_getmetafield (mask_State *L, int obj, const char *event) {
  if (!mask_getmetatable(L, obj))  /* no metatable? */
    return MASK_TNIL;
  else {
    int tt;
    mask_pushstring(L, event);
    tt = mask_rawget(L, -2);
    if (tt == MASK_TNIL)  /* is metafield nil? */
      mask_pop(L, 2);  /* remove metatable and metafield */
    else
      mask_remove(L, -2);  /* remove only metatable */
    return tt;  /* return metafield type */
  }
}


MASKLIB_API int maskL_callmeta (mask_State *L, int obj, const char *event) {
  obj = mask_absindex(L, obj);
  if (maskL_getmetafield(L, obj, event) == MASK_TNIL)  /* no metafield? */
    return 0;
  mask_pushvalue(L, obj);
  mask_call(L, 1, 1);
  return 1;
}


MASKLIB_API mask_Integer maskL_len (mask_State *L, int idx) {
  mask_Integer l;
  int isnum;
  mask_len(L, idx);
  l = mask_tointegerx(L, -1, &isnum);
  if (l_unlikely(!isnum))
    maskL_error(L, "object length is not an integer");
  mask_pop(L, 1);  /* remove object */
  return l;
}


MASKLIB_API const char *maskL_tolstring (mask_State *L, int idx, size_t *len) {
  idx = mask_absindex(L,idx);
  if (maskL_callmeta(L, idx, "__tostring")) {  /* metafield? */
    if (!mask_isstring(L, -1))
      maskL_error(L, "'__tostring' must return a string");
  }
  else {
    switch (mask_type(L, idx)) {
      case MASK_TNUMBER: {
        if (mask_isinteger(L, idx))
          mask_pushfstring(L, "%I", (MASKI_UACINT)mask_tointeger(L, idx));
        else
          mask_pushfstring(L, "%f", (MASKI_UACNUMBER)mask_tonumber(L, idx));
        break;
      }
      case MASK_TSTRING:
        mask_pushvalue(L, idx);
        break;
      case MASK_TBOOLEAN:
        mask_pushstring(L, (mask_toboolean(L, idx) ? "true" : "false"));
        break;
      case MASK_TNIL:
        mask_pushliteral(L, "nil");
        break;
      default: {
        int tt = maskL_getmetafield(L, idx, "__name");  /* try name */
        const char *kind = (tt == MASK_TSTRING) ? mask_tostring(L, -1) :
                                                 maskL_typename(L, idx);
        mask_pushfstring(L, "%s: %p", kind, mask_topointer(L, idx));
        if (tt != MASK_TNIL)
          mask_remove(L, -2);  /* remove '__name' */
        break;
      }
    }
  }
  return mask_tolstring(L, -1, len);
}


/*
** set functions from list 'l' into table at top - 'nup'; each
** function gets the 'nup' elements at the top as upvalues.
** Returns with only the table at the stack.
*/
MASKLIB_API void maskL_setfuncs (mask_State *L, const maskL_Reg *l, int nup) {
  maskL_checkstack(L, nup, "too many upvalues");
  for (; l->name != NULL; l++) {  /* fill the table with given functions */
    if (l->func == NULL)  /* place holder? */
      mask_pushboolean(L, 0);
    else {
      int i;
      for (i = 0; i < nup; i++)  /* copy upvalues to the top */
        mask_pushvalue(L, -nup);
      mask_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
    }
    mask_setfield(L, -(nup + 2), l->name);
  }
  mask_pop(L, nup);  /* remove upvalues */
}


/*
** ensure that stack[idx][fname] has a table and push that table
** into the stack
*/
MASKLIB_API int maskL_getsubtable (mask_State *L, int idx, const char *fname) {
  if (mask_getfield(L, idx, fname) == MASK_TTABLE)
    return 1;  /* table already there */
  else {
    mask_pop(L, 1);  /* remove previous result */
    idx = mask_absindex(L, idx);
    mask_newtable(L);
    mask_pushvalue(L, -1);  /* copy to be left at top */
    mask_setfield(L, idx, fname);  /* assign new table to field */
    return 0;  /* false, because did not find table there */
  }
}


/*
** Stripped-down 'require': After checking "loaded" table, calls 'openf'
** to open a module, registers the result in 'package.loaded' table and,
** if 'glb' is true, also registers the result in the global table.
** Leaves resulting module on the top.
*/
MASKLIB_API void maskL_requiref (mask_State *L, const char *modname,
                               mask_CFunction openf, int glb) {
  maskL_getsubtable(L, MASK_REGISTRYINDEX, MASK_LOADED_TABLE);
  mask_getfield(L, -1, modname);  /* LOADED[modname] */
  if (!mask_toboolean(L, -1)) {  /* package not already loaded? */
    mask_pop(L, 1);  /* remove field */
    mask_pushcfunction(L, openf);
    mask_pushstring(L, modname);  /* argument to open function */
    mask_call(L, 1, 1);  /* call 'openf' to open module */
    mask_pushvalue(L, -1);  /* make copy of module (call result) */
    mask_setfield(L, -3, modname);  /* LOADED[modname] = module */
  }
  mask_remove(L, -2);  /* remove LOADED table */
  if (glb) {
    mask_pushvalue(L, -1);  /* copy of module */
    mask_setglobal(L, modname);  /* _G[modname] = module */
  }
}


MASKLIB_API void maskL_addgsub (maskL_Buffer *b, const char *s,
                                     const char *p, const char *r) {
  const char *wild;
  size_t l = strlen(p);
  while ((wild = strstr(s, p)) != NULL) {
    maskL_addlstring(b, s, wild - s);  /* push prefix */
    maskL_addstring(b, r);  /* push replacement in place of pattern */
    s = wild + l;  /* continue after 'p' */
  }
  maskL_addstring(b, s);  /* push last suffix */
}


MASKLIB_API const char *maskL_gsub (mask_State *L, const char *s,
                                  const char *p, const char *r) {
  maskL_Buffer b;
  maskL_buffinit(L, &b);
  maskL_addgsub(&b, s, p, r);
  maskL_pushresult(&b);
  return mask_tostring(L, -1);
}


static void *l_alloc (void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud; (void)osize;  /* not used */
  if (nsize == 0) {
    free(ptr);
    return NULL;
  }
  else
    return realloc(ptr, nsize);
}


static int panic (mask_State *L) {
  const char *msg = mask_tostring(L, -1);
  if (msg == NULL) msg = "error object is not a string";
  mask_writestringerror("PANIC: unprotected error in call to Mask API (%s)\n",
                        msg);
  return 0;  /* return to Mask to abort */
}


/*
** Warning functions:
** warnfoff: warning system is off
** warnfon: ready to start a new message
** warnfcont: previous message is to be continued
*/
static void warnfoff (void *ud, const char *message, int tocont);
static void warnfon (void *ud, const char *message, int tocont);
static void warnfcont (void *ud, const char *message, int tocont);


/*
** Check whether message is a control message. If so, execute the
** control or ignore it if unknown.
*/
static int checkcontrol (mask_State *L, const char *message, int tocont) {
  if (tocont || *(message++) != '@')  /* not a control message? */
    return 0;
  else {
    if (strcmp(message, "off") == 0)
      mask_setwarnf(L, warnfoff, L);  /* turn warnings off */
    else if (strcmp(message, "on") == 0)
      mask_setwarnf(L, warnfon, L);   /* turn warnings on */
    return 1;  /* it was a control message */
  }
}


static void warnfoff (void *ud, const char *message, int tocont) {
  checkcontrol((mask_State *)ud, message, tocont);
}


/*
** Writes the message and handle 'tocont', finishing the message
** if needed and setting the next warn function.
*/
static void warnfcont (void *ud, const char *message, int tocont) {
  mask_State *L = (mask_State *)ud;
  mask_writestringerror("%s", message);  /* write message */
  if (tocont)  /* not the last part? */
    mask_setwarnf(L, warnfcont, L);  /* to be continued */
  else {  /* last part */
    mask_writestringerror("%s", "\n");  /* finish message with end-of-line */
    mask_setwarnf(L, warnfon, L);  /* next call is a new message */
  }
}


static void warnfon (void *ud, const char *message, int tocont) {
  if (checkcontrol((mask_State *)ud, message, tocont))  /* control message? */
    return;  /* nothing else to be done */
  warnfcont(ud, message, tocont);  /* finish processing */
}


MASKLIB_API mask_State *maskL_newstate (void) {
  mask_State *L = mask_newstate(l_alloc, NULL);
  if (l_likely(L)) {
    mask_atpanic(L, &panic);
    mask_setwarnf(L, warnfon, L);  /* unlike mask, warnings are enabled by default in mask */
  }
  return L;
}


MASKLIB_API void maskL_checkversion_ (mask_State *L, mask_Number ver, size_t sz) {
  mask_Number v = mask_version(L);
  if (sz != MASKL_NUMSIZES)  /* check numeric types */
    maskL_error(L, "core and library have incompatible numeric types");
  else if (v != ver)
    maskL_error(L, "version mismatch: app. needs %f, Mask core provides %f",
                  (MASKI_UACNUMBER)ver, (MASKI_UACNUMBER)v);
}

