/**
 * Aften: A/52 audio encoder
 * Copyright (c) 2006  Justin Ruggles <jruggle@earthlink.net>
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
 * @file filter.c
 * Audio filters
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "filter.h"

typedef struct Filter {
    const char *name;
    enum FilterID id;
    int private_size;
    int (*init)(FilterContext *f);
    void (*filter)(FilterContext *f, FLOAT *out, FLOAT *in, int n);
} Filter;


typedef struct {
    FLOAT coefs[5];
    FLOAT state[2][5];
} BiquadContext;

static void
biquad_generate_lowpass(BiquadContext *f, FLOAT fc)
{
    FLOAT omega, alpha, cs;
    FLOAT a[3], b[3];

    omega = 2.0 * M_PI * fc;
    alpha = sin(omega) / 2.0;
    cs = cos(omega);

    a[0] = 1.0 + alpha;
    a[1] = -2.0 * cs;
    a[2] = 1.0 - alpha;
    b[0] = (1.0 - cs) / 2.0;
    b[1] = (1.0 - cs);
    b[2] = (1.0 - cs) / 2.0;

    f->coefs[0] = b[0] / a[0];
    f->coefs[1] = b[1] / a[0];
    f->coefs[2] = b[2] / a[0];
    f->coefs[3] = a[1] / a[0];
    f->coefs[4] = a[2] / a[0];
}

static void
biquad_generate_highpass(BiquadContext *f, FLOAT fc)
{
    FLOAT omega, alpha, cs;
    FLOAT a[3], b[3];

    omega = 2.0 * M_PI * fc;
    alpha = sin(omega) / 2.0;
    cs = cos(omega);

    a[0] = 1.0 + alpha;
    a[1] = -2.0 * cs;
    a[2] = 1.0 - alpha;
    b[0] = (1.0 + cs) / 2.0;
    b[1] = -(1.0 + cs);
    b[2] = (1.0 + cs) / 2.0;

    f->coefs[0] = b[0] / a[0];
    f->coefs[1] = b[1] / a[0];
    f->coefs[2] = b[2] / a[0];
    f->coefs[3] = a[1] / a[0];
    f->coefs[4] = a[2] / a[0];
}

static int
biquad_init(FilterContext *f)
{
    int i, j;
    BiquadContext *b = f->private;
    FLOAT fc;

    if(f->samplerate <= 0) {
        return -1;
    }
    if(f->cutoff < 0 || f->cutoff > (f->samplerate/2.0)) {
        return -1;
    }
    fc = f->cutoff / f->samplerate;

    if(f->type == FILTER_TYPE_LOWPASS) {
        biquad_generate_lowpass(b, fc);
    } else if(f->type == FILTER_TYPE_HIGHPASS) {
        biquad_generate_highpass(b, fc);
    } else {
        return -1;
    }

    for(j=0; j<2; j++) {
        for(i=0; i<5; i++) {
            b->state[j][i] = 0.0;
        }
    }

    return 0;
}

static void
biquad_i_run_filter(FilterContext *f, FLOAT *out, FLOAT *in, int n)
{
    int i, j, datasize, loops;
    FLOAT v;
    FLOAT *tmp;
    BiquadContext *b = f->private;

    datasize = 0;
    tmp = in;
    loops = 1;
    if(f->cascaded) {
        loops = 2;
        datasize = n * sizeof(FLOAT);
        tmp = malloc(datasize);
        memcpy(tmp, in, datasize);
    }

    for(j=0; j<loops; j++) {
        for(i=0; i<n; i++) {
            b->state[j][0] = tmp[i];

            v = 0;
            v += b->coefs[0] * b->state[j][0];
            v += b->coefs[1] * b->state[j][1];
            v += b->coefs[2] * b->state[j][2];
            v -= b->coefs[3] * b->state[j][3];
            v -= b->coefs[4] * b->state[j][4];

            b->state[j][2] = b->state[j][1];
            b->state[j][1] = b->state[j][0];
            b->state[j][4] = b->state[j][3];
            b->state[j][3] = v;

            if(v < -1.0) v = -1.0;
            if(v > 1.0) v = 1.0;

            out[i] = v;
        }
        if(f->cascaded && j != loops-1) {
            memcpy(tmp, out, datasize);
        }
    }

    if(f->cascaded) free(tmp);
}

static void
biquad_ii_run_filter(FilterContext *f, FLOAT *out, FLOAT *in, int n)
{
    int i, j, datasize, loops;
    FLOAT v;
    FLOAT *tmp;
    BiquadContext *b = f->private;

    datasize = 0;
    tmp = in;
    loops = 1;
    if(f->cascaded) {
        loops = 2;
        datasize = n * sizeof(FLOAT);
        tmp = malloc(datasize);
        memcpy(tmp, in, datasize);
    }

    for(j=0; j<loops; j++) {
        for(i=0; i<n; i++) {
            b->state[j][0] = tmp[i];

            v = b->coefs[0] * b->state[j][0] + b->state[j][1];
            b->state[j][1] = b->coefs[1] * b->state[j][0] - b->coefs[3] * v + b->state[j][2];
            b->state[j][2] = b->coefs[2] * b->state[j][0] - b->coefs[4] * v;

            if(v < -1.0) v = -1.0;
            if(v > 1.0) v = 1.0;

            out[i] = v;
        }
        if(f->cascaded && j != loops-1) {
            memcpy(tmp, out, datasize);
        }
    }

    if(f->cascaded) free(tmp);
}

Filter biquad_i_filter = {
    "Biquad Direct Form I",
    FILTER_ID_BIQUAD_I,
    sizeof(BiquadContext),
    biquad_init,
    biquad_i_run_filter,
};

Filter biquad_ii_filter = {
    "Biquad Direct Form II",
    FILTER_ID_BIQUAD_II,
    sizeof(BiquadContext),
    biquad_init,
    biquad_ii_run_filter,
};


#ifndef M_SQRT2
#define M_SQRT2 1.41421356237309504880
#endif

static void
butterworth_generate_lowpass(BiquadContext *f, FLOAT fc)
{
    FLOAT c = 1.0 / tan(M_PI * fc);
    FLOAT c2 = (c * c);

    f->coefs[0] = 1.0 / (c2 + M_SQRT2 * c + 1.0);
    f->coefs[1] = 2.0 * f->coefs[0];
    f->coefs[2] = f->coefs[0];
    f->coefs[3] = 2.0 * (1.0 - c2) * f->coefs[0];
    f->coefs[4] = (c2 - M_SQRT2 * c + 1.0) * f->coefs[0];
}

static void
butterworth_generate_highpass(BiquadContext *f, FLOAT fc)
{
    FLOAT c = tan(M_PI * fc);
    FLOAT c2 = (c * c);

    f->coefs[0] = 1.0 / (c2 + M_SQRT2 * c + 1.0);
    f->coefs[1] = -2.0 * f->coefs[0];
    f->coefs[2] = f->coefs[0];
    f->coefs[3] = 2.0 * (c2 - 1.0) * f->coefs[0];
    f->coefs[4] = (c2 - M_SQRT2 * c + 1.0) * f->coefs[0];
}

static int
butterworth_init(FilterContext *f)
{
    int i, j;
    BiquadContext *b = f->private;
    FLOAT fc;

    if(f->samplerate <= 0) {
        return -1;
    }
    if(f->cutoff < 0 || f->cutoff > (f->samplerate/2.0)) {
        return -1;
    }
    fc = f->cutoff / f->samplerate;

    if(f->type == FILTER_TYPE_LOWPASS) {
        butterworth_generate_lowpass(b, fc);
    } else if(f->type == FILTER_TYPE_HIGHPASS) {
        butterworth_generate_highpass(b, fc);
    } else {
        return -1;
    }

    for(j=0; j<2; j++) {
        for(i=0; i<5; i++) {
            b->state[j][i] = 0.0;
        }
    }

    return 0;
}

Filter butterworth_i_filter = {
    "Butterworth Direct Form I",
    FILTER_ID_BUTTERWORTH_I,
    sizeof(BiquadContext),
    butterworth_init,
    biquad_i_run_filter,
};

Filter butterworth_ii_filter = {
    "Butterworth Direct Form II",
    FILTER_ID_BUTTERWORTH_II,
    sizeof(BiquadContext),
    butterworth_init,
    biquad_ii_run_filter,
};


/* One-pole, One-zero filter */

