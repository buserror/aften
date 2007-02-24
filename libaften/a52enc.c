/**
 * Aften: A/52 audio encoder
 * Copyright (c) 2006 Justin Ruggles <justinruggles@bellsouth.net>
 *               2007 Prakash Punnoor <prakash@punnoor.de>
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
 * @file a52enc.c
 * A/52 encoder
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "a52.h"
#include "bitalloc.h"
#include "crc.h"
#include "mdct.h"
#include "window.h"
#include "exponent.h"
#include "dynrng.h"
#include "cpu_caps.h"

static const uint8_t rematbndtab[4][2] = {
    {13, 24}, {25, 36}, {37, 60}, {61, 252}
};

/* possible frequencies */
const uint16_t a52_freqs[3] = { 48000, 44100, 32000 };

/* possible bitrates */
const uint16_t a52_bitratetab[19] = {
    32, 40, 48, 56, 64, 80, 96, 112, 128,
    160, 192, 224, 256, 320, 384, 448, 512, 576, 640
};

static int threaded_encode(void* vtctx);

const char *
aften_get_version(void)
{
#ifdef SVN_VERSION
    static const char *const str = AFTEN_VERSION "-r" SVN_VERSION;
#else
    static const char *const str = AFTEN_VERSION;
#endif

    return str;
}

void
aften_set_defaults(AftenContext *s)
{
    if(s == NULL) {
        fprintf(stderr, "NULL parameter passed to aften_set_defaults\n");
        return;
    }

    /**
     * These 4 must be set explicitly before initialization.
     * There are utility functions to help setting acmod and lfe.
     */
    s->channels = -1;
    s->samplerate = -1;
    s->sample_format = A52_SAMPLE_FMT_S16;
    s->acmod = -1;
    s->lfe = -1;

    s->private_context = NULL;
    s->params.verbose = 1;
    s->params.encoding_mode = AFTEN_ENC_MODE_CBR;
    s->params.bitrate = 0;
    s->params.quality = 240;
    s->params.bwcode = -1;
    s->params.use_rematrixing = 1;
    s->params.use_block_switching = 0;
    s->params.use_bw_filter = 0;
    s->params.use_dc_filter = 0;
    s->params.use_lfe_filter = 0;
    s->params.bitalloc_fast = 0;
    s->params.expstr_fast = 0;
    s->params.dynrng_profile = DYNRNG_PROFILE_NONE;

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

    s->status.quality = 0;
    s->status.bit_rate = 0;
    s->status.bwcode = 0;
}

static void
fmt_convert_from_u8(FLOAT dest[A52_MAX_CHANNELS][A52_SAMPLES_PER_FRAME],
                    const void *vsrc, int nch, int n)
{
    int i, j, ch;
    const uint8_t *src = vsrc;

    for(ch=0; ch<nch; ch++) {
        FLOAT *dest_ch = dest[ch];
        const uint8_t *src_ch = src + ch;
        for(i=0, j=0; i<n; i++, j+=nch) {
            dest_ch[i] = (src_ch[j]-FCONST(128.0)) / FCONST(128.0);
        }
    }
}

static void
fmt_convert_from_s16(FLOAT dest[A52_MAX_CHANNELS][A52_SAMPLES_PER_FRAME],
                     const void *vsrc, int nch, int n)
{
    int i, j, ch;
    const int16_t *src = vsrc;

    for(ch=0; ch<nch; ch++) {
        FLOAT *dest_ch = dest[ch];
        const int16_t *src_ch = src + ch;
        for(i=0, j=0; i<n; i++, j+=nch) {
            dest_ch[i] = src_ch[j] / FCONST(32768.0);
        }
    }
}

static void
fmt_convert_from_s20(FLOAT dest[A52_MAX_CHANNELS][A52_SAMPLES_PER_FRAME],
                     const void *vsrc, int nch, int n)
{
    int i, j, ch;
    const int32_t *src = vsrc;

    for(ch=0; ch<nch; ch++) {
        FLOAT *dest_ch = dest[ch];
        const int32_t *src_ch = src + ch;
        for(i=0, j=0; i<n; i++, j+=nch) {
            dest_ch[i] = src_ch[j] / FCONST(524288.0);
        }
    }
}

static void
fmt_convert_from_s24(FLOAT dest[A52_MAX_CHANNELS][A52_SAMPLES_PER_FRAME],
                     const void *vsrc, int nch, int n)
{
    int i, j, ch;
    const int32_t *src = vsrc;

    for(ch=0; ch<nch; ch++) {
        FLOAT *dest_ch = dest[ch];
        const int32_t *src_ch = src + ch;
        for(i=0, j=0; i<n; i++, j+=nch) {
            dest_ch[i] = src_ch[j] / FCONST(8388608.0);
        }
    }
}

static void
fmt_convert_from_s32(FLOAT dest[A52_MAX_CHANNELS][A52_SAMPLES_PER_FRAME],
                     const void *vsrc, int nch, int n)
{
    int i, j, ch;
    const int32_t *src = vsrc;

    for(ch=0; ch<nch; ch++) {
        FLOAT *dest_ch = dest[ch];
        const int32_t *src_ch = src + ch;
        for(i=0, j=0; i<n; i++, j+=nch) {
            dest_ch[i] = src_ch[j] / FCONST(2147483648.0);
        }
    }
}

