#pragma once
/*
** $Id: helloconf.h $
** Configuration file for Hello
** See Copyright Notice in hello.h
*/

#include <limits.h>
#include <stddef.h>


/*
** ===================================================================
** General Configuration File for Hello
**
** Some definitions here can be changed externally, through the compiler
** (e.g., with '-D' options): They are commented out or protected
** by '#if !defined' guards. However, several other definitions
** should be changed directly here, either because they affect the
** Hello ABI (by making the changes here, you ensure that all software
** connected to Hello, such as C libraries, will be compiled with the same
** configuration); or because they are seldom changed.
**
** Search for "@@" to find all configurable definitions.
** ===================================================================
*/


/*
** {====================================================================
** System Configuration: macros to adapt (if needed) Hello to some
** particular platform, for instance restricting it to C89.
** =====================================================================
*/

/*
@@ HELLO_USE_C89 controls the use of non-ISO-C89 features.
** Define it if you want Hello to avoid the use of a few C99 features
** or Windows-specific features on Windows.
*/
/* #define HELLO_USE_C89 */


/*
** By default, Hello on Windows use (some) specific Windows features
*/
#if !defined(HELLO_USE_C89) && defined(_WIN32) && !defined(_WIN32_WCE)
#define HELLO_USE_WINDOWS  /* enable goodies for regular Windows */
#endif


#if defined(HELLO_USE_WINDOWS)
#define HELLO_DL_DLL	/* enable support for DLL */
#define HELLO_USE_C89	/* broadly, Windows is C89 */
#endif


#if defined(HELLO_USE_LINUX)
#define HELLO_USE_POSIX
#define HELLO_USE_DLOPEN		/* needs an extra library: -ldl */
#endif


#if defined(HELLO_USE_MACOSX)
#define HELLO_USE_POSIX
#define HELLO_USE_DLOPEN		/* MacOS does not need -ldl */
#endif


/*
@@ HELLOI_IS32INT is true iff 'int' has (at least) 32 bits.
*/
#define HELLOI_IS32INT	((UINT_MAX >> 30) >= 3)

/* }================================================================== */



/*
** {==================================================================
** Configuration for Number types. These options should not be
** set externally, because any other code connected to Hello must
** use the same configuration.
** ===================================================================
*/

/*
@@ HELLO_INT_TYPE defines the type for Hello integers.
@@ HELLO_FLOAT_TYPE defines the type for Hello floats.
** Hello should work fine with any mix of these options supported
** by your C compiler. The usual configurations are 64-bit integers
** and 'double' (the default), 32-bit integers and 'float' (for
** restricted platforms), and 'long'/'double' (for C compilers not
** compliant with C99, which may not have support for 'long long').
*/

/* predefined options for HELLO_INT_TYPE */
#define HELLO_INT_INT		1
#define HELLO_INT_LONG		2
#define HELLO_INT_LONGLONG	3

/* predefined options for HELLO_FLOAT_TYPE */
#define HELLO_FLOAT_FLOAT		1
#define HELLO_FLOAT_DOUBLE	2
#define HELLO_FLOAT_LONGDOUBLE	3


/* Default configuration ('long long' and 'double', for 64-bit Hello) */
#define HELLO_INT_DEFAULT		HELLO_INT_LONGLONG
#define HELLO_FLOAT_DEFAULT	HELLO_FLOAT_DOUBLE


/*
@@ HELLO_32BITS enables Hello with 32-bit integers and 32-bit floats.
*/
#define HELLO_32BITS	0


/*
@@ HELLO_C89_NUMBERS ensures that Hello uses the largest types available for
** C89 ('long' and 'double'); Windows always has '__int64', so it does
** not need to use this case.
*/
#if defined(HELLO_USE_C89) && !defined(HELLO_USE_WINDOWS)
#define HELLO_C89_NUMBERS		1
#else
#define HELLO_C89_NUMBERS		0
#endif


#if HELLO_32BITS		/* { */
/*
** 32-bit integers and 'float'
*/
#if HELLOI_IS32INT  /* use 'int' if big enough */
#define HELLO_INT_TYPE	HELLO_INT_INT
#else  /* otherwise use 'long' */
#define HELLO_INT_TYPE	HELLO_INT_LONG
#endif
#define HELLO_FLOAT_TYPE	HELLO_FLOAT_FLOAT

