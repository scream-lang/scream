/*
** $Id: lauxlib.c $
** Auxiliary functions for building Hello libraries
** See Copyright Notice in hello.h
*/

#define lauxlib_c
#define HELLO_LIB

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <Windows.h>
#endif


/*
** This file uses only the official API of Hello.
** Any function declared here could be written as an application function.
*/

#include "hello.h"
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
static int findfield (hello_State *L, int objidx, int level) {
  if (level == 0 || !hello_istable(L, -1))
    return 0;  /* not found */
  hello_pushnil(L);  /* start 'next' loop */
  while (hello_next(L, -2)) {  /* for each pair in table */
    if (hello_type(L, -2) == HELLO_TSTRING) {  /* ignore non-string keys */
      if (hello_rawequal(L, objidx, -1)) {  /* found object? */
        hello_pop(L, 1);  /* remove value (but keep name) */
        return 1;
      }
      else if (findfield(L, objidx, level - 1)) {  /* try recursively */
        /* stack: lib_name, lib_table, field_name (top) */
        hello_pushliteral(L, ".");  /* place '.' between the two names */
        hello_replace(L, -3);  /* (in the slot occupied by table) */
        hello_concat(L, 3);  /* lib_name.field_name */
        return 1;
      }
    }
    hello_pop(L, 1);  /* remove value */
  }
  return 0;  /* not found */
}


/*
** Search for a name for a function in all loaded modules
*/
static int pushglobalfuncname (hello_State *L, hello_Debug *ar) {
  int top = hello_gettop(L);
  hello_getinfo(L, "f", ar);  /* push function */
  hello_getfield(L, HELLO_REGISTRYINDEX, HELLO_LOADED_TABLE);
  if (findfield(L, top + 1, 2)) {
    const char *name = hello_tostring(L, -1);
    if (strncmp(name, HELLO_GNAME ".", 3) == 0) {  /* name start with '_G.'? */
      hello_pushstring(L, name + 3);  /* push name without prefix */
      hello_remove(L, -2);  /* remove original name */
    }
    hello_copy(L, -1, top + 1);  /* copy name to proper place */
    hello_settop(L, top + 1);  /* remove table "loaded" and name copy */
    return 1;
  }
  else {
    hello_settop(L, top);  /* remove function and global table */
    return 0;
  }
}


static void pushfuncname (hello_State *L, hello_Debug *ar) {
  if (pushglobalfuncname(L, ar)) {  /* try first a global name */
    hello_pushfstring(L, "function '%s'", hello_tostring(L, -1));
    hello_remove(L, -2);  /* remove name */
  }
  else if (*ar->namewhat != '\0')  /* is there a name from code? */
    hello_pushfstring(L, "%s '%s'", ar->namewhat, ar->name);  /* use it */
  else if (*ar->what == 'm')  /* main? */
      hello_pushliteral(L, "main chunk");
  else if (*ar->what != 'C')  /* for Hello functions, use <file:line> */
    hello_pushfstring(L, "function <%s:%d>", ar->short_src, ar->linedefined);
  else  /* nothing left... */
    hello_pushliteral(L, "?");
}


static int lastlevel (hello_State *L) {
  hello_Debug ar;
  int li = 1, le = 1;
  /* find an upper bound */
  while (hello_getstack(L, le, &ar)) { li = le; le *= 2; }
  /* do a binary search */
  while (li < le) {
    int m = (li + le)/2;
    if (hello_getstack(L, m, &ar)) li = m + 1;
    else le = m;
  }
  return le - 1;
}


HELLOLIB_API void helloL_traceback (hello_State *L, hello_State *L1,
                                const char *msg, int level) {
  helloL_Buffer b;
  hello_Debug ar;
  int last = lastlevel(L1);
  int limit2show = (last - level > LEVELS1 + LEVELS2) ? LEVELS1 : -1;
  helloL_buffinit(L, &b);
  if (msg) {
    helloL_addstring(&b, msg);
    helloL_addchar(&b, '\n');
  }
  helloL_addstring(&b, "stack traceback:");
  while (hello_getstack(L1, level++, &ar)) {
    if (limit2show-- == 0) {  /* too many levels? */
      int n = last - level - LEVELS2 + 1;  /* number of levels to skip */
      hello_pushfstring(L, "\n\t...\t(skipping %d levels)", n);
      helloL_addvalue(&b);  /* add warning about skip */
      level += n;  /* and skip to last levels */
    }
    else {
      hello_getinfo(L1, "Slnt", &ar);
      if (ar.currentline <= 0)
        hello_pushfstring(L, "\n\t%s: in ", ar.short_src);
      else
        hello_pushfstring(L, "\n\t%s:%d: in ", ar.short_src, ar.currentline);
      helloL_addvalue(&b);
      pushfuncname(L, &ar);
      helloL_addvalue(&b);
      if (ar.istailcall)
        helloL_addstring(&b, "\n\t(...tail calls...)");
    }
  }
  helloL_pushresult(&b);
}