static void
fmt_convert_from_float(FLOAT dest[A52_MAX_CHANNELS][A52_SAMPLES_PER_FRAME],
                       const void *vsrc, int nch, int n)
{
    int i, j, ch;
    const float *src = vsrc;

    for(ch=0; ch<nch; ch++) {
        FLOAT *dest_ch = dest[ch];
        const float *src_ch = src + ch;
        for(i=0, j=0; i<n; i++, j+=nch) {
            dest_ch[i] = src_ch[j];
        }
    }
}

static void
fmt_convert_from_double(FLOAT dest[A52_MAX_CHANNELS][A52_SAMPLES_PER_FRAME],
                        const void *vsrc, int nch, int n)
{
    int i, j, ch;
    const double *src = vsrc;

    for(ch=0; ch<nch; ch++) {
        FLOAT *dest_ch = dest[ch];
        const double *src_ch = src + ch;
        for(i=0, j=0; i<n; i++, j+=nch) {
            dest_ch[i] = (FLOAT)src_ch[j];
        }
    }
}

static void
select_mdct(A52Context *ctx)
{
#ifdef HAVE_SSE3
    if (_alHaveSSE3()) {
        sse3_mdct_init(ctx);
        return;
    }
#endif
#ifdef HAVE_SSE
    if (_alHaveSSE()) {
        sse_mdct_init(ctx);
        return;
    }
#endif
    mdct_init(ctx);
}

static void
select_mdct_thread(A52ThreadContext *tctx)
{
#ifdef HAVE_SSE3
    if (_alHaveSSE3()) {
        sse3_mdct_thread_init(tctx);
        return;
    }
#endif
#ifdef HAVE_SSE
    if (_alHaveSSE()) {
        sse_mdct_thread_init(tctx);
        return;
    }
#endif
    mdct_thread_init(tctx);
}