#elif HELLO_C89_NUMBERS	/* }{ */
/*
** largest types available for C89 ('long' and 'double')
*/
#define HELLO_INT_TYPE	HELLO_INT_LONG
#define HELLO_FLOAT_TYPE	HELLO_FLOAT_DOUBLE

#else		/* }{ */
/* use defaults */

#define HELLO_INT_TYPE	HELLO_INT_DEFAULT
#define HELLO_FLOAT_TYPE	HELLO_FLOAT_DEFAULT

#endif				/* } */


/* }================================================================== */



/*
** {==================================================================
** Configuration for Paths.
** ===================================================================
*/

/*
** HELLO_PATH_SEP is the character that separates templates in a path.
** HELLO_PATH_MARK is the string that marks the substitution points in a
** template.
** HELLO_EXEC_DIR in a Windows path is replaced by the executable's
** directory.
*/
#define HELLO_PATH_SEP            ";"
#define HELLO_PATH_MARK           "?"
#define HELLO_EXEC_DIR            "!"


/*
@@ HELLO_PATH_DEFAULT is the default path that Hello uses to look for
** Hello libraries.
@@ HELLO_CPATH_DEFAULT is the default path that Hello uses to look for
** C libraries.
** CHANGE them if your machine has a non-conventional directory
** hierarchy or if you want to install your libraries in
** non-conventional directories.
*/

#define HELLO_VDIR	HELLO_VERSION_MAJOR "." HELLO_VERSION_MINOR
#if defined(_WIN32)	/* { */
/*
** In Windows, any exclamation mark ('!') in the path is replaced by the
** path of the directory of the executable file of the current process.
*/
#define HELLO_LDIR	"!\\hello\\"
#define HELLO_CDIR	"!\\"
#define HELLO_SHRDIR	"!\\..\\share\\hello\\" HELLO_VDIR "\\"

#if !defined(HELLO_PATH_DEFAULT)
#define HELLO_PATH_DEFAULT  \
        HELLO_LDIR"?.hello;"  HELLO_LDIR"?\\init.hello;" \
        HELLO_CDIR"?.hello;"  HELLO_CDIR"?\\init.hello;" \
        HELLO_SHRDIR"?.hello;" HELLO_SHRDIR"?\\init.hello;" \
        ".\\?.hello;" ".\\?\\init.hello;" \
        ".\\?.hello"
#endif

#if !defined(HELLO_CPATH_DEFAULT)
#define HELLO_CPATH_DEFAULT \
        HELLO_CDIR"?.dll;" \
        HELLO_CDIR"..\\lib\\hello\\" HELLO_VDIR "\\?.dll;" \
        HELLO_CDIR"loadall.dll;" ".\\?.dll"
#endif

#else			/* }{ */

#define HELLO_ROOT	"/usr/local/"
#define HELLO_LDIR	HELLO_ROOT "share/hello/" HELLO_VDIR "/"
#define HELLO_CDIR	HELLO_ROOT "lib/hello/" HELLO_VDIR "/"

#if !defined(HELLO_PATH_DEFAULT)
#define HELLO_PATH_DEFAULT  \
        HELLO_LDIR"?.hello;"  HELLO_LDIR"?/init.hello;" \
        HELLO_CDIR"?.hello;"  HELLO_CDIR"?/init.hello;" \
        "./?.hello;" "./?/init.hello;" \
        "./?.hello"
#endif

#if !defined(HELLO_CPATH_DEFAULT)
#define HELLO_CPATH_DEFAULT \
        HELLO_CDIR"?.so;" HELLO_CDIR"loadall.so;" "./?.so"
#endif

#endif			/* } */


/*
@@ HELLO_DIRSEP is the directory separator (for submodules).
** CHANGE it if your machine does not use "/" as the directory separator
** and is not Windows. (On Windows Hello automatically uses "\".)
*/
#if !defined(HELLO_DIRSEP)

#if defined(_WIN32)
#define HELLO_DIRSEP	"\\"
#else
#define HELLO_DIRSEP	"/"
#endif

#endif

/* }================================================================== */


/*
** {==================================================================
** Marks for exported symbols in the C code
** ===================================================================
*/

