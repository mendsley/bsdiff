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
__FBSDID("$FreeBSD: src/usr.bin/bsdiff/bsdiff/bsdiff.c,v 1.1 2005/08/06 01:59:05 cperciva Exp $");
#endif

#include <sys/types.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct bsdiff_header
{
	uint8_t signature[8];
	uint64_t ctrl_block_length;
	uint64_t diff_block_length;
	uint64_t new_file_length;
};

typedef int (*bsdiff_write)(void* file, const void* buffer, int size);

struct bsdiff_compressor
{
	void* opaque;

	int (*write)(struct bsdiff_compressor* compresor, const void* buffer, int size);
	int (*finish)(struct bsdiff_compressor* compressor);
};

#if defined(__linux__)
#include <err.h>
#else
static int err(int eval, const char* fmt, ...)
{
	const char* errortext;
	char* strp;
	va_list args;

	errortext = strerror(errno);
	if (fmt != NULL || strcmp(fmt,"") != 0) {
		strp = (char*)malloc(1024 * sizeof(char));
		va_start(args, fmt);
		vsnprintf(strp, 1023, fmt, args);
		va_end(args);
		fprintf(stderr, "%s: %s\b", strp, errortext);
		free(strp);
	} else {
		fprintf(stderr, "%s\n", errortext);
	}

	exit(eval);
	return 0;
}

static int errx(int eval, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	exit(eval);
	return 0;
}
#endif

#define MIN(x,y) (((x)<(y)) ? (x) : (y))

static void split(off_t *I,off_t *V,off_t start,off_t len,off_t h)
{
	off_t i,j,k,x,tmp,jj,kk;

	if(len<16) {
		for(k=start;k<start+len;k+=j) {
			j=1;x=V[I[k]+h];
			for(i=1;k+i<start+len;i++) {
				if(V[I[k+i]+h]<x) {
					x=V[I[k+i]+h];
					j=0;
				};
				if(V[I[k+i]+h]==x) {
					tmp=I[k+j];I[k+j]=I[k+i];I[k+i]=tmp;
					j++;
				};
			};
			for(i=0;i<j;i++) V[I[k+i]]=k+j-1;
			if(j==1) I[k]=-1;
		};
		return;
	};

	x=V[I[start+len/2]+h];
	jj=0;kk=0;
	for(i=start;i<start+len;i++) {
		if(V[I[i]+h]<x) jj++;
		if(V[I[i]+h]==x) kk++;
	};
	jj+=start;kk+=jj;

	i=start;j=0;k=0;
	while(i<jj) {
		if(V[I[i]+h]<x) {
			i++;
		} else if(V[I[i]+h]==x) {
			tmp=I[i];I[i]=I[jj+j];I[jj+j]=tmp;
			j++;
		} else {
			tmp=I[i];I[i]=I[kk+k];I[kk+k]=tmp;
			k++;
		};
	};

	while(jj+j<kk) {
		if(V[I[jj+j]+h]==x) {
			j++;
		} else {
			tmp=I[jj+j];I[jj+j]=I[kk+k];I[kk+k]=tmp;
			k++;
		};
	};

	if(jj>start) split(I,V,start,jj-start,h);

	for(i=0;i<kk-jj;i++) V[I[jj+i]]=kk-1;
	if(jj==kk-1) I[jj]=-1;

	if(start+len>kk) split(I,V,kk,start+len-kk,h);
}

static void qsufsort(off_t *I,off_t *V,uint8_t *old,off_t oldsize)
{
	off_t buckets[256];
	off_t i,h,len;

	for(i=0;i<256;i++) buckets[i]=0;
	for(i=0;i<oldsize;i++) buckets[old[i]]++;
	for(i=1;i<256;i++) buckets[i]+=buckets[i-1];
	for(i=255;i>0;i--) buckets[i]=buckets[i-1];
	buckets[0]=0;

	for(i=0;i<oldsize;i++) I[++buckets[old[i]]]=i;
	I[0]=oldsize;
	for(i=0;i<oldsize;i++) V[i]=buckets[old[i]];
	V[oldsize]=0;
	for(i=1;i<256;i++) if(buckets[i]==buckets[i-1]+1) I[buckets[i]]=-1;
	I[0]=-1;

	for(h=1;I[0]!=-(oldsize+1);h+=h) {
		len=0;
		for(i=0;i<oldsize+1;) {
			if(I[i]<0) {
				len-=I[i];
				i-=I[i];
			} else {
				if(len) I[i-len]=-len;
				len=V[I[i]]+1-i;
				split(I,V,i,len,h);
				i+=len;
				len=0;
			};
		};
		if(len) I[i-len]=-len;
	};

	for(i=0;i<oldsize+1;i++) I[V[i]]=i;
}

