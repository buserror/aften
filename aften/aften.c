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
 * @file aften.c
 * Commandline encoder frontend
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

#include "aften.h"
#include "wav.h"

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
                 "(c) 2006-2007 Justin Ruggles, Prakash Punnoor, et al.\n\n", vers);
}

static void
print_usage(FILE *out)
{
    fprintf(out, "usage: aften [options] <input.wav> <output.ac3>\n"
                 "type 'aften -h' for more details.\n\n");
}

static void
print_long_help(FILE *out)
{
    fprintf(out,
"usage: aften [options] <input.wav> <output.ac3>\n"
"options:\n\n"
"CONSOLE OUTPUT OPTIONS\n"
"\n"
"    [-h]           Print out list of commandline options\n"
"\n"
"    [-longhelp]    Print out commandline option details\n"
"\n"
"    [-v #]         Verbosity\n"
"                       This controls the level of console output to stderr.\n"
"                       0 - Quiet Mode. No output to stderr.\n"
"                       1 - Shows a running average of encoding statistics.\n"
"                           This is the default setting.\n"
"                       2 - Shows the statistics for each frame.\n"
"\n"
"ENCODING OPTIONS\n"
"\n"
"    [-b #]         CBR bitrate in kbps\n"
"                       CBR mode is selected by default. This option allows for\n"
"                       setting the fixed bitrate. The default bitrate depends\n"
"                       on the number of channels (not including LFE).\n"
"                       1 = 96 kbps\n"
"                       2 = 192 kbps\n"
"                       3 = 256 kbps\n"
"                       4 = 384 kbps\n"
"                       5 = 448 kbps\n"
"\n"
"    [-q #]         VBR quality\n"
"                       A value 0 to 1023 which corresponds to SNR offset, where\n"
"                       q=240 equates to an SNR offset of 0.  240 is the default\n"
"                       value.  This scale will most likely be replaced in the\n"
"                       future with a better quality measurement.\n"
"\n"
"    [-fba #]      Fast bit allocation\n"
"                       The current CBR bit allocation algorithm does a search\n"
"                       for the highest SNR offset value which still allows for\n"
"                       the data to fit in the fixed-sized frame.  It is split\n"
"                       into 2 stages.  The first stage narrows down the SNR\n"
"                       value to within 16 of the target.  The second stage does\n"
"                       a weighted estimate followed by a fine-level search.\n"
"                       Turning on fast bit allocation skips the 2nd step.\n"
"\n"
"    [-w #]         Bandwidth\n"
"                       The bandwidth setting corresponds to the high-frequency\n"
"                       cutoff.  Specifically, it sets the highest frequency bin\n"
"                       which is encoded.  The AC-3 format uses a 512-point MDCT\n"
"                       which gives 256 frequency levels from 0 to 1/2 of the\n"
"                       samplerate.  The formula to give the number of coded\n"
"                       frequency bins from bandwidth setting is:\n"
"                       (w * 3) + 73, which gives a range of 73 to 253\n"
"                       I hope to replace this setting with one where the user\n"
"                       specifies the actual cutoff frequency rather than the\n"
"                       bandwidth code.\n"
"                       There are 2 special values, -1 and -2.\n"
"                       When -1 is used, Aften automatically selects what\n"
"                       it thinks is an appropriate bandwidth.  This is the\n"
"                       default setting.\n"
"                       When -2 is used, a bandwidth is chosen for each frame\n"
"                       based on the results from the previous frame.\n"
"\n"
"    [-m #]         Stereo rematrixing\n"
"                       Using stereo rematrixing can increase quality by\n"
"                       removing redundant information between the left and\n"
"                       right channels.  This technique is common in audio\n"
"                       encoding, and is sometimes called mid/side encoding.\n"
"                       When this setting is turned on, Aften adaptively turns\n"
"                       rematrixing on or off for each of 4 frequency bands for\n"
"                       each block.  When this setting is turned off,\n"
"                       rematrixing is not used for any blocks. The default\n"
"                       value is 1.\n"
"\n");
fprintf(out,
"    [-s #]         Block switching\n"
"                       The AC-3 format allows for 2 different types of MDCT\n"
"                       transformations to translate from time-domain to\n"
"                       frequency-domain.  The default is a 512-point transform,\n"
"                       which gives better frequency resolution.  There is also\n"
"                       a 256-point transform, which gives better time\n"
"                       resolution.  The specification gives a suggested method\n"
"                       for determining when to use the 256-point transform.\n"
"                       When block switching is turned on, Aften uses the spec\n"
"                       method for selecting the 256-point MDCT.  When it is\n"
"                       turned off, only the 512-point MDCT is used, which is\n"
"                       faster.  Block switching is turned off by default.\n"
"\n"
"BITSTREAM INFO METADATA\n"
"\n"
"    [-cmix #]      Center mix level\n"
"                       When three front channels are in use, this code\n"
"                       indicates the nominal down mix level of the center\n"
"                       channel with respect to the left and right channels.\n"
"                       0 = -3.0 dB (default)\n"
"                       1 = -4.5 dB\n"
"                       2 = -6.0 dB\n"
"\n"
"    [-smix #]      Surround mix level\n"
"                       If surround channels are in use, this code indicates\n"
"                       the nominal down mix level of the surround channels.\n"
"                       0 = -3 dB (default)\n"
"                       1 = -6 dB\n"
"                       2 = 0\n"
"\n"
"    [-dsur #]      Dolby Surround mode\n"
"                       When operating in the two channel mode, this code\n"
"                       indicates whether or not the program has been encoded in\n"
"                       Dolby Surround.  This information is not used by the\n"
"                       AC-3 decoder, but may be used by other portions of the\n"
"                       audio reproduction equipment.\n"
"                       0 = not indicated (default)\n"
"                       1 = not Dolby surround encoded\n"
"                       2 = Dolby surround encoded\n"
"\n"
"DYNAMIC RANGE COMPRESSION AND DIALOG NORMALIZATION\n"
"\n"
"    [-dynrng #]    Dynamic Range Compression profile\n"
"                       Dynamic Range Compression allows for the final output\n"
"                       dynamic range to be limited without sacrificing quality.\n"
"                       The full dynamic range audio is still encoded, but a\n"
"                       code is given for each block which tells the decoder to\n"
"                       adjust the output volume for that block.  The encoder\n"
"                       must analyze the input audio to determine the best way\n"
"                       to compress the dynamic range based on the loudness and\n"
"                       type of input (film, music, speech).\n"
"                       0 = Film Light\n"
"                       1 = Film Standard\n"
"                       2 = Music Light\n"
"                       3 = Music Standard\n"
"                       4 = Speech\n"
"                       5 = None (default)\n"
"\n"
"    [-dnorm #]     Dialog normalization [0 - 31] (default: 31)\n"
"                       The dialog normalization value sets the average dialog\n"
"                       level.  The value is typically constant for a particular\n"
"                       audio program.  The decoder has a target output dialog\n"
"                       level of -31dB, so if the dialog level is specified as\n"
"                       being -31dB already (default), the output volume is not\n"
"                       altered.  Otherwise, the overall output volume is\n"
"                       decreased so that the dialog level is adjusted down to\n"
"                       -31dB.\n"
"\n");
fprintf(out,
"CHANNEL OPTIONS\n"
"\n"
"    By default, the channel configuration is determined by the input file wav\n"
"    header.  However, while the extensible wav format can specify all possible\n"
"    options in AC-3, the basic wav format cannot.  The acmod and lfe options\n"
"    allow the user to explicitly select the desired channel layout.  This only\n"
"    controls the interpretation of the input, so no downmixing or upmixing is\n"
"    done.\n"
"\n"
"    [-acmod #]     Audio coding mode (overrides wav header)\n"
"                       0 = 1+1 (Ch1,Ch2)\n"
"                       1 = 1/0 (C)\n"
"                       2 = 2/0 (L,R)\n"
"                       3 = 3/0 (L,R,C)\n"
"                       4 = 2/1 (L,R,S)\n"
"                       5 = 3/1 (L,R,C,S)\n"
"                       6 = 2/2 (L,R,SL,SR)\n"
"                       7 = 3/2 (L,R,C,SL,SR)\n"
"\n"
"    [-lfe #]       Specify use of LFE channel (overrides wav header)\n"
"                       0 = LFE channel is not present\n"
"                       1 = LFE channel is present\n"
"\n"
"    [-chmap #]     Channel mapping order of input audio\n"
"                       Some programs create wav files which use AC-3 channel\n"
"                       mapping rather than standard wav mapping.  This option\n"
"                       allows the user to specify if the input file uses AC-3\n"
"                       or wav channel mapping.\n"
"                       0 = wav mapping (default)\n"
"                       1 = AC-3 mapping\n"
"\n"
"INPUT FILTERS\n"
"\n"
"    [-bwfilter #]  Specify use of the bandwidth low-pass filter\n"
"                       The bandwidth low-pass filter pre-filters the input\n"
"                       audio before converting to frequency-domain.  This\n"
"                       smooths the cutoff frequency transition for slightly\n"
"                       better quality.\n"
"                       0 = do not apply filter (default)\n"
"                       1 = apply filter\n"
"\n"
"    [-dcfilter #]  Specify use of the DC high-pass filter\n"
"                       The DC high-pass filter is listed as optional by the\n"
"                       AC-3 specification.  The implementation, as suggested,\n"
"                       is a single pole filter at 3 Hz.\n"
"                       0 = do not apply filter (default)\n"
"                       1 = apply filter\n"
"\n"
"    [-lfefilter #] Specify use of the LFE low-pass filter\n"
"                       The LFE low-pass filter is recommended by the AC-3\n"
"                       specification.  The cutoff is 120 Hz.  The specification\n"
"                       recommends an 8th order elliptic filter, but instead,\n"
"                       Aften uses a Butterworth 2nd order cascaded direct\n"
"                       form II filter.\n"
"                       0 = do not apply filter (default)\n"
"                       1 = apply filter\n"
"\n"
"ALTERNATE BIT STREAM SYNTAX\n"
"\n"
"    The alternate bit stream syntax allows for encoding additional metadata by\n"
"    adding 1 or 2 extended bitstream info sections to each frame.\n"
"\n"
"    [-xbsi1 #]     Specify use of extended bitstream info 1\n"
"                       Extended bitstream info 1 contains the dmixmod,\n"
"                       ltrtcmix, ltrtsmix, lorocmix, and lorosmix fields.  If\n"
"                       this option is turned on, all these values are written\n"
"                       to the output stream.\n"
"                       0 = do not write xbsi1\n"
"                       1 = write xbsi1\n"
"\n"
"    [-dmixmod #]   Preferred stereo downmix mode\n"
"                       This code indicates the type of stereo downmix preferred\n"
"                       by the mastering engineer, and can be optionally used,\n"
"                       overridden, or ignored by the decoder.\n"
"                       0 = not indicated (default)\n"
"                       1 = Lt/Rt downmix preferred\n"
"                       2 = Lo/Ro downmix preferred\n"
"\n");
fprintf(out,
"    The 4 mix level options below all use the same code table:\n"
"                       0 = +3.0 dB\n"
"                       1 = +1.5 dB\n"
"                       2 =  0.0 dB\n"
"                       3 = -1.5 dB\n"
"                       4 = -3.0 dB (default)\n"
"                       5 = -4.5 dB\n"
"                       6 = -6.0 dB\n"
"                       7 = -inf dB\n"
"\n"
"    [-ltrtcmix #]  Lt/Rt center mix level\n"
"                       This code indicates the nominal down mix level of the\n"
"                       center channel with respect to the left and right\n"
"                       channels in an Lt/Rt downmix.\n"
"\n"
"    [-ltrtsmix #]  Lt/Rt surround mix level\n"
"                       This code indicates the nominal down mix level of the\n"
"                       surround channels with respect to the left and right\n"
"                       channels in an Lt/Rt downmix.\n"
"\n"
"    [-lorocmix #]  Lo/Ro center mix level\n"
"                       This code indicates the nominal down mix level of the\n"
"                       center channel with respect to the left and right\n"
"                       channels in an Lo/Ro downmix.\n"
"\n"
"    [-lorosmix #]  Lo/Ro surround mix level\n"
"                       This code indicates the nominal down mix level of the\n"
"                       surround channels with respect to the left and right\n"
"                       channels in an Lo/Ro downmix.\n"
"\n"
"    [-xbsi2 #]     Specify use of extended bitstream info 2\n"
"                       Extended bitstream info 2 contains the dsurexmod,\n"
"                       dheadphon, and adconvtyp fields.  If this option is\n"
"                       turned on, all these values are written to the output\n"
"                       stream.  These options are not used by the AC-3 decoder,\n"
"                       but may be used by other portions of the audio\n"
"                       reproduction equipment.\n"
"                       0 = do not write xbsi2\n"
"                       1 = write xbsi2\n"
"\n"
"    [-dsurexmod #] Dolby Surround EX mode\n"
"                       This code indicates whether or not the program has been\n"
"                       encoded in Dolby Surround EX.\n"
"                       0 = not indicated (default)\n"
"                       1 = Not Dolby Surround EX encoded\n"
"                       2 = Dolby Surround EX encoded\n"
"\n"
"    [-dheadphon #] Dolby Headphone mode\n"
"                       This code indicates whether or not the program has been\n"
"                       Dolby Headphone-encoded.\n"
"                       0 = not indicated (default)\n"
"                       1 = Not Dolby Headphone encoded\n"
"                       2 = Dolby Headphone encoded\n"
"\n"
"    [-adconvtyp #] A/D converter type\n"
"                       This code indicates the type of A/D converter technology\n"
"                       used to capture the PCM audio.\n"
"                       0 = Standard (default)\n"
"                       1 = HDCD\n"
"\n");
}

