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

/*! \file dataHandlerDate.h
    \brief Declaration of the data handler able to handle the dicom tags
	        of type "DA" (date).

*/

#if !defined(imebraDataHandlerDate_BAA5E237_A37C_40bc_96EF_460B2D53DC12__INCLUDED_)
#define imebraDataHandlerDate_BAA5E237_A37C_40bc_96EF_460B2D53DC12__INCLUDED_

#include "dataHandlerDateTimeBase.h"


///////////////////////////////////////////////////////////
//
// Everything is in the namespace puntoexe::imebra
//
///////////////////////////////////////////////////////////
namespace puntoexe
{

namespace imebra
{

namespace handlers
{

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief This %data handler is returned by the class
///         buffer when the application wants to deal
///         with a dicom tag of type "DA" (date)
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class dataHandlerDate : public dataHandlerDateTimeBase
{
public:
	virtual imbxUint32 getUnitSize() const;

	virtual void getDate(const imbxUint32 index,
		imbxInt32* pYear, 
		imbxInt32* pMonth, 
		imbxInt32* pDay, 
		imbxInt32* pHour, 
		imbxInt32* pMinutes,
		imbxInt32* pSeconds,
		imbxInt32* pNanoseconds,
		imbxInt32* pOffsetHours,
		imbxInt32* pOffsetMinutes) const;

	virtual void setDate(const imbxUint32 index,
		imbxInt32 year, 
		imbxInt32 month, 
		imbxInt32 day, 
		imbxInt32 hour, 
		imbxInt32 minutes,
		imbxInt32 seconds,
		imbxInt32 nanoseconds,
		imbxInt32 offsetHours,
		imbxInt32 offsetMinutes);

	/// \brief Return a string representing the date stored in 
	///         the buffer.
	///
	/// The returned string has the format: "YYYY-MM-DD"
	///  where:
	///  - YYYY is the year
	///  - MM is the month
	///  - DD is the day
	///
	/// @param index the zero based index of the date to
	///         retrieve
	/// @return a string representing the date stored in the
	///          buffer
	///
	///////////////////////////////////////////////////////////
	virtual std::wstring getUnicodeString(const imbxUint32 index) const;

	/// \brief Set the date from a string.
	///
	/// The string must have the format: "YYYY-MM-DD"
	///  where:
	///  - YYYY is the year
	///  - MM is the month
	///  - DD is the day
	///
	/// @param index the zero based index of the date to be
	///         set
	/// @param value the string representing the date to be set
	///
	///////////////////////////////////////////////////////////
	virtual void setUnicodeString(const imbxUint32 index, const std::wstring& value);

	void parseBuffer(const ptr<memory>& memoryBuffer);

protected:
	virtual imbxUint32 maxSize() const;
};

} // namespace handlers

} // namespace imebra

} // namespace puntoexe

#endif // !defined(imebraDataHandlerDate_BAA5E237_A37C_40bc_96EF_460B2D53DC12__INCLUDED_)