static off_t matchlen(uint8_t *old,off_t oldsize,uint8_t *new,off_t newsize)
{
	off_t i;

	for(i=0;(i<oldsize)&&(i<newsize);i++)
		if(old[i]!=new[i]) break;

	return i;
}

static off_t search(off_t *I,uint8_t *old,off_t oldsize,
		uint8_t *new,off_t newsize,off_t st,off_t en,off_t *pos)
{
	off_t x,y;

	if(en-st<2) {
		x=matchlen(old+I[st],oldsize-I[st],new,newsize);
		y=matchlen(old+I[en],oldsize-I[en],new,newsize);

		if(x>y) {
			*pos=I[st];
			return x;
		} else {
			*pos=I[en];
			return y;
		}
	};

	x=st+(en-st)/2;
	if(memcmp(old+I[x],new,MIN(oldsize-I[x],newsize))<0) {
		return search(I,old,oldsize,new,newsize,x,en,pos);
	} else {
		return search(I,old,oldsize,new,newsize,st,x,pos);
	};
}

static void offtout(off_t x,uint8_t *buf)
{
	off_t y;

	if(x<0) y=-x; else y=x;

		buf[0]=y%256;y-=buf[0];
	y=y/256;buf[1]=y%256;y-=buf[1];
	y=y/256;buf[2]=y%256;y-=buf[2];
	y=y/256;buf[3]=y%256;y-=buf[3];
	y=y/256;buf[4]=y%256;y-=buf[4];
	y=y/256;buf[5]=y%256;y-=buf[5];
	y=y/256;buf[6]=y%256;y-=buf[6];
	y=y/256;buf[7]=y%256;

	if(x<0) buf[7]|=0x80;
}

