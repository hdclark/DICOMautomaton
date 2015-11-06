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

/*! \file dataHandlerStringDS.cpp
    \brief Implementation of the class dataHandlerStringDS.

*/

#include <sstream>
#include <iomanip>

#include "../../base/include/exception.h"
#include "../include/dataHandlerStringDS.h"

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
// dataHandlerStringDS
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
// Get the value as a signed long.
// Overwritten to use getDouble()
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxInt32 dataHandlerStringDS::getSignedLong(const imbxUint32 index) const
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerStringDS::getSignedLong");

	return (imbxInt32)getDouble(index);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Get the value as an unsigned long.
// Overwritten to use getDouble()
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint32 dataHandlerStringDS::getUnsignedLong(const imbxUint32 index) const
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerStringDS::getUnsignedLong");

	return (imbxInt32)getDouble(index);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Set the value as a signed long.
// Overwritten to use setDouble()
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dataHandlerStringDS::setSignedLong(const imbxUint32 index, const imbxInt32 value)
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerStringDS::setSignedLong");

	setDouble(index, (double)value);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Set the value as an unsigned long.
// Overwritten to use setDouble()
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dataHandlerStringDS::setUnsignedLong(const imbxUint32 index, const imbxUint32 value)
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerStringDS::setUnsignedLong");

	setDouble(index, (double)value);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Overwrite setDouble so the value is written in the
//  exponential form if needed
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dataHandlerStringDS::setDouble(const imbxUint32 index, const double value)
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerStringDS::setDouble");

	std::wostringstream convStream;
	convStream << value;
	setUnicodeString(index, convStream.str());

	PUNTOEXE_FUNCTION_END();
}

imbxUint8 dataHandlerStringDS::getPaddingByte() const
{
	return 0x20;
}

imbxUint32 dataHandlerStringDS::getUnitSize() const
{
	return 0;
}

imbxUint32 dataHandlerStringDS::maxSize() const
{
	return 16;
}

} // namespace handlers

} // namespace imebra

} // namespace puntoexe
