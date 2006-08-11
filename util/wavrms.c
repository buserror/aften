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
 * @file wavrms.c
 * Console WAV File RMS/Dialog Normalization Utility
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "wav.h"

static uint8_t
calculate_rms(double *samples, int ch, int n)
{
    double rms_all, rms_left, rms_right;
    int i;

    // Calculate RMS values
    if(ch == 1) {
        rms_all = 0.0;
        for(i=0; i<n; i++) {
            rms_all += (samples[i] * samples[i]);
        }
        rms_all /= n;
    } else {
        rms_left = rms_right = 0.0;
        for(i=0; i<n*ch; i+=ch) {
            rms_left  += (samples[i  ] * samples[i  ]);
            rms_right += (samples[i+1] * samples[i+1]);
        }
        rms_left /= n;
        rms_right /= n;
        rms_all = (rms_left + rms_right) / 2.0;
    }

    // Convert to dB
    rms_all = 10.0 * log10(rms_all + 1e-10);
    return -((int)(rms_all + 0.5));
}

static void
heap_sort(uint8_t *a, int n)
{
    int v, w, t;

    for(v=(n>>1)-1; v>=0; v--) {
        w = (v<<1)+1;
        while(w < n) {
            if((w+1) < n && a[w+1] > a[w]) w++;
            if(a[v] >= a[w]) break;
            t = a[v];
            a[v] = a[w];
            a[w] = t;
            v = w;
            w = (v<<1)+1;
        }
    }
    while(n > 1) {
        n--;
        t = a[0];
        a[0] = a[n];
        a[n] = t;
        v = 0;
        w = (v<<1)+1;
        while(w < n) {
            if((w+1) < n && a[w+1] > a[w]) w++;
            if(a[v] >= a[w]) break;
            t = a[v];
            a[v] = a[w];
            a[w] = t;
            v = w;
            w = (v<<1)+1;
        }
    } 
}

int
main(int argc, char **argv)
{
    FILE *fp;
    WavFile wf;
    double *buf;
    uint8_t rms;
    uint8_t *rms_list;
    int list_size, frame_size;
    uint32_t frame_count;
    int replay_rms, max_rms, min_rms, dialnorm;
    uint64_t avg_rms;
    int nr, i;

    /* open file */
    if(argc > 2) {
        fprintf(stderr, "\nusage: wavrms [<test.wav>]\n\n");
        exit(1);
    }
    if(argc == 2) {
        fp = fopen(argv[1], "rb");
    } else {
        fp = stdin;
    }
    if(!fp) {
        fprintf(stderr, "cannot open file\n");
        exit(1);
    }

    if(wavfile_init(&wf, fp)) {
        fprintf(stderr, "error initializing wav reader\n\n");
        exit(1);
    }
    frame_size = wf.sample_rate * 50 / 1000;

    list_size = 8192;
    rms_list = malloc(list_size);
    buf = malloc(frame_size * wf.channels * sizeof(double));

    frame_count = 0;
    avg_rms = 0;
    nr = wavfile_read_samples(&wf, buf, frame_size);
    while(nr > 0) {
        frame_count++;
        if(nr < frame_size) {
            for(i=nr*wf.channels; i<frame_size*wf.channels; i++) {
                buf[i] = 0.0;
            }
        }
        rms = calculate_rms(buf, wf.channels, frame_size);
        avg_rms += rms;
        if(frame_count > list_size) {
            list_size += 8192;
            rms_list = realloc(rms_list, list_size);
        }
        rms_list[frame_count-1] = rms;

        nr = wavfile_read_samples(&wf, buf, frame_size);
    }
    avg_rms /= frame_count;

    heap_sort(rms_list, frame_count);

    max_rms = rms_list[0];
    min_rms = rms_list[frame_count-1];
    replay_rms = rms_list[frame_count * 5 / 100];
    dialnorm = replay_rms + 5;
    if(dialnorm > 31) dialnorm = 31;

    printf("\n=-=-=-=-=-=-=-=-=-=-=\n");
    printf("Max RMS:   -%d dB\n", max_rms);
    printf("Min RMS:   -%d dB\n", min_rms);
    printf("Avg RMS:   -%d dB\n", (int)avg_rms);
    printf("Dialnorm:  -%d dB\n", dialnorm);
    printf("=-=-=-=-=-=-=-=-=-=-=\n\n");

    free(buf);
    free(rms_list);
    fclose(fp);

    return 0;
}
