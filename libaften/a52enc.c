/**
 * Aften: A/52 audio encoder
 * Copyright (c) 2006  Justin Ruggles <jruggle@eathlink.net>
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

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "a52.h"
#include "bitalloc.h"
#include "crc.h"
#include "dsp.h"

static const uint8_t rematbndtab[4][2] = {
    {13, 24}, {25, 36}, {37, 60}, {61, 252}
};

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

static void
compute_exp_strategy(A52Context *ctx)
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
encode_exp(uint8_t *exp, int exp_strategy)
{
    int grpsize, ngrps, i, j, k, exp_min;
    uint8_t exp1[256];

    // group count based on full bandwidth (253 coefs)
    grpsize = ngrps = 0;
    switch(exp_strategy) {
        case EXP_D15: grpsize = 1; ngrps = 252;
        case EXP_D25: grpsize = 2; ngrps = 126;
        case EXP_D45: grpsize = 4; ngrps = 63;
    }

    // for each group, compute the minimum exponent
    exp1[0] = exp[0]; // DC exponent is handled separately
    k = 1;
    for(i=1; i<=ngrps; i++) {
        exp_min = exp[k];
        for(j=1; j<grpsize; j++) {
            if(exp[k+j] < exp_min)
                exp_min = exp[k+j];
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
    int blk, ch, i, gsize;
    int delta[3];
    uint8_t exp0, exp1;
    uint8_t *p;
    A52Frame *frame;
    A52Block *block;

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
            block->nexpgrps[ch] = (frame->ncoefs[ch]+(gsize*3)-4)/(3*gsize);
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
            encode_exp(blocks[i].exp[ch], blocks[i].exp_strategy[ch]);
            // copy encoded exponents for reuse case
            for(k=i+1; k<j; k++) {
                memcpy(blocks[k].exp[ch], blocks[i].exp[ch], A52_MAX_COEFS);
            }
            i = j;
        }
    }
}

void
aften_set_defaults(AftenContext *s)
{
    if(s == NULL) return;

    /**
     * These 4 must be set explicitly before initialization.
     * There are utility functions to help setting acmod and lfe.
     */
    s->channels = -1;
    s->samplerate = -1;
    s->acmod = -1;
    s->lfe = -1;

    s->private = NULL;
    s->params.verbose = 1;
    s->params.encoding_mode = AFTEN_ENC_MODE_CBR;
    s->params.bitrate = 0;
    s->params.quality = 220;
    s->params.bwcode = -1;
    s->params.use_rematrixing = 1;
    s->params.use_block_switching = 0;
    s->params.use_bw_filter = 0;
    s->params.use_dc_filter = 0;
    s->params.use_lfe_filter = 0;

    s->meta.cmixlev = 0;
    s->meta.surmixlev = 0;
    s->meta.dsurmod = 0;
    s->meta.dialnorm = 31;
    s->meta.xbsi1e = 0;
    s->meta.dmixmod = 0;
    s->meta.ltrtcmixlev = 4;
    s->meta.ltrtsmixlev = 4;
    s->meta.lorocmixlev = 4;
    s->meta.lorosmixlev = 4;
    s->meta.xbsi2e = 0;
    s->meta.dsurexmod = 0;
    s->meta.dheadphonmod = 0;
    s->meta.adconvtyp = 0;
}

