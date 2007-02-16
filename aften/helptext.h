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
 * @file helptext.h
 * string constants for commandline help and longhelp
 */

#ifndef HELPTEXT_H
#define HELPTEXT_H

static const char *usage_heading = "usage: aften [options] <input.wav> <output.ac3>\n";

#define HELP_OPTIONS_COUNT 31

static const char *help_options[HELP_OPTIONS_COUNT] = {
"    [-h]           Print out list of commandline options\n",

"    [-longhelp]    Print out commandline option details\n",

"    [-v #]         Verbosity (controls output to console)\n"
"                       0 = quiet mode\n"
"                       1 = show average stats (default)\n"
"                       2 = show each frame's stats\n",

"    [-b #]         CBR bitrate in kbps (default: about 96kbps per channel)\n",

"    [-q #]         VBR quality [0 - 1023] (default: 240)\n",

"    [-fba #]       Fast bit allocation (default: 0)\n"
"                       0 = more accurate encoding\n"
"                       1 = faster encoding\n",

"    [-pad #]       Start-of-stream padding\n"
"                       0 = no padding\n"
"                       1 = 256 samples of padding (default)\n",

"    [-w #]         Bandwidth\n"
"                       0 to 60 = fixed bandwidth (28%%-99%% of full bandwidth)\n"
"                      -1 = fixed adaptive bandwidth (default)\n"
"                      -2 = variable adaptive bandwidth\n",

"    [-m #]         Stereo rematrixing\n"
"                       0 = independent L+R channels\n"
"                       1 = mid/side rematrixing (default)\n",

"    [-s #]         Block switching\n"
"                       0 = use only 512-point MDCT (default)\n"
"                       1 = selectively use 256-point MDCT\n",

"    [-cmix #]      Center mix level\n"
"                       0 = -3.0 dB (default)\n"
"                       1 = -4.5 dB\n"
"                       2 = -6.0 dB\n",

"    [-smix #]      Surround mix level\n"
"                       0 = -3 dB (default)\n"
"                       1 = -6 dB\n"
"                       2 = 0\n",

"    [-dsur #]      Dolby Surround mode\n"
"                       0 = not indicated (default)\n"
"                       1 = not Dolby surround encoded\n"
"                       2 = Dolby surround encoded\n",

"    [-dnorm #]     Dialog normalization [0 - 31] (default: 31)\n",

"    [-dynrng #]    Dynamic Range Compression profile\n"
"                       0 = Film Light\n"
"                       1 = Film Standard\n"
"                       2 = Music Light\n"
"                       3 = Music Standard\n"
"                       4 = Speech\n"
"                       5 = None (default)\n",

"    [-acmod #]     Audio coding mode (overrides wav header)\n"
"                       0 = 1+1 (Ch1,Ch2)\n"
"                       1 = 1/0 (C)\n"
"                       2 = 2/0 (L,R)\n"
"                       3 = 3/0 (L,R,C)\n"
"                       4 = 2/1 (L,R,S)\n"
"                       5 = 3/1 (L,R,C,S)\n"
"                       6 = 2/2 (L,R,SL,SR)\n"
"                       7 = 3/2 (L,R,C,SL,SR)\n",

"    [-lfe #]       Specify use of LFE channel (overrides wav header)\n"
"                       0 = LFE channel is not present\n"
"                       1 = LFE channel is present\n",

"    [-chmap #]     Channel mapping order of input audio\n"
"                       0 = wav mapping (default)\n"
"                       1 = AC-3 mapping\n",

"    [-bwfilter #]  Specify use of the bandwidth low-pass filter\n"
"                       0 = do not apply filter (default)\n"
"                       1 = apply filter\n",

"    [-dcfilter #]  Specify use of the DC high-pass filter\n"
"                       0 = do not apply filter (default)\n"
"                       1 = apply filter\n",

"    [-lfefilter #] Specify use of the LFE low-pass filter\n"
"                       0 = do not apply filter (default)\n"
"                       1 = apply filter\n",

"    [-xbsi1 #]     Specify use of extended bitstream info 1\n"
"                       0 = do not write xbsi1\n"
"                       1 = write xbsi1\n",

"    [-dmixmod #]   Preferred stereo downmix mode\n"
"                       0 = not indicated (default)\n"
"                       1 = Lt/Rt downmix preferred\n"
"                       2 = Lo/Ro downmix preferred\n",

"    [-ltrtcmix #]  Lt/Rt center mix level\n",
"    [-ltrtsmix #]  Lt/Rt surround mix level\n",
"    [-lorocmix #]  Lo/Ro center mix level\n",
"    [-lorosmix #]  Lo/Ro surround mix level\n"
"                       0 = +3.0 dB\n"
"                       1 = +1.5 dB\n"
"                       2 =  0.0 dB\n"
"                       3 = -1.5 dB\n"
"                       4 = -3.0 dB (default)\n"
"                       5 = -4.5 dB\n"
"                       6 = -6.0 dB\n"
"                       7 = -inf dB\n",

"    [-xbsi2 #]     Specify use of extended bitstream info 2\n"
"                       0 = do not write xbsi2\n"
"                       1 = write xbsi2\n",

"    [-dsurexmod #] Dolby Surround EX mode\n"
"                       0 = not indicated (default)\n"
"                       1 = Not Dolby Surround EX encoded\n"
"                       2 = Dolby Surround EX encoded\n",

"    [-dheadphon #] Dolby Headphone mode\n"
"                       0 = not indicated (default)\n"
"                       1 = Not Dolby Headphone encoded\n"
"                       2 = Dolby Headphone encoded\n",

"    [-adconvtyp #] A/D converter type\n"
"                       0 = Standard (default)\n"
"                       1 = HDCD\n"
};


