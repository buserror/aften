/**
 * Aften: A/52 audio encoder
 * Copyright (c) 2006  Justin Ruggles <justinruggles@bellsouth.net>
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

/**
 * LUT for number of exponent groups present.
 * expsizetab[exponent strategy][number of coefficients]
 */
static int nexpgrptab[3][256];

/**
 * Pre-defined sets of exponent strategies. A strategy set is selected for
 * each channel in a frame.  All sets 1 to 5 use the same number of exponent
 * bits.  Set 0 is only used as the reference of optimal accuracy.
 * TODO: more options and other sets which use greater or fewer bits
 */
static uint8_t str_predef[6][6] = {
    { EXP_D15,   EXP_D15,   EXP_D15,   EXP_D15,   EXP_D15,   EXP_D15 },
    { EXP_D15, EXP_REUSE, EXP_REUSE, EXP_REUSE, EXP_REUSE, EXP_REUSE },
    { EXP_D25, EXP_REUSE, EXP_REUSE,   EXP_D25, EXP_REUSE, EXP_REUSE },
    { EXP_D25, EXP_REUSE, EXP_REUSE,   EXP_D45, EXP_REUSE,   EXP_D45 },
    { EXP_D25, EXP_REUSE,   EXP_D45, EXP_REUSE,   EXP_D45, EXP_REUSE },
    { EXP_D45,   EXP_D45, EXP_REUSE,   EXP_D45, EXP_REUSE,   EXP_D45 }
};

/**
 * Initialize exponent group size table
 */
void
exponent_init(void)
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
            nexpgrptab[i-1][j] = ngrps;
        }
    }
}

/* set exp[i] to min(exp[i], exp1[i]) */
static void
exponent_min(uint8_t *exp, uint8_t *exp1, int n)
{
    int i;
    for(i=0; i<n; i++) {
        exp[i] = MIN(exp[i], exp1[i]);
    }
}

/**
 * Update the exponents so that they are the ones the decoder will decode.
 * Constrain DC exponent, group exponents based on strategy, constrain delta
 * between adjacent exponents to +2/-2.
 */
static void
encode_exp_blk_ch(uint8_t *exp, int ncoefs, int exp_strategy)
{
    int grpsize, ngrps, i, k, exp_min1, exp_min2;
    uint8_t exp1[256];
    uint8_t v;

    ngrps = nexpgrptab[exp_strategy-1][ncoefs] * 3;
    grpsize = exp_strategy + (exp_strategy / 3);

    // for D15 strategy, there is no need to group/ungroup exponents
    if (grpsize == 1) {
        // constraint for DC exponent
        exp[0] = MIN(exp[0], 15);

        // Decrease the delta between each groups to within 2
        // so that they can be differentially encoded
        for(i=1; i<=ngrps; i++)
            exp[i] = MIN(exp[i], exp[i-1]+2);
        for(i=ngrps-1; i>=0; i--)
            exp[i] = MIN(exp[i], exp[i+1]+2);

        return;
    }

    // for each group, compute the minimum exponent
    exp1[0] = exp[0]; // DC exponent is handled separately
    if (grpsize == 2) {
        for(i=1,k=1; i<=ngrps; i++) {
            exp1[i] = MIN(exp[k], exp[k+1]);
            k += 2;
        }
    } else {
        for(i=1,k=1; i<=ngrps; i++) {
            exp_min1 = MIN(exp[k  ], exp[k+1]);
            exp_min2 = MIN(exp[k+2], exp[k+3]);
            exp1[i]  = MIN(exp_min1, exp_min2);
            k += 4;
        }
    }

    // constraint for DC exponent
    exp1[0] = MIN(exp1[0], 15);

    // Decrease the delta between each groups to within 2
    // so that they can be differentially encoded
    for(i=1; i<=ngrps; i++)
        exp1[i] = MIN(exp1[i], exp1[i-1]+2);
    for(i=ngrps-1; i>=0; i--)
        exp1[i] = MIN(exp1[i], exp1[i+1]+2);

    // now we have the exponent values the decoder will see
    exp[0] = exp1[0]; // DC exponent is handled separately
    if (grpsize == 2) {
        for(i=1,k=1; i<=ngrps; i++) {
            v = exp1[i];
            exp[k] = v;
            exp[k+1] = v;
            k += 2;
        }
    } else {
        for(i=1,k=1; i<=ngrps; i++) {
            v = exp1[i];
            exp[k] = v;
            exp[k+1] = v;
            exp[k+2] = v;
            exp[k+3] = v;
            k += 4;
        }
    }
}

