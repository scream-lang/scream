#pragma once
/*
** $Id: maskconf.h $
** Configuration file for Mask
** See Copyright Notice in mask.h
*/

#include <limits.h>
#include <stddef.h>


/*
** ===================================================================
** General Configuration File for Mask
**
** Some definitions here can be changed externally, through the compiler
** (e.g., with '-D' options): They are commented out or protected
** by '#if !defined' guards. However, several other definitions
** should be changed directly here, either because they affect the
** Mask ABI (by making the changes here, you ensure that all software
** connected to Mask, such as C libraries, will be compiled with the same
** configuration); or because they are seldom changed.
**
** Search for "@@" to find all configurable definitions.
** ===================================================================
*/


/*
** {====================================================================
** System Configuration: macros to adapt (if needed) Mask to some
** particular platform, for instance restricting it to C89.
** =====================================================================
*/

/*
@@ MASK_USE_C89 controls the use of non-ISO-C89 features.
** Define it if you want Mask to avoid the use of a few C99 features
** or Windows-specific features on Windows.
*/
/* #define MASK_USE_C89 */


/*
** By default, Mask on Windows use (some) specific Windows features
*/
#if !defined(MASK_USE_C89) && defined(_WIN32) && !defined(_WIN32_WCE)
#define MASK_USE_WINDOWS  /* enable goodies for regular Windows */
#endif


#if defined(MASK_USE_WINDOWS)
#define MASK_DL_DLL	/* enable support for DLL */
#define MASK_USE_C89	/* broadly, Windows is C89 */
#endif


#if defined(MASK_USE_LINUX)
#define MASK_USE_POSIX
#define MASK_USE_DLOPEN		/* needs an extra library: -ldl */
#endif


#if defined(MASK_USE_MACOSX)
#define MASK_USE_POSIX
#define MASK_USE_DLOPEN		/* MacOS does not need -ldl */
#endif


/*
@@ MASKI_IS32INT is true iff 'int' has (at least) 32 bits.
*/
#define MASKI_IS32INT	((UINT_MAX >> 30) >= 3)

/* }================================================================== */



/*
** {==================================================================
** Configuration for Number types. These options should not be
** set externally, because any other code connected to Mask must
** use the same configuration.
** ===================================================================
*/

/*
@@ MASK_INT_TYPE defines the type for Mask integers.
@@ MASK_FLOAT_TYPE defines the type for Mask floats.
** Mask should work fine with any mix of these options supported
** by your C compiler. The usual configurations are 64-bit integers
** and 'double' (the default), 32-bit integers and 'float' (for
** restricted platforms), and 'long'/'double' (for C compilers not
** compliant with C99, which may not have support for 'long long').
*/

/* predefined options for MASK_INT_TYPE */
#define MASK_INT_INT		1
#define MASK_INT_LONG		2
#define MASK_INT_LONGLONG	3

/* predefined options for MASK_FLOAT_TYPE */
#define MASK_FLOAT_FLOAT		1
#define MASK_FLOAT_DOUBLE	2
#define MASK_FLOAT_LONGDOUBLE	3


/* Default configuration ('long long' and 'double', for 64-bit Mask) */
#define MASK_INT_DEFAULT		MASK_INT_LONGLONG
#define MASK_FLOAT_DEFAULT	MASK_FLOAT_DOUBLE


/*
@@ MASK_32BITS enables Mask with 32-bit integers and 32-bit floats.
*/
#define MASK_32BITS	0


/*
@@ MASK_C89_NUMBERS ensures that Mask uses the largest types available for
** C89 ('long' and 'double'); Windows always has '__int64', so it does
** not need to use this case.
*/
#if defined(MASK_USE_C89) && !defined(MASK_USE_WINDOWS)
#define MASK_C89_NUMBERS		1
#else
#define MASK_C89_NUMBERS		0
#endif


#if MASK_32BITS		/* { */
/*
** 32-bit integers and 'float'
*/
#if MASKI_IS32INT  /* use 'int' if big enough */
#define MASK_INT_TYPE	MASK_INT_INT
#else  /* otherwise use 'long' */
#define MASK_INT_TYPE	MASK_INT_LONG
#endif
#define MASK_FLOAT_TYPE	MASK_FLOAT_FLOAT

