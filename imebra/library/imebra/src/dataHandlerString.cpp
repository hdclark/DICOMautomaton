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

/*! \file dataHandlerString.cpp
    \brief Implementation of the base class for the string handlers.

*/

#include <sstream>
#include <iomanip>

#include "../../base/include/exception.h"
#include "../include/dataHandlerString.h"


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
// dataHandlerString
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
// Parse the buffer
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dataHandlerString::parseBuffer(const ptr<memory>& memoryBuffer)
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerString::parseBuffer");

	m_strings.clear();

	// Convert the string to unicode
	///////////////////////////////////////////////////////////
	std::wstring tempBufferUnicode;
	imbxUint32 tempBufferSize(memoryBuffer->size());
	if(tempBufferSize != 0)
	{
		tempBufferUnicode = convertToUnicode(std::string((char*)(memoryBuffer->data()), tempBufferSize));
	}

	// Remove the extra spaces and zero bytes
	///////////////////////////////////////////////////////////
	size_t removeTrail;
	for(removeTrail = tempBufferUnicode.size(); removeTrail != 0 && (tempBufferUnicode[removeTrail - 1] == 0x20 || tempBufferUnicode[removeTrail - 1] == 0x0); --removeTrail){};
	tempBufferUnicode.resize(removeTrail);

	if( getSeparator() == 0)
	{
		m_strings.push_back(tempBufferUnicode);
		return;
	}

	size_t nextPosition;
	imbxUint32 unitSize=getUnitSize();
	for(size_t firstPosition = 0; firstPosition<tempBufferUnicode.size(); firstPosition = nextPosition)
	{
		nextPosition = tempBufferUnicode.find(getSeparator(), firstPosition);
		bool bSepFound=(nextPosition != tempBufferUnicode.npos);
		if(!bSepFound)
			nextPosition = tempBufferUnicode.size();

		if(unitSize && (nextPosition-firstPosition) > (size_t)unitSize)
			nextPosition=firstPosition+unitSize;

		m_strings.push_back(tempBufferUnicode.substr(firstPosition, nextPosition-firstPosition));

		if(bSepFound)
		{
			++nextPosition;
		}
	}

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Rebuild the buffer
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dataHandlerString::buildBuffer(const ptr<memory>& memoryBuffer)
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerString::buildBuffer");

	std::wstring completeString;
	for(size_t stringsIterator = 0; stringsIterator < m_strings.size(); ++stringsIterator)
	{
		if(stringsIterator)
		{
			completeString += getSeparator();
		}
		completeString += m_strings[stringsIterator];
	}

	std::string asciiString = convertFromUnicode(completeString, &m_charsetsList);

	memoryBuffer->assign((imbxUint8*)asciiString.c_str(), (imbxUint32)asciiString.size());

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Returns TRUE if the pointer is valid
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
bool dataHandlerString::pointerIsValid(const imbxUint32 index) const
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerString::pointerIsValid");

	return index < m_strings.size();

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Get data element as a signed long
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxInt32 dataHandlerString::getSignedLong(const imbxUint32 index) const
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerString::getSignedLong");

	std::wstring tempString = getUnicodeString(index);
	std::wistringstream convStream(tempString);
	imbxInt32 value;
	convStream >> value;
	return value;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Get data element as an unsigned long
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint32 dataHandlerString::getUnsignedLong(const imbxUint32 index) const
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerString::getUnsignedLong");

	std::wstring tempString = getUnicodeString(index);
	std::wistringstream convStream(tempString);
	imbxUint32 value;
	convStream >> value;
	return value;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Get data element as a double
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
double dataHandlerString::getDouble(const imbxUint32 index) const
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerString::getDouble");

	std::wstring tempString = getUnicodeString(index);
	std::wistringstream convStream(tempString);
	double value;
	convStream >> value;
	return value;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Get data element as a string
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
std::string dataHandlerString::getString(const imbxUint32 index) const
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerString::getString");

        charsetsList::tCharsetsList localCharsetsList(m_charsetsList);
	return convertFromUnicode(getUnicodeString(index), &localCharsetsList);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Get data element as an unicode string
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
std::wstring dataHandlerString::getUnicodeString(const imbxUint32 index) const
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerString::getUnicodeString");

	if(index >= m_strings.size())
	{
		return L"";
	}
	return m_strings[index];

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Set data element as a signed long
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dataHandlerString::setSignedLong(const imbxUint32 index, const imbxInt32 value)
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerString::setSignedLong");

	std::wostringstream convStream;
	convStream << value;
	setUnicodeString(index, convStream.str());

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Set data element as an unsigned long
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dataHandlerString::setUnsignedLong(const imbxUint32 index, const imbxUint32 value)
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerString::setUnsignedLong");

	std::wostringstream convStream;
	convStream << value;
	setUnicodeString(index, convStream.str());

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Set data element as a double
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dataHandlerString::setDouble(const imbxUint32 index, const double value)
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerString::setDouble");

	std::wostringstream convStream;
	convStream << std::fixed << value;
	setUnicodeString(index, convStream.str());

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Set data element as a string
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dataHandlerString::setString(const imbxUint32 index, const std::string& value)
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerString::setString");

	setUnicodeString(index, convertToUnicode(value));

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Set data element as an string
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dataHandlerString::setUnicodeString(const imbxUint32 index, const std::wstring& value)
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerString::setUnicodeString");

	if(index >= m_strings.size())
        {
            return;
        }
        imbxUint32 stringMaxSize(maxSize());

        if(stringMaxSize > 0 && value.size() > stringMaxSize)
        {
            m_strings[index] = value.substr(0, stringMaxSize);
            return;
        }
        m_strings[index] = value;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Set the buffer's size (in data elements)
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dataHandlerString::setSize(const imbxUint32 elementsNumber)
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerString::setSize");

	if(getSeparator() == 0)
	{
		m_strings.resize(1, L"");
                return;
	}

	m_strings.resize(elementsNumber, L"");

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Get the size in strings
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint32 dataHandlerString::getSize() const
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerString::getSize");

	return (imbxUint32)m_strings.size();

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Get the max size of a string
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint32 dataHandlerString::maxSize() const
{
	return 0;
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Get the separator
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
wchar_t dataHandlerString::getSeparator() const
{
	return L'\\';
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Convert a string to unicode, without using the dicom
//  charsets
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
std::wstring dataHandlerString::convertToUnicode(const std::string& value) const
{
	size_t stringLength = value.size();
	std::wstring returnString;
	returnString.resize(stringLength, 0);
	for(size_t copyString = 0; copyString < stringLength; ++copyString)
	{
		returnString[copyString]=(wchar_t)value[copyString];
	}
	return returnString;
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Convert a string from unicode, without using the dicom 
//  charsets
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
std::string dataHandlerString::convertFromUnicode(const std::wstring& value, charsetsList::tCharsetsList* /* pCharsetsList */) const
{
	size_t stringLength = value.size();
	std::string returnString;
	returnString.resize(stringLength);
	for(size_t copyString = 0; copyString < stringLength; ++copyString)
	{
		returnString[copyString]=(char)value[copyString];
	}
	return returnString;
}


} // namespace handlers

} // namespace imebra

} // namespace puntoexe