int
aften_encode_init(AftenContext *s)
{
    A52Context *ctx;
    A52ThreadContext *tctx;
    int i, j, brate;
    int last_quality;

    if(s == NULL) {
        fprintf(stderr, "NULL parameter passed to aften_encode_init\n");
        return -1;
    }
    _alDetectCPUCaps();

    ctx = calloc(sizeof(A52Context), 1);
    if(!ctx) {
        fprintf(stderr, "error allocating memory for A52Context\n");
        return -1;
    }
    select_mdct(ctx);
    s->private_context = ctx;

    switch(s->sample_format) {
        case A52_SAMPLE_FMT_U8:  ctx->fmt_convert_from_src = fmt_convert_from_u8;
                                 break;
        case A52_SAMPLE_FMT_S16: ctx->fmt_convert_from_src = fmt_convert_from_s16;
                                 break;
        case A52_SAMPLE_FMT_S20: ctx->fmt_convert_from_src = fmt_convert_from_s20;
                                 break;
        case A52_SAMPLE_FMT_S24: ctx->fmt_convert_from_src = fmt_convert_from_s24;
                                 break;
        case A52_SAMPLE_FMT_S32: ctx->fmt_convert_from_src = fmt_convert_from_s32;
                                 break;
        case A52_SAMPLE_FMT_FLT: ctx->fmt_convert_from_src = fmt_convert_from_float;
                                 break;
        case A52_SAMPLE_FMT_DBL: ctx->fmt_convert_from_src = fmt_convert_from_double;
                                 break;
        default: break;
    }

    // channel configuration
    if(s->channels < 1 || s->channels > 6) {
        fprintf(stderr, "invalid number of channels\n");
        return -1;
    }
    if(s->acmod < 0 || s->acmod > 7) {
        fprintf(stderr, "invalid acmod\n");
        return -1;
    }
    if(s->channels == 6 && !s->lfe) {
        fprintf(stderr, "6-channel audio must have LFE channel\n");
        return -1;
    }
    if(s->channels == 1 && s->lfe) {
        fprintf(stderr, "cannot encode stand-alone LFE channel\n");
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
    fprintf(stderr, "invalid sample rate\n");
    return -1;
 found:
    ctx->sample_rate = s->samplerate;
    ctx->halfratecod = i;
    ctx->fscod = j;
    if(ctx->halfratecod) {
        // DolbyNet
        ctx->bsid = 8 + ctx->halfratecod;
    } else if(ctx->meta.xbsi1e || ctx->meta.xbsi2e) {
        // alternate bit stream syntax
        ctx->bsid = 6;
    } else {
        // normal AC-3
        ctx->bsid = 8;
    }
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
        if(s->params.quality < 0 || s->params.quality > 1023) {
            fprintf(stderr, "invalid quality setting\n");
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
            fprintf(stderr, "invalid bitrate\n");
            return -1;
        }
        i = 18;
    }
    ctx->frmsizecod = i*2;
    ctx->target_bitrate = a52_bitratetab[i] >> ctx->halfratecod;

    bitalloc_init();
    crc_init();
    a52_window_init();
    exponent_init();
    dynrng_init();

    // can't do block switching with low sample rate due to the high-pass filter
    if(ctx->sample_rate <= 16000) {
        ctx->params.use_block_switching = 0;
    }

    last_quality = 240;
    if(ctx->params.encoding_mode == AFTEN_ENC_MODE_VBR) {
        last_quality = ctx->params.quality;
    } else if(ctx->params.encoding_mode == AFTEN_ENC_MODE_CBR) {
        last_quality = ((((ctx->target_bitrate/ctx->n_channels)*35)/24)+95)+(25*ctx->halfratecod);
    }

    // Initialize thread specific contexts
    ctx->n_threads = get_ncpus();
    tctx = calloc(sizeof(A52ThreadContext), ctx->n_threads);
    ctx->tctx = tctx;

    for (j=0; j<ctx->n_threads; ++j) {
        A52ThreadContext *cur_tctx = &ctx->tctx[j];
        cur_tctx->ctx = ctx;

        select_mdct_thread(cur_tctx);

        cur_tctx->bit_cnt = 0;
        cur_tctx->sample_cnt = 0;

        cur_tctx->last_quality = last_quality;

        if (ctx->n_threads > 1) {
            cur_tctx->state = START;

            posix_cond_init(&cur_tctx->ts.enter_cond);
            posix_cond_init(&cur_tctx->ts.confirm_cond);

            posix_mutex_init(&cur_tctx->ts.enter_mutex);
            posix_mutex_init(&cur_tctx->ts.confirm_mutex);

            windows_event_init(&cur_tctx->ts.ready_event);
            windows_event_init(&cur_tctx->ts.enter_event);

            posix_mutex_lock(&cur_tctx->ts.enter_mutex);
            thread_create(&cur_tctx->ts.thread, threaded_encode, cur_tctx);
            posix_cond_wait(&cur_tctx->ts.enter_cond, &cur_tctx->ts.enter_mutex);
            posix_mutex_unlock(&cur_tctx->ts.enter_mutex);
        }
    }

    if(s->params.bwcode < -2 || s->params.bwcode > 60) {
        fprintf(stderr, "invalid bandwidth code\n");
        return -1;
    }
    if(ctx->params.bwcode < 0) {
        int cutoff = ((last_quality-120) * 120) + 4000;
        ctx->fixed_bwcode = ((cutoff * 512 / ctx->sample_rate) - 73) / 3;
        ctx->fixed_bwcode = CLIP(ctx->fixed_bwcode, 0, 60);
    } else {
        ctx->fixed_bwcode = ctx->params.bwcode;
    }

    // initialize transient-detect filters (one for each channel)
    // cascaded biquad direct form I high-pass w/ cutoff of 8 kHz
    if(ctx->params.use_block_switching) {
        for(i=0; i<ctx->n_all_channels; i++) {
            ctx->bs_filter[i].type = FILTER_TYPE_HIGHPASS;
            ctx->bs_filter[i].cascaded = 1;
            ctx->bs_filter[i].cutoff = 8000;
            ctx->bs_filter[i].samplerate = (FLOAT)ctx->sample_rate;
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
            ctx->dc_filter[i].samplerate = (FLOAT)ctx->sample_rate;
            if(filter_init(&ctx->dc_filter[i], FILTER_ID_ONEPOLE)) {
                fprintf(stderr, "error initializing dc filter\n");
                return -1;
            }
        }
    }

    // initialize bandwidth filters (one for each channel)
    // butterworth 2nd order cascaded direct form II low-pass
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
                ctx->bw_filter[i].cutoff = (FLOAT)cutoff;
                ctx->bw_filter[i].samplerate = (FLOAT)ctx->sample_rate;
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
        ctx->lfe_filter.samplerate = (FLOAT)ctx->sample_rate;
        if(filter_init(&ctx->lfe_filter, FILTER_ID_BUTTERWORTH_II)) {
            fprintf(stderr, "error initializing lfe filter\n");
            return -1;
        }
    }

    return 0;
}

static int
frame_init(A52ThreadContext *tctx)
{
    A52Context *ctx = tctx->ctx;
    A52Frame *frame = &tctx->frame;
    A52Block *block;
    int blk, bnd, ch;
    int cutoff;

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

            // input_samples will be null if context is not initialized
            if(block->input_samples[ch] == NULL) {
                return -1;
            }
        }
    }

    if(ctx->params.encoding_mode == AFTEN_ENC_MODE_CBR) {
        frame->bit_rate = ctx->target_bitrate;
        frame->frmsizecod = ctx->frmsizecod;
        frame->frame_size_min = frame->bit_rate * 96000 / ctx->sample_rate;
        frame->frame_size = frame->frame_size_min;
    }

    if(ctx->params.bwcode == -2) {
        if(ctx->params.encoding_mode == AFTEN_ENC_MODE_VBR) {
            cutoff = ((tctx->last_quality-120) * 120) + 4000;
            frame->bwcode = ((cutoff * 512 / ctx->sample_rate) - 73) / 3;
            frame->bwcode = CLIP(frame->bwcode, 0, 60);
        } else if(ctx->params.encoding_mode == AFTEN_ENC_MODE_CBR) {
            frame->bwcode = ctx->fixed_bwcode;
            if(tctx->last_quality < 240) {
                frame->bwcode -= ((240-tctx->last_quality)/2);
            } else if(tctx->last_quality > 245) {
                frame->bwcode += ((tctx->last_quality-240)/5);
            }
            frame->bwcode = CLIP(frame->bwcode, 0, 60);
            ctx->fixed_bwcode = frame->bwcode;
        }
    } else {
        frame->bwcode = ctx->fixed_bwcode;
    }
    for(ch=0; ch<ctx->n_channels; ch++) {
        frame->ncoefs[ch] = (frame->bwcode * 3) + 73;
    }
    if(ctx->lfe) {
        frame->ncoefs[ctx->lfe_channel] = 7;
    }

    frame->frame_bits = 0;
    frame->exp_bits = 0;
    frame->mant_bits = 0;

    // default bit allocation params
    frame->sdecaycod = 2;
    frame->fdecaycod = 1;
    frame->sgaincod = 1;
    frame->dbkneecod = 2;
    frame->floorcod = 7;
    frame->fgaincod = 3;

    return 0;
}

