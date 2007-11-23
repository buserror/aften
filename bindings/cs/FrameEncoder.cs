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
		private readonly byte[] m_FrameBuffer = new byte[A52Sizes.MaximumCodedFrameSize];
		private readonly float[] m_Samples;
		private readonly int m_nTotalSamplesPerFrame;

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
			MemoryStream stream = new MemoryStream();
			this.Encode( samples, stream );

			return stream;
		}

		/// <summary>
		/// Encodes the specified interleaved samples.
		/// </summary>
		/// <param name="samples">The samples.</param>
		/// <param name="stream">The stream.</param>
		public void Encode( float[] samples, Stream stream )
		{
			if ( samples.Length % m_Context.Channels != 0 )
				throw new InvalidOperationException( "Each channel must have an equal amount of samples." );

			int nOffset = m_nRemainingSamplesCount;
			int nSamplesNeeded = m_nTotalSamplesPerFrame - nOffset;
			int nSamplesDone = 0;
			while ( samples.Length - nSamplesDone >= m_nTotalSamplesPerFrame ) {
				Buffer.BlockCopy( samples, nSamplesDone * sizeof( float ), m_Samples, nOffset, nSamplesNeeded * sizeof( float ) );
				int size = aften_encode_frame( ref m_Context, m_FrameBuffer, m_Samples, A52Sizes.SamplesPerFrame );
				if ( size < 0 )
					throw new InvalidOperationException( "Encoding error" );

				stream.Write( m_FrameBuffer, 0, size );
				nSamplesDone += m_nTotalSamplesPerFrame;
				nSamplesNeeded = m_nTotalSamplesPerFrame;
				nOffset = 0;
			}
			m_nRemainingSamplesCount = samples.Length - nSamplesDone;
			if ( m_nRemainingSamplesCount > 0 )
				Buffer.BlockCopy( samples, nSamplesDone * sizeof( float ), m_Samples, 0, m_nRemainingSamplesCount * sizeof( float ) );
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
		public void Flush( Stream stream )
		{
			int size;
			int nSamplesCount = m_nRemainingSamplesCount / m_Context.Channels;
			do {
				size = aften_encode_frame( ref m_Context, m_FrameBuffer, m_Samples, nSamplesCount );
				if ( size < 0 )
					throw new InvalidOperationException( "Encoding error" );

				stream.Write( m_FrameBuffer, 0, size );
				nSamplesCount = 0;
			} while ( size > 0 );
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