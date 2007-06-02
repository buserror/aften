/**
 * Aften: A/52 audio encoder
 * Copyright (c) 2006 Justin Ruggles
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
 * @file wav.h
 * WAV decoder header
 */

#ifndef WAV_H
#define WAV_H

#include "common.h"

#include <stdio.h>

/* supported TWOCC formats */
#define WAVE_FORMAT_PCM         0x0001
#define WAVE_FORMAT_IEEEFLOAT   0x0003
#define WAVE_FORMAT_EXTENSIBLE  0xFFFE

/* "whence" values for seek functions */
#define WAV_SEEK_SET 0
#define WAV_SEEK_CUR 1
#define WAV_SEEK_END 2

/* maximum single read size: 5 sec at 48kHz */
#define WAV_MAX_READ 240000

/* supported raw audio sample formats */
enum WavSampleFormat {
    WAV_SAMPLE_FMT_UNKNOWN = -1,
    WAV_SAMPLE_FMT_U8 = 0,
    WAV_SAMPLE_FMT_S16,
    WAV_SAMPLE_FMT_S20,
    WAV_SAMPLE_FMT_S24,
    WAV_SAMPLE_FMT_S32,
    WAV_SAMPLE_FMT_FLT,
    WAV_SAMPLE_FMT_DBL,
};

/* main decoder context */
typedef struct WavFile {
    // private fields. do not modify.
    void (*fmt_convert)(void *dest_v, void *src_v, int n);
    FILE *fp;
    uint64_t filepos;
    int seekable;
    uint64_t file_size;
    uint64_t data_start;
    uint64_t data_size;
    uint64_t samples;
    uint16_t format;
    int channels;
    uint32_t ch_mask;
    int sample_rate;
    uint64_t bytes_per_sec;
    int block_align;
    int bit_width;
    int read_to_eof;
    enum WavSampleFormat source_format;

    // public field set by user prior to calling wavfile_init.
    // this is the data format in which samples are returned by
    // wavfile_read_samples.
    enum WavSampleFormat read_format;
} WavFile;

/**
 * Initializes WavFile structure using the given input file pointer.
 * Examines the wave header to get audio information and has the file
 * pointer aligned at start of data when it exits.
 * Returns non-zero value if an error occurs.
 */
extern int wavfile_init(WavFile *wf, FILE *fp, enum WavSampleFormat read_format);

/**
 * Reads audio samples to the output buffer.
 * Output is channel-interleaved, native byte order.
 * Only up to WAVE_MAX_READ samples can be read in one call.
 * The output sample format depends on the value of wf->read_format.
 * Returns number of samples read or -1 on error.
 */
extern int wavfile_read_samples(WavFile *wf, void *buffer, int num_samples);

/**
 * Seeks to sample offset.
 * Syntax works like fseek. use WAV_SEEK_SET, WAV_SEEK_CUR, or WAV_SEEK_END
 * for the whence value.  Returns -1 on error, 0 otherwise.
 */
extern int wavfile_seek_samples(WavFile *wf, int64_t offset, int whence);

/**
 * Seeks to time offset, in milliseconds, based on the audio sample rate.
 * Syntax works like fseek. use WAV_SEEK_SET, WAV_SEEK_CUR, or WAV_SEEK_END
 * for the whence value.  Returns -1 on error, 0 otherwise.
 */
extern int wavfile_seek_time_ms(WavFile *wf, int64_t offset, int whence);

/**
 * Returns the current stream position, in samples.
 * Returns -1 on error.
 */
extern uint64_t wavfile_position(WavFile *wf);

/**
 * Returns the current stream position, in milliseconds.
 * Returns -1 on error.
 */
extern uint64_t wavfile_position_time_ms(WavFile *wf);

/**
 * Prints out a description of the wav format to the specified
 * output stream.
 */
extern void wavfile_print(FILE *st, WavFile *wf);

#endif /* WAV_H */
