/*
** $Id: loslib.c $
** Standard Operating System library
** See Copyright Notice in mask.h
*/

#define loslib_c
#define MASK_LIB

#include "lprefix.h"

#include <thread>
#include <chrono>
#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mask.h"

#include "lauxlib.h"
#include "masklib.h"


/*
** {==================================================================
** List of valid conversion specifiers for the 'strftime' function;
** options are grouped by length; group of length 2 start with '||'.
** ===================================================================
*/
#if !defined(MASK_STRFTIMEOPTIONS)	/* { */

/* options for ANSI C 89 (only 1-char options) */
#define L_STRFTIMEC89		"aAbBcdHIjmMpSUwWxXyYZ%"

/* options for ISO C 99 and POSIX */
#define L_STRFTIMEC99 "aAbBcCdDeFgGhHIjmMnprRStTuUVwWxXyYzZ%" \
    "||" "EcECExEXEyEY" "OdOeOHOIOmOMOSOuOUOVOwOWOy"  /* two-char options */

/* options for Windows */
#define L_STRFTIMEWIN "aAbBcdHIjmMpSUwWxXyYzZ%" \
    "||" "#c#x#d#H#I#j#m#M#S#U#w#W#y#Y"  /* two-char options */

#if defined(MASK_USE_WINDOWS)
#define MASK_STRFTIMEOPTIONS	L_STRFTIMEWIN
#elif defined(MASK_USE_C89)
#define MASK_STRFTIMEOPTIONS	L_STRFTIMEC89
#else  /* C99 specification */
#define MASK_STRFTIMEOPTIONS	L_STRFTIMEC99
#endif

#endif					/* } */
/* }================================================================== */


/*
** {==================================================================
** Configuration for time-related stuff
** ===================================================================
*/

/*
** type to represent time_t in Mask
*/
#if !defined(MASK_NUMTIME)	/* { */

#define l_timet			mask_Integer
#define l_pushtime(L,t)		mask_pushinteger(L,(mask_Integer)(t))
#define l_gettime(L,arg)	maskL_checkinteger(L, arg)

#else				/* }{ */

#define l_timet			mask_Number
#define l_pushtime(L,t)		mask_pushnumber(L,(mask_Number)(t))
#define l_gettime(L,arg)	maskL_checknumber(L, arg)

#endif				/* } */


#if !defined(l_gmtime)		/* { */
/*
** By default, Mask uses gmtime/localtime, except when POSIX is available,
** where it uses gmtime_r/localtime_r
*/

#if defined(MASK_USE_POSIX)	/* { */

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
** By default, Mask uses tmpnam except when POSIX is available, where
** it uses mkstemp.
** ===================================================================
*/
#if !defined(mask_tmpnam)	/* { */

#if defined(MASK_USE_POSIX)	/* { */

#include <unistd.h>

#define MASK_TMPNAMBUFSIZE	32

#if !defined(MASK_TMPNAMTEMPLATE)
#define MASK_TMPNAMTEMPLATE	"/tmp/mask_XXXXXX"
#endif

#define mask_tmpnam(b,e) { \
        strcpy(b, MASK_TMPNAMTEMPLATE); \
        e = mkstemp(b); \
        if (e != -1) close(e); \
        e = (e == -1); }

#else				/* }{ */

/* ISO C definitions */
#define MASK_TMPNAMBUFSIZE	L_tmpnam
#define mask_tmpnam(b,e)		{ e = (tmpnam(b) == NULL); }

#endif				/* } */

#endif				/* } */
/* }================================================================== */



static int os_execute (mask_State *L) {
  const char *cmd = maskL_optstring(L, 1, NULL);
  int stat;
  errno = 0;
  stat = system(cmd);
  if (cmd != NULL)
    return maskL_execresult(L, stat);
  else {
    mask_pushboolean(L, stat);  /* true if there is a shell */
    return 1;
  }
}


static int os_tmpname (mask_State *L) {
  char buff[MASK_TMPNAMBUFSIZE];
  int err;
  mask_tmpnam(buff, err);
  if (l_unlikely(err))
    maskL_error(L, "unable to generate a unique filename");
  mask_pushstring(L, buff);
  return 1;
}


static int os_getenv (mask_State *L) {
  mask_pushstring(L, getenv(maskL_checkstring(L, 1)));  /* if NULL push nil */
  return 1;
}