#elif MASK_C89_NUMBERS	/* }{ */
/*
** largest types available for C89 ('long' and 'double')
*/
#define MASK_INT_TYPE	MASK_INT_LONG
#define MASK_FLOAT_TYPE	MASK_FLOAT_DOUBLE

#else		/* }{ */
/* use defaults */

#define MASK_INT_TYPE	MASK_INT_DEFAULT
#define MASK_FLOAT_TYPE	MASK_FLOAT_DEFAULT

#endif				/* } */


/* }================================================================== */



/*
** {==================================================================
** Configuration for Paths.
** ===================================================================
*/

/*
** MASK_PATH_SEP is the character that separates templates in a path.
** MASK_PATH_MARK is the string that marks the substitution points in a
** template.
** MASK_EXEC_DIR in a Windows path is replaced by the executable's
** directory.
*/
#define MASK_PATH_SEP            ";"
#define MASK_PATH_MARK           "?"
#define MASK_EXEC_DIR            "!"


/*
@@ MASK_PATH_DEFAULT is the default path that Mask uses to look for
** Mask libraries.
@@ MASK_CPATH_DEFAULT is the default path that Mask uses to look for
** C libraries.
** CHANGE them if your machine has a non-conventional directory
** hierarchy or if you want to install your libraries in
** non-conventional directories.
*/

#define MASK_VDIR	MASK_VERSION_MAJOR "." MASK_VERSION_MINOR
#if defined(_WIN32)	/* { */
/*
** In Windows, any exclamation mark ('!') in the path is replaced by the
** path of the directory of the executable file of the current process.
*/
#define MASK_LDIR	"!\\mask\\"
#define MASK_CDIR	"!\\"
#define MASK_SHRDIR	"!\\..\\share\\mask\\" MASK_VDIR "\\"

#if !defined(MASK_PATH_DEFAULT)
#define MASK_PATH_DEFAULT  \
        MASK_LDIR"?.mask;"  MASK_LDIR"?\\init.mask;" \
        MASK_CDIR"?.mask;"  MASK_CDIR"?\\init.mask;" \
        MASK_SHRDIR"?.mask;" MASK_SHRDIR"?\\init.mask;" \
        ".\\?.mask;" ".\\?\\init.mask;" \
        ".\\?.mask"
#endif

#if !defined(MASK_CPATH_DEFAULT)
#define MASK_CPATH_DEFAULT \
        MASK_CDIR"?.dll;" \
        MASK_CDIR"..\\lib\\mask\\" MASK_VDIR "\\?.dll;" \
        MASK_CDIR"loadall.dll;" ".\\?.dll"
#endif

#else			/* }{ */

#define MASK_ROOT	"/usr/local/"
#define MASK_LDIR	MASK_ROOT "share/mask/" MASK_VDIR "/"
#define MASK_CDIR	MASK_ROOT "lib/mask/" MASK_VDIR "/"

#if !defined(MASK_PATH_DEFAULT)
#define MASK_PATH_DEFAULT  \
        MASK_LDIR"?.mask;"  MASK_LDIR"?/init.mask;" \
        MASK_CDIR"?.mask;"  MASK_CDIR"?/init.mask;" \
        "./?.mask;" "./?/init.mask;" \
        "./?.mask"
#endif

#if !defined(MASK_CPATH_DEFAULT)
#define MASK_CPATH_DEFAULT \
        MASK_CDIR"?.so;" MASK_CDIR"loadall.so;" "./?.so"
#endif

#endif			/* } */


/*
@@ MASK_DIRSEP is the directory separator (for submodules).
** CHANGE it if your machine does not use "/" as the directory separator
** and is not Windows. (On Windows Mask automatically uses "\".)
*/
#if !defined(MASK_DIRSEP)

#if defined(_WIN32)
#define MASK_DIRSEP	"\\"
#else
#define MASK_DIRSEP	"/"
#endif

#endif

/* }================================================================== */


/*
** {==================================================================
** Marks for exported symbols in the C code
** ===================================================================
*/

/*
@@ MASK_API is a mark for all core API functions.
@@ MASKLIB_API is a mark for all auxiliary library functions.
@@ MASKMOD_API is a mark for all standard library opening functions.
** CHANGE them if you need to define those functions in some special way.
** For instance, if you want to create one Windows DLL with the core and
** the libraries, you may want to use the following definition (define
** MASK_BUILD_AS_DLL to get it).
*/
#if defined(MASK_BUILD_AS_DLL)	/* { */

