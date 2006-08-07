/**
 * Aften: A/52 audio encoder
 * Copyright (c) 2006  Justin Ruggles <jruggle@eathlink.net>
 *                     Prakash Punnoor <prakash@punnoor.de>
 *
 * Based on "The simplest AC3 encoder" from FFmpeg
 * Copyright (c) 2000 Fabrice Bellard.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file bitalloc.c
 * A/52 bit allocation
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bitalloc.h"
#include "a52.h"

static const uint8_t latab[260]= {
    64, 63, 62, 61, 60, 59, 58, 57, 56, 55,
    54, 53, 52, 52, 51, 50, 49, 48, 47, 47,
    46, 45, 44, 44, 43, 42, 41, 41, 40, 39,
    38, 38, 37, 36, 36, 35, 35, 34, 33, 33,
    32, 32, 31, 30, 30, 29, 29, 28, 28, 27,
    27, 26, 26, 25, 25, 24, 24, 23, 23, 22,
    22, 21, 21, 21, 20, 20, 19, 19, 19, 18,
    18, 18, 17, 17, 17, 16, 16, 16, 15, 15,
    15, 14, 14, 14, 13, 13, 13, 13, 12, 12,
    12, 12, 11, 11, 11, 11, 10, 10, 10, 10,
    10,  9,  9,  9,  9,  9,  8,  8,  8,  8,
     8,  8,  7,  7,  7,  7,  7,  7,  6,  6,
     6,  6,  6,  6,  6,  6,  5,  5,  5,  5,
     5,  5,  5,  5,  4,  4,  4,  4,  4,  4,
     4,  4,  4,  4,  4,  3,  3,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  3,  3,  3,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0
};

static const uint16_t hth[50][3]= {
    { 0x04d0,0x04f0,0x0580 }, { 0x04d0,0x04f0,0x0580 },
    { 0x0440,0x0460,0x04b0 }, { 0x0400,0x0410,0x0450 },
    { 0x03e0,0x03e0,0x0420 }, { 0x03c0,0x03d0,0x03f0 },
    { 0x03b0,0x03c0,0x03e0 }, { 0x03b0,0x03b0,0x03d0 },
    { 0x03a0,0x03b0,0x03c0 }, { 0x03a0,0x03a0,0x03b0 },
    { 0x03a0,0x03a0,0x03b0 }, { 0x03a0,0x03a0,0x03b0 },
    { 0x03a0,0x03a0,0x03a0 }, { 0x0390,0x03a0,0x03a0 },
    { 0x0390,0x0390,0x03a0 }, { 0x0390,0x0390,0x03a0 },
    { 0x0380,0x0390,0x03a0 }, { 0x0380,0x0380,0x03a0 },
    { 0x0370,0x0380,0x03a0 }, { 0x0370,0x0380,0x03a0 },
    { 0x0360,0x0370,0x0390 }, { 0x0360,0x0370,0x0390 },
    { 0x0350,0x0360,0x0390 }, { 0x0350,0x0360,0x0390 },
    { 0x0340,0x0350,0x0380 }, { 0x0340,0x0350,0x0380 },
    { 0x0330,0x0340,0x0380 }, { 0x0320,0x0340,0x0370 },
    { 0x0310,0x0320,0x0360 }, { 0x0300,0x0310,0x0350 },
    { 0x02f0,0x0300,0x0340 }, { 0x02f0,0x02f0,0x0330 },
    { 0x02f0,0x02f0,0x0320 }, { 0x02f0,0x02f0,0x0310 },
    { 0x0300,0x02f0,0x0300 }, { 0x0310,0x0300,0x02f0 },
    { 0x0340,0x0320,0x02f0 }, { 0x0390,0x0350,0x02f0 },
    { 0x03e0,0x0390,0x0300 }, { 0x0420,0x03e0,0x0310 },
    { 0x0460,0x0420,0x0330 }, { 0x0490,0x0450,0x0350 },
    { 0x04a0,0x04a0,0x03c0 }, { 0x0460,0x0490,0x0410 },
    { 0x0440,0x0460,0x0470 }, { 0x0440,0x0440,0x04a0 },
    { 0x0520,0x0480,0x0460 }, { 0x0800,0x0630,0x0440 },
    { 0x0840,0x0840,0x0450 }, { 0x0840,0x0840,0x04e0 }
};

static const uint8_t baptab[64]= {
     0,  1,  1,  1,  1,  1,  2,  2,  3,  3,  3,  4,  4,  5,  5,  6,
     6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  8,  9,  9,  9,  9, 10,
    10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 14,
    14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15
};

static const uint8_t sdecaytab[4]={
    0x0f, 0x11, 0x13, 0x15,
};

static const uint8_t fdecaytab[4]={
    0x3f, 0x53, 0x67, 0x7b,
};

static const uint16_t sgaintab[4]= {
    0x540, 0x4d8, 0x478, 0x410,
};

static const uint16_t dbkneetab[4]= {
    0x000, 0x700, 0x900, 0xb00,
};

static const uint16_t floortab[8]= {
    0x2f0, 0x2b0, 0x270, 0x230, 0x1f0, 0x170, 0x0f0, 0xf800,
};

static const uint16_t fgaintab[8]= {
    0x080, 0x100, 0x180, 0x200, 0x280, 0x300, 0x380, 0x400,
};

static const uint8_t bndsz[50]={
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, 1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  3,  3,  3,  3,  3, 3,
     3,  6,  6,  6,  6,  6,  6, 12, 12, 12, 12, 24, 24, 24, 24, 24
};

static uint16_t psdtab[25];
static uint8_t masktab[253];
static uint8_t bndtab[51];
static uint16_t frmsizetab[38][3];
static int expsizetab[3][256];

void
bitalloc_init()
{
    int i, j, k, l, v;

    // compute psdtab
    for(i=0; i<25; i++) {
        psdtab[i] = 3072 - (i << 7);
    }

    // compute bndtab and masktab from bandsz
    k = l = i = 0;
    bndtab[i] = l;
    while(i < 50) {
        v = bndsz[i];
        for(j=0; j<v; j++) masktab[k++] = i;
        l += v;
        bndtab[++i] = l;
    }

    // compute frmsizetab (in bits)
    for(i=0; i<19; i++) {
        for(j=0; j<3; j++) {
            v = a52_bitratetab[i] * 96000 / a52_freqs[j];
            frmsizetab[i*2][j] = frmsizetab[i*2+1][j] = v * 16;
            if(j == 1) frmsizetab[i*2+1][j] += 16;
        }
    }

    // compute expsizetab
    for(i=1; i<4; i++) {
        for(j=0; j<256; j++) {
            int grpsize = i;
            int ngrps = 0;
            if(i == EXP_D45) {
                grpsize = 4;
            }
            if(j == 7) {
                ngrps = 2;
            } else {
                ngrps = (j + (grpsize * 3) - 4) / (3 * grpsize);
            }
            expsizetab[i-1][j] = (4 + (ngrps * 7));
        }
    }
}

static inline int
calc_lowcomp1(int a, int b0, int b1)
{
    if((b0 + 256) == b1) {
        a = 384;
    } else if(b0 > b1) {
        a = a - 64;
        if(a < 0) a = 0;
    }
    return a;
}

static inline int
calc_lowcomp(int a, int b0, int b1, int bin)
{
    if(bin < 7) {
        if((b0 + 256) == b1) {
            a = 384;
        } else if(b0 > b1) {
            a = a - 64;
            if(a < 0) a=0;
        }
    } else if(bin < 20) {
        if((b0 + 256) == b1) {
            a = 320;
        } else if(b0 > b1) {
            a = a - 64;
            if(a < 0) a = 0;
        }
    } else {
        a = a - 128;
        if(a < 0) a = 0;
    }
    return a;
}

/* A52 bit allocation preparation to speed up matching left bits. */
static void
a52_bit_allocation_prepare(A52BitAllocParams *s, int blk, int ch,
                   uint8_t *exp, int16_t *psd, int16_t *mask,
                   int end, int fgain, int is_lfe,
                   int deltbae,int deltnseg, uint8_t *deltoffst,
                   uint8_t *deltlen, uint8_t *deltba)
{
    int bin, i, j, k, end1, v, v1, bndstrt, bndend, lowcomp, begin;
    int fastleak, slowleak, tmp;
    int16_t bndpsd[50];                     // interpolated exponents
    int16_t excite[50];                     // excitation

    // exponent mapping to PSD
    for(bin=0; bin<end; bin++) {
        psd[bin] = psdtab[exp[bin]];
    }

    // PSD integration
    j = 0;
    k = masktab[0];
    do {
        v = psd[j];
        j++;
        end1 = bndtab[k+1];
        if(end1 > end) end1 = end;
        for(i=j; i<end1; i++) {
            int adr;
            // logadd
            v1 = psd[j];
            adr = MIN((ABS(v-v1) >> 1), 255);
            if(v1 <= v) {
                v = v + latab[adr];
            } else {
                v = v1 + latab[adr];
            }
            j++;
        }
        bndpsd[k] = v;
        k++;
    } while(end > bndtab[k]);

    // excitation function
    bndstrt = masktab[0];
    bndend = masktab[end-1] + 1;

    if(bndstrt == 0) {
        lowcomp = 0;
        lowcomp = calc_lowcomp1(lowcomp, bndpsd[0], bndpsd[1]);
        excite[0] = bndpsd[0] - fgain - lowcomp;
        lowcomp = calc_lowcomp1(lowcomp, bndpsd[1], bndpsd[2]);
        excite[1] = bndpsd[1] - fgain - lowcomp ;
        begin = 7;
        for(bin=2; bin<7; bin++) {
            if(!(is_lfe && bin == 6))
                lowcomp = calc_lowcomp1(lowcomp, bndpsd[bin], bndpsd[bin+1]);
            fastleak = bndpsd[bin] - fgain;
            slowleak = bndpsd[bin] - s->sgain;
            excite[bin] = fastleak - lowcomp;
            if(!(is_lfe && bin == 6)) {
                if(bndpsd[bin] <= bndpsd[bin+1]) {
                    begin = bin + 1;
                    break;
                }
            }
        }

        end1 = bndend;
        if(end1 > 22) end1 = 22;

        for(bin=begin; bin<end1; bin++) {
            if(!(is_lfe && bin == 6))
                lowcomp = calc_lowcomp(lowcomp, bndpsd[bin], bndpsd[bin+1], bin);

            fastleak -= s->fdecay;
            v = bndpsd[bin] - fgain;
            if(fastleak < v) fastleak = v;

            slowleak -= s->sdecay;
            v = bndpsd[bin] - s->sgain;
            if(slowleak < v) slowleak = v;

            v = fastleak - lowcomp;
            if(slowleak > v) v=slowleak;

            excite[bin] = v;
        }
        begin = 22;
    } else {
        // coupling channel
        begin = bndstrt;
        fastleak = (s->cplfleak << 8) + 768;
        slowleak = (s->cplsleak << 8) + 768;
    }

    for(bin=begin; bin<bndend; bin++) {
        fastleak -= s->fdecay;
        v = bndpsd[bin] - fgain;
        if(fastleak < v) fastleak = v;
        slowleak -= s->sdecay;
        v = bndpsd[bin] - s->sgain;
        if(slowleak < v) slowleak = v;

        v = fastleak;
        if(slowleak > v) v = slowleak;
        excite[bin] = v;
    }

    // compute masking curve
    for(bin=bndstrt; bin<bndend; bin++) {
        v1 = excite[bin];
        tmp = s->dbknee - bndpsd[bin];
        if(tmp > 0) {
            v1 += tmp >> 2;
        }
        v = hth[bin >> s->halfratecod][s->fscod];
        if(v1 > v) v = v1;
        mask[bin] = v;
    }

    // delta bit allocation
    if(deltbae == 0 || deltbae == 1) {
        int band, seg, delta;
        band = 0;
        for(seg=0; seg<deltnseg; seg++) {
            band += deltoffst[seg];
            if(deltba[seg] >= 4) {
                delta = (deltba[seg] - 3) << 7;
            } else {
                delta = (deltba[seg] - 4) << 7;
            }
            for(k=0; k<deltlen[seg]; k++) {
                mask[band] += delta;
                band++;
            }
        }
    }
}

