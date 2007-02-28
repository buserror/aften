/**
 * Aften: A/52 audio encoder
 * Copyright (c) 2006  Justin Ruggles <justinruggles@bellsouth.net>
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
 * @file wav.c
 * WAV decoder
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "wav.h"

#define RIFF_ID     0x46464952
#define WAVE_ID     0x45564157
#define FMT__ID     0x20746D66
#define DATA_ID     0x61746164

/**
 * Reads a 4-byte little-endian word from the input stream
 */
static inline uint32_t
read4le(FILE *fp)
{
    uint32_t x;
    fread(&x, 4, 1, fp);
    if(feof(fp)) return 0;
    return le2me_32(x);
}

/**
 * Reads a 2-byte little-endian word from the input stream
 */
static uint16_t
read2le(FILE *fp)
{
    uint16_t x;
    fread(&x, 2, 1, fp);
    if(feof(fp)) return 0;
    return le2me_16(x);
}

/**
 * Seeks to byte offset within file.
 * Limits the seek position or offset to signed 32-bit.
 * It also does slower forward seeking for streaming input.
 */
static inline int
aft_seek_set(WavFile *wf, uint64_t dest)
{
    int slow_seek = !(wf->seekable);

    if(wf->seekable) {
        if(dest <= INT32_MAX) {
            // destination is within first 2GB
            if(fseek(wf->fp, (long)dest, SEEK_SET)) return -1;
        } else {
            int64_t offset = (int64_t)dest - (int64_t)wf->filepos;
            if(offset >= INT32_MIN && offset <= INT32_MAX) {
                // offset is within +/- 2GB of file start
                if(fseek(wf->fp, (long)offset, SEEK_CUR)) return -1;
            } else {
                // absolute offset is more than 2GB
                if(offset < 0) {
                    fprintf(stderr, "error: backward seeking is limited to 2GB\n");
                    return -1;
                } else {
                    fprintf(stderr, "warning: forward seeking more than 2GB will be slow.\n");
                }
                slow_seek = 1;
            }
        }
    }
    if(slow_seek) {
        // do forward-only seek by reading data to temp buffer
        uint64_t offset;
        uint8_t buf[1024];
        int c=0;
        if(dest < wf->filepos) return -1;
        offset = dest - wf->filepos;
        while(offset > 1024) {
            fread(buf, 1, 1024, wf->fp);
            offset -= 1024;
        }
        while(offset-- > 0 && c != EOF) {
            c = fgetc(wf->fp);
        }
    }
    wf->filepos = dest;
    return 0;
}

static void
fmt_convert_u8_to_u8(void *dest_v, void *src_v, int n)
{
    memcpy(dest_v, src_v, n * sizeof(uint8_t));
}

static void
fmt_convert_s16_to_u8(void *dest_v, void *src_v, int n)
{
    uint8_t *dest = dest_v;
    int16_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] >> 8) + 128;
}

static void
fmt_convert_s20_to_u8(void *dest_v, void *src_v, int n)
{
    uint8_t *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] >> 12) + 128;
}

static void
fmt_convert_s24_to_u8(void *dest_v, void *src_v, int n)
{
    uint8_t *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] >> 16) + 128;
}

static void
fmt_convert_s32_to_u8(void *dest_v, void *src_v, int n)
{
    uint8_t *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] >> 24) + 128;
}

static void
fmt_convert_float_to_u8(void *dest_v, void *src_v, int n)
{
    uint8_t *dest = dest_v;
    float *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (uint8_t)CLIP(((src[i] * 128) + 128), 0, 255);
}

static void
fmt_convert_double_to_u8(void *dest_v, void *src_v, int n)
{
    uint8_t *dest = dest_v;
    double *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (uint8_t)CLIP(((src[i] * 128) + 128), 0, 255);
}

static void
fmt_convert_u8_to_s16(void *dest_v, void *src_v, int n)
{
    int16_t *dest = dest_v;
    uint8_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] - 128) << 8;
}

static void
fmt_convert_s16_to_s16(void *dest_v, void *src_v, int n)
{
    memcpy(dest_v, src_v, n * sizeof(int16_t));
}

