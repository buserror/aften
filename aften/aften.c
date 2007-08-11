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
 * @file aften.c
 * Commandline encoder frontend
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

#include "aften.h"
#include "pcm.h"
#include "opts.h"

static const int acmod_to_ch[8] = { 2, 1, 2, 3, 3, 4, 4, 5 };

static const char *acmod_str[8] = {
    "dual mono (1+1)", "mono (1/0)", "stereo (2/0)",
    "3/0", "2/1", "3/1", "2/2", "3/2"
};

static void
print_intro(FILE *out)
{
    const char *vers = aften_get_version();
    fprintf(out, "\nAften: A/52 audio encoder\n"
                 "Version %s\n"
                 "(c) 2006-2007 Justin Ruggles, Prakash Punnoor, et al.\n\n",
                 vers);
}

static void
print_simd_in_use(FILE *out, AftenSimdInstructions *simd_instructions)
{
    fprintf(out, "SIMD usage:");
    if (simd_instructions->mmx)
        fprintf(out, " MMX");
    if (simd_instructions->sse)
        fprintf(out, " SSE");
    if (simd_instructions->sse2)
        fprintf(out, " SSE2");
    if (simd_instructions->sse3)
        fprintf(out, " SSE3");
    if (simd_instructions->ssse3)
        fprintf(out, " SSSE3");
    if (simd_instructions->amd_3dnow)
        fprintf(out, " 3DNOW");
    if (simd_instructions->amd_3dnowext)
        fprintf(out, " 3DNOWEXT");
    if (simd_instructions->amd_sse_mmx)
        fprintf(out, " SSE-MMX");
    if (simd_instructions->altivec)
        fprintf(out, " Altivec");
    fprintf(out, "\n");
}

