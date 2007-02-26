/**
 * Aften: A/52 audio encoder
 * Copyright (c) 2007 Prakash Punnoor <prakash@punnoor.de>
 *               2006 Justin Ruggles <justinruggles@bellsouth.net>
 *
 * Based on "The simplest AC3 encoder" from FFmpeg
 * Copyright (c) 2000 Fabrice Bellard.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License
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
 * @file x86_sse2_exponent.c
 * A/52 sse2 optimized exponent functions
 */

#include "exponent_common.c"

#include <emmintrin.h>

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

    for(i=0; i<(n^15); i+=16) {
        __m128i vexp = _mm_loadu_si128((__m128i*)&exp[i]);
        __m128i vexp1 = _mm_loadu_si128((__m128i*)&exp1[i]);
        vexp = _mm_min_epu8(vexp, vexp1);
        _mm_storeu_si128 ((__m128i*)&exp[i], vexp);
    }
    for(; i<n; ++i)
        exp[i] = MIN(exp[i], exp1[i]);
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
    ALIGN16(uint8_t) exponents[A52_NUM_BLOCKS][256];

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
            union {
                __m128i v;
                int32_t res[4];
            } ures;
            __m128i vzero = _mm_setzero_si128();
            __m128i vres = vzero;

            for(i=0; i<(ncoefs^15); i+=16) {
                __m128i vexp = _mm_loadu_si128((__m128i*)&exp_blk[i]);
                __m128i vexp2 = _mm_load_si128((__m128i*)&exponents_blk[i]);
#if 0
                //safer but needed?
                __m128i vexphi = _mm_unpackhi_epi8(vexp, vzero);
                __m128i vexp2hi = _mm_unpackhi_epi8(vexp2, vzero);
                __m128i vexplo = _mm_unpacklo_epi8(vexp, vzero);
                __m128i vexp2lo = _mm_unpacklo_epi8(vexp2, vzero);
                __m128i verrhi = _mm_sub_epi16(vexphi, vexp2hi);
                __m128i verrlo = _mm_sub_epi16(vexplo, vexp2lo);
#else
                __m128i verr = _mm_sub_epi8(vexp, vexp2);
                __m128i vsign = _mm_cmplt_epi8(verr, vzero);
                __m128i verrhi = _mm_unpackhi_epi8(verr, vsign);
                __m128i verrlo = _mm_unpacklo_epi8(verr, vsign);
#endif
                verrhi = _mm_madd_epi16(verrhi, verrhi);
                verrlo = _mm_madd_epi16(verrlo, verrlo);
                verrhi = _mm_add_epi32(verrhi, verrlo);
                vres = _mm_add_epi32(vres, verrhi);
            }
            _mm_store_si128(&ures.v, vres);
            ures.res[0]+=ures.res[1];
            ures.res[2]+=ures.res[3];
            exp_error[str] += ures.res[0]+ures.res[2];
            for(; i<ncoefs; ++i) {
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
void
sse2_process_exponents(A52ThreadContext *tctx)
{
    extract_exponents(tctx);

    compute_exponent_strategy(tctx);

    encode_exponents(tctx);

    group_exponents(tctx);
}
