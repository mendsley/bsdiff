/*-
 * Copyright 2003-2005 Colin Percival
 * Copyright 2012-2018 Matthew Endsley
 * Copyright 2018-2020 Emanuel Komínek
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

#ifndef BSDIFF_H
# define BSDIFF_H

# include <stddef.h>
# include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BSDIFF_WRITECONTROL 0
#define BSDIFF_WRITEDIFF    1
#define BSDIFF_WRITEEXTRA   2

struct bsdiff_stream
{
	void* opaque;

	void* (*malloc)(size_t size);
	void (*free)(void* ptr);
	int (*write)(struct bsdiff_stream* stream, const void* buffer, int size, int type);
};

int bsdiff(const uint8_t* source, int64_t sourcesize, const uint8_t* target, int64_t targetsize, struct bsdiff_stream* stream);

#ifdef __cplusplus
}
#endif

#endif