static void
fmt_convert_s20_to_s16(void *dest_v, void *src_v, int n)
{
    int16_t *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] >> 4);
}

static void
fmt_convert_s24_to_s16(void *dest_v, void *src_v, int n)
{
    int16_t *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] >> 8);
}

static void
fmt_convert_s32_to_s16(void *dest_v, void *src_v, int n)
{
    int16_t *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] >> 16);
}

static void
fmt_convert_float_to_s16(void *dest_v, void *src_v, int n)
{
    int16_t *dest = dest_v;
    float *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (int16_t)CLIP((src[i] * 32768), -32768, 32767);
}

static void
fmt_convert_double_to_s16(void *dest_v, void *src_v, int n)
{
    int16_t *dest = dest_v;
    double *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (int16_t)CLIP((src[i] * 32768), -32768, 32767);
}

static void
fmt_convert_u8_to_s20(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    uint8_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] - 128) << 12;
}

static void
fmt_convert_s16_to_s20(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    int16_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] << 4);
}

static void
fmt_convert_s20_to_s20(void *dest_v, void *src_v, int n)
{
    memcpy(dest_v, src_v, n * sizeof(int32_t));
}

static void
fmt_convert_s24_to_s20(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] >> 4);
}

static void
fmt_convert_s32_to_s20(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] >> 12);
}

static void
fmt_convert_float_to_s20(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    float *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (int32_t)CLIP((src[i] * 524288), -524288, 524287);
}

static void
fmt_convert_double_to_s20(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    double *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (int32_t)CLIP((src[i] * 524288), -524288, 524287);
}

static void
fmt_convert_u8_to_s24(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    uint8_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] - 128) << 16;
}

static void
fmt_convert_s16_to_s24(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    int16_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] << 8);
}

static void
fmt_convert_s20_to_s24(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] << 4);
}

static void
fmt_convert_s24_to_s24(void *dest_v, void *src_v, int n)
{
    memcpy(dest_v, src_v, n * sizeof(int32_t));
}

static void
fmt_convert_s32_to_s24(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] >> 8);
}

static void
fmt_convert_float_to_s24(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    float *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (int32_t)CLIP((src[i] * 8388608), -8388608, 8388607);
}

static void
fmt_convert_double_to_s24(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    double *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (int32_t)CLIP((src[i] * 8388608), -8388608, 8388607);
}

static void
fmt_convert_u8_to_s32(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    uint8_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] - 128) << 24;
}

static void
fmt_convert_s16_to_s32(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    int16_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] << 16);
}

static void
fmt_convert_s20_to_s32(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] << 12);
}

static void
fmt_convert_s24_to_s32(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] << 8);
}

static void
fmt_convert_s32_to_s32(void *dest_v, void *src_v, int n)
{
    memcpy(dest_v, src_v, n * sizeof(int32_t));
}

static void
fmt_convert_float_to_s32(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    float *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (int32_t)(src[i] * 2147483648LL);
}

static void
fmt_convert_double_to_s32(void *dest_v, void *src_v, int n)
{
    int32_t *dest = dest_v;
    double *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (int32_t)(src[i] * 2147483648LL);
}

static void
fmt_convert_u8_to_float(void *dest_v, void *src_v, int n)
{
    float *dest = dest_v;
    uint8_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] - FCONST(128.0)) / FCONST(128.0);
}

static void
fmt_convert_s16_to_float(void *dest_v, void *src_v, int n)
{
    float *dest = dest_v;
    int16_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = src[i] / FCONST(32768.0);
}

static void
fmt_convert_s20_to_float(void *dest_v, void *src_v, int n)
{
    float *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = src[i] / FCONST(524288.0);
}

static void
fmt_convert_s24_to_float(void *dest_v, void *src_v, int n)
{
    float *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = src[i] / FCONST(8388608.0);
}

static void
fmt_convert_s32_to_float(void *dest_v, void *src_v, int n)
{
    float *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = src[i] / FCONST(2147483648.0);
}

