/**
 * Aften: A/52 audio encoder
 * Copyright (c) 2007  Justin Ruggles
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
#include "helptext.h"

void
print_usage(FILE *out)
{
    fprintf(out, "%s", usage_heading);
    fprintf(out, "type 'aften -h' for more details.\n\n");
}

void
print_long_help(FILE *out)
{
    int i, j;

    fprintf(out, "%s", usage_heading);
    fprintf(out, "options:\n\n");

    for(i=0; i<LONG_HELP_SECTIONS_COUNT; i++) {
        fprintf(out, "%s\n", long_help_sections[i].section_heading);
        for(j=0; j<long_help_sections[i].section_count; j++) {
            fprintf(out, "%s\n", long_help_sections[i].section_options[j]);
        }
    }
}

void
print_help(FILE *out)
{
    int i;

    fprintf(out, "usage: aften [options] <input.wav> <output.ac3>\n"
                 "options:\n");
    for(i=0; i<HELP_OPTIONS_COUNT; i++) {
        fprintf(out, "%s", help_options[i]);
    }
    fprintf(out, "\n");
}

int
parse_commandline(int argc, char **argv, CommandOptions *opts)
{
    int i;
    int found_input = 0;
    int found_output = 0;

    if(argc < 2) {
        return 1;
    }

    opts->chmap = 0;
    opts->infile = argv[1];
    opts->outfile = argv[2];
    opts->pad_start = 1;

    for(i=1; i<argc; i++) {
        if(argv[i][0] == '-' && argv[i][1] != '\0') {
            if(argv[i][2] != '\0') {
                // multi-character arguments
                if(!strncmp(&argv[i][1], "cmix", 5)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->meta.cmixlev = atoi(argv[i]);
                    if(opts->s->meta.cmixlev < 0 || opts->s->meta.cmixlev > 2) {
                        fprintf(stderr, "invalid cmix: %d. must be 0 to 2.\n", opts->s->meta.cmixlev);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "smix", 5)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->meta.surmixlev = atoi(argv[i]);
                    if(opts->s->meta.surmixlev < 0 || opts->s->meta.surmixlev > 2) {
                        fprintf(stderr, "invalid smix: %d. must be 0 to 2.\n", opts->s->meta.surmixlev);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "dsur", 5)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->meta.dsurmod = atoi(argv[i]);
                    if(opts->s->meta.dsurmod < 0 || opts->s->meta.dsurmod > 2) {
                        fprintf(stderr, "invalid dsur: %d. must be 0 to 2.\n", opts->s->meta.dsurmod);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "dnorm", 6)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->meta.dialnorm = atoi(argv[i]);
                    if(opts->s->meta.dialnorm < 0 || opts->s->meta.dialnorm > 31) {
                        fprintf(stderr, "invalid dnorm: %d. must be 0 to 31.\n", opts->s->meta.dialnorm);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "dynrng", 7)) {
                    int profile;
                    i++;
                    if(i >= argc) return 1;
                    profile = atoi(argv[i]);
                    if(profile < 0 || profile > 5) {
                        fprintf(stderr, "invalid dynrng: %d. must be 0 to 5.\n", profile);
                        return 1;
                    }
                    opts->s->params.dynrng_profile = (DynRngProfile) profile;
                } else if(!strncmp(&argv[i][1], "acmod", 6)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->acmod = atoi(argv[i]);
                    if(opts->s->acmod < 0 || opts->s->acmod > 7) {
                        fprintf(stderr, "invalid acmod: %d. must be 0 to 7.\n", opts->s->acmod);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "lfe", 4)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->lfe = atoi(argv[i]);
                    if(opts->s->lfe < 0 || opts->s->lfe > 1) {
                        fprintf(stderr, "invalid lfe: %d. must be 0 or 1.\n", opts->s->lfe);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "bwfilter", 9)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->params.use_bw_filter = atoi(argv[i]);
                    if(opts->s->params.use_bw_filter < 0 || opts->s->params.use_bw_filter > 1) {
                        fprintf(stderr, "invalid bwfilter: %d. must be 0 or 1.\n", opts->s->params.use_bw_filter);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "dcfilter", 9)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->params.use_dc_filter = atoi(argv[i]);
                    if(opts->s->params.use_dc_filter < 0 || opts->s->params.use_dc_filter > 1) {
                        fprintf(stderr, "invalid dcfilter: %d. must be 0 or 1.\n", opts->s->params.use_dc_filter);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "lfefilter", 10)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->params.use_lfe_filter = atoi(argv[i]);
                    if(opts->s->params.use_lfe_filter < 0 || opts->s->params.use_lfe_filter > 1) {
                        fprintf(stderr, "invalid lfefilter: %d. must be 0 or 1.\n", opts->s->params.use_lfe_filter);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "xbsi1", 6)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->meta.xbsi1e = atoi(argv[i]);
                    if(opts->s->meta.xbsi1e < 0 || opts->s->meta.xbsi1e > 1) {
                        fprintf(stderr, "invalid xbsi1: %d. must be 0 or 1.\n", opts->s->meta.xbsi1e);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "dmixmod", 8)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->meta.dmixmod = atoi(argv[i]);
                    if(opts->s->meta.dmixmod < 0 || opts->s->meta.dmixmod > 2) {
                        fprintf(stderr, "invalid dmixmod: %d. must be 0 to 2.\n", opts->s->meta.dmixmod);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "ltrtcmix", 9)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->meta.ltrtcmixlev = atoi(argv[i]);
                    if(opts->s->meta.ltrtcmixlev < 0 || opts->s->meta.ltrtcmixlev > 7) {
                        fprintf(stderr, "invalid ltrtcmix: %d. must be 0 to 7.\n", opts->s->meta.ltrtcmixlev);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "ltrtsmix", 9)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->meta.ltrtsmixlev = atoi(argv[i]);
                    if(opts->s->meta.ltrtsmixlev < 0 || opts->s->meta.ltrtsmixlev > 7) {
                        fprintf(stderr, "invalid ltrtsmix: %d. must be 0 to 7.\n", opts->s->meta.ltrtsmixlev);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "lorocmix", 9)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->meta.lorocmixlev = atoi(argv[i]);
                    if(opts->s->meta.lorocmixlev < 0 || opts->s->meta.lorocmixlev > 7) {
                        fprintf(stderr, "invalid lorocmix: %d. must be 0 to 7.\n", opts->s->meta.lorocmixlev);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "lorosmix", 9)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->meta.lorosmixlev = atoi(argv[i]);
                    if(opts->s->meta.lorosmixlev < 0 || opts->s->meta.lorosmixlev > 7) {
                        fprintf(stderr, "invalid lorosmix: %d. must be 0 to 7.\n", opts->s->meta.lorosmixlev);
                        return 1;
                    }
                    opts->s->meta.xbsi1e = 1;
                } else if(!strncmp(&argv[i][1], "xbsi2", 6)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->meta.xbsi2e = atoi(argv[i]);
                    if(opts->s->meta.xbsi2e < 0 || opts->s->meta.xbsi2e > 1) {
                        fprintf(stderr, "invalid xbsi2: %d. must be 0 or 1.\n", opts->s->meta.xbsi2e);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "dsurexmod", 10)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->meta.dsurexmod = atoi(argv[i]);
                    if(opts->s->meta.dsurexmod < 0 || opts->s->meta.dsurexmod > 2) {
                        fprintf(stderr, "invalid dsurexmod: %d. must be 0 to 2.\n", opts->s->meta.dsurexmod);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "dheadphon", 10)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->meta.dheadphonmod = atoi(argv[i]);
                    if(opts->s->meta.dheadphonmod < 0 || opts->s->meta.dheadphonmod > 2) {
                        fprintf(stderr, "invalid dheadphon: %d. must be 0 to 2.\n", opts->s->meta.dheadphonmod);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "adconvtyp", 10)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->meta.adconvtyp = atoi(argv[i]);
                    if(opts->s->meta.adconvtyp < 0 || opts->s->meta.adconvtyp > 1) {
                        fprintf(stderr, "invalid adconvtyp: %d. must be 0 or 1.\n", opts->s->meta.adconvtyp);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "fba", 4)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->params.bitalloc_fast = atoi(argv[i]);
                    if(opts->s->params.bitalloc_fast < 0 || opts->s->params.bitalloc_fast > 1) {
                        fprintf(stderr, "invalid fba: %d. must be 0 or 1.\n", opts->s->params.bitalloc_fast);
                        return 1;
                    }
                }  else if(!strncmp(&argv[i][1], "fes", 4)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->params.expstr_fast = atoi(argv[i]);
                    if(opts->s->params.expstr_fast < 0 || opts->s->params.expstr_fast > 1) {
                        fprintf(stderr, "invalid fes: %d. must be 0 or 1.\n", opts->s->params.expstr_fast);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "longhelp", 9)) {
                    return 3;
                } else if(!strncmp(&argv[i][1], "chmap", 6)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->chmap = atoi(argv[i]);
                    if(opts->chmap < 0 || opts->chmap > 1) {
                        fprintf(stderr, "invalid chmap: %d. must be 0 or 1.\n", opts->chmap);
                        return 1;
                    }
                } else if(!strncmp(&argv[i][1], "pad", 4)) {
                    i++;
                    if(i >= argc) return 1;
                    opts->pad_start = atoi(argv[i]);
                    if(opts->pad_start < 0 || opts->pad_start > 1) {
                        fprintf(stderr, "invalid pad: %d. must be 0 or 1.\n", opts->pad_start);
                        return 1;
                    }
                }
            } else {
                // single-character arguments
                if(argv[i][1] == 'h') {
                    return 2;
                } else if(argv[i][1] == 'v') {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->params.verbose = atoi(argv[i]);
                    if(opts->s->params.verbose < 0 || opts->s->params.verbose > 2) {
                        fprintf(stderr, "invalid verbosity level: %d. must be 0 to 2.\n", opts->s->params.verbose);
                        return 1;
                    }
                } else if(argv[i][1] == 'b') {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->params.bitrate = atoi(argv[i]);
                    if(opts->s->params.bitrate < 0 || opts->s->params.bitrate > 640) {
                        fprintf(stderr, "invalid bitrate: %d. must be 0 to 640.\n", opts->s->params.bitrate);
                        return 1;
                    }
                } else if(argv[i][1] == 'q') {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->params.quality = atoi(argv[i]);
                    opts->s->params.encoding_mode = AFTEN_ENC_MODE_VBR;
                    if(opts->s->params.quality < 0 || opts->s->params.quality > 1023) {
                        fprintf(stderr, "invalid quality: %d. must be 0 to 1023.\n", opts->s->params.quality);
                        return 1;
                    }
                } else if(argv[i][1] == 'w') {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->params.bwcode = atoi(argv[i]);
                    if(opts->s->params.bwcode < -2 || opts->s->params.bwcode > 60) {
                        fprintf(stderr, "invalid bandwidth code: %d. must be 0 to 60.\n", opts->s->params.bwcode);
                        return 1;
                    }
                } else if(argv[i][1] == 'm') {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->params.use_rematrixing = atoi(argv[i]);
                    if(opts->s->params.use_rematrixing < 0 || opts->s->params.use_rematrixing > 1) {
                        fprintf(stderr, "invalid stereo rematrixing: %d. must be 0 or 1.\n", opts->s->params.use_rematrixing);
                        return 1;
                    }
                } else if(argv[i][1] == 's') {
                    i++;
                    if(i >= argc) return 1;
                    opts->s->params.use_block_switching = atoi(argv[i]);
                    if(opts->s->params.use_block_switching < 0 || opts->s->params.use_block_switching > 1) {
                        fprintf(stderr, "invalid block switching: %d. must be 0 or 1.\n", opts->s->params.use_block_switching);
                        return 1;
                    }
                }
            }
        } else {
            if(!found_input) {
                opts->infile = argv[i];
                found_input = 1;
            } else {
                if(found_output) return 1;
                opts->outfile = argv[i];
                found_output = 1;
            }
        }
    }
    if(!found_input || !found_output) {
        return 1;
    }
    // disallow infile & outfile with same name except with piping
    if(strncmp(opts->infile, "-", 2) && strncmp(opts->outfile, "-", 2)) {
        if(!strcmp(opts->infile, opts->outfile)) {
            fprintf(stderr, "output filename cannot match input filename\n");
            return 1;
        }
    }
    return 0;
}