#if defined(MASK_CORE) || defined(MASK_LIB)	/* { */
#define MASK_API __declspec(dllexport)
#else						/* }{ */
#define MASK_API __declspec(dllimport)
#endif						/* } */

#else				/* }{ */

#define MASK_API		extern

#endif				/* } */


/*
** More often than not the libs go together with the core.
*/
#define MASKLIB_API	MASK_API
#define MASKMOD_API	MASK_API


/*
@@ MASKI_FUNC is a mark for all extern functions that are not to be
** exported to outside modules.
@@ MASKI_DDEF and MASKI_DDEC are marks for all extern (const) variables,
** none of which to be exported to outside modules (MASKI_DDEF for
** definitions and MASKI_DDEC for declarations).
** CHANGE them if you need to mark them in some special way. Elf/gcc
** (versions 3.2 and later) mark them as "hidden" to optimize access
** when Mask is compiled as a shared library. Not all elf targets support
** this attribute. Unfortunately, gcc does not offer a way to check
** whether the target offers that support, and those without support
** give a warning about it. To avoid these warnings, change to the
** default definition.
*/
#if defined(__GNUC__) && ((__GNUC__*100 + __GNUC_MINOR__) >= 302) && \
    defined(__ELF__)		/* { */
#define MASKI_FUNC	__attribute__((visibility("internal"))) extern
#else				/* }{ */
#define MASKI_FUNC	extern
#endif				/* } */

#define MASKI_DDEC(dec)	MASKI_FUNC dec
#define MASKI_DDEF	/* empty */

/* }================================================================== */


/*
** {==================================================================
** Compatibility with previous versions
** ===================================================================
*/

/*
@@ MASK_COMPAT_5_3 controls other macros for compatibility with Mask 5.3.
** You can define it to get all options, or change specific options
** to fit your specific needs.
*/
#if defined(MASK_COMPAT_5_3)	/* { */

/*
@@ MASK_COMPAT_MATHLIB controls the presence of several deprecated
** functions in the mathematical library.
** (These functions were already officially removed in 5.3;
** nevertheless they are still available here.)
*/
#define MASK_COMPAT_MATHLIB

/*
@@ MASK_COMPAT_APIINTCASTS controls the presence of macros for
** manipulating other integer types (mask_pushunsigned, mask_tounsigned,
** maskL_checkint, maskL_checklong, etc.)
** (These macros were also officially removed in 5.3, but they are still
** available here.)
*/
#define MASK_COMPAT_APIINTCASTS


/*
@@ MASK_COMPAT_LT_LE controls the emulation of the '__le' metamethod
** using '__lt'.
*/
#define MASK_COMPAT_LT_LE


/*
@@ The following macros supply trivial compatibility for some
** changes in the API. The macros themselves document how to
** change your code to avoid using them.
** (Once more, these macros were officially removed in 5.3, but they are
** still available here.)
*/
#define mask_strlen(L,i)		mask_rawlen(L, (i))

#define mask_objlen(L,i)		mask_rawlen(L, (i))

#define mask_equal(L,idx1,idx2)		mask_compare(L,(idx1),(idx2),MASK_OPEQ)
#define mask_lessthan(L,idx1,idx2)	mask_compare(L,(idx1),(idx2),MASK_OPLT)

#endif				/* } */

/* }================================================================== */



/*
** {==================================================================
** Configuration for Numbers (low-level part).
** Change these definitions if no predefined MASK_FLOAT_* / MASK_INT_*
** satisfy your needs.
** ===================================================================
*/

/*
@@ MASKI_UACNUMBER is the result of a 'default argument promotion'
@@ over a floating number.
@@ l_floatatt(x) corrects float attribute 'x' to the proper float type
** by prefixing it with one of FLT/DBL/LDBL.
@@ MASK_NUMBER_FRMLEN is the length modifier for writing floats.
@@ MASK_NUMBER_FMT is the format for writing floats.
@@ mask_number2str converts a float to a string.
@@ l_mathop allows the addition of an 'l' or 'f' to all math operations.
@@ l_floor takes the floor of a float.
@@ mask_str2number converts a decimal numeral to a number.
*/


