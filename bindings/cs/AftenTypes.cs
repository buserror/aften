/********************************************************************************
 * Copyright (C) 2007 by Prakash Punnoor                                        *
 * prakash@punnoor.de                                                           *
 *                                                                              *
 * This library is free software; you can redistribute it and/or                *
 * modify it under the terms of the GNU Lesser General Public                   *
 * License as published by the Free Software Foundation; either                 *
 * version 2 of the License                                                     *
 *                                                                              *
 * This library is distributed in the hope that it will be useful,              *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of               *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU            *
 * Lesser General Public License for more details.                              *
 *                                                                              *
 * You should have received a copy of the GNU Lesser General Public             *
 * License along with this library; if not, write to the Free Software          *
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA *
 ********************************************************************************/
using System;

namespace Aften
{
	/**
	 * Some helpful size constants
	 */
	public static class A52Sizes
	{
		public const int MaximumCodedFrameSize = 3840;
		public const int SamplesPerFrame = 1536;
	}

	/**
		* Aften Encoding Mode
		*/
	public enum EncodingMode
	{
		Cbr = 0,
		Vbr
	}

	/**
		* Floating-Point Data Types
		*/
	public enum FloatType
	{
		Double,
		Float
	}

	/**
		* Audio Sample Formats
		*/
	public enum A52SampleFormat
	{
		Unsigned8 = 0,
		Signed16,
		Signed20,
		Signed24,
		Signed32,
		Float,
		Double,
		Signed8
	}

	/**
		* Dynamic Range Profiles
		*/
	public enum DynamicRangeProfile
	{
		FilmLight = 0,
		FilmStandard,
		MusicLight,
		MusicStandard,
		Speech,
		None
	}

	/**
		* Audio Coding Mode (acmod) options
		*/
	public enum AudioCodingMode
	{
		DualMono = 0,
		Mono,
		Stereo,
		Front3Rear0,
		Front2Rear1,
		Front3Rear1,
		Front2Rear2,
		Front3Rear2
	}

	/**
		* SIMD instruction sets
		*/
	public struct SimdInstructions
	{
		public bool Mmx;
		public bool Sse;
		public bool Sse2;
		public bool Sse3;
		public bool Ssse3;
		public bool Amd3Dnow;
		public bool Amd3Dnowext;
		public bool AmdSseMmx;
		public bool Altivec;
	}

	/**
		* Performance related parameters
		*/
	public struct SystemParameters
	{
		/**
			* Number of threads
			* How many threads should be used.
			* Default value is 0, which indicates detecting number of CPUs.
			* Maximum value is AFTEN_MAX_THREADS.
			*/
		public int ThreadsCount;

		/**
			* Available SIMD instruction sets; shouldn't be modified
			*/
		public SimdInstructions AvailableSimdInstructions;

		/**
			* Wanted SIMD instruction sets
			*/
		public SimdInstructions WantedSimdInstructions;
	}

	/**
		* Parameters which affect encoded audio output
		*/
	public struct EncodingParameters
	{
		/**
			* Bitrate selection mode.
			* AFTEN_ENC_MODE_CBR : constant bitrate
			* AFTEN_ENC_MODE_VBR : variable bitrate
			* default is CBR
			*/
		public EncodingMode EncodingMode;

		/**
			* Stereo rematrixing option.
			* Set to 0 to disable stereo rematrixing, 1 to enable it.
			* default is 1
			*/
		public bool UseRematrixing;

		/**
			* Block switching option.
			* Set to 0 to disable block switching, 1 to enable it.
			* default is 0
			*/
		public bool UseBlockSwitching;

		/**
			* DC high-pass filter option.
			* Set to 0 to disable the filter, 1 to enable it.
			* default is 0
			*/
		public bool UseDCFilter;

		/**
			* Bandwidth low-pass filter option.
			* Set to 0 to disable the, 1 to enable it.
			* This option cannot be enabled with variable bandwidth mode (bwcode=-2)
			* default is 0
			*/
		public bool UseBandwithFilter;

		/**
			* LFE low-pass filter option.
			* Set to 0 to disable the filter, 1 to enable it.
			* This limits the LFE bandwidth, and can only be used if the input audio
			* has an LFE channel.
			* default is 0
			*/
		public bool UseLfeFilter;

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
		public int Bitrate;

