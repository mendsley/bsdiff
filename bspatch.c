/*-
 * Copyright 2003-2005 Colin Percival
 * Copyright 2012 Matthew Endsley
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

#if 0
__FBSDID("$FreeBSD: src/usr.bin/bsdiff/bspatch/bspatch.c,v 1.1 2005/08/06 01:59:06 cperciva Exp $");
#endif

#include <stdint.h>

struct bspatch_stream
{
	void* opaque;
	int (*read)(const struct bspatch_stream* stream, void* buffer, int length);
};

struct bspatch_request
{
	const uint8_t* old;
	int64_t oldsize;
	uint8_t* new;
	int64_t newsize;
	struct bspatch_stream stream;
};

int bspatch(const struct bspatch_request req);

#if !defined(BSPATCH_HEADER_ONLY)

static int64_t offtin(uint8_t *buf)
{
	int64_t y;

	y=buf[7]&0x7F;
	y=y*256;y+=buf[6];
	y=y*256;y+=buf[5];
	y=y*256;y+=buf[4];
	y=y*256;y+=buf[3];
	y=y*256;y+=buf[2];
	y=y*256;y+=buf[1];
	y=y*256;y+=buf[0];

	if(buf[7]&0x80) y=-y;

	return y;
}

int bspatch(const struct bspatch_request req)
{
	uint8_t buf[8];
	int64_t oldpos,newpos;
	int64_t ctrl[3];
	int64_t lenread;
	int64_t i;

	oldpos=0;newpos=0;
	while(newpos<req.newsize) {
		/* Read control data */
		for(i=0;i<=2;i++) {
			lenread = req.stream.read(&req.stream, buf, 8);
			if (lenread != 8)
				return -1;
			ctrl[i]=offtin(buf);
		};

		/* Sanity-check */
		if(newpos+ctrl[0]>req.newsize)
			return -1;

		/* Read diff string */
		lenread = req.stream.read(&req.stream, req.new + newpos, ctrl[0]);
		if (lenread != ctrl[0])
			return -1;

		/* Add old data to diff string */
		for(i=0;i<ctrl[0];i++)
			if((oldpos+i>=0) && (oldpos+i<req.oldsize))
				req.new[newpos+i]+=req.old[oldpos+i];

		/* Adjust pointers */
		newpos+=ctrl[0];
		oldpos+=ctrl[0];

		/* Sanity-check */
		if(newpos+ctrl[1]>req.newsize)
			return -1;

		/* Read extra string */
		lenread = req.stream.read(&req.stream, req.new + newpos, ctrl[1]);
		if (lenread != ctrl[1])
			return -1;

		/* Adjust pointers */
		newpos+=ctrl[1];
		oldpos+=ctrl[2];
	};

	return 0;
}

#if defined(BSPATCH_EXECUTABLE)

#include <bzlib.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <unistd.h>
#include <fcntl.h>

#define BUFFER_SIZE 4096
struct bspatch_bz2_buffer
{
	FILE* pf;
	bz_stream bz2;
	char buffer[BUFFER_SIZE];
};

static int readcompress(const struct bspatch_stream* stream, void* buffer, int length)
{
	int n;
	int ret;
	struct bspatch_bz2_buffer* buf = (struct bspatch_bz2_buffer*)stream->opaque;
	bz_stream* bz2 = &buf->bz2;

	bz2->next_out = (char*)buffer;
	bz2->avail_out = length;

	for (;;)
	{
		if (bz2->avail_in == 0 && !feof(buf->pf) && bz2->avail_out > 0)
		{
			n = fread(buf->buffer, 1, BUFFER_SIZE, buf->pf);
			if (ferror(buf->pf))
				return -1;

			bz2->next_in = buf->buffer;
			bz2->avail_in = n;
		}

		ret = BZ2_bzDecompress(bz2);
		if (ret != BZ_OK && ret != BZ_STREAM_END)
			return -1;

		if (ret == BZ_OK && feof(buf->pf) && bz2->avail_in == 0 && bz2->avail_out > 0)
			return -1;

		if (ret == BZ_STREAM_END)
			return length - bz2->avail_out;
		if (bz2->avail_out == 0)
			return length;
	}

	// unreachable
	return -1;
}

int main(int argc,char * argv[])
{
	FILE * f;
	int fd;
	int bz2err;
	uint8_t header[16];
	uint8_t *old;
	struct bspatch_request req;
	struct bspatch_bz2_buffer bz2;

	memset(&bz2, 0, sizeof(bz2));

	if(argc!=4) errx(1,"usage: %s oldfile newfile patchfile\n",argv[0]);

	/* Open patch file */
	if ((f = fopen(argv[3], "r")) == NULL)
		err(1, "fopen(%s)", argv[3]);

	/* Read header */
	if (fread(header, 1, 16, f) != 16) {
		if (feof(f))
			errx(1, "Corrupt patch\n");
		err(1, "fread(%s)", argv[3]);
	}

	/* Check for appropriate magic */
	if (memcmp(header, "BSDIFF40", 8) != 0)
		errx(1, "Corrupt patch\n");

	/* Read lengths from header */
	req.newsize=offtin(header+8);
	if(req.newsize<0)
		errx(1,"Corrupt patch\n");

	/* Close patch file and re-open it via libbzip2 at the right places */
	if(((fd=open(argv[1],O_RDONLY,0))<0) ||
		((req.oldsize=lseek(fd,0,SEEK_END))==-1) ||
		((old=malloc(req.oldsize+1))==NULL) ||
		(lseek(fd,0,SEEK_SET)!=0) ||
		(read(fd,old,req.oldsize)!=req.oldsize) ||
		(close(fd)==-1)) err(1,"%s",argv[1]);
	req.old = old;
	if((req.new=malloc(req.newsize+1))==NULL) err(1,NULL);

	bz2.pf = f;
	req.stream.opaque = &bz2;
	req.stream.read = readcompress;

	if (BZ_OK != (bz2err = BZ2_bzDecompressInit(&bz2.bz2, 0, 0)))
		errx(1, "BZ2_bzDecompressInit, bz2err = %d", bz2err);

	if (bspatch(req))
		err(1, "bspatch");

	/* Clean up the bzip2 reads */
	BZ2_bzDecompressEnd(&bz2.bz2);
	fclose(f);

	/* Write the new file */
	if(((fd=open(argv[2],O_CREAT|O_TRUNC|O_WRONLY,0666))<0) ||
		(write(fd,req.new,req.newsize)!=req.newsize) || (close(fd)==-1))
		err(1,"%s",argv[2]);

	free(req.new);
	free(old);

	return 0;
}

#endif
#endif