/* A52 bit allocation */
static void
a52_bit_allocation(uint8_t *bap, int16_t *psd, int16_t *mask,
                   int blk, int ch, int end, int snroffset, int floor)
{
    int i, j, endj;

    for (i = 0, j = masktab[0]; end > bndtab[j]; ++j) {
        int v = mask[j];
        v -= snroffset;
        v -= floor;
        v = (v < 0) ? 0 : v;
        v &= 0x1fe0;
        v += floor;

        endj = MIN(bndtab[j] + bndsz[j], end);
        while (i < endj) {
            int address = (psd[i] - v) >> 5;
            if (address < 0) address = 0;
            else if (address > 63) address = 63;
            bap[i] = baptab[address];
            ++i;
        }
    }
}

/** return the size in bits taken by the mantissas */
static int
compute_mantissa_size(int mant_cnt[3], uint8_t *bap, int ncoefs)
{
    int bits, b, i;

    bits = 0;
    for(i=0; i<ncoefs; i++) {
        b = bap[i];
        switch(b) {
            case 0:  break;
            case 1:  if(!(mant_cnt[0] & 2)) bits += 5;
                     mant_cnt[0]++;
                     break;
            case 2:  if(!(mant_cnt[1] & 2)) bits += 7;
                     mant_cnt[1]++;
                     break;
            case 3:  bits += 3;
                     break;
            case 4:  if(!(mant_cnt[2] & 1)) bits += 7;
                     mant_cnt[2]++;
                     break;
            case 14: bits += 14;
                     break;
            case 15: bits += 16;
                     break;
            default: bits += b - 1;
        }
    }
    return bits;
}