/*
@@ HELLO_API is a mark for all core API functions.
@@ HELLOLIB_API is a mark for all auxiliary library functions.
@@ HELLOMOD_API is a mark for all standard library opening functions.
** CHANGE them if you need to define those functions in some special way.
** For instance, if you want to create one Windows DLL with the core and
** the libraries, you may want to use the following definition (define
** HELLO_BUILD_AS_DLL to get it).
*/
#if defined(HELLO_BUILD_AS_DLL)	/* { */

#if defined(HELLO_CORE) || defined(HELLO_LIB)	/* { */
#define HELLO_API __declspec(dllexport)
#else						/* }{ */
#define HELLO_API __declspec(dllimport)
#endif						/* } */

#else				/* }{ */

#define HELLO_API		extern

#endif				/* } */


/*
** More often than not the libs go together with the core.
*/
#define HELLOLIB_API	HELLO_API
#define HELLOMOD_API	HELLO_API


/*
@@ HELLOI_FUNC is a mark for all extern functions that are not to be
** exported to outside modules.
@@ HELLOI_DDEF and HELLOI_DDEC are marks for all extern (const) variables,
** none of which to be exported to outside modules (HELLOI_DDEF for
** definitions and HELLOI_DDEC for declarations).
** CHANGE them if you need to mark them in some special way. Elf/gcc
** (versions 3.2 and later) mark them as "hidden" to optimize access
** when Hello is compiled as a shared library. Not all elf targets support
** this attribute. Unfortunately, gcc does not offer a way to check
** whether the target offers that support, and those without support
** give a warning about it. To avoid these warnings, change to the
** default definition.
*/
#if defined(__GNUC__) && ((__GNUC__*100 + __GNUC_MINOR__) >= 302) && \
    defined(__ELF__)		/* { */
#define HELLOI_FUNC	__attribute__((visibility("internal"))) extern
#else				/* }{ */
#define HELLOI_FUNC	extern
#endif				/* } */

#define HELLOI_DDEC(dec)	HELLOI_FUNC dec
#define HELLOI_DDEF	/* empty */

/* }================================================================== */


/*
** {==================================================================
** Compatibility with previous versions
** ===================================================================
*/

/*
@@ HELLO_COMPAT_5_3 controls other macros for compatibility with Hello 5.3.
** You can define it to get all options, or change specific options
** to fit your specific needs.
*/
#if defined(HELLO_COMPAT_5_3)	/* { */

/*
@@ HELLO_COMPAT_MATHLIB controls the presence of several deprecated
** functions in the mathematical library.
** (These functions were already officially removed in 5.3;
** nevertheless they are still available here.)
*/
#define HELLO_COMPAT_MATHLIB

/*
@@ HELLO_COMPAT_APIINTCASTS controls the presence of macros for
** manipulating other integer types (hello_pushunsigned, hello_tounsigned,
** helloL_checkint, helloL_checklong, etc.)
** (These macros were also officially removed in 5.3, but they are still
** available here.)
*/
#define HELLO_COMPAT_APIINTCASTS


/*
@@ HELLO_COMPAT_LT_LE controls the emulation of the '__le' metamethod
** using '__lt'.
*/
#define HELLO_COMPAT_LT_LE


/*
@@ The following macros supply trivial compatibility for some
** changes in the API. The macros themselves document how to
** change your code to avoid using them.
** (Once more, these macros were officially removed in 5.3, but they are
** still available here.)
*/
#define hello_strlen(L,i)		hello_rawlen(L, (i))

#define hello_objlen(L,i)		hello_rawlen(L, (i))

#define hello_equal(L,idx1,idx2)		hello_compare(L,(idx1),(idx2),HELLO_OPEQ)
#define hello_lessthan(L,idx1,idx2)	hello_compare(L,(idx1),(idx2),HELLO_OPLT)

#endif				/* } */

/* }================================================================== */



/*
** {==================================================================
** Configuration for Numbers (low-level part).
** Change these definitions if no predefined HELLO_FLOAT_* / HELLO_INT_*
** satisfy your needs.
** ===================================================================
*/

/*
@@ HELLOI_UACNUMBER is the result of a 'default argument promotion'
@@ over a floating number.
@@ l_floatatt(x) corrects float attribute 'x' to the proper float type
** by prefixing it with one of FLT/DBL/LDBL.
@@ HELLO_NUMBER_FRMLEN is the length modifier for writing floats.
@@ HELLO_NUMBER_FMT is the format for writing floats.
@@ hello_number2str converts a float to a string.
@@ l_mathop allows the addition of an 'l' or 'f' to all math operations.
@@ l_floor takes the floor of a float.
@@ hello_str2number converts a decimal numeral to a number.
*/