/* The following definitions are good for most cases here */

#define l_floor(x)		(l_mathop(floor)(x))

#define mask_number2str(s,sz,n)  \
    l_sprintf((s), sz, MASK_NUMBER_FMT, (MASKI_UACNUMBER)(n))

/*
@@ mask_numbertointeger converts a float number with an integral value
** to an integer, or returns 0 if float is not within the range of
** a mask_Integer.  (The range comparisons are tricky because of
** rounding. The tests here assume a two-complement representation,
** where MININTEGER always has an exact representation as a float;
** MAXINTEGER may not have one, and therefore its conversion to float
** may have an ill-defined value.)
*/
#define mask_numbertointeger(n,p) \
  ((n) >= (MASK_NUMBER)(MASK_MININTEGER) && \
   (n) < -(MASK_NUMBER)(MASK_MININTEGER) && \
      (*(p) = (MASK_INTEGER)(n), 1))


/* now the variable definitions */

#if MASK_FLOAT_TYPE == MASK_FLOAT_FLOAT		/* { single float */

#define MASK_NUMBER	float

#define l_floatatt(n)		(FLT_##n)

#define MASKI_UACNUMBER	double

#define MASK_NUMBER_FRMLEN	""
#define MASK_NUMBER_FMT		"%.7g"

#define l_mathop(op)		op##f

#define mask_str2number(s,p)	strtof((s), (p))


#elif MASK_FLOAT_TYPE == MASK_FLOAT_LONGDOUBLE	/* }{ long double */

#define MASK_NUMBER	long double

#define l_floatatt(n)		(LDBL_##n)

#define MASKI_UACNUMBER	long double

#define MASK_NUMBER_FRMLEN	"L"
#define MASK_NUMBER_FMT		"%.19Lg"

#define l_mathop(op)		op##l

#define mask_str2number(s,p)	strtold((s), (p))

#elif MASK_FLOAT_TYPE == MASK_FLOAT_DOUBLE	/* }{ double */

#define MASK_NUMBER	double

#define l_floatatt(n)		(DBL_##n)

#define MASKI_UACNUMBER	double

#define MASK_NUMBER_FRMLEN	""
#define MASK_NUMBER_FMT		"%.14g"

#define l_mathop(op)		op

#define mask_str2number(s,p)	strtod((s), (p))

#else						/* }{ */

#error "numeric float type not defined"

#endif					/* } */



/*
@@ MASK_UNSIGNED is the unsigned version of MASK_INTEGER.
@@ MASKI_UACINT is the result of a 'default argument promotion'
@@ over a MASK_INTEGER.
@@ MASK_INTEGER_FRMLEN is the length modifier for reading/writing integers.
@@ MASK_INTEGER_FMT is the format for writing integers.
@@ MASK_MAXINTEGER is the maximum value for a MASK_INTEGER.
@@ MASK_MININTEGER is the minimum value for a MASK_INTEGER.
@@ MASK_MAXUNSIGNED is the maximum value for a MASK_UNSIGNED.
@@ mask_integer2str converts an integer to a string.
*/


/* The following definitions are good for most cases here */

#define MASK_INTEGER_FMT		"%" MASK_INTEGER_FRMLEN "d"

#define MASKI_UACINT		MASK_INTEGER

#define mask_integer2str(s,sz,n)  \
    l_sprintf((s), sz, MASK_INTEGER_FMT, (MASKI_UACINT)(n))

/*
** use MASKI_UACINT here to avoid problems with promotions (which
** can turn a comparison between unsigneds into a signed comparison)
*/
#define MASK_UNSIGNED		unsigned MASKI_UACINT


/* now the variable definitions */

#if MASK_INT_TYPE == MASK_INT_INT		/* { int */

#define MASK_INTEGER		int
#define MASK_INTEGER_FRMLEN	""

#define MASK_MAXINTEGER		INT_MAX
#define MASK_MININTEGER		INT_MIN

#define MASK_MAXUNSIGNED		UINT_MAX

#elif MASK_INT_TYPE == MASK_INT_LONG	/* }{ long */

#define MASK_INTEGER		long
#define MASK_INTEGER_FRMLEN	"l"

