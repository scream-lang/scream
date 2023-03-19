/*
** $Id: loslib.c $
** Standard Operating System library
** See Copyright Notice in hello.h
*/

#define loslib_c
#define HELLO_LIB

#include "lprefix.h"

#include <thread>
#include <chrono>
#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hello.h"

#include "lauxlib.h"
#include "hellolib.h"


/*
** {==================================================================
** List of valid conversion specifiers for the 'strftime' function;
** options are grouped by length; group of length 2 start with '||'.
** ===================================================================
*/
#if !defined(HELLO_STRFTIMEOPTIONS)	/* { */

/* options for ANSI C 89 (only 1-char options) */
#define L_STRFTIMEC89		"aAbBcdHIjmMpSUwWxXyYZ%"

/* options for ISO C 99 and POSIX */
#define L_STRFTIMEC99 "aAbBcCdDeFgGhHIjmMnprRStTuUVwWxXyYzZ%" \
    "||" "EcECExEXEyEY" "OdOeOHOIOmOMOSOuOUOVOwOWOy"  /* two-char options */

/* options for Windows */
#define L_STRFTIMEWIN "aAbBcdHIjmMpSUwWxXyYzZ%" \
    "||" "#c#x#d#H#I#j#m#M#S#U#w#W#y#Y"  /* two-char options */

#if defined(HELLO_USE_WINDOWS)
#define HELLO_STRFTIMEOPTIONS	L_STRFTIMEWIN
#elif defined(HELLO_USE_C89)
#define HELLO_STRFTIMEOPTIONS	L_STRFTIMEC89
#else  /* C99 specification */
#define HELLO_STRFTIMEOPTIONS	L_STRFTIMEC99
#endif

#endif					/* } */
/* }================================================================== */


/*
** {==================================================================
** Configuration for time-related stuff
** ===================================================================
*/

/*
** type to represent time_t in Hello
*/
#if !defined(HELLO_NUMTIME)	/* { */

#define l_timet			hello_Integer
#define l_pushtime(L,t)		hello_pushinteger(L,(hello_Integer)(t))
#define l_gettime(L,arg)	helloL_checkinteger(L, arg)

#else				/* }{ */

#define l_timet			hello_Number
#define l_pushtime(L,t)		hello_pushnumber(L,(hello_Number)(t))
#define l_gettime(L,arg)	helloL_checknumber(L, arg)

#endif				/* } */


#if !defined(l_gmtime)		/* { */
/*
** By default, Hello uses gmtime/localtime, except when POSIX is available,
** where it uses gmtime_r/localtime_r
*/

#if defined(HELLO_USE_POSIX)	/* { */

#define l_gmtime(t,r)		gmtime_r(t,r)
#define l_localtime(t,r)	localtime_r(t,r)

#else				/* }{ */

/* ISO C definitions */
#define l_gmtime(t,r)		((void)(r)->tm_sec, gmtime(t))
#define l_localtime(t,r)	((void)(r)->tm_sec, localtime(t))

#endif				/* } */

#endif				/* } */

/* }================================================================== */


/*
** {==================================================================
** Configuration for 'tmpnam':
** By default, Hello uses tmpnam except when POSIX is available, where
** it uses mkstemp.
** ===================================================================
*/
#if !defined(hello_tmpnam)	/* { */

#if defined(HELLO_USE_POSIX)	/* { */

#include <unistd.h>

#define HELLO_TMPNAMBUFSIZE	32

#if !defined(HELLO_TMPNAMTEMPLATE)
#define HELLO_TMPNAMTEMPLATE	"/tmp/hello_XXXXXX"
#endif

#define hello_tmpnam(b,e) { \
        strcpy(b, HELLO_TMPNAMTEMPLATE); \
        e = mkstemp(b); \
        if (e != -1) close(e); \
        e = (e == -1); }

#else				/* }{ */

/* ISO C definitions */
#define HELLO_TMPNAMBUFSIZE	L_tmpnam
#define hello_tmpnam(b,e)		{ e = (tmpnam(b) == NULL); }

#endif				/* } */

#endif				/* } */
/* }================================================================== */



static int os_execute (hello_State *L) {
  const char *cmd = helloL_optstring(L, 1, NULL);
  int stat;
  errno = 0;
  stat = system(cmd);
  if (cmd != NULL)
    return helloL_execresult(L, stat);
  else {
    hello_pushboolean(L, stat);  /* true if there is a shell */
    return 1;
  }
}


static int os_tmpname (hello_State *L) {
  char buff[HELLO_TMPNAMBUFSIZE];
  int err;
  hello_tmpnam(buff, err);
  if (l_unlikely(err))
    helloL_error(L, "unable to generate a unique filename");
  hello_pushstring(L, buff);
  return 1;
}


static int os_getenv (hello_State *L) {
  hello_pushstring(L, getenv(helloL_checkstring(L, 1)));  /* if NULL push nil */
  return 1;
}