/* longhelp string constants */

#define CONSOLE_OPTIONS_COUNT 3

static const char console_heading[24] = "CONSOLE OUTPUT OPTIONS\n";
static const char *console_options[CONSOLE_OPTIONS_COUNT] = {
"    [-h]           Print out list of commandline options\n",

"    [-longhelp]    Print out commandline option details\n",

"    [-v #]         Verbosity\n"
"                       This controls the level of console output to stderr.\n"
"                       0 - Quiet Mode. No output to stderr.\n"
"                       1 - Shows a running average of encoding statistics.\n"
"                           This is the default setting.\n"
"                       2 - Shows the statistics for each frame.\n"
};

#define ENCODING_OPTIONS_COUNT 7

static const char encoding_heading[18] = "ENCODING OPTIONS\n";
static const char *encoding_options[ENCODING_OPTIONS_COUNT] = {
"    [-b #]         CBR bitrate in kbps\n"
"                       CBR mode is selected by default. This option allows for\n"
"                       setting the fixed bitrate. The default bitrate depends\n"
"                       on the number of channels (not including LFE).\n"
"                       1 = 96 kbps\n"
"                       2 = 192 kbps\n"
"                       3 = 256 kbps\n"
"                       4 = 384 kbps\n"
"                       5 = 448 kbps\n",

"    [-q #]         VBR quality\n"
"                       A value 0 to 1023 which corresponds to SNR offset, where\n"
"                       q=240 equates to an SNR offset of 0.  240 is the default\n"
"                       value.  This scale will most likely be replaced in the\n"
"                       future with a better quality measurement.\n",

"    [-fba #]      Fast bit allocation\n"
"                       The current CBR bit allocation algorithm does a search\n"
"                       for the highest SNR offset value which still allows for\n"
"                       the data to fit in the fixed-sized frame.  It is split\n"
"                       into 2 stages.  The first stage narrows down the SNR\n"
"                       value to within 16 of the target.  The second stage does\n"
"                       a weighted estimate followed by a fine-level search.\n"
"                       Turning on fast bit allocation skips the 2nd step.\n",

"    [-pad #]      Start-of-stream padding\n"
"                       The AC-3 format uses an overlap/add cycle for encoding\n"
"                       each block.  By default, Aften pads the delay buffer\n"
"                       with a block of silence to avoid inaccurate encoding\n"
"                       of the first frame of audio.  If this behavior is not\n"
"                       wanted, it can be disabled.  The pad value can be a\n"
"                       1 (default) to use padding or 0 to not use padding.\n",

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
"                       based on the results from the previous frame.\n",

"    [-m #]         Stereo rematrixing\n"
"                       Using stereo rematrixing can increase quality by\n"
"                       removing redundant information between the left and\n"
"                       right channels.  This technique is common in audio\n"
"                       encoding, and is sometimes called mid/side encoding.\n"
"                       When this setting is turned on, Aften adaptively turns\n"
"                       rematrixing on or off for each of 4 frequency bands for\n"
"                       each block.  When this setting is turned off,\n"
"                       rematrixing is not used for any blocks. The default\n"
"                       value is 1.\n",

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
"                       faster.  Block switching is turned off by default.\n",
};

#define BSI_OPTIONS_COUNT 3

