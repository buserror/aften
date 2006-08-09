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

typedef struct Complex {
    double re, im;
} Complex;

static double costab[2][64];
static double sintab[2][64];
static int   fft_rev[2][512];

static void
fft_init(int ln, double *ctab, double *stab, int *rev)
{
    int i, j, m, n;
    double alpha;

    n = 1 << ln;

    for(i=0; i<(n/2); i++) {
        alpha = 2.0 * M_PI * i / n;
        ctab[i] = cos(alpha);
        stab[i] = sin(alpha);
    }

    for(i=0; i<n; i++) {
        m = 0;
        for(j=0; j<ln; j++) {
            m |= ((i >> j) & 1) << (ln-j-1);
        }
        rev[i] = m;
    }
}

static double xcos1[2][128];
static double xsin1[2][128];

static void
mdct_init(int n, double *xc, double *xs)
{
    int i, n2, n4;
    double alpha;

    n2 = n >> 1;
    n4 = n >> 2;
    for(i=0; i<n4; i++) {
        alpha = M_PI * (i + 0.125) / n2;
        xc[i] = -cos(alpha);
        xs[i] = -sin(alpha);
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
fft(Complex *z, int ln)
{
    int j, l, np, np2;
    int nblocks, nloops;
    Complex *p, *q;
    Complex tmp, tmp1;

    np = 1 << ln;

    // reverse
    for(j=0; j<np; j++) {
        int k;
        k = fft_rev[ln-6][j];
        if (k < j) {
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
                tmp1.re = costab[ln-6][l];
                tmp1.im = -sintab[ln-6][l];
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
dct_iv(double *out, double *in, int ln)
{
    int i;
    Complex tmp, tmp1;
    int n, n2, n4;
    Complex *x;

    n = 1 << ln;
    n2 = n >> 1;
    n4 = n >> 2;
    x = malloc(n4 * sizeof(Complex));

    // pre rotation
    for(i=0; i<n4; i++) {
        tmp.re = (in[2*i] - in[n-1-2*i]) / 2.0;
        tmp.im = -(in[n2+2*i] - in[n2-1-2*i]) / 2.0;
        tmp1.re = -xcos1[ln-8][i];
        tmp1.im = xsin1[ln-8][i];
        complex_mul(&x[i], &tmp, &tmp1);
    }

    fft(x, ln-2);

    // post rotation
    for(i=0;i<n4;i++) {
        tmp1.re = xsin1[ln-8][i];
        tmp1.im = xcos1[ln-8][i];
        complex_mul(&tmp, &x[i], &tmp1);
        out[2*i] = tmp.im;
        out[n2-1-2*i] = tmp.re;
    }

    free(x);
}

void
mdct512(double *out, double *in)
{
    int i;
    double *xx;

    xx = calloc(512, sizeof(double));
    for(i=0; i<512; i++) {
        if(i < 128) xx[i] = -in[i+384];
        else xx[i] = in[i-128];
    }
    dct_iv(out, xx, 9);
    free(xx);
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
mdct256(double *out, double *in)
{
    int i;
    double *coef_a, *coef_b, *xx;

    coef_a = calloc(128, sizeof(double));
    coef_b = calloc(128, sizeof(double));
    xx = calloc(256, sizeof(double));

    dct_iv(coef_a, in, 8);

    for(i=0; i<128; i++) {
        xx[i] = -in[i+384];
        xx[i+128] = in[i+256];
    }
    dct_iv(coef_b, xx, 8);

    for(i=0; i<128; i++) {
        out[2*i] = coef_a[i];
        out[2*i+1] = coef_b[i];
    }

    free(coef_a);
    free(coef_b);
    free(xx);
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
dsp_init()
{
    kbd_window_init(5.0, a52_window, 512, 50);
    fft_init(6, costab[0], sintab[0], fft_rev[0]);
    fft_init(7, costab[1], sintab[1], fft_rev[1]);
    mdct_init(256, xcos1[0], xsin1[0]);
    mdct_init(512, xcos1[1], xsin1[1]);
}