static void
print_help(FILE *out)
{
    fprintf(out, "usage: aften [options] <input.wav> <output.ac3>\n"
                 "options:\n"
                 "    [-h]           Print out list of commandline options\n"
                 "    [-longhelp]    Print out commandline option details\n"
                 "    [-v #]         Verbosity (controls output to console)\n"
                 "                       0 = quiet mode\n"
                 "                       1 = show average stats (default)\n"
                 "                       2 = show each frame's stats\n"
                 "    [-b #]         CBR bitrate in kbps (default: about 96kbps per channel)\n"
                 "    [-q #]         VBR quality [0 - 1023] (default: 240)\n"
                 "    [-fba #]       Fast bit allocation (default: 0)\n"
                 "                       0 = more accurate encoding\n"
                 "                       1 = faster encoding\n"
                 "    [-w #]         Bandwidth\n"
                 "                       0 to 60 = fixed bandwidth (28%%-99%% of full bandwidth)\n"
                 "                      -1 = fixed adaptive bandwidth (default)\n"
                 "                      -2 = variable adaptive bandwidth\n"
                 "    [-m #]         Stereo rematrixing\n"
                 "                       0 = independent L+R channels\n"
                 "                       1 = mid/side rematrixing (default)\n"
                 "    [-s #]         Block switching\n"
                 "                       0 = use only 512-point MDCT (default)\n"
                 "                       1 = selectively use 256-point MDCT\n"
                 "    [-cmix #]      Center mix level\n"
                 "                       0 = -3.0 dB (default)\n"
                 "                       1 = -4.5 dB\n"
                 "                       2 = -6.0 dB\n"
                 "    [-smix #]      Surround mix level\n"
                 "                       0 = -3 dB (default)\n"
                 "                       1 = -6 dB\n"
                 "                       2 = 0\n"
                 "    [-dsur #]      Dolby Surround mode\n"
                 "                       0 = not indicated (default)\n"
                 "                       1 = not Dolby surround encoded\n"
                 "                       2 = Dolby surround encoded\n"
                 "    [-dnorm #]     Dialog normalization [0 - 31] (default: 31)\n"
                 "    [-dynrng #]    Dynamic Range Compression profile\n"
                 "                       0 = Film Light\n"
                 "                       1 = Film Standard\n"
                 "                       2 = Music Light\n"
                 "                       3 = Music Standard\n"
                 "                       4 = Speech\n"
                 "                       5 = None (default)\n"
                 "    [-acmod #]     Audio coding mode (overrides wav header)\n"
                 "                       0 = 1+1 (Ch1,Ch2)\n"
                 "                       1 = 1/0 (C)\n"
                 "                       2 = 2/0 (L,R)\n"
                 "                       3 = 3/0 (L,R,C)\n"
                 "                       4 = 2/1 (L,R,S)\n"
                 "                       5 = 3/1 (L,R,C,S)\n"
                 "                       6 = 2/2 (L,R,SL,SR)\n"
                 "                       7 = 3/2 (L,R,C,SL,SR)\n"
                 "    [-lfe #]       Specify use of LFE channel (overrides wav header)\n"
                 "                       0 = LFE channel is not present\n"
                 "                       1 = LFE channel is present\n"
                 "    [-chmap #]     Channel mapping order of input audio\n"
                 "                       0 = wav mapping (default)\n"
                 "                       1 = AC-3 mapping\n"
                 "    [-bwfilter #]  Specify use of the bandwidth low-pass filter\n"
                 "                       0 = do not apply filter (default)\n"
                 "                       1 = apply filter\n"
                 "    [-dcfilter #]  Specify use of the DC high-pass filter\n"
                 "                       0 = do not apply filter (default)\n"
                 "                       1 = apply filter\n"
                 "    [-lfefilter #] Specify use of the LFE low-pass filter\n"
                 "                       0 = do not apply filter (default)\n"
                 "                       1 = apply filter\n");
    fprintf(out, "    [-xbsi1 #]     Specify use of extended bitstream info 1\n"
                 "                       0 = do not write xbsi1\n"
                 "                       1 = write xbsi1\n"
                 "    [-dmixmod #]   Preferred stereo downmix mode\n"
                 "                       0 = not indicated (default)\n"
                 "                       1 = Lt/Rt downmix preferred\n"
                 "                       2 = Lo/Ro downmix preferred\n"
                 "    [-ltrtcmix #]  Lt/Rt center mix level\n"
                 "    [-ltrtsmix #]  Lt/Rt surround mix level\n"
                 "    [-lorocmix #]  Lo/Ro center mix level\n"
                 "    [-lorosmix #]  Lo/Ro surround mix level\n"
                 "                       0 = +3.0 dB\n"
                 "                       1 = +1.5 dB\n"
                 "                       2 =  0.0 dB\n"
                 "                       3 = -1.5 dB\n"
                 "                       4 = -3.0 dB (default)\n"
                 "                       5 = -4.5 dB\n"
                 "                       6 = -6.0 dB\n"
                 "                       7 = -inf dB\n"
                 "    [-xbsi2 #]     Specify use of extended bitstream info 2\n"
                 "                       0 = do not write xbsi2\n"
                 "                       1 = write xbsi2\n"
                 "    [-dsurexmod #] Dolby Surround EX mode\n"
                 "                       0 = not indicated (default)\n"
                 "                       1 = Not Dolby Surround EX encoded\n"
                 "                       2 = Dolby Surround EX encoded\n"
                 "    [-dheadphon #] Dolby Headphone mode\n"
                 "                       0 = not indicated (default)\n"
                 "                       1 = Not Dolby Headphone encoded\n"
                 "                       2 = Dolby Headphone encoded\n"
                 "    [-adconvtyp #] A/D converter type\n"
                 "                       0 = Standard (default)\n"
                 "                       1 = HDCD\n"
                 "\n");
}

