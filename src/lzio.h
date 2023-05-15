#pragma once
/*
** $Id: lzio.h $
** Buffered streams
** See Copyright Notice in hello.h
*/

#include "hello.h"

#include "lmem.h"


#define EOZ	(-1)			/* end of stream */

typedef struct Zio ZIO;

#define zgetc(z)  (((z)->n--)>0 ?  cast_uchar(*(z)->p++) : helloZ_fill(z))


typedef struct Mbuffer {
  char *buffer;
  size_t n;
  size_t buffsize;
} Mbuffer;

#define helloZ_initbuffer(L, buff) ((buff)->buffer = NULL, (buff)->buffsize = 0)

#define helloZ_buffer(buff)	((buff)->buffer)
#define helloZ_sizebuffer(buff)	((buff)->buffsize)
#define helloZ_bufflen(buff)	((buff)->n)

#define helloZ_buffremove(buff,i)	((buff)->n -= (i))
#define helloZ_resetbuffer(buff) ((buff)->n = 0)


#define helloZ_resizebuffer(L, buff, size) \
    ((buff)->buffer = helloM_reallocvchar(L, (buff)->buffer, \
                (buff)->buffsize, size), \
    (buff)->buffsize = size)

#define helloZ_freebuffer(L, buff)	helloZ_resizebuffer(L, buff, 0)


HELLOI_FUNC void helloZ_init (hello_State *L, ZIO *z, hello_Reader reader,
                                        void *data);
HELLOI_FUNC size_t helloZ_read (ZIO* z, void *b, size_t n);	/* read next n bytes */



/* --------- Private Part ------------------ */

struct Zio {
  size_t n;			/* bytes still unread */
  const char *p;		/* current position in buffer */
  hello_Reader reader;		/* reader function */
  void *data;			/* additional data */
  hello_State *L;			/* Hello state (for reader) */
};


HELLOI_FUNC int helloZ_fill (ZIO *z);
