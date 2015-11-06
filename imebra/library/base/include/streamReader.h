/*

Imebra 2011 build 2011-09-18_22-24-41

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

/*! \file streamReader.h
    \brief Declaration of the the class used to read the streams.

*/

#if !defined(imebraStreamReader_F6221390_BC44_4B83_B5BB_3485222FF1DD__INCLUDED_)
#define imebraStreamReader_F6221390_BC44_4B83_B5BB_3485222FF1DD__INCLUDED_

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

/// \brief Exception thrown when a jpeg tag is found but
///         wasn't expected.
///
///////////////////////////////////////////////////////////
class streamJpegTagInStream : public streamException
{
public:
	streamJpegTagInStream(std::string message): streamException(message){}
};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief Represents a stream reader.
///        A stream reader can read data from a stream.
///        Several stream readers can share a single
///        baseStream derived object.
///
/// The stream reader object is not multithread safe, but
///  one single stream can have several streamReaders
///  (in several threads) connected to it.
///
/// A stream reader can also be connected only to a part
///  of a stream: when this feature is used, the
///  streamReaded's client thinks that he is using a
///  whole stream, while the reader limits its view
///  to allowed stream's bytes only.
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class streamReader: public streamController
{
public:
	/// \brief Builf a streamReader and connect it to an
	///         existing stream.
	///
	/// The stream reader can also be connected to only a part
	///  of the stream.
	///
	/// When the streamReader is connected to a part of a
	///  stream then all the its functions will act on the
	///  viewable stream's part only.
	///
	/// @param pControlledStream  the stream that will be
	///                            controlled by the reader
	/// @param virtualStart       the first stream's byte
	///                            visible to the reader
	/// @param virtualLength      the number of bytes visible
	///                            to the reader. A value of 0
	///                            means that all the bytes
	///                            are visible
	///
	///////////////////////////////////////////////////////////
	streamReader(ptr<baseStream> pControlledStream, imbxUint32 virtualStart = 0, imbxUint32 virtualLength = 0);

	/// \brief Read raw data from the stream.
	///
	/// The number of bytes specified in the parameter
	///  bufferLength will be read from the stream and copied
	///  into a buffer referenced by the parameter pBuffer.
	/// The buffer's size must be equal or greater than
	///  the number of bytes to read.
	///
	/// The functions throws a streamExceptionRead
	///  exception if an error occurs.
	///
	/// @param pBuffer   a pointer to the buffer where the
	///                  read data must be copied.
	///                  the buffer's size must be at least
	///                  equal to the size indicated in the
	///                  bufferLength parameter.
	/// @param bufferLength the number of bytes to read from
	///                  the stream.
	///
	///////////////////////////////////////////////////////////
	void read(imbxUint8* pBuffer, imbxUint32 bufferLength);

	/// \brief Returns true if the last byte in the stream
	///         has already been read.
	///
	/// @return true if the last byte in the stream has already
	///          been read
	///
	///////////////////////////////////////////////////////////
	bool endReached();

	/// \brief Seek the stream's read position.
	///
	/// The read position is moved to the specified byte in the
	///  stream.
	/// Subsequent calls to the read operations like read(),
	///  readBits(), readBit(), addBit() and readByte() will
	///  read data from the position specified here.
	///
	/// @param newPosition the new position to use for read
	///                   operations, in bytes from the
	///                   beginning of the stream if bCurrent
	///                   is false, or in bytes relative to
	///                   the current position if bCurrent is
	///                   true
	/// @param bCurrent  when this parameter is true then the
	///                   new read position is calculated by
	///                   adding the content of position to
	///                   the current position, otherwise the
	///                   parameter position stores the new
	///                   absolute position
	///
	///////////////////////////////////////////////////////////
	void seek(imbxInt32 newPosition, bool bCurrent = false);

	/// \brief Read the specified amount of bits from the
	///         stream.
	///
	/// The functions uses a special bit pointer to keep track
	///  of the bytes that haven't been completly read.
	///
	/// The function throws a streamExceptionRead exception if
	///  an error occurs.
	///
	/// @param bitsNum   the number of bits to read.
	///                  The function can read 32 bits maximum
	/// @return an integer containing the fetched bits, right
	///                   aligned
	///
	///////////////////////////////////////////////////////////
	inline imbxUint32 readBits(int bitsNum)
	{
		static const int bufferSize(sizeof(m_inBitsBuffer) * 8);

		// All the requested bits are already in the buffer.
		// Just return them.
		///////////////////////////////////////////////////////////
		if(bitsNum <= m_inBitsNum)
		{
			imbxUint32 returnValue(m_inBitsBuffer >> (bufferSize - bitsNum));
			m_inBitsBuffer <<= bitsNum;
			m_inBitsNum -= bitsNum;
			return returnValue;
		}

		PUNTOEXE_FUNCTION_START(L"streamReader::readBits");

		// Fill a local variable with the read bits
		///////////////////////////////////////////////////////////
		imbxUint32 returnValue(0);

		// Some bits are in the bits buffer, copy them and reset
		//  the bits buffer
		///////////////////////////////////////////////////////////
		if(m_inBitsNum != 0)
		{
			bitsNum -= m_inBitsNum;
			returnValue = ((imbxUint32)(m_inBitsBuffer >> (bufferSize - m_inBitsNum))) << bitsNum;
		}

		// Read the requested number of bits
		///////////////////////////////////////////////////////////
		for(;;)
		{
			if(bitsNum <= 8)
			{
                                m_inBitsBuffer = readByte();
				returnValue |= (m_inBitsBuffer >> (bufferSize - bitsNum));
				m_inBitsBuffer <<= bitsNum;
				m_inBitsNum = 8 - bitsNum;
				return returnValue;
			}

			bitsNum -= 8;
			returnValue |= ((imbxUint32)readByte()) << bitsNum;
		}

		PUNTOEXE_FUNCTION_END();
	}

	/// \brief Read one bit from the stream.
	///
	/// The returned buffer will store the value 0 or 1,
	///  depending on the value of the read bit.
	///
	/// The function throws a streamExceptionRead if an error
	///  occurs.
	///
	/// @return the value of the read bit (1 or 0)
        ///
	///////////////////////////////////////////////////////////
	inline imbxUint32 readBit()
	{
		if(m_inBitsNum != 0)
		{
			--m_inBitsNum;
			if((imbxInt8)m_inBitsBuffer < 0)
			{
                            m_inBitsBuffer <<= 1;
                            return 1;
			}
                        m_inBitsBuffer <<= 1;
			return 0;
		}

		PUNTOEXE_FUNCTION_START(L"streamReader::readBit");

		m_inBitsBuffer = readByte();
                m_inBitsNum = 7; // We consider that one bit will go away
                if((imbxInt8)m_inBitsBuffer < 0)
                {
                        m_inBitsBuffer <<= 1;
                        return 1;
                }
                m_inBitsBuffer <<= 1;
                return 0;

		PUNTOEXE_FUNCTION_END();
	}


	/// \brief Read one bit from the stream and add its value
	///         to the specified buffer.
	///
	/// The buffer pointed by the pBuffer parameter is
	///  left-shifted before the read bit is inserted in the
	///  least significant bit of the buffer.
	///
	/// The function throws a streamExceptionRead if an error
	///  occurs.
	///
	/// @param pBuffer   a pointer to a imbxUint32 value that
	///                   will be left shifted and filled
	///                   with the read bit.
        ///
	///////////////////////////////////////////////////////////
	inline void addBit(imbxUint32* const pBuffer)
	{
        	(*pBuffer) <<= 1;
                
		if(m_inBitsNum != 0)
		{
			if((imbxInt8)m_inBitsBuffer < 0)
			{
				++(*pBuffer);
			}
			--m_inBitsNum;
			m_inBitsBuffer <<= 1;
                        return;
		}

		PUNTOEXE_FUNCTION_START(L"streamReader::addBit");

		m_inBitsBuffer = readByte();
                m_inBitsNum = 7; // We consider that one bit will go away
                if((imbxInt8)m_inBitsBuffer < 0)
                {
                        ++(*pBuffer);
                }
                m_inBitsBuffer <<= 1;

		PUNTOEXE_FUNCTION_END();
	}

	/// \brief Reset the bit pointer used by readBits(),
	///         readBit() and addBit().
	///
	/// A subsequent call to readBits(), readBit and
	///  addBit() will read data from a byte-aligned boundary.
	///
	///////////////////////////////////////////////////////////
	inline void resetInBitsBuffer()
	{
		m_inBitsNum = 0;
	}

	/// \brief Read a single byte from the stream, parsing it
	///         if m_pTagByte is not zero.
	///
	/// The read byte is stored in the buffer pointed by the
	///  parameter pBuffer.
	///
	/// If m_pTagByte is zero, then the function reads a byte
	///  and returns true.
	///
	/// If m_pTagByte is not zero, then the function skips
	///  all the bytes in the stream that have the value 0xFF.
	/// If the function doesn't have to skip any 0xFF bytes,
	///  then the function just read a byte and returns true.
	///
	/// If one or more 0xFF bytes have been skipped, then
	///  the function returns a value depending on the byte
	///  that follows the 0xFF run:
	/// - if the byte is 0, then the function fill the pBuffer
	///    parameter with a value 0xFF and returns true
	/// - if the byte is not 0, then that value is stored in
	///    the location pointed by m_pTagByte and the function
	///    returns false.
	///
	/// This mechanism is used to parse the jpeg tags in a
	///  stream.
	///
	/// @return          the read byte
	///
	///////////////////////////////////////////////////////////
	inline imbxUint8 readByte()
	{
		// Update the data buffer if it is empty
		///////////////////////////////////////////////////////////
		if(m_pDataBufferCurrent == m_pDataBufferEnd && fillDataBuffer() == 0)
                {
                    throw(streamExceptionEOF("Attempt to read past the end of the file"));
                }

		// Read one byte. Return immediatly if the tags are not
		//  activated
		///////////////////////////////////////////////////////////
                if(*m_pDataBufferCurrent != 0xff || !m_bJpegTags)
                {
                    return *(m_pDataBufferCurrent++);
                }
                do
                {
                    if(++m_pDataBufferCurrent == m_pDataBufferEnd && fillDataBuffer() == 0)
                    {
                        throw(streamExceptionEOF("Attempt to read past the end of the file"));
                    }
                }while(*m_pDataBufferCurrent == 0xff);

                if(*(m_pDataBufferCurrent++) != 0)
                {
                    throw(streamJpegTagInStream("Corrupted jpeg stream"));
                }

                return 0xff;
	}

private:
	/// \brief Read data from the file into the data buffer.
	///
	/// The function reads as many bytes as possible until the
	///  data buffer is full or the controlled stream cannot
	///  supply any more byte.
	///
	///////////////////////////////////////////////////////////
	imbxUint32 fillDataBuffer();

private:
	imbxUint8 m_inBitsBuffer;
	int       m_inBitsNum;

};

///@}

} // namespace puntoexe

#endif // !defined(imebraStreamReader_F6221390_BC44_4B83_B5BB_3485222FF1DD__INCLUDED_)
