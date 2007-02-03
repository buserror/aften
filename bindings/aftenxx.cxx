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
#include "aftenxx.h"

namespace Aften
{
#include "aften.h"

/// Constructor: Initializes the encoder with specified context
FrameEncoder::FrameEncoder(AftenContext &context)
{
    m_context = context;
    int nReturnValue = aften_encode_init(&m_context);
    if (nReturnValue > 0)
        throw nReturnValue;
}

/// Destructor: Deinitalizes the encoder
FrameEncoder::~FrameEncoder()
{
    aften_encode_close(&m_context);
}

/// Encodes PCM samples to an A/52 frame
int FrameEncoder::Encode(unsigned char *frameBuffer, void *samples)
{
    return aften_encode_frame(&m_context, frameBuffer, samples);
}

/// Gets a context with default values
AftenContext FrameEncoder::GetDefaultsContext()
{
    AftenContext context;
    aften_set_defaults(&context);
    return context;
}

/// Determines the proper A/52 acmod and lfe parameters based on the
/// WAVE_FORMAT_EXTENSIBLE channel mask.  This is more accurate than assuming
/// that all the proper channels are present.
void FrameEncoder::WaveChannelMaskToAcmod(int channels, int channelMask, int &acmod, bool &hasLfe)
{
    int nLfe;
    aften_wav_chmask_to_acmod(channels, channelMask, &acmod, &nLfe);
    hasLfe = nLfe != 0;
}

/// Determines probable A/52 acmod and lfe parameters based on the number of
/// channels present.  You can use this if you don't have the channel mask.
void FrameEncoder::PlainWaveToAcmod(int channels, int &acmod, bool &hasLfe)
{
    int nLfe;
    aften_plain_wav_to_acmod(channels, &acmod, &nLfe);
    hasLfe = nLfe != 0;
}

/// Takes a channel-interleaved array of audio samples, where the channel order
/// is the default Wave order. The samples are rearranged to the proper A/52
/// channel order based on the acmod and lfe parameters.
void FrameEncoder::RemapWaveToA52(void *samples, int samplesCount, int channels,
                           enum A52SampleFormat format, int acmod)
{
    aften_remap_wav_to_a52(samples, samplesCount, channels, format, acmod);
}

/// Tells whether libaften was configured to use floats or doubles
enum FloatType FrameEncoder::GetFloatType()
{
    return aften_get_float_type();
}
}