typedef struct CommandOptions {
    int chmap;
    char *infile;
    char *outfile;
    AftenContext *s;
} CommandOptions;

static int
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
                    opts->s->params.dynrng_profile = profile;
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

int
main(int argc, char **argv)
{
    uint8_t *frame;
    FLOAT *fwav;
    int nr, fs, err;
    FILE *ifp;
    FILE *ofp;
    WavFile wf;
    CommandOptions opts;
    AftenContext s;
    uint32_t samplecount, bytecount, t0, t1, percent;
    FLOAT kbps, qual, bw;
    int last_frame;
    int frame_cnt;
    int done;

    opts.s = &s;
    aften_set_defaults(&s);
    err = parse_commandline(argc, argv, &opts);
    if(err) {
        if(err == 2 || err == 3) {
            print_intro(stdout);
            if(err == 2) {
                print_help(stdout);
            } else {
                print_long_help(stdout);
            }
            return 0;
        } else {
            print_intro(stderr);
            print_usage(stderr);
            return 1;
        }
    }

    if(s.params.verbose > 0) {
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

    // initialize wavfile using input
    if(wavfile_init(&wf, ifp)) {
        fprintf(stderr, "invalid wav file: %s\n", argv[1]);
        return 1;
    }
    // print wav info to console
    if(s.params.verbose > 0) {
        fprintf(stderr, "input format: ");
        wavfile_print(stderr, &wf);
    }

    // if acmod is given on commandline, determine lfe from number of channels
    if(s.acmod >= 0) {
        int ch = acmod_to_ch[s.acmod];
        if(ch == wf.channels) {
            if(s.lfe < 0) {
                s.lfe = 0;
            } else {
                if(s.lfe != 0) {
                    fprintf(stderr, "acmod and lfe do not match number of channels\n");
                    return 1;
                }
            }
        } else if(ch == (wf.channels - 1)) {
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
    }
    // if acmod is not given on commandline, determine from WAVE file
    if(s.acmod < 0) {
        int ch = wf.channels;
        if(s.lfe >= 0) {
            if(s.lfe == 0 && ch == 6) {
                fprintf(stderr, "cannot encode 6 channels w/o LFE\n");
                return 1;
            } else if(s.lfe == 1 && ch == 1) {
                fprintf(stderr, "cannot encode LFE channel only\n");
                return 1;
            }
            if(s.lfe) {
                wf.ch_mask |= 0x08;
            }
        }
        if(aften_wav_channels_to_acmod(ch, wf.ch_mask, &s.acmod, &s.lfe)) {
            fprintf(stderr, "mismatch in channels, acmod, and lfe params\n");
            return 1;
        }
    }
    // set some encoding parameters using wav info
    s.channels = wf.channels;
    s.samplerate = wf.sample_rate;
#ifdef CONFIG_DOUBLE
    wf.read_format = WAV_SAMPLE_FMT_DBL;
    s.sample_format = A52_SAMPLE_FMT_DBL;
#else
    wf.read_format = WAV_SAMPLE_FMT_FLT;
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
    if(s.params.verbose > 0) {
        fprintf(stderr, "output format: %d Hz %s", s.samplerate, acmod_str[s.acmod]);
        if(s.lfe) {
            fprintf(stderr, " + LFE");
        }
        fprintf(stderr, "\n\n");
    }

    // allocate memory for coded frame and sample buffer
    frame = calloc(A52_MAX_CODED_FRAME_SIZE, 1);
    fwav = calloc(A52_SAMPLES_PER_FRAME * wf.channels, sizeof(FLOAT));
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
    nr = wavfile_read_samples(&wf, fwav, A52_SAMPLES_PER_FRAME);
    while(nr > 0 || fs > 0) {
        if(opts.chmap == 0) {
            aften_remap_wav_to_a52(fwav, nr, wf.channels, s.sample_format,
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
            for(i=nr*wf.channels; i<A52_SAMPLES_PER_FRAME*wf.channels; i++) {
                fwav[i] = 0.0;
            }
        }

        fs = aften_encode_frame(&s, frame, done ? NULL : fwav);

        if(fs < 0) {
            fprintf(stderr, "Error encoding frame %d\n", frame_cnt);
        } else {
            if(s.params.verbose > 0) {
                samplecount += A52_SAMPLES_PER_FRAME;
                bytecount += fs;
                qual += s.status.quality;
                bw += s.status.bwcode;
                if(s.params.verbose == 1) {
                    t1 = samplecount / wf.sample_rate;
                    if(frame_cnt > 0 && (t1 > t0 || samplecount >= wf.samples)) {
                        kbps = (bytecount * FCONST(8.0) * wf.sample_rate) /
                               (FCONST(1000.0) * samplecount);
                        percent = 0;
                        if(wf.samples > 0) {
                            percent = (uint32_t)((samplecount * FCONST(100.0)) /
                                      wf.samples);
                            percent = CLIP(percent, 0, 100);
                        }
                        fprintf(stderr, "\rprogress: %3u%% | q: %4.1f | "
                                        "bw: %2.1f | bitrate: %4.1f kbps ",
                                percent, (qual / (frame_cnt+1)),
                                (bw / (frame_cnt+1)), kbps);
                    }
                    t0 = t1;
                } else if(s.params.verbose == 2) {
                    fprintf(stderr, "frame: %7d | q: %4d | bw: %2d | bitrate: %3d kbps\n",
                            frame_cnt, s.status.quality, s.status.bwcode,
                            s.status.bit_rate);
                }
            }
            fwrite(frame, 1, fs, ofp);
        }
        frame_cnt++;
        last_frame = nr;
        nr = wavfile_read_samples(&wf, fwav, A52_SAMPLES_PER_FRAME);
    }
    if(s.params.verbose == 1) {
        fprintf(stderr, "\n\n");
    } else if(s.params.verbose == 2) {
        if(samplecount > 0) {
            kbps = (bytecount * FCONST(8.0) * wf.sample_rate) / (FCONST(1000.0) * samplecount);
        } else {
            kbps = 0;
        }
        frame_cnt = MAX(frame_cnt, 1);
        fprintf(stderr, "\n");
        fprintf(stderr, "average quality:   %4.1f\n", (qual / frame_cnt));
        fprintf(stderr, "average bandwidth: %2.1f\n", (bw / frame_cnt));
        fprintf(stderr, "average bitrate:   %4.1f kbps\n\n", kbps);
    }

    free(fwav);
    free(frame);

    fclose(ifp);
    fclose(ofp);

    aften_encode_close(&s);

    return 0;
}
