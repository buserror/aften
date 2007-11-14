/**
 * Aften: A/52 audio encoder
 * Copyright (c) 2007 Justin Ruggles
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
 * @file pcmfile.h
 * raw PCM file i/o header
 */

#ifndef PCMFILE_H
#define PCMFILE_H

#include "common.h"

#include "byteio.h"

/* "whence" values for seek functions */
#define PCM_SEEK_SET 0
#define PCM_SEEK_CUR 1
#define PCM_SEEK_END 2

/* main decoder context */
typedef struct PcmFile {
    /** Format conversion function */
    void (*fmt_convert)(void *dest_v, void *src_v, int n);

    ByteIOContext io;       ///< input buffer
    uint64_t filepos;       ///< current file position
    int seekable;           ///< indicates if input stream is seekable
    int read_to_eof;        ///< indicates that data is to be read until EOF
    uint64_t file_size;     ///< total file size, if known
    uint64_t data_start;    ///< byte position for start of data
    uint64_t data_size;     ///< data size, in bytes
    uint64_t samples;       ///< total number of audio samples

    int sample_type;        ///< sample type (integer or floating-point)
    int file_format;        ///< file format (raw, wav, etc...)
    int order;              ///< sample byte order
    int channels;           ///< number of channels
    uint32_t ch_mask;       ///< channel mask, indicates speaker locations
    int sample_rate;        ///< audio sampling frequency
    int block_align;        ///< bytes in each sample, for all channels
    int bit_width;          ///< bits-per-sample

    int source_format;      ///< sample type in the input file
    int read_format;        ///< sample type to convert to when reading

    int internal_fmt;       ///< internal format (e.g. WAVE wFormatTag)
} PcmFile;

/**
 * Reads audio samples to the output buffer.
 * Output is channel-interleaved, native byte order.
 * Only up to PCM_MAX_READ samples can be read in one call.
 * The output sample format depends on the value of pf->read_format.
 * Returns number of samples read or -1 on error.
 */
extern int pcmfile_read_samples(PcmFile *pf, void *buffer, int num_samples);

/**
 * Seeks to byte offset within file.
 * Limits the seek position or offset to signed 32-bit.
 * It also does slower forward seeking for streaming input.
 */
extern int pcmfile_seek_set(PcmFile *pf, uint64_t dest);

/**
 * Seeks to sample offset.
 * Syntax works like fseek. use PCM_SEEK_SET, PCM_SEEK_CUR, or PCM_SEEK_END
 * for the whence value.  Returns -1 on error, 0 otherwise.
 */
extern int pcmfile_seek_samples(PcmFile *pf, int64_t offset, int whence);

/**
 * Seeks to time offset, in milliseconds, based on the audio sample rate.
 * Syntax works like fseek. use PCM_SEEK_SET, PCM_SEEK_CUR, or PCM_SEEK_END
 * for the whence value.  Returns -1 on error, 0 otherwise.
 */
extern int pcmfile_seek_time_ms(PcmFile *pf, int64_t offset, int whence);

/**
 * Returns the current stream position, in samples.
 * Returns -1 on error.
 */
extern uint64_t pcmfile_position(PcmFile *pf);

/**
 * Returns the current stream position, in milliseconds.
 * Returns -1 on error.
 */
extern uint64_t pcmfile_position_time_ms(PcmFile *pf);

#endif /* PCMFILE_H */