static int os_clock (hello_State *L) {
  hello_pushnumber(L, ((hello_Number)clock())/(hello_Number)CLOCKS_PER_SEC);
  return 1;
}


/*
** {======================================================
** Time/Date operations
** { year=%Y, month=%m, day=%d, hour=%H, min=%M, sec=%S,
**   wday=%w+1, yday=%j, isdst=? }
** =======================================================
*/

/*
** About the overflow check: an overflow cannot occur when time
** is represented by a hello_Integer, because either hello_Integer is
** large enough to represent all int fields or it is not large enough
** to represent a time that cause a field to overflow.  However, if
** times are represented as doubles and hello_Integer is int, then the
** time 0x1.e1853b0d184f6p+55 would cause an overflow when adding 1900
** to compute the year.
*/
static void setfield (hello_State *L, const char *key, int value, int delta) {
  #if (defined(HELLO_NUMTIME) && HELLO_MAXINTEGER <= INT_MAX)
    if (l_unlikely(value > HELLO_MAXINTEGER - delta))
      helloL_error(L, "field '%s' is out-of-bound", key);
  #endif
  hello_pushinteger(L, (hello_Integer)value + delta);
  hello_setfield(L, -2, key);
}


static void setboolfield (hello_State *L, const char *key, int value) {
  if (value < 0)  /* undefined? */
    return;  /* does not set field */
  hello_pushboolean(L, value);
  hello_setfield(L, -2, key);
}


/*
** Set all fields from structure 'tm' in the table on top of the stack
*/
static void setallfields (hello_State *L, struct tm *stm) {
  setfield(L, "year", stm->tm_year, 1900);
  setfield(L, "month", stm->tm_mon, 1);
  setfield(L, "day", stm->tm_mday, 0);
  setfield(L, "hour", stm->tm_hour, 0);
  setfield(L, "min", stm->tm_min, 0);
  setfield(L, "sec", stm->tm_sec, 0);
  setfield(L, "yday", stm->tm_yday, 1);
  setfield(L, "wday", stm->tm_wday, 1);
  setboolfield(L, "isdst", stm->tm_isdst);
}


static int getboolfield (hello_State *L, const char *key) {
  int res;
  res = (hello_getfield(L, -1, key) == HELLO_TNIL) ? -1 : hello_toboolean(L, -1);
  hello_pop(L, 1);
  return res;
}


static int getfield (hello_State *L, const char *key, int d, int delta) {
  int isnum;
  int t = hello_getfield(L, -1, key);  /* get field and its type */
  hello_Integer res = hello_tointegerx(L, -1, &isnum);
  if (!isnum) {  /* field is not an integer? */
    if (l_unlikely(t != HELLO_TNIL))  /* some other value? */
      helloL_error(L, "field '%s' is not an integer", key);
    else if (l_unlikely(d < 0))  /* absent field; no default? */
      helloL_error(L, "field '%s' missing in date table", key);
    res = d;
  }
  else {
    if (!(res >= 0 ? res - delta <= INT_MAX : INT_MIN + delta <= res))
      helloL_error(L, "field '%s' is out-of-bound", key);
    res -= delta;
  }
  hello_pop(L, 1);
  return (int)res;
}


static const char *checkoption (hello_State *L, const char *conv,
                                ptrdiff_t convlen, char *buff) {
  const char *option = HELLO_STRFTIMEOPTIONS;
  int oplen = 1;  /* length of options being checked */
  for (; *option != '\0' && oplen <= convlen; option += oplen) {
    if (*option == '|')  /* next block? */
      oplen++;  /* will check options with next length (+1) */
    else if (memcmp(conv, option, oplen) == 0) {  /* match? */
      memcpy(buff, conv, oplen);  /* copy valid option to buffer */
      buff[oplen] = '\0';
      return conv + oplen;  /* return next item */
    }
  }
  helloL_argerror(L, 1,
    hello_pushfstring(L, "invalid conversion specifier '%%%s'", conv));
  return conv;  /* to avoid warnings */
}


static time_t l_checktime (hello_State *L, int arg) {
  l_timet t = l_gettime(L, arg);
  helloL_argcheck(L, (time_t)t == t, arg, "time out-of-bounds");
  return (time_t)t;
}


/* maximum size for an individual 'strftime' item */
#define SIZETIMEFMT	250


