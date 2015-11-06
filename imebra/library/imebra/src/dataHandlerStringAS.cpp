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

/*! \file dataHandlerStringAS.cpp
    \brief Implementation of the class dataHandlerStringAS.

*/

#include <sstream>
#include <iomanip>

#include "../../base/include/exception.h"
#include "../include/dataHandlerStringAS.h"

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
// dataHandlerStringAS
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
// Set the age
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dataHandlerStringAS::setAge(const imbxUint32 index, const imbxUint32 age, const tAgeUnit unit)
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerStringAS::setAge");

	std::wostringstream ageStream;
	ageStream << std::setfill(L'0');
	ageStream << std::setw(3) << age;
	ageStream << std::setw(1) << (wchar_t)unit;

	setUnicodeString(index, ageStream.str());

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Retrieve the age
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint32 dataHandlerStringAS::getAge(const imbxUint32 index, tAgeUnit* pUnit) const
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerStringAS::getAge");

	std::wstring ageString = getUnicodeString(index);
	if(ageString.size() < 3)
	{
		ageString.resize(3, L'0');
	}
	if(ageString.size() < 4)
	{
		ageString.resize(4, L'Y');
	}
	std::wistringstream ageStream(ageString);
	imbxUint32 age;
	ageStream >> age;
	*pUnit = (tAgeUnit)ageString[3];

	return age;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Retrieve the age in years as a signed long
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxInt32 dataHandlerStringAS::getSignedLong(const imbxUint32 index) const
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerStringAS::getSignedLong");

	return (imbxInt32)getDouble(index);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Retrieve the age in years as an unsigned long
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint32 dataHandlerStringAS::getUnsignedLong(const imbxUint32 index) const
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerStringAS::getUnsignedLong");

	return (imbxInt32)getDouble(index);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Retrieve the age in years as a double
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
double dataHandlerStringAS::getDouble(const imbxUint32 index) const
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerStringAS::getDouble");

	tAgeUnit ageUnit;
	double age = (double)getAge(index, &ageUnit);

	if(ageUnit == days)
	{
		return age / (double)365;
	}
	if(ageUnit == weeks)
	{
		return age / 52.14;
	}
	if(ageUnit == months)
	{
		return age / (double)12;
	}
	return age;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Set the age in years as a signed long
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dataHandlerStringAS::setSignedLong(const imbxUint32 index, const imbxInt32 value)
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerStringAS::setSignedLong");

	setDouble(index, (double)value);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Set the age in years as an unsigned long
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dataHandlerStringAS::setUnsignedLong(const imbxUint32 index, const imbxUint32 value)
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerStringAS::setUnsignedLong");

	setDouble(index, (double)value);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Set the age in years as a double
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dataHandlerStringAS::setDouble(const imbxUint32 index, const double value)
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerStringAS::setDouble");

	if(value < 0)
	{
		setAge(index, 0, days);
	}
	if(value < 0.08)
	{
		setAge(index, (imbxUint32)(value * 365), days);
		return;
	}
	if(value < 0.5)
	{
		setAge(index, (imbxUint32)(value * 52.14), weeks);
		return;
	}
	if(value < 2)
	{
		setAge(index, (imbxUint32)(value * 12), months);
		return;
	}
	setAge(index, (imbxUint32)value, years);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Get the padding byte
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint8 dataHandlerStringAS::getPaddingByte() const
{
	return 0x20;
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Get the element's size
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint32 dataHandlerStringAS::getUnitSize() const
{
	return 4L;
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Get the maximum size
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint32 dataHandlerStringAS::maxSize() const
{
	return 4L;
}

} // namespace handlers

} // namespace imebra

} // namespace puntoexe