/* output the A52 frame header */
static void
output_frame_header(A52ThreadContext *tctx, uint8_t *frame_buffer)
{
    A52Context *ctx = tctx->ctx;
    A52Frame *f = &tctx->frame;
    BitWriter *bw = &tctx->bw;
    int frmsizecod = f->frmsizecod+(f->frame_size-f->frame_size_min);

    bitwriter_init(bw, frame_buffer, A52_MAX_CODED_FRAME_SIZE);

    bitwriter_writebits(bw, 16, 0x0B77); /* frame header */
    bitwriter_writebits(bw, 16, 0); /* crc1: will be filled later */
    bitwriter_writebits(bw, 2, ctx->fscod);
    bitwriter_writebits(bw, 6, frmsizecod);
    bitwriter_writebits(bw, 5, ctx->bsid);
    bitwriter_writebits(bw, 3, ctx->bsmod);
    bitwriter_writebits(bw, 3, ctx->acmod);
    if((ctx->acmod & 0x01) && (ctx->acmod != A52_ACMOD_MONO))
        bitwriter_writebits(bw, 2, ctx->meta.cmixlev);
    if(ctx->acmod & 0x04)
        bitwriter_writebits(bw, 2, ctx->meta.surmixlev);
    if(ctx->acmod == A52_ACMOD_STEREO)
        bitwriter_writebits(bw, 2, ctx->meta.dsurmod);
    bitwriter_writebits(bw, 1, ctx->lfe);
    bitwriter_writebits(bw, 5, ctx->meta.dialnorm);
    bitwriter_writebits(bw, 1, 0); /* no compression control word */
    bitwriter_writebits(bw, 1, 0); /* no lang code */
    bitwriter_writebits(bw, 1, 0); /* no audio production info */
    if(ctx->acmod == A52_ACMOD_DUAL_MONO) {
        bitwriter_writebits(bw, 5, ctx->meta.dialnorm);
        bitwriter_writebits(bw, 1, 0); /* no compression control word 2 */
        bitwriter_writebits(bw, 1, 0); /* no lang code 2 */
        bitwriter_writebits(bw, 1, 0); /* no audio production info 2 */
    }
    bitwriter_writebits(bw, 1, 0); /* no copyright */
    bitwriter_writebits(bw, 1, 1); /* original bitstream */
    if(ctx->bsid == 6) {
        // alternate bit stream syntax
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
    } else {
        bitwriter_writebits(bw, 1, 0); // timecod1e
        bitwriter_writebits(bw, 1, 0); // timecod2e
    }
    bitwriter_writebits(bw, 1, 0); /* no addtional bit stream info */
}

/* symmetric quantization on 'levels' levels */
#define sym_quant(c, e, levels) \
    ((((((levels) * (c)) >> (24-(e))) + 1) >> 1) + ((levels) >> 1))

/* asymmetric quantization on 2^qbits levels */
static inline int
asym_quant(int c, int e, int qbits)
{
    int lshift, m, v;

    lshift = e + (qbits-1) - 24;
    if(lshift >= 0) v = c << lshift;
    else v = c >> (-lshift);

    m = (1 << (qbits-1));
    v = CLIP(v, -m, m-1);

    return v & ((1 << qbits)-1);
}

