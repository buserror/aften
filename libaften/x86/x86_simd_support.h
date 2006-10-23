/***************************************************************************
 *   Copyright (C) 2006 by Prakash Punnoor                                 *
 *   prakash@punnoor.de                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#ifndef X86_SIMD_SUPPORT_H
#define X86_SIMD_SUPPORT_H

#include <xmmintrin.h>

#ifdef EMU_MM_MALLOC
/* FIXME: check for posix_memalign */
static inline void*
_mm_malloc(size_t size, size_t alignment)
{
    void *mem;

    if (posix_memalign(&mem, alignment, size))
        return NULL;

    return mem;
}

#define _mm_free(X) free(X)
#endif /* EMU_MM_MALLOC */

#ifdef USE_SSE3
#include <pmmintrin.h>

#ifdef EMU_CASTSI128
#define _mm_castsi128_ps(X) ((__m128)(X))
#endif

#define _mm_lddqu_ps(x) _mm_castsi128_ps(_mm_lddqu_si128((__m128i*)(x)))
#else

#define _mm_lddqu_ps(x) _mm_loadu_ps(x)
#endif /* USE_SSE3 */

#define cPI3_8 .38268343236508977175F
#define cPI2_8 .70710678118654752441F
#define cPI1_8 .92387953251128675613F

#ifndef _MM_ALIGN16
#define _MM_ALIGN16  __attribute__((aligned(16)))
#endif
#define aligned_malloc(X) _mm_malloc(X,16)

#define PM128(x) (*(__m128*)(x))

union __m128ui {
    unsigned int ui[4];
    __m128 v;
};

union __m128f {
    float f[4];
    __m128 v;
};

#endif /* X86_SIMD_SUPPORT_H */
