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
 * @file mdct.h
 * MDCT header
 */

#ifndef MDCT_H
#define MDCT_H

#include "common.h"

struct A52Context;

typedef struct {
    int length;
    FLOAT *costab;
    FLOAT *sintab;
    int *revtab;
} FFTContext;

typedef struct {
    int length;
    FLOAT *xcos1;
    FLOAT *xsin1;
    FFTContext *fft;
    FLOAT *buffer;
    void *cbuffer;
} MDCTContext;

extern void mdct_init(struct A52Context *ctx);

extern void mdct_close(struct A52Context *ctx);

extern void mdct_512(struct A52Context *ctx, FLOAT *out, FLOAT *in);

extern void mdct_256(struct A52Context *ctx, FLOAT *out, FLOAT *in);

#endif /* MDCT_H */
