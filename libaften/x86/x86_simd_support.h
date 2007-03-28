/********************************************************************************
 * Copyright (C) 2005-2007 by Prakash Punnoor                                   *
 * prakash@punnoor.de                                                           *
 *                                                                              *
 * This library is free software; you can redistribute it and/or                *
 * modify it under the terms of the GNU Lesser General Public                   *
 * License as published by the Free Software Foundation; either                 *
 * version 2 of the License                                                     *
 *                                                                              *
 * This library is distributed in the hope that it will be useful,              *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of               *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU            *
 * Lesser General Public License for more details.                              *
 *                                                                              *
 * You should have received a copy of the GNU Lesser General Public             *
 * License along with this library; if not, write to the Free Software          *
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA *
 ********************************************************************************/
#ifndef X86_SIMD_SUPPORT_H
#define X86_SIMD_SUPPORT_H

#include "mem.h"

#include <xmmintrin.h>

#ifdef USE_SSE3
#include <pmmintrin.h>

#ifdef EMU_CASTSI128
#define _mm_castsi128_ps(X) ((__m128)(X))
#endif

#define _mm_lddqu_ps(x) _mm_castsi128_ps(_mm_lddqu_si128((__m128i*)(x)))
#else

#define _mm_lddqu_ps(x) _mm_loadu_ps(x)
#endif /* USE_SSE3 */

#ifndef _MM_ALIGN16
#define _MM_ALIGN16  __attribute__((aligned(16)))
#endif

#define PM128(x) (*(__m128*)(x))

union __m64ui {
    unsigned int ui[4];
    __m64 v;
};

union __m128ui {
    unsigned int ui[4];
    __m128 v;
};

union __m128f {
    float f[4];
    __m128 v;
};

union __m128iui {
    unsigned int ui[4];
    __m128i v;
};

#endif /* X86_SIMD_SUPPORT_H */
