/**
 * Aften: A/52 audio encoder
 * Copyright (c) 2007 Justin Ruggles
 *               2007 Prakash Punnoor <prakash@punnoor.de>
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
 * @file opts.c
 * Commandline options
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "opts.h"
#include "pcm.h"

static int
deactivate_simd(char *simd, AftenSimdInstructions *wanted_simd_instructions)
{
    int i, j;
    int last = 0;

    for (i=0; simd[i]; i=j+1) {
        for (j=i; simd[j]!=','; ++j) {
            if (!simd[j]) {
                last = 1;
                break;
            }
        }
        simd[j] = 0;

        if (!strcmp(&simd[i], "mmx"))
            wanted_simd_instructions->mmx = 0;
        else if (!strcmp(&simd[i], "sse"))
            wanted_simd_instructions->sse = 0;
        else if (!strcmp(&simd[i], "sse2"))
            wanted_simd_instructions->sse2 = 0;
        else if (!strcmp(&simd[i], "sse3"))
            wanted_simd_instructions->sse3 = 0;
        else if (!strcmp(&simd[i], "altivec"))
            wanted_simd_instructions->altivec = 0;
        else {
            fprintf(stderr, "invalid simd instruction set: %s. must be mmx, sse, sse2, sse3 or altivec.\n", &simd[i]);
            return 1;
        }
        if (last)
            break;
    }
    return 0;
}

/**
 * Channel bitmask values
 * Note that the bitmask used here is not the same as the WAVE channel mask.
 * They are arranged so that any valid channel combination will be in proper
 * AC3 channel order.
 */
#define CHMASK_FL  0x1
#define CHMASK_FC  0x2
#define CHMASK_FR  0x4
#define CHMASK_SL  0x8
#define CHMASK_S   0x10
#define CHMASK_SR  0x20
#define CHMASK_M1  0x40
#define CHMASK_M2  0x80
#define CHMASK_LFE 0x100

static int
validate_input_files(char **infile, int input_mask, int *acmod, int *lfe) {
    int i, j, l;

    // check for valid configurations
    l = !!(input_mask & CHMASK_LFE);
    input_mask &= 0xFF;
    switch(input_mask) {
        case (CHMASK_M1 | CHMASK_M2):
            *acmod = 0;
            break;
        case (CHMASK_FC):
            *acmod = 1;
            break;
        case (CHMASK_FL | CHMASK_FR):
            *acmod = 2;
            break;
        case (CHMASK_FL | CHMASK_FC | CHMASK_FR):
            *acmod = 3;
            break;
        case (CHMASK_FL | CHMASK_FR | CHMASK_S):
            *acmod = 4;
            break;
        case (CHMASK_FL | CHMASK_FC | CHMASK_FR | CHMASK_S):
            *acmod = 5;
            break;
        case (CHMASK_FL | CHMASK_FR | CHMASK_SL | CHMASK_SR):
            *acmod = 6;
            break;
        case (CHMASK_FL | CHMASK_FC | CHMASK_FR | CHMASK_SL | CHMASK_SR):
            *acmod = 7;
            break;
        default:
            return -1;
    }
    *lfe = l;

    // shift used channels, filling NULL gaps in infile array
    for(i=1; i<A52_NUM_SPEAKERS; i++) {
        for(j=i; j > 0 && infile[j] != NULL && infile[j-1] == NULL; j--) {
            infile[j-1] = infile[j];
            infile[j] = NULL;
        }
    }

    return 0;
}

