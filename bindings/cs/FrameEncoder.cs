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
using System.IO;
using System.Runtime.InteropServices;

namespace Aften
{
	/// <summary>
	/// The Aften AC3 Encoder
	/// </summary>
	public class FrameEncoder : IDisposable
	{
		const string _EqualAmountOfSamples = "Each channel must have an equal amount of samples.";

		private readonly byte[] m_FrameBuffer = new byte[A52Sizes.MaximumCodedFrameSize];
		private readonly byte[] m_StreamBuffer = new byte[sizeof( float )];
		private readonly float[] m_Samples;
		private readonly float[] m_StreamSamples;
		private readonly int m_nTotalSamplesPerFrame;
		private readonly RemappingDelegate m_Remap;

		private bool m_bDisposed;
		private EncodingContext m_Context;
		private int m_nRemainingSamplesCount;


		[DllImport( "aften.dll" )]
		private static extern void aften_set_defaults( ref EncodingContext context );

		[DllImport( "aften.dll" )]
		private static extern int aften_encode_init( ref EncodingContext context );

		[DllImport( "aften.dll" )]
		private static extern void aften_encode_close( ref EncodingContext context );

		[DllImport( "aften.dll" )]
		private static extern int aften_encode_frame(
			ref EncodingContext context, byte[] frameBuffer, float[] samples, int count );

		/// <summary>
		/// Releases unmanaged and - optionally - managed resources
		/// </summary>
		/// <param name="disposing"><c>true</c> to release both managed and unmanaged resources; <c>false</c> to release only unmanaged resources.</param>
		private void Dispose( bool disposing )
		{
			// Check to see if Dispose has already been called.
			if ( !m_bDisposed ) {
				if ( disposing ) {
					// Dispose managed resources
				}
				// Release unmanaged resources
				aften_encode_close( ref m_Context );

				m_bDisposed = true;
			}
		}

		/// <summary>
		/// Gets a default context.
		/// </summary>
		/// <returns></returns>
		public static EncodingContext GetDefaultsContext()
		{
			EncodingContext context = new EncodingContext();
			aften_set_defaults( ref context );

			return context;
		}

		public delegate void RemappingDelegate(
			float[] samples, int channels, A52SampleFormat format, AudioCodingMode audioCodingMode );

		/// <summary>
		/// Performs application-defined tasks associated with freeing, releasing, or resetting unmanaged resources.
		/// </summary>
		public void Dispose()
		{
			this.Dispose( true );
			GC.SuppressFinalize( this );
		}

		/// <summary>
		/// Encodes the specified interleaved samples.
		/// </summary>
		/// <param name="samples">The samples.</param>
		/// <returns>MemoryStream containing the encoded frames</returns>
		public MemoryStream Encode( float[] samples )
		{
			if ( samples.Length % m_Context.Channels != 0 )
				throw new InvalidOperationException( _EqualAmountOfSamples );

			return this.Encode( samples, samples.Length / m_Context.Channels );
		}

		/// <summary>
		/// Encodes the specified interleaved samples.
		/// </summary>
		/// <param name="samples">The samples.</param>
		/// <param name="samplesPerChannelCount">The samples per channel count.</param>
		/// <returns>
		/// MemoryStream containing the encoded frames
		/// </returns>
		public MemoryStream Encode( float[] samples, int samplesPerChannelCount )
		{
			MemoryStream stream = new MemoryStream();
			this.Encode( samples, samplesPerChannelCount, stream );

			return stream;
		}

		/// <summary>
		/// Encodes the specified interleaved samples.
		/// </summary>
		/// <param name="samples">The samples.</param>
		/// <param name="frames">The frames.</param>
		public void Encode( float[] samples, Stream frames )
		{
			if ( samples.Length % m_Context.Channels != 0 )
				throw new InvalidOperationException( _EqualAmountOfSamples );

			this.Encode( samples, samples.Length / m_Context.Channels, frames );
		}