/* The following definitions are good for most cases here */

#define l_floor(x)		(l_mathop(floor)(x))

#define hello_number2str(s,sz,n)  \
    l_sprintf((s), sz, HELLO_NUMBER_FMT, (HELLOI_UACNUMBER)(n))

/*
@@ hello_numbertointeger converts a float number with an integral value
** to an integer, or returns 0 if float is not within the range of
** a hello_Integer.  (The range comparisons are tricky because of
** rounding. The tests here assume a two-complement representation,
** where MININTEGER always has an exact representation as a float;
** MAXINTEGER may not have one, and therefore its conversion to float
** may have an ill-defined value.)
*/
#define hello_numbertointeger(n,p) \
  ((n) >= (HELLO_NUMBER)(HELLO_MININTEGER) && \
   (n) < -(HELLO_NUMBER)(HELLO_MININTEGER) && \
      (*(p) = (HELLO_INTEGER)(n), 1))


/* now the variable definitions */

#if HELLO_FLOAT_TYPE == HELLO_FLOAT_FLOAT		/* { single float */

#define HELLO_NUMBER	float

#define l_floatatt(n)		(FLT_##n)

#define HELLOI_UACNUMBER	double

#define HELLO_NUMBER_FRMLEN	""
#define HELLO_NUMBER_FMT		"%.7g"

#define l_mathop(op)		op##f

#define hello_str2number(s,p)	strtof((s), (p))


#elif HELLO_FLOAT_TYPE == HELLO_FLOAT_LONGDOUBLE	/* }{ long double */

#define HELLO_NUMBER	long double

#define l_floatatt(n)		(LDBL_##n)

#define HELLOI_UACNUMBER	long double

#define HELLO_NUMBER_FRMLEN	"L"
#define HELLO_NUMBER_FMT		"%.19Lg"

#define l_mathop(op)		op##l

#define hello_str2number(s,p)	strtold((s), (p))

#elif HELLO_FLOAT_TYPE == HELLO_FLOAT_DOUBLE	/* }{ double */

#define HELLO_NUMBER	double

#define l_floatatt(n)		(DBL_##n)

#define HELLOI_UACNUMBER	double

#define HELLO_NUMBER_FRMLEN	""
#define HELLO_NUMBER_FMT		"%.14g"

#define l_mathop(op)		op

#define hello_str2number(s,p)	strtod((s), (p))

#else						/* }{ */

#error "numeric float type not defined"

#endif					/* } */



/*
@@ HELLO_UNSIGNED is the unsigned version of HELLO_INTEGER.
@@ HELLOI_UACINT is the result of a 'default argument promotion'
@@ over a HELLO_INTEGER.
@@ HELLO_INTEGER_FRMLEN is the length modifier for reading/writing integers.
@@ HELLO_INTEGER_FMT is the format for writing integers.
@@ HELLO_MAXINTEGER is the maximum value for a HELLO_INTEGER.
@@ HELLO_MININTEGER is the minimum value for a HELLO_INTEGER.
@@ HELLO_MAXUNSIGNED is the maximum value for a HELLO_UNSIGNED.
@@ hello_integer2str converts an integer to a string.
*/


/* The following definitions are good for most cases here */

#define HELLO_INTEGER_FMT		"%" HELLO_INTEGER_FRMLEN "d"

#define HELLOI_UACINT		HELLO_INTEGER

#define hello_integer2str(s,sz,n)  \
    l_sprintf((s), sz, HELLO_INTEGER_FMT, (HELLOI_UACINT)(n))

/*
** use HELLOI_UACINT here to avoid problems with promotions (which
** can turn a comparison between unsigneds into a signed comparison)
*/
#define HELLO_UNSIGNED		unsigned HELLOI_UACINT


/* now the variable definitions */

#if HELLO_INT_TYPE == HELLO_INT_INT		/* { int */

#define HELLO_INTEGER		int
#define HELLO_INTEGER_FRMLEN	""

#define HELLO_MAXINTEGER		INT_MAX
#define HELLO_MININTEGER		INT_MIN

#define HELLO_MAXUNSIGNED		UINT_MAX

#elif HELLO_INT_TYPE == HELLO_INT_LONG	/* }{ long */

#define HELLO_INTEGER		long
#define HELLO_INTEGER_FRMLEN	"l"