		/**
			* VBR Quality.
			* This option sets the target quality for VBR encoding mode.
			* The range is 0 to 1023 and corresponds to the SNR offset.
			* default is 240
			*/
		public int Quality;

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
		public int BandwidthCode;

		/**
			* Bit Allocation speed/accuracy
			* This determines how accurate the bit allocation search method is.
			* Set to 0 for better quality
			* Set to 1 for faster encoding
			* default is 0
			*/
		public bool UseFastBitAllocation;

		/**
			* Exponent Strategy speed/quality
			* This determines whether to use a fixed or adaptive exponent strategy.
			* Set to 0 for adaptive strategy (better quality, slower)
			* Set to 1 for fixed strategy (lower quality, faster)
			*/
		public bool UseFastExponentStrategy;

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
		public DynamicRangeProfile DynamicRangeProfile;

		/**
			* Minimum bandwidth code.
			* For use with variable bandwidth mode, this option determines the
			* minimum value for the bandwidth code.
			* default is 0.
			*/
		public int MinimumBandwidthCode;

		/**
			* Maximum bandwidth code.
			* For use with variable bandwidth mode, this option determines the
			* maximum value for the bandwidth code.
			* default is 60.
			*/
		public int MaximumBandwidthCode;
	}
	/**
		* Metadata parameters
		* See the A/52 specification for details regarding the metadata.
		*/
	public struct Metadata
	{
		/** Center mix level */
		public int CenterMixLevel;

		/** Surround mix level */
		public int SurroundMixLevel;

		/** Dolby(R) Surround mode */
		public int DolbySurroundMode;

		/** Dialog normalization */
		public int DialogNormalization;

		/** Extended bit stream info 1 exists */
		public bool HaveExtendedBitstreamInfo1;

		/** Preferred downmix mode */
		public int DownMixMode;

		/** LtRt center mix level */
		public int LtRtCenterMixLevel;

		/** LtRt surround mix level */
		public int LtRtSurroundMixLevel;

		/** LoRo center mix level */
		public int LoRoCenterMixLevel;

		/** LoRo surround mix level */
		public int LoRoSurroundMixLevel;

		/** Extended bit stream info 2 exists */
		public bool HaveExtendedBitstreamInfo2;

		/** Dolby(R) Surround EX mode */
		public int DolbySurroundEXMode;

		/** Dolby(R) Headphone mode */
		public int DolbyHeadphoneMode;

		/** A/D converter type */
		public int ADConverterType;

	}
	/**
		* Values in this structure are updated by Aften during encoding.
		* They give information about the previously encoded frame.
		*/
	public struct Status
	{
		public int Quality;
		public int BitRate;
		public int BandwidthCode;
	}

	/**
		* libaften public encoding context
		*/
	public struct EncodingContext
	{
		public EncodingParameters EncodingParameters;
		public Metadata Metadata;
		public Status Status;
		public SystemParameters SystemParameters;

		/**
			* Verbosity level.
			* 0 is quiet mode. 1 and 2 are more verbose.
			* default is 1
			*/
		public int Verbosity;

		/**
			* Total number of channels in the input stream.
			*/
		public int Channels;

		/**
			* Audio coding mode (channel configuration).
			* There are utility functions to set this if you don't know the proper
			* value.
			*/
		public AudioCodingMode AudioCodingMode;

		/**
			* Indicates that there is an LFE channel present.
			* There are utility functions to set this if you don't know the proper
			* value.
			*/
		public bool HasLfe;

		/**
			* Audio sample rate in Hz
			*/
		public int SampleRate;

		/**
			* Audio sample format
			* default: A52_SAMPLE_FMT_S16
			*/
		public A52SampleFormat SampleFormat;
		
		/**
		* Initial samples
		* To prevent padding und thus to get perfect sync,
		* exactly 256 samples/channel can be provided here.
		* This is not recommended, as without padding these samples can't be properly
		* reconstructed anymore.
		*/
		public float[] InitialSamples;

		/**
			* Used internally by the encoder. The user should leave this alone.
			* It is allocated in aften_encode_init and free'd in aften_encode_close.
			*/
		private IntPtr m_Context;
	}
}