static void
fmt_convert_float_to_float(void *dest_v, void *src_v, int n)
{
    memcpy(dest_v, src_v, n * sizeof(float));
}

static void
fmt_convert_double_to_float(void *dest_v, void *src_v, int n)
{
    float *dest = dest_v;
    double *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (float)src[i];
}

static void
fmt_convert_u8_to_double(void *dest_v, void *src_v, int n)
{
    double *dest = dest_v;
    uint8_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = (src[i] - FCONST(128.0)) / FCONST(128.0);
}

static void
fmt_convert_s16_to_double(void *dest_v, void *src_v, int n)
{
    double *dest = dest_v;
    int16_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = src[i] / FCONST(32768.0);
}

static void
fmt_convert_s20_to_double(void *dest_v, void *src_v, int n)
{
    double *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = src[i] / FCONST(524288.0);
}

static void
fmt_convert_s24_to_double(void *dest_v, void *src_v, int n)
{
    double *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = src[i] / FCONST(8388608.0);
}

static void
fmt_convert_s32_to_double(void *dest_v, void *src_v, int n)
{
    double *dest = dest_v;
    int32_t *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = src[i] / FCONST(2147483648.0);
}

static void
fmt_convert_float_to_double(void *dest_v, void *src_v, int n)
{
    double *dest = dest_v;
    float *src = src_v;
    int i;

    for(i=0; i<n; i++)
        dest[i] = src[i];
}

static void
fmt_convert_double_to_double(void *dest_v, void *src_v, int n)
{
    memcpy(dest_v, src_v, n * sizeof(double));
}

static void
set_fmt_convert_from_u8(WavFile *wf)
{
    enum WavSampleFormat fmt = wf->read_format;

    if(fmt == WAV_SAMPLE_FMT_U8)
        wf->fmt_convert = fmt_convert_u8_to_u8;
    else if(fmt == WAV_SAMPLE_FMT_S16)
        wf->fmt_convert = fmt_convert_u8_to_s16;
    else if(fmt == WAV_SAMPLE_FMT_S20)
        wf->fmt_convert = fmt_convert_u8_to_s20;
    else if(fmt == WAV_SAMPLE_FMT_S24)
        wf->fmt_convert = fmt_convert_u8_to_s24;
    else if(fmt == WAV_SAMPLE_FMT_S32)
        wf->fmt_convert = fmt_convert_u8_to_s32;
    else if(fmt == WAV_SAMPLE_FMT_FLT)
        wf->fmt_convert = fmt_convert_u8_to_float;
    else if(fmt == WAV_SAMPLE_FMT_DBL)
        wf->fmt_convert = fmt_convert_u8_to_double;
}

static void
set_fmt_convert_from_s16(WavFile *wf)
{
    enum WavSampleFormat fmt = wf->read_format;

    if(fmt == WAV_SAMPLE_FMT_U8)
        wf->fmt_convert = fmt_convert_s16_to_u8;
    else if(fmt == WAV_SAMPLE_FMT_S16)
        wf->fmt_convert = fmt_convert_s16_to_s16;
    else if(fmt == WAV_SAMPLE_FMT_S20)
        wf->fmt_convert = fmt_convert_s16_to_s20;
    else if(fmt == WAV_SAMPLE_FMT_S24)
        wf->fmt_convert = fmt_convert_s16_to_s24;
    else if(fmt == WAV_SAMPLE_FMT_S32)
        wf->fmt_convert = fmt_convert_s16_to_s32;
    else if(fmt == WAV_SAMPLE_FMT_FLT)
        wf->fmt_convert = fmt_convert_s16_to_float;
    else if(fmt == WAV_SAMPLE_FMT_DBL)
        wf->fmt_convert = fmt_convert_s16_to_double;
}