static int os_clock (mask_State *L) {
  mask_pushnumber(L, ((mask_Number)clock())/(mask_Number)CLOCKS_PER_SEC);
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
** is represented by a mask_Integer, because either mask_Integer is
** large enough to represent all int fields or it is not large enough
** to represent a time that cause a field to overflow.  However, if
** times are represented as doubles and mask_Integer is int, then the
** time 0x1.e1853b0d184f6p+55 would cause an overflow when adding 1900
** to compute the year.
*/
static void setfield (mask_State *L, const char *key, int value, int delta) {
  #if (defined(MASK_NUMTIME) && MASK_MAXINTEGER <= INT_MAX)
    if (l_unlikely(value > MASK_MAXINTEGER - delta))
      maskL_error(L, "field '%s' is out-of-bound", key);
  #endif
  mask_pushinteger(L, (mask_Integer)value + delta);
  mask_setfield(L, -2, key);
}


static void setboolfield (mask_State *L, const char *key, int value) {
  if (value < 0)  /* undefined? */
    return;  /* does not set field */
  mask_pushboolean(L, value);
  mask_setfield(L, -2, key);
}


/*
** Set all fields from structure 'tm' in the table on top of the stack
*/
static void setallfields (mask_State *L, struct tm *stm) {
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


static int getboolfield (mask_State *L, const char *key) {
  int res;
  res = (mask_getfield(L, -1, key) == MASK_TNIL) ? -1 : mask_toboolean(L, -1);
  mask_pop(L, 1);
  return res;
}


static int getfield (mask_State *L, const char *key, int d, int delta) {
  int isnum;
  int t = mask_getfield(L, -1, key);  /* get field and its type */
  mask_Integer res = mask_tointegerx(L, -1, &isnum);
  if (!isnum) {  /* field is not an integer? */
    if (l_unlikely(t != MASK_TNIL))  /* some other value? */
      maskL_error(L, "field '%s' is not an integer", key);
    else if (l_unlikely(d < 0))  /* absent field; no default? */
      maskL_error(L, "field '%s' missing in date table", key);
    res = d;
  }
  else {
    if (!(res >= 0 ? res - delta <= INT_MAX : INT_MIN + delta <= res))
      maskL_error(L, "field '%s' is out-of-bound", key);
    res -= delta;
  }
  mask_pop(L, 1);
  return (int)res;
}


static const char *checkoption (mask_State *L, const char *conv,
                                ptrdiff_t convlen, char *buff) {
  const char *option = MASK_STRFTIMEOPTIONS;
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
  maskL_argerror(L, 1,
    mask_pushfstring(L, "invalid conversion specifier '%%%s'", conv));
  return conv;  /* to avoid warnings */
}


static time_t l_checktime (mask_State *L, int arg) {
  l_timet t = l_gettime(L, arg);
  maskL_argcheck(L, (time_t)t == t, arg, "time out-of-bounds");
  return (time_t)t;
}


/* maximum size for an individual 'strftime' item */
#define SIZETIMEFMT	250


static int os_date (mask_State *L) {
  size_t slen;
  const char *s = maskL_optlstring(L, 1, "%c", &slen);
  time_t t = maskL_opt(L, l_checktime, 2, time(NULL));
  const char *se = s + slen;  /* 's' end */
  struct tm tmr, *stm;
  if (*s == '!') {  /* UTC? */
    stm = l_gmtime(&t, &tmr);
    s++;  /* skip '!' */
  }
  else
    stm = l_localtime(&t, &tmr);
  if (stm == NULL)  /* invalid date? */
    maskL_error(L, "date result cannot be represented in this installation");
  if (strcmp(s, "*t") == 0) {
    mask_createtable(L, 0, 9);  /* 9 = number of fields */
    setallfields(L, stm);
  }
  else {
    char cc[4];  /* buffer for individual conversion specifiers */
    maskL_Buffer b;
    cc[0] = '%';
    maskL_buffinit(L, &b);
    while (s < se) {
      if (*s != '%')  /* not a conversion specifier? */
        maskL_addchar(&b, *s++);
      else {
        size_t reslen;
        char *buff = maskL_prepbuffsize(&b, SIZETIMEFMT);
        s++;  /* skip '%' */
        s = checkoption(L, s, se - s, cc + 1);  /* copy specifier to 'cc' */
        reslen = strftime(buff, SIZETIMEFMT, cc, stm);
        maskL_addsize(&b, reslen);
      }
    }
    maskL_pushresult(&b);
  }
  return 1;
}


static int os_time (mask_State *L) {
  time_t t;
  if (mask_isnoneornil(L, 1))  /* called without args? */
    t = time(NULL);  /* get current time */
  else {
    struct tm ts;
    maskL_checktype(L, 1, MASK_TTABLE);
    mask_settop(L, 1);  /* make sure table is at the top */
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
    maskL_error(L, "time result cannot be represented in this installation");
  l_pushtime(L, t);
  return 1;
}


static int os_difftime (mask_State *L) {
  time_t t1 = l_checktime(L, 1);
  time_t t2 = l_checktime(L, 2);
  mask_pushnumber(L, (mask_Number)difftime(t1, t2));
  return 1;
}


static int os_unixseconds(mask_State *L) {
  mask_pushinteger(L, (mask_Integer)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
  return 1;
}


static int os_seconds(mask_State* L) {
  mask_pushinteger(L, (mask_Integer)std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
  return 1;
}


static int os_millis(mask_State* L) {
  mask_pushinteger(L, (mask_Integer)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
  return 1;
}


static int os_nanos(mask_State* L) {
  mask_pushinteger(L, (mask_Integer)std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
  return 1;
}


static int os_sleep (mask_State *L) {
  std::chrono::milliseconds timespan(maskL_checkinteger(L, 1));
  std::this_thread::sleep_for(timespan);
  return 0;
}


/* }====================================================== */


static int os_setlocale (mask_State *L) {
  static const int cat[] = {LC_ALL, LC_COLLATE, LC_CTYPE, LC_MONETARY,
                      LC_NUMERIC, LC_TIME};
  static const char *const catnames[] = {"all", "collate", "ctype", "monetary",
     "numeric", "time", NULL};
  const char *l = maskL_optstring(L, 1, NULL);
  int op = maskL_checkoption(L, 2, "all", catnames);
  mask_pushstring(L, setlocale(cat[op], l));
  return 1;
}


static int os_exit (mask_State *L) {
  int status;
  if (mask_isboolean(L, 1))
    status = (mask_toboolean(L, 1) ? EXIT_SUCCESS : EXIT_FAILURE);
  else
    status = (int)maskL_optinteger(L, 1, EXIT_SUCCESS);
  if (mask_toboolean(L, 2))
    mask_close(L);
  if (L) exit(status);  /* 'if' to avoid warnings for unreachable 'return' */
  return 0;
}


int l_remove(mask_State* L);
int l_rename(mask_State* L);

static const maskL_Reg syslib[] = {
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



MASKMOD_API int maskopen_os (mask_State *L) {
  maskL_newlib(L, syslib);
  return 1;
}