int
aften_encode_init(AftenContext *s)
{
    int i, j, brate;
    A52Context ctx0;
    A52Context *ctx;

    ctx = &ctx0;

    // channel configuration
    if(s->channels < 1 || s->channels > 6) {
        return -1;
    }
    if(s->acmod < 0) {
        return -1;
    }
    if(s->channels == 6 && !s->lfe) {
        return -1;
    }
    if(s->channels == 1 && s->lfe) {
        return -1;
    }
    ctx->acmod = s->acmod;
    ctx->lfe = s->lfe;
    ctx->n_all_channels = s->channels;
    ctx->n_channels = s->channels - s->lfe;
    ctx->lfe_channel = s->lfe ? (s->channels - 1) : -1;

    ctx->params = s->params;
    ctx->meta = s->meta;

    // frequency
    for(i=0;i<3;i++) {
        for(j=0;j<3;j++)
            if((a52_freqs[j] >> i) == s->samplerate)
                goto found;
    }
    fprintf(stderr, "invalid sample rate: %d\n", s->samplerate);
    return -1;
 found:
    ctx->sample_rate = s->samplerate;
    ctx->halfratecod = i;
    ctx->fscod = j;
    ctx->bsid = 8 + ctx->halfratecod;
    ctx->bsmod = 0;

    // bitrate & frame size
    brate = s->params.bitrate;
    if(ctx->params.encoding_mode == AFTEN_ENC_MODE_CBR) {
        if(brate == 0) {
            switch(ctx->n_channels) {
                case 1: brate =  96; break;
                case 2: brate = 192; break;
                case 3: brate = 256; break;
                case 4: brate = 384; break;
                case 5: brate = 448; break;
            }
        }
    } else if(ctx->params.encoding_mode == AFTEN_ENC_MODE_VBR) {
        if(s->params.quality < 1 || s->params.quality > 1023) {
            return -1;
        }
    } else {
        return -1;
    }

    for(i=0; i<19; i++) {
        if((a52_bitratetab[i] >> ctx->halfratecod) == brate)
            break;
    }
    if(i == 19) {
        if(ctx->params.encoding_mode == AFTEN_ENC_MODE_CBR) {
            return -1;
        }
        i = 18;
    }
    ctx->frmsizecod = i*2;
    ctx->target_bitrate = a52_bitratetab[i] >> ctx->halfratecod;

    ctx->frame_cnt = 0;
    ctx->bit_cnt = 0;
    ctx->sample_cnt = 0;

    // initial snr offset
    ctx->last_csnroffst = 15;

    if(s->params.bwcode < -2 || s->params.bwcode > 60) {
        return -1;
    }
    ctx->last_quality = 220;
    if(ctx->params.encoding_mode == AFTEN_ENC_MODE_VBR) {
        ctx->last_quality = ctx->params.quality;
    } else if(ctx->params.encoding_mode == AFTEN_ENC_MODE_CBR) {
        ctx->last_quality = ((((ctx->target_bitrate/ctx->n_channels)*35)/24)+95)+(25*ctx->halfratecod);
    }
    if(ctx->params.bwcode == -2) {
        ctx->fixed_bwcode = -2;
    } else if(ctx->params.bwcode == -1) {
        int cutoff = ((ctx->last_quality-120) * 120) + 4000;
        ctx->fixed_bwcode = ((cutoff * 512 / ctx->sample_rate) - 73) / 3;
        ctx->fixed_bwcode = MIN(MAX(ctx->fixed_bwcode, 0), 60);
    } else {
        ctx->fixed_bwcode = ctx->params.bwcode;
    }

    bitalloc_init();
    crc_init();
    dsp_init();

    // can't do block switching with low sample rate due to the high-pass filter
    if(ctx->sample_rate <= 16000) {
        ctx->params.use_block_switching = 0;
    }
    // initialize transient-detect filters (one for each channel)
    // cascaded biquad direct form I high-pass w/ cutoff of 8 kHz
    if(ctx->params.use_block_switching) {
        for(i=0; i<ctx->n_all_channels; i++) {
            ctx->bs_filter[i].type = FILTER_TYPE_HIGHPASS;
            ctx->bs_filter[i].cascaded = 1;
            ctx->bs_filter[i].cutoff = 8000;
            ctx->bs_filter[i].samplerate = (double)ctx->sample_rate;
            if(filter_init(&ctx->bs_filter[i], FILTER_ID_BIQUAD_I)) {
                fprintf(stderr, "error initializing transient-detect filter\n");
                return -1;
            }
        }
    }

    // initialize DC filters (one for each channel)
    // one-pole high-pass w/ cutoff of 3 Hz
    if(ctx->params.use_dc_filter) {
        for(i=0; i<ctx->n_all_channels; i++) {
            ctx->dc_filter[i].type = FILTER_TYPE_HIGHPASS;
            ctx->dc_filter[i].cascaded = 0;
            ctx->dc_filter[i].cutoff = 3;
            ctx->dc_filter[i].samplerate = (double)ctx->sample_rate;
            if(filter_init(&ctx->dc_filter[i], FILTER_ID_ONEPOLE)) {
                fprintf(stderr, "error initializing dc filter\n");
                return -1;
            }
        }
    }

    // initialize DC filters (one for each channel)
    // one-pole high-pass w/ cutoff of 3 Hz
    if(ctx->params.use_bw_filter) {
        int cutoff;
        if(ctx->params.bwcode == -2) {
            fprintf(stderr, "cannot use bandwidth filter with variable bandwidth\n");
            return -1;
        }
        cutoff = (((ctx->fixed_bwcode * 3) + 73) * ctx->sample_rate) / 512;
        if(cutoff < 4000) {
            // disable bandwidth filter if cutoff is below 4000 Hz
            ctx->params.use_bw_filter = 0;
        } else {
            for(i=0; i<ctx->n_channels; i++) {
                ctx->bw_filter[i].type = FILTER_TYPE_LOWPASS;
                ctx->bw_filter[i].cascaded = 1;
                ctx->bw_filter[i].cutoff = cutoff;
                ctx->bw_filter[i].samplerate = (double)ctx->sample_rate;
                if(filter_init(&ctx->bw_filter[i], FILTER_ID_BUTTERWORTH_II)) {
                    fprintf(stderr, "error initializing bandwidth filter\n");
                    return -1;
                }
            }
        }
    }

    // initialize LFE filter
    // butterworth 2nd order cascaded direct form II low-pass w/ cutoff of 120 Hz
    if(ctx->params.use_lfe_filter) {
        if(!ctx->lfe) {
            fprintf(stderr, "cannot use lfe filter. no lfe channel\n");
            return -1;
        }
        ctx->lfe_filter.type = FILTER_TYPE_LOWPASS;
        ctx->lfe_filter.cascaded = 1;
        ctx->lfe_filter.cutoff = 120;
        ctx->lfe_filter.samplerate = (double)ctx->sample_rate;
        if(filter_init(&ctx->lfe_filter, FILTER_ID_BUTTERWORTH_II)) {
            fprintf(stderr, "error initializing lfe filter\n");
            return -1;
        }
    }

    ctx = malloc(sizeof(A52Context));
    memcpy(ctx, &ctx0, sizeof(A52Context));
    s->private = ctx;

    return 0;
}