static void
set_fmt_convert_from_s20(WavFile *wf)
{
    enum WavSampleFormat fmt = wf->read_format;

    if(fmt == WAV_SAMPLE_FMT_U8)
        wf->fmt_convert = fmt_convert_s20_to_u8;
    else if(fmt == WAV_SAMPLE_FMT_S16)
        wf->fmt_convert = fmt_convert_s20_to_s16;
    else if(fmt == WAV_SAMPLE_FMT_S20)
        wf->fmt_convert = fmt_convert_s20_to_s20;
    else if(fmt == WAV_SAMPLE_FMT_S24)
        wf->fmt_convert = fmt_convert_s20_to_s24;
    else if(fmt == WAV_SAMPLE_FMT_S32)
        wf->fmt_convert = fmt_convert_s20_to_s32;
    else if(fmt == WAV_SAMPLE_FMT_FLT)
        wf->fmt_convert = fmt_convert_s20_to_float;
    else if(fmt == WAV_SAMPLE_FMT_DBL)
        wf->fmt_convert = fmt_convert_s20_to_double;
}

static void
set_fmt_convert_from_s24(WavFile *wf)
{
    enum WavSampleFormat fmt = wf->read_format;

    if(fmt == WAV_SAMPLE_FMT_U8)
        wf->fmt_convert = fmt_convert_s24_to_u8;
    else if(fmt == WAV_SAMPLE_FMT_S16)
        wf->fmt_convert = fmt_convert_s24_to_s16;
    else if(fmt == WAV_SAMPLE_FMT_S20)
        wf->fmt_convert = fmt_convert_s24_to_s20;
    else if(fmt == WAV_SAMPLE_FMT_S24)
        wf->fmt_convert = fmt_convert_s24_to_s24;
    else if(fmt == WAV_SAMPLE_FMT_S32)
        wf->fmt_convert = fmt_convert_s24_to_s32;
    else if(fmt == WAV_SAMPLE_FMT_FLT)
        wf->fmt_convert = fmt_convert_s24_to_float;
    else if(fmt == WAV_SAMPLE_FMT_DBL)
        wf->fmt_convert = fmt_convert_s24_to_double;
}

static void
set_fmt_convert_from_s32(WavFile *wf)
{
    enum WavSampleFormat fmt = wf->read_format;

    if(fmt == WAV_SAMPLE_FMT_U8)
        wf->fmt_convert = fmt_convert_s32_to_u8;
    else if(fmt == WAV_SAMPLE_FMT_S16)
        wf->fmt_convert = fmt_convert_s32_to_s16;
    else if(fmt == WAV_SAMPLE_FMT_S20)
        wf->fmt_convert = fmt_convert_s32_to_s20;
    else if(fmt == WAV_SAMPLE_FMT_S24)
        wf->fmt_convert = fmt_convert_s32_to_s24;
    else if(fmt == WAV_SAMPLE_FMT_S32)
        wf->fmt_convert = fmt_convert_s32_to_s32;
    else if(fmt == WAV_SAMPLE_FMT_FLT)
        wf->fmt_convert = fmt_convert_s32_to_float;
    else if(fmt == WAV_SAMPLE_FMT_DBL)
        wf->fmt_convert = fmt_convert_s32_to_double;
}

static void
set_fmt_convert_from_float(WavFile *wf)
{
    enum WavSampleFormat fmt = wf->read_format;

    if(fmt == WAV_SAMPLE_FMT_U8)
        wf->fmt_convert = fmt_convert_float_to_u8;
    else if(fmt == WAV_SAMPLE_FMT_S16)
        wf->fmt_convert = fmt_convert_float_to_s16;
    else if(fmt == WAV_SAMPLE_FMT_S20)
        wf->fmt_convert = fmt_convert_float_to_s20;
    else if(fmt == WAV_SAMPLE_FMT_S24)
        wf->fmt_convert = fmt_convert_float_to_s24;
    else if(fmt == WAV_SAMPLE_FMT_S32)
        wf->fmt_convert = fmt_convert_float_to_s32;
    else if(fmt == WAV_SAMPLE_FMT_FLT)
        wf->fmt_convert = fmt_convert_float_to_float;
    else if(fmt == WAV_SAMPLE_FMT_DBL)
        wf->fmt_convert = fmt_convert_float_to_double;
}