#define MASK_MAXINTEGER		LONG_MAX
#define MASK_MININTEGER		LONG_MIN

#define MASK_MAXUNSIGNED		ULONG_MAX

#elif MASK_INT_TYPE == MASK_INT_LONGLONG	/* }{ long long */

/* use presence of macro LLONG_MAX as proxy for C99 compliance */
#if defined(LLONG_MAX)		/* { */
/* use ISO C99 stuff */

#define MASK_INTEGER		long long
#define MASK_INTEGER_FRMLEN	"ll"

#define MASK_MAXINTEGER		LLONG_MAX
#define MASK_MININTEGER		LLONG_MIN

#define MASK_MAXUNSIGNED		ULLONG_MAX

#elif defined(MASK_USE_WINDOWS) /* }{ */
/* in Windows, can use specific Windows types */

#define MASK_INTEGER		__int64
#define MASK_INTEGER_FRMLEN	"I64"

#define MASK_MAXINTEGER		_I64_MAX
#define MASK_MININTEGER		_I64_MIN

#define MASK_MAXUNSIGNED		_UI64_MAX

#else				/* }{ */

#error "Compiler does not support 'long long'. Use option '-DMASK_32BITS' \
  or '-DMASK_C89_NUMBERS' (see file 'maskconf.h' for details)"

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
** (All uses in Mask have only one format item.)
*/
#if !defined(MASK_USE_C89)
#define l_sprintf(s,sz,f,i)	snprintf(s,sz,f,i)
#else
#define l_sprintf(s,sz,f,i)	((void)(sz), sprintf(s,f,i))
#endif


/*
@@ mask_strx2number converts a hexadecimal numeral to a number.
** In C99, 'strtod' does that conversion. Otherwise, you can
** leave 'mask_strx2number' undefined and Mask will provide its own
** implementation.
*/
#if !defined(MASK_USE_C89)
#define mask_strx2number(s,p)		mask_str2number(s,p)
#endif


/*
@@ mask_pointer2str converts a pointer to a readable string in a
** non-specified way.
*/
#define mask_pointer2str(buff,sz,p)	l_sprintf(buff,sz,"%p",p)


/*
@@ mask_number2strx converts a float to a hexadecimal numeral.
** In C99, 'sprintf' (with format specifiers '%a'/'%A') does that.
** Otherwise, you can leave 'mask_number2strx' undefined and Mask will
** provide its own implementation.
*/
#if !defined(MASK_USE_C89)
#define mask_number2strx(L,b,sz,f,n)  \
    ((void)L, l_sprintf(b,sz,f,(MASKI_UACNUMBER)(n)))
#endif


/*
** 'strtof' and 'opf' variants for math functions are not valid in
** C89. Otherwise, the macro 'HUGE_VALF' is a good proxy for testing the
** availability of these variants. ('math.h' is already included in
** all files that use these macros.)
*/
#if defined(MASK_USE_C89) || (defined(HUGE_VAL) && !defined(HUGE_VALF))
#undef l_mathop  /* variants not available */
#undef mask_str2number
#define l_mathop(op)		(mask_Number)op  /* no variant */
#define mask_str2number(s,p)	((mask_Number)strtod((s), (p)))
#endif


/*
@@ MASK_KCONTEXT is the type of the context ('ctx') for continuation
** functions.  It must be a numerical type; Mask will use 'intptr_t' if
** available, otherwise it will use 'ptrdiff_t' (the nearest thing to
** 'intptr_t' in C89)
*/
#define MASK_KCONTEXT	ptrdiff_t

#if !defined(MASK_USE_C89) && defined(__STDC_VERSION__) && \
    __STDC_VERSION__ >= 199901L
#include <stdint.h>
#if defined(INTPTR_MAX)  /* even in C99 this type is optional */
#undef MASK_KCONTEXT
#define MASK_KCONTEXT	intptr_t
#endif
#endif


/*
@@ mask_getlocaledecpoint gets the locale "radix character" (decimal point).
** Change that if you do not want to use C locales. (Code using this
** macro must include the header 'locale.h'.)
*/
#if !defined(mask_getlocaledecpoint)
#define mask_getlocaledecpoint()		(localeconv()->decimal_point[0])
#endif