typedef struct {
    FLOAT p;
    FLOAT last;
} OnePoleContext;

static void
onepole_generate_lowpass(OnePoleContext *o, FLOAT fc)
{
    FLOAT omega, cs;

    omega = 2.0 * M_PI * fc;
    cs = 2.0 - cos(omega);
    o->p = cs - sqrt((cs*cs)-1.0);
    o->last = 0.0;
}

static void
onepole_generate_highpass(OnePoleContext *o, FLOAT fc)
{
    FLOAT omega, cs;

    omega = 2.0 * M_PI * fc;
    cs = 2.0 + cos(omega);
    o->p = cs - sqrt((cs*cs)-1.0);
    o->last = 0.0;
}

static int
onepole_init(FilterContext *f)
{
    OnePoleContext *o = f->private;
    FLOAT fc;

    if(f->cascaded) {
        return -1;
    }

    if(f->samplerate <= 0) {
        return -1;
    }
    if(f->cutoff < 0 || f->cutoff > (f->samplerate/2.0)) {
        return -1;
    }
    fc = f->cutoff / f->samplerate;

    if(f->type == FILTER_TYPE_LOWPASS) {
        onepole_generate_lowpass(o, fc);
    } else if(f->type == FILTER_TYPE_HIGHPASS) {
        onepole_generate_highpass(o, fc);
    } else {
        return -1;
    }

    return 0;
}