static void
frame_init(A52Context *ctx)
{
    int blk, bnd, ch;
    int cutoff;
    A52Block *block;
    A52Frame *frame;

    frame = &ctx->frame;
    frame->frame_num = ctx->frame_cnt;

    for(blk=0; blk<A52_NUM_BLOCKS; blk++) {
        block = &frame->blocks[blk];
        block->block_num = blk;
        block->rematstr = 0;
        if(blk == 0) {
            block->rematstr = 1;
            for(bnd=0; bnd<4; bnd++) {
                block->rematflg[bnd] = 0;
            }
        }
        for(ch=0; ch<ctx->n_channels; ch++) {
            block->blksw[ch] = 0;
            block->dithflag[ch] = 1;
        }
    }

    if(ctx->params.encoding_mode == AFTEN_ENC_MODE_CBR) {
        frame->bit_rate = ctx->target_bitrate;
        frame->frmsizecod = ctx->frmsizecod;
        frame->frame_size_min = frame->bit_rate * 96000 / ctx->sample_rate;
        frame->frame_size = frame->frame_size_min;
    }

    if(ctx->params.bwcode == -2) {
        cutoff = ((ctx->last_quality-120) * 120) + 4000;
        frame->bwcode = ((cutoff * 512 / ctx->sample_rate) - 73) / 3;
        frame->bwcode = MIN(MAX(frame->bwcode, 0), 60);
    } else {
        frame->bwcode = ctx->fixed_bwcode;
    }
    for(ch=0; ch<ctx->n_channels; ch++) {
        frame->ncoefs[ch] = (frame->bwcode * 3) + 73;
    }
    if(ctx->lfe) {
        frame->ncoefs[ctx->lfe_channel] = 7;
    }

    // default bit allocation params
    frame->sdecaycod = 2;
    frame->fdecaycod = 1;
    frame->sgaincod = 1;
    frame->dbkneecod = 2;
    frame->floorcod = 4;
    frame->fgaincod = 4;
}

