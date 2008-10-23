/**
 * Aften: A/52 audio encoder
 * Copyright (c) 2006 Justin Ruggles
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

#include "exponent_common.c"

#include "cpu_caps.h"

uint16_t expstr_set_bits[A52_EXPSTR_SETS][256] = {{0}};

static void process_exponents(A52ThreadContext *tctx);

/**
 * Initialize exponent group size table
 */
void
exponent_init(A52Context *ctx)
{
    int i, j, grpsize, ngrps, nc, blk;

    for (i = 1; i < 4; i++) {
        for (j = 0; j < 256; j++) {
            grpsize = i + (i == EXP_D45);
            ngrps = 0;
            if (j == 7)
                ngrps = 2;
            else
                ngrps = (j + (grpsize * 3) - 4) / (3 * grpsize);
            nexpgrptab[i-1][j] = ngrps;
        }
    }

    for (i = 0; i < 6; i++) {
        uint16_t *expbits = expstr_set_bits[i];
        for (nc = 0; nc <= 253; nc++) {
            uint16_t bits = 0;
            for (blk = 0; blk < A52_NUM_BLOCKS; blk++) {
                uint8_t es = str_predef[i][blk];
                if (es != EXP_REUSE)
                    bits += (4 + (nexpgrptab[es-1][nc] * 7));
            }
            expbits[nc] = bits;
        }
    }

    ctx->process_exponents = process_exponents;
#ifdef HAVE_MMX
    if (cpu_caps_have_mmx()) {
        ctx->process_exponents = mmx_process_exponents;
    }
#endif /* HAVE_MMX */
#ifdef HAVE_SSE2
    if (cpu_caps_have_sse2()) {
        ctx->process_exponents = sse2_process_exponents;
    }
#endif /* HAVE_SSE2 */
}

/** Set exp[i] to min(exp[i], exp1[i]) */
static void
exponent_min(uint8_t *exp, uint8_t *exp1, int n)
{
    int i;
    for (i = 0; i < n; i++)
        exp[i] = MIN(exp[i], exp1[i]);
}


/**
 * Update the exponents so that they are the ones the decoder will decode.
 * Constrain DC exponent, group exponents based on strategy, constrain delta
 * between adjacent exponents to +2/-2.
 */
static void
encode_exp_blk_ch(uint8_t *exp, int ncoefs, int exp_strategy)
{
    int i, k;
    int ngrps;
    int exp_min1, exp_min2;

    ngrps = nexpgrptab[exp_strategy-1][ncoefs] * 3;

    // constraint for DC exponent
    exp[0] = MIN(exp[0], 15);

    // for each group, compute the minimum exponent
    switch (exp_strategy) {
    case EXP_D25:
        for (i = 1, k = 1; i <= ngrps; i++) {
            exp[i] = MIN(exp[k], exp[k+1]);
            k += 2;
        }
        break;
    case EXP_D45:
        for (i = 1, k = 1; i <= ngrps; i++) {
            exp_min1 = MIN(exp[k  ], exp[k+1]);
            exp_min2 = MIN(exp[k+2], exp[k+3]);
            exp[i]   = MIN(exp_min1, exp_min2);
            k += 4;
        }
        break;
    }

    // Decrease the delta between each groups to within 2
    // so that they can be differentially encoded
    for (i = 1; i <= ngrps; i++)
        exp[i] = MIN(exp[i], exp[i-1]+2);
    for (i = ngrps-1; i >= 0; i--)
        exp[i] = MIN(exp[i], exp[i+1]+2);

    // expand exponent groups to generate final set of exponents
    switch (exp_strategy) {
    case EXP_D25:
        for (i = ngrps, k = ngrps*2; i > 0; i--) {
            exp[k] = exp[k-1] = exp[i];
            k -= 2;
        }
        break;
    case EXP_D45:
        for (i = ngrps, k = ngrps*4; i > 0; i--) {
            exp[k] = exp[k-1] = exp[k-2] = exp[k-3] = exp[i];
            k -= 4;
        }
        break;
    }
}


static int
exponent_sum_square_error(uint8_t *exp0, uint8_t *exp1, int ncoefs)
{
    int i, err;
    int exp_error = 0;

            for (i = 0; i < ncoefs; i++) {
                err = exp0[i] - exp1[i];
                exp_error += (err * err);
            }
    return exp_error;
}


/**
 * Runs all the processes in extracting, analyzing, and encoding exponents
 */
static void
process_exponents(A52ThreadContext *tctx)
{
    extract_exponents(tctx);

    compute_exponent_strategy(tctx);

    encode_exponents(tctx);

    group_exponents(tctx);
}
