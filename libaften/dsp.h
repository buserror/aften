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
 * @file dsp.h
 * Signal processing header
 */

#ifndef DSP_H
#define DSP_H

struct A52Context;

typedef struct {
    int length;
    double *costab;
    double *sintab;
    int *revtab;
} FFTContext;

typedef struct {
    int length;
    double *xcos1;
    double *xsin1;
    FFTContext *fft;
    double *buffer;
    void *cbuffer;
} MDCTContext;

extern void dsp_init(struct A52Context *ctx);

extern void dsp_close(struct A52Context *ctx);

extern void apply_a52_window(double *samples);

extern void mdct512(struct A52Context *ctx, double *out, double *in);

extern void mdct256(struct A52Context *ctx, double *out, double *in);

#endif /* DSP_H */