static void
set_fmt_convert_from_double(WavFile *wf)
{
    enum WavSampleFormat fmt = wf->read_format;

    if(fmt == WAV_SAMPLE_FMT_U8)
        wf->fmt_convert = fmt_convert_double_to_u8;
    else if(fmt == WAV_SAMPLE_FMT_S16)
        wf->fmt_convert = fmt_convert_double_to_s16;
    else if(fmt == WAV_SAMPLE_FMT_S20)
        wf->fmt_convert = fmt_convert_double_to_s20;
    else if(fmt == WAV_SAMPLE_FMT_S24)
        wf->fmt_convert = fmt_convert_double_to_s24;
    else if(fmt == WAV_SAMPLE_FMT_S32)
        wf->fmt_convert = fmt_convert_double_to_s32;
    else if(fmt == WAV_SAMPLE_FMT_FLT)
        wf->fmt_convert = fmt_convert_double_to_float;
    else if(fmt == WAV_SAMPLE_FMT_DBL)
        wf->fmt_convert = fmt_convert_double_to_double;
}

/**
 * Initializes WavFile structure using the given input file pointer.
 * Examines the wave header to get audio information and has the file
 * pointer aligned at start of data when it exits.
 * Returns non-zero value if an error occurs.
 */
int
wavfile_init(WavFile *wf, FILE *fp, enum WavSampleFormat read_format)
{
    int id, found_fmt, found_data;
    uint32_t chunksize;

    if(wf == NULL || fp == NULL) {
        fprintf(stderr, "null input to wav reader\n");
        return -1;
    }

    memset(wf, 0, sizeof(WavFile));
    wf->fp = fp;
    wf->read_format = read_format;

    // attempt to get file size
    wf->file_size = 0;
#ifdef _WIN32
    // in Windows, don't try to detect seeking support for stdin
    if(fp != stdin) {
        wf->seekable = !fseek(fp, 0, SEEK_END);
    }
#else
    wf->seekable = !fseek(fp, 0, SEEK_END);
#endif
    if(wf->seekable) {
        // TODO: portable 64-bit ftell
        long fs = ftell(fp);
        // ftell should return an error if value cannot fit in return type
        if(fs < 0) {
            fprintf(stderr, "Warning, unsupported file size.\n");
            wf->file_size = 0;
        } else {
            wf->file_size = (uint64_t)fs;
        }
        fseek(fp, 0, SEEK_SET);
    }
    wf->filepos = 0;

    // read RIFF id. ignore size.
    id = read4le(fp);
    wf->filepos += 4;
    if(id != RIFF_ID) {
        fprintf(stderr, "invalid RIFF id in wav header\n");
        return -1;
    }
    read4le(fp);
    wf->filepos += 4;

    // read WAVE id. ignore size.
    id = read4le(fp);
    wf->filepos += 4;
    if(id != WAVE_ID) {
        fprintf(stderr, "invalid WAVE id in wav header\n");
        return -1;
    }

    // read all header chunks. skip unknown chunks.
    found_data = found_fmt = 0;
    while(!found_data) {
        id = read4le(fp);
        wf->filepos += 4;
        chunksize = read4le(fp);
        wf->filepos += 4;
        if(id == 0 || chunksize == 0) {
            fprintf(stderr, "invalid or empty chunk in wav header\n");
            return -1;
        }
        switch(id) {
            case FMT__ID:
                if(chunksize < 16) {
                    fprintf(stderr, "invalid fmt chunk in wav header\n");
                    return -1;
                }
                wf->format = read2le(fp);
                wf->filepos += 2;
                wf->channels = read2le(fp);
                wf->filepos += 2;
                if(wf->channels == 0) {
                    fprintf(stderr, "invalid number of channels in wav header\n");
                    return -1;
                }
                wf->sample_rate = read4le(fp);
                wf->filepos += 4;
                if(wf->sample_rate == 0) {
                    fprintf(stderr, "invalid sample rate in wav header\n");
                    return -1;
                }
                read4le(fp);
                wf->filepos += 4;
                wf->block_align = read2le(fp);
                wf->filepos += 2;
                wf->bit_width = read2le(fp);
                wf->filepos += 2;
                if(wf->bit_width == 0) {
                    fprintf(stderr, "invalid sample bit width in wav header\n");
                    return -1;
                }
                chunksize -= 16;

                // WAVE_FORMAT_EXTENSIBLE data
                wf->ch_mask = 0;
                if(wf->format == WAVE_FORMAT_EXTENSIBLE && chunksize >= 10) {
                    read4le(fp);    // skip CbSize and ValidBitsPerSample
                    wf->filepos += 4;
                    wf->ch_mask = read4le(fp);
                    wf->filepos += 4;
                    wf->format = read2le(fp);
                    wf->filepos += 2;
                    chunksize -= 10;
                }

                if(wf->format == WAVE_FORMAT_PCM || wf->format == WAVE_FORMAT_IEEEFLOAT) {
                    // override block alignment in header for uncompressed pcm
                    wf->block_align = MAX(1, ((wf->bit_width + 7) >> 3) * wf->channels);
                }
                wf->bytes_per_sec = wf->sample_rate * wf->block_align;

                // make up channel mask if not using WAVE_FORMAT_EXTENSIBLE
                // or if ch_mask is set to zero (unspecified configuration)
                // TODO: select default configurations for >6 channels
                if(wf->ch_mask == 0) {
                    switch(wf->channels) {
                        case 1: wf->ch_mask = 0x04;  break;
                        case 2: wf->ch_mask = 0x03;  break;
                        case 3: wf->ch_mask = 0x07;  break;
                        case 4: wf->ch_mask = 0x107; break;
                        case 5: wf->ch_mask = 0x37;  break;
                        case 6: wf->ch_mask = 0x3F;  break;
                    }
                }

                // skip any leftover bytes in fmt chunk
                if(aft_seek_set(wf, wf->filepos + chunksize)) {
                    fprintf(stderr, "error seeking in wav file\n");
                    return -1;
                }
                found_fmt = 1;
                break;
            case DATA_ID:
                if(!found_fmt) return -1;
                wf->data_size = chunksize;
                wf->data_start = wf->filepos;
                if(wf->seekable && wf->file_size > 0) {
                    // limit data size to end-of-file
                    wf->data_size = MIN(wf->data_size, wf->file_size - wf->data_start);
                }
                wf->samples = (wf->data_size / wf->block_align);
                found_data = 1;
                break;
            default:
                // skip unknown chunk
                if(aft_seek_set(wf, wf->filepos + chunksize)) {
                    fprintf(stderr, "error seeking in wav file\n");
                    return -1;
                }
        }
    }

    // set audio data format based on bit depth and twocc format code
    wf->source_format = WAV_SAMPLE_FMT_UNKNOWN;
    if(wf->format == WAVE_FORMAT_PCM || wf->format == WAVE_FORMAT_IEEEFLOAT) {
        switch(wf->bit_width) {
        case 8:
            wf->source_format = WAV_SAMPLE_FMT_U8;
            set_fmt_convert_from_u8(wf);
            break;
        case 16:
            wf->source_format = WAV_SAMPLE_FMT_S16;
            set_fmt_convert_from_s16(wf);
            break;
        case 20:
            wf->source_format = WAV_SAMPLE_FMT_S20;
            set_fmt_convert_from_s20(wf);
            break;
        case 24:
            wf->source_format = WAV_SAMPLE_FMT_S24;
            set_fmt_convert_from_s24(wf);
            break;
        case 32:
            if(wf->format == WAVE_FORMAT_IEEEFLOAT) {
                wf->source_format = WAV_SAMPLE_FMT_FLT;
                set_fmt_convert_from_float(wf);
            } else if(wf->format == WAVE_FORMAT_PCM) {
                wf->source_format = WAV_SAMPLE_FMT_S32;
                set_fmt_convert_from_s32(wf);
            }
            break;
        case 64:
            if(wf->format == WAVE_FORMAT_IEEEFLOAT) {
                wf->source_format = WAV_SAMPLE_FMT_DBL;
                set_fmt_convert_from_double(wf);
            }
            break;
        }
    }

    return 0;
}