#define HELLO_MAXINTEGER		LONG_MAX
#define HELLO_MININTEGER		LONG_MIN

#define HELLO_MAXUNSIGNED		ULONG_MAX

#elif HELLO_INT_TYPE == HELLO_INT_LONGLONG	/* }{ long long */

/* use presence of macro LLONG_MAX as proxy for C99 compliance */
#if defined(LLONG_MAX)		/* { */
/* use ISO C99 stuff */

#define HELLO_INTEGER		long long
#define HELLO_INTEGER_FRMLEN	"ll"

#define HELLO_MAXINTEGER		LLONG_MAX
#define HELLO_MININTEGER		LLONG_MIN

#define HELLO_MAXUNSIGNED		ULLONG_MAX

#elif defined(HELLO_USE_WINDOWS) /* }{ */
/* in Windows, can use specific Windows types */

#define HELLO_INTEGER		__int64
#define HELLO_INTEGER_FRMLEN	"I64"

#define HELLO_MAXINTEGER		_I64_MAX
#define HELLO_MININTEGER		_I64_MIN

#define HELLO_MAXUNSIGNED		_UI64_MAX

#else				/* }{ */

#error "Compiler does not support 'long long'. Use option '-DHELLO_32BITS' \
  or '-DHELLO_C89_NUMBERS' (see file 'helloconf.h' for details)"

#endif				/* } */

#else				/* }{ */

#error "numeric integer type not defined"

#endif				/* } */

/* }================================================================== */


/*
** {==================================================================
** Dependencies with C99 and other C details
** ===================================================================
*/

/*
@@ l_sprintf is equivalent to 'snprintf' or 'sprintf' in C89.
** (All uses in Hello have only one format item.)
*/
#if !defined(HELLO_USE_C89)
#define l_sprintf(s,sz,f,i)	snprintf(s,sz,f,i)
#else
#define l_sprintf(s,sz,f,i)	((void)(sz), sprintf(s,f,i))
#endif


/*
@@ hello_strx2number converts a hexadecimal numeral to a number.
** In C99, 'strtod' does that conversion. Otherwise, you can
** leave 'hello_strx2number' undefined and Hello will provide its own
** implementation.
*/
#if !defined(HELLO_USE_C89)
#define hello_strx2number(s,p)		hello_str2number(s,p)
#endif


/*
@@ hello_pointer2str converts a pointer to a readable string in a
** non-specified way.
*/
#define hello_pointer2str(buff,sz,p)	l_sprintf(buff,sz,"%p",p)


/*
@@ hello_number2strx converts a float to a hexadecimal numeral.
** In C99, 'sprintf' (with format specifiers '%a'/'%A') does that.
** Otherwise, you can leave 'hello_number2strx' undefined and Hello will
** provide its own implementation.
*/
#if !defined(HELLO_USE_C89)
#define hello_number2strx(L,b,sz,f,n)  \
    ((void)L, l_sprintf(b,sz,f,(HELLOI_UACNUMBER)(n)))
#endif


/*
** 'strtof' and 'opf' variants for math functions are not valid in
** C89. Otherwise, the macro 'HUGE_VALF' is a good proxy for testing the
** availability of these variants. ('math.h' is already included in
** all files that use these macros.)
*/
#if defined(HELLO_USE_C89) || (defined(HUGE_VAL) && !defined(HUGE_VALF))
#undef l_mathop  /* variants not available */
#undef hello_str2number
#define l_mathop(op)		(hello_Number)op  /* no variant */
#define hello_str2number(s,p)	((hello_Number)strtod((s), (p)))
#endif


/*
@@ HELLO_KCONTEXT is the type of the context ('ctx') for continuation
** functions.  It must be a numerical type; Hello will use 'intptr_t' if
** available, otherwise it will use 'ptrdiff_t' (the nearest thing to
** 'intptr_t' in C89)
*/
#define HELLO_KCONTEXT	ptrdiff_t

#if !defined(HELLO_USE_C89) && defined(__STDC_VERSION__) && \
    __STDC_VERSION__ >= 199901L
#include <stdint.h>
#if defined(INTPTR_MAX)  /* even in C99 this type is optional */
#undef HELLO_KCONTEXT
#define HELLO_KCONTEXT	intptr_t
#endif
#endif