/* }====================================================== */


/*
** {======================================================
** Error-report functions
** =======================================================
*/

HELLOLIB_API void helloL_argerror (hello_State *L, int arg, const char *extramsg) {
  hello_Debug ar;
  if (!hello_getstack(L, 0, &ar))  /* no stack frame? */
    helloL_error(L, "bad argument #%d (%s)", arg, extramsg);
  hello_getinfo(L, "n", &ar);
  if (strcmp(ar.namewhat, "method") == 0) {
    arg--;  /* do not count 'self' */
    if (arg == 0)  /* error is in the self argument itself? */
      helloL_error(L, "calling '%s' on bad self (%s)",
                           ar.name, extramsg);
  }
  if (ar.name == NULL)
    ar.name = (pushglobalfuncname(L, &ar)) ? hello_tostring(L, -1) : "?";
  helloL_error(L, "bad argument #%d to '%s' (%s)",
                        arg, ar.name, extramsg);
}


HELLOLIB_API void helloL_typeerror (hello_State *L, int arg, const char *tname) {
  const char *msg;
  const char *typearg;  /* name for the type of the actual argument */
  if (helloL_getmetafield(L, arg, "__name") == HELLO_TSTRING)
    typearg = hello_tostring(L, -1);  /* use the given type name */
  else if (hello_type(L, arg) == HELLO_TLIGHTUSERDATA)
    typearg = "light userdata";  /* special name for messages */
  else
    typearg = helloL_typename(L, arg);  /* standard name */
  msg = hello_pushfstring(L, "%s expected, got %s", tname, typearg);
  helloL_argerror(L, arg, msg);
}


[[noreturn]] static void tag_error (hello_State *L, int arg, int tag) {
  helloL_typeerror(L, arg, hello_typename(L, tag));
}


/*
** The use of 'hello_pushfstring' ensures this function does not
** need reserved stack space when called.
*/
HELLOLIB_API void helloL_where (hello_State *L, int level) {
  hello_Debug ar;
  if (hello_getstack(L, level, &ar)) {  /* check function at level */
    hello_getinfo(L, "Sl", &ar);  /* get info about it */
    if (ar.currentline > 0) {  /* is there info? */
      hello_pushfstring(L, "%s:%d: ", ar.short_src, ar.currentline);
      return;
    }
  }
  hello_pushfstring(L, "");  /* else, no information available... */
}


/*
** Again, the use of 'hello_pushvfstring' ensures this function does
** not need reserved stack space when called. (At worst, it generates
** an error with "stack overflow" instead of the given message.)
*/
HELLOLIB_API void helloL_error (hello_State *L, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  helloL_where(L, 1);
  hello_pushvfstring(L, fmt, argp);
  va_end(argp);
  hello_concat(L, 2);
  hello_error(L);
}


HELLOLIB_API int helloL_fileresult (hello_State *L, int stat, const char *fname) {
  int en = errno;  /* calls to Hello API may change this value */
  if (stat) {
    hello_pushboolean(L, 1);
    return 1;
  }
  else {
    helloL_pushfail(L);
    if (fname)
      hello_pushfstring(L, "%s: %s", fname, strerror(en));
    else
      hello_pushstring(L, strerror(en));
    hello_pushinteger(L, en);
    return 3;
  }
}


#if !defined(l_inspectstat)	/* { */

#if defined(HELLO_USE_POSIX)

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


HELLOLIB_API int helloL_execresult (hello_State *L, int stat) {
  if (stat != 0 && errno != 0)  /* error with an 'errno'? */
    return helloL_fileresult(L, 0, NULL);
  else {
    const char *what = "exit";  /* type of termination */
    l_inspectstat(stat, what);  /* interpret result */
    if (*what == 'e' && stat == 0)  /* successful termination? */
      hello_pushboolean(L, 1);
    else
      helloL_pushfail(L);
    hello_pushstring(L, what);
    hello_pushinteger(L, stat);
    return 3;  /* return true/fail,what,code */
  }
}

