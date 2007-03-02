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

#include "exponent_common.c"

#include "cpu_caps.h"

static void process_exponents(A52ThreadContext *tctx);

/**
 * Initialize exponent group size table
 */
void
exponent_init(A52Context *ctx)
{
    int i, j, grpsize, ngrps;

    for(i=1; i<4; i++) {
        for(j=0; j<256; j++) {
            grpsize = i + (i == EXP_D45);
            ngrps = 0;
            if(j == 7) {
                ngrps = 2;
            } else {
                ngrps = (j + (grpsize * 3) - 4) / (3 * grpsize);
            }
            nexpgrptab[i-1][j] = ngrps;
        }
    }

#ifdef HAVE_SSE2
    if (_alHaveSSE2()) {
        ctx->process_exponents = sse2_process_exponents;
        return;
    }
#endif /* HAVE_SSE2 */
#ifdef HAVE_MMX
    if (_alHaveMMX()) {
        ctx->process_exponents = mmx_process_exponents;
        return;
    }
#endif /* HAVE_MMX */
    ctx->process_exponents = process_exponents;
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
