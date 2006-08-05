/**
 * Bitwise File Writer
 * Copyright (c) 2006 Justin Ruggles
 *
 * derived from ffmpeg/libavcodec/bitstream.h
 * Common bit i/o utils
 * Copyright (c) 2000, 2001 Fabrice Bellard
 * Copyright (c) 2002-2004 Michael Niedermayer
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

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "bitio.h"

void
bitwriter_init(BitWriter *bw, void *buf, int len)
{
    if(len < 0) {
        len = 0;
        buf = NULL;
    }
    bw->buffer = buf;
    bw->buf_end = bw->buffer + len;
    bw->buf_ptr = bw->buffer;
    bw->bit_left = 32;
    bw->bit_buf = 0;
    bw->eof = 0;
}

void
bitwriter_flushbits(BitWriter *bw)
{
    bw->bit_buf <<= bw->bit_left;
    while(bw->bit_left < 32) {
        *bw->buf_ptr++ = bw->bit_buf >> 24;
        bw->bit_buf <<= 8;
        bw->bit_left += 8;
    }
    bw->bit_left = 32;
    bw->bit_buf = 0;
}

void
bitwriter_writebits(BitWriter *bw, int bits, uint32_t val)
{
    unsigned int bit_buf;
    int bit_left;

    assert(bits == 32 || val < (1U << bits));

    if(bw->eof || bw->buf_ptr >= bw->buf_end) {
        bw->eof = 1;
        return;
    }

    bit_buf = bw->bit_buf;
    bit_left = bw->bit_left;

    if(bits < bit_left) {
        bit_buf = (bit_buf << bits) | val;
        bit_left -= bits;
    } else {
        bit_buf <<= bit_left;
        bit_buf |= val >> (bits - bit_left);
        *(uint32_t *)bw->buf_ptr = be2me_32(bit_buf);
        bw->buf_ptr += 4;
        bit_left += (32 - bits);
        bit_buf = val;
    }

    bw->bit_buf = bit_buf;
    bw->bit_left = bit_left;
}

uint32_t
bitwriter_bitcount(BitWriter *bw)
{
    return (((bw->buf_ptr - bw->buffer) << 3) + 32 - bw->bit_left);
}
