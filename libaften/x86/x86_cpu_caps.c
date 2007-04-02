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
#include <string.h>
#include <stdlib.h>
#include "x86_cpu_caps.h"

/* caps1 */
#define MMX_BIT             23
#define SSE_BIT             25
#define SSE2_BIT            26

/* caps2 */
#define SSE3_BIT             0
#define SSSE3_BIT            9

/* caps3 */
#define AMD_3DNOW_BIT       31
#define AMD_3DNOWEXT_BIT    30
#define AMD_SSE_MMX_BIT     22
#define CYRIX_MMXEXT_BIT    24

#ifdef __INTEL_COMPILER
#pragma warning(disable : 1419)
#endif
int CDECL cpu_caps_detect_x86(uint32_t* caps1, uint32_t* caps2, uint32_t* caps3);

static struct x86cpu_caps_s x86cpu_caps_compile = { 0, 0, 0, 0, 0, 0, 0, 0, 0};
static struct x86cpu_caps_s x86cpu_caps_detect = { 1, 1, 1, 1, 1, 1, 1, 1, 1};
struct x86cpu_caps_s x86cpu_caps_use = { 0, 0, 0, 0, 0, 0, 0, 0, 0};

void cpu_caps_detect(void)
{
    /* compiled in SIMD routines */
#ifdef HAVE_MMX
    x86cpu_caps_compile.mmx = 1;
#endif
#ifdef HAVE_SSE
    x86cpu_caps_compile.sse = 1;
#endif
#ifdef HAVE_SSE2
    x86cpu_caps_compile.sse2 = 1;
#endif
#ifdef HAVE_SSE3
    x86cpu_caps_compile.sse3 = 1;
#endif
#ifdef HAVE_SSSE3
    x86cpu_caps_compile.ssse3 = 1;
#endif
#ifdef HAVE_3DNOW
    x86cpu_caps_compile.amd_3dnow = 1;
#endif
#ifdef HAVE_SSE_MMX
    x86cpu_caps_compile.amd_sse_mmx = 1;
#endif
#ifdef HAVE_3DNOWEXT
    x86cpu_caps_compile.amd_3dnowext = 1;
#endif
    /* end compiled in SIMD routines */

    /* runtime detection */
#ifdef HAVE_CPU_CAPS_DETECTION
    {
        uint32_t caps1, caps2, caps3;

        if (cpu_caps_detect_x86(&caps1, &caps2, &caps3)) {

            x86cpu_caps_detect.mmx          = (caps1 >> MMX_BIT) & 1;
            x86cpu_caps_detect.sse          = (caps1 >> SSE_BIT) & 1;
            x86cpu_caps_detect.sse2         = (caps1 >> SSE2_BIT) & 1;

            x86cpu_caps_detect.sse3         = (caps2 >> SSE3_BIT) & 1;
            x86cpu_caps_detect.ssse3         = (caps2 >> SSSE3_BIT) & 1;

            x86cpu_caps_detect.amd_3dnow    = (caps3 >> AMD_3DNOW_BIT) & 1;
            x86cpu_caps_detect.amd_3dnowext = (caps3 >> AMD_3DNOWEXT_BIT) & 1;
            x86cpu_caps_detect.amd_sse_mmx  = (caps3 >> AMD_SSE_MMX_BIT) & 1;
            /* FIXME: For Cyrix MMXEXT detect Cyrix CPU first! */
            /*
            x86cpu_caps.cyrix_mmxext = (caps3 >> CYRIX_MMXEXT_BIT) & 1;
            */
        }
    }
#endif /*HAVE_CPU_CAPS_DETECTION*/
    /* end runtime detection */

    x86cpu_caps_use.mmx          = x86cpu_caps_detect.mmx          & x86cpu_caps_compile.mmx;
    x86cpu_caps_use.sse          = x86cpu_caps_detect.sse          & x86cpu_caps_compile.sse;
    x86cpu_caps_use.sse2         = x86cpu_caps_detect.sse2         & x86cpu_caps_compile.sse2;
    x86cpu_caps_use.sse3         = x86cpu_caps_detect.sse3         & x86cpu_caps_compile.sse3;
    x86cpu_caps_use.ssse3         = x86cpu_caps_detect.ssse3         & x86cpu_caps_compile.ssse3;
    x86cpu_caps_use.amd_3dnow    = x86cpu_caps_detect.amd_3dnow    & x86cpu_caps_compile.amd_3dnow;
    x86cpu_caps_use.amd_3dnowext = x86cpu_caps_detect.amd_3dnowext & x86cpu_caps_compile.amd_3dnowext;
    x86cpu_caps_use.amd_sse_mmx  = x86cpu_caps_detect.amd_sse_mmx  & x86cpu_caps_compile.amd_sse_mmx;
}

