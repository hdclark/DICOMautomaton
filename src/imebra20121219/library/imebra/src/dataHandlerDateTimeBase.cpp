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

/*! \file dataHandlerDateTimeBase.cpp
    \brief Implementation of the base class for the date/time handlers.

*/

#include "../../base/include/exception.h"
#include "../include/dataHandlerDateTimeBase.h"
#include <ctime>
#include <cstdlib>
#include <sstream>
#include <iomanip>

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
// dataHandlerDateTimeBase
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
// Returns a long integer representing the date/time (UTC)
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxInt32 dataHandlerDateTimeBase::getSignedLong(const imbxUint32 index) const
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerDateTimeBase::getSignedLong");

	imbxInt32 year, month, day, hour, minutes, seconds, nanoseconds, offsetHours, offsetMinutes;
	getDate(index, &year, &month, &day, &hour, &minutes, &seconds, &nanoseconds, &offsetHours, &offsetMinutes);

	tm timeStructure;
	timeStructure.tm_isdst= -1;
	timeStructure.tm_wday= 0;
	timeStructure.tm_yday= 0;
	timeStructure.tm_year = year;
	timeStructure.tm_mon = month-1;
	timeStructure.tm_mday = day;
	timeStructure.tm_hour = hour;
	timeStructure.tm_min = minutes;
	timeStructure.tm_sec = seconds;
	
	return (imbxInt32)mktime(&timeStructure);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Cast getSignedLong to unsigned long
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint32 dataHandlerDateTimeBase::getUnsignedLong(const imbxUint32 index) const
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerDateTimeBase::getUnsignedLong");

	return (imbxUint32)getSignedLong(index);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Cast getSignedLong to double
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
double dataHandlerDateTimeBase::getDouble(const imbxUint32 index) const
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerDateTimeBase::getDouble");

	return (double)getSignedLong(index);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Set the date as a signed long (from time_t)
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dataHandlerDateTimeBase::setSignedLong(const imbxUint32 index, const imbxInt32 value)
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerDateTimeBase::setSignedLong");

#ifdef PUNTOEXE_WINDOWS
	std::unique_ptr<tm> timeStructure(new tm);
	localtime_s(timeStructure.get(), ((time_t*)&value));
#else
	//tm* timeStructure = localtime((time_t*)&value);
	std::unique_ptr<tm> timeStructure(new tm);
	//localtime_r(timeStructure.get(), ((time_t*)&value));
	localtime_r(((time_t*)&value), timeStructure.get());
