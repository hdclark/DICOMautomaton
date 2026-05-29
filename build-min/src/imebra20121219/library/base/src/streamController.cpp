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

/*! \file streamController.cpp
    \brief Implementation of the streamController class.

*/

#include "../include/streamController.h"

namespace puntoexe
{

// Used for the endian check
///////////////////////////////////////////////////////////
static const imbxUint16 m_endianCheck(0x00ff);
static imbxUint8 const * const pBytePointer((imbxUint8*)&m_endianCheck);
static const streamController::tByteOrdering m_platformByteOrder((*pBytePointer)==0xff ? streamController::lowByteEndian : streamController::highByteEndian);

///////////////////////////////////////////////////////////
//
// Constructor
//
///////////////////////////////////////////////////////////
streamController::streamController(ptr<baseStream> pControlledStream, imbxUint32 virtualStart /* =0 */, imbxUint32 virtualLength /* =0 */):
	m_bJpegTags(false),
        m_pControlledStream(pControlledStream),
		m_dataBuffer(new imbxUint8[IMEBRA_STREAM_CONTROLLER_MEMORY_SIZE]),
		m_virtualStart(virtualStart),
		m_virtualLength(virtualLength),
		m_dataBufferStreamPosition(0)
{
	m_pDataBufferStart = m_pDataBufferEnd = m_pDataBufferCurrent = m_dataBuffer.get();
	m_pDataBufferMaxEnd = m_pDataBufferStart + IMEBRA_STREAM_CONTROLLER_MEMORY_SIZE;
}


///////////////////////////////////////////////////////////
//
// Retrieve the current position
//
///////////////////////////////////////////////////////////
imbxUint32 streamController::position()
{
	return m_dataBufferStreamPosition + (imbxUint32)(m_pDataBufferCurrent - m_pDataBufferStart);
}


///////////////////////////////////////////////////////////
//
// Retrieve a pointer to the controlled stream
//
///////////////////////////////////////////////////////////
ptr<baseStream> streamController::getControlledStream()
{
	return m_pControlledStream;
}


///////////////////////////////////////////////////////////
//
// Retrieve the position without considering the virtual
//  start's position
//
///////////////////////////////////////////////////////////
imbxUint32 streamController::getControlledStreamPosition()
{
	return m_dataBufferStreamPosition + (imbxUint32)(m_pDataBufferCurrent - m_pDataBufferStart) + m_virtualStart;
}


///////////////////////////////////////////////////////////
//
// Adjust the byte ordering of pBuffer
//
///////////////////////////////////////////////////////////
void streamController::adjustEndian(imbxUint8* pBuffer, const imbxUint32 wordLength, const tByteOrdering endianType, const imbxUint32 words /* =1 */)
{
	if(endianType == m_platformByteOrder || wordLength<2L)
	{
		return;
	}

	switch(wordLength)
	{
	case 2:
		{ // Block needed by evc4. Prevent error on multiple definitions of scanWords
			imbxUint8 tempByte;
			for(imbxUint32 scanWords = words; scanWords != 0; --scanWords)
			{
				tempByte=*pBuffer;
				*pBuffer=*(pBuffer+1);
				*(++pBuffer)=tempByte;
				++pBuffer;
			}
		}
		return;
	case 4:
		{ // Block needed by evc4. Prevent error on multiple definitions of scanWords
			imbxUint8 tempByte0;
			imbxUint8 tempByte1;
			for(imbxUint32 scanWords = words; scanWords != 0; --scanWords)
			{
				tempByte0 = *pBuffer;
				*pBuffer = *(pBuffer+3);
				tempByte1 = *(++pBuffer);
				*pBuffer = *(pBuffer + 1);
				*(++pBuffer) = tempByte1;
				*(++pBuffer) = tempByte0;
				++pBuffer;
			}
		}
		return;
	}
}





} // namespace puntoexe