int bsdiff(uint8_t* old, off_t oldsize, uint8_t* new, off_t newsize, struct bsdiff_compressor* compressor, struct bsdiff_header* header)
{
	off_t *I,*V;
	off_t scan,pos,len;
	off_t compresslen, filelen;
	off_t lastscan,lastpos,lastoffset;
	off_t oldscore,scsc;
	off_t s,Sf,lenf,Sb,lenb;
	off_t overlap,Ss,lens;
	off_t i;
	off_t dblen,eblen;
	uint8_t *db,*eb;
	uint8_t buf[8 * 3];

	if(((I=malloc((oldsize+1)*sizeof(off_t)))==NULL) ||
		((V=malloc((oldsize+1)*sizeof(off_t)))==NULL)) err(1,NULL);

	qsufsort(I,V,old,oldsize);

	free(V);
	if(((db=malloc(newsize+1))==NULL) ||
		((eb=malloc(newsize+1))==NULL)) err(1,NULL);
	dblen=0;
	eblen=0;
	filelen=0;

	/* Header is
		0	8	 "BSDIFF40"
		8	8	length of bzip2ed ctrl block
		16	8	length of bzip2ed diff block
		24	8	length of new file */
	/* File is
		0	32	Header
		32	??	Bzip2ed ctrl block
		??	??	Bzip2ed diff block
		??	??	Bzip2ed extra block */
	memcpy(header->signature,"BSDIFF40",sizeof(header->signature));
	header->ctrl_block_length = 0;
	header->diff_block_length = 0;
	header->new_file_length = newsize;

	/* Compute the differences, writing ctrl as we go */
	scan=0;len=0;
	lastscan=0;lastpos=0;lastoffset=0;
	while(scan<newsize) {
		oldscore=0;

		for(scsc=scan+=len;scan<newsize;scan++) {
			len=search(I,old,oldsize,new+scan,newsize-scan,
					0,oldsize,&pos);

			for(;scsc<scan+len;scsc++)
			if((scsc+lastoffset<oldsize) &&
				(old[scsc+lastoffset] == new[scsc]))
				oldscore++;

			if(((len==oldscore) && (len!=0)) || 
				(len>oldscore+8)) break;

			if((scan+lastoffset<oldsize) &&
				(old[scan+lastoffset] == new[scan]))
				oldscore--;
		};

		if((len!=oldscore) || (scan==newsize)) {
			s=0;Sf=0;lenf=0;
			for(i=0;(lastscan+i<scan)&&(lastpos+i<oldsize);) {
				if(old[lastpos+i]==new[lastscan+i]) s++;
				i++;
				if(s*2-i>Sf*2-lenf) { Sf=s; lenf=i; };
			};

			lenb=0;
			if(scan<newsize) {
				s=0;Sb=0;
				for(i=1;(scan>=lastscan+i)&&(pos>=i);i++) {
					if(old[pos-i]==new[scan-i]) s++;
					if(s*2-i>Sb*2-lenb) { Sb=s; lenb=i; };
				};
			};

			if(lastscan+lenf>scan-lenb) {
				overlap=(lastscan+lenf)-(scan-lenb);
				s=0;Ss=0;lens=0;
				for(i=0;i<overlap;i++) {
					if(new[lastscan+lenf-overlap+i]==
					   old[lastpos+lenf-overlap+i]) s++;
					if(new[scan-lenb+i]==
					   old[pos-lenb+i]) s--;
					if(s>Ss) { Ss=s; lens=i+1; };
				};

				lenf+=lens-overlap;
				lenb-=lens;
			};

			for(i=0;i<lenf;i++)
				db[dblen+i]=new[lastscan+i]-old[lastpos+i];
			for(i=0;i<(scan-lenb)-(lastscan+lenf);i++)
				eb[eblen+i]=new[lastscan+lenf+i];

			dblen+=lenf;
			eblen+=(scan-lenb)-(lastscan+lenf);

			offtout(lenf,buf);
			offtout((scan-lenb)-(lastscan+lenf),buf+8);
			offtout((pos-lenb)-(lastpos+lenf),buf+16);

			compresslen = compressor->write(compressor, buf, sizeof(buf));
			if (compresslen == -1)
				errx(1, "compressor->write");
			filelen += compresslen;

			lastscan=scan-lenb;
			lastpos=pos-lenb;
			lastoffset=pos-scan;
		};
	};
	compresslen = compressor->finish(compressor);
	if (compresslen == -1)
		errx(1, "compressor->finish");
	filelen += compresslen;

	/* Compute size of compressed ctrl data */
	header->ctrl_block_length = filelen;

	/* Write compressed diff data */
	compresslen = compressor->write(compressor, db, dblen);
	if (compresslen == -1)
		errx(1, "compressor->write");
	filelen += compresslen;
	compresslen = compressor->finish(compressor);
	if (compresslen == -1)
		errx(1, "compressor->finish");
	filelen += compresslen;

	/* Compute size of compressed diff data */
	header->diff_block_length = filelen - header->ctrl_block_length;

	/* Write compressed extra data */
	compresslen = compressor->write(compressor, eb, eblen);
	if (compresslen == -1)
		errx(1, "compressor->write");
	compresslen = compressor->finish(compressor);
	if (compresslen == -1)
		errx(1, "compressor->finish");

	/* Free the memory we used */
	free(db);
	free(eb);
	free(I);

	return 0;
}

#if !defined(BSDIFF_LIBRARY)

#include <bzlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