/* output the A52 frame header */
static void
output_frame_header(A52Context *ctx, uint8_t *frame_buffer)
{
    A52Frame *f = &ctx->frame;
    BitWriter *bw = &ctx->bw;
    int frmsizecod = f->frmsizecod+(f->frame_size-f->frame_size_min);

    bitwriter_init(bw, frame_buffer, A52_MAX_CODED_FRAME_SIZE);

    bitwriter_writebits(bw, 16, 0x0B77); /* frame header */
    bitwriter_writebits(bw, 16, 0); /* crc1: will be filled later */
    bitwriter_writebits(bw, 2, ctx->fscod);
    bitwriter_writebits(bw, 6, frmsizecod);
    bitwriter_writebits(bw, 5, ctx->bsid);
    bitwriter_writebits(bw, 3, ctx->bsmod);
    bitwriter_writebits(bw, 3, ctx->acmod);
    if((ctx->acmod & 0x01) && ctx->acmod != 0x01)
        bitwriter_writebits(bw, 2, ctx->meta.cmixlev);
    if(ctx->acmod & 0x04)
        bitwriter_writebits(bw, 2, ctx->meta.surmixlev);
    if(ctx->acmod == 0x02)
        bitwriter_writebits(bw, 2, ctx->meta.dsurmod);
    bitwriter_writebits(bw, 1, ctx->lfe);
    bitwriter_writebits(bw, 5, ctx->meta.dialnorm);
    bitwriter_writebits(bw, 1, 0); /* no compression control word */
    bitwriter_writebits(bw, 1, 0); /* no lang code */
    bitwriter_writebits(bw, 1, 0); /* no audio production info */
    bitwriter_writebits(bw, 1, 0); /* no copyright */
    bitwriter_writebits(bw, 1, 1); /* original bitstream */
    bitwriter_writebits(bw, 1, ctx->meta.xbsi1e);
    if(ctx->meta.xbsi1e) {
        bitwriter_writebits(bw, 2, ctx->meta.dmixmod);
        bitwriter_writebits(bw, 3, ctx->meta.ltrtcmixlev);
        bitwriter_writebits(bw, 3, ctx->meta.ltrtsmixlev);
        bitwriter_writebits(bw, 3, ctx->meta.lorocmixlev);
        bitwriter_writebits(bw, 3, ctx->meta.lorosmixlev);
    }
    bitwriter_writebits(bw, 1, ctx->meta.xbsi2e);
    if(ctx->meta.xbsi2e) {
        bitwriter_writebits(bw, 2, ctx->meta.dsurexmod);
        bitwriter_writebits(bw, 2, ctx->meta.dheadphonmod);
        bitwriter_writebits(bw, 1, ctx->meta.adconvtyp);
        bitwriter_writebits(bw, 9, 0);
    }
    bitwriter_writebits(bw, 1, 0); /* no addtional bit stream info */
}

/* symetric quantization on 'levels' levels */
static inline int
sym_quant(int c, int e, int levels)
{
    int v, sign;

    sign = (c < 0)? -1 : 1;
    v = (levels * ((sign * c) << e)) >> 24;
    v = (v + 1) >> 1;
    v = (levels >> 1) + (sign * v);
    assert(v >= 0 && v < levels);
    return v;
}

/* asymetric quantization on 2^qbits levels */
static inline int
asym_quant(int c, int e, int qbits)
{
    int lshift, m, v;

    lshift = e + (qbits-1) - 24;
    if(lshift >= 0) v = c << lshift;
    else v = c >> (-lshift);

    m = (1 << (qbits-1));
    if(v >= m) v = m - 1;
    if(v < -m) v = -m;

    return v & ((1 << qbits)-1);
}

static void
quantize_mantissas(A52Context *ctx)
{
    int blk, ch, i, b, c, e, v;
    int mant1_cnt, mant2_cnt, mant4_cnt;
    uint16_t *qmant1_ptr, *qmant2_ptr, *qmant4_ptr;
    A52Frame *frame;
    A52Block *block;

    frame = &ctx->frame;
    for(blk=0; blk<A52_NUM_BLOCKS; blk++) {
        block = &frame->blocks[blk];
        mant1_cnt = mant2_cnt = mant4_cnt = 0;
        qmant1_ptr = qmant2_ptr = qmant4_ptr = NULL;
        for(ch=0; ch<ctx->n_all_channels; ch++) {
            for(i=0; i<frame->ncoefs[ch]; i++) {
                c = block->mdct_coef[ch][i] * (1 << 24);
                e = block->exp[ch][i];
                b = block->bap[ch][i];
                switch(b) {
                    case 0:
                        v = 0;
                        break;
                    case 1:
                        v = sym_quant(c, e, 3);
                        if(mant1_cnt == 0) {
                            qmant1_ptr = &block->qmant[ch][i];
                            v = 9 * v;
                        } else if(mant1_cnt == 1) {
                            *qmant1_ptr += 3 * v;
                            v = 128;
                        } else {
                            *qmant1_ptr += v;
                            v = 128;
                        }
                        mant1_cnt = (mant1_cnt + 1) % 3;
                        break;
                    case 2:
                        v = sym_quant(c, e, 5);
                        if(mant2_cnt == 0) {
                            qmant2_ptr = &block->qmant[ch][i];
                            v = 25 * v;
                        } else if(mant2_cnt == 1) {
                            *qmant2_ptr += 5 * v;
                            v = 128;
                        } else {
                            *qmant2_ptr += v;
                            v = 128;
                        }
                        mant2_cnt = (mant2_cnt + 1) % 3;
                        break;
                    case 3:
                        v = sym_quant(c, e, 7);
                        break;
                    case 4:
                        v = sym_quant(c, e, 11);
                        if(mant4_cnt == 0) {
                            qmant4_ptr = &block->qmant[ch][i];
                            v = 11 * v;
                        } else {
                            *qmant4_ptr += v;
                            v = 128;
                        }
                        mant4_cnt = (mant4_cnt + 1) % 2;
                        break;
                    case 5:
                        v = sym_quant(c, e, 15);
                        break;
                    case 14:
                        v = asym_quant(c, e, 14);
                        break;
                    case 15:
                        v = asym_quant(c, e, 16);
                        break;
                    default:
                        v = asym_quant(c, e, b - 1);
                }
                block->qmant[ch][i] = v;
            }
        }
    }
}