int
main(int argc, char **argv)
{
    uint8_t *frame;
    FLOAT *fwav;
    int nr, fs, err;
    FILE *ifp;
    FILE *ofp;
    PcmFile pf;
    CommandOptions opts;
    AftenContext s;
    uint32_t samplecount, bytecount, t0, t1, percent;
    FLOAT kbps, qual, bw;
    int last_frame;
    int frame_cnt;
    int done;
    int input_file_format;
    enum PcmSampleFormat read_format;
    /* update output every 200ms */
    clock_t update_clock_span = 0.2f * CLOCKS_PER_SEC;
    clock_t current_clock;
    clock_t last_update_clock = clock() - update_clock_span;

    opts.s = &s;
    aften_set_defaults(&s);
    err = parse_commandline(argc, argv, &opts);
    if(err) {
        if(err == 1) {
            print_intro(stderr);
            print_usage(stderr);
            return 1;
        } else {
            print_intro(stdout);
            if(err == 2) {
                print_help(stdout);
            } else if(err == 3) {
                print_long_help(stdout);
            }
            return 0;
        }
    }

    if(s.verbose > 0) {
        print_intro(stderr);
    }

    if(!strncmp(opts.infile, "-", 2)) {
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
#endif
        ifp = stdin;
    } else {
        ifp = fopen(opts.infile, "rb");
        if(!ifp) {
            fprintf(stderr, "error opening input file: %s\n", opts.infile);
            return 1;
        }
    }

#ifdef CONFIG_DOUBLE
    read_format = PCM_SAMPLE_FMT_DBL;
#else
    read_format = PCM_SAMPLE_FMT_FLT;
#endif

    // initialize pcmfile using input
    input_file_format = PCM_FORMAT_UNKNOWN;
    if(opts.raw_input)
        input_file_format = PCM_FORMAT_RAW;
    if(pcmfile_init(&pf, ifp, read_format, input_file_format)) {
        fprintf(stderr, "invalid input file: %s\n", argv[1]);
        return 1;
    }
    if(opts.read_to_eof)
        pf.read_to_eof = 1;
    if(opts.raw_input) {
        pf.sample_rate = opts.raw_sr;
        pf.channels = opts.raw_ch;
        pcmfile_set_source(&pf, opts.raw_fmt, opts.raw_order);
    }

    // print wav info to console
    if(s.verbose > 0) {
        fprintf(stderr, "input format: ");
        pcmfile_print(&pf, stderr);
    }

    // if acmod is given on commandline, determine lfe from number of channels
    if(s.acmod >= 0) {
        int ch = acmod_to_ch[s.acmod];
        if(ch == pf.channels) {
            if(s.lfe < 0) {
                s.lfe = 0;
            } else {
                if(s.lfe != 0) {
                    fprintf(stderr, "acmod and lfe do not match number of channels\n");
                    return 1;
                }
            }
        } else if(ch == (pf.channels - 1)) {
            if(s.lfe < 0) {
                s.lfe = 1;
            } else {
                if(s.lfe != 1) {
                    fprintf(stderr, "acmod and lfe do not match number of channels\n");
                    return 1;
                }
            }
        } else {
            fprintf(stderr, "acmod does not match number of channels\n");
            return 1;
        }
    } else {
        // if acmod is not given on commandline, determine from WAVE file
        int ch = pf.channels;
        if(s.lfe >= 0) {
            if(s.lfe == 0 && ch == 6) {
                fprintf(stderr, "cannot encode 6 channels w/o LFE\n");
                return 1;
            } else if(s.lfe == 1 && ch == 1) {
                fprintf(stderr, "cannot encode LFE channel only\n");
                return 1;
            }
            if(s.lfe) {
                pf.ch_mask |= 0x08;
            }
        }
        if(aften_wav_channels_to_acmod(ch, pf.ch_mask, &s.acmod, &s.lfe)) {
            fprintf(stderr, "mismatch in channels, acmod, and lfe params\n");
            return 1;
        }
    }
    // set some encoding parameters using wav info
    s.channels = pf.channels;
    s.samplerate = pf.sample_rate;
#ifdef CONFIG_DOUBLE
    s.sample_format = A52_SAMPLE_FMT_DBL;
#else
    s.sample_format = A52_SAMPLE_FMT_FLT;
#endif

    // initialize encoder
    if(aften_encode_init(&s)) {
        fprintf(stderr, "error initializing encoder\n");
        aften_encode_close(&s);
        return 1;
    }

    // open output file
    if(!strncmp(opts.outfile, "-", 2)) {
#ifdef _WIN32
        _setmode(_fileno(stdout), _O_BINARY);
#endif
        ofp = stdout;
    } else {
        ofp = fopen(opts.outfile, "wb");
        if(!ofp) {
            fprintf(stderr, "error opening output file: %s\n", opts.outfile);
            return 1;
        }
    }

    // print ac3 info to console
    if(s.verbose > 0) {
        fprintf(stderr, "output format: %d Hz %s", s.samplerate,
                acmod_str[s.acmod]);
        if(s.lfe) {
            fprintf(stderr, " + LFE");
        }
        fprintf(stderr, "\n\n");
    }

    /* print SIMD instructions used */
    print_simd_in_use(stderr, &s.system.wanted_simd_instructions);

    /* print number of threads used */
    fprintf(stderr, "Threads: %i\n\n", s.system.n_threads);

    // allocate memory for coded frame and sample buffer
    frame = calloc(A52_MAX_CODED_FRAME_SIZE, 1);
    fwav = calloc(A52_SAMPLES_PER_FRAME * s.channels, sizeof(FLOAT));
    if(frame == NULL || fwav == NULL) {
        aften_encode_close(&s);
        exit(1);
    }

    samplecount = bytecount = t0 = t1 = percent = 0;
    qual = bw = 0.0;
    last_frame = 0;
    frame_cnt = 0;
    done = 0;
    fs = 0;
    nr = 0;

    // don't pad start with zero samples, use input audio instead.
    // not recommended, but providing the option here for when sync is needed
    if(!opts.pad_start) {
        FLOAT *sptr = &fwav[1280*s.channels];
        nr = pcmfile_read_samples(&pf, sptr, 256);
        if(opts.chmap == 0) {
            aften_remap_wav_to_a52(sptr, nr, s.channels, s.sample_format,
                                   s.acmod);
        } else if(opts.chmap == 2) {
            aften_remap_mpeg_to_a52(sptr, nr, s.channels, s.sample_format,
                                    s.acmod);
        }
        fs = aften_encode_frame(&s, frame, fwav);
        if(fs < 0) {
            fprintf(stderr, "Error encoding initial frame\n");
            goto end;
        }
    }

    nr = pcmfile_read_samples(&pf, fwav, A52_SAMPLES_PER_FRAME);
    while(nr > 0 || fs > 0) {
        if(opts.chmap == 0) {
            aften_remap_wav_to_a52(fwav, nr, s.channels, s.sample_format,
                                   s.acmod);
        } else if(opts.chmap == 2) {
            aften_remap_mpeg_to_a52(fwav, nr, s.channels, s.sample_format,
                                    s.acmod);
        }

        // append extra silent frame if final frame is > 1280 samples
        if(nr == 0) {
            if(last_frame <= 1280) {
                done = 1;
            }
        }

        // zero leftover samples at end of last frame
        if(!done && nr < A52_SAMPLES_PER_FRAME) {
            int i;
            for(i=nr*s.channels; i<A52_SAMPLES_PER_FRAME*s.channels; i++) {
                fwav[i] = 0.0;
            }
        }

        fs = aften_encode_frame(&s, frame, done ? NULL : fwav);

        if(fs < 0) {
            fprintf(stderr, "Error encoding frame %d\n", frame_cnt);
            break;
        } else {
            if(s.verbose > 0) {
                if (fs > 0) {
                    samplecount += A52_SAMPLES_PER_FRAME;
                    bytecount += fs;
                    qual += s.status.quality;
                    bw += s.status.bwcode;
                }
                current_clock = clock();
                /* make sure we write out when finished, i.e. when fs == 0 */
                if (current_clock - last_update_clock >= update_clock_span || !fs || s.verbose == 2) {
                    if(s.verbose == 1) {
                        t1 = samplecount / pf.sample_rate;
                        if(frame_cnt > 0 && (t1 > t0 || samplecount >= pf.samples)) {
                            kbps = (bytecount * FCONST(8.0) * pf.sample_rate) /
                                (FCONST(1000.0) * samplecount);
                            percent = 0;
                            if(pf.samples > 0) {
                                percent = (uint32_t)((samplecount * FCONST(100.0)) /
                                                     pf.samples);
                                percent = CLIP(percent, 0, 100);
                            }
                            fprintf(stderr, "\rprogress: %3u%% | q: %4.1f | "
                                    "bw: %2.1f | bitrate: %4.1f kbps ",
                                    percent, (qual / (frame_cnt+1)),
                                    (bw / (frame_cnt+1)), kbps);
                        }
                        t0 = t1;
                    } else if(s.verbose == 2) {
                        fprintf(stderr, "frame: %7d | q: %4d | bw: %2d | bitrate: %3d kbps\n",
                                frame_cnt, s.status.quality, s.status.bwcode,
                                s.status.bit_rate);
                    }
                    last_update_clock = current_clock;
                }
            }
            fwrite(frame, 1, fs, ofp);
        }
        frame_cnt++;
        last_frame = nr;
        nr = pcmfile_read_samples(&pf, fwav, A52_SAMPLES_PER_FRAME);
    }
    if(s.verbose == 1) {
        fprintf(stderr, "\n\n");
    } else if(s.verbose == 2) {
        if(samplecount > 0) {
            kbps = (bytecount * FCONST(8.0) * pf.sample_rate) / (FCONST(1000.0) * samplecount);
        } else {
            kbps = 0;
        }
        frame_cnt = MAX(frame_cnt, 1);
        fprintf(stderr, "\n");
        fprintf(stderr, "average quality:   %4.1f\n", (qual / frame_cnt));
        fprintf(stderr, "average bandwidth: %2.1f\n", (bw / frame_cnt));
        fprintf(stderr, "average bitrate:   %4.1f kbps\n\n", kbps);
    }
end:
    free(fwav);
    free(frame);

    pcmfile_close(&pf);
    fclose(ifp);
    fclose(ofp);

    aften_encode_close(&s);

    return 0;
}