/*
@@ hello_getlocaledecpoint gets the locale "radix character" (decimal point).
** Change that if you do not want to use C locales. (Code using this
** macro must include the header 'locale.h'.)
*/
#if !defined(hello_getlocaledecpoint)
#define hello_getlocaledecpoint()		(localeconv()->decimal_point[0])
#endif


/*
** macros to improve jump prediction, used mostly for error handling
** and debug facilities. (Some macros in the Hello API use these macros.
** Define HELLO_NOBUILTIN if you do not want '__builtin_expect' in your
** code.)
*/
#if !defined(helloi_likely)

#if defined(__GNUC__) && !defined(HELLO_NOBUILTIN)
#define helloi_likely(x)		(__builtin_expect(((x) != 0), 1))
#define helloi_unlikely(x)	(__builtin_expect(((x) != 0), 0))
#else
#define helloi_likely(x)		(x)
#define helloi_unlikely(x)	(x)
#endif

#endif


#if defined(HELLO_CORE) || defined(HELLO_LIB)
/* shorter names for Hello's own use */
#define l_likely(x)	helloi_likely(x)
#define l_unlikely(x)	helloi_unlikely(x)
#endif



/* }================================================================== */


/*
** {==================================================================
** Language Variations
** =====================================================================
*/

/*
@@ HELLO_NOCVTN2S/HELLO_NOCVTS2N control how Hello performs some
** coercions. Define HELLO_NOCVTN2S to turn off automatic coercion from
** numbers to strings. Define HELLO_NOCVTS2N to turn off automatic
** coercion from strings to numbers.
*/
/* #define HELLO_NOCVTN2S */
/* #define HELLO_NOCVTS2N */


/*
@@ HELLO_USE_APICHECK turns on several consistency checks on the C API.
** Define it as a help when debugging C code.
*/
#if defined(HELLO_USE_APICHECK)
#include <assert.h>
#define helloi_apicheck(l,e)	assert(e)
#endif

/* }================================================================== */


/*
** {==================================================================
** Macros that affect the API and must be stable (that is, must be the
** same when you compile Hello and when you compile code that links to
** Hello).
** =====================================================================
*/

/*
@@ HELLOI_MAXSTACK limits the size of the Hello stack.
** CHANGE it if you need a different limit. This limit is arbitrary;
** its only purpose is to stop Hello from consuming unlimited stack
** space (and to reserve some numbers for pseudo-indices).
** (It must fit into max(size_t)/32.)
*/
#if HELLOI_IS32INT
#define HELLOI_MAXSTACK		1000000
#else
#define HELLOI_MAXSTACK		15000
#endif


/*
@@ HELLO_EXTRASPACE defines the size of a raw memory area associated with
** a Hello state with very fast access.
** CHANGE it if you need a different size.
*/
#define HELLO_EXTRASPACE		(sizeof(void *))


/*
@@ HELLO_IDSIZE gives the maximum size for the description of the source
** of a function in debug information.
** CHANGE it if you want a different size.
*/
#define HELLO_IDSIZE	60


/*
@@ HELLOL_BUFFERSIZE is the initial buffer size used by the lauxlib
** buffer system.
*/
#define HELLOL_BUFFERSIZE   ((int)(16 * sizeof(void*) * sizeof(hello_Number)))


/*
@@ HELLOI_MAXALIGN defines fields that, when used in a union, ensure
** maximum alignment for the other items in that union.
*/
#define HELLOI_MAXALIGN  hello_Number n; double u; void *s; hello_Integer i; long l

/* }================================================================== */

/*
** {====================================================================
** hello configuration
** =====================================================================}
*/

// If defined, hello won't emit parser warnings.
//#define HELLO_NO_PARSER_WARNINGS

// If defined, hello errors will use ANSI color codes.
//#define HELLO_USE_COLORED_OUTPUT

// If defined, hello will exclude code snippets from error messages to make them shorter.
//#define HELLO_SHORT_ERRORS

// If defined, hello won't assume that source files are UTF-8 encoded and restrict valid symbol names.
//#define HELLO_NO_UTF8

// If defined, hello will use a jumptable in the VM even if not compiled via GCC.
// This will generally improve runtime performance but can add minutes to compile time, depending on the setup.
//#define HELLO_FORCE_JUMPTABLE

// If defined, hello will use C++ exceptions to implement Hello longjumps.
// This is generally slower and complicates exception handling.
//#define HELLO_USE_THROW

