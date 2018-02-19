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

/*! \file dataHandler.cpp
    \brief Implementation of the base class for the data handlers.

*/

#include "../../base/include/exception.h"
#include "../include/dataHandler.h"

namespace puntoexe
{

namespace imebra
{

namespace handlers
{

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// imebraDataHandler
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
// Disconnect the handler
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
bool dataHandler::preDelete()
{
	PUNTOEXE_FUNCTION_START(L"dataHandler::preDelete");

	if(!m_bCommitted)
	{
		lockObject lockAccess(m_buffer.get());

		copyBack();
		commit();
	}
	return true;

	PUNTOEXE_FUNCTION_END();
}


void dataHandler::copyBack()
{
	PUNTOEXE_FUNCTION_START(L"dataHandler::copyBack");

	if(m_buffer == nullptr)
	{
		return;
	}
	m_buffer->copyBack(this);

	PUNTOEXE_FUNCTION_END();
}

void dataHandler::commit()
{
	PUNTOEXE_FUNCTION_START(L"dataHandler::copyBack");

	if(m_buffer == nullptr)
	{
		return;
	}

	m_buffer->commit();
	m_bCommitted = true;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Discard all the changes made on a writing handler
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dataHandler::abort()
{
	m_buffer.release();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Retrieve an element's size
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint32 dataHandler::getUnitSize() const
{
	return 0;
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Retrieve the padding byte
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint8 dataHandler::getPaddingByte() const
{
	return 0;
}



///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Retrieve the data 's type
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
std::string dataHandler::getDataType() const
{
	return m_bufferType;
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Parse the buffer's content
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dataHandler::parseBuffer(const imbxUint8* pBuffer, const imbxUint32 bufferLength)
{
	PUNTOEXE_FUNCTION_START(L"dataHandler::parseBuffer");

	ptr<memory> tempMemory(memoryPool::getMemoryPool()->getMemory(bufferLength));
	if(pBuffer && bufferLength)
	{
		tempMemory->assign(pBuffer, bufferLength);
	}
	parseBuffer(tempMemory);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Retrieve the date
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dataHandler::getDate(const imbxUint32 /* index */,
		imbxInt32* pYear, 
		imbxInt32* pMonth, 
		imbxInt32* pDay, 
		imbxInt32* pHour, 
		imbxInt32* pMinutes,
		imbxInt32* pSeconds,
		imbxInt32* pNanoseconds,
		imbxInt32* pOffsetHours,
		imbxInt32* pOffsetMinutes) const
{
	*pYear = 0;
	*pMonth = 0;
	*pDay = 0;
	*pHour = 0;
	*pMinutes = 0;
	*pSeconds = 0;
	*pNanoseconds = 0;
	*pOffsetHours = 0;
	*pOffsetMinutes = 0;
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Set the date
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dataHandler::setDate(const imbxUint32 /* index */,
		imbxInt32 /* year */, 
		imbxInt32 /* month */, 
		imbxInt32 /* day */, 
		imbxInt32 /* hour */, 
		imbxInt32 /*minutes */,
		imbxInt32 /*seconds */,
		imbxInt32 /*nanoseconds */,
		imbxInt32 /*offsetHours */,
		imbxInt32 /*offsetMinutes */)
{
	return;
}

void dataHandler::setCharsetsList(charsetsList::tCharsetsList* /* pCharsetsList */)
{
	// Intentionally empty
}

void dataHandler::getCharsetsList(charsetsList::tCharsetsList* /* pCharsetsList */) const
{
	// Intentionally empty
}


} // namespace handlers

} // namespace imebra

} // namespace puntoexe