static int os_date (hello_State *L) {
  size_t slen;
  const char *s = helloL_optlstring(L, 1, "%c", &slen);
  time_t t = helloL_opt(L, l_checktime, 2, time(NULL));
  const char *se = s + slen;  /* 's' end */
  struct tm tmr, *stm;
  if (*s == '!') {  /* UTC? */
    stm = l_gmtime(&t, &tmr);
    s++;  /* skip '!' */
  }
  else
    stm = l_localtime(&t, &tmr);
  if (stm == NULL)  /* invalid date? */
    helloL_error(L, "date result cannot be represented in this installation");
  if (strcmp(s, "*t") == 0) {
    hello_createtable(L, 0, 9);  /* 9 = number of fields */
    setallfields(L, stm);
  }
  else {
    char cc[4];  /* buffer for individual conversion specifiers */
    helloL_Buffer b;
    cc[0] = '%';
    helloL_buffinit(L, &b);
    while (s < se) {
      if (*s != '%')  /* not a conversion specifier? */
        helloL_addchar(&b, *s++);
      else {
        size_t reslen;
        char *buff = helloL_prepbuffsize(&b, SIZETIMEFMT);
        s++;  /* skip '%' */
        s = checkoption(L, s, se - s, cc + 1);  /* copy specifier to 'cc' */
        reslen = strftime(buff, SIZETIMEFMT, cc, stm);
        helloL_addsize(&b, reslen);
      }
    }
    helloL_pushresult(&b);
  }
  return 1;
}


static int os_time (hello_State *L) {
  time_t t;
  if (hello_isnoneornil(L, 1))  /* called without args? */
    t = time(NULL);  /* get current time */
  else {
    struct tm ts;
    helloL_checktype(L, 1, HELLO_TTABLE);
    hello_settop(L, 1);  /* make sure table is at the top */
    ts.tm_year = getfield(L, "year", -1, 1900);
    ts.tm_mon = getfield(L, "month", -1, 1);
    ts.tm_mday = getfield(L, "day", -1, 0);
    ts.tm_hour = getfield(L, "hour", 12, 0);
    ts.tm_min = getfield(L, "min", 0, 0);
    ts.tm_sec = getfield(L, "sec", 0, 0);
    ts.tm_isdst = getboolfield(L, "isdst");
    t = mktime(&ts);
    setallfields(L, &ts);  /* update fields with normalized values */
  }
  if (t != (time_t)(l_timet)t || t == (time_t)(-1))
    helloL_error(L, "time result cannot be represented in this installation");
  l_pushtime(L, t);
  return 1;
}


static int os_difftime (hello_State *L) {
  time_t t1 = l_checktime(L, 1);
  time_t t2 = l_checktime(L, 2);
  hello_pushnumber(L, (hello_Number)difftime(t1, t2));
  return 1;
}


static int os_unixseconds(hello_State *L) {
  hello_pushinteger(L, (hello_Integer)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
  return 1;
}


static int os_seconds(hello_State* L) {
  hello_pushinteger(L, (hello_Integer)std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
  return 1;
}


static int os_millis(hello_State* L) {
  hello_pushinteger(L, (hello_Integer)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
  return 1;
}


static int os_nanos(hello_State* L) {
  hello_pushinteger(L, (hello_Integer)std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
  return 1;
}


static int os_sleep (hello_State *L) {
  std::chrono::milliseconds timespan(helloL_checkinteger(L, 1));
  std::this_thread::sleep_for(timespan);
  return 0;
}


/* }====================================================== */


static int os_setlocale (hello_State *L) {
  static const int cat[] = {LC_ALL, LC_COLLATE, LC_CTYPE, LC_MONETARY,
                      LC_NUMERIC, LC_TIME};
  static const char *const catnames[] = {"all", "collate", "ctype", "monetary",
     "numeric", "time", NULL};
  const char *l = helloL_optstring(L, 1, NULL);
  int op = helloL_checkoption(L, 2, "all", catnames);
  hello_pushstring(L, setlocale(cat[op], l));
  return 1;
}


static int os_exit (hello_State *L) {
  int status;
  if (hello_isboolean(L, 1))
    status = (hello_toboolean(L, 1) ? EXIT_SUCCESS : EXIT_FAILURE);
  else
    status = (int)helloL_optinteger(L, 1, EXIT_SUCCESS);
  if (hello_toboolean(L, 2))
    hello_close(L);
  if (L) exit(status);  /* 'if' to avoid warnings for unreachable 'return' */
  return 0;
}


int l_remove(hello_State* L);
int l_rename(hello_State* L);

static const helloL_Reg syslib[] = {
  {"sleep",       os_sleep},
  {"clock",       os_clock},
  {"date",        os_date},
  {"difftime",    os_difftime},
  {"execute",     os_execute},
  {"exit",        os_exit},
  {"getenv",      os_getenv},
  {"remove",      l_remove},
  {"rename",      l_rename},
  {"setlocale",   os_setlocale},
  {"time",        os_time},
  {"tmpname",     os_tmpname},
  {"unixseconds", os_unixseconds},
  {"seconds",     os_seconds},
  {"millis",      os_millis},
  {"nanos",       os_nanos},
  {NULL, NULL}
};

/* }====================================================== */



HELLOMOD_API int helloopen_os (hello_State *L) {
  helloL_newlib(L, syslib);
  return 1;
}

