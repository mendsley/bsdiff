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

#include "bsdiff.h"

#include <limits.h>
#include <string.h>
#include <stdio.h>

#define MIN(x, y) (((x)<(y)) ? (x) : (y))

static void split(int64_t *I, int64_t *V, int64_t start, int64_t len, int64_t h) {
    int64_t i, j, k, x, tmp, jj, kk;

    if (len < 16) {
        for (k = start; k < start + len; k += j) {
            j = 1;
            x = V[I[k] + h];
            for (i = 1; k + i < start + len; i++) {
                if (V[I[k + i] + h] < x) {
                    x = V[I[k + i] + h];
                    j = 0;
                };
                if (V[I[k + i] + h] == x) {
                    tmp = I[k + j];
                    I[k + j] = I[k + i];
                    I[k + i] = tmp;
                    j++;
                };
            };
            for (i = 0; i < j; i++) V[I[k + i]] = k + j - 1;
            if (j == 1) I[k] = -1;
        };
        return;
    };

    x = V[I[start + len / 2] + h];
    jj = 0;
    kk = 0;
    for (i = start; i < start + len; i++) {
        if (V[I[i] + h] < x) jj++;
        if (V[I[i] + h] == x) kk++;
    };
    jj += start;
    kk += jj;

    i = start;
    j = 0;
    k = 0;
    while (i < jj) {
        if (V[I[i] + h] < x) {
            i++;
        } else if (V[I[i] + h] == x) {
            tmp = I[i];
            I[i] = I[jj + j];
            I[jj + j] = tmp;
            j++;
        } else {
            tmp = I[i];
            I[i] = I[kk + k];
            I[kk + k] = tmp;
            k++;
        };
    };

    while (jj + j < kk) {
        if (V[I[jj + j] + h] == x) {
            j++;
        } else {
            tmp = I[jj + j];
            I[jj + j] = I[kk + k];
            I[kk + k] = tmp;
            k++;
        };
    };

    if (jj > start) split(I, V, start, jj - start, h);

    for (i = 0; i < kk - jj; i++) V[I[jj + i]] = kk - 1;
    if (jj == kk - 1) I[jj] = -1;

    if (start + len > kk) split(I, V, kk, start + len - kk, h);
}

static void qsufsort(int64_t *I, int64_t *V, const uint8_t *old, int64_t oldsize) {
    int64_t buckets[256];
    int64_t i, h, len;

    for (i = 0; i < 256; i++) buckets[i] = 0;
    for (i = 0; i < oldsize; i++) buckets[old[i]]++;
    for (i = 1; i < 256; i++) buckets[i] += buckets[i - 1];
    for (i = 255; i > 0; i--) buckets[i] = buckets[i - 1];
    buckets[0] = 0;

    for (i = 0; i < oldsize; i++) I[++buckets[old[i]]] = i;
    for (i = 0; i < oldsize; i++) V[i] = buckets[old[i]];
    V[oldsize] = 0;
    for (i = 1; i < 256; i++) if (buckets[i] == buckets[i - 1] + 1) I[buckets[i]] = -1;
    I[0] = -1;

    for (h = 1; I[0] != -(oldsize + 1); h += h) {
        len = 0;
        for (i = 0; i < oldsize + 1;) {
            if (I[i] < 0) {
                len -= I[i];
                i -= I[i];
            } else {
                if (len) I[i - len] = -len;
                len = V[I[i]] + 1 - i;
                split(I, V, i, len, h);
                i += len;
                len = 0;
            };
        };
        if (len) I[i - len] = -len;
    };

    for (i = 0; i < oldsize + 1; i++) I[V[i]] = i;
}

/*
 * Given two pointers, old and new, limited by old_size and new_size accordingly,
 * return me the amount of bytes that matched from the beginning of the pointers
 * (not to exceed either old_size or new_size).
 */
static int64_t matchlen(const uint8_t *old, int64_t old_size, const uint8_t *new, int64_t new_size) {
    int64_t i;

    for (i = 0; (i < old_size) && (i < new_size); i++)
        if (old[i] != new[i]) break;

    return i;
}

/**
 * @param I index (suffix array of old)
 * @param old original binary (never changes!)
 * @param old_size original binary size
 * @param new search from this location in new binary
 * @param new_size how many bytes available in new
 * @param st starting index(?)
 * @param en ending index(?)
 * @param pos where to write position in old where the longer
 * match was found.
 * @return matched length.
 */
