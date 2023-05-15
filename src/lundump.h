#pragma once
/*
** $Id: lundump.h $
** load precompiled Hello chunks
** See Copyright Notice in hello.h
*/

#include "llimits.h"
#include "lobject.h"
#include "lzio.h"


/* data to catch conversion errors */
#define HELLOC_DATA	"\x19\x93\r\n\x1a\n"

#define HELLOC_INT	0x5678
#define HELLOC_NUM	cast_num(370.5)

/*
** Encode major-minor version in one byte, one nibble for each
*/
#define MYINT(s)	(s[0]-'0')  /* assume one-digit numerals */
#define HELLOC_VERSION	(MYINT(HELLO_VERSION_MAJOR)*16+MYINT(HELLO_VERSION_MINOR))

#define HELLOC_FORMAT	0	/* this is the official format */

/* load one chunk; from lundump.c */
HELLOI_FUNC LClosure* helloU_undump (hello_State* L, ZIO* Z, const char* name);

/* dump one chunk; from ldump.c */
HELLOI_FUNC int helloU_dump (hello_State* L, const Proto* f, hello_Writer w,
                         void* data, int strip);
