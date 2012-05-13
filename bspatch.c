/*-
 * Copyright 2003-2005 Colin Percival
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

#include <bzlib.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <unistd.h>
#include <fcntl.h>

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

static const int BUFFER_SIZE = 4096;

static int readcompress(bz_stream* bz2, FILE* pf, uint8_t* dest, int length)
{
	int n;
	int ret;

	bz2->next_out = (char*)dest;
	bz2->avail_out = length;

	for (;;)
	{
		if (bz2->avail_in == 0 && !feof(pf) && bz2->avail_out > 0)
		{
			n = fread(bz2->opaque, 1, BUFFER_SIZE, pf);
			if (ferror(pf))
				return -1;

			bz2->next_in = bz2->opaque;
			bz2->avail_in = n;
		}

		ret = BZ2_bzDecompress(bz2);
		if (ret != BZ_OK && ret != BZ_STREAM_END)
			return -1;

		if (ret == BZ_OK && feof(pf) && bz2->avail_in == 0 && bz2->avail_out > 0)
			return -1;

		if (ret == BZ_STREAM_END)
			return length - bz2->avail_out;
		if (bz2->avail_out == 0)
			return length;
	}

	// unreachable
	return -1;
}

int bspatch(const uint8_t* old, const int oldsize, uint8_t* new, const int newsize, FILE* cpf, FILE* dpf, FILE* epf)
{
	bz_stream cbz2 = {0};
	bz_stream dbz2 = {0};
	bz_stream ebz2 = {0};
	char cbuf[BUFFER_SIZE], dbuf[BUFFER_SIZE], ebuf[BUFFER_SIZE];
	int cbz2err, dbz2err, ebz2err;
	uint8_t buf[8];
	int64_t oldpos,newpos;
	int64_t ctrl[3];
	int64_t lenread;
	int64_t i;

	cbz2.opaque = cbuf;
	dbz2.opaque = dbuf;
	ebz2.opaque = ebuf;

	if (BZ_OK != (cbz2err = BZ2_bzDecompressInit(&cbz2, 0, 0)))
		errx(1, "BZ2_bzDecompressInit, bz2err = %d", cbz2err);
	if (BZ_OK != (dbz2err = BZ2_bzDecompressInit(&dbz2, 0, 0)))
		errx(1, "BZ2_bzDecompressInit, bz2err = %d", dbz2err);
	if (BZ_OK != (ebz2err = BZ2_bzDecompressInit(&ebz2, 0, 0)))
		errx(1, "BZ2_bzDecompressInit, bz2err = %d", ebz2err);

	oldpos=0;newpos=0;
	while(newpos<newsize) {
		/* Read control data */
		for(i=0;i<=2;i++) {
			lenread = readcompress(&cbz2, cpf, buf, 8);
			if (lenread != 8)
				errx(1, "Corrupt patch\n");
			ctrl[i]=offtin(buf);
		};

		/* Sanity-check */
		if(newpos+ctrl[0]>newsize)
			errx(1,"Corrupt patch\n");

		/* Read diff string */
		lenread = readcompress(&dbz2, dpf, new + newpos, ctrl[0]);
		if (lenread != ctrl[0])
			errx(1, "Corrupt patch\n");

		/* Add old data to diff string */
		for(i=0;i<ctrl[0];i++)
			if((oldpos+i>=0) && (oldpos+i<oldsize))
				new[newpos+i]+=old[oldpos+i];

		/* Adjust pointers */
		newpos+=ctrl[0];
		oldpos+=ctrl[0];

		/* Sanity-check */
		if(newpos+ctrl[1]>newsize)
			errx(1,"Corrupt patch\n");

		/* Read extra string */
		lenread = readcompress(&ebz2, epf, new + newpos, ctrl[1]);
		if (lenread != ctrl[1])
			errx(1, "Corrupt patch\n");

		/* Adjust pointers */
		newpos+=ctrl[1];
		oldpos+=ctrl[2];
	};

	/* Clean up the bzip2 reads */
	BZ2_bzDecompressEnd(&cbz2);
	BZ2_bzDecompressEnd(&dbz2);
	BZ2_bzDecompressEnd(&ebz2);

	return 0;
}