/* Output each audio block. */
static void
output_audio_blocks(A52Context *ctx)
{
    int blk, ch, i, baie, rbnd;
    A52Frame *frame;
    A52Block *block;
    BitWriter *bw;

    frame = &ctx->frame;
    bw = &ctx->bw;
    for(blk=0; blk<A52_NUM_BLOCKS; blk++) {
        block = &frame->blocks[blk];
        for(ch=0; ch<ctx->n_channels; ch++) {
            bitwriter_writebits(bw, 1, block->blksw[ch]);
        }
        for(ch=0; ch<ctx->n_channels; ch++) {
            bitwriter_writebits(bw, 1, block->dithflag[ch]);
        }
        bitwriter_writebits(bw, 1, 0); // no dynamic range
        if(block->block_num == 0) {
            // must define coupling strategy in block 0
            bitwriter_writebits(bw, 1, 1); // new coupling strategy
            bitwriter_writebits(bw, 1, 0); // no coupling in use
        } else {
            bitwriter_writebits(bw, 1, 0); // no new coupling strategy
        }

        if(ctx->acmod == 2) {
            bitwriter_writebits(bw, 1, block->rematstr);
            if(block->rematstr) {
                for(rbnd=0; rbnd<4; rbnd++) {
                    bitwriter_writebits(bw, 1, block->rematflg[rbnd]);
                }
            }
        }

        // exponent strategy
        for(ch=0; ch<ctx->n_channels; ch++) {
            bitwriter_writebits(bw, 2, block->exp_strategy[ch]);
        }

        if(ctx->lfe) {
            bitwriter_writebits(bw, 1, block->exp_strategy[ctx->lfe_channel]);
        }

        for(ch=0; ch<ctx->n_channels; ch++) {
            if(block->exp_strategy[ch] != EXP_REUSE)
                bitwriter_writebits(bw, 6, frame->bwcode);
        }

        // exponents
        for(ch=0; ch<ctx->n_all_channels; ch++) {
            if(block->exp_strategy[ch] != EXP_REUSE) {
                // first exponent
                bitwriter_writebits(bw, 4, block->grp_exp[ch][0]);

                // delta-encoded exponent groups
                for(i=1; i<=block->nexpgrps[ch]; i++) {
                    bitwriter_writebits(bw, 7, block->grp_exp[ch][i]);
                }

                // gain range info
                if(ch != ctx->lfe_channel) {
                    bitwriter_writebits(bw, 2, 0);
                }
            }
        }

        // bit allocation info
        baie = (block->block_num == 0);
        bitwriter_writebits(bw, 1, baie);
        if(baie) {
            bitwriter_writebits(bw, 2, frame->sdecaycod);
            bitwriter_writebits(bw, 2, frame->fdecaycod);
            bitwriter_writebits(bw, 2, frame->sgaincod);
            bitwriter_writebits(bw, 2, frame->dbkneecod);
            bitwriter_writebits(bw, 3, frame->floorcod);
        }

        // snr offset
        bitwriter_writebits(bw, 1, baie);
        if(baie) {
            bitwriter_writebits(bw, 6, frame->csnroffst);
            for(ch=0; ch<ctx->n_all_channels; ch++) {
                bitwriter_writebits(bw, 4, frame->fsnroffst);
                bitwriter_writebits(bw, 3, frame->fgaincod);
            }
        }

        bitwriter_writebits(bw, 1, 0); // no delta bit allocation
        bitwriter_writebits(bw, 1, 0); // no data to skip

        // mantissas
        for(ch=0; ch<ctx->n_all_channels; ch++) {
            int b, q;
            for(i=0; i<frame->ncoefs[ch]; i++) {
                q = block->qmant[ch][i];
                b = block->bap[ch][i];
                switch(b) {
                    case 0:  break;
                    case 1:  if(q != 128) bitwriter_writebits(bw, 5, q);
                             break;
                    case 2:  if(q != 128) bitwriter_writebits(bw, 7, q);
                             break;
                    case 3:  bitwriter_writebits(bw, 3, q);
                             break;
                    case 4:  if(q != 128) bitwriter_writebits(bw, 7, q);
                             break;
                    case 14: bitwriter_writebits(bw, 14, q);
                             break;
                    case 15: bitwriter_writebits(bw, 16, q);
                             break;
                    default: bitwriter_writebits(bw, b - 1, q);
                }
            }
        }
    }
}

