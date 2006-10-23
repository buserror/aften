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
 * @file common.h
 * Common header file
 */

#ifndef COMMON_H
#define COMMON_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#else
#if defined(_WIN32) && defined(_MSC_VER)
typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#endif
#endif /* EMULATE_INTTYPES */

#if __GNUC__ && !__INTEL_COMPILER
#define ALIGN16(x) x __attribute__((aligned(16)))
#else
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
#define ALIGN16(x) __declspec(align(16)) x
#else
#define ALIGN16(x) x
#endif
#endif

#ifdef _WIN32
#define CDECL __cdecl
#ifdef _MSC_VER
#define inline __inline
#endif /* _MSC_VER */
#else
#define CDECL
#endif

#ifdef CONFIG_DOUBLE
typedef double FLOAT;
#define FCONST(X) (X)
#define AFT_COS cos
#define AFT_SIN sin
#define AFT_TAN tan
#define AFT_LOG10 log10
#define AFT_EXP exp
#define AFT_FABS fabs
#define AFT_SQRT sqrt
#define AFT_EXP exp
#define AFT_EXP10 exp10
#else
typedef float FLOAT;
#define FCONST(X) (X##f)
#define AFT_COS cosf
#define AFT_SIN sinf
#define AFT_TAN tanf
#define AFT_LOG10 log10f
#define AFT_FABS fabsf
#define AFT_SQRT sqrtf
#define AFT_EXP expf
#define AFT_EXP10 exp10f
#endif

#define AFT_PI  FCONST(3.14159265358979323846)
#define AFT_SQRT2 FCONST(1.41421356237309504880)
#define AFT_LN10 FCONST(2.30258509299404568402)

#ifndef HAVE_EXP10
#undef AFT_EXP10
#define AFT_EXP10(x) AFT_EXP((x) * AFT_LN10)
#endif

#define ABS(a) ((a) >= 0 ? (a) : (-(a)))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) > (b) ? (b) : (a))
#define CLIP(x,min,max) MAX(MIN((x), (max)), (min))

static const uint8_t log2tab[256] = {
    0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
};

static inline int
log2i(uint32_t v)
{
    int n = 0;
    if(v & 0xffff0000){ v >>= 16; n += 16; }
    if(v & 0xff00){ v >>= 8; n += 8; }
    n += log2tab[v];
    return n;
}

#include "bswap.h"

#endif /* COMMON_H */