int
parse_commandline(int argc, char **argv, CommandOptions *opts)
{
    int i;
    int found_output = 0;
    int single_input = 0;
    int multi_input = 0;
    int empty_params = 0;
    int input_mask = 0;

    if(argc < 2) {
        return 1;
    }

    opts->chmap = 0;
    opts->num_input_files = 0;
    memset(opts->infile, 0, A52_NUM_SPEAKERS * sizeof(char *));
    opts->outfile = NULL;
    opts->pad_start = 1;
    opts->read_to_eof = 0;
    opts->raw_input = 0;
    opts->raw_fmt = PCM_SAMPLE_FMT_S16;
    opts->raw_order = PCM_BYTE_ORDER_LE;
    opts->raw_sr = 48000;
    opts->raw_ch = 2;

    for(i=1; i<argc; i++) {
        if(argv[i][0] == '-' && argv[i][1] != '\0') {
            if(argv[i][2] != '\0') {
                // multi-character arguments
                if(!strncmp(&argv[i][1], "cmix", 5)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->meta.cmixlev = atoi(argv[i]);
                    if(opts->s->meta.cmixlev < 0 || opts->s->meta.cmixlev > 2) {
                        fprintf(stderr, "invalid cmix: %d. must be 0 to 2.\n",
                                opts->s->meta.cmixlev);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "smix", 5)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->meta.surmixlev = atoi(argv[i]);
                    if(opts->s->meta.surmixlev < 0 || opts->s->meta.surmixlev > 2) {
                        fprintf(stderr, "invalid smix: %d. must be 0 to 2.\n",
                                opts->s->meta.surmixlev);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "dsur", 5)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->meta.dsurmod = atoi(argv[i]);
                    if(opts->s->meta.dsurmod < 0 || opts->s->meta.dsurmod > 2) {
                        fprintf(stderr, "invalid dsur: %d. must be 0 to 2.\n",
                                opts->s->meta.dsurmod);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "dnorm", 6)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->meta.dialnorm = atoi(argv[i]);
                    if(opts->s->meta.dialnorm < 0 || opts->s->meta.dialnorm > 31) {
                        fprintf(stderr, "invalid dnorm: %d. must be 0 to 31.\n",
                                opts->s->meta.dialnorm);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "dynrng", 7)) {
                    int profile;
                    i++;
                    if(i >= argc) return 1;
                    profile = atoi(argv[i]);
                    if(profile < 0 || profile > 5) {
                        fprintf(stderr, "invalid dynrng: %d. must be 0 to 5.\n",
                                profile);
                        return 1;
                    }
                    opts->s->params.dynrng_profile = (DynRngProfile) profile;
                } else if(!strncmp(&argv[i][1], "acmod", 6)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->acmod = atoi(argv[i]);
                    if(opts->s->acmod < 0 || opts->s->acmod > 7) {
                        fprintf(stderr, "invalid acmod: %d. must be 0 to 7.\n",
                                opts->s->acmod);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "lfe", 4)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->lfe = atoi(argv[i]);
                    if(opts->s->lfe < 0 || opts->s->lfe > 1) {
                        fprintf(stderr, "invalid lfe: %d. must be 0 or 1.\n",
                                opts->s->lfe);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "bwfilter", 9)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->params.use_bw_filter = atoi(argv[i]);
                    if(opts->s->params.use_bw_filter < 0 || opts->s->params.use_bw_filter > 1) {
                        fprintf(stderr, "invalid bwfilter: %d. must be 0 or 1.\n",
                                opts->s->params.use_bw_filter);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "dcfilter", 9)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->params.use_dc_filter = atoi(argv[i]);
                    if(opts->s->params.use_dc_filter < 0 || opts->s->params.use_dc_filter > 1) {
                        fprintf(stderr, "invalid dcfilter: %d. must be 0 or 1.\n",
                                opts->s->params.use_dc_filter);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "lfefilter", 10)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->params.use_lfe_filter = atoi(argv[i]);
                    if(opts->s->params.use_lfe_filter < 0 || opts->s->params.use_lfe_filter > 1) {
                        fprintf(stderr, "invalid lfefilter: %d. must be 0 or 1.\n",
                                opts->s->params.use_lfe_filter);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "xbsi1", 6)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->meta.xbsi1e = atoi(argv[i]);
                    if(opts->s->meta.xbsi1e < 0 || opts->s->meta.xbsi1e > 1) {
                        fprintf(stderr, "invalid xbsi1: %d. must be 0 or 1.\n",
                                opts->s->meta.xbsi1e);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "dmixmod", 8)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->meta.dmixmod = atoi(argv[i]);
                    if(opts->s->meta.dmixmod < 0 || opts->s->meta.dmixmod > 2) {
                        fprintf(stderr, "invalid dmixmod: %d. must be 0 to 2.\n",
                                opts->s->meta.dmixmod);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "ltrtcmix", 9)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->meta.ltrtcmixlev = atoi(argv[i]);
                    if(opts->s->meta.ltrtcmixlev < 0 || opts->s->meta.ltrtcmixlev > 7) {
                        fprintf(stderr, "invalid ltrtcmix: %d. must be 0 to 7.\n",
                                opts->s->meta.ltrtcmixlev);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "ltrtsmix", 9)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->meta.ltrtsmixlev = atoi(argv[i]);
                    if(opts->s->meta.ltrtsmixlev < 0 || opts->s->meta.ltrtsmixlev > 7) {
                        fprintf(stderr, "invalid ltrtsmix: %d. must be 0 to 7.\n",
                                opts->s->meta.ltrtsmixlev);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "lorocmix", 9)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->meta.lorocmixlev = atoi(argv[i]);
                    if(opts->s->meta.lorocmixlev < 0 || opts->s->meta.lorocmixlev > 7) {
                        fprintf(stderr, "invalid lorocmix: %d. must be 0 to 7.\n",
                                opts->s->meta.lorocmixlev);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "lorosmix", 9)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->meta.lorosmixlev = atoi(argv[i]);
                    if(opts->s->meta.lorosmixlev < 0 || opts->s->meta.lorosmixlev > 7) {
                        fprintf(stderr, "invalid lorosmix: %d. must be 0 to 7.\n",
                                opts->s->meta.lorosmixlev);
                        return 1;
                    }
                    opts->s->meta.xbsi1e = 1;
                } else if(!strncmp(&argv[i][1], "xbsi2", 6)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->meta.xbsi2e = atoi(argv[i]);
                    if(opts->s->meta.xbsi2e < 0 || opts->s->meta.xbsi2e > 1) {
                        fprintf(stderr, "invalid xbsi2: %d. must be 0 or 1.\n",
                                opts->s->meta.xbsi2e);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "dsurexmod", 10)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->meta.dsurexmod = atoi(argv[i]);
                    if(opts->s->meta.dsurexmod < 0 || opts->s->meta.dsurexmod > 2) {
                        fprintf(stderr, "invalid dsurexmod: %d. must be 0 to 2.\n",
                                opts->s->meta.dsurexmod);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "dheadphon", 10)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->meta.dheadphonmod = atoi(argv[i]);
                    if(opts->s->meta.dheadphonmod < 0 || opts->s->meta.dheadphonmod > 2) {
                        fprintf(stderr, "invalid dheadphon: %d. must be 0 to 2.\n",
                                opts->s->meta.dheadphonmod);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "adconvtyp", 10)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->meta.adconvtyp = atoi(argv[i]);
                    if(opts->s->meta.adconvtyp < 0 || opts->s->meta.adconvtyp > 1) {
                        fprintf(stderr, "invalid adconvtyp: %d. must be 0 or 1.\n",
                                opts->s->meta.adconvtyp);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "fba", 4)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->params.bitalloc_fast = atoi(argv[i]);
                    if(opts->s->params.bitalloc_fast < 0 || opts->s->params.bitalloc_fast > 1) {
                        fprintf(stderr, "invalid fba: %d. must be 0 or 1.\n",
                                opts->s->params.bitalloc_fast);
                        return 1;
                    }
                }  else if(!strncmp(&argv[i][1], "fes", 4)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->params.expstr_fast = atoi(argv[i]);
                    if(opts->s->params.expstr_fast < 0 || opts->s->params.expstr_fast > 1) {
                        fprintf(stderr, "invalid fes: %d. must be 0 or 1.\n",
                                opts->s->params.expstr_fast);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "longhelp", 9)) {
                    return 3;
                } else if(!strncmp(&argv[i][1], "chmap", 6)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->chmap = atoi(argv[i]);
                    if(opts->chmap < 0 || opts->chmap > 2) {
                        fprintf(stderr, "invalid chmap: %d. must be 0 to 2.\n",
                                opts->chmap);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "nosimd", 7)) {
                    i++;
                    if(i >= argc) return 1;
                    if (deactivate_simd(argv[i], &opts->s->system.wanted_simd_instructions)) return 1;
                } else if(!strncmp(&argv[i][1], "pad", 4)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->pad_start = atoi(argv[i]);
                    if(opts->pad_start < 0 || opts->pad_start > 1) {
                        fprintf(stderr, "invalid pad: %d. must be 0 or 1.\n",
                                opts->pad_start);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "readtoeof", 10)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->read_to_eof = atoi(argv[i]);
                    if(opts->read_to_eof < 0) {
                        fprintf(stderr, "invalid readtoeof: %d. must 0 or 1.\n",
                                opts->read_to_eof);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "threads", 8)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->system.n_threads = atoi(argv[i]);
                    if(opts->s->system.n_threads < 0 ||
                            opts->s->system.n_threads > MAX_NUM_THREADS) {
                        fprintf(stderr, "invalid readtoeof: %d. must 0 to %d.\n",
                                opts->s->system.n_threads, MAX_NUM_THREADS);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "wmin", 5)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->params.min_bwcode = atoi(argv[i]);
                    if(opts->s->params.min_bwcode < 0 ||
                            opts->s->params.min_bwcode > 60) {
                        fprintf(stderr, "invalid wmin: %d. must 0 to 60.\n",
                                opts->s->params.min_bwcode);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "wmax", 5)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->params.max_bwcode = atoi(argv[i]);
                    if(opts->s->params.max_bwcode < 0 ||
                            opts->s->params.max_bwcode > 60) {
                        fprintf(stderr, "invalid wmax: %d. must 0 to 60.\n",
                                opts->s->params.max_bwcode);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "raw_fmt", 8)) {
                    static const char *raw_fmt_strs[8] = {
                        "u8", "s8", "s16_", "s20_", "s24_", "s32_", "float_",
                        "double_"
                    };
                    int j;
                    i++;
                    if(i >= argc) return 1;

                    // parse format
                    for(j=0; j<8; j++) {
                        if(!strncmp(argv[i], raw_fmt_strs[j], strlen(raw_fmt_strs[j]))) {
                            opts->raw_fmt = j;
                            break;
                        }
                    }
                    if(j >= 8) {
                        fprintf(stderr, "invalid raw_fmt: %s\n", argv[i]);
                        return 1;
                    }

                    // parse byte order
                    opts->raw_order = PCM_BYTE_ORDER_LE;
                    if(strncmp(argv[i], "u8", 3) && strncmp(argv[i], "s8", 3)) {
                        if(!strncmp(&argv[i][4], "be", 3)) {
                            opts->raw_order = PCM_BYTE_ORDER_BE;
                        } else if(strncmp(&argv[i][4], "le", 3)) {
                            fprintf(stderr, "invalid raw_fmt: %s\n", argv[i]);
                            return 1;
                        }
                    }

                    opts->raw_input = 1;
                } else if(!strncmp(&argv[i][1], "raw_sr", 7)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->raw_sr = atoi(argv[i]);
                    if(opts->raw_sr <= 0) {
                        fprintf(stderr, "invalid raw_sr: %d. must be > 0.\n",
                                opts->raw_sr);
                        return 1;
                    }
                    opts->raw_input = 1;
                } else if(!strncmp(&argv[i][1], "raw_ch", 7)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->raw_ch = atoi(argv[i]);
                    if(opts->raw_ch <= 0 || opts->raw_ch > 255) {
                        fprintf(stderr, "invalid raw_ch: %d. must be 0 to 255.\n",
                                opts->raw_ch);
                        return 1;
                    }
                    opts->raw_input = 1;
                } else if(!strncmp(&argv[i][1], "version", 8)) {
                    return 4;
                } else if(!strncmp(&argv[i][1], "chconfig", 9)) {
                    i++;
                    if(i >= argc) return 1;
                    if(!strncmp(argv[i], "1+1", 4)) {
                        opts->s->acmod = 0;
                    } else if(strlen(argv[i]) >= 3 && argv[i][1] == '/') {
                        int front_ch = argv[i][0] - '0';
                        int rear_ch = argv[i][2] - '0';
                        if (front_ch > 3 || front_ch < 1 || rear_ch < 0 || rear_ch > 2 || (front_ch < 2 && rear_ch != 0))
                            goto invalid_config;

                        opts->s->acmod = front_ch + 2 * rear_ch;
                    } else {
                    invalid_config:
                        fprintf(stderr, "invalid chconfig: %s\n", argv[i]);
                        return 1;
                    }
                    if(!strncmp(&argv[i][3], "+LFE", 5) || !strncmp(&argv[i][3], "+lfe", 5)) {
                        opts->s->lfe = 1;
                    }
                } else if(!strncmp(&argv[i][1], "ch_", 3)) {
                    static const char chstrs[A52_NUM_SPEAKERS][4] = {
                        "fl", "fc", "fr", "sl", "s", "sr", "m1", "m2", "lfe" };
                    char *chstr = &argv[i][4];
                    int n_chstr, mask_bit;

                    if(single_input) {
                        fprintf(stderr, "cannot mix single-input syntax and multi-input syntax\n");
                        return 1;
                    }
                    i++;
                    if(i >= argc) return 1;

                    mask_bit = 1;
                    for(n_chstr=0; n_chstr<A52_NUM_SPEAKERS; ++n_chstr) {
                        if(!strncmp(chstr, chstrs[n_chstr], strlen(chstrs[n_chstr])+1)) {
                            if(input_mask & mask_bit) {
                                fprintf(stderr, "invalid input channel parameter\n");
                                return 1;
                            }
                            input_mask |= mask_bit;
                            opts->infile[n_chstr] = argv[i];
                        }
                        mask_bit += mask_bit;
                    }

                    multi_input = 1;
                    opts->num_input_files++;
                } else {
                    fprintf(stderr, "invalid option: %s\n", argv[i]);
                    return 1;
                }
            } else {
                // single-character arguments
                if(argv[i][1] == 'h') {
                    return 2;
                } else if(argv[i][1] == 'v') {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->verbose = atoi(argv[i]);
                    if(opts->s->verbose < 0 || opts->s->verbose > 2) {
                        fprintf(stderr, "invalid verbosity level: %d. must be 0 to 2.\n",
                                opts->s->verbose);
                        return 1;
                    }
                } else if(argv[i][1] == 'b') {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->params.bitrate = atoi(argv[i]);
                    if(opts->s->params.bitrate < 0 || opts->s->params.bitrate > 640) {
                        fprintf(stderr, "invalid bitrate: %d. must be 0 to 640.\n",
                                opts->s->params.bitrate);
                        return 1;
                    }
                } else if(argv[i][1] == 'q') {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->params.quality = atoi(argv[i]);
                    opts->s->params.encoding_mode = AFTEN_ENC_MODE_VBR;
                    if(opts->s->params.quality < 0 || opts->s->params.quality > 1023) {
                        fprintf(stderr, "invalid quality: %d. must be 0 to 1023.\n",
                                opts->s->params.quality);
                        return 1;
                    }
                } else if(argv[i][1] == 'w') {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->params.bwcode = atoi(argv[i]);
                    if(opts->s->params.bwcode < -2 || opts->s->params.bwcode > 60) {
                        fprintf(stderr, "invalid bandwidth code: %d. must be 0 to 60.\n",
                                opts->s->params.bwcode);
                        return 1;
                    }
                } else if(argv[i][1] == 'm') {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->params.use_rematrixing = atoi(argv[i]);
                    if(opts->s->params.use_rematrixing < 0 || opts->s->params.use_rematrixing > 1) {
                        fprintf(stderr, "invalid stereo rematrixing: %d. must be 0 or 1.\n",
                                opts->s->params.use_rematrixing);
                        return 1;
                    }
                } else if(argv[i][1] == 's') {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->params.use_block_switching = atoi(argv[i]);
                    if(opts->s->params.use_block_switching < 0 || opts->s->params.use_block_switching > 1) {
                        fprintf(stderr, "invalid block switching: %d. must be 0 or 1.\n",
                                opts->s->params.use_block_switching);
                        return 1;
                    }
                } else {
                    fprintf(stderr, "invalid option: %s\n", argv[i]);
                    return 1;
                }
            }
        } else {
            // empty parameter can be single input file or output file
            if(i >= argc) return 1;
            empty_params++;
            if(empty_params == 1) {
                opts->outfile = argv[i];
                found_output = 1;
            } else if(empty_params == 2) {
                if(multi_input) {
                    fprintf(stderr, "cannot mix single-input syntax and multi-input syntax\n");
                    return 1;
                }
                single_input = 1;
                opts->infile[0] = opts->outfile;
                opts->outfile = argv[i];
                opts->num_input_files = 1;
            } else {
                fprintf(stderr, "too many empty parameters\n");
                return 1;
            }
        }
    }

    // check that proper input file syntax is used
    if(!(single_input ^ multi_input)) {
        fprintf(stderr, "cannot mix single-input syntax and multi-input syntax\n");
        return 1;
    }

    // check that the number of input files is valid
    if(opts->num_input_files < 1 || opts->num_input_files > 6) {
        fprintf(stderr, "invalid number of input channels. must be 1 to 6.\n");
        return 1;
    }

    // check that an output file has been specified
    if(!found_output) {
        fprintf(stderr, "no output file specified.\n");
        return 1;
    }

    // check the channel configuration
    if(multi_input) {
        if(validate_input_files(opts->infile, input_mask, &opts->s->acmod, &opts->s->lfe)) {
            fprintf(stderr, "invalid input channel configuration\n");
            return 1;
        }
        opts->chmap = 1;
    }

    // disallow piped input with multiple files
    if(opts->num_input_files > 1) {
        for(i=0; i<opts->num_input_files; i++) {
            if(!strncmp(opts->infile[i], "-", 2)) {
                fprintf(stderr, "cannot use piped input with multiple files\n");
                return 1;
            }
        }
    }

    // disallow infile & outfile with same name except with piping
    for(i=0; i<opts->num_input_files; i++) {
        if(strncmp(opts->infile[i], "-", 2) && strncmp(opts->outfile, "-", 2)) {
            if(!strcmp(opts->infile[i], opts->outfile)) {
                fprintf(stderr, "output filename cannot match any input filename\n");
                return 1;
            }
        }
    }

    return 0;
}
