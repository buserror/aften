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
using System.Runtime.InteropServices;
using System.Diagnostics;

namespace Aften
{
	public class FrameEncoder : IDisposable
	{
		private bool m_bDisposed;
		private EncodingContext m_Context;


		[DllImport( "aften.dll" )]
		private static extern void aften_set_defaults( ref EncodingContext context );

		[DllImport( "aften.dll" )]
		private static extern int aften_encode_init( ref EncodingContext context );

		[DllImport( "aften.dll" )]
		private static extern void aften_encode_close( ref EncodingContext context );

		[DllImport( "aften.dll" )]
		private static extern int aften_encode_frame(
			ref EncodingContext context, byte[] frameBuffer, float[] samples, int count );

		private void Dispose( bool disposing )
		{
			// Check to see if Dispose has already been called.
			if(!m_bDisposed) {
				if(disposing) {
					// Dispose managed resources
				}
				// Release unmanaged resources
				aften_encode_close( ref m_Context );
				
				m_bDisposed = true;
			}
		}

		public static EncodingContext GetDefaultsContext()
		{
			EncodingContext context = new EncodingContext();
			aften_set_defaults( ref context );
			
			return context;
		}

		public void Dispose()
		{
			this.Dispose( true );
			GC.SuppressFinalize( this );
		}

		public int Encode( byte[] frameBuffer, float[] samples, int count )
		{
			int size = aften_encode_frame( ref m_Context, frameBuffer, samples, count );
			if (size < 0)
				throw new InvalidOperationException( "Encoding error");

			return size;
		}

		public FrameEncoder( ref EncodingContext context )
		{
			if ( aften_encode_init( ref context ) != 0 )
				throw new InvalidOperationException( "Initialization failed" );

			m_Context = context;
		}

		~FrameEncoder()
		{
			this.Dispose( false );
		}
	}
}