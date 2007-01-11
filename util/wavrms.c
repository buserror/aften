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
 * @file wavrms.c
 * Console WAV File RMS/Dialog Normalization Utility
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

#include "wav.h"

static int
calculate_rms(FLOAT *samples, int ch, int n)
{
    FLOAT rms_all, rms_left, rms_right;
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
        rms_all = (rms_left + rms_right) / FCONST(2.0);
    }

    // Convert to dB
    rms_all = FCONST(10.0) * AFT_LOG10(rms_all + FCONST(1e-10));
    return -((int)(rms_all + FCONST(0.5)));
}

int
main(int argc, char **argv)
{
    FILE *fp;
    WavFile wf;
    FLOAT *buf;
    int start_sec, end_sec;
    int frame_size, nr, rms, dialnorm;
    uint64_t avg_rms, avg_cnt;

    /* open file */
    if(argc != 2 && argc != 4) {
        fprintf(stderr, "\nusage: wavrms <test.wav> [<start> <end>]\n\n");
        exit(1);
    }
    start_sec = end_sec = -1;
    if(argc == 4) {
        start_sec = MAX(atoi(argv[2]), 0);
        end_sec = MAX(atoi(argv[3]), 0);
        if(end_sec <= start_sec) {
            fprintf(stderr, "invalid time range\n");
            exit(1);
        }
    }
    if(!strncmp(argv[1], "-", 2)) {
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
#endif
        fp = stdin;
    } else {
        fp = fopen(argv[1], "rb");
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
    // seek to start of time range
    if(start_sec >= 0) {
        wavfile_seek_time_ms(&wf, start_sec*1000, WAV_SEEK_SET);
    }

#ifdef CONFIG_DOUBLE
    wf.read_format = WAV_SAMPLE_FMT_DBL;
#else
    wf.read_format = WAV_SAMPLE_FMT_FLT;
#endif

    buf = calloc(frame_size * wf.channels, sizeof(FLOAT));

    avg_rms = 0;
    avg_cnt = 1;
    nr = wavfile_read_samples(&wf, buf, frame_size);
    while(nr > 0) {
        // check for end of time range
        if(end_sec >= 0 && wavfile_position_time_ms(&wf) >= (end_sec*1000)) {
            break;
        }

        rms = calculate_rms(buf, wf.channels, nr);
        // use a reasonable dialog range
        if(rms < 40 && rms > 15) {
            avg_rms += rms;
            avg_cnt++;
        }

        nr = wavfile_read_samples(&wf, buf, frame_size);
    }
    avg_rms /= avg_cnt;
    dialnorm = MIN(avg_rms, 31);
    printf("Dialnorm: -%d dB\n", dialnorm);

    free(buf);
    fclose(fp);

    return 0;
}