static const char bsi_heading[25] = "BITSTREAM INFO METADATA\n";
static const char *bsi_options[BSI_OPTIONS_COUNT] = {
"    [-cmix #]      Center mix level\n"
"                       When three front channels are in use, this code\n"
"                       indicates the nominal down mix level of the center\n"
"                       channel with respect to the left and right channels.\n"
"                       0 = -3.0 dB (default)\n"
"                       1 = -4.5 dB\n"
"                       2 = -6.0 dB\n",

"    [-smix #]      Surround mix level\n"
"                       If surround channels are in use, this code indicates\n"
"                       the nominal down mix level of the surround channels.\n"
"                       0 = -3 dB (default)\n"
"                       1 = -6 dB\n"
"                       2 = 0\n",

"    [-dsur #]      Dolby Surround mode\n"
"                       When operating in the two channel mode, this code\n"
"                       indicates whether or not the program has been encoded in\n"
"                       Dolby Surround.  This information is not used by the\n"
"                       AC-3 decoder, but may be used by other portions of the\n"
"                       audio reproduction equipment.\n"
"                       0 = not indicated (default)\n"
"                       1 = not Dolby surround encoded\n"
"                       2 = Dolby surround encoded\n"
};

#define DRC_OPTIONS_COUNT 2

static const char drc_heading[52] = "DYNAMIC RANGE COMPRESSION AND DIALOG NORMALIZATION\n";
static const char *drc_options[DRC_OPTIONS_COUNT] = {
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
"                       5 = None (default)\n",

"    [-dnorm #]     Dialog normalization [0 - 31] (default: 31)\n"
"                       The dialog normalization value sets the average dialog\n"
"                       level.  The value is typically constant for a particular\n"
"                       audio program.  The decoder has a target output dialog\n"
"                       level of -31dB, so if the dialog level is specified as\n"
"                       being -31dB already (default), the output volume is not\n"
"                       altered.  Otherwise, the overall output volume is\n"
"                       decreased so that the dialog level is adjusted down to\n"
"                       -31dB.\n"
};

#define CHANNEL_OPTIONS_COUNT 4

static const char channel_heading[17] = "CHANNEL OPTIONS\n";
static const char *channel_options[CHANNEL_OPTIONS_COUNT] = {
"    By default, the channel configuration is determined by the input file wav\n"
"    header.  However, while the extensible wav format can specify all possible\n"
"    options in AC-3, the basic wav format cannot.  The acmod and lfe options\n"
"    allow the user to explicitly select the desired channel layout.  This only\n"
"    controls the interpretation of the input, so no downmixing or upmixing is\n"
"    done.\n",

"    [-acmod #]     Audio coding mode (overrides wav header)\n"
"                       0 = 1+1 (Ch1,Ch2)\n"
"                       1 = 1/0 (C)\n"
"                       2 = 2/0 (L,R)\n"
"                       3 = 3/0 (L,R,C)\n"
"                       4 = 2/1 (L,R,S)\n"
"                       5 = 3/1 (L,R,C,S)\n"
"                       6 = 2/2 (L,R,SL,SR)\n"
"                       7 = 3/2 (L,R,C,SL,SR)\n",

"    [-lfe #]       Specify use of LFE channel (overrides wav header)\n"
"                       0 = LFE channel is not present\n"
"                       1 = LFE channel is present\n",

"    [-chmap #]     Channel mapping order of input audio\n"
"                       Some programs create wav files which use AC-3 channel\n"
"                       mapping rather than standard wav mapping.  This option\n"
"                       allows the user to specify if the input file uses AC-3\n"
"                       or wav channel mapping.\n"
"                       0 = wav mapping (default)\n"
"                       1 = AC-3 mapping\n"
};

#define FILTER_OPTIONS_COUNT 3

static const char filter_heading[15] = "INPUT FILTERS\n";
static const char *filter_options[FILTER_OPTIONS_COUNT] = {
"    [-bwfilter #]  Specify use of the bandwidth low-pass filter\n"
"                       The bandwidth low-pass filter pre-filters the input\n"
"                       audio before converting to frequency-domain.  This\n"
"                       smooths the cutoff frequency transition for slightly\n"
"                       better quality.\n"
"                       0 = do not apply filter (default)\n"
"                       1 = apply filter\n",

"    [-dcfilter #]  Specify use of the DC high-pass filter\n"
"                       The DC high-pass filter is listed as optional by the\n"
"                       AC-3 specification.  The implementation, as suggested,\n"
"                       is a single pole filter at 3 Hz.\n"
"                       0 = do not apply filter (default)\n"
"                       1 = apply filter\n",

"    [-lfefilter #] Specify use of the LFE low-pass filter\n"
"                       The LFE low-pass filter is recommended by the AC-3\n"
"                       specification.  The cutoff is 120 Hz.  The specification\n"
"                       recommends an 8th order elliptic filter, but instead,\n"
"                       Aften uses a Butterworth 2nd order cascaded direct\n"
"                       form II filter.\n"
"                       0 = do not apply filter (default)\n"
"                       1 = apply filter\n"
};

#define ALT_OPTIONS_COUNT 12