/* }====================================================== */



/*
** {======================================================
** Userdata's metatable manipulation
** =======================================================
*/

HELLOLIB_API int helloL_newmetatable (hello_State *L, const char *tname) {
  if (helloL_getmetatable(L, tname) != HELLO_TNIL)  /* name already in use? */
    return 0;  /* leave previous value on top, but return 0 */
  hello_pop(L, 1);
  hello_createtable(L, 0, 2);  /* create metatable */
  hello_pushstring(L, tname);
  hello_setfield(L, -2, "__name");  /* metatable.__name = tname */
  hello_pushvalue(L, -1);
  hello_setfield(L, HELLO_REGISTRYINDEX, tname);  /* registry.name = metatable */
  return 1;
}


HELLOLIB_API void helloL_setmetatable (hello_State *L, const char *tname) {
  helloL_getmetatable(L, tname);
  hello_setmetatable(L, -2);
}


HELLOLIB_API void *helloL_testudata (hello_State *L, int ud, const char *tname) {
  void *p = hello_touserdata(L, ud);
  if (p != NULL) {  /* value is a userdata? */
    if (hello_getmetatable(L, ud)) {  /* does it have a metatable? */
      helloL_getmetatable(L, tname);  /* get correct metatable */
      if (!hello_rawequal(L, -1, -2))  /* not the same? */
        p = NULL;  /* value is a userdata with wrong metatable */
      hello_pop(L, 2);  /* remove both metatables */
      return p;
    }
  }
  return NULL;  /* value is not a userdata with a metatable */
}


HELLOLIB_API void *helloL_checkudata (hello_State *L, int ud, const char *tname) {
  void *p = helloL_testudata(L, ud, tname);
  helloL_argexpected(L, p != NULL, ud, tname);
  return p;
}

/* }====================================================== */


/*
** {======================================================
** Argument check functions
** =======================================================
*/

HELLOLIB_API int helloL_checkoption (hello_State *L, int arg, const char *def,
                                 const char *const lst[]) {
  const char *name = (def) ? helloL_optstring(L, arg, def) :
                             helloL_checkstring(L, arg);
  int i;
  for (i=0; lst[i]; i++)
    if (strcmp(lst[i], name) == 0)
      return i;
  helloL_argerror(L, arg, hello_pushfstring(L, "invalid option '%s'", name));
}


/*
** Ensures the stack has at least 'space' extra slots, raising an error
** if it cannot fulfill the request. (The error handling needs a few
** extra slots to format the error message. In case of an error without
** this extra space, Hello will generate the same 'stack overflow' error,
** but without 'msg'.)
*/
HELLOLIB_API void helloL_checkstack (hello_State *L, int space, const char *msg) {
  if (l_unlikely(!hello_checkstack(L, space))) {
    if (msg)
      helloL_error(L, "stack overflow (%s)", msg);
    else
      helloL_error(L, "stack overflow");
  }
}


HELLOLIB_API void helloL_checktype (hello_State *L, int arg, int t) {
  if (l_unlikely(hello_type(L, arg) != t))
    tag_error(L, arg, t);
}


HELLOLIB_API void helloL_checkany (hello_State *L, int arg) {
  if (l_unlikely(hello_type(L, arg) == HELLO_TNONE))
    helloL_argerror(L, arg, "value expected");
}


HELLOLIB_API const char *helloL_checklstring (hello_State *L, int arg, size_t *len) {
  const char *s = hello_tolstring(L, arg, len);
  if (l_unlikely(!s)) tag_error(L, arg, HELLO_TSTRING);
  return s;
}


HELLOLIB_API const char *helloL_optlstring (hello_State *L, int arg,
                                        const char *def, size_t *len) {
  if (hello_isnoneornil(L, arg)) {
    if (len)
      *len = (def ? strlen(def) : 0);
    return def;
  }
  else return helloL_checklstring(L, arg, len);
}


HELLOLIB_API hello_Number helloL_checknumber (hello_State *L, int arg) {
  int isnum;
  hello_Number d = hello_tonumberx(L, arg, &isnum);
  if (l_unlikely(!isnum))
    tag_error(L, arg, HELLO_TNUMBER);
  return d;
}


HELLOLIB_API hello_Number helloL_optnumber (hello_State *L, int arg, hello_Number def) {
  return helloL_opt(L, helloL_checknumber, arg, def);
}