/**
 * Reads audio samples to the output buffer.
 * Output is channel-interleaved, native byte order.
 * Only up to WAVE_MAX_READ samples can be read in one call.
 * The output sample format depends on the value of wf->read_format.
 * Returns number of samples read or -1 on error.
 */
int
wavfile_read_samples(WavFile *wf, void *output, int num_samples)
{
    uint8_t *buffer;
    uint8_t *read_buffer;
    uint32_t bytes_needed, buffer_size;
    int nr, i, j, bps, nsmp;

    // check input and limit number of samples
    if(wf == NULL || wf->fp == NULL || output == NULL || wf->fmt_convert == NULL) {
        fprintf(stderr, "null input to wavfile_read_samples\n");
        return -1;
    }
    if(wf->block_align <= 0) {
        fprintf(stderr, "invalid block_align\n");
        return -1;
    }
    num_samples = MIN(num_samples, WAV_MAX_READ);

    // calculate number of bytes to read, being careful not to read past
    // the end of the data chunk
    bytes_needed = wf->block_align * num_samples;
    if(!wf->read_to_eof) {
        if((wf->filepos + bytes_needed) >= (wf->data_start + wf->data_size)) {
            bytes_needed = (uint32_t)((wf->data_start + wf->data_size) - wf->filepos);
            num_samples = bytes_needed / wf->block_align;
        }
    }
    if(num_samples <= 0) return 0;

    // allocate temporary buffer for raw input data
    bps = wf->block_align / wf->channels;
    buffer_size = (bps != 3) ? bytes_needed : num_samples * sizeof(int32_t) *  wf->channels;
    buffer = calloc(buffer_size, 1);
    read_buffer = buffer + (buffer_size - bytes_needed);

    // read raw audio samples from input stream into temporary buffer
    nr = fread(read_buffer, wf->block_align, num_samples, wf->fp);
    if (nr <= 0)
        return nr;
    wf->filepos += nr * wf->block_align;
    nsmp = nr * wf->channels;

    // do any necessary conversion based on source_format and read_format
    // also do byte swapping on big-endian systems since wave data is always
    // in little-endian order.
    switch (bps) {
#ifdef WORDS_BIGENDIAN
    case 2:
        {
            uint16_t *buf16 = (uint16_t *)buffer;
            for(i=0; i<nsmp; i++) {
                buf16[i] = bswap_16(buf16[i]);
            }
        }
        break;
#endif
    case 3:
        {
            int32_t *input = (int32_t*)buffer;
            int unused_bits = 32 - wf->bit_width;
            int32_t v;
            // last sample could cause invalid mem access for little endians
            // but instead of complex logic use simple solution...
            for(i=0,j=0; i<(nsmp-1)*bps; i+=bps,j++) {
#ifdef WORDS_BIGENDIAN
                v = read_buffer[i] | (read_buffer[i+1] << 8) | (read_buffer[i+2] << 16);
#else
                v = *(int32_t*)(read_buffer + i);
#endif
                v <<= unused_bits; // clear unused high bits
                v >>= unused_bits; // sign extend
                input[j] = v;
            }
            v = read_buffer[i] | (read_buffer[i+1] << 8) | (read_buffer[i+2] << 16);
            v <<= unused_bits; // clear unused high bits
            v >>= unused_bits; // sign extend
            input[j] = v;
        }
        break;
#ifdef WORDS_BIGENDIAN
    case 4:
        {
            uint32_t *buf32 = (uint32_t *)buffer;
            for(i=0; i<nsmp; i++) {
                buf32[i] = bswap_32(buf32[i]);
            }
        }
        break;
    default:
        {
            uint64_t *buf64 = (uint64_t *)buffer;
            for(i=0; i<nsmp; i++) {
                buf64[i] = bswap_64(buf64[i]);
            }
        }
        break;
#endif
    }
    wf->fmt_convert(output, buffer, nsmp);

    // free temporary buffer
    free(buffer);

    return nr;
}

