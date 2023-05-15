#pragma once
/*
** $Id: lundump.h $
** load precompiled Mask chunks
** See Copyright Notice in mask.h
*/

#include "llimits.h"
#include "lobject.h"
#include "lzio.h"


/* data to catch conversion errors */
#define MASKC_DATA	"\x19\x93\r\n\x1a\n"

#define MASKC_INT	0x5678
#define MASKC_NUM	cast_num(370.5)

/*
** Encode major-minor version in one byte, one nibble for each
*/
#define MYINT(s)	(s[0]-'0')  /* assume one-digit numerals */
#define MASKC_VERSION	(MYINT(MASK_VERSION_MAJOR)*16+MYINT(MASK_VERSION_MINOR))

#define MASKC_FORMAT	0	/* this is the official format */

/* load one chunk; from lundump.c */
MASKI_FUNC LClosure* maskU_undump (mask_State* L, ZIO* Z, const char* name);

/* dump one chunk; from ldump.c */
MASKI_FUNC int maskU_dump (mask_State* L, const Proto* f, mask_Writer w,
                         void* data, int strip);
