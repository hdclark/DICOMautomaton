/*

Imebra 2011 build 2012-12-19_20-05-13

Imebra: a C++ Dicom library

Copyright (c) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010 by Paolo Brandoli
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE FREEBSD PROJECT ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE FREEBSD PROJECT OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those of the authors
 and should not be interpreted as representing official policies, either expressed or implied,
 of the Imebra Project.

Imebra is available at http://imebra.com



*/

/*! \file streamWriter.h
    \brief Declaration of the the class used to write the streams.

*/


#if !defined(imebraStreamWriter_2C008538_F046_401C_8C83_2F76E1077DB0__INCLUDED_)
#define imebraStreamWriter_2C008538_F046_401C_8C83_2F76E1077DB0__INCLUDED_

#include "streamController.h"

///////////////////////////////////////////////////////////
//
// Everything is in the namespace puntoexe
//
///////////////////////////////////////////////////////////
namespace puntoexe
{

/// \addtogroup group_baseclasses
///
/// @{

/// \brief Use this class to write into a baseStream
///         derived class.
///
/// Like the streamReader, this class is not multithread
///  safe, but several streamWriter (in several threads) 
///  can be connected to a single stream.
///
/// A streamWriter can also be connected only to a part
///  of the original stream: when this feature is used then
///  the streamWriter will act as if only the visible bytes
///  exist.
///
///////////////////////////////////////////////////////////
class streamWriter: public streamController
{
public:

	/// \brief Creates the streamWriter and connects it to a 
	///         baseStream object.
	///
	/// @param pControlledStream   the stream used by the 
	///                             streamWriter to write
	/// @param virtualStart        the first stream's byte
	///                             visible to the streamWriter
	/// @param virtualLength       the number of stream's bytes
	///                             visible to the 
	///                             streamWriter. Set to 0 to
	///                             allow the streamWriter to
	///                             see all the bytes
	///
	///////////////////////////////////////////////////////////
	streamWriter(ptr<baseStream> pControlledStream, imbxUint32 virtualStart = 0, imbxUint32 virtualLength = 0);

	/// \brief Writes the internal buffer into the connected
	///         stream. This function is automatically called
	///         when needed, but your application can call it
	///         when  syncronization between the cached data
	///         and the stream is needed.
	///
	///////////////////////////////////////////////////////////
	void flushDataBuffer();

	/// \brief Write raw data into the stream.
	///
	/// The data stored in the pBuffer parameter will be
	///  written into the stream.
	/// 
	/// The function throws a streamExceptionWrite exception
	///  if an error occurs.
	///
	/// @param pBuffer   a pointer to the buffer which stores
	///                   the data that must be written into
	///                   the stream
	/// @param bufferLength the number of bytes that must be
	///                   written to the stream
	///
	///////////////////////////////////////////////////////////
	void write(const imbxUint8* pBuffer, imbxUint32 bufferLength);

	/// \brief Write the specified amount of bits to the
	///         stream.
	///
	/// The functions uses a special bit pointer to keep track
	///  of the bytes that haven't been completly written.
	///
	/// The function throws a streamExceptionWrite exception
	///  if an error occurs.
	///
	/// @param buffer    bits to be written.
	///                  The bits must be right aligned
	/// @param bitsNum   the number of bits to write.
	///                  The function can write max 32 bits
	///                   
	///////////////////////////////////////////////////////////
	inline void writeBits(const imbxUint32 buffer, int bitsNum)
	{
		PUNTOEXE_FUNCTION_START(L"streamWriter::writeBits");

		imbxUint32 tempBuffer(buffer);

		while(bitsNum != 0)
		{
			if(bitsNum <= (8 - m_outBitsNum))
			{
				m_outBitsBuffer |= (imbxUint8)(tempBuffer << (8 - m_outBitsNum - bitsNum));
				m_outBitsNum += bitsNum;
                                if(m_outBitsNum==8)
                                {
                                        m_outBitsNum = 0;
                                        writeByte(m_outBitsBuffer);
                                        m_outBitsBuffer = 0;
                                }
                                return;
			}
			if(m_outBitsNum == 0 && bitsNum >= 8)
                        {
                                bitsNum -= 8;
                                writeByte(tempBuffer >> bitsNum);
                        }
                        else
                        {
                            m_outBitsBuffer |= (imbxUint8)(tempBuffer >> (bitsNum + m_outBitsNum - 8));
                            bitsNum -= (8-m_outBitsNum);
                            writeByte(m_outBitsBuffer);
                            m_outBitsBuffer = 0;
                            m_outBitsNum = 0;
                        }

                        tempBuffer &= (((imbxUint32)1) << bitsNum) - 1;

                }

		PUNTOEXE_FUNCTION_END();
	}

	/// \brief Reset the bit pointer used by writeBits().
	///
	/// A subsequent call to writeBits() will write data to
	///  a byte-aligned boundary.
	///
	///////////////////////////////////////////////////////////
	inline void resetOutBitsBuffer()
	{
		PUNTOEXE_FUNCTION_START(L"streamWriter::resetOutBitsBuffer");

		if(m_outBitsNum == 0)
			return;

		writeByte(m_outBitsBuffer);
		flushDataBuffer();
		m_outBitsBuffer = 0;
		m_outBitsNum = 0;

		PUNTOEXE_FUNCTION_END();
	}

	/// \brief Write a single byte to the stream, parsing it
	///         if m_pTagByte is not zero.
	///
	/// The byte to be written must be stored in the buffer 
	///  pointed by the parameter pBuffer.
	///
	/// If m_pTagByte is zero, then the function writes 
	///  the byte and returns.
	///
	/// If m_pTagByte is not zero, then the function adds a
	///  byte with value 0x0 after all the bytes with value
	///  0xFF.
	/// This mechanism is used to avoid the generation of
	///  the jpeg tags in a stream.
	///
	/// @param buffer    byte to be written
	///
	///////////////////////////////////////////////////////////
	inline void writeByte(const imbxUint8 buffer)
	{
		if(m_pDataBufferCurrent == m_pDataBufferMaxEnd)
		{
			flushDataBuffer();
		}
		*(m_pDataBufferCurrent++) = buffer;
		if(m_bJpegTags && buffer == (imbxUint8)0xff)
		{
			if(m_pDataBufferCurrent == m_pDataBufferMaxEnd)
			{
				flushDataBuffer();
			}
			*(m_pDataBufferCurrent++) = 0;
		}
	}

protected:
	/// \brief Flushes the internal buffer, disconnects the
	///         stream and destroys the streamWriter.
	///
	///////////////////////////////////////////////////////////
	virtual ~streamWriter();

private:
	imbxUint8 m_outBitsBuffer;
	int       m_outBitsNum;

};

///@}

} // namespace puntoexe


#endif // !defined(imebraStreamWriter_2C008538_F046_401C_8C83_2F76E1077DB0__INCLUDED_)
