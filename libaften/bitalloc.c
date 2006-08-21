/**
 * Aften: A/52 audio encoder
 * Copyright (c) 2006  Justin Ruggles <jruggle@earthlink.net>
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

/* log addition table */
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

/* hearing threshold table */
static const uint16_t hth[50][3]= {
    { 1232, 1264, 1408 },
    { 1232, 1264, 1408 },
    { 1088, 1120, 1200 },
    { 1024, 1040, 1104 },
    {  992,  992, 1056 },
    {  960,  976, 1008 },
    {  944,  960,  992 },
    {  944,  944,  976 },
    {  928,  944,  960 },
    {  928,  928,  944 },
    {  928,  928,  944 },
    {  928,  928,  944 },
    {  928,  928,  928 },
    {  912,  928,  928 },
    {  912,  912,  928 },
    {  912,  912,  928 },
    {  896,  912,  928 },
    {  896,  896,  928 },
    {  880,  896,  928 },
    {  880,  896,  928 },
    {  864,  880,  912 },
    {  864,  880,  912 },
    {  848,  864,  912 },
    {  848,  864,  912 },
    {  832,  848,  896 },
    {  832,  848,  896 },
    {  816,  832,  896 },
    {  800,  832,  880 },
    {  784,  800,  864 },
    {  768,  784,  848 },
    {  752,  768,  832 },
    {  752,  752,  816 },
    {  752,  752,  800 },
    {  752,  752,  784 },
    {  768,  752,  768 },
    {  784,  768,  752 },
    {  832,  800,  752 },
    {  912,  848,  752 },
    {  992,  912,  768 },
    { 1056,  992,  784 },
    { 1120, 1056,  816 },
    { 1168, 1104,  848 },
    { 1184, 1184,  960 },
    { 1120, 1168, 1040 },
    { 1088, 1120, 1136 },
    { 1088, 1088, 1184 },
    { 1312, 1152, 1120 },
    { 2048, 1584, 1088 },
    { 2112, 2112, 1104 },
    { 2112, 2112, 1248 },
};

/* bit allocation pointer table */
static const uint8_t baptab[64]= {
     0,  1,  1,  1,  1,  1,  2,  2,  3,  3,  3,  4,  4,  5,  5,  6,
     6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  8,  9,  9,  9,  9, 10,
    10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 14,
    14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15
};

/* slow gain table */
static const uint16_t sgaintab[4]= {
    1344, 1240, 1144, 1040,
};

/* dB per bit table */
static const uint16_t dbkneetab[4]= {
    0, 1792, 2304, 2816,
};

/* floor table */
static const uint16_t floortab[8]= {
    752, 688, 624, 560, 496, 368, 240, 63488,
};

/* band size table (number of bins in each band) */
static const uint8_t bndsz[50]={
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, 1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  3,  3,  3,  3,  3, 3,
     3,  6,  6,  6,  6,  6,  6, 12, 12, 12, 12, 24, 24, 24, 24, 24
};

/* slow decay table */
static uint8_t sdecaytab[4];

/* fast decay table */
static uint8_t fdecaytab[4];

/* fast gain table */
static uint16_t fgaintab[8];

/* power spectral density table */
static uint16_t psdtab[25];

/* mask table (maps bin# to band#) */
static uint8_t masktab[253];

/* band table (starting bin for each band) */
static uint8_t bndtab[51];

/* frame size table */
static uint16_t frmsizetab[38][3];