static int
output_frame_end(A52Context *ctx)
{
    int fs, fs58, n, crc1, crc2, bitcount;
    uint8_t *frame;

    fs = ctx->frame.frame_size;
    // align to 8 bits
    bitwriter_flushbits(&ctx->bw);
    // add zero bytes to reach the frame size
    frame = ctx->bw.buffer;
    bitcount = bitwriter_bitcount(&ctx->bw);
    n = 2 * fs - (bitcount>>3) - 2;
    if(n < 0) {
        fprintf(stderr, "fs=%d data=%d\n", 2*fs-2, bitcount>>3);
        assert(n >= 0);
    }
    if(n > 0) memset(&ctx->bw.buffer[bitcount>>3], 0, n);

    // compute crc1 for 1st 5/8 of frame
    fs58 = (fs >> 1) + (fs >> 3);
    crc1 = calc_crc16(&frame[4], 2*fs58-4);
    crc1 = crc16_zero(crc1, 2*fs58-2);
    frame[2] = crc1 >> 8;
    frame[3] = crc1;
    // double-check
    crc1 = calc_crc16(&frame[2], 2*fs58-2);
    if(crc1 != 0) fprintf(stderr, "CRC ERROR\n");

    // compute crc2 for final 3/8 of frame
    crc2 = calc_crc16(&frame[2*fs58], (fs - fs58) * 2 - 2);
    frame[2*fs-2] = crc2 >> 8;
    frame[2*fs-1] = crc2;

    return fs * 2;
}

static void
copy_samples(A52Context *ctx, double *samples)
{
    int ch, blk, j;
    int sinc;
    double filtered_audio[A52_FRAME_SIZE];
    A52Frame *frame;
    A52Block *block;

    sinc = ctx->n_all_channels;
    frame = &ctx->frame;

    for(ch=0; ch<sinc; ch++) {
        for(j=0; j<A52_FRAME_SIZE; j++) {
            frame->input_audio[ch][j] = samples[j*sinc+ch];
        }
    }

    // DC-removal high-pass filter
    if(ctx->params.use_dc_filter) {
        for(ch=0; ch<sinc; ch++) {
            filter_run(&ctx->dc_filter[ch], filtered_audio,
                       frame->input_audio[ch], A52_FRAME_SIZE);
            memcpy(frame->input_audio[ch], filtered_audio,
                   A52_FRAME_SIZE * sizeof(double));
        }
    }

    // channel bandwidth filter will go here
    if(ctx->params.use_bw_filter) {
        for(ch=0; ch<ctx->n_channels; ch++) {
            filter_run(&ctx->bw_filter[ch], filtered_audio,
                       frame->input_audio[ch], A52_FRAME_SIZE);
            memcpy(frame->input_audio[ch], filtered_audio,
                   A52_FRAME_SIZE * sizeof(double));
        }
    }

    // block-switching high-pass filter
    if(ctx->params.use_block_switching) {
        for(ch=0; ch<ctx->n_channels; ch++) {
            filter_run(&ctx->bs_filter[ch], filtered_audio,
                       frame->input_audio[ch], A52_FRAME_SIZE);
            for(blk=0; blk<A52_NUM_BLOCKS; blk++) {
                block = &frame->blocks[blk];
                memcpy(block->transient_samples[ch],
                       ctx->last_transient_samples[ch], 256 * sizeof(double));
                memcpy(&block->transient_samples[ch][256],
                       &filtered_audio[256*blk], 256 * sizeof(double));
                memcpy(ctx->last_transient_samples[ch],
                       &filtered_audio[256*blk], 256 * sizeof(double));
            }
        }
    }

    // LFE bandwidth low-pass filter
    if(ctx->params.use_lfe_filter) {
        ch = ctx->lfe_channel;
        filter_run(&ctx->lfe_filter, filtered_audio,
                   frame->input_audio[ch], A52_FRAME_SIZE);
        memcpy(frame->input_audio[ch], filtered_audio,
               A52_FRAME_SIZE * sizeof(double));
    }

    for(ch=0; ch<sinc; ch++) {
        for(blk=0; blk<A52_NUM_BLOCKS; blk++) {
            block = &frame->blocks[blk];
            memcpy(block->input_samples[ch], ctx->last_samples[ch],
                   256 * sizeof(double));
            memcpy(&block->input_samples[ch][256],
                   &frame->input_audio[ch][256*blk], 256 * sizeof(double));
            memcpy(ctx->last_samples[ch],
                   &frame->input_audio[ch][256*blk], 256 * sizeof(double));
        }
    }
}