static int bz2_write(struct bsdiff_compressor* compressor, const void* buffer, int size)
{
	bz_stream* bz2;
	FILE* pf;
	int err;
	int totalwritten;
	char compress_buffer[4096];

	bz2 = (bz_stream*)compressor->opaque;
	pf = (FILE*)bz2->opaque;

	if (!bz2->state)
	{
		if (BZ_OK != BZ2_bzCompressInit(bz2, 9, 0, 0))
			return -1;
	}

	bz2->next_in = (char*)buffer;
	bz2->avail_in = size;

	totalwritten = 0;
	for (;;)
	{
		bz2->next_out = compress_buffer;
		bz2->avail_out = sizeof(compress_buffer);
		if (BZ_RUN_OK != (err = BZ2_bzCompress(bz2, BZ_RUN)))
			return -1;

		if (bz2->avail_out < sizeof(compress_buffer))
		{
			const int written = sizeof(compress_buffer) - bz2->avail_out;
			if (written != fwrite(compress_buffer, 1, written, pf))
				return -1;

			totalwritten += written;
		}

		if (bz2->avail_in == 0)
			return totalwritten;
	}
}

static int bz2_finish(struct bsdiff_compressor* compressor)
{
	int err;
	int totalwritten;
	bz_stream* bz2;
	FILE* pf;
	char compress_buffer[4096];

	bz2 = (bz_stream*)compressor->opaque;
	pf = (FILE*)bz2->opaque;

	totalwritten = 0;
	for (;;)
	{
		bz2->next_out = compress_buffer;
		bz2->avail_out = sizeof(compress_buffer);

		err = BZ2_bzCompress(bz2, BZ_FINISH);
		if (BZ_FINISH_OK != err && BZ_STREAM_END != err)
			return -1;

		if (bz2->avail_out < sizeof(compress_buffer))
		{
			const int written = sizeof(compress_buffer) - bz2->avail_out;
			if (written != fwrite(compress_buffer, 1, written, pf))
				return -1;

			totalwritten += written;
		}

		if (BZ_STREAM_END == err)
			break;
	}

	BZ2_bzCompressEnd(bz2);
	return totalwritten;
}

int main(int argc,char *argv[])
{
	int fd;
	uint8_t *old,*new;
	off_t oldsize,newsize;
	struct bsdiff_header header;
	FILE * pf;
	struct bsdiff_compressor compressor;
	bz_stream bz2 = {0};
	compressor.opaque = &bz2;
	compressor.write = bz2_write;
	compressor.finish = bz2_finish;

	if(argc!=4) errx(1,"usage: %s oldfile newfile patchfile\n",argv[0]);

	/* Allocate oldsize+1 bytes instead of oldsize bytes to ensure
		that we never try to malloc(0) and get a NULL pointer */
	if(((fd=open(argv[1],O_RDONLY,0))<0) ||
		((oldsize=lseek(fd,0,SEEK_END))==-1) ||
		((old=malloc(oldsize+1))==NULL) ||
		(lseek(fd,0,SEEK_SET)!=0) ||
		(read(fd,old,oldsize)!=oldsize) ||
		(close(fd)==-1)) err(1,"%s",argv[1]);


	/* Allocate newsize+1 bytes instead of newsize bytes to ensure
		that we never try to malloc(0) and get a NULL pointer */
	if(((fd=open(argv[2],O_RDONLY,0))<0) ||
		((newsize=lseek(fd,0,SEEK_END))==-1) ||
		((new=malloc(newsize+1))==NULL) ||
		(lseek(fd,0,SEEK_SET)!=0) ||
		(read(fd,new,newsize)!=newsize) ||
		(close(fd)==-1)) err(1,"%s",argv[2]);

	/* Create the patch file */
	if ((pf = fopen(argv[3], "w")) == NULL)
		err(1, "%s", argv[3]);

	/* Save space for header */
	if (fwrite(&header, sizeof(header), 1, pf) != 1)
		err(1, "Failed to write header");

	bz2.opaque = pf;
	if (bsdiff(old, oldsize, new, newsize, &compressor, &header))
		err(1, "bsdiff");

	if (fseek(pf, 0, SEEK_SET) ||
		fwrite(&header, sizeof(header), 1, pf) != 1)
		err(1, "Failed to write header");

	if (fclose(pf))
		err(1, "fclose");

	/* Free the memory we used */
	free(old);
	free(new);

	return 0;
}

#endif
