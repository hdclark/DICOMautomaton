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

/*! \file streamReader.cpp
    \brief Implementation of the streamReader class.

*/

#include "../include/streamReader.h"
#include <string.h>

namespace puntoexe
{

///////////////////////////////////////////////////////////
//
// Constructor
//
///////////////////////////////////////////////////////////
streamReader::streamReader(ptr<baseStream> pControlledStream, imbxUint32 virtualStart /* =0 */, imbxUint32 virtualLength /* =0 */):
	streamController(pControlledStream, virtualStart, virtualLength),
	m_inBitsBuffer(0),
	m_inBitsNum(0)
{
}


///////////////////////////////////////////////////////////
//
// Returns true if the last byte has been read
//
///////////////////////////////////////////////////////////
bool streamReader::endReached()
{
    return (m_pDataBufferCurrent == m_pDataBufferEnd && fillDataBuffer() == 0);
}


///////////////////////////////////////////////////////////
//
// Refill the data buffer
//
///////////////////////////////////////////////////////////
imbxUint32 streamReader::fillDataBuffer()
{
	PUNTOEXE_FUNCTION_START(L"streamReader::fillDataBuffer");

	imbxUint32 currentPosition = position();
	imbxUint32 readLength = (imbxUint32)(m_pDataBufferMaxEnd - m_pDataBufferStart);
	if(m_virtualLength != 0)
	{
		if(currentPosition >= m_virtualLength)
		{
			m_dataBufferStreamPosition = m_virtualLength;
			m_pDataBufferCurrent = m_pDataBufferEnd = m_pDataBufferStart;
			return 0;
		}
		if(currentPosition + readLength > m_virtualLength)
		{
			readLength = m_virtualLength - currentPosition;
		}
	}
	imbxUint32 readBytes = m_pControlledStream->read(currentPosition + m_virtualStart, m_pDataBufferStart, readLength);
	m_dataBufferStreamPosition = currentPosition;
	m_pDataBufferEnd = m_pDataBufferStart + readBytes;
	m_pDataBufferCurrent = m_pDataBufferStart;
	return readBytes;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
//
// Return the specified number of bytes from the stream
//
///////////////////////////////////////////////////////////
void streamReader::read(imbxUint8* pBuffer, imbxUint32 bufferLength)
{
	while(bufferLength != 0)
	{
		// Update the data buffer if it is empty
		///////////////////////////////////////////////////////////
		if(m_pDataBufferCurrent == m_pDataBufferEnd)
		{
			// EOF?
			///////////////////////////////////////////////////////////
			if(fillDataBuffer() == 0)
			{
				throw(streamExceptionEOF("Attempt to read past the end of the file"));
			}
		}

		// Copy the available data into the return buffer
		///////////////////////////////////////////////////////////
		imbxUint32 copySize = bufferLength;
		imbxUint32 maxSize = (imbxUint32)(m_pDataBufferEnd - m_pDataBufferCurrent);
		if(copySize > maxSize)
		{
			copySize = maxSize;
		}
		::memcpy(pBuffer, m_pDataBufferCurrent, (size_t)copySize);
		bufferLength -= copySize;
		pBuffer += copySize;
		m_pDataBufferCurrent += copySize;
	}
}


///////////////////////////////////////////////////////////
//
// Seek the read position
//
///////////////////////////////////////////////////////////
void streamReader::seek(imbxInt32 newPosition, bool bCurrent /* =false */)
{
	// Calculate the absolute position
	///////////////////////////////////////////////////////////
	imbxUint32 finalPosition = bCurrent ? (position() + newPosition) : newPosition;

	// The requested position is already in the data buffer?
	///////////////////////////////////////////////////////////
	imbxUint32 bufferEndPosition = m_dataBufferStreamPosition + (imbxUint32)(m_pDataBufferEnd - m_pDataBufferStart);
	if(finalPosition >= m_dataBufferStreamPosition && finalPosition < bufferEndPosition)
	{
		m_pDataBufferCurrent = m_pDataBufferStart + finalPosition - m_dataBufferStreamPosition;
		return;
	}

	// The requested position is not in the data buffer
	///////////////////////////////////////////////////////////
	m_pDataBufferCurrent = m_pDataBufferEnd = m_pDataBufferStart;
	m_dataBufferStreamPosition = finalPosition;

	fillDataBuffer();
}



} // namespace puntoexe
