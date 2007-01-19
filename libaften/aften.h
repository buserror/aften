/**
 * Aften: A/52 audio encoder
 * Copyright (c) 2006  Justin Ruggles <justinruggles@bellsouth.net>
 *
 * Based on "The simplest AC3 encoder" from FFmpeg
 * Copyright (c) 2000 Fabrice Bellard.
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
 * @file aften.h
 * libaften public header
 */

#ifndef AFTEN_H
#define AFTEN_H

#ifdef __cplusplus
extern "C" {
#endif

#define AFTEN_VERSION SVN

#define A52_MAX_CODED_FRAME_SIZE 3840

#define A52_NUM_BLOCKS 6
#define A52_FRAME_SIZE (A52_NUM_BLOCKS * 256)

#define AFTEN_ENC_MODE_CBR    0
#define AFTEN_ENC_MODE_VBR    1


#if defined(_WIN32) && !defined(_XBOX)
 #if defined(AFTEN_BUILD_LIBRARY)
  #define AFTEN_API __declspec(dllexport)
 #else
  #define AFTEN_API __declspec(dllimport)
 #endif
#else
 #if defined(AFTEN_BUILD_LIBRARY) && defined(HAVE_GCC_VISIBILITY)
  #define AFTEN_API __attribute__((visibility("default")))
 #else
  #define AFTEN_API extern
 #endif
#endif


enum A52SampleFormat {
    A52_SAMPLE_FMT_U8 = 0,
    A52_SAMPLE_FMT_S16,
    A52_SAMPLE_FMT_S20,
    A52_SAMPLE_FMT_S24,
    A52_SAMPLE_FMT_S32,
    A52_SAMPLE_FMT_FLT,
    A52_SAMPLE_FMT_DBL,
};

enum DynRngProfile {
    DYNRNG_PROFILE_FILM_LIGHT=0,
    DYNRNG_PROFILE_FILM_STANDARD,
    DYNRNG_PROFILE_MUSIC_LIGHT,
    DYNRNG_PROFILE_MUSIC_STANDARD,
    DYNRNG_PROFILE_SPEECH,
    DYNRNG_PROFILE_NONE
};

typedef struct {

    /**
     * Verbosity level.
     * 0 is quiet mode. 1 and 2 are more verbose.
     * default is 1
     */
    int verbose;

    /**
     * Bitrate selection mode.
     * AFTEN_ENC_MODE_CBR : constant bitrate
     * AFTEN_ENC_MODE_VBR : variable bitrate
     * default is CBR
     */
    int encoding_mode;

    /**
     * Stereo rematrixing option.
     * Set to 0 to disable stereo rematrixing, 1 to enable it.
     * default is 1
     */
    int use_rematrixing;

    /**
     * Block switching option.
     * Set to 0 to disable block switching, 1 to enable it.
     * default is 0
     */
    int use_block_switching;

    /**
     * DC high-pass filter option.
     * Set to 0 to disable the filter, 1 to enable it.
     * default is 0
     */
    int use_dc_filter;

    /**
     * Bandwidth low-pass filter option.
     * Set to 0 to disable the, 1 to enable it.
     * This option cannot be enabled with variable bandwidth mode (bwcode=-2)
     * default is 0
     */
    int use_bw_filter;

    /**
     * LFE low-pass filter option.
     * Set to 0 to disable the filter, 1 to enable it.
     * This limits the LFE bandwidth, and can only be used if the input audio
     * has an LFE channel.
     * default is 0
     */
    int use_lfe_filter;

    /**
     * Constant bitrate.
     * This option sets the bitrate for CBR encoding mode.
     * It can also be used to set the maximum bitrate for VBR mode.
     * It is specified in kbps. Only certain bitrates are valid:
     *   0,  32,  40,  48,  56,  64,  80,  96, 112, 128,
     * 160, 192, 224, 256, 320, 384, 448, 512, 576, 640
     * default is 0
     * For CBR mode, this selects bitrate based on the number of channels.
     * For VBR mode, this sets the maximum bitrate to 640 kbps.
     */
    int bitrate;

    /**
     * VBR Quality.
     * This option sets the target quality for VBR encoding mode.
     * The range is 0 to 1023 and corresponds to the SNR offset.
     * default is 220
     */
    int quality;

    /**
     * Bandwidth code.
     * This option determines the cutoff frequency for encoded bandwidth.
     * 0 to 60 corresponds to a cutoff of 28.5% to 98.8% of the full bandwidth.
     * -1 is used for constant adaptive bandwidth. Aften selects a good value
     *    based on the quality or bitrate parameters.
     * -2 is used for variable adaptive bandwidth. Aften selects a value for
     *    each frame based on the encoding quality level for that frame.
     * default is -1
     */
    int bwcode;

    /**
     * Bit Allocation speed/accuracy
     * This determines how accurate the bit allocation search method is.
     * Set to 0 for better quality
     * Set to 1 for faster encoding
     * default is 0
     */
    int bitalloc_fast;

    /**
     * Dynamic Range Compression profile
     * This determines which DRC profile to use.
     * Film Light:     DYNRNG_PROFILE_FILM_LIGHT
     * Film Standard:  DYNRNG_PROFILE_FILM_STANDARD
     * Music Light:    DYNRNG_PROFILE_MUSIC_LIGHT
     * Music Standard: DYNRNG_PROFILE_MUSIC_STANDARD
     * Speech:         DYNRNG_PROFILE_SPEECH,
     * None:           DYNRNG_PROFILE_NONE
     * default is None
     */
    enum DynRngProfile dynrng_profile;

} AftenEncParams;

/**
 * See the A/52 specification for details regarding the metadata.
 */
typedef struct {

    /** Center mix level */
    int cmixlev;

    /** Surround mix level */
    int surmixlev;

    /** Dolby(R) Surround mode */
    int dsurmod;

    /** Dialog normalization */
    int dialnorm;

    /** Extended bit stream info 1 exists */
    int xbsi1e;

    /** Preferred downmix mode */
    int dmixmod;

    /** LtRt center mix level */
    int ltrtcmixlev;

    /** LtRt surround mix level */
    int ltrtsmixlev;

    /** LoRo center mix level */
    int lorocmixlev;

    /** LoRo surround mix level */
    int lorosmixlev;

    /** Extended bit stream info 2 exists */
    int xbsi2e;

    /** Dolby(R) Surround EX mode */
    int dsurexmod;

    /** Dolby(R) Headphone mode */
    int dheadphonmod;

    /** A/D converter type */
    int adconvtyp;

} AftenMetadata;

/**
 * Values in this structure are updated by Aften during encoding.
 * They give information about the previously encoded frame.
 */
typedef struct {
    int frame_num;
    int quality;
    int bit_rate;
    int bwcode;
} AftenStatus;

typedef struct {

    AftenEncParams params;
    AftenMetadata meta;
    AftenStatus status;

    /**
     * Total number of channels in the input stream.
     */
    int channels;

    /**
     * Audio coding mode (channel configuration).
     * There are utility functions to set this if you don't know the proper
     * value.
     */
    int acmod;

    /**
     * Indicates that there is an LFE channel present.
     * There are utility functions to set this if you don't know the proper
     * value.
     */
    int lfe;

    /**
     * Audio sample rate in Hz
     */
    int samplerate;

    /**
     * Audio sample format
     * default: A52_SAMPLE_FMT_S16
     */
    enum A52SampleFormat sample_format;

    /**
     * Used internally by the encoder. The user should leave this alone.
     * It is allocated in aften_encode_init and free'd in aften_encode_close.
     */
    void *private_context;
} AftenContext;

/* main encoding functions */

AFTEN_API void aften_set_defaults(AftenContext *s);

AFTEN_API int aften_encode_init(AftenContext *s);

AFTEN_API int aften_encode_frame(AftenContext *s, unsigned char *frame_buffer,
                                 void *samples);

AFTEN_API void aften_encode_close(AftenContext *s);

/* utility functions */

/**
 * Determines the proper A/52 acmod and lfe parameters based on the
 * WAVE_FORMAT_EXTENSIBLE channel mask.  This is more accurate than assuming
 * that all the proper channels are present.
 */
AFTEN_API void aften_wav_chmask_to_acmod(int ch, int chmask, int *acmod,
                                         int *lfe);

/**
 * Determines probable A/52 acmod and lfe parameters based on the number of
 * channels present.  You can use this if you don't have the channel mask.
 */
AFTEN_API void aften_plain_wav_to_acmod(int ch, int *acmod, int *lfe);

/**
 * Takes a channel-interleaved array of audio samples, where the channel order
 * is the default WAV order. The samples are rearranged to the proper A/52
 * channel order based on the acmod and lfe parameters.
 */
AFTEN_API void aften_remap_wav_to_a52(void *samples, int n, int ch,
                                      enum A52SampleFormat fmt, int acmod);

enum FloatType {
    FLOAT_TYPE_DOUBLE,
    FLOAT_TYPE_FLOAT
};

/**
 * Tells whether libaften was configured to use floats or doubles
 */
AFTEN_API enum FloatType aften_get_float_type(void);

#if defined(__cplusplus)
}
#endif

#endif /* AFTEN_H */