/*
** macros to improve jump prediction, used mostly for error handling
** and debug facilities. (Some macros in the Mask API use these macros.
** Define MASK_NOBUILTIN if you do not want '__builtin_expect' in your
** code.)
*/
#if !defined(maski_likely)

#if defined(__GNUC__) && !defined(MASK_NOBUILTIN)
#define maski_likely(x)		(__builtin_expect(((x) != 0), 1))
#define maski_unlikely(x)	(__builtin_expect(((x) != 0), 0))
#else
#define maski_likely(x)		(x)
#define maski_unlikely(x)	(x)
#endif

#endif


#if defined(MASK_CORE) || defined(MASK_LIB)
/* shorter names for Mask's own use */
#define l_likely(x)	maski_likely(x)
#define l_unlikely(x)	maski_unlikely(x)
#endif



/* }================================================================== */


/*
** {==================================================================
** Language Variations
** =====================================================================
*/

/*
@@ MASK_NOCVTN2S/MASK_NOCVTS2N control how Mask performs some
** coercions. Define MASK_NOCVTN2S to turn off automatic coercion from
** numbers to strings. Define MASK_NOCVTS2N to turn off automatic
** coercion from strings to numbers.
*/
/* #define MASK_NOCVTN2S */
/* #define MASK_NOCVTS2N */


/*
@@ MASK_USE_APICHECK turns on several consistency checks on the C API.
** Define it as a help when debugging C code.
*/
#if defined(MASK_USE_APICHECK)
#include <assert.h>
#define maski_apicheck(l,e)	assert(e)
#endif

/* }================================================================== */


/*
** {==================================================================
** Macros that affect the API and must be stable (that is, must be the
** same when you compile Mask and when you compile code that links to
** Mask).
** =====================================================================
*/

/*
@@ MASKI_MAXSTACK limits the size of the Mask stack.
** CHANGE it if you need a different limit. This limit is arbitrary;
** its only purpose is to stop Mask from consuming unlimited stack
** space (and to reserve some numbers for pseudo-indices).
** (It must fit into max(size_t)/32.)
*/
#if MASKI_IS32INT
#define MASKI_MAXSTACK		1000000
#else
#define MASKI_MAXSTACK		15000
#endif


/*
@@ MASK_EXTRASPACE defines the size of a raw memory area associated with
** a Mask state with very fast access.
** CHANGE it if you need a different size.
*/
#define MASK_EXTRASPACE		(sizeof(void *))


/*
@@ MASK_IDSIZE gives the maximum size for the description of the source
** of a function in debug information.
** CHANGE it if you want a different size.
*/
#define MASK_IDSIZE	60


/*
@@ MASKL_BUFFERSIZE is the initial buffer size used by the lauxlib
** buffer system.
*/
#define MASKL_BUFFERSIZE   ((int)(16 * sizeof(void*) * sizeof(mask_Number)))


/*
@@ MASKI_MAXALIGN defines fields that, when used in a union, ensure
** maximum alignment for the other items in that union.
*/
#define MASKI_MAXALIGN  mask_Number n; double u; void *s; mask_Integer i; long l

/* }================================================================== */

/*
** {====================================================================
** mask configuration
** =====================================================================}
*/

// If defined, mask won't emit parser warnings.
//#define MASK_NO_PARSER_WARNINGS

// If defined, mask errors will use ANSI color codes.
//#define MASK_USE_COLORED_OUTPUT

// If defined, mask will exclude code snippets from error messages to make them shorter.
//#define MASK_SHORT_ERRORS

// If defined, mask won't assume that source files are UTF-8 encoded and restrict valid symbol names.
//#define MASK_NO_UTF8

// If defined, mask will use a jumptable in the VM even if not compiled via GCC.
// This will generally improve runtime performance but can add minutes to compile time, depending on the setup.
//#define MASK_FORCE_JUMPTABLE

// If defined, mask will use C++ exceptions to implement Mask longjumps.
// This is generally slower and complicates exception handling.
//#define MASK_USE_THROW

/*
** {====================================================================
** mask configuration: Compatible Mode
** =====================================================================}
*/