static void
quantize_mantissas(A52ThreadContext *tctx)
{
    A52Context *ctx = tctx->ctx;
    A52Frame *frame = &tctx->frame;
    A52Block *block;
    uint16_t *qmant1_ptr, *qmant2_ptr, *qmant4_ptr;
    int blk, ch, i, b, c, e, v;
    int mant1_cnt, mant2_cnt, mant4_cnt;

    for(blk=0; blk<A52_NUM_BLOCKS; blk++) {
        block = &frame->blocks[blk];
        mant1_cnt = mant2_cnt = mant4_cnt = 0;
        qmant1_ptr = qmant2_ptr = qmant4_ptr = NULL;
        for(ch=0; ch<ctx->n_all_channels; ch++) {
            for(i=0; i<frame->ncoefs[ch]; i++) {
                c = (int)(block->mdct_coef[ch][i] * (1 << 24));
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
output_audio_blocks(A52ThreadContext *tctx)
{
    A52Context *ctx = tctx->ctx;
    A52Frame *frame = &tctx->frame;
    A52Block *block;
    BitWriter *bw;
    int blk, ch, i, baie, rbnd;

    bw = &tctx->bw;
    for(blk=0; blk<A52_NUM_BLOCKS; blk++) {
        block = &frame->blocks[blk];
        for(ch=0; ch<ctx->n_channels; ch++) {
            bitwriter_writebits(bw, 1, block->blksw[ch]);
        }
        for(ch=0; ch<ctx->n_channels; ch++) {
            bitwriter_writebits(bw, 1, block->dithflag[ch]);
        }
        if(ctx->params.dynrng_profile == DYNRNG_PROFILE_NONE) {
            bitwriter_writebits(bw, 1, 0); // no dynamic range
            if(ctx->acmod == A52_ACMOD_DUAL_MONO) {
                bitwriter_writebits(bw, 1, 0); // no dynamic range 2
            }
        } else {
            bitwriter_writebits(bw, 1, 1);
            bitwriter_writebits(bw, 8, block->dynrng);
            if(ctx->acmod == A52_ACMOD_DUAL_MONO) {
                bitwriter_writebits(bw, 1, 1);
                bitwriter_writebits(bw, 8, block->dynrng);
            }
        }
        if(block->block_num == 0) {
            // must define coupling strategy in block 0
            bitwriter_writebits(bw, 1, 1); // new coupling strategy
            bitwriter_writebits(bw, 1, 0); // no coupling in use
        } else {
            bitwriter_writebits(bw, 1, 0); // no new coupling strategy
        }

        if(ctx->acmod == A52_ACMOD_STEREO) {
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
output_frame_end(A52ThreadContext *tctx)
{
    uint8_t *frame;
    int fs, fs58, n, crc1, crc2, bitcount;

    fs = tctx->frame.frame_size;
    // align to 8 bits
    bitwriter_flushbits(&tctx->bw);
    // add zero bytes to reach the frame size
    frame = tctx->bw.buffer;
    bitcount = bitwriter_bitcount(&tctx->bw);
    n = (fs << 1) - 2 - (bitcount >> 3);
    if(n < 0) {
        fprintf(stderr, "data size exceeds frame size (frame=%d data=%d)\n", (fs << 1) - 2, bitcount >> 3);
        return -1;
    }
    if(n > 0) memset(&tctx->bw.buffer[bitcount>>3], 0, n);

    // compute crc1 for 1st 5/8 of frame
    fs58 = (fs >> 1) + (fs >> 3);
    crc1 = calc_crc16(&frame[4], (fs58<<1)-4);
    crc1 = crc16_zero(crc1, (fs58<<1)-2);
    frame[2] = crc1 >> 8;
    frame[3] = crc1;
    // double-check
    crc1 = calc_crc16(&frame[2], (fs58<<1)-2);
    if(crc1 != 0) fprintf(stderr, "CRC ERROR\n");

    // compute crc2 for final 3/8 of frame
    crc2 = calc_crc16(&frame[fs58<<1], ((fs - fs58) << 1) - 2);
    frame[(fs<<1)-2] = crc2 >> 8;
    frame[(fs<<1)-1] = crc2;

    return (fs << 1);
}

static void
copy_samples(A52ThreadContext *tctx)
{
    A52Context *ctx = tctx->ctx;
    A52Frame *frame = &tctx->frame;
    FLOAT buffer[A52_SAMPLES_PER_FRAME];
    FLOAT *in_audio;
    FLOAT *out_audio;
    FLOAT *temp;
    int ch, blk;
#define SWAP_BUFFERS temp=in_audio;in_audio=out_audio;out_audio=temp;

    for(ch=0; ch<ctx->n_all_channels; ch++) {
        out_audio = buffer;
        in_audio = frame->input_audio[ch];
        // DC-removal high-pass filter
        if(ctx->params.use_dc_filter) {
            filter_run(&ctx->dc_filter[ch], out_audio, in_audio,
                       A52_SAMPLES_PER_FRAME);
            SWAP_BUFFERS
        }
        if (ch < ctx->n_channels) {
            // channel bandwidth filter
            if(ctx->params.use_bw_filter) {
                filter_run(&ctx->bw_filter[ch], out_audio, in_audio,
                           A52_SAMPLES_PER_FRAME);
                SWAP_BUFFERS
            }
            // block-switching high-pass filter
            if(ctx->params.use_block_switching) {
                filter_run(&ctx->bs_filter[ch], out_audio, in_audio,
                           A52_SAMPLES_PER_FRAME);
                memcpy(frame->blocks[0].transient_samples[ch],
                       ctx->last_transient_samples[ch], 256 * sizeof(FLOAT));
                memcpy(&frame->blocks[0].transient_samples[ch][256], out_audio,
                       256 * sizeof(FLOAT));
                for(blk=1; blk<A52_NUM_BLOCKS; blk++) {
                    memcpy(frame->blocks[blk].transient_samples[ch],
                           &out_audio[256*(blk-1)], 512 * sizeof(FLOAT));
                }
                memcpy(ctx->last_transient_samples[ch],
                       &out_audio[256*5], 256 * sizeof(FLOAT));
            }
        } else {
            // LFE bandwidth low-pass filter
            if(ctx->params.use_lfe_filter) {
                assert(ch == ctx->lfe_channel);
                filter_run(&ctx->lfe_filter, out_audio, in_audio,
                           A52_SAMPLES_PER_FRAME);
                SWAP_BUFFERS
            }
        }

        memcpy(frame->blocks[0].input_samples[ch], ctx->last_samples[ch],
               256 * sizeof(FLOAT));
        memcpy(&frame->blocks[0].input_samples[ch][256], in_audio,
               256 * sizeof(FLOAT));
        for(blk=1; blk<A52_NUM_BLOCKS; blk++) {
            memcpy(frame->blocks[blk].input_samples[ch], &in_audio[256*(blk-1)],
                   512 * sizeof(FLOAT));
        }
        memcpy(ctx->last_samples[ch],
               &in_audio[256*5], 256 * sizeof(FLOAT));
    }
#undef SWAP_BUFFERS
}

/* determines block length by detecting transients */
static int
detect_transient(FLOAT *in)
{
    FLOAT *xx = in;
    int i, j;
    FLOAT level1[2];
    FLOAT level2[4];
    FLOAT level3[8];
    FLOAT tmax = FCONST(100.0) / FCONST(32768.0);
    FLOAT t1 = FCONST(0.100);
    FLOAT t2 = FCONST(0.075);
    FLOAT t3 = FCONST(0.050);

    // level 1 (2 x 256)
    for(i=0; i<2; i++) {
        level1[i] = 0;
        for(j=0; j<256; j++) {
            level1[i] = MAX(AFT_FABS(xx[i*256+j]), level1[i]);
        }
        if(level1[i] < tmax) {
            return 0;
        }
        if((i > 0) && (level1[i] * t1 > level1[i-1])) {
            return 1;
        }
    }

    // level 2 (4 x 128)
    for(i=1; i<4; i++) {
        level2[i] = 0;
        for(j=0; j<128; j++) {
            level2[i] = MAX(AFT_FABS(xx[i*128+j]), level2[i]);
        }
        if((i > 1) && (level2[i] * t2 > level2[i-1])) {
            return 1;
        }
    }

    // level 3 (8 x 64)
    for(i=3; i<8; i++) {
        level3[i] = 0;
        for(j=0; j<64; j++) {
            level3[i] = MAX(AFT_FABS(xx[i*64+j]), level3[i]);
        }
        if((i > 3) && (level3[i] * t3 > level3[i-1])) {
            return 1;
        }
    }

    return 0;
}

static void
generate_coefs(A52ThreadContext *tctx)
{
    A52Context *ctx = tctx->ctx;
    A52Block *block;
    void (*mdct_256)(struct A52ThreadContext *tctx, FLOAT *out, FLOAT *in) =
        ctx->mdct_ctx_256.mdct;
    void (*mdct_512)(struct A52ThreadContext *tctx, FLOAT *out, FLOAT *in) =
        ctx->mdct_ctx_512.mdct;
    int blk, ch, i;

    for(ch=0; ch<ctx->n_all_channels; ch++) {
        for(blk=0; blk<A52_NUM_BLOCKS; blk++) {
            block = &tctx->frame.blocks[blk];
            if(ctx->params.use_block_switching) {
                block->blksw[ch] = detect_transient(block->transient_samples[ch]);
            } else {
                block->blksw[ch] = 0;
            }
            apply_a52_window(block->input_samples[ch]);
            if(block->blksw[ch]) {
                mdct_256(tctx, block->mdct_coef[ch], block->input_samples[ch]);
            } else {
                mdct_512(tctx, block->mdct_coef[ch], block->input_samples[ch]);
            }
            for(i=tctx->frame.ncoefs[ch]; i<256; i++) {
                block->mdct_coef[ch][i] = 0.0;
            }
        }
    }
}

static void
calc_rematrixing(A52ThreadContext *tctx)
{
    A52Context *ctx = tctx->ctx;
    A52Block *block;
    FLOAT sum[4][4];
    FLOAT lt, rt, ctmp1, ctmp2;
    int blk, bnd, i;


    if(!ctx->params.use_rematrixing) {
        tctx->frame.blocks[0].rematstr = 1;
        for(bnd=0; bnd<4; bnd++) {
            tctx->frame.blocks[0].rematflg[bnd] = 0;
        }
        for(blk=1; blk<A52_NUM_BLOCKS; blk++) {
            tctx->frame.blocks[blk].rematstr = 0;
        }
        return;
    }

    for(blk=0; blk<A52_NUM_BLOCKS; blk++) {
        block = &tctx->frame.blocks[blk];

        block->rematstr = 0;
        if(blk == 0) block->rematstr = 1;

        for(bnd=0; bnd<4; bnd++) {
            block->rematflg[bnd] = 0;
            sum[bnd][0] = sum[bnd][1] = sum[bnd][2] = sum[bnd][3] = 0;
            for(i=rematbndtab[bnd][0]; i<=rematbndtab[bnd][1]; i++) {
                if(i == tctx->frame.ncoefs[0]) break;
                lt = block->mdct_coef[0][i];
                rt = block->mdct_coef[1][i];
                sum[bnd][0] += lt * lt;
                sum[bnd][1] += rt * rt;
                sum[bnd][2] += (lt + rt) * (lt + rt) / FCONST(4.0);
                sum[bnd][3] += (lt - rt) * (lt - rt) / FCONST(4.0);
            }
            if(sum[bnd][0]+sum[bnd][1] >= (sum[bnd][2]+sum[bnd][3])/FCONST(2.0)) {
                block->rematflg[bnd] = 1;
                for(i=rematbndtab[bnd][0]; i<=rematbndtab[bnd][1]; i++) {
                    if(i == tctx->frame.ncoefs[0]) break;
                    ctmp1 = block->mdct_coef[0][i] * FCONST(0.5);
                    ctmp2 = block->mdct_coef[1][i] * FCONST(0.5);
                    block->mdct_coef[0][i] = ctmp1 + ctmp2;
                    block->mdct_coef[1][i] = ctmp1 - ctmp2;
                }
            }
            if(blk != 0 && block->rematstr == 0 &&
               block->rematflg[bnd] != tctx->frame.blocks[blk-1].rematflg[bnd]) {
                block->rematstr = 1;
            }
        }
    }
}

/** Adjust for fractional frame sizes in CBR mode */
static void
adjust_frame_size(A52ThreadContext *tctx)
{
    A52Context *ctx = tctx->ctx;
    A52Frame *f = &tctx->frame;
    uint32_t kbps = f->bit_rate * 1000;
    uint32_t srate = ctx->sample_rate;
    int add;

    while(tctx->bit_cnt >= kbps && tctx->sample_cnt >= srate) {
        tctx->bit_cnt -= kbps;
        tctx->sample_cnt -= srate;
    }
    add = !!(tctx->bit_cnt * srate < tctx->sample_cnt * kbps);
    f->frame_size = f->frame_size_min + add;
}

static void
compute_dither_strategy(A52ThreadContext *tctx)
{
    A52Block *block0;
    A52Block *block1;
    int channels = tctx->ctx->n_channels;
    int blk, ch;

    block0 = NULL;
    for(blk=0; blk<A52_NUM_BLOCKS; blk++) {
        block1 = &tctx->frame.blocks[blk];
        for(ch=0; ch<channels; ch++) {
            if(block1->blksw[ch] || ((blk>0) && block0->blksw[ch])) {
                block1->dithflag[ch] = 0;
            } else {
                block1->dithflag[ch] = 1;
            }
        }
        block0 = block1;
    }
}

static void
calculate_dynrng(A52ThreadContext *tctx)
{
    A52Context *ctx = tctx->ctx;
    A52Block *block;
    int blk;

    if(ctx->params.dynrng_profile == DYNRNG_PROFILE_NONE)
        return;

    for(blk=0; blk<A52_NUM_BLOCKS; blk++) {
        block = &tctx->frame.blocks[blk];
        block->dynrng = calculate_block_dynrng(block->input_samples,
                                               ctx->n_all_channels,
                                               -ctx->meta.dialnorm,
                                               ctx->params.dynrng_profile);
    }
}

static int
encode_frame(A52ThreadContext *tctx, uint8_t *frame_buffer)
{
    A52Context *ctx = tctx->ctx;
    A52Frame *frame = &tctx->frame;

    calculate_dynrng(tctx);

    generate_coefs(tctx);

    compute_dither_strategy(tctx);

    if(ctx->acmod == A52_ACMOD_STEREO) {
        calc_rematrixing(tctx);
    }

    process_exponents(tctx);

    if(ctx->params.encoding_mode == AFTEN_ENC_MODE_CBR) {
        adjust_frame_size(tctx);
    }

    if(compute_bit_allocation(tctx)) {
        fprintf(stderr, "Error in bit allocation\n");
        tctx->framesize = 0;
        return -1;
    }

    quantize_mantissas(tctx);

    // increment counters
    tctx->bit_cnt += frame->frame_size * 16;
    tctx->sample_cnt += A52_SAMPLES_PER_FRAME;

    // update encoding status
    tctx->status.quality = frame->quality;
    tctx->status.bit_rate = frame->bit_rate;
    tctx->status.bwcode = frame->bwcode;

    output_frame_header(tctx, frame_buffer);
    output_audio_blocks(tctx);
    tctx->framesize = output_frame_end(tctx);

    return 0;
}

static int
threaded_encode(void* vtctx)
{
    A52ThreadContext *tctx = vtctx;

    posix_mutex_lock(&tctx->ts.enter_mutex);
    posix_cond_signal(&tctx->ts.enter_cond);
    while(1) {
        posix_cond_wait(&tctx->ts.enter_cond, &tctx->ts.enter_mutex);
        posix_mutex_lock(&tctx->ts.confirm_mutex);
        posix_cond_signal(&tctx->ts.confirm_cond);
        posix_mutex_unlock(&tctx->ts.confirm_mutex);

        windows_event_set(&tctx->ts.ready_event);
        windows_event_wait(&tctx->ts.enter_event);
        /* end thread if nothing to encode */
        if (tctx->state == END) {
            tctx->framesize = 0;
            break;
        }
        if (tctx->state == ABORT) {
            tctx->framesize = -1;
            break;
        }
        if (encode_frame(tctx, tctx->frame_buffer))
            tctx->state = ABORT;
    }
    posix_mutex_unlock(&tctx->ts.enter_mutex);

    windows_event_set(&tctx->ts.ready_event);

    return 0;
}

static int
encode_frame_parallel(AftenContext *s, uint8_t *frame_buffer, const void *samples)
{
    static int thread_num = 0;
    static int threads_to_abort = 0;
    A52Context *ctx = s->private_context;
    int framesize = 0;

    do {
        A52ThreadContext *tctx = &ctx->tctx[thread_num];

        posix_mutex_lock(&tctx->ts.enter_mutex);

        windows_event_wait(&tctx->ts.ready_event);

        if (tctx->state == ABORT || threads_to_abort) {
            tctx->state = ABORT;
            framesize = -1;
            if (!threads_to_abort)
                threads_to_abort = ctx->n_threads;
            --threads_to_abort;
        } else {
            if (tctx->state == START)
                tctx->state = WORK;
            else {
                if(tctx->framesize > 0) {
                    framesize = tctx->framesize;
                    memcpy(frame_buffer, tctx->frame_buffer, framesize);
                   // update encoding status
                    s->status.quality   = tctx->status.quality;
                    s->status.bit_rate  = tctx->status.bit_rate;
                    s->status.bwcode    = tctx->status.bwcode;
                } else {
                    posix_mutex_unlock(&tctx->ts.enter_mutex);
                    goto end;
                }
            }
            if(!samples)
                tctx->state = END;
            else {
                // convert sample format and de-interleave channels
                ctx->fmt_convert_from_src(tctx->frame.input_audio, samples, ctx->n_all_channels, A52_SAMPLES_PER_FRAME);
                if(!frame_init(tctx))
                    copy_samples(tctx);
                else {
                    fprintf(stderr, "Encoding has not properly initialized\n");
                    tctx->state=ABORT;
                    threads_to_abort = ctx->n_threads - 1;
                    framesize = -1;
                }
            }
        }
        posix_mutex_lock(&tctx->ts.confirm_mutex);
        posix_cond_signal(&tctx->ts.enter_cond);
        posix_mutex_unlock(&tctx->ts.enter_mutex);
        posix_cond_wait(&tctx->ts.confirm_cond, &tctx->ts.confirm_mutex);
        posix_mutex_unlock(&tctx->ts.confirm_mutex);

        windows_event_set(&tctx->ts.enter_event);
end:
        ++thread_num;
        thread_num %= ctx->n_threads;
    } while(threads_to_abort);

    return framesize;
}

int
aften_encode_frame(AftenContext *s, uint8_t *frame_buffer, const void *samples)
{
    A52Context *ctx;
    A52ThreadContext *tctx;
    A52Frame *frame;

    if(s == NULL || frame_buffer == NULL) {
        fprintf(stderr, "One or more NULL parameters passed to aften_encode_frame\n");
        return -1;
    }
    ctx = s->private_context;

    if (ctx->n_threads > 1)
        return encode_frame_parallel(s, frame_buffer, samples);

    if (!samples)
        return 0;

    tctx = ctx->tctx;
    frame = &tctx->frame;

    ctx->fmt_convert_from_src(frame->input_audio, samples, ctx->n_all_channels, A52_SAMPLES_PER_FRAME);

    if(frame_init(tctx)) {
        fprintf(stderr, "Encoding has not properly initialized\n");
        return -1;
    }

    copy_samples(tctx);

    encode_frame(tctx, frame_buffer);

    s->status.quality   = tctx->status.quality;
    s->status.bit_rate  = tctx->status.bit_rate;
    s->status.bwcode    = tctx->status.bwcode;

    return tctx->framesize;
}

void
aften_encode_close(AftenContext *s)
{
    if(s != NULL && s->private_context != NULL) {
        A52Context *ctx = s->private_context;
        /* mdct_close deinits both mdcts */
        ctx->mdct_ctx_512.mdct_close(ctx);
        if (ctx->n_threads == 1)
            ctx->tctx[0].mdct_tctx_512.mdct_thread_close(&ctx->tctx[0]);
        else {
            int i;
            for (i=0; i<ctx->n_threads; ++i) {
                A52ThreadContext cur_tctx = ctx->tctx[i];
                thread_join(cur_tctx.ts.thread);
                cur_tctx.mdct_tctx_512.mdct_thread_close(&cur_tctx);
                posix_cond_destroy(&cur_tctx.ts.enter_cond);
                posix_cond_destroy(&cur_tctx.ts.confirm_cond);

                posix_mutex_destroy(&cur_tctx.ts.enter_mutex);
                posix_mutex_destroy(&cur_tctx.ts.confirm_mutex);

                windows_event_destroy(&cur_tctx.ts.ready_event);
                windows_event_destroy(&cur_tctx.ts.enter_event);
            }
        }
        free(ctx);
        s->private_context = NULL;
    }
}
