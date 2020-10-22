/*-
 * Copyright 2015 Colin Walters <walters@verbum.org>
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions 
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* Compile with $(pkg-config --cflags --libs glib-2.0) */

#include "bsdiff.h"
#undef BSDIFF_EXECUTABLE
#include "bsdiff.c"
#include "bspatch.c"

#include <glib.h>
#include <err.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

static int
bsdiff_write_gstring (struct bsdiff_stream *stream,
		      const void *buf,
		      int size)
{
  g_string_append_len ((GString*)stream->opaque, buf, size);
  return 0;
}

typedef struct {
  GString *buf;
  size_t off;
} GStringInputStream;

static int
gstringin_read (const struct bspatch_stream* stream, void* buffer, int length)
{
  GStringInputStream *gin = stream->opaque;
  size_t rlen = MIN(gin->buf->len - gin->off, length);

  if (rlen == 0)
    return 0;

  memcpy (buffer, gin->buf->str + gin->off, rlen);
  gin->off += rlen;

  return 0;
}

int main(int argc,char *argv[])
{
  const char *original = NULL;
  size_t original_len;
  const char *modified = NULL;
  size_t modified_len;
  GString *gmembuf = g_string_new (NULL);
  GString *gpatchbuf = g_string_new (NULL);

  {
    ssize_t bytes_read;
    char inbuf[1024];
    while ((bytes_read = read (0, inbuf, sizeof (inbuf))) != 0)
    g_string_append_len (gmembuf, inbuf, bytes_read);
    if (bytes_read < 0)
      err (1, "read");
    if (gmembuf->len < 2)
      errx (1, "Input length is less than 2");
  }

  original = gmembuf->str;
  original_len = gmembuf->len / 2;
  modified = original + original_len;
  modified_len = gmembuf->len - original_len;
  g_string_free (gmembuf, FALSE);
  gmembuf = NULL;

  {
    struct bsdiff_stream stream;
    
    stream.malloc = malloc;
    stream.free = free;
    stream.write = bsdiff_write_gstring;
    stream.opaque = gpatchbuf;
    if (bsdiff (original, original_len, modified, modified_len, &stream))
      err(1, "bsdiff");
  }

  {
    GStringInputStream gin = { gpatchbuf, 0 };
    struct bspatch_stream pstream;
    GString *newbuf = g_string_new (NULL);

    pstream.opaque = &gin;
    pstream.read = gstringin_read;

    g_string_set_size (newbuf, modified_len);
    
    if (bspatch (original, original_len, newbuf->str, modified_len, &pstream))
      errx (1, "bspatch failed");

    if (memcmp (modified, newbuf->str, modified_len) != 0)
      errx (1, "bspatch did not reproduce modified content!");

    g_string_free (newbuf, TRUE);
  }

  /* Free the memory we used */
  g_string_free (gpatchbuf, TRUE);

  return 0;
}
