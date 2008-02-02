/**
 * Aften: A/52 audio encoder -  Common code between encoder and decoder
 * Copyright (c) 2008 Prakash Punnoor <prakash@punnoor.de>
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
 * @file a52.c
 * Common code between A/52 encoder and decoder.
 */

#include <string.h>

#include "common.h"
#include "a52.h"

static uint8_t band_start_tab[51];
static uint8_t bin_to_band_tab[253];

/* power spectral density table */
static uint16_t psdtab[25];


static inline int calc_lowcomp1(int a, int b0, int b1, int c)
{
    if ((b0 + 256) == b1) {
        a = c;
    } else if (b0 > b1) {
        a = MAX(a - 64, 0);
    }
    return a;
}

static inline int calc_lowcomp(int a, int b0, int b1, int bin)
{
    if (bin < 7) {
        return calc_lowcomp1(a, b0, b1, 384);
    } else if (bin < 20) {
        return calc_lowcomp1(a, b0, b1, 320);
    } else {
        return MAX(a - 128, 0);
    }
}

void a52_bit_alloc_calc_psd(uint8_t *exp, int start, int end, int16_t *psd,
                               int16_t *band_psd)
{
    int bin, i, j, k, end1, v;

    /* exponent mapping to PSD */
    for(bin=start;bin<end;bin++)
        psd[bin] = psdtab[exp[bin]];

    /* PSD integration */
    j=start;
    k=bin_to_band_tab[start];
    do {
        v=psd[j];
        j++;
        end1 = MIN(band_start_tab[k+1], end);
        for(i=j;i<end1;i++) {// logadd
            int adr = MIN(ABS(v - psd[j]) >> 1, 255);
            v = MAX(v, psd[j]) + a52_log_add_tab[adr];
            j++;
        }
        band_psd[k]=v;
        k++;
    } while (end > band_start_tab[k]);
}

void a52_bit_alloc_calc_mask(A52BitAllocParams *s, int16_t *band_psd,
                                int start, int end, int fast_gain,
                                int dba_mode, int dba_nsegs, uint8_t *dba_offsets,
                                uint8_t *dba_lengths, uint8_t *dba_values,
                                int16_t *mask)
{
    int16_t excite[50]; /* excitation */
    int bin, k;
    int bndstrt, bndend, begin, end1, tmp;
    int lowcomp, fastleak, slowleak;

    /* excitation function */
    bndstrt = bin_to_band_tab[start];
    bndend = bin_to_band_tab[end-1] + 1;

    if (bndstrt == 0) {
        lowcomp = 0;
        lowcomp = calc_lowcomp1(lowcomp, band_psd[0], band_psd[1], 384);
        excite[0] = band_psd[0] - fast_gain - lowcomp;
        lowcomp = calc_lowcomp1(lowcomp, band_psd[1], band_psd[2], 384);
        excite[1] = band_psd[1] - fast_gain - lowcomp;
        begin = 7;
        for (bin = 2; bin < 7; bin++) {
            if (bin+1 < bndend)
                lowcomp = calc_lowcomp1(lowcomp, band_psd[bin], band_psd[bin+1], 384);
            fastleak = band_psd[bin] - fast_gain;
            slowleak = band_psd[bin] - s->sgain;
            excite[bin] = fastleak - lowcomp;
            if (bin+1 < bndend) {
                if (band_psd[bin] <= band_psd[bin+1]) {
                    begin = bin + 1;
                    break;
                }
            }
        }

        end1 = MIN(bndend, 22);

        for (bin = begin; bin < end1; bin++) {
            if (bin+1 < bndend)
                lowcomp = calc_lowcomp(lowcomp, band_psd[bin], band_psd[bin+1], bin);

            fastleak = MAX(fastleak - s->fdecay, band_psd[bin] - fast_gain);
            slowleak = MAX(slowleak - s->sdecay, band_psd[bin] - s->sgain);
            excite[bin] = MAX(fastleak - lowcomp, slowleak);
        }
        begin = 22;
    } else {
        /* coupling channel */
        begin = bndstrt;
        fastleak = (s->cplfleak << 8) + 768;
        slowleak = (s->cplsleak << 8) + 768;
    }

    for (bin = begin; bin < bndend; bin++) {
        fastleak = MAX(fastleak - s->fdecay, band_psd[bin] - fast_gain);
        slowleak = MAX(slowleak - s->sdecay, band_psd[bin] - s->sgain);
        excite[bin] = MAX(fastleak, slowleak);
    }

    /* compute masking curve */

    for (bin = bndstrt; bin < bndend; bin++) {
        tmp = s->dbknee - band_psd[bin];
        if (tmp > 0) {
            excite[bin] += tmp >> 2;
        }
        mask[bin] = MAX(a52_hearing_threshold_tab[bin >> s->halfratecod][s->fscod], excite[bin]);
    }

    /* delta bit allocation */
    if (dba_mode == DBA_REUSE || dba_mode == DBA_NEW) {
        int band, seg, delta;
        band = 0;
        for (seg = 0; seg < dba_nsegs; seg++) {
            band += dba_offsets[seg];
            if (dba_values[seg] >= 4) {
                delta = (dba_values[seg] - 3) << 7;
            } else {
                delta = (dba_values[seg] - 4) << 7;
            }
            for (k = 0; k < dba_lengths[seg]; k++) {
                mask[band] += delta;
                band++;
            }
        }
    }
}

/**
 * A52 bit allocation
 * Generate bit allocation pointers for each mantissa, which determines the
 * number of bits allocated for each mantissa.  The fine-grain power-spectral
 * densities and the masking curve have been pre-generated in the preparation
 * step.  They are used along with the given snroffset and floor values to
 * calculate each bap value.
 */
void a52_bit_alloc_calc_bap(int16_t *mask, int16_t *psd, int start, int end,
                               int snr_offset, int floor, uint8_t *bap)
{
    int i, j;

    // special case, if snr offset is -960, set all bap's to zero
    if(snr_offset == -960) {
        memset(bap, 0, end-start);
        return;
    }

    i = start;
    j = bin_to_band_tab[start];
    do {
        int v = (MAX(mask[j] - snr_offset - floor, 0) & 0x1FE0) + floor;
        int endj = MIN(band_start_tab[j] + a52_critical_band_size_tab[j], end);
        if ((endj-i) & 1) {
            int address = CLIP((psd[i] - v) >> 5, 0, 63);
            bap[i] = a52_bap_tab[address];
            ++i;
        }
        while (i < endj) {
            int address1 = CLIP((psd[i  ] - v) >> 5, 0, 63);
            int address2 = CLIP((psd[i+1] - v) >> 5, 0, 63);
            bap[i  ] = a52_bap_tab[address1];
            bap[i+1] = a52_bap_tab[address2];
            i += 2;
        }
    } while (end > band_start_tab[j++]);
}

/**
 * Initializes some tables.
 */
void a52_common_init(void)
{
   int i, j, k, l, v;

    // compute psdtab
    for(i=0; i<25; i++)
        psdtab[i] = 3072 - (i << 7);

    // compute bndtab and masktab from bandsz
    k = 0;
    l = 0;
    for(i=0;i<50;i++) {
        band_start_tab[i] = l;
        v = a52_critical_band_size_tab[i];
        for(j=0;j<v;j++) bin_to_band_tab[k++]=i;
        l += v;
    }
    band_start_tab[50] = l;
}