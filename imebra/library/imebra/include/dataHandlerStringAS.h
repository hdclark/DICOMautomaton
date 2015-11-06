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

/*! \file dataHandlerStringAS.h
    \brief Declaration of the class dataHandlerStringAS.

*/

#if !defined(imebraDataHandlerStringAS_367AAE47_6FD7_4107_AB5B_25A355C5CB6E__INCLUDED_)
#define imebraDataHandlerStringAS_367AAE47_6FD7_4107_AB5B_25A355C5CB6E__INCLUDED_

#include "dataHandlerString.h"


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
/// \brief Handles the Dicom data type "AS" (age string)
///
/// The handler supplies two additional functions designed
///  to set/get the age (setAge() and getAge()) and 
///  overwrite the functions getSignedLong(), 
///  getUnsignedLong(), getDouble(), setSignedLong(),
///  setUnsignedLong(), setDouble() to make them work with
///  the years.
///
/// setDouble() and getDouble() work also with fraction
///  of a year, setting the age unit correctly (days, 
///  weeks, months or years).
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class dataHandlerStringAS : public dataHandlerString
{
public:
	/// \brief Used by setAge() and getAge() to specify the
	///         unit of the age value.
	///
	///////////////////////////////////////////////////////////
	enum tAgeUnit
	{
		days = L'D',   ///< the age value is in days
		weeks = L'W',  ///< the age value is in weeks
		months = L'M', ///< the age value is in months
		years = L'Y'   ///< the age value is in years
	};
	
	/// \brief Set the value of the age string and specify
	///         its unit.
	///
	/// @param index the zero based index of the age value to
	///               read
	/// @param age   the age to be written in the buffer
	/// @param unit  the units used for the parameter age
	///
	///////////////////////////////////////////////////////////
	virtual void setAge(const imbxUint32 index, const imbxUint32 age, const tAgeUnit unit);

	/// \brief Retrieve the age value and its unit from the
	///         buffer handled by this handler.
	///
	/// @param index the zero based index of the age value to
	///               modify
	/// @param pUnit a pointer to a tAgeUnit that will be
	///               filled with the unit information related
	///               to the returned value
	/// @return the age, expressed in the unit written in the
	///               location referenced by the parameter 
	///               pUnit
	///
	///////////////////////////////////////////////////////////
	virtual imbxUint32 getAge(const imbxUint32 index, tAgeUnit* pUnit) const;

	/// \brief Retrieve the age from the handled buffer.
	///
	/// The returned value is always expressed in years.
	///
	/// @param index the zero based index of the age value to
	///               read
	/// @return the age contained in the buffer converted into
	///          years
	///
	///////////////////////////////////////////////////////////
	virtual imbxInt32 getSignedLong(const imbxUint32 index) const;

	/// \brief Retrieve the age from the handled buffer.
	///
	/// The returned value is always expressed in years.
	///
	/// @param index the zero based index of the age value to
	///               read
	/// @return the age contained in the buffer converted into
	///          years
	///
	///////////////////////////////////////////////////////////
	virtual imbxUint32 getUnsignedLong(const imbxUint32 index) const;

	/// \brief Retrieve the age from the handled buffer.
	///
	/// The returned value is always expressed in years.
	/// The function can return fraction of a year if the
	///  age contained in the buffer is expressed in days,
	///  weeks or months.
	///
	/// @param index the zero based index of the age value to
	///               read
	/// @return the age contained in the buffer converted into
	///          years
	///
	///////////////////////////////////////////////////////////
	virtual double getDouble(const imbxUint32 index) const;

	/// \brief Write the specified age into the handled buffer.
	///
	/// @param index the zero based index of the age value to
	///               modify
	/// @param value the age to be written, in years
	///
	///////////////////////////////////////////////////////////
	virtual void setSignedLong(const imbxUint32 index, const imbxInt32 value);

	/// \brief Write the specified age into the handled buffer.
	///
	/// @param index the zero based index of the age value to
	///               modify
	/// @param value the age to be written, in years
	///
	///////////////////////////////////////////////////////////
	virtual void setUnsignedLong(const imbxUint32 index, const imbxUint32 value);

	/// \brief Write the specified age into the handled buffer.
	///
	/// If a fraction of a year is specified, then the function
	///  set the age in days, weeks or months according to
	///  the value of the fraction.
	///
	/// @param index the zero based index of the age value to
	///               modify
	/// @param value the age to be written, in years
	///
	///////////////////////////////////////////////////////////
	virtual void setDouble(const imbxUint32 index, const double value);


	virtual imbxUint8 getPaddingByte() const;

	virtual imbxUint32 getUnitSize() const;

protected:
	// Return the maximum string's length
	///////////////////////////////////////////////////////////
	virtual imbxUint32 maxSize() const;
};

} // namespace handlers

} // namespace imebra

} // namespace puntoexe

#endif // !defined(imebraDataHandlerStringAS_367AAE47_6FD7_4107_AB5B_25A355C5CB6E__INCLUDED_)