/**
 * Determine a good exponent strategy for all blocks of a single channel.
 * A pre-defined set of strategies is chosen based on the SSE between each set
 * and the most accurate strategy set (all blocks EXP_D15).
 */
static int
compute_expstr_ch(uint8_t *exp[A52_NUM_BLOCKS], int ncoefs)
{
    int blk, str, i, j, k;
    int min_error, exp_error[6];
    int err;
    uint8_t exponents[A52_NUM_BLOCKS][256];

    min_error = 1;
    for(str=1; str<6; str++) {
        // collect exponents
        for(blk=0; blk<A52_NUM_BLOCKS; blk++) {
            memcpy(exponents[blk], exp[blk], 256);
        }

        // encode exponents
        i = 0;
        while(i < A52_NUM_BLOCKS) {
            j = i + 1;
            while(j < A52_NUM_BLOCKS && str_predef[str][j]==EXP_REUSE) {
                exponent_min(exponents[i], exponents[j], ncoefs);
                j++;
            }
            encode_exp_blk_ch(exponents[i], ncoefs, str_predef[str][i]);
            for(k=i+1; k<j; k++) {
                memcpy(exponents[k], exponents[i], 256);
            }
            i = j;
        }

        // select strategy based on minimum error from unencoded exponents
        exp_error[str] = 0;
        for(blk=0; blk<A52_NUM_BLOCKS; blk++) {
            uint8_t *exp_blk = exp[blk];
            uint8_t *exponents_blk = exponents[blk];
            for(i=0; i<ncoefs; i++) {
                err = exp_blk[i] - exponents_blk[i];
                exp_error[str] += (err * err);
            }
        }
        if(exp_error[str] < exp_error[min_error]) {
            min_error = str;
        }
    }
    return min_error;
}

/**
 * Runs the per-channel exponent strategy decision function for all channels
 */
static void
compute_exponent_strategy(A52ThreadContext *tctx)
{
    A52Context *ctx = tctx->ctx;
    A52Frame *frame = &tctx->frame;
    A52Block *blocks = frame->blocks;
    int *ncoefs = frame->ncoefs;
    uint8_t *exp[A52_MAX_CHANNELS][A52_NUM_BLOCKS];
    int ch, blk, str;

    if(ctx->params.expstr_fast) {
        for(ch=0; ch<ctx->n_channels; ch++) {
            for(blk=0; blk<A52_NUM_BLOCKS; blk++) {
                blocks[blk].exp_strategy[ch] = str_predef[4][blk];
            }
        }
    } else {
        for(ch=0; ch<ctx->n_channels; ch++) {
            for(blk=0; blk<A52_NUM_BLOCKS; blk++)
                exp[ch][blk] = blocks[blk].exp[ch];

            str = compute_expstr_ch(exp[ch], ncoefs[ch]);
            for(blk=0; blk<A52_NUM_BLOCKS; blk++) {
                blocks[blk].exp_strategy[ch] = str_predef[str][blk];
            }
        }
    }

    // lfe channel
    if(ctx->lfe) {
        for(blk=0; blk<A52_NUM_BLOCKS; blk++) {
            blocks[blk].exp_strategy[ctx->lfe_channel] = str_predef[1][blk];
        }
    }
}

/**
 * Encode exponent groups.  3 exponents are in per 7-bit group.  The number of
 * groups varies depending on exponent strategy and bandwidth
 */
