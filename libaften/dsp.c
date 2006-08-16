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
 * @file dsp.c
 * Signal processing
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "a52.h"
#include "dsp.h"

typedef struct Complex {
    double re, im;
} Complex;

static void
fft_init(FFTContext *fft, int len)
{
    int i, j, m, nbits, n2;
    double c;

    fft->length = len;
    nbits = log2i(len);
    n2 = len >> 1;
    c = 2.0 * M_PI / len;

    fft->costab = calloc(n2, sizeof(double));
    fft->sintab = calloc(n2, sizeof(double));
    fft->revtab = calloc(len, sizeof(double));

    for(i=0; i<n2; i++) {
        fft->costab[i] = cos(c * i);
        fft->sintab[i] = sin(c * i);
    }

    for(i=0; i<len; i++) {
        m = 0;
        for(j=0; j<nbits; j++) {
            m |= ((i >> j) & 1) << (nbits-j-1);
        }
        fft->revtab[i] = m;
    }
}

static void
fft_close(FFTContext *fft) {
    if(fft) {
        fft->length = 0;
        if(fft->costab) {
            free(fft->costab);
            fft->costab = NULL;
        }
        if(fft->sintab) {
            free(fft->sintab);
            fft->sintab = NULL;
        }
        if(fft->revtab) {
            free(fft->revtab);
            fft->revtab = NULL;
        }
    }
}

static void
mdct_init(MDCTContext *mdct, int len)
{
    int n4;
    int i;
    double alpha, c;

    mdct->length = len;
    c = 2.0 * M_PI / mdct->length;
    n4 = mdct->length >> 2;

    mdct->xcos1 = calloc(n4, sizeof(double));
    mdct->xsin1 = calloc(n4, sizeof(double));

    for(i=0; i<n4; i++) {
        alpha = c * (i + 0.125);
        mdct->xcos1[i] = -cos(alpha);
        mdct->xsin1[i] = -sin(alpha);
    }

    mdct->fft = calloc(1, sizeof(FFTContext));
    fft_init(mdct->fft, n4);

    mdct->buffer = calloc(len, sizeof(double));
    mdct->cbuffer = calloc(n4, sizeof(Complex));
}

static void
mdct_close(MDCTContext *mdct)
{
    if(mdct) {
        mdct->length = 0;
        if(mdct->xcos1) {
            free(mdct->xcos1);
            mdct->xcos1 = NULL;
        }
        if(mdct->xsin1) {
            free(mdct->xsin1);
            mdct->xsin1 = NULL;
        }
        if(mdct->fft) {
            fft_close(mdct->fft);
            free(mdct->fft);
            mdct->fft = NULL;
        }
        if(mdct->buffer) {
            free(mdct->buffer);
            mdct->buffer = NULL;
        }
        if(mdct->cbuffer) {
            free(mdct->cbuffer);
            mdct->cbuffer = NULL;
        }
    }
}

static inline void
butterfly(double *p_re, double *p_im, double *q_re, double *q_im,
          double p1_re, double p1_im, double q1_re, double q1_im)
{
    *p_re = (p1_re + q1_re) * 0.5;
    *p_im = (p1_im + q1_im) * 0.5;
    *q_re = (p1_re - q1_re) * 0.5;
    *q_im = (p1_im - q1_im) * 0.5;
}

static inline void
complex_mul(Complex *p, Complex *a, Complex *b)
{
    p->re = (a->re * b->re) - (a->im * b->im);
    p->im = (a->re * b->im) + (b->re * a->im);
}

/* do a 2^n point complex fft on 2^ln points. */
static void
fft(FFTContext *fft, Complex *z)
{
    int j, l, np, np2;
    int nblocks, nloops;
    Complex *p, *q;
    Complex tmp, tmp1;

    np = fft->length;

    // reverse
    for(j=0; j<np; j++) {
        int k;
        k = fft->revtab[j];
        if(k < j) {
            tmp = z[k];
            z[k] = z[j];
            z[j] = tmp;
        }
    }

    // pass 0

    p=&z[0];
    j=(np >> 1);
    do {
        butterfly(&p[0].re, &p[0].im, &p[1].re, &p[1].im,
                  p[0].re, p[0].im, p[1].re, p[1].im);
        p+=2;
    } while (--j != 0);

    // pass 1

    p=&z[0];
    j=np >> 2;
    do {
        butterfly(&p[0].re, &p[0].im, &p[2].re, &p[2].im,
                  p[0].re, p[0].im, p[2].re, p[2].im);
        butterfly(&p[1].re, &p[1].im, &p[3].re, &p[3].im,
                  p[1].re, p[1].im, p[3].im, -p[3].re);
        p+=4;
    } while (--j != 0);

    // pass 2 .. ln-1

    nblocks = np >> 3;
    nloops = 1 << 2;
    np2 = np >> 1;
    do {
        p = z;
        q = z + nloops;
        for (j = 0; j < nblocks; ++j) {

            butterfly(&p->re, &p->im, &q->re, &q->im,
                      p->re, p->im, q->re, q->im);

            p++;
            q++;
            for(l = nblocks; l < np2; l += nblocks) {
                tmp1.re = fft->costab[l];
                tmp1.im = -fft->sintab[l];
                complex_mul(&tmp, &tmp1, q);
                butterfly(&p->re, &p->im, &q->re, &q->im,
                          p->re, p->im, tmp.re, tmp.im);
                p++;
                q++;
            }
            p += nloops;
            q += nloops;
        }
        nblocks = nblocks >> 1;
        nloops = nloops << 1;
    } while(nblocks != 0);
}