static const char alt_heading[29] = "ALTERNATE BIT STREAM SYNTAX\n";
static const char *alt_options[ALT_OPTIONS_COUNT] = {
"    The alternate bit stream syntax allows for encoding additional metadata by\n"
"    adding 1 or 2 extended bitstream info sections to each frame.\n",

"    [-xbsi1 #]     Specify use of extended bitstream info 1\n"
"                       Extended bitstream info 1 contains the dmixmod,\n"
"                       ltrtcmix, ltrtsmix, lorocmix, and lorosmix fields.  If\n"
"                       this option is turned on, all these values are written\n"
"                       to the output stream.\n"
"                       0 = do not write xbsi1\n"
"                       1 = write xbsi1\n",

"    [-dmixmod #]   Preferred stereo downmix mode\n"
"                       This code indicates the type of stereo downmix preferred\n"
"                       by the mastering engineer, and can be optionally used,\n"
"                       overridden, or ignored by the decoder.\n"
"                       0 = not indicated (default)\n"
"                       1 = Lt/Rt downmix preferred\n"
"                       2 = Lo/Ro downmix preferred\n",

"    The 4 mix level options below all use the same code table:\n"
"                       0 = +3.0 dB\n"
"                       1 = +1.5 dB\n"
"                       2 =  0.0 dB\n"
"                       3 = -1.5 dB\n"
"                       4 = -3.0 dB (default)\n"
"                       5 = -4.5 dB\n"
"                       6 = -6.0 dB\n"
"                       7 = -inf dB\n",

"    [-ltrtcmix #]  Lt/Rt center mix level\n"
"                       This code indicates the nominal down mix level of the\n"
"                       center channel with respect to the left and right\n"
"                       channels in an Lt/Rt downmix.\n",

"    [-ltrtsmix #]  Lt/Rt surround mix level\n"
"                       This code indicates the nominal down mix level of the\n"
"                       surround channels with respect to the left and right\n"
"                       channels in an Lt/Rt downmix.\n",

"    [-lorocmix #]  Lo/Ro center mix level\n"
"                       This code indicates the nominal down mix level of the\n"
"                       center channel with respect to the left and right\n"
"                       channels in an Lo/Ro downmix.\n",

"    [-lorosmix #]  Lo/Ro surround mix level\n"
"                       This code indicates the nominal down mix level of the\n"
"                       surround channels with respect to the left and right\n"
"                       channels in an Lo/Ro downmix.\n",

"    [-xbsi2 #]     Specify use of extended bitstream info 2\n"
"                       Extended bitstream info 2 contains the dsurexmod,\n"
"                       dheadphon, and adconvtyp fields.  If this option is\n"
"                       turned on, all these values are written to the output\n"
"                       stream.  These options are not used by the AC-3 decoder,\n"
"                       but may be used by other portions of the audio\n"
"                       reproduction equipment.\n"
"                       0 = do not write xbsi2\n"
"                       1 = write xbsi2\n",

"    [-dsurexmod #] Dolby Surround EX mode\n"
"                       This code indicates whether or not the program has been\n"
"                       encoded in Dolby Surround EX.\n"
"                       0 = not indicated (default)\n"
"                       1 = Not Dolby Surround EX encoded\n"
"                       2 = Dolby Surround EX encoded\n",

"    [-dheadphon #] Dolby Headphone mode\n"
"                       This code indicates whether or not the program has been\n"
"                       Dolby Headphone-encoded.\n"
"                       0 = not indicated (default)\n"
"                       1 = Not Dolby Headphone encoded\n"
"                       2 = Dolby Headphone encoded\n",

"    [-adconvtyp #] A/D converter type\n"
"                       This code indicates the type of A/D converter technology\n"
"                       used to capture the PCM audio.\n"
"                       0 = Standard (default)\n"
"                       1 = HDCD\n"
};

typedef struct {
    int section_count;
    const char *section_heading;
    const char **section_options;
} LongHelpSection;

#define LONG_HELP_SECTIONS_COUNT 7

static const LongHelpSection long_help_sections[LONG_HELP_SECTIONS_COUNT] = {
    {   CONSOLE_OPTIONS_COUNT,
        console_heading,
        console_options },
    {   ENCODING_OPTIONS_COUNT,
        encoding_heading,
        encoding_options },
    {   BSI_OPTIONS_COUNT,
        bsi_heading,
        bsi_options },
    {   DRC_OPTIONS_COUNT,
        drc_heading,
        drc_options },
    {   CHANNEL_OPTIONS_COUNT,
        channel_heading,
        channel_options },
    {   FILTER_OPTIONS_COUNT,
        filter_heading,
        filter_options },
    {   ALT_OPTIONS_COUNT,
        alt_heading,
        alt_options }
};

#endif /* HELPTEXT_H */