static int64_t search(const int64_t *I, const uint8_t *old, int64_t old_size,
        const uint8_t *new, int64_t new_size, int64_t st, int64_t en, int64_t *pos) {
    int64_t x, y;

    // If distance between 2 indexes less than 2
    // $TODO is there a guarantee that st != en?
    if (en - st < 2) {
        // try to match suffix #st (x) and suffix #en (y) at 'new'
        x = matchlen(old + I[st], old_size - I[st], new, new_size);
        y = matchlen(old + I[en], old_size - I[en], new, new_size);

        if (x > y) {
            *pos = I[st];
            return x;
        } else {
            *pos = I[en];
            return y;
        }
    };

    // x is the middle between start and end
    x = st + (en - st) / 2;
    // If bytewise, the string at index (x) is "left" of string at "new"...
    if (memcmp(old + I[x], new, MIN(old_size - I[x], new_size)) < 0) {
        // let's look for a match between "x" and "en"
        // that's because it's not gonna ever be left of x, since I
        // is ordered
        return search(I, old, old_size, new, new_size, x, en, pos);
    } else {
        // otherwise, let's look between st and x (can't be right of x)
        return search(I, old, old_size, new, new_size, st, x, pos);
    };
}

static void offtout(int64_t x, uint8_t *buf) {
    int64_t y;

    if (x < 0) y = -x; else y = x;

    buf[0] = y % 256;
    y -= buf[0];
    y = y / 256;
    buf[1] = y % 256;
    y -= buf[1];
    y = y / 256;
    buf[2] = y % 256;
    y -= buf[2];
    y = y / 256;
    buf[3] = y % 256;
    y -= buf[3];
    y = y / 256;
    buf[4] = y % 256;
    y -= buf[4];
    y = y / 256;
    buf[5] = y % 256;
    y -= buf[5];
    y = y / 256;
    buf[6] = y % 256;
    y -= buf[6];
    y = y / 256;
    buf[7] = y % 256;

    if (x < 0) buf[7] |= 0x80;
}

static int64_t writedata(struct bsdiff_stream *stream, const void *buffer, int64_t length) {
    int64_t result = 0;

    while (length > 0) {
        const int smallsize = (int) MIN(length, INT_MAX);
        const int writeresult = stream->write(stream, buffer, smallsize);
        if (writeresult == -1) {
            return -1;
        }

        result += writeresult;
        length -= smallsize;
        buffer = (uint8_t *) buffer + smallsize;
    }

    return result;
}

struct bsdiff_request {
    const uint8_t *old;
    int64_t oldsize;
    const uint8_t *new;
    int64_t newsize;
    struct bsdiff_stream *stream;
    int64_t *I;
    uint8_t *buffer;
};