/*
** {====================================================================
** hello configuration: Compatible Mode
** =====================================================================}
*/

// If defined, hello will assign 'hello_' to new keywords which break previously valid Hello identifiers.
// If you decide to leave this undefined:
//     - Both 'hello_switch' and 'switch' are valid syntaxes for hello.
//     - Keywords like 'continue' remain keywords.
// If you decide to define this:
//     - Only 'hello_switch' will be valid. 'switch' will not exist.
//     - Keywords like 'continue' will now be 'hello_continue'.
//#define HELLO_COMPATIBLE_MODE

#ifdef HELLO_COMPATIBLE_MODE

// If defined, only 'hello_switch' will be valid. 'switch' will not exist.
#define HELLO_COMPATIBLE_SWITCH

// If defined, only 'hello_... you get the idea.
#define HELLO_COMPATIBLE_CONTINUE

#define HELLO_COMPATIBLE_WHEN

#define HELLO_COMPATIBLE_ENUM

#endif // HELLO_COMPATIBLE_MODE

/*
** {====================================================================
** hello configuration: Infinite Loop Prevention (ILP)
**
** This is only useful in game regions, where a long loop may block the main thread and crash the game.
** These places usually implement a yield (or wait) function, which can be detected and hooked to reset iterations.
** =====================================================================}
*/

// If defined, hello will attempt to prevent infinite loops.
//#define HELLO_ILP_ENABLE

#ifdef HELLO_ILP_ENABLE
/*
** This is the maximum amount of backward jumps permitted in a singular loop block.
** If exceeded, the backward jump is ignored to escape the loop.
*/
#ifndef HELLO_ILP_MAX_ITERATIONS
#define HELLO_ILP_MAX_ITERATIONS			1000000
#endif

// If you want (i.e) `helloB_next` to reset iteration counters, define as `helloB_next`.
// #define HELLO_ILP_HOOK_FUNCTION		helloB_next

// If defined, hello won't throw an error and instead just break out of the loop.
//#define HELLO_ILP_SILENT_BREAK

#endif // HELLO_ILP_ENABLE

/*
** {====================================================================
** hello configuration: Execution Time Limit (ETL)
**
** This is only useful in sandbox environments where stalling is absolutely unacceptable.
** =====================================================================}
*/

//#define HELLO_ETL_ENABLE

#ifdef HELLO_ETL_ENABLE
/*
** This is the maximum amount of nanoseconds the VM is allowed to run.
*/
#ifndef HELLO_ETL_NANOS
#define HELLO_ETL_NANOS			1'000'000 /* 1ms */
#endif

/*
** This can be used to execute custom code when the time limit is exceeded and
** the VM is about to be terminated.
*/
#ifndef HELLO_ETL_TIMESUP
#define HELLO_ETL_TIMESUP
#endif
#endif

/*
** {====================================================================
** hello configuration: VM Dump
** =====================================================================}
*/

// If defined, hello will print every VM instruction that is ran.
// Note that you can modify hello_writestring to redirect output.
//#define HELLO_VMDUMP

#ifdef HELLO_VMDUMP
/* Example:
**  #define vmDumpIgnore \
**      OP_LOADI \
**      OP_LOADF
*/

// Opcodes listed in this structure are a blacklist. They not be printed when VM dumping.
#define vmDumpIgnore


// Opcodes listed in this structure are a whitelist. They are only printed when VM dumping.
#define vmDumpAllow

// If defined, hello will use vmDumpAllow instead of vmDumpIgnore.
//#define HELLO_VMDUMP_WHITELIST

// Defines under what circumstances the VM Dump is active.
#ifndef HELLO_VMDUMP_COND
#define HELLO_VMDUMP_COND(L) true
#endif

#endif // HELLO_VMDUMP

/*
** {====================================================================
** hello configuration: Content Moderation
** =====================================================================}
*/

// If defined, hello will not load compiled Hello or hello code.
//#define HELLO_DISABLE_COMPILED

// If defined, the provided function will be called as bool(const char* filename).
// It needs to have C ABI linkage (extern "C").
// If it returns false, a Hello error is raised.
// This will affect require and dofile.
//#define HELLO_LOADFILE_HOOK ContmodOnLoadfile

/*
** {====================================================================
** hello color macros.
** =====================================================================}
*/

