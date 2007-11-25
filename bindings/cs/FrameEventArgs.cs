using System;

namespace Aften
{
	/// <summary>
	/// EventArgs for frame related events
	/// </summary>
	public class FrameEventArgs : EventArgs
	{
		private readonly int m_nFrameNumber;
		private readonly int m_nSize;
		private readonly Status m_Status;


		/// <summary>
		/// Gets the status.
		/// </summary>
		/// <value>The status.</value>
		public Status Status
		{
			get { return m_Status; }
		}

		/// <summary>
		/// Gets the size of the encoded frame.
		/// </summary>
		/// <value>The size.</value>
		public int Size
		{
			get { return m_nSize; }
		}

		/// <summary>
		/// Gets the frame number.
		/// </summary>
		/// <value>The frame number.</value>
		public int FrameNumber
		{
			get { return m_nFrameNumber; }
		}

		/// <summary>
		/// Initializes a new instance of the <see cref="FrameEventArgs"/> class.
		/// </summary>
		/// <param name="frameNumber">The frame number.</param>
		/// <param name="size">The size of the encoded frame.</param>
		/// <param name="status">The status.</param>
		public FrameEventArgs( int frameNumber, int size, Status status )
		{
			m_nFrameNumber = frameNumber;
			m_nSize = size;
			m_Status = status;
		}
	}
}