/* call to prepare bit allocation */
static void
bit_alloc_prepare(A52Context *ctx)
{
    int blk, ch;
    A52Frame *frame;
    A52Block *block;

    frame = &ctx->frame;

    for(blk=0; blk<A52_NUM_BLOCKS; blk++) {
        block = &ctx->frame.blocks[blk];
        for(ch=0; ch<ctx->n_all_channels; ch++) {
            a52_bit_allocation_prepare(&frame->bit_alloc, blk, ch,
                               block->exp[ch], block->psd[ch], block->mask[ch],
                               frame->ncoefs[ch],
                               fgaintab[frame->fgaincod],
                               (ch == ctx->lfe_channel),
                               2, 0, NULL, NULL, NULL);
        }
    }
}

/** returns number of mantissa & exponent bits used */
static int
bit_alloc(A52Context *ctx, int csnroffst, int fsnroffst)
{
    int blk, ch;
    int snroffset, bits;
    int mant_cnt[3];
    A52Frame *frame;
    A52Block *block;

    frame = &ctx->frame;
    bits = 0;
    snroffset = (((csnroffst-15) << 4) + fsnroffst) << 2;

    for(blk=0; blk<A52_NUM_BLOCKS; blk++) {
        block = &ctx->frame.blocks[blk];
        mant_cnt[0] = mant_cnt[1] = mant_cnt[2] = 0;
        for(ch=0; ch<ctx->n_all_channels; ch++) {
            memset(block->bap[ch], 0, 256);
            a52_bit_allocation(block->bap[ch], block->psd[ch], block->mask[ch],
                               blk, ch, frame->ncoefs[ch], snroffset,
                               frame->bit_alloc.floor);
            bits += compute_mantissa_size(mant_cnt, block->bap[ch], frame->ncoefs[ch]);
            if(block->exp_strategy[ch] > 0) {
                bits += expsizetab[block->exp_strategy[ch]-1][frame->ncoefs[ch]];
            }
        }
    }

    return bits;
}