#endif
	imbxInt32 year = timeStructure->tm_year;
	imbxInt32 month = timeStructure->tm_mon + 1;
	imbxInt32 day = timeStructure->tm_mday;
	imbxInt32 hour = timeStructure->tm_hour;
	imbxInt32 minutes = timeStructure->tm_min;
	imbxInt32 seconds = timeStructure->tm_sec;
	setDate(index, year, month, day, hour, minutes, seconds, 0, 0, 0);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Set the date as a long (from time_t)
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dataHandlerDateTimeBase::setUnsignedLong(const imbxUint32 index, const imbxUint32 value)
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerDateTimeBase::setUnsignedLong");

	setSignedLong(index, (imbxInt32)value);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Set the date as a double (from time_t)
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dataHandlerDateTimeBase::setDouble(const imbxUint32 index, const double value)
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerDateTimeBase::setDouble");

	setSignedLong(index, (imbxInt32)value);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Return the separator
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
wchar_t dataHandlerDateTimeBase::getSeparator() const
{
	return 0;
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Parse a date string
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dataHandlerDateTimeBase::parseDate(
		std::wstring dateString,
		imbxInt32* pYear, 
		imbxInt32* pMonth, 
		imbxInt32* pDay) const
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerDateTimeBase::parseDate");

	if(dateString.size()<8)
		dateString.resize(8, L'0');

	std::wstring dateYear=dateString.substr(0, 4);
	std::wstring dateMonth=dateString.substr(4, 2);
	std::wstring dateDay=dateString.substr(6, 2);

	std::wistringstream yearStream(dateYear);
	yearStream >> (*pYear);

	std::wistringstream monthStream(dateMonth);
	monthStream >> (*pMonth);

	std::wistringstream dayStream(dateDay);
	dayStream >> (*pDay);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Build a date string
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
std::wstring dataHandlerDateTimeBase::buildDate(
		imbxUint32 year,
		imbxUint32 month,
		imbxUint32 day) const
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerDateTimeBase::buildDate");

        if((year > 9999) || (month < 1) || (month>12) || (day<1) || (day>31))
	{
		year = month = day = 0;
	}

	std::wostringstream dateStream;
	dateStream << std::setfill(L'0');
	dateStream << std::setw(4) << year;
	dateStream << std::setw(2) << month;
	dateStream << std::setw(2) << day;

	return dateStream.str();

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Parse a time string
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dataHandlerDateTimeBase::parseTime(
		std::wstring timeString,
		imbxInt32* pHour, 
		imbxInt32* pMinutes,
		imbxInt32* pSeconds,
		imbxInt32* pNanoseconds,
		imbxInt32* pOffsetHours,
		imbxInt32* pOffsetMinutes) const
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerDateTimeBase::parseTime");

	if(timeString.size() < 6)
	{
		timeString.resize(6, L'0');
	}
	if(timeString.size() < 7)
	{
		timeString += L'.';
	}
	if(timeString.size() < 13)
	{
		timeString.resize(13, L'0');
	}
	if(timeString.size() < 14)
	{
		timeString += L'+';
	}
	if(timeString.size() < 18)
	{
		timeString.resize(18, L'0');
	}
	
	std::wstring timeHour = timeString.substr(0, 2);
	std::wstring timeMinutes = timeString.substr(2, 2);
	std::wstring timeSeconds = timeString.substr(4, 2);
	std::wstring timeNanoseconds = timeString.substr(7, 6);
	std::wstring timeOffsetHours = timeString.substr(13, 3);
	std::wstring timeOffsetMinutes = timeString.substr(16, 2);

	std::wistringstream hourStream(timeHour);
	hourStream >> (*pHour);

	std::wistringstream minutesStream(timeMinutes);
	minutesStream >> (*pMinutes);

	std::wistringstream secondsStream(timeSeconds);
	secondsStream >> (*pSeconds);

	std::wistringstream nanosecondsStream(timeNanoseconds);
	nanosecondsStream >> (*pNanoseconds);

	std::wistringstream offsetHoursStream(timeOffsetHours);
	offsetHoursStream >> (*pOffsetHours);

	std::wistringstream offsetMinutesStream(timeOffsetMinutes);
	offsetMinutesStream >> (*pOffsetMinutes);

	if(*pOffsetHours < 0)
	{
		*pOffsetMinutes= - *pOffsetMinutes;
	}

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Build the time string
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
std::wstring dataHandlerDateTimeBase::buildTime(
		imbxInt32 hour,
		imbxInt32 minutes,
		imbxInt32 seconds,
		imbxInt32 nanoseconds,
		imbxInt32 offsetHours,
		imbxInt32 offsetMinutes
		) const
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerDateTimeBase::buildTime");

	if(
		   (hour < 0)
		|| (hour >= 24)
		|| (minutes < 0)
		|| (minutes >= 60)
		|| (seconds <0)
		|| (seconds >= 60)
		|| (nanoseconds < 0)
		|| (nanoseconds > 999999)
		|| (offsetHours < -12)
		|| (offsetHours > 12)
		|| (offsetMinutes < -59)
		|| (offsetMinutes > 59))
	{
		hour = minutes = seconds = nanoseconds = offsetHours = offsetMinutes = 0;
	}

	bool bMinus=offsetHours < 0;

	std::wostringstream timeStream;
	timeStream << std::setfill(L'0');
	timeStream << std::setw(2) << hour;
	timeStream << std::setw(2) << minutes;
	timeStream << std::setw(2) << seconds;
	timeStream << std::setw(1) << L".";
	timeStream << std::setw(6) << nanoseconds;
	timeStream << std::setw(1) << (bMinus ? L"-" : L"+");
	timeStream << std::setw(2) << labs(offsetHours);
	timeStream << std::setw(2) << labs(offsetMinutes);
	
	return timeStream.str();

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Split several parts of a string
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void dataHandlerDateTimeBase::split(const std::wstring& timeString, const std::wstring& separators, std::vector<std::wstring> *pComponents) const
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerDateTimeBase::split");

        if(timeString.empty())
        {
            return;
        }

	for(size_t startPos(0), sepPos(timeString.find_first_of(separators)); /* empty */; sepPos = timeString.find_first_of(separators, startPos))
	{
                if(sepPos == timeString.npos)
                {
                    pComponents->push_back(timeString.substr(startPos));
                    break;
                }
		pComponents->push_back(timeString.substr(startPos, sepPos - startPos));
		startPos = ++sepPos;
                if(startPos == timeString.size())
                {
                    pComponents->push_back(L"");
                    break;
                }
	}

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Add the specified char to the left of a string until
//  its length reaches the desidered value
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
std::wstring dataHandlerDateTimeBase::padLeft(const std::wstring& source, const wchar_t fillChar, const size_t length) const
{
	PUNTOEXE_FUNCTION_START(L"dataHandlerDateTimeBase::padLeft");
        
        if(source.size() >= length)
        {
            return source;
        }

        std::wstring paddedString(length - source.size(), fillChar);
        paddedString += source;

	return paddedString;

	PUNTOEXE_FUNCTION_END();
}

} // namespace handlers

} // namespace imebra

} // namespace puntoexe