static int bsdiff_internal(const struct bsdiff_request req) {
    int64_t *I, *V;
    int64_t scan, pos, len;
    int64_t lastscan, lastpos, lastoffset;
    int64_t oldscore, scsc;
    int64_t s, lenf, lenb, fill_rate;
    int64_t overlap, Ss, lens;
    int64_t i;
    uint8_t *buffer;
    uint8_t buf[8 * 3];

    if ((V = req.stream->malloc((req.oldsize + 1) * sizeof(int64_t))) == NULL) return -1;
    I = req.I;

    qsufsort(I, V, req.old, req.oldsize);
    req.stream->free(V);

    buffer = req.buffer;

    /* Compute the differences, writing ctrl as we go */

    scan = 0; // position we are scanning New from.
    len = 0;
    pos = 0;
    lastscan = 0;
    lastpos = 0;
    lastoffset = 0;


    while (scan < req.newsize) {
        oldscore = 0;

        printf("About to start scan@%'lld, last_scan=%'lld, last_pos=%'lld, last_offset=%'lld\n",
                scan, lastscan, lastpos, lastoffset);

        for (scsc = scan += len; scan < req.newsize; scan++) {
            len = search(I, req.old, req.oldsize, req.new + scan, req.newsize - scan,
                    0, req.oldsize, &pos);

            // len now has the length of a matched string of 'req.new+scan' anywhere in old.
            // pos points to where that match was found.

            printf("New@%'lld matched old@%'lld for %'lld bytes. Will calc old score from %'lld\n",
                    scan, pos, len, (lastoffset+scsc));

            for (; scsc < scan + len; scsc++)

                // let's drag scsc from the start of the scan, in amount of matched length.

                if ((scsc + lastoffset < req.oldsize) &&
                        (req.old[scsc + lastoffset] == req.new[scsc]))
                    oldscore++;

            printf("Old score %'lld\n", oldscore);

            if (((len == oldscore) && (len != 0)) ||
                    (len > oldscore + 8)) {
                printf("stopping scan because %s\n", (len > oldscore + 8)?"Length is superior":"100% match");
                break;
            }

            if ((scan + lastoffset < req.oldsize) &&
                    (req.old[scan + lastoffset] == req.new[scan])) {
                printf("Old score %'lld, decreasing\n", oldscore);
                oldscore--;
            }
        };

        printf("Scan completed, len=%'lld, score %'lld, %'lld bytes left in new\n", len, oldscore, req.newsize-scan);

        if ((len != oldscore) || (scan == req.newsize)) {

#if 0
            printf("Writing buffer, len %'lld oldscore %'lld, scan %'lld, new size %'lld\n",
                    len, oldscore, scan, req.newsize);
#endif

            // we want to establish 2 values: lenf and lenb
            // lenf is how many bytes are we gonna 'diff'
            // lenb is how many bytes we gonna not serve at all right now
            // (and instead deal with them in the next scan)

            // we are effectively dealing with the block of New[lastscan to scan]
            // and Old[lastpos to pos] The beginning of New block is aligned with the beginning of the Old
            // block, and their ends are also aligned, which creates "compression" or "tear" in the "middle".

            // lenf is calculated as: the index between lastscan and scan (not to exceed oldsize, counting from lastpos),
            // where the difference between the doubled amount of matches from Old[lastpos] and New[lastscan] to (i)
            // is maximized.
            s = 0;
            fill_rate = 0;
            lenf = 0;
            for (i = 0; (lastscan + i < scan) && (lastpos + i < req.oldsize);) {
                // s is the amount of matched characters.
                if (req.old[lastpos + i] == req.new[lastscan + i]) s++;
                i++;
                if (s * 2 - i > fill_rate) {
                    // if the amount of matched characters minus
                    fill_rate = s * 2 - i;
                    lenf = i;
                };
            };

            // lenb is in essence reverse of lenf. We start an index from the end of the current block.
            // We then find the value where the amount of matched bytes from the end of the block, doubled,
            // minus total amount of bytes considered, is maximum.

            lenb = 0;
            if (scan < req.newsize) {
                s = 0;
                fill_rate = 0;
                for (i = 1; (scan >= lastscan + i) && (pos >= i); i++) {
                    if (req.old[pos - i] == req.new[scan - i]) s++;
                    if (s * 2 - i > fill_rate) {
                        fill_rate = s * 2 - i;
                        lenb = i;
                    };
                };
            };

            // now of course it is possible there is an overlap, considering how we
            // arrived to both lenf and lenb
            if (lastscan + lenf > scan - lenb) {
                overlap = (lastscan + lenf) - (scan - lenb);

                s = 0;
                Ss = 0;
                lens = 0;
                for (i = 0; i < overlap; i++) {

                    // we are comparing the overlapped block simultaneously
                    // to the old binary as if overlap was relative with the beginning
                    // of the old block, and as if it was relative to the end of the old block.
                    //
                    // Think of it that way:
                    // new block |-----------------|
                    // new block |-----[ ovl ]-----|
                    // old block |---------------|
                    // can be viewed as:
                    // |-----[ ovl ]---| (relative to the beginning of the block)
                    // |---[ ovl ]-----| (relative to the end of the block)
                    // we are trying to establish "lens",
                    // which tells us how to divide up the overlaid part

                    // if the byte matches with an overlap if taken from the beginning of the old
                    // block, increase s.
                    if (req.new[lastscan + lenf - overlap + i] ==
                            req.old[lastpos + lenf - overlap + i])
                        s++;

                    // but if the byte matches with the overlap if taken from the end of the old block,
                    // decrease s.
                    if (req.new[scan - lenb + i] ==
                            req.old[pos - lenb + i])
                        s--;


                    if (s > Ss) {
                        // remember the index inside the overlap where s value was maximum.
                        Ss = s;
                        lens = i + 1;
                    };
                };


                // lens is a stake inside the overlap region, we divide along the stake,
                // everything on the left goes to lenf, and on the right to lenb.

                lenf += lens - overlap;
                lenb -= lens;
            };

            // now, all of space between oldscan and scan is divided into potentially 3 regions:
            // 1. oldscan to oldscan + lenf : all of that region is to be considered a "diff"
            //    region right now.
            // 2. oldscan + lenf to scan - lenb : area that is considered "new", and is quoted verbatim
            // 3. scan - lenb to scan : area that we are postponing for now.

            // read diff stream of that length
            offtout(lenf, buf);
            // write out that many bytes of new file
            offtout((scan - lenb) - (lastscan + lenf), buf + 8);
            // replacing that many bytes of the old file
            offtout((pos - lenb) - (lastpos + lenf), buf + 16);

            /* Write control data */
            if (writedata(req.stream, buf, sizeof(buf)))
                return -1;

            int64_t zrs = 0;

            /* Write diff data */
            for (i = 0; i < lenf; i++)
                if (!(buffer[i] = req.new[lastscan + i] - req.old[lastpos + i])) zrs++;
            if (writedata(req.stream, buffer, lenf))
                return -1;

            printf("Buffer output has %'lld diff, %'lld as-is, %'lld backup, old index %'lld, zero count %'lld\n",
                    lenf, (scan - lenb) - (lastscan + lenf), lenb, (pos - lenb) - (lastpos + lenf), zrs);

            /* Write extra data */
            for (i = 0; i < (scan - lenb) - (lastscan + lenf); i++)
                buffer[i] = req.new[lastscan + lenf + i];
            if (writedata(req.stream, buffer, (scan - lenb) - (lastscan + lenf)))
                return -1;

            lastscan = scan - lenb;
            lastpos = pos - lenb;
            lastoffset = pos - scan;
        }
    };

    return 0;
}