void
bitalloc_init()
{
    int i, j, k, l, v;

    // compute sdecaytab
    for(i=0; i<4; i++) {
        sdecaytab[i] = (i * 2) + 15;
    }

    // compute fdecaytab
    for(i=0; i<4; i++) {
        fdecaytab[i] = (i * 20) + 63;
    }

    // compute fgaintab
    for(i=0; i<8; i++) {
        fgaintab[i] = (i + 1) * 128;
    }

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
        a = calc_lowcomp1(a, b0, b1);
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

static int
psd_combine(int16_t *psd, int bins)
{
    int i, adr;
    int v;

    v = psd[0];
    for(i=1; i<bins; i++) {
        adr = MIN((ABS(v-psd[i]) >> 1), 255);
        v = MAX(v, psd[i]) + latab[adr];
    }
    return v;
}

/* A52 bit allocation preparation to speed up matching left bits. */
static void
a52_bit_allocation_prepare(A52BitAllocParams *s, int blk, int ch,
                   uint8_t *exp, int16_t *psd, int16_t *mask,
                   int end, int is_lfe,
                   int deltbae,int deltnseg, uint8_t *deltoffst,
                   uint8_t *deltlen, uint8_t *deltba)
{
    int bnd, i, end1, bndstrt, bndend, lowcomp, begin;
    int fastleak, slowleak;
    int16_t bndpsd[50]; // power spectral density for critical bands
    int16_t excite[50]; // excitation function

    if(end <= 0) return;

    // exponent mapping to PSD
    for(i=0; i<end; i++) {
        psd[i] = psdtab[exp[i]];
    }

    // use log addition to combine PSD for each critical band
    bndstrt = masktab[0];
    bndend = masktab[end-1] + 1;
    i = 0;
    for(bnd=bndstrt; bnd<bndend; bnd++) {
        int bins = MIN(bndtab[bnd+1], end) - i;
        bndpsd[bnd] = psd_combine(&psd[i], bins);
        i += bins;
    }

    // excitation function
    if(bndstrt == 0) {
        // fbw and lfe channels
        lowcomp = 0;
        lowcomp = calc_lowcomp1(lowcomp, bndpsd[0], bndpsd[1]);
        excite[0] = bndpsd[0] - s->fgain - lowcomp;
        lowcomp = calc_lowcomp1(lowcomp, bndpsd[1], bndpsd[2]);
        excite[1] = bndpsd[1] - s->fgain - lowcomp ;
        begin = 7;
        for(bnd=2; bnd<7; bnd++) {
            if(!(is_lfe && bnd == 6)) {
                lowcomp = calc_lowcomp1(lowcomp, bndpsd[bnd], bndpsd[bnd+1]);
            }
            fastleak = bndpsd[bnd] - s->fgain;
            slowleak = bndpsd[bnd] - s->sgain;
            excite[bnd] = fastleak - lowcomp;
            if(!(is_lfe && bnd == 6)) {
                if(bndpsd[bnd] <= bndpsd[bnd+1]) {
                    begin = bnd + 1;
                    break;
                }
            }
        }

        end1 = MIN(bndend, 22);

        for(bnd=begin; bnd<end1; bnd++) {
            if(!(is_lfe && bnd == 6)) {
                lowcomp = calc_lowcomp(lowcomp, bndpsd[bnd], bndpsd[bnd+1], bnd);
            }
            fastleak -= s->fdecay;
            fastleak = MAX(fastleak, bndpsd[bnd]-s->fgain);
            slowleak -= s->sdecay;
            slowleak = MAX(slowleak, bndpsd[bnd]-s->sgain);
            excite[bnd] = MAX(slowleak, fastleak-lowcomp);
        }
        begin = 22;
    } else {
        // coupling channel
        begin = bndstrt;
        fastleak = (s->cplfleak << 8) + 768;
        slowleak = (s->cplsleak << 8) + 768;
    }

    for(bnd=begin; bnd<bndend; bnd++) {
        fastleak -= s->fdecay;
        fastleak = MAX(fastleak, bndpsd[bnd]-s->fgain);
        slowleak -= s->sdecay;
        slowleak = MAX(slowleak, bndpsd[bnd]-s->sgain);
        excite[bnd] = MAX(slowleak, fastleak);
    }

    // compute masking curve from excitation function and hearing threshold
    for(bnd=bndstrt; bnd<bndend; bnd++) {
        if(bndpsd[bnd] < s->dbknee) {
            excite[bnd] += (s->dbknee - bndpsd[bnd]) >> 2;
        }
        mask[bnd] = MAX(excite[bnd], hth[bnd >> s->halfratecod][s->fscod]);
    }

    // delta bit allocation
    if(deltbae == 0 || deltbae == 1) {
        int seg, delta;
        bnd = 0;
        for(seg=0; seg<deltnseg; seg++) {
            bnd += deltoffst[seg];
            if(deltba[seg] >= 4) {
                delta = (deltba[seg] - 3) << 7;
            } else {
                delta = (deltba[seg] - 4) << 7;
            }
            for(i=0; i<deltlen[seg]; i++) {
                mask[bnd] += delta;
                bnd++;
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
    int v, address1, address2, offset;

    if(snroffset == SNROFFST(0, 0)) {
        for(i=0; i<end; i++) {
            bap[i] = 0;
        }
        return;
    }

    offset = snroffset + floor;
    for (i = 0, j = masktab[0]; end > bndtab[j]; ++j) {
        v = (MAX(mask[j] - offset, 0) & 0x1FE0) + floor;
        endj = MIN(bndtab[j] + bndsz[j], end);
        if ((endj-i) & 1) {
            address1 = (psd[i] - v) >> 5;
            address1 = CLIP(address1, 0, 63);
            bap[i] = baptab[address1];
            ++i;
        }
        while (i < endj) {
            address1 = (psd[i  ] - v) >> 5;
            address2 = (psd[i+1] - v) >> 5;
            address1 = CLIP(address1, 0, 63);
            address2 = CLIP(address2, 0, 63);
            bap[i  ] = baptab[address1];
            bap[i+1] = baptab[address2];
            i+=2;
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
                               (ch == ctx->lfe_channel),
                               2, 0, NULL, NULL, NULL);
        }
    }
}

/** returns number of mantissa bits used */
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
    snroffset = SNROFFST(csnroffst, fsnroffst);

    for(blk=0; blk<A52_NUM_BLOCKS; blk++) {
        block = &ctx->frame.blocks[blk];
        mant_cnt[0] = mant_cnt[1] = mant_cnt[2] = 0;
        for(ch=0; ch<ctx->n_all_channels; ch++) {
            memset(block->bap[ch], 0, 256);
            a52_bit_allocation(block->bap[ch], block->psd[ch], block->mask[ch],
                               blk, ch, frame->ncoefs[ch], snroffset,
                               frame->bit_alloc.floor);
            bits += compute_mantissa_size(mant_cnt, block->bap[ch], frame->ncoefs[ch]);

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
    current_bits = frame->frame_bits + frame->exp_bits;
    blocks = frame->blocks;
    avail_bits = (16 * frame->frame_size) - current_bits;
    csnroffst = ctx->last_csnroffst;
    fsnroffst = 0;

    if(prepare)
         bit_alloc_prepare(ctx);
    // decrease csnroffst if necessary until data fits in frame
    leftover = avail_bits - bit_alloc(ctx, csnroffst, fsnroffst);
    while(csnroffst > 0 && leftover < 0) {
        csnroffst = MAX(csnroffst-4, 0);
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
        csnroffst+=4;
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

    frame->mant_bits = bit_alloc(ctx, csnroffst, fsnroffst);

    ctx->last_csnroffst = csnroffst;
    frame->csnroffst = csnroffst;
    frame->fsnroffst = fsnroffst;
    frame->quality = QUALITY(csnroffst, fsnroffst);
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
    current_bits = frame->frame_bits + frame->exp_bits;
    blocks = frame->blocks;

    // convert quality in range 0 to 1023 to csnroffst & fsnroffst
    // csnroffst has range 0 to 63, fsnroffst has range 0 to 15
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
    i = MIN(i, ctx->frmsizecod);
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
    f->bit_alloc.fgain = fgaintab[f->fgaincod];
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