/** Counts all frame bits except for mantissas and exponents */
static void
count_frame_bits(A52Context *ctx)
{
    int blk, ch;
    int frame_bits;
    static int frame_bits_inc[8] = { 0, 0, 2, 2, 2, 4, 2, 4 };
    A52Frame *frame;
    A52Block *block;

    frame = &ctx->frame;
    frame_bits = 0;

    // header size
    frame_bits += 65;
    frame_bits += frame_bits_inc[ctx->acmod];
    if(ctx->meta.xbsi1e) frame_bits += 14;
    if(ctx->meta.xbsi2e) frame_bits += 14;

    // audio blocks
    for(blk=0; blk<A52_NUM_BLOCKS; blk++) {
        block = &frame->blocks[blk];
        frame_bits += ctx->n_channels * 2; // nch * (blksw[1] + dithflg[1])
        frame_bits += 2; // dynrnge, cplstre
        if(ctx->acmod == 2) {
            frame_bits++; // rematstr
            if(block->rematstr) frame_bits += 4; // rematflg
        }
        frame_bits += 2 * ctx->n_channels; // nch * chexpstr[2]
        if(ctx->lfe) frame_bits++; // lfeexpstr
        for(ch=0; ch<ctx->n_channels; ch++) {
            if(block->exp_strategy[ch] != EXP_REUSE)
                frame_bits += 6 + 2; // chbwcod[6], gainrng[2]
        }
        frame_bits++; // baie
        frame_bits++; // snr
        frame_bits += 2; // delta / skip
    }
    frame_bits++; // cplinu for block 0

    // bit alloc info for block 0
    // sdcycod[2], fdcycod[2], sgaincod[2], dbpbcod[2], floorcod[3]
    // csnroffset[6]
    // nch * (fsnoffset[4] + fgaincod[3])
    frame_bits += 2 + 2 + 2 + 2 + 3 + 6 + ctx->n_all_channels * (4 + 3);

    // auxdatae, crcrsv
    frame_bits += 2;

    // CRC
    frame_bits += 16;

    frame->frame_bits = frame_bits;
}

