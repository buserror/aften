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
 * @file pcm.h
 * raw PCM decoder header
 */

#ifndef PCM_H
#define PCM_H

#include "common.h"

#include <stdio.h>

#include "pcmfile.h"

/* maximum single read size: 5 sec at 48kHz */
#define PCM_MAX_READ 240000

/* supported TWOCC WAVE formats */
#define WAVE_FORMAT_PCM         0x0001
#define WAVE_FORMAT_IEEEFLOAT   0x0003
#define WAVE_FORMAT_EXTENSIBLE  0xFFFE

/* raw audio sample types */
enum PcmSampleType {
    PCM_SAMPLE_TYPE_INT = 0,
    PCM_SAMPLE_TYPE_FLOAT
};

/* supported raw audio sample formats */
enum PcmSampleFormat {
    PCM_SAMPLE_FMT_UNKNOWN = -1,
    PCM_SAMPLE_FMT_U8 = 0,
    PCM_SAMPLE_FMT_S16,
    PCM_SAMPLE_FMT_S20,
    PCM_SAMPLE_FMT_S24,
    PCM_SAMPLE_FMT_S32,
    PCM_SAMPLE_FMT_FLT,
    PCM_SAMPLE_FMT_DBL
};

/* supported file formats */
enum PcmFileFormat {
    PCM_FORMAT_UNKNOWN = -1,
    PCM_FORMAT_RAW     =  0,
    PCM_FORMAT_WAVE    =  1
};

/* byte orders */
enum PcmByteOrder {
    PCM_BYTE_ORDER_LE = 0,
    PCM_BYTE_ORDER_BE = 1
};

/**
 * Initializes PcmFile structure using the given input file pointer.
 * Examines the header (if present) to get audio information and has the file
 * pointer aligned at start of data when it exits.
 * Returns non-zero value if an error occurs.
 */
extern int pcmfile_init(PcmFile *pf, FILE *fp, int read_format, int file_format);

/**
 * Frees memory from internal buffer.
 */
extern void pcmfile_close(PcmFile *pf);

/**
 * Sets the source sample format
 */
extern void pcmfile_set_source(PcmFile *pf, int fmt, int order);

/**
 * Sets source audio information
 */
extern void pcmfile_set_source_params(PcmFile *pf, int ch, int fmt, int order, int sr);

/**
 * Sets the requested read format
 */
extern void pcmfile_set_read_format(PcmFile *pf, int read_format);

/**
 * Prints out a description of the pcm format to the specified
 * output stream.
 */
extern void pcmfile_print(PcmFile *pf, FILE *st);

/**
 * Returns a default channel mask value based on the number of channels
 */
extern int pcm_get_default_ch_mask(int channels);

/**
 * File format functions
 */

extern int pcmfile_probe_raw(uint8_t *data, int size);
extern int pcmfile_init_raw(PcmFile *pf);

extern int pcmfile_probe_wave(uint8_t *data, int size);
extern int pcmfile_init_wave(PcmFile *pf);

#endif /* PCM_H */