static void
group_exponents(A52ThreadContext *tctx)
{
    A52Context *ctx = tctx->ctx;
    A52Frame *frame = &tctx->frame;
    A52Block *block;
    uint8_t *p;
    int delta[3];
    int blk, ch, i, gsize, bits;
    int expstr;
    int exp0, exp1, exp2, exp3;

    bits = 0;
    for(blk=0; blk<A52_NUM_BLOCKS; blk++) {
        block = &frame->blocks[blk];
        for(ch=0; ch<ctx->n_all_channels; ch++) {
            expstr = block->exp_strategy[ch];
            if(expstr == EXP_REUSE) {
                block->nexpgrps[ch] = 0;
                continue;
            }
            block->nexpgrps[ch] = nexpgrptab[expstr-1][frame->ncoefs[ch]];
            bits += (4 + (block->nexpgrps[ch] * 7));
            gsize = expstr + (expstr == EXP_D45);
            p = block->exp[ch];

            exp1 = *p++;
            block->grp_exp[ch][0] = exp1;

            for(i=1; i<=block->nexpgrps[ch]; i++) {
                /* merge three delta into one code */
                exp0 = exp1;
                exp1 = p[0];
                p += gsize;
                delta[0] = exp1 - exp0 + 2;

                exp2 = p[0];
                p += gsize;
                delta[1] = exp2 - exp1 + 2;

                exp3 = p[0];
                p += gsize;
                delta[2] = exp3 - exp2 + 2;
                exp1 = exp3;

                block->grp_exp[ch][i] = ((delta[0]*5+delta[1])*5)+delta[2];
            }
        }
    }
    frame->exp_bits = bits;
}

/**
 * Creates final exponents for the entire frame based on exponent strategies.
 * If the strategy for a block & channel is EXP_REUSE, exponents are copied,
 * otherwise they are encoded according to the specific exponent strategy.
 */
static void
encode_exponents(A52ThreadContext *tctx)
{
    A52Context *ctx = tctx->ctx;
    A52Frame *frame = &tctx->frame;
    A52Block *blocks = frame->blocks;
    int *ncoefs = frame->ncoefs;
    int ch, i, j, k;

    for(ch=0; ch<ctx->n_all_channels; ch++) {
        // compute the exponents as the decoder will see them. The
        // EXP_REUSE case must be handled carefully : we select the
        // min of the exponents
        i = 0;
        while(i < A52_NUM_BLOCKS) {
            j = i + 1;
            while(j < A52_NUM_BLOCKS && blocks[j].exp_strategy[ch]==EXP_REUSE) {
                exponent_min(blocks[i].exp[ch], blocks[j].exp[ch], ncoefs[ch]);
                j++;
            }
            encode_exp_blk_ch(blocks[i].exp[ch], ncoefs[ch],
                              blocks[i].exp_strategy[ch]);
            // copy encoded exponents for reuse case
            for(k=i+1; k<j; k++) {
                memcpy(blocks[k].exp[ch], blocks[i].exp[ch], ncoefs[ch]);
            }
            i = j;
        }
    }
}

/**
 * Extracts the optimal exponent portion of each MDCT coefficient.
 */
static void
extract_exponents(A52ThreadContext *tctx)
{
    A52Frame *frame = &tctx->frame;
    A52Block *block;
    int all_channels = tctx->ctx->n_all_channels;
    int blk, ch, j;
    uint32_t v1, v2;
    FLOAT mul;

    mul = (FLOAT)(1 << 24);
    for(ch=0; ch<all_channels; ch++) {
        for(blk=0; blk<A52_NUM_BLOCKS; blk++) {
            block = &frame->blocks[blk];
            for(j=0; j<256; j+=2) {
                v1 = (uint32_t)AFT_FABS(block->mdct_coef[ch][j  ] * mul);
                v2 = (uint32_t)AFT_FABS(block->mdct_coef[ch][j+1] * mul);
                block->exp[ch][j  ] = (v1 == 0)? 24 : 23 - log2i(v1);
                block->exp[ch][j+1] = (v2 == 0)? 24 : 23 - log2i(v2);
            }
        }
    }
}

/**
 * Runs all the processes in extracting, analyzing, and encoding exponents
 */
void
process_exponents(A52ThreadContext *tctx)
{
    extract_exponents(tctx);

    compute_exponent_strategy(tctx);

    encode_exponents(tctx);

    group_exponents(tctx);
}