/**
 * Seeks to sample offset.
 * Syntax works like fseek. use WAV_SEEK_SET, WAV_SEEK_CUR, or WAV_SEEK_END
 * for the whence value.  Returns -1 on error, 0 otherwise.
 */
int
wavfile_seek_samples(WavFile *wf, int64_t offset, int whence)
{
    int64_t byte_offset;
    uint64_t newpos, fpos, dst, dsz;

    if(wf == NULL || wf->fp == NULL) return -1;
    if(wf->block_align <= 0) return -1;
    if(wf->filepos < wf->data_start) return -1;
    if(wf->data_size == 0) return 0;

    fpos = wf->filepos;
    dst = wf->data_start;
    dsz = wf->data_size;
    byte_offset = offset;
    byte_offset *= wf->block_align;

    // calculate new destination within file
    switch(whence) {
        case WAV_SEEK_SET:
            newpos = dst + CLIP(byte_offset, 0, (int64_t)dsz);
            break;
        case WAV_SEEK_CUR:
            newpos = fpos - MIN(-byte_offset, (int64_t)(fpos - dst));
            newpos = MIN(newpos, dst + dsz);
            break;
        case WAV_SEEK_END:
            newpos = dst + dsz - CLIP(byte_offset, 0, (int64_t)dsz);
            break;
        default: return -1;
    }

    // seek to the destination point
    if(aft_seek_set(wf, newpos)) return -1;

    return 0;
}

