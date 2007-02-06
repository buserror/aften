/**
 * Aften: A/52 audio encoder
 * Copyright (c) 2006-2007 Justin Ruggles <justinruggles@bellsouth.net>
 *               2006-2007 Prakash Punnoor <prakash@punnoor.de>
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
 * @file aften.h
 * libaften public header
 */

#ifndef AFTEN_H
#define AFTEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "aften-types.h"

#if defined(_WIN32) && !defined(_XBOX)
 #if defined(AFTEN_BUILD_LIBRARY)
  #define AFTEN_API __declspec(dllexport)
 #else
  #define AFTEN_API __declspec(dllimport)
 #endif
#else
 #if defined(AFTEN_BUILD_LIBRARY) && defined(HAVE_GCC_VISIBILITY)
  #define AFTEN_API __attribute__((visibility("default")))
 #else
  #define AFTEN_API extern
 #endif
#endif

/* main encoding functions */

AFTEN_API const char *aften_get_version(void);

AFTEN_API void aften_set_defaults(AftenContext *s);

AFTEN_API int aften_encode_init(AftenContext *s);

AFTEN_API int aften_encode_frame(AftenContext *s, unsigned char *frame_buffer,
                                 void *samples);

AFTEN_API void aften_encode_close(AftenContext *s);

/* utility functions */

/**
 * Determines the proper A/52 acmod and lfe parameters based on the
 * number of channels and the WAVE_FORMAT_EXTENSIBLE channel mask.  If the
 * chmask value has the high bit set to 1 (e.g. 0xFFFFFFFF), then the default
 * plain WAVE channel selection is assumed.
 * On error, the acmod and lfe output params are set to -1 and the function
 * returns -1.  On success, the acmod and lfe params are set to appropriate
 * values and the function returns 0.
 */
AFTEN_API int aften_wav_channels_to_acmod(int ch, unsigned int chmask,
                                          int *acmod, int *lfe);

/**
 * Takes a channel-interleaved array of audio samples, where the channel order
 * is the default WAV order. The samples are rearranged to the proper A/52
 * channel order based on the acmod and lfe parameters.
 */
AFTEN_API void aften_remap_wav_to_a52(void *samples, int n, int ch,
                                      A52SampleFormat fmt, int acmod);

/**
 * Tells whether libaften was configured to use floats or doubles
 */
AFTEN_API FloatType aften_get_float_type(void);

#undef AFTEN_API

#if defined(__cplusplus)
}
#endif

#endif /* AFTEN_H */