static void
onepole_run_filter(FilterContext *f, FLOAT *out, FLOAT *in, int n)
{
    int i;
    FLOAT v;
    FLOAT p1 = 0;
    OnePoleContext *o = f->private;

    if(f->type == FILTER_TYPE_LOWPASS) {
        p1 = 1.0 - o->p;
    } else if(f->type == FILTER_TYPE_HIGHPASS) {
        p1 = o->p - 1.0;
    }

    for(i=0; i<n; i++) {
        v = (p1 * in[i]) + (o->p * o->last);

        if(v < -1.0) v = -1.0;
        if(v > 1.0) v = 1.0;

        o->last = out[i] = v;
    }
}

Filter onepole_filter = {
    "One-Pole Filter",
    FILTER_ID_ONEPOLE,
    sizeof(OnePoleContext),
    onepole_init,
    onepole_run_filter,
};


int
filter_init(FilterContext *f, enum FilterID id)
{
    if(f == NULL) return -1;

    switch(id) {
        case FILTER_ID_BIQUAD_I:        f->filter = &biquad_i_filter;
                                        break;
        case FILTER_ID_BIQUAD_II:       f->filter = &biquad_ii_filter;
                                        break;
        case FILTER_ID_BUTTERWORTH_I:   f->filter = &butterworth_i_filter;
                                        break;
        case FILTER_ID_BUTTERWORTH_II:  f->filter = &butterworth_ii_filter;
                                        break;
        case FILTER_ID_ONEPOLE:         f->filter = &onepole_filter;
                                        break;
        default:                        return -1;
    }

    f->private = malloc(f->filter->private_size);

    return f->filter->init(f);
}

void
filter_run(FilterContext *f, FLOAT *out, FLOAT *in, int n)
{
    f->filter->filter(f, out, in, n);
}

void
filter_close(FilterContext *f)
{
    if(!f) return;
    if(f->private) {
        free(f->private);
        f->private = NULL;
    }
    f->filter = NULL;
}