int bsdiff(const uint8_t *old, int64_t oldsize, const uint8_t *new, int64_t newsize, struct bsdiff_stream *stream) {
    int result;
    struct bsdiff_request req;

    if ((req.I = stream->malloc((oldsize + 1) * sizeof(int64_t))) == NULL)
        return -1;

    if ((req.buffer = stream->malloc(newsize + 1)) == NULL) {
        stream->free(req.I);
        return -1;
    }

    req.old = old;
    req.oldsize = oldsize;
    req.new = new;
    req.newsize = newsize;
    req.stream = stream;

    result = bsdiff_internal(req);

    stream->free(req.buffer);
    stream->free(req.I);

    return result;
}

#if defined(BSDIFF_EXECUTABLE)

#include <sys/types.h>

#include <bzlib.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int bz2_write(struct bsdiff_stream* stream, const void* buffer, int size)
{
    int bz2err;
    BZFILE* bz2;

    bz2 = (BZFILE*)stream->opaque;
    BZ2_bzWrite(&bz2err, bz2, (void*)buffer, size);
    if (bz2err != BZ_STREAM_END && bz2err != BZ_OK)
        return -1;

    return 0;
}

int main(int argc,char *argv[])
{
    int fd;
    int bz2err;
    uint8_t *old,*new;
    off_t oldsize,newsize;
    uint8_t buf[8];
    FILE * pf;
    struct bsdiff_stream stream;
    BZFILE* bz2;

    memset(&bz2, 0, sizeof(bz2));
    stream.malloc = malloc;
    stream.free = free;
    stream.write = bz2_write;

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

    /* Write header (signature+newsize)*/
    offtout(newsize, buf);
    if (fwrite("ENDSLEY/BSDIFF43", 16, 1, pf) != 1 ||
        fwrite(buf, sizeof(buf), 1, pf) != 1)
        err(1, "Failed to write header");


    if (NULL == (bz2 = BZ2_bzWriteOpen(&bz2err, pf, 9, 0, 0)))
        errx(1, "BZ2_bzWriteOpen, bz2err=%d", bz2err);

    stream.opaque = bz2;
    if (bsdiff(old, oldsize, new, newsize, &stream))
        err(1, "bsdiff");

    BZ2_bzWriteClose(&bz2err, bz2, 0, NULL, NULL);
    if (bz2err != BZ_OK)
        err(1, "BZ2_bzWriteClose, bz2err=%d", bz2err);

    if (fclose(pf))
        err(1, "fclose");

    /* Free the memory we used */
    free(old);
    free(new);

    return 0;
}

#endif
