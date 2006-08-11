/**
 * Aften: A/52 audio encoder
 * Copyright (c) 2006  Justin Ruggles <jruggle@earthlink.net>
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

static inline uint32_t
read4le(FILE *fp)
{
    uint32_t x;
    fread(&x, 4, 1, fp);
    return le2me_32(x);
}

static uint16_t
read2le(FILE *fp)
{
    uint16_t x;
    fread(&x, 2, 1, fp);
    return le2me_16(x);
}

int
wavfile_init(WavFile *wf, FILE *fp)
{
    int id, chunksize, found_fmt, found_data;

    if(wf == NULL || fp == NULL) return -1;

    memset(wf, 0, sizeof(WavFile));
    wf->fp = fp;

    /* attempt to get file size */
    wf->file_size = 0;
    wf->seekable = !fseek(fp, 0, SEEK_END);
    if(wf->seekable) {
        wf->file_size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
    }

    wf->filepos = 0;
    id = read4le(fp);
    wf->filepos += 4;
    if(id != RIFF_ID) return -1;
    read4le(fp);
    wf->filepos += 4;
    id = read4le(fp);
    wf->filepos += 4;
    if(id != WAVE_ID) return -1;
    found_data = found_fmt = 0;
    while(!found_data) {
        id = read4le(fp);
        wf->filepos += 4;
        chunksize = read4le(fp);
        wf->filepos += 4;
        if(id == 0 || chunksize == 0) return -1;
        switch(id) {
            case FMT__ID:
                if(chunksize < 16) return -1;
                wf->format = read2le(fp);
                wf->filepos += 2;
                wf->channels = read2le(fp);
                wf->filepos += 2;
                if(wf->channels == 0) return -1;
                wf->sample_rate = read4le(fp);
                wf->filepos += 4;
                if(wf->sample_rate == 0) return -1;
                read4le(fp);
                wf->filepos += 4;
                wf->block_align = read2le(fp);
                wf->filepos += 2;
                if(wf->block_align == 0) return -1;
                wf->bit_width = read2le(fp);
                wf->filepos += 2;
                if(wf->bit_width == 0) return -1;
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

                while(chunksize-- > 0) {
                    fgetc(fp);
                    wf->filepos++;
                }
                found_fmt = 1;
                break;
            case DATA_ID:
                if(!found_fmt) return -1;
                wf->data_size = chunksize;
                wf->data_start = wf->filepos;
                wf->samples = (wf->data_size / wf->block_align);
                found_data = 1;
                break;
            default:
                if(wf->seekable) {
                    fseek(fp, chunksize, SEEK_CUR);
                } else {
                    while(chunksize-- > 0) {
                        fgetc(fp);
                    }
                }
                wf->filepos += chunksize;
        }
    }

    return 0;
}

int
wavfile_read_samples(WavFile *wf, double *output, int num_samples)
{
    int nr, bytes_needed, i, j, bps;
    uint8_t *buffer;

    if(wf == NULL || wf->fp == NULL || output == NULL) return -1;
    if(wf->block_align <= 0) return -1;
    if(num_samples < 0) return -1;
    if(num_samples == 0) return 0;

    bytes_needed = wf->block_align * num_samples;
    buffer = malloc(bytes_needed);

    nr = fread(buffer, wf->block_align, num_samples, wf->fp);
    wf->filepos += nr * wf->block_align;

    bps = wf->block_align / wf->channels;
    if(bps == 1) {
        for(i=0; i<nr*wf->channels; i++) {
            output[i] = (((int)buffer[i])-128) / 128.0f;
        }
    } else if(bps == 2) {
        int v;
        for(i=0,j=0; i<nr*wf->channels*2; i+=2,j++) {
            v = buffer[i] + (buffer[i+1] << 8);
            if(v >= (1<<15)) v -= (1<<16);
            output[j] = v / (double)(1<<15);
        }
    } else if(bps == 3) {
        int v;
        for(i=0,j=0; i<nr*wf->channels*3; i+=3,j++) {
            v = buffer[i] + (buffer[i+1] << 8) + (buffer[i+2] << 16);
            if(v >= (1<<23)) v -= (1<<24);
            output[j] = v / (double)(1<<23);
        }
    } else if(bps == 4) {
        if(wf->format == WAVE_FORMAT_IEEEFLOAT) {
            float *fbuf = (float *)buffer;
#ifdef WORDS_BIG_ENDIAN
            uint32_t *buf32 = (uint32_t *)buffer;
            for(i=0; i<nr*wf->channels; i++) {
                buf32[i] = bswap_32(buf32[i]);
            }
#endif
            for(i=0; i<nr*wf->channels; i++) {
                output[i] = fbuf[i];
            }
        } else {
            int64_t v;
            for(i=0,j=0; i<nr*wf->channels*4; i+=4,j++) {
                v = buffer[i] + (buffer[i+1] << 8) + (buffer[i+2] << 16) +
                    (((uint32_t)buffer[i+3]) << 24);
                if(v >= (1LL<<31)) v -= (1LL<<32);
                output[j] = v / (double)(1LL<<31);
            }
        }
    } else if(wf->format == WAVE_FORMAT_IEEEFLOAT && bps == 8) {
        double *dbuf = (double *)buffer;
#ifdef WORDS_BIG_ENDIAN
        uint64_t *buf64 = (uint64_t *)buffer;
        for(i=0; i<nr*wf->channels; i++) {
            buf64[i] = bswap_64(buf64[i]);
        }
#endif
        for(i=0; i<nr*wf->channels; i++) {
            output[i] = dbuf[i];
        }
    }
    free(buffer);

    if(nr < num_samples) {
        for(i=nr*wf->channels; i<num_samples*wf->channels; i++) {
            output[i] = 0.0;
        }
    }

    return nr;
}