static int
cbr_bit_allocation(A52Context *ctx, int prepare)
{
    int csnroffst, fsnroffst;
    int current_bits, avail_bits, leftover;
    A52Frame *frame;
    A52Block *blocks;

    frame = &ctx->frame;
    current_bits = frame->frame_bits;
    blocks = frame->blocks;
    avail_bits = (16 * frame->frame_size) - current_bits;
    csnroffst = ctx->last_csnroffst;
    fsnroffst = 0;

    if(prepare)
         bit_alloc_prepare(ctx);
    // decrease csnroffst if necessary until data fits in frame
    leftover = avail_bits - bit_alloc(ctx, csnroffst, fsnroffst);
    while(csnroffst > 0 && leftover < 0) {
        csnroffst--;
        if(csnroffst == 0) fsnroffst = 1;
        leftover = avail_bits - bit_alloc(ctx, csnroffst, fsnroffst);
    }

    // if data still won't fit with lowest snroffst, exit with error
    if(leftover < 0) {
        fprintf(stderr, "bitrate: %d kbps too small\n", frame->bit_rate);
        return -1;
    }

    // increase csnroffst while data fits in frame
    leftover = avail_bits - bit_alloc(ctx, csnroffst+4, fsnroffst);
    while((csnroffst+4) <= 63 && leftover >= 0) {
        csnroffst++;
        leftover = avail_bits - bit_alloc(ctx, csnroffst+4, fsnroffst);
    }
    leftover = avail_bits - bit_alloc(ctx, csnroffst+1, fsnroffst);
    while((csnroffst+1) <= 63 && leftover >= 0) {
        csnroffst++;
        leftover = avail_bits - bit_alloc(ctx, csnroffst+1, fsnroffst);
    }

    // increase fsnroffst while data fits in frame
    leftover = avail_bits - bit_alloc(ctx, csnroffst, fsnroffst+4);
    while((fsnroffst+4) <= 15 && leftover >= 0) {
        fsnroffst += 4;
        leftover = avail_bits - bit_alloc(ctx, csnroffst, fsnroffst+4);
    }
    leftover = avail_bits - bit_alloc(ctx, csnroffst, fsnroffst+1);
    while((fsnroffst+1) <= 15 && leftover >= 0) {
        fsnroffst++;
        leftover = avail_bits - bit_alloc(ctx, csnroffst, fsnroffst+1);
    }

    bit_alloc(ctx, csnroffst, fsnroffst);

    ctx->last_csnroffst = csnroffst;
    frame->csnroffst = csnroffst;
    frame->fsnroffst = fsnroffst;
    frame->quality = (((((csnroffst-15) << 4) + fsnroffst) << 2)+960)/4;
    ctx->last_quality = frame->quality;

    return 0;
}