		/// <summary>
		/// Encodes the ssamplesCount of the specified interleaved samples.
		/// </summary>
		/// <param name="samples">The samples.</param>
		/// <param name="samplesPerChannelCount">The samples per channel count.</param>
		/// <param name="frames">The frames.</param>
		public void Encode( float[] samples, int samplesPerChannelCount, Stream frames )
		{
			int samplesCount = samplesPerChannelCount * m_Context.Channels;
			if ( samplesCount > samples.Length )
				throw new InvalidOperationException( "samples contains less data then specified." );

			int nOffset = m_nRemainingSamplesCount;
			int nSamplesNeeded = m_nTotalSamplesPerFrame - nOffset;
			int nSamplesDone = 0;
			while ( samplesCount - nSamplesDone >= m_nTotalSamplesPerFrame ) {
				Buffer.BlockCopy( samples, nSamplesDone * sizeof( float ), m_Samples, nOffset, nSamplesNeeded * sizeof( float ) );
				if ( m_Remap != null )
					m_Remap( m_Samples, m_Context.Channels, m_Context.SampleFormat, m_Context.AudioCodingMode );

				int nSize = aften_encode_frame( ref m_Context, m_FrameBuffer, m_Samples, A52Sizes.SamplesPerFrame );
				if ( nSize < 0 )
					throw new InvalidOperationException( "Encoding error" );

				frames.Write( m_FrameBuffer, 0, nSize );
				nSamplesDone += m_nTotalSamplesPerFrame;
				nSamplesNeeded = m_nTotalSamplesPerFrame;
				nOffset = 0;
			}
			m_nRemainingSamplesCount = samplesCount - nSamplesDone;
			if ( m_nRemainingSamplesCount > 0 )
				Buffer.BlockCopy( samples, nSamplesDone * sizeof( float ), m_Samples, 0, m_nRemainingSamplesCount * sizeof( float ) );
		}

		/// <summary>
		/// Encodes the specified interleaved samples.
		/// </summary>
		/// <param name="samples">The samples.</param>
		/// <returns>
		/// MemoryStream containing the encoded frames
		/// </returns>
		public MemoryStream Encode( Stream samples )
		{
			MemoryStream stream = new MemoryStream();
			this.Encode( samples, stream );

			return stream;
		}

		/// <summary>
		/// Encodes the specified interleaved samples stream and flushes the encoder.
		/// </summary>
		/// <param name="samples">The samples.</param>
		/// <param name="frames">The frames.</param>
		public void Encode( Stream samples, Stream frames )
		{
			if ( !samples.CanRead )
				throw new InvalidOperationException( "Samples stream must be readable." );

			int nOffset = 0;
			int nCurrentRead;
			while ( (nCurrentRead = samples.Read( m_StreamBuffer, 0, sizeof( float ) )) == sizeof( float ) ) {
				m_StreamSamples[nOffset] = BitConverter.ToSingle( m_StreamBuffer, 0 );
				++nOffset;
				if ( nOffset == m_nTotalSamplesPerFrame ) {
					this.Encode( m_StreamSamples, frames );
					nOffset = 0;
				}
			}
			if ( (nCurrentRead != 0) || (nOffset % m_Context.Channels != 0) )
				throw new InvalidOperationException( _EqualAmountOfSamples );

			if ( nOffset > 0 )
				this.Encode( m_StreamSamples, nOffset / m_Context.Channels, frames );

			this.Flush( frames );
		}

		/// <summary>
		/// Flushes the encoder und returns the remaining frames.
		/// </summary>
		/// <returns>MemoryStream containing the encoded frames</returns>
		public MemoryStream Flush()
		{
			MemoryStream stream = new MemoryStream();
			Flush( stream );

			return stream;
		}

		/// <summary>
		/// Flushes the encoder und returns the remaining frames.
		/// </summary>
		/// <param name="stream">The stream.</param>
		public void Flush( Stream frames )
		{
			int nSize;
			int nSamplesCount = m_nRemainingSamplesCount / m_Context.Channels;
			do {
				if ( (nSamplesCount > 0) && (m_Remap != null) )
					m_Remap( m_Samples, m_Context.Channels, m_Context.SampleFormat, m_Context.AudioCodingMode );

				nSize = aften_encode_frame( ref m_Context, m_FrameBuffer, m_Samples, nSamplesCount );
				if ( nSize < 0 )
					throw new InvalidOperationException( "Encoding error" );

				frames.Write( m_FrameBuffer, 0, nSize );
				nSamplesCount = 0;
			} while ( nSize > 0 );
		}

		/// <summary>
		/// Initializes a new instance of the <see cref="FrameEncoder"/> class.
		/// </summary>
		/// <param name="context">The context.</param>
		/// <param name="remap">The remapping function.</param>
		public FrameEncoder( ref EncodingContext context, RemappingDelegate remap )
			: this( ref context )
		{
			m_Remap = remap;
		}

		/// <summary>
		/// Initializes a new instance of the <see cref="FrameEncoder"/> class.
		/// </summary>
		/// <param name="context">The context.</param>
		public FrameEncoder( ref EncodingContext context )
		{
			if ( aften_encode_init( ref context ) != 0 )
				throw new InvalidOperationException( "Initialization failed" );

			m_Context = context;

			m_nTotalSamplesPerFrame = A52Sizes.SamplesPerFrame * m_Context.Channels;
			m_Samples = new float[m_nTotalSamplesPerFrame];
			m_StreamSamples = new float[m_nTotalSamplesPerFrame];
		}

		/// <summary>
		/// Releases unmanaged resources and performs other cleanup operations before the
		/// <see cref="FrameEncoder"/> is reclaimed by garbage collection.
		/// </summary>
		~FrameEncoder()
		{
			this.Dispose( false );
		}
	}
}