/* determines block length by detecting transients */
static int
detect_transient(double *in)
{
    int i, j;
    double level1[2];
    double level2[4];
    double level3[8];
    double v;
    double *xx = in;

    // level 1 (2 x 256)
    for(i=0; i<2; i++) {
        level1[i] = 0;
        for(j=0; j<256; j++) {
            v = fabs(xx[i*256+j]);
            if(v > level1[i]) level1[i] = v;
        }
        if(level1[i] < (100.0 / 32768.0)) {
            return 0;
        }
        if((i > 0) && (level1[i] * 0.100 > level1[i-1])) {
            return 1;
        }
    }

    // level 2 (4 x 128)
    for(i=1; i<4; i++) {
        level2[i] = 0;
        for(j=0; j<128; j++) {
            v = fabs(xx[i*128+j]);
            if(v > level2[i]) level2[i] = v;
        }
        if((i > 1) && (level2[i] * 0.075 > level2[i-1])) {
            return 1;
        }
    }

    // level 3 (8 x 64)
    for(i=3; i<8; i++) {
        level3[i] = 0;
        for(j=0; j<64; j++) {
            v = fabs(xx[i*64+j]);
            if(v > level3[i]) level3[i] = v;
        }
        if((i > 3) && (level3[i] * 0.050 > level3[i-1])) {
            return 1;
        }
    }

    return 0;
}

static void
generate_coefs(A52Context *ctx)
{
    int blk, ch, i;
    A52Block *block;

    for(ch=0; ch<ctx->n_all_channels; ch++) {
        for(blk=0; blk<A52_NUM_BLOCKS; blk++) {
            block = &ctx->frame.blocks[blk];
            if(ctx->params.use_block_switching) {
                block->blksw[ch] = detect_transient(block->transient_samples[ch]);
            } else {
                block->blksw[ch] = 0;
            }
            //if(block->blksw[ch]) printf("\nshort block\n");
            apply_a52_window(block->input_samples[ch]);
            if(block->blksw[ch]) {
                mdct256(block->mdct_coef[ch], block->input_samples[ch]);
            } else {
                mdct512(block->mdct_coef[ch], block->input_samples[ch]);
            }
            for(i=ctx->frame.ncoefs[ch]; i<256; i++) {
                block->mdct_coef[ch][i] = 0.0;
            }
        }
    }
}