int
wavfile_seek_samples(WavFile *wf, int32_t offset, int whence)
{
    int byte_offset, pos, cur;

    if(wf == NULL || wf->fp == NULL) return -1;
    if(wf->block_align <= 0) return -1;
    if(wf->data_start == 0 || wf->data_size == 0) return -1;
    byte_offset = offset * wf->block_align;
    pos = wf->data_start;
    switch(whence) {
        case WAV_SEEK_SET:
            pos = wf->data_start + byte_offset;
            break;
        case WAV_SEEK_CUR:
            cur = wf->filepos;
            while(cur < wf->data_start) {
                fgetc(wf->fp);
                cur++;
                wf->filepos++;
            }
            pos = cur + byte_offset;
        case WAV_SEEK_END:
            pos = (wf->data_start+wf->data_size) - byte_offset;
            break;
        default: return -1;
    }
    if(pos < wf->data_start) {
        pos = 0;
    }
    if(pos >= wf->data_start+wf->data_size) {
        pos = wf->data_start+wf->data_size-1;
    }
    if(!wf->seekable) {
        if(pos < wf->filepos) return -1;
        while(wf->filepos < pos) {
            fgetc(wf->fp);
            wf->filepos++;
        }
    } else {
        if(fseek(wf->fp, pos, SEEK_SET)) return -1;
    }
    return 0;
}

int
wavfile_seek_time_ms(WavFile *wf, int32_t offset, int whence)
{
    int32_t samples;
    if(wf == NULL || wf->sample_rate == 0) return -1;
    samples = (offset * wf->sample_rate) / 1000;
    return wavfile_seek_samples(wf, samples, whence);
}

int
wavfile_position(WavFile *wf)
{
    int cur;

    if(wf == NULL) return 0;
    if(wf->data_start == 0 || wf->data_size == 0) return 0;

    cur = (wf->filepos - wf->data_start) / wf->block_align;
    if(cur <= 0) return 0;
    cur /= wf->block_align;
    return cur;
}

void
wavfile_print(FILE *st, WavFile *wf)
{
    char *type, *chan;
    if(st == NULL || wf == NULL) return;
    if(wf->format == WAVE_FORMAT_PCM) {
        if(wf->bit_width > 8) type = "Signed";
        else type = "Unsigned";
    } else if(wf->format == WAVE_FORMAT_IEEEFLOAT) {
        type = "Floating-point";
    } else {
        type = "[unsupported type]";
    }
    switch(wf->channels) {
        case 1: chan = "mono"; break;
        case 2: chan = "stereo"; break;
        case 3: chan = "3-channel"; break;
        case 4: chan = "4-channel"; break;
        case 5: chan = "5-channel"; break;
        case 6: chan = "6-channel"; break;
        default: chan = "multi-channel"; break;
    }
    fprintf(st, "%s %d-bit %d Hz %s\n", type, wf->bit_width, wf->sample_rate,
            chan);
}