#ifdef HELLO_USE_COLORED_OUTPUT // Don't need to write any 'ifdef' macro logic inside of Hello::ErrorMessage.
#define ESC "\x1B"

#define BLK ESC "[0;30m"
#define RED ESC "[0;31m"
#define GRN ESC "[0;32m"
#define YEL ESC "[0;33m"
#define BLU ESC "[0;34m"
#define MAG ESC "[0;35m"
#define CYN ESC "[0;36m"
#define WHT ESC "[0;37m"

//Regular bold text
#define BBLK ESC "[1;30m"
#define BRED ESC "[1;31m"
#define BGRN ESC "[1;32m"
#define BYEL ESC "[1;33m"
#define BBLU ESC "[1;34m"
#define BMAG ESC "[1;35m"
#define BCYN ESC "[1;36m"
#define BWHT ESC "[1;37m"

//Regular underline text
#define UBLK ESC "[4;30m"
#define URED ESC "[4;31m"
#define UGRN ESC "[4;32m"
#define UYEL ESC "[4;33m"
#define UBLU ESC "[4;34m"
#define UMAG ESC "[4;35m"
#define UCYN ESC "[4;36m"
#define UWHT ESC "[4;37m"

//Regular background
#define BLKB ESC "[40m"
#define REDB ESC "[41m"
#define GRNB ESC "[42m"
#define YELB ESC "[43m"
#define BLUB ESC "[44m"
#define MAGB ESC "[45m"
#define CYNB ESC "[46m"
#define WHTB ESC "[47m"

//High intensty background 
#define BLKHB ESC "[0;100m"
#define REDHB ESC "[0;101m"
#define GRNHB ESC "[0;102m"
#define YELHB ESC "[0;103m"
#define BLUHB ESC "[0;104m"
#define MAGHB ESC "[0;105m"
#define CYNHB ESC "[0;106m"
#define WHTHB ESC "[0;107m"

//High intensty text
#define HBLK ESC "[0;90m"
#define HRED ESC "[0;91m"
#define HGRN ESC "[0;92m"
#define HYEL ESC "[0;93m"
#define HBLU ESC "[0;94m"
#define HMAG ESC "[0;95m"
#define HCYN ESC "[0;96m"
#define HWHT ESC "[0;97m"

//Bold high intensity text
#define BHBLK ESC "[1;90m"
#define BHRED ESC "[1;91m"
#define BHGRN ESC "[1;92m"
#define BHYEL ESC "[1;93m"
#define BHBLU ESC "[1;94m"
#define BHMAG ESC "[1;95m"
#define BHCYN ESC "[1;96m"
#define BHWHT ESC "[1;97m"

//Reset
#define RESET ESC "[0m"
#define CRESET ESC "[0m"
#define COLOR_RESET ESC "[0m"
#else // HELLO_USE_COLORED_OUTPUT
#define ESC ""
#define BLK ESC
#define RED ESC
#define GRN ESC
#define YEL ESC
#define BLU ESC
#define MAG ESC
#define CYN ESC
#define WHT ESC
#define BBLK ESC
#define BRED ESC
#define BGRN ESC
#define BYEL ESC
#define BBLU ESC
#define BMAG ESC
#define BCYN ESC
#define BWHT ESC
#define UBLK ESC
#define URED ESC
#define UGRN ESC
#define UYEL ESC
#define UBLU ESC
#define UMAG ESC
#define UCYN ESC
#define UWHT ESC
#define BLKB ESC
#define REDB ESC
#define GRNB ESC
#define YELB ESC
#define BLUB ESC
#define MAGB ESC
#define CYNB ESC
#define WHTB ESC
#define BLKHB ESC
#define REDHB ESC
#define GRNHB ESC
#define YELHB ESC
#define BLUHB ESC
#define MAGHB ESC
#define CYNHB ESC
#define WHTHB ESC
#define HBLK ESC
#define HRED ESC
#define HGRN ESC
#define HYEL ESC
#define HBLU ESC
#define HMAG ESC
#define HCYN ESC
#define HWHT ESC
#define BHBLK ESC
#define BHRED ESC
#define BHGRN ESC
#define BHYEL ESC
#define BHBLU ESC
#define BHMAG ESC
#define BHCYN ESC
#define BHWHT ESC
#define RESET ESC
#define CRESET ESC
#define COLOR_RESET ESC
#endif // HELLO_USE_COLORED_OUTPUT


/* }================================================================== */