static void
calc_rematrixing(A52Context *ctx)
{
    double sum[4][4];
    double lt, rt, ctmp1, ctmp2;
    int blk, bnd, i, chmin;
    A52Block *block;

    if(ctx->acmod != 2) return;

    if(!ctx->params.use_rematrixing) {
        ctx->frame.blocks[0].rematstr = 1;
        for(bnd=0; bnd<4; bnd++) {
            ctx->frame.blocks[0].rematflg[bnd] = 0;
        }
        for(blk=1; blk<A52_NUM_BLOCKS; blk++) {
            ctx->frame.blocks[blk].rematstr = 0;
        }
        return;
    }

    for(blk=0; blk<A52_NUM_BLOCKS; blk++) {
        block = &ctx->frame.blocks[blk];

        block->rematstr = 0;
        if(blk == 0) block->rematstr = 1;

        for(bnd=0; bnd<4; bnd++) {
            sum[bnd][0] = sum[bnd][1] = sum[bnd][2] = sum[bnd][3] = 0;
            for(i=rematbndtab[bnd][0]; i<=rematbndtab[bnd][1]; i++) {
                lt = block->mdct_coef[0][i];
                rt = block->mdct_coef[1][i];
                sum[bnd][0] += lt * lt;
                sum[bnd][1] += rt * rt;
                sum[bnd][2] += (lt + rt) * (lt + rt);
                sum[bnd][3] += (lt - rt) * (lt - rt);
            }
            chmin = 0;
            if(sum[bnd][1] < sum[bnd][chmin])
                chmin = 1;
            if(sum[bnd][2] < sum[bnd][chmin])
                chmin = 2;
            if(sum[bnd][3] < sum[bnd][chmin])
                chmin = 3;
            if(chmin < 2) {
                block->rematflg[bnd] = 0;
            } else {
                block->rematflg[bnd] = 1;
                for(i=rematbndtab[bnd][0]; i<=rematbndtab[bnd][1]; i++) {
                    ctmp1 = block->mdct_coef[0][i] * 0.5;
                    ctmp2 = block->mdct_coef[1][i] * 0.5;
                    block->mdct_coef[0][i] = ctmp1 + ctmp2;
                    block->mdct_coef[1][i] = ctmp1 - ctmp2;
                }
            }
            if(blk != 0 && block->rematstr == 0 &&
               block->rematflg[bnd] != ctx->frame.blocks[blk-1].rematflg[bnd]) {
                block->rematstr = 1;
            }
        }
    }
}

static void
extract_exps(A52Context *ctx)
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

/** Adjust for fractional frame sizes in CBR mode */
static void
adjust_frame_size(A52Context *ctx)
{
    A52Frame *f = &ctx->frame;
    int kbps = f->bit_rate * 1000;
    int srate = ctx->sample_rate;
    int add;

    if(ctx->params.encoding_mode != AFTEN_ENC_MODE_CBR)
        return;

    while(ctx->bit_cnt >= kbps && ctx->sample_cnt >= srate) {
        ctx->bit_cnt -= kbps;
        ctx->sample_cnt -= srate;
    }
    add = !!(ctx->bit_cnt * srate < ctx->sample_cnt * kbps);
    f->frame_size = f->frame_size_min + add;
}

static void
compute_dither_strategy(A52Context *ctx)
{
    int blk, ch;
    A52Block *block0;
    A52Block *block1;

    block0 = NULL;
    for(blk=0; blk<A52_NUM_BLOCKS; blk++) {
        block1 = &ctx->frame.blocks[blk];
        for(ch=0; ch<ctx->n_channels; ch++) {
            if(block1->blksw[ch] || ((blk>0) && block0->blksw[ch])) {
                block1->dithflag[ch] = 0;
            } else {
                block1->dithflag[ch] = 1;
            }
        }
        block0 = block1;
    }
}

int
aften_encode_frame(AftenContext *s, uint8_t *frame_buffer, double *samples)
{
    A52Context *ctx;
    A52Frame *frame;

    ctx = s->private;
    frame = &ctx->frame;

    frame_init(ctx);

    copy_samples(ctx, samples);

    generate_coefs(ctx);

    compute_dither_strategy(ctx);

    calc_rematrixing(ctx);

    extract_exps(ctx);

    compute_exp_strategy(ctx);

    encode_exponents(ctx);

    adjust_frame_size(ctx);

    if(compute_bit_allocation(ctx)) {
        fprintf(stderr, "Error in bit allocation\n");
        return 0;
    }

    group_exponents(ctx);
    quantize_mantissas(ctx);

    // increment counters
    ctx->bit_cnt += frame->frame_size * 16;
    ctx->sample_cnt += A52_FRAME_SIZE;
    ctx->frame_cnt++;

    // update encoding status
    s->status.frame_num = frame->frame_num;
    s->status.quality = frame->quality;
    s->status.bit_rate = frame->bit_rate;
    s->status.bwcode = frame->bwcode;

    output_frame_header(ctx, frame_buffer);
    output_audio_blocks(ctx);
    return output_frame_end(ctx);
}

void
aften_encode_close(AftenContext *s)
{
    if(s != NULL && s->private != NULL) {
        free(s->private);
        s->private = NULL;
    }
    return;
}
