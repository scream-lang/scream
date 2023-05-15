#pragma once
/*
** $Id: lzio.h $
** Buffered streams
** See Copyright Notice in mask.h
*/

#include "mask.h"

#include "lmem.h"


#define EOZ	(-1)			/* end of stream */

typedef struct Zio ZIO;

#define zgetc(z)  (((z)->n--)>0 ?  cast_uchar(*(z)->p++) : maskZ_fill(z))


typedef struct Mbuffer {
  char *buffer;
  size_t n;
  size_t buffsize;
} Mbuffer;

#define maskZ_initbuffer(L, buff) ((buff)->buffer = NULL, (buff)->buffsize = 0)

#define maskZ_buffer(buff)	((buff)->buffer)
#define maskZ_sizebuffer(buff)	((buff)->buffsize)
#define maskZ_bufflen(buff)	((buff)->n)

#define maskZ_buffremove(buff,i)	((buff)->n -= (i))
#define maskZ_resetbuffer(buff) ((buff)->n = 0)


#define maskZ_resizebuffer(L, buff, size) \
    ((buff)->buffer = maskM_reallocvchar(L, (buff)->buffer, \
                (buff)->buffsize, size), \
    (buff)->buffsize = size)

#define maskZ_freebuffer(L, buff)	maskZ_resizebuffer(L, buff, 0)


MASKI_FUNC void maskZ_init (mask_State *L, ZIO *z, mask_Reader reader,
                                        void *data);
MASKI_FUNC size_t maskZ_read (ZIO* z, void *b, size_t n);	/* read next n bytes */



/* --------- Private Part ------------------ */

struct Zio {
  size_t n;			/* bytes still unread */
  const char *p;		/* current position in buffer */
  mask_Reader reader;		/* reader function */
  void *data;			/* additional data */
  mask_State *L;			/* Mask state (for reader) */
};


MASKI_FUNC int maskZ_fill (ZIO *z);
