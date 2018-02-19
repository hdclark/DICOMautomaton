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

/*! \file charsetConversion.cpp
    \brief Implementation of the charsetConversion class.

*/

#include "../include/exception.h"
#include "../include/charsetConversion.h"
#include <memory>

namespace puntoexe
{

///////////////////////////////////////////////////////////
//
// This table contains the recognized ISO charsets
//
///////////////////////////////////////////////////////////
static charsetInformation m_charsetTable[]={
	{"LOCALE", "", 0, false},
	{"ISO-IR 6", "ASCII", 20127, false},
	{"ISO-8859-1", "ISO-8859-1", 28591, false},
	{"ISO-8859-2", "ISO-8859-2", 28592, false},
	{"ISO-8859-3", "ISO-8859-3", 28593, false},
	{"ISO-8859-4", "ISO-8859-4", 28594, false},
	{"ISO-8859-5", "ISO-8859-5", 28595, false},
	{"ISO-8859-6", "ISO-8859-6", 28596, false},
	{"ISO-8859-7", "ISO-8859-7", 28597, false},
	{"ISO-8859-8", "ISO-8859-8", 28598, false},
	{"ISO-8859-9", "ISO-8859-9", 28599, false},
	{"ISO-IR 13",  "JIS-X0201",  50222, true},
	{"ISO-IR 166", "ISO-8859-11",874, false},
	{"ISO-IR 87",  "JIS-X0208" ,20932, false},
	{"ISO-IR 159", "JIS-X0212",20932, false},
	{"ISO-IR 149", "KSC-5601", 50225, false},

	{"ISO-IR 192", "UTF-8", 65001, true},
	{"GB18030",    "GB18030", 54936, true},

        {nullptr,nullptr,0, false}
};


///////////////////////////////////////////////////////////
//
// Constructor
//
///////////////////////////////////////////////////////////
charsetConversion::charsetConversion()
{
#if defined(PUNTOEXE_USEICONV)
	m_iconvToUnicode = (iconv_t)-1;
	m_iconvFromUnicode = (iconv_t)-1;
#else
	m_codePage = 0;
	m_bZeroFlag = false;
#endif
}


///////////////////////////////////////////////////////////
//
// Destructor
//
///////////////////////////////////////////////////////////
charsetConversion::~charsetConversion()
{
	close();
}


///////////////////////////////////////////////////////////
//
// Find the ID of the charset with the specified name
//
///////////////////////////////////////////////////////////
int charsetConversion::findTable(const std::string& tableName) const
{
	PUNTOEXE_FUNCTION_START(L"charsetConversion::findTable");

	for(int scanTables = 0; m_charsetTable[scanTables].m_iconvName != nullptr; ++scanTables)
	{
		if(tableName == m_charsetTable[scanTables].m_iconvName ||
			tableName == m_charsetTable[scanTables].m_isoRegistration)
		{
			return scanTables;
		}
	}
	return -1;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
//
// Initialize the charsetConversion object
//
///////////////////////////////////////////////////////////
void charsetConversion::initialize(const std::string& tableName)
{
	PUNTOEXE_FUNCTION_START(L"charsetConversion::initialize");

	// Find the table ID
	///////////////////////////////////////////////////////////
	int requestedTable = findTable(tableName);
	if(requestedTable == -1)
	{
		close();
		PUNTOEXE_THROW(charsetConversionExceptionNoTable, "The requested ISO table doesn't exist");
	}

	// The specified table is already active. Simply return
	///////////////////////////////////////////////////////////
	if(m_charsetTable[requestedTable].m_isoRegistration == m_isoCharset)
	{
		return;
	}

	// Close the active table
	///////////////////////////////////////////////////////////
	close();

	// Initialize the codePage or the iconv
	///////////////////////////////////////////////////////////
#if defined(PUNTOEXE_USEICONV)
	std::string toCodeIgnore(m_charsetTable[requestedTable].m_iconvName);
	toCodeIgnore += "//IGNORE";

	// Check little endian/big endian
	///////////////////////////////////////////////////////////
	static imbxUint16 m_endianCheck=0x00ff;
	const char* utfCode;
	switch(sizeof(wchar_t))
	{
	case 2:
		utfCode = (*((imbxUint8*)&m_endianCheck) == 0xff) ? "UTF-16LE" : "UTF-16BE";
		break;
	case 4:
		utfCode = (*((imbxUint8*)&m_endianCheck) == 0xff) ? "UTF-32LE" : "UTF-32BE";
		break;
	default:
		PUNTOEXE_THROW(charsetConversionExceptionUtfSizeNotSupported, "The system utf size is not supported");
	}

	m_iconvToUnicode = iconv_open(utfCode, m_charsetTable[requestedTable].m_iconvName);
	m_iconvFromUnicode = iconv_open(toCodeIgnore.c_str(), utfCode);
	if(m_iconvToUnicode == (iconv_t)-1 || m_iconvFromUnicode == (iconv_t)-1)
	{
		PUNTOEXE_THROW(charsetConversionExceptionNoSupportedTable, "The requested ISO table is not supported by the system");
	}
#else
	m_codePage = m_charsetTable[requestedTable].m_codePage;
	m_bZeroFlag = m_charsetTable[requestedTable].m_bZeroFlag;
#endif

	// Save the name of the active table
	///////////////////////////////////////////////////////////
	m_isoCharset = m_charsetTable[requestedTable].m_isoRegistration;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
//
// Uninitialize the charsetConversion object
//
///////////////////////////////////////////////////////////
void charsetConversion::close()
{
	PUNTOEXE_FUNCTION_START(L"charsetConversion::close");

	m_isoCharset.clear();
#if defined(PUNTOEXE_USEICONV)
	if(m_iconvToUnicode != (iconv_t)-1)
	{
		iconv_close(m_iconvToUnicode);
		m_iconvToUnicode = (iconv_t)-1;
	}
	if(m_iconvFromUnicode != (iconv_t)-1)
	{
		iconv_close(m_iconvFromUnicode);
		m_iconvFromUnicode = (iconv_t)-1;
	}
#else
	m_codePage = 0;
	m_bZeroFlag = false;
#endif

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
//
// Return the ISO name of the active charset
//
///////////////////////////////////////////////////////////
std::string charsetConversion::getIsoCharset() const
{
	return m_isoCharset;
}


///////////////////////////////////////////////////////////
//
// Convert a string from unicode to multibyte
//
///////////////////////////////////////////////////////////
std::string charsetConversion::fromUnicode(const std::wstring& unicodeString) const
{
	PUNTOEXE_FUNCTION_START(L"charsetConversion::fromUnicode");

	if(unicodeString.empty())
	{
		return std::string();
	}
#if defined(PUNTOEXE_USEICONV)
	return myIconv(m_iconvFromUnicode, (char*)unicodeString.c_str(), unicodeString.length() * sizeof(wchar_t));
#else
	BOOL bUsedDefault = false;
	int requiredChars = ::WideCharToMultiByte(m_codePage, m_bZeroFlag ? 0 : WC_NO_BEST_FIT_CHARS | WC_COMPOSITECHECK | WC_DEFAULTCHAR, unicodeString.c_str(), (int)(unicodeString.length()), 0, 0, 0, m_bZeroFlag ? 0 : &bUsedDefault);
	std::auto_ptr<char> convertedString(new char[requiredChars]);
	::WideCharToMultiByte(m_codePage, m_bZeroFlag ? 0 : WC_NO_BEST_FIT_CHARS | WC_COMPOSITECHECK | WC_DEFAULTCHAR, unicodeString.c_str(), (int)(unicodeString.length()), convertedString.get(), requiredChars, 0, m_bZeroFlag ? 0 : &bUsedDefault);
	if(bUsedDefault)
	{
		return std::string();
	}
	std::string returnString(convertedString.get(), requiredChars);
	return returnString;
#endif

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
//
// Convert a string from multibyte to unicode
//
///////////////////////////////////////////////////////////
std::wstring charsetConversion::toUnicode(const std::string& asciiString) const
{
	PUNTOEXE_FUNCTION_START(L"charsetConversion::toUnicode");

	if(asciiString.empty())
	{
		return std::wstring();
	}
#if defined(PUNTOEXE_USEICONV)
	std::string convertedString(myIconv(m_iconvToUnicode, (char*)asciiString.c_str(), asciiString.length()));
	std::wstring returnString((wchar_t*)convertedString.c_str(), convertedString.size() / sizeof(wchar_t));
#else
	int requiredWChars = ::MultiByteToWideChar(m_codePage, 0, asciiString.c_str(), (int)(asciiString.length()), 0, 0);
	std::auto_ptr<wchar_t> convertedString(new wchar_t[requiredWChars]);
	::MultiByteToWideChar(m_codePage, 0, asciiString.c_str(), (int)(asciiString.length()), convertedString.get(), requiredWChars);
	std::wstring returnString(convertedString.get(), requiredWChars);
#endif
	return returnString;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
//
// In Posix systems executes an iconv
//
///////////////////////////////////////////////////////////
#if defined(PUNTOEXE_USEICONV)

#if defined(PUNTOEXE_WINDOWS)
std::string charsetConversion::myIconv(iconv_t context, const char* inputString, size_t inputStringLengthBytes)
#else
std::string charsetConversion::myIconv(iconv_t context, char* inputString, size_t inputStringLengthBytes)
#endif
{
	PUNTOEXE_FUNCTION_START(L"charsetConversion::myIconv");

	std::string finalString;

	// Reset the state
	///////////////////////////////////////////////////////////
	iconv(context, nullptr, nullptr, nullptr, nullptr);

	// Buffer for the conversion
	///////////////////////////////////////////////////////////
	char conversionBuffer[1024];

	// Convert the string
	///////////////////////////////////////////////////////////
	for(size_t iconvReturn = (size_t)-1; iconvReturn == (size_t)-1;)
	{
		// Executes the conversion
		///////////////////////////////////////////////////////////
		size_t progressiveOutputBufferSize = sizeof(conversionBuffer);
		char* progressiveOutputBuffer(conversionBuffer);
		iconvReturn = iconv(context, &inputString, &inputStringLengthBytes, &progressiveOutputBuffer, &progressiveOutputBufferSize);

		// Update buffer's size
		///////////////////////////////////////////////////////////
		size_t outputLengthBytes = sizeof(conversionBuffer) - progressiveOutputBufferSize;

		finalString.append(conversionBuffer, outputLengthBytes);

		// Throw if an invalid sequence is found
		///////////////////////////////////////////////////////////
		if(iconvReturn == (size_t)-1 && errno != E2BIG)
		{
		    return std::string();
		}
	}

	return finalString;

	PUNTOEXE_FUNCTION_END();
}
#endif

} // namespace puntoexe