[[noreturn]] static void interror (hello_State *L, int arg) {
  if (hello_isnumber(L, arg))
    helloL_argerror(L, arg, "number has no integer representation");
  else
    tag_error(L, arg, HELLO_TNUMBER);
}


HELLOLIB_API hello_Integer helloL_checkinteger (hello_State *L, int arg) {
  int isnum;
  hello_Integer d = hello_tointegerx(L, arg, &isnum);
  if (l_unlikely(!isnum)) {
    interror(L, arg);
  }
  return d;
}


HELLOLIB_API hello_Integer helloL_optinteger (hello_State *L, int arg,
                                                      hello_Integer def) {
  return helloL_opt(L, helloL_checkinteger, arg, def);
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


static void *resizebox (hello_State *L, int idx, size_t newsize) {
  void *ud;
  hello_Alloc allocf = hello_getallocf(L, &ud);
  UBox *box = (UBox *)hello_touserdata(L, idx);
  void *temp = allocf(ud, box->box, box->bsize, newsize);
  if (l_unlikely(temp == NULL && newsize > 0)) {  /* allocation error? */
    hello_pushliteral(L, "not enough memory");
    hello_error(L);  /* raise a memory error */
  }
  box->box = temp;
  box->bsize = newsize;
  return temp;
}


static int boxgc (hello_State *L) {
  resizebox(L, 1, 0);
  return 0;
}


static const helloL_Reg boxmt[] = {  /* box metamethods */
  {"__gc", boxgc},
  {"__close", boxgc},
  {NULL, NULL}
};


static void newbox (hello_State *L) {
  UBox *box = (UBox *)hello_newuserdatauv(L, sizeof(UBox), 0);
  box->box = NULL;
  box->bsize = 0;
  if (helloL_newmetatable(L, "_UBOX*"))  /* creating metatable? */
    helloL_setfuncs(L, boxmt, 0);  /* set its metamethods */
  hello_setmetatable(L, -2);
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
  hello_assert(buffonstack(B) ? hello_touserdata(B->L, idx) != NULL  \
                            : hello_touserdata(B->L, idx) == (void*)B)


/*
** Compute new size for buffer 'B', enough to accommodate extra 'sz'
** bytes. (The test for "not big enough" also gets the case when the
** computation of 'newsize' overflows.)
*/
static size_t newbuffsize (helloL_Buffer *B, size_t sz) {
  size_t newsize = (B->size / 2) * 3;  /* buffer size * 1.5 */
  if (l_unlikely(MAX_SIZET - sz < B->n))  /* overflow in (B->n + sz)? */
    helloL_error(B->L, "buffer too large");
  if (newsize < B->n + sz)  /* not big enough? */
    newsize = B->n + sz;
  return newsize;
}


/*
** Returns a pointer to a free area with at least 'sz' bytes in buffer
** 'B'. 'boxidx' is the relative position in the stack where is the
** buffer's box or its placeholder.
*/
static char *prepbuffsize (helloL_Buffer *B, size_t sz, int boxidx) {
  checkbufferlevel(B, boxidx);
  if (B->size - B->n >= sz)  /* enough space? */
    return B->b + B->n;
  else {
    hello_State *L = B->L;
    char *newbuff;
    size_t newsize = newbuffsize(B, sz);
    /* create larger buffer */
    if (buffonstack(B))  /* buffer already has a box? */
      newbuff = (char *)resizebox(L, boxidx, newsize);  /* resize it */
    else {  /* no box yet */
      hello_remove(L, boxidx);  /* remove placeholder */
      newbox(L);  /* create a new box */
      hello_insert(L, boxidx);  /* move box to its intended position */
      hello_toclose(L, boxidx);
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
HELLOLIB_API char *helloL_prepbuffsize (helloL_Buffer *B, size_t sz) {
  return prepbuffsize(B, sz, -1);
}


HELLOLIB_API void helloL_addlstring (helloL_Buffer *B, const char *s, size_t l) {
  if (l > 0) {  /* avoid 'memcpy' when 's' can be NULL */
    char *b = prepbuffsize(B, l, -1);
    memcpy(b, s, l * sizeof(char));
    helloL_addsize(B, l);
  }
}


HELLOLIB_API void helloL_addstring (helloL_Buffer *B, const char *s) {
  helloL_addlstring(B, s, strlen(s));
}


HELLOLIB_API void helloL_pushresult (helloL_Buffer *B) {
  hello_State *L = B->L;
  checkbufferlevel(B, -1);
  hello_pushlstring(L, B->b, B->n);
  if (buffonstack(B))
    hello_closeslot(L, -2);  /* close the box */
  hello_remove(L, -2);  /* remove box or placeholder from the stack */
}


HELLOLIB_API void helloL_pushresultsize (helloL_Buffer *B, size_t sz) {
  helloL_addsize(B, sz);
  helloL_pushresult(B);
}


/*
** 'helloL_addvalue' is the only function in the Buffer system where the
** box (if existent) is not on the top of the stack. So, instead of
** calling 'helloL_addlstring', it replicates the code using -2 as the
** last argument to 'prepbuffsize', signaling that the box is (or will
** be) below the string being added to the buffer. (Box creation can
** trigger an emergency GC, so we should not remove the string from the
** stack before we have the space guaranteed.)
*/
HELLOLIB_API void helloL_addvalue (helloL_Buffer *B) {
  hello_State *L = B->L;
  size_t len;
  const char *s = hello_tolstring(L, -1, &len);
  char *b = prepbuffsize(B, len, -2);
  memcpy(b, s, len * sizeof(char));
  helloL_addsize(B, len);
  hello_pop(L, 1);  /* pop string */
}


HELLOLIB_API void helloL_buffinit (hello_State *L, helloL_Buffer *B) {
  B->L = L;
  B->b = B->init.b;
  B->n = 0;
  B->size = HELLOL_BUFFERSIZE;
  hello_pushlightuserdata(L, (void*)B);  /* push placeholder */
}


HELLOLIB_API char *helloL_buffinitsize (hello_State *L, helloL_Buffer *B, size_t sz) {
  helloL_buffinit(L, B);
  return prepbuffsize(B, sz, -1);
}

/* }====================================================== */


/*
** {======================================================
** Reference system
** =======================================================
*/

/* index of free-list header (after the predefined values) */
#define freelist	(HELLO_RIDX_LAST + 1)

/*
** The previously freed references form a linked list:
** t[freelist] is the index of a first free index, or zero if list is
** empty; t[t[freelist]] is the index of the second element; etc.
*/
HELLOLIB_API int helloL_ref (hello_State *L, int t) {
  int ref;
  if (hello_isnil(L, -1)) {
    hello_pop(L, 1);  /* remove from stack */
    return HELLO_REFNIL;  /* 'nil' has a unique fixed reference */
  }
  t = hello_absindex(L, t);
  if (hello_rawgeti(L, t, freelist) == HELLO_TNIL) {  /* first access? */
    ref = 0;  /* list is empty */
    hello_pushinteger(L, 0);  /* initialize as an empty list */
    hello_rawseti(L, t, freelist);  /* ref = t[freelist] = 0 */
  }
  else {  /* already initialized */
    hello_assert(hello_isinteger(L, -1));
    ref = (int)hello_tointeger(L, -1);  /* ref = t[freelist] */
  }
  hello_pop(L, 1);  /* remove element from stack */
  if (ref != 0) {  /* any free element? */
    hello_rawgeti(L, t, ref);  /* remove it from list */
    hello_rawseti(L, t, freelist);  /* (t[freelist] = t[ref]) */
  }
  else  /* no free elements */
    ref = (int)hello_rawlen(L, t) + 1;  /* get a new reference */
  hello_rawseti(L, t, ref);
  return ref;
}


HELLOLIB_API void helloL_unref (hello_State *L, int t, int ref) {
  if (ref >= 0) {
    t = hello_absindex(L, t);
    hello_rawgeti(L, t, freelist);
    hello_assert(hello_isinteger(L, -1));
    hello_rawseti(L, t, ref);  /* t[ref] = t[freelist] */
    hello_pushinteger(L, ref);
    hello_rawseti(L, t, freelist);  /* t[freelist] = ref */
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


static const char *getF (hello_State *L, void *ud, size_t *size) {
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


static int errfile (hello_State *L, const char *what, int fnameindex) {
  const char *serr = strerror(errno);
  const char *filename = hello_tostring(L, fnameindex) + 1;
  hello_pushfstring(L, "cannot %s %s: %s", what, filename, serr);
  hello_remove(L, fnameindex);
  return HELLO_ERRFILE;
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
std::wstring helloL_utf8_to_utf16(const char *utf8, size_t utf8_len) {
  std::wstring utf16;
  const int sizeRequired = MultiByteToWideChar(CP_UTF8, 0, utf8, (int)utf8_len, nullptr, 0);
  if (l_likely(sizeRequired != 0)) {
    utf16 = std::wstring(sizeRequired, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8, (int)utf8_len, utf16.data(), sizeRequired);
  }
  return utf16;
}

std::string helloL_utf16_to_utf8(const wchar_t *utf16, size_t utf16_len) {
  std::string utf8;
  const int sizeRequired = WideCharToMultiByte(CP_UTF8, 0, utf16, (int)utf16_len, nullptr, 0, 0, 0);
  if (l_likely(sizeRequired != 0)) {
    utf8 = std::string(sizeRequired, 0);
    WideCharToMultiByte(CP_UTF8, 0, utf16, (int)utf16_len, utf8.data(), sizeRequired, 0, 0);
  }
  return utf8;
}
#endif


HELLOLIB_API FILE* (helloL_fopen) (const char *filename, size_t filename_len,
                               const char *mode, size_t mode_len) {
#ifdef _WIN32
  // From what I could gather online, UTF-8 is the path encoding convention on *nix systems,
  // so I've ultimately decided that we should just "fix" the fact that Windows doesn't use UTF-8.
  std::wstring wfilename = helloL_utf8_to_utf16(filename, filename_len);
  std::wstring wmode = helloL_utf8_to_utf16(mode, mode_len);
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

#ifdef HELLO_LOADFILE_HOOK
extern "C" bool HELLO_LOADFILE_HOOK(const char* filename);
#endif

HELLOLIB_API int helloL_loadfilex (hello_State *L, const char *filename,
                                             const char *mode) {
#ifdef HELLO_LOADFILE_HOOK
  if (!HELLO_LOADFILE_HOOK(filename)) {
    hello_pushfstring(L, "%s failed content moderation policy", filename);
    return HELLO_ERRFILE;
  }
#endif
  LoadF lf;
  int status, readstatus;
  int c;
  int fnameindex = hello_gettop(L) + 1;  /* index of filename on the stack */
  if (filename == NULL) {
    hello_pushliteral(L, "=stdin");
    lf.f = stdin;
  }
  else {
    hello_pushfstring(L, "@%s", filename);
    lf.f = helloL_fopen(filename, strlen(filename), "r", sizeof("r") - sizeof(char));
    if (lf.f == NULL) return errfile(L, "open", fnameindex);
  }
  lf.n = 0;
  if (skipcomment(lf.f, &c))  /* read initial portion */
    lf.buff[lf.n++] = '\n';  /* add newline to correct line numbers */
  if (c == HELLO_SIGNATURE[0]) {  /* binary file? */
    lf.n = 0;  /* remove possible newline */
    if (filename) {  /* "real" file? */
      lf.f = freopen(filename, "rb", lf.f);  /* reopen in binary mode */
      if (lf.f == NULL) return errfile(L, "reopen", fnameindex);
      skipcomment(lf.f, &c);  /* re-read initial portion */
    }
  }
  if (c != EOF)
    lf.buff[lf.n++] = c;  /* 'c' is the first character of the stream */
  status = hello_load(L, getF, &lf, hello_tostring(L, -1), mode);
  readstatus = ferror(lf.f);
  if (filename) fclose(lf.f);  /* close file (even in case of errors) */
  if (readstatus) {
    hello_settop(L, fnameindex);  /* ignore results from 'hello_load' */
    return errfile(L, "read", fnameindex);
  }
  hello_remove(L, fnameindex);
  return status;
}


typedef struct LoadS {
  const char *s;
  size_t size;
} LoadS;


static const char *getS (hello_State *L, void *ud, size_t *size) {
  LoadS *ls = (LoadS *)ud;
  (void)L;  /* not used */
  if (ls->size == 0) return NULL;
  *size = ls->size;
  ls->size = 0;
  return ls->s;
}


HELLOLIB_API int helloL_loadbufferx (hello_State *L, const char *buff, size_t size,
                                 const char *name, const char *mode) {
  LoadS ls;
  ls.s = buff;
  ls.size = size;
  return hello_load(L, getS, &ls, name, mode);
}


HELLOLIB_API int helloL_loadstring (hello_State *L, const char *s) {
  return helloL_loadbuffer(L, s, strlen(s), s);
}

/* }====================================================== */



HELLOLIB_API int helloL_getmetafield (hello_State *L, int obj, const char *event) {
  if (!hello_getmetatable(L, obj))  /* no metatable? */
    return HELLO_TNIL;
  else {
    int tt;
    hello_pushstring(L, event);
    tt = hello_rawget(L, -2);
    if (tt == HELLO_TNIL)  /* is metafield nil? */
      hello_pop(L, 2);  /* remove metatable and metafield */
    else
      hello_remove(L, -2);  /* remove only metatable */
    return tt;  /* return metafield type */
  }
}


HELLOLIB_API int helloL_callmeta (hello_State *L, int obj, const char *event) {
  obj = hello_absindex(L, obj);
  if (helloL_getmetafield(L, obj, event) == HELLO_TNIL)  /* no metafield? */
    return 0;
  hello_pushvalue(L, obj);
  hello_call(L, 1, 1);
  return 1;
}


HELLOLIB_API hello_Integer helloL_len (hello_State *L, int idx) {
  hello_Integer l;
  int isnum;
  hello_len(L, idx);
  l = hello_tointegerx(L, -1, &isnum);
  if (l_unlikely(!isnum))
    helloL_error(L, "object length is not an integer");
  hello_pop(L, 1);  /* remove object */
  return l;
}


HELLOLIB_API const char *helloL_tolstring (hello_State *L, int idx, size_t *len) {
  idx = hello_absindex(L,idx);
  if (helloL_callmeta(L, idx, "__tostring")) {  /* metafield? */
    if (!hello_isstring(L, -1))
      helloL_error(L, "'__tostring' must return a string");
  }
  else {
    switch (hello_type(L, idx)) {
      case HELLO_TNUMBER: {
        if (hello_isinteger(L, idx))
          hello_pushfstring(L, "%I", (HELLOI_UACINT)hello_tointeger(L, idx));
        else
          hello_pushfstring(L, "%f", (HELLOI_UACNUMBER)hello_tonumber(L, idx));
        break;
      }
      case HELLO_TSTRING:
        hello_pushvalue(L, idx);
        break;
      case HELLO_TBOOLEAN:
        hello_pushstring(L, (hello_toboolean(L, idx) ? "true" : "false"));
        break;
      case HELLO_TNIL:
        hello_pushliteral(L, "nil");
        break;
      default: {
        int tt = helloL_getmetafield(L, idx, "__name");  /* try name */
        const char *kind = (tt == HELLO_TSTRING) ? hello_tostring(L, -1) :
                                                 helloL_typename(L, idx);
        hello_pushfstring(L, "%s: %p", kind, hello_topointer(L, idx));
        if (tt != HELLO_TNIL)
          hello_remove(L, -2);  /* remove '__name' */
        break;
      }
    }
  }
  return hello_tolstring(L, -1, len);
}


/*
** set functions from list 'l' into table at top - 'nup'; each
** function gets the 'nup' elements at the top as upvalues.
** Returns with only the table at the stack.
*/
HELLOLIB_API void helloL_setfuncs (hello_State *L, const helloL_Reg *l, int nup) {
  helloL_checkstack(L, nup, "too many upvalues");
  for (; l->name != NULL; l++) {  /* fill the table with given functions */
    if (l->func == NULL)  /* place holder? */
      hello_pushboolean(L, 0);
    else {
      int i;
      for (i = 0; i < nup; i++)  /* copy upvalues to the top */
        hello_pushvalue(L, -nup);
      hello_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
    }
    hello_setfield(L, -(nup + 2), l->name);
  }
  hello_pop(L, nup);  /* remove upvalues */
}


/*
** ensure that stack[idx][fname] has a table and push that table
** into the stack
*/
HELLOLIB_API int helloL_getsubtable (hello_State *L, int idx, const char *fname) {
  if (hello_getfield(L, idx, fname) == HELLO_TTABLE)
    return 1;  /* table already there */
  else {
    hello_pop(L, 1);  /* remove previous result */
    idx = hello_absindex(L, idx);
    hello_newtable(L);
    hello_pushvalue(L, -1);  /* copy to be left at top */
    hello_setfield(L, idx, fname);  /* assign new table to field */
    return 0;  /* false, because did not find table there */
  }
}


/*
** Stripped-down 'require': After checking "loaded" table, calls 'openf'
** to open a module, registers the result in 'package.loaded' table and,
** if 'glb' is true, also registers the result in the global table.
** Leaves resulting module on the top.
*/
HELLOLIB_API void helloL_requiref (hello_State *L, const char *modname,
                               hello_CFunction openf, int glb) {
  helloL_getsubtable(L, HELLO_REGISTRYINDEX, HELLO_LOADED_TABLE);
  hello_getfield(L, -1, modname);  /* LOADED[modname] */
  if (!hello_toboolean(L, -1)) {  /* package not already loaded? */
    hello_pop(L, 1);  /* remove field */
    hello_pushcfunction(L, openf);
    hello_pushstring(L, modname);  /* argument to open function */
    hello_call(L, 1, 1);  /* call 'openf' to open module */
    hello_pushvalue(L, -1);  /* make copy of module (call result) */
    hello_setfield(L, -3, modname);  /* LOADED[modname] = module */
  }
  hello_remove(L, -2);  /* remove LOADED table */
  if (glb) {
    hello_pushvalue(L, -1);  /* copy of module */
    hello_setglobal(L, modname);  /* _G[modname] = module */
  }
}


HELLOLIB_API void helloL_addgsub (helloL_Buffer *b, const char *s,
                                     const char *p, const char *r) {
  const char *wild;
  size_t l = strlen(p);
  while ((wild = strstr(s, p)) != NULL) {
    helloL_addlstring(b, s, wild - s);  /* push prefix */
    helloL_addstring(b, r);  /* push replacement in place of pattern */
    s = wild + l;  /* continue after 'p' */
  }
  helloL_addstring(b, s);  /* push last suffix */
}


HELLOLIB_API const char *helloL_gsub (hello_State *L, const char *s,
                                  const char *p, const char *r) {
  helloL_Buffer b;
  helloL_buffinit(L, &b);
  helloL_addgsub(&b, s, p, r);
  helloL_pushresult(&b);
  return hello_tostring(L, -1);
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


static int panic (hello_State *L) {
  const char *msg = hello_tostring(L, -1);
  if (msg == NULL) msg = "error object is not a string";
  hello_writestringerror("PANIC: unprotected error in call to Hello API (%s)\n",
                        msg);
  return 0;  /* return to Hello to abort */
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
static int checkcontrol (hello_State *L, const char *message, int tocont) {
  if (tocont || *(message++) != '@')  /* not a control message? */
    return 0;
  else {
    if (strcmp(message, "off") == 0)
      hello_setwarnf(L, warnfoff, L);  /* turn warnings off */
    else if (strcmp(message, "on") == 0)
      hello_setwarnf(L, warnfon, L);   /* turn warnings on */
    return 1;  /* it was a control message */
  }
}


static void warnfoff (void *ud, const char *message, int tocont) {
  checkcontrol((hello_State *)ud, message, tocont);
}


/*
** Writes the message and handle 'tocont', finishing the message
** if needed and setting the next warn function.
*/
static void warnfcont (void *ud, const char *message, int tocont) {
  hello_State *L = (hello_State *)ud;
  hello_writestringerror("%s", message);  /* write message */
  if (tocont)  /* not the last part? */
    hello_setwarnf(L, warnfcont, L);  /* to be continued */
  else {  /* last part */
    hello_writestringerror("%s", "\n");  /* finish message with end-of-line */
    hello_setwarnf(L, warnfon, L);  /* next call is a new message */
  }
}


static void warnfon (void *ud, const char *message, int tocont) {
  if (checkcontrol((hello_State *)ud, message, tocont))  /* control message? */
    return;  /* nothing else to be done */
  warnfcont(ud, message, tocont);  /* finish processing */
}


HELLOLIB_API hello_State *helloL_newstate (void) {
  hello_State *L = hello_newstate(l_alloc, NULL);
  if (l_likely(L)) {
    hello_atpanic(L, &panic);
    hello_setwarnf(L, warnfon, L);  /* unlike hello, warnings are enabled by default in hello */
  }
  return L;
}


HELLOLIB_API void helloL_checkversion_ (hello_State *L, hello_Number ver, size_t sz) {
  hello_Number v = hello_version(L);
  if (sz != HELLOL_NUMSIZES)  /* check numeric types */
    helloL_error(L, "core and library have incompatible numeric types");
  else if (v != ver)
    helloL_error(L, "version mismatch: app. needs %f, Hello core provides %f",
                  (HELLOI_UACNUMBER)ver, (HELLOI_UACNUMBER)v);
}