int main(int argc,char * argv[])
{
	FILE * f, * cpf, * dpf, * epf;
	int fd;
	ssize_t oldsize,newsize;
	ssize_t bzctrllen,bzdatalen;
	uint8_t header[32];
	uint8_t *old, *new;

	if(argc!=4) errx(1,"usage: %s oldfile newfile patchfile\n",argv[0]);

	/* Open patch file */
	if ((f = fopen(argv[3], "r")) == NULL)
		err(1, "fopen(%s)", argv[3]);

	/*
	File format:
		0	8	"BSDIFF40"
		8	8	X
		16	8	Y
		24	8	sizeof(newfile)
		32	X	bzip2(control block)
		32+X	Y	bzip2(diff block)
		32+X+Y	???	bzip2(extra block)
	with control block a set of triples (x,y,z) meaning "add x bytes
	from oldfile to x bytes from the diff block; copy y bytes from the
	extra block; seek forwards in oldfile by z bytes".
	*/

	/* Read header */
	if (fread(header, 1, 32, f) < 32) {
		if (feof(f))
			errx(1, "Corrupt patch\n");
		err(1, "fread(%s)", argv[3]);
	}

	/* Check for appropriate magic */
	if (memcmp(header, "BSDIFF40", 8) != 0)
		errx(1, "Corrupt patch\n");

	/* Read lengths from header */
	bzctrllen=offtin(header+8);
	bzdatalen=offtin(header+16);
	newsize=offtin(header+24);
	if((bzctrllen<0) || (bzdatalen<0) || (newsize<0))
		errx(1,"Corrupt patch\n");

	/* Close patch file and re-open it via libbzip2 at the right places */
	if (fclose(f))
		err(1, "fclose(%s)", argv[3]);
	if ((cpf = fopen(argv[3], "r")) == NULL)
		err(1, "fopen(%s)", argv[3]);
	if (fseeko(cpf, 32, SEEK_SET))
		err(1, "fseeko(%s, %lld)", argv[3],
		    (long long)32);
	if ((dpf = fopen(argv[3], "r")) == NULL)
		err(1, "fopen(%s)", argv[3]);
	if (fseeko(dpf, 32 + bzctrllen, SEEK_SET))
		err(1, "fseeko(%s, %lld)", argv[3],
		    (long long)(32 + bzctrllen));
	if ((epf = fopen(argv[3], "r")) == NULL)
		err(1, "fopen(%s)", argv[3]);
	if (fseeko(epf, 32 + bzctrllen + bzdatalen, SEEK_SET))
		err(1, "fseeko(%s, %lld)", argv[3],
		    (long long)(32 + bzctrllen + bzdatalen));

	if(((fd=open(argv[1],O_RDONLY,0))<0) ||
		((oldsize=lseek(fd,0,SEEK_END))==-1) ||
		((old=malloc(oldsize+1))==NULL) ||
		(lseek(fd,0,SEEK_SET)!=0) ||
		(read(fd,old,oldsize)!=oldsize) ||
		(close(fd)==-1)) err(1,"%s",argv[1]);
	if((new=malloc(newsize+1))==NULL) err(1,NULL);

	if (bspatch(old, oldsize, new, newsize, cpf, dpf, epf))
		err(1, "bspatch");

	/* Write the new file */
	if(((fd=open(argv[2],O_CREAT|O_TRUNC|O_WRONLY,0666))<0) ||
		(write(fd,new,newsize)!=newsize) || (close(fd)==-1))
		err(1,"%s",argv[2]);

	free(new);
	free(old);

	return 0;
}
