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

/*! \file memoryStream.cpp
    \brief Implementation of the memoryStream class.

*/

#include "../include/exception.h"
#include "../include/memoryStream.h"
#include <cstring>

namespace puntoexe
{

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// memoryStream
//
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Constructor
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
memoryStream::memoryStream(ptr<memory> memoryStream): m_memory(memoryStream)
{
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Write raw data into the stream
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void memoryStream::write(imbxUint32 startPosition, const imbxUint8* pBuffer, imbxUint32 bufferLength)
{
	PUNTOEXE_FUNCTION_START(L"memoryStream::write");

	// Nothing happens if we have nothing to write
	///////////////////////////////////////////////////////////
	if(bufferLength == 0)
	{
		return;
	}

	lockObject lockThis(this);

	// Copy the buffer into the memory
	///////////////////////////////////////////////////////////
	if(startPosition + bufferLength > m_memory->size())
	{
		imbxUint32 newSize = startPosition + bufferLength;
		imbxUint32 reserveSize = ((newSize + 1023) >> 10) << 10; // preallocate blocks of 1024 bytes
		m_memory->reserve(reserveSize);
		m_memory->resize(startPosition + bufferLength);
	}

	::memcpy(m_memory->data() + startPosition, pBuffer, bufferLength);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Read raw data from the stream
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint32 memoryStream::read(imbxUint32 startPosition, imbxUint8* pBuffer, imbxUint32 bufferLength)
{
	PUNTOEXE_FUNCTION_START(L"memoryStream::read");

	if(bufferLength == 0)
	{
		return 0;
	}

	lockObject lockThis(this);

	// Don't read if the requested position isn't valid
	///////////////////////////////////////////////////////////
	imbxUint32 memorySize = m_memory->size();
	if(startPosition >= memorySize)
	{
		return 0;
	}

	// Check if all the bytes are available
	///////////////////////////////////////////////////////////
	imbxUint32 copySize = bufferLength;
	if(startPosition + bufferLength > memorySize)
	{
		copySize = memorySize - startPosition;
	}

	if(copySize == 0)
	{
		return 0;
	}

	::memcpy(pBuffer, m_memory->data() + startPosition, copySize);

	return copySize;

	PUNTOEXE_FUNCTION_END();
}


} // namespace puntoexe
