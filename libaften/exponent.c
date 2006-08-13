/**
 * Aften: A/52 audio encoder
 * Copyright (c) 2006  Justin Ruggles <jruggle@earthlink.net>
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
 * @file exponent.c
 * A/52 exponent functions
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "exponent.h"
#include "a52.h"

static int expsizetab[3][256];

#define EXP_DIFF_THRESHOLD 0.75

/* sum of absolute difference between exponents in adjacent blocks */
static double
calc_exp_diff(uint8_t *exp1, uint8_t *exp2, int n)
{
    int i;
    double sum;
    sum = 0;
    for(i=0; i<n; i++) {
        sum += ABS(exp1[i]-exp2[i]);
    }
    sum /= n;
    return sum;
}

void
expsizetab_init()
{
    int i, j, grpsize, ngrps;

    for(i=1; i<4; i++) {
        for(j=0; j<256; j++) {
            grpsize = i;
            ngrps = 0;
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

static void
compute_exponent_strategy(A52Context *ctx)
{
    int ch, i, j;
    int exp_diff;
    int is_lfe;
    A52Frame *frame;
    A52Block *blocks;

    frame = &ctx->frame;
    blocks = frame->blocks;
    for(ch=0; ch<ctx->n_all_channels; ch++) {
        is_lfe = (ch == ctx->lfe_channel);
        // estimate if the exponent variation & decide if they should be
        // reused in the next frame
        blocks[0].exp_strategy[ch] = EXP_NEW;
        for(i=1;i<A52_NUM_BLOCKS;i++) {
            exp_diff = calc_exp_diff(blocks[i].exp[ch], blocks[i-1].exp[ch],
                                     A52_MAX_COEFS);
            if(exp_diff > EXP_DIFF_THRESHOLD)
                blocks[i].exp_strategy[ch] = EXP_NEW;
            else
                blocks[i].exp_strategy[ch] = EXP_REUSE;
        }
        // lfe channel exp strategy can only be EXP_REUSE or EXP_D15
        if(is_lfe) continue;

        // now select the encoding strategy type : if exponents are often
        // recoded, we use a coarse encoding
        i = 0;
        while (i < A52_NUM_BLOCKS) {
            j = i + 1;
            while(j < A52_NUM_BLOCKS && blocks[j].exp_strategy[ch] == EXP_REUSE)
                j++;
            switch(j-i) {
                case 1:  blocks[i].exp_strategy[ch] = EXP_D45; break;
                case 2:
                case 3:  blocks[i].exp_strategy[ch] = EXP_D25; break;
                default: blocks[i].exp_strategy[ch] = EXP_D15;
            }
            i = j;
        }
    }
}

/* set exp[i] to min(exp[i], exp1[i]) */
static void
exponent_min(uint8_t *exp, uint8_t *exp1, int n)
{
    int i;
    for(i=0; i<n; i++) {
        if(exp1[i] < exp[i]) exp[i] = exp1[i];
    }
}

/* update the exponents so that they are the ones the decoder will decode.*/
static void
encode_exp_blk_ch(uint8_t *exp, int ncoefs, int exp_strategy)
{
    int grpsize, ngrps, i, j, k, exp_min;
    uint8_t exp1[256];

    // group count based on full bandwidth (253 coefs)
    grpsize = ngrps = 0;
    switch(exp_strategy) {
        case EXP_D15: grpsize = 1; break;
        case EXP_D25: grpsize = 2; break;
        case EXP_D45: grpsize = 4; break;
    }
    ngrps = ((ncoefs + (grpsize * 3) - 4) / (3 * grpsize)) * 3;

    // for each group, compute the minimum exponent
    exp1[0] = exp[0]; // DC exponent is handled separately
    k = 1;
    for(i=1; i<=ngrps; i++) {
        exp_min = exp[k];
        for(j=1; j<grpsize; j++) {
            exp_min = MIN(exp_min, exp[k+j]);
        }
        exp1[i] = exp_min;
        k += grpsize;
    }

    // constraint for DC exponent
    if(exp1[0] > 15) exp1[0] = 15;

    // Decrease the delta between each groups to within 2
    // so that they can be differentially encoded
    for(i=1; i<=ngrps; i++)
        exp1[i] = MIN(exp1[i], exp1[i-1]+2);
    for(i=ngrps-1; i>=0; i--)
        exp1[i] = MIN(exp1[i], exp1[i+1]+2);

    // now we have the exponent values the decoder will see
    exp[0] = exp1[0];
    k = 1;
    for(i=1; i<=ngrps; i++) {
        for(j=0; j<grpsize; j++) {
            exp[k+j] = exp1[i];
        }
        k += grpsize;
    }
}

static void
group_exponents(A52Context *ctx)
{
    int blk, ch, i, gsize, bits;
    int delta[3];
    uint8_t exp0, exp1;
    uint8_t *p;
    A52Frame *frame;
    A52Block *block;

    bits = 0;
    frame = &ctx->frame;
    for(blk=0; blk<A52_NUM_BLOCKS; blk++) {
        block = &frame->blocks[blk];
        for(ch=0; ch<ctx->n_all_channels; ch++) {
            gsize = block->exp_strategy[ch];
            if(gsize == EXP_REUSE) {
                block->nexpgrps[ch] = 0;
                continue;
            }
            if(gsize == EXP_D45) gsize = 4;
            if(ch == ctx->lfe_channel) {
                block->nexpgrps[ch] = 2;
            } else {
                block->nexpgrps[ch] = (frame->ncoefs[ch]+(gsize*3)-4)/(3*gsize);
            }
            bits += (4 + (block->nexpgrps[ch] * 7));
            p = block->exp[ch];

            exp1 = *p++;
            block->grp_exp[ch][0] = exp1;

            for(i=1; i<=block->nexpgrps[ch]; i++) {
                /* merge three delta into one code */
                exp0 = exp1;
                exp1 = p[0];
                p += gsize;
                delta[0] = exp1 - exp0 + 2;

                exp0 = exp1;
                exp1 = p[0];
                p += gsize;
                delta[1] = exp1 - exp0 + 2;

                exp0 = exp1;
                exp1 = p[0];
                p += gsize;
                delta[2] = exp1 - exp0 + 2;

                block->grp_exp[ch][i] = ((delta[0]*5+delta[1])*5)+delta[2];
            }
        }
    }
    frame->exp_bits = bits;
}

static void
encode_exponents(A52Context *ctx)
{
    int ch, i, j, k;
    A52Frame *frame;
    A52Block *blocks;

    frame = &ctx->frame;
    blocks = frame->blocks;
    for(ch=0; ch<ctx->n_all_channels; ch++) {
        // compute the exponents as the decoder will see them. The
        // EXP_REUSE case must be handled carefully : we select the
        // min of the exponents
        i = 0;
        while(i < A52_NUM_BLOCKS) {
            j = i + 1;
            while(j < A52_NUM_BLOCKS && blocks[j].exp_strategy[ch]==EXP_REUSE) {
                exponent_min(blocks[i].exp[ch], blocks[j].exp[ch],
                             A52_MAX_COEFS);
                j++;
            }
            encode_exp_blk_ch(blocks[i].exp[ch], frame->ncoefs[ch],
                              blocks[i].exp_strategy[ch]);
            // copy encoded exponents for reuse case
            for(k=i+1; k<j; k++) {
                memcpy(blocks[k].exp[ch], blocks[i].exp[ch], A52_MAX_COEFS);
            }
            i = j;
        }
    }
}

static void
extract_exponents(A52Context *ctx)
{
    int blk, ch, j;
    int mul, v;
    A52Block *block;

    mul = (1 << 24);
    for(ch=0; ch<ctx->n_all_channels; ch++) {
        for(blk=0; blk<A52_NUM_BLOCKS; blk++) {
            block = &ctx->frame.blocks[blk];
            for(j=0; j<256; j++) {
                v = fabs(block->mdct_coef[ch][j] * mul);
                block->exp[ch][j] = (v == 0)? 24 : 23 - log2i(v);
            }
        }
    }
}

void
process_exponents(A52Context *ctx)
{
    extract_exponents(ctx);

    compute_exponent_strategy(ctx);

    encode_exponents(ctx);

    group_exponents(ctx);
}