// If defined, mask will assign 'mask_' to new keywords which break previously valid Mask identifiers.
// If you decide to leave this undefined:
//     - Both 'mask_switch' and 'switch' are valid syntaxes for mask.
//     - Keywords like 'continue' remain keywords.
// If you decide to define this:
//     - Only 'mask_switch' will be valid. 'switch' will not exist.
//     - Keywords like 'continue' will now be 'mask_continue'.
//#define MASK_COMPATIBLE_MODE

#ifdef MASK_COMPATIBLE_MODE

// If defined, only 'mask_switch' will be valid. 'switch' will not exist.
#define MASK_COMPATIBLE_SWITCH

// If defined, only 'mask_... you get the idea.
#define MASK_COMPATIBLE_CONTINUE

#define MASK_COMPATIBLE_WHEN

#define MASK_COMPATIBLE_ENUM

#endif // MASK_COMPATIBLE_MODE

/*
** {====================================================================
** mask configuration: Infinite Loop Prevention (ILP)
**
** This is only useful in game regions, where a long loop may block the main thread and crash the game.
** These places usually implement a yield (or wait) function, which can be detected and hooked to reset iterations.
** =====================================================================}
*/

// If defined, mask will attempt to prevent infinite loops.
//#define MASK_ILP_ENABLE

#ifdef MASK_ILP_ENABLE
/*
** This is the maximum amount of backward jumps permitted in a singular loop block.
** If exceeded, the backward jump is ignored to escape the loop.
*/
#ifndef MASK_ILP_MAX_ITERATIONS
#define MASK_ILP_MAX_ITERATIONS			1000000
#endif

// If you want (i.e) `maskB_next` to reset iteration counters, define as `maskB_next`.
// #define MASK_ILP_HOOK_FUNCTION		maskB_next

// If defined, mask won't throw an error and instead just break out of the loop.
//#define MASK_ILP_SILENT_BREAK

#endif // MASK_ILP_ENABLE

/*
** {====================================================================
** mask configuration: Execution Time Limit (ETL)
**
** This is only useful in sandbox environments where stalling is absolutely unacceptable.
** =====================================================================}
*/

//#define MASK_ETL_ENABLE

#ifdef MASK_ETL_ENABLE
/*
** This is the maximum amount of nanoseconds the VM is allowed to run.
*/
#ifndef MASK_ETL_NANOS
#define MASK_ETL_NANOS			1'000'000 /* 1ms */
#endif

/*
** This can be used to execute custom code when the time limit is exceeded and
** the VM is about to be terminated.
*/
#ifndef MASK_ETL_TIMESUP
#define MASK_ETL_TIMESUP
#endif
#endif

/*
** {====================================================================
** mask configuration: VM Dump
** =====================================================================}
*/

// If defined, mask will print every VM instruction that is ran.
// Note that you can modify mask_writestring to redirect output.
//#define MASK_VMDUMP

#ifdef MASK_VMDUMP
/* Example:
**  #define vmDumpIgnore \
**      OP_LOADI \
**      OP_LOADF
*/

// Opcodes listed in this structure are a blacklist. They not be printed when VM dumping.
#define vmDumpIgnore


// Opcodes listed in this structure are a whitelist. They are only printed when VM dumping.
#define vmDumpAllow

// If defined, mask will use vmDumpAllow instead of vmDumpIgnore.
//#define MASK_VMDUMP_WHITELIST

// Defines under what circumstances the VM Dump is active.
#ifndef MASK_VMDUMP_COND
#define MASK_VMDUMP_COND(L) true
#endif

#endif // MASK_VMDUMP

/*
** {====================================================================
** mask configuration: Content Moderation
** =====================================================================}
*/

// If defined, mask will not load compiled Mask or mask code.
//#define MASK_DISABLE_COMPILED

// If defined, the provided function will be called as bool(const char* filename).
// It needs to have C ABI linkage (extern "C").
// If it returns false, a Mask error is raised.
// This will affect require and dofile.
//#define MASK_LOADFILE_HOOK ContmodOnLoadfile

/*
** {====================================================================
** mask color macros.
** =====================================================================}
*/

#ifdef MASK_USE_COLORED_OUTPUT // Don't need to write any 'ifdef' macro logic inside of Mask::ErrorMessage.
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
#else // MASK_USE_COLORED_OUTPUT
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
#endif // MASK_USE_COLORED_OUTPUT


/* }================================================================== */