static void
dct_iv(MDCTContext *mdct, double *out, double *in)
{
    int i;
    Complex tmp, tmp1;
    int n, n2, n4;
    Complex *x;

    n = mdct->length;
    n2 = n >> 1;
    n4 = n >> 2;
    x = mdct->cbuffer;

    // pre rotation
    for(i=0; i<n4; i++) {
        tmp.re = (in[2*i] - in[n-1-2*i]) / 2.0;
        tmp.im = -(in[n2+2*i] - in[n2-1-2*i]) / 2.0;
        tmp1.re = -mdct->xcos1[i];
        tmp1.im = mdct->xsin1[i];
        complex_mul(&x[i], &tmp, &tmp1);
    }

    fft(mdct->fft, x);

    // post rotation
    for(i=0; i<n4; i++) {
        tmp1.re = mdct->xsin1[i];
        tmp1.im = mdct->xcos1[i];
        complex_mul(&tmp, &x[i], &tmp1);
        out[2*i] = tmp.im;
        out[n2-1-2*i] = tmp.re;
    }
}

void
mdct512(A52Context *ctx, double *out, double *in)
{
    int i;
    double *xx;

    xx = ctx->mdct_ctx_512.buffer;
    for(i=0; i<512; i++) {
        if(i < 128) xx[i] = -in[i+384];
        else xx[i] = in[i-128];
    }
    dct_iv(&ctx->mdct_ctx_512, out, xx);
}

#if 0
static void
mdct256_slow(double *out, double *in)
{
    int k, n;
    double s, a;

    for(k=0; k<128; k++) {
        s = 0;
        for(n=0; n<256; n++) {
            a = (M_PI/512.0)*(2.0*n+1)*(2.0*k+1);
            s += in[n] * cos(a);
        }
        out[k*2] = -2.0 * s / 256.0;
    }
    for(k=0; k<128; k++) {
        s = 0;
        for(n=0; n<256; n++) {
            a = (M_PI/512.0)*(2.0*n+1)*(2.0*k+1)+M_PI*(k+0.5);
            s += in[n+256] * cos(a);
        }
        out[2*k+1] = -2.0 * s / 256.0;
    }
}
#endif

void
mdct256(A52Context *ctx, double *out, double *in)
{
    int i;
    double *coef_a, *coef_b, *xx;

    coef_a = out;
    coef_b = &out[128];
    xx = ctx->mdct_ctx_256.buffer;

    dct_iv(&ctx->mdct_ctx_256, coef_a, in);

    for(i=0; i<128; i++) {
        xx[i] = -in[i+384];
        xx[i+128] = in[i+256];
    }
    dct_iv(&ctx->mdct_ctx_256, coef_b, xx);

    for(i=0; i<128; i++) {
        xx[2*i] = coef_a[i];
        xx[2*i+1] = coef_b[i];
    }
    memcpy(out, xx, 256 * sizeof(double));
}

static double a52_window[256];

/**
 * Generate a Kaiser-Bessel Derived Window.
 * @param alpha         Determines window shape
 * @param out_window    Array to fill with window values
 * @param n             Full window size
 * @param iter          Number of iterations to use in BesselI0
 */
static void
kbd_window_init(int alpha, double *window, int n, int iter)
{
    int j, k, n2;
    double a, x, wlast;

    n2 = n >> 1;
    a = alpha * M_PI / 256;
    a = a*a;
    for(k=0; k<n2; k++) {
        x = k * (n2 - k) * a;
        window[k] = 1.0;
        for(j=iter; j>0; j--) {
            window[k] = (window[k] * x / (j*j)) + 1.0;
        }
        if(k > 0) window[k] = window[k-1] + window[k];
    }
    wlast = sqrt(window[n2-1]+1);
    for(k=0; k<n2; k++) {
        window[k] = sqrt(window[k]) / wlast;
    }
}

void
apply_a52_window(double *samples)
{
    int i;
    for(i=0; i<256; i++) {
        samples[i] *= a52_window[i];
        samples[511-i] *= a52_window[i];
    }
}

void
dsp_init(A52Context *ctx)
{
    kbd_window_init(5.0, a52_window, 512, 50);
    mdct_init(&ctx->mdct_ctx_512, 512);
    mdct_init(&ctx->mdct_ctx_256, 256);
}

void
dsp_close(A52Context *ctx)
{
    mdct_close(&ctx->mdct_ctx_512);
    mdct_close(&ctx->mdct_ctx_256);
}