static int
vbr_bit_allocation(A52Context *ctx)
{
    int i;
    int frame_size;
    int snroffst, csnroffst, fsnroffst;
    int frame_bits, current_bits;
    A52Frame *frame;
    A52Block *blocks;

    frame = &ctx->frame;
    current_bits = frame->frame_bits;
    blocks = frame->blocks;

    // convert quality in range 0 to 1023 to csnroffst & fsnroffst
    // csnroffst has range 0 to 63, fsnroffst has range 0 to 15
    // quality of 0 (csnr=0,fsnr=0) is within spec, but seems to have
    // decoding issues...maybe a bug in bit allocation or in liba52?
    snroffst = (ctx->params.quality - 240);
    csnroffst = (snroffst / 16) + 15;
    fsnroffst = (snroffst % 16);
    while(fsnroffst < 0) {
        csnroffst--;
        fsnroffst += 16;
    }

    bit_alloc_prepare(ctx);
    // find an A52 frame size that can hold the data.
    frame_size = frame_bits = 0;
    for(i=0; i<=ctx->frmsizecod; i++) {
        frame_size = frmsizetab[i][ctx->fscod];
        frame_bits = current_bits + bit_alloc(ctx, csnroffst, fsnroffst);
        if(frame_size >= frame_bits) break;
    }
    if(i > ctx->frmsizecod) i = ctx->frmsizecod;
    frame->bit_rate = a52_bitratetab[i/2] >> ctx->halfratecod;

    frame->frmsizecod = i;
    frame->frame_size = frame_size / 16;
    frame->frame_size_min = frame->frame_size;
    ctx->last_csnroffst = csnroffst;

    // run CBR bit allocation.
    // this will increase snroffst to make optimal use of the frame bits.
    // also it will lower snroffst if vbr frame won't fit in largest frame.
    return cbr_bit_allocation(ctx, 0);
}

int
compute_bit_allocation(A52Context *ctx)
{
    A52Frame *f;

    // read bit allocation table values
    f = &ctx->frame;
    f->bit_alloc.fscod = ctx->fscod;
    f->bit_alloc.halfratecod = ctx->halfratecod;
    f->bit_alloc.sdecay = sdecaytab[f->sdecaycod] >> ctx->halfratecod;
    f->bit_alloc.fdecay = fdecaytab[f->fdecaycod] >> ctx->halfratecod;
    f->bit_alloc.sgain = sgaintab[f->sgaincod];
    f->bit_alloc.dbknee = dbkneetab[f->dbkneecod];
    f->bit_alloc.floor = floortab[f->floorcod];

    count_frame_bits(ctx);
    if(ctx->params.encoding_mode == AFTEN_ENC_MODE_VBR) {
        if(vbr_bit_allocation(ctx)) {
            return -1;
        }
    } else if(ctx->params.encoding_mode == AFTEN_ENC_MODE_CBR) {
        if(cbr_bit_allocation(ctx, 1)) {
            return -1;
        }
    } else {
        return -1;
    }
    return 0;
}
