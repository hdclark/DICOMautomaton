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

/*! \file stream.cpp
    \brief Implementation of the stream class.

*/

#include "../include/exception.h"
#include "../include/stream.h"
#include "../include/charsetConversion.h"
#include <sstream>

namespace puntoexe
{

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// Stream
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
// Destructor
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
stream::~stream()
{
	if(m_openFile != 0)
	{
		::fclose(m_openFile);
	}
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Open a file (ansi)
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void stream::openFile(const std::string& fileName, const int mode)
{
	PUNTOEXE_FUNCTION_START(L"stream::openFile (ansi)");

	std::wstring wFileName;
	size_t fileNameSize(fileName.size());
	wFileName.resize(fileNameSize);
	for(size_t copyChars = 0; copyChars != fileNameSize; ++copyChars)
	{
		wFileName[copyChars] = (wchar_t)fileName[copyChars];
	}
	openFile(wFileName, mode);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Open a file (unicode)
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void stream::openFile(const std::wstring& fileName, const int mode)
{
	PUNTOEXE_FUNCTION_START(L"stream::openFile (unicode)");
	if(m_openFile)
	{
		if(::fclose(m_openFile)!=0)
		{
			PUNTOEXE_THROW(streamExceptionClose, "Error while closing the file");
		}
		m_openFile = 0;
	}

	std::wstring strMode;

	int tempMode = mode & (~std::ios::binary);

	if(tempMode == (int)(std::ios::in | std::ios::out))
	{
		strMode = L"r+";
	}

	if(tempMode == (int)(std::ios::in | std::ios::out | std::ios::app))
	{
		strMode = L"a+";
	}
	
	if(tempMode == (int)(std::ios::in | std::ios::out | std::ios::trunc))
	{
		strMode = L"w+";
	}
	
	if(tempMode == (int)(std::ios::out) || tempMode == (int)(std::ios::out | std::ios::trunc))
	{
		strMode = L"w";
	}
	
	if(tempMode == (int)(std::ios::out | std::ios::app))
	{
		strMode = L"a";
	}
	
	if(tempMode == (int)(std::ios::in))
	{
		strMode = L"r";
	}

	strMode += L"b";

#if defined(PUNTOEXE_WINDOWS)
	m_openFile = ::_wfopen(fileName.c_str(), strMode.c_str());
#else
	// Convert the filename to UTF8
	charsetConversion toUtf8;
	toUtf8.initialize("ISO-IR 192");
	std::string utf8FileName(toUtf8.fromUnicode(fileName));

	// Convert the filemode to UTF8
	std::string utf8Mode(toUtf8.fromUnicode(strMode));
	
	m_openFile = ::fopen(utf8FileName.c_str(), utf8Mode.c_str());
#endif
	if(m_openFile == 0)
	{
		PUNTOEXE_THROW(streamExceptionOpen, "stream::openFile failure");
	}

	PUNTOEXE_FUNCTION_END();
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
void stream::write(imbxUint32 startPosition, const imbxUint8* pBuffer, imbxUint32 bufferLength)
{
	PUNTOEXE_FUNCTION_START(L"stream::write");

	lockObject lockThis(this);

	::fseek(m_openFile, startPosition, SEEK_SET);
	if(ferror(m_openFile) != 0)
	{
		PUNTOEXE_THROW(streamExceptionWrite, "stream::seek failure");
	}

	if(::fwrite(pBuffer, 1, bufferLength, m_openFile) != bufferLength)
	{
		PUNTOEXE_THROW(streamExceptionWrite, "stream::write failure");
	}

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
imbxUint32 stream::read(imbxUint32 startPosition, imbxUint8* pBuffer, imbxUint32 bufferLength)
{
	PUNTOEXE_FUNCTION_START(L"stream::read");

	lockObject lockThis(this);

	::fseek(m_openFile, startPosition, SEEK_SET);
	if(ferror(m_openFile) != 0)
	{
		return 0;
	}

	imbxUint32 readBytes = (imbxUint32)::fread(pBuffer, 1, bufferLength, m_openFile);
	if(ferror(m_openFile) != 0)
	{
		PUNTOEXE_THROW(streamExceptionRead, "stream::read failure");
	}
	return readBytes;

	PUNTOEXE_FUNCTION_END();
}


} // namespace puntoexe