/**
 * Seeks to time offset, in milliseconds, based on the audio sample rate.
 * Syntax works like fseek. use WAV_SEEK_SET, WAV_SEEK_CUR, or WAV_SEEK_END
 * for the whence value.  Returns -1 on error, 0 otherwise.
 */
int
wavfile_seek_time_ms(WavFile *wf, int64_t offset, int whence)
{
    int64_t samples;
    if(wf == NULL) return -1;
    samples = offset * wf->sample_rate / 1000;
    return wavfile_seek_samples(wf, samples, whence);
}

/**
 * Returns the current stream position, in samples.
 * Returns -1 on error.
 */
uint64_t
wavfile_position(WavFile *wf)
{
    uint64_t cur;

    if(wf == NULL) return -1;
    if(wf->block_align <= 0) return -1;
    if(wf->data_start == 0 || wf->data_size == 0) return 0;

    cur = (wf->filepos - wf->data_start) / wf->block_align;
    return cur;
}

/**
 * Returns the current stream position, in milliseconds.
 * Returns -1 on error.
 */
uint64_t
wavfile_position_time_ms(WavFile *wf)
{
    return (wavfile_position(wf) * 1000 / wf->sample_rate);
}

/**
 * Prints out a description of the wav format to the specified
 * output stream.
 */
void
wavfile_print(FILE *st, WavFile *wf)
{
    char *type, *chan;
    if(st == NULL || wf == NULL) return;
    type = "?";
    chan = "?-channel";
    if(wf->format == WAVE_FORMAT_PCM) {
        if(wf->bit_width > 8) type = "Signed";
        else type = "Unsigned";
    } else if(wf->format == WAVE_FORMAT_IEEEFLOAT) {
        type = "Floating-point";
    } else {
        type = "[unsupported type]";
    }
    if(wf->ch_mask & 0x08) {
        switch(wf->channels-1) {
            case 1: chan = "1.1-channel"; break;
            case 2: chan = "2.1-channel"; break;
            case 3: chan = "3.1-channel"; break;
            case 4: chan = "4.1-channel"; break;
            case 5: chan = "5.1-channel"; break;
            default: chan = "multi-channel with LFE"; break;
        }
    } else {
        switch(wf->channels) {
            case 1: chan = "mono"; break;
            case 2: chan = "stereo"; break;
            case 3: chan = "3-channel"; break;
            case 4: chan = "4-channel"; break;
            case 5: chan = "5-channel"; break;
            case 6: chan = "6-channel"; break;
            default: chan = "multi-channel"; break;
        }
    }
    fprintf(st, "%s %d-bit %d Hz %s\n", type, wf->bit_width, wf->sample_rate,
            chan);
}
