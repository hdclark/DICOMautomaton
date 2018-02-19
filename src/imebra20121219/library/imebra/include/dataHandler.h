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

/*! \file dataHandler.h
    \brief Declaration of the base class used by all the data handlers.

*/

#if !defined(imebraDataHandler_6F85D344_DEF8_468d_BF73_AC5BB17FD22A__INCLUDED_)
#define imebraDataHandler_6F85D344_DEF8_468d_BF73_AC5BB17FD22A__INCLUDED_

#include "../../base/include/baseObject.h"
#include "buffer.h"
#include "charsetsList.h"


///////////////////////////////////////////////////////////
//
// Everything is in the namespace puntoexe::imebra
//
///////////////////////////////////////////////////////////
namespace puntoexe
{

namespace imebra
{

	class transaction; // is a friend

/// \namespace handlers
/// \brief All the data handlers returned by the class
///         buffer are defined in this namespace
///
///////////////////////////////////////////////////////////
namespace handlers
{

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief This is the base class for all the imebra data
///         handlers.
///        A data handler allows to read/write the data
///         stored in a \ref puntoexe::imebra::buffer 
///         object without worrying about the %data format.
///
/// Data handlers work on a local copy of the buffer,
///  then they don't need to worry about multithreading
///  accesses.
///
/// Also, once a dataHandler has been obtained from
///  a \ref buffer, it cannot be shared between threads
///  and it doesn't provide any multithread-safe mechanism,
///  except for its destructor which copies the local
///  buffer back to the original one (only for the writable
///  handlers).
///
/// Data handlers are also used to access to the
///  decompressed image's pixels (see image and 
///  handlers::dataHandlerNumericBase).
///
/// To obtain a data handler your application has to
///  call buffer::getDataHandler() or 
///  image::getDataHandler().
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class dataHandler : public baseObject
{
	// buffer & transaction are friend of this class
	///////////////////////////////////////////////////////////
	friend class puntoexe::imebra::buffer;
	friend class puntoexe::imebra::transaction;

public:
	// Contructor
	///////////////////////////////////////////////////////////
	dataHandler(): m_bCommitted(false){}

	// The data handler is disconnected
	///////////////////////////////////////////////////////////
	bool preDelete() override;

	/// \brief In a writing handler copies back the modified
	///         data to the buffer.
	///
	/// Is not necessary to call this function directly because
	///  it is called by the handler's destructor, which copy
	///  any modification back to the buffer and finalize it.
	///
	/// The operation must be finalized by a call to commit(),
	///  or will be finalized by the destructor unless a call
	///  to abort() happen.
	///
	///////////////////////////////////////////////////////////
	void copyBack();
	
	/// \brief Finalize the copy of the data from the handler
	///         to the buffer.
	///
	/// Is not necessary to call this function directly because
	///  it is called by the handler's destructor, which copy
	///  any modification back to the buffer and then finalize
	///  it.
	///
	///////////////////////////////////////////////////////////
	void commit();

	/// \brief Discard all the changes made on a writing
	///         handler.
	///
	/// The function also switches the write flag to false,
	///  so it also prevent further changes from being
	///  committed into the buffer.
	///
	///////////////////////////////////////////////////////////
	void abort();

public:
	///////////////////////////////////////////////////////////
	/// \name Data pointer
	///
	/// The following functions set the value of the internal
	///  pointer that references the %data element read/written
	///  by the reading/writing functions.
	///
	///////////////////////////////////////////////////////////
	//@{

	/// \brief Returns true if the specified index points to
	///        a valid element in the buffer.
	///
	/// If the index is out of the valid range, then this
	///  method returns false.
	///
	/// @param index the index to be tested
	/// @return true if the index is valid, false if it is out
	///          of range
	///
	///////////////////////////////////////////////////////////
	virtual bool pointerIsValid(const imbxUint32 index) const=0;

	//@}


	///////////////////////////////////////////////////////////
	/// \name Buffer and elements size
	///
	///////////////////////////////////////////////////////////
	//@{

	/// \brief Set the buffer's size, in elements.
	///
	/// The function resize the data handler's local buffer
	///  to the requested number of elements.
	///
	/// @param elementsNumber the requested buffer's size,
	///                        in data elements
	///
	///////////////////////////////////////////////////////////
	virtual void setSize(const imbxUint32 elementsNumber) =0;

	/// \brief Retrieve the data handler's local buffer buffer
	///         size (in elements).
	///
	/// @return the buffer's size in elements
	///
	///////////////////////////////////////////////////////////
	virtual imbxUint32 getSize() const=0;

	/// \brief Returns a single element's size in bytes.
	///
	/// If the element doesn't have a fixed size, then
	///  this function return 0.
	///
	/// @return the element's size in bytes, or zero if the
	///         element doesn't have a fixed size
	///
	///////////////////////////////////////////////////////////
	virtual imbxUint32 getUnitSize() const=0;

	//@}
	

	///////////////////////////////////////////////////////////
	/// \internal
	/// \name Copy the local buffer from/to the buffer
	///
	///////////////////////////////////////////////////////////
	
	/// \internal
	/// \brief This function copies the %data from the 
	///         \ref buffer into the local %buffer
	///
	/// @param memoryBuffer the \ref memory managed by the 
	///                      \ref buffer
	///
	///////////////////////////////////////////////////////////
	virtual void parseBuffer(const ptr<memory>& memoryBuffer)=0;
	
	/// \internal
	/// \brief This function copies the %data from the 
	///         \ref buffer into the local %buffer
	///
	/// @param pBuffer a pointer to the %memory that stores the
	///                 data to be copied
	/// @param bufferLength the number of bytes stored in
	///                 pBuffer
	///
	///////////////////////////////////////////////////////////
	void parseBuffer(const imbxUint8* pBuffer, const imbxUint32 bufferLength);

	/// \internal
	/// \brief Copies the local %buffer into the original
	///         \ref buffer object.
	///
	/// @param memoryBuffer the \ref memory managed by the 
	///                      \ref buffer
	///
	///////////////////////////////////////////////////////////
	virtual void buildBuffer(const ptr<memory>& memoryBuffer)=0;

	/// \internal
	/// \brief Defines the charsets used in the string
	///
	/// @param pCharsetsList a list of dicom charsets
	///
	///////////////////////////////////////////////////////////
	virtual void setCharsetsList(charsetsList::tCharsetsList* pCharsetsList);

	/// \internal
	/// \brief Retrieve the charsets used in the string.
	///
	/// @param pCharsetsList a list that will be filled with the
	///                      dicom charsets used in the string
	///
	///////////////////////////////////////////////////////////
	virtual void getCharsetsList(charsetsList::tCharsetsList* pCharsetsList) const;

	//@}


	///////////////////////////////////////////////////////////
	/// \name Attributes
	///
	///////////////////////////////////////////////////////////
	//@{

	/// \brief Get the dicom data type managed by this handler.
	///
	/// The dicom data type is formed by 2 uppercase chars,
	///  as described by the dicom standard.
	/// See \ref buffer for further information.
	///
	/// @return the data handler's dicom data type
	///
	///////////////////////////////////////////////////////////
	std::string getDataType() const;

	/// \brief Return the byte that this handler uses to fill
	///         its content to make its size even.
	///
	/// @return the byte used to make the content's size even
	///
	///////////////////////////////////////////////////////////
	virtual imbxUint8 getPaddingByte() const;

	//@}


	///////////////////////////////////////////////////////////
	/// \name Reading/writing functions
	///
	///////////////////////////////////////////////////////////
	//@{

	/// \brief Retrieve the buffer's element referenced by the
	///         zero-based index specified in the parameter and
	///         returns it as a signed long value.
	///
	/// Returns 0 if the specified index is out of range.
	/// You can check the validity of the index by using the
	///  function pointerIsValid().
	///
	/// @param index   the zero base index of the buffer's
	///                 element to retrieve
	/// @return the value of the data element referenced by
	///          the index, transformed into a signed long, or
	///          0 if the index is out of range
	///
	///////////////////////////////////////////////////////////
	virtual imbxInt32 getSignedLong(const imbxUint32 index) const=0;

	/// \brief Retrieve the buffer's element referenced by the
	///         zero-based index specified in the parameter and
	///         returns it as an unsigned long value.
	///
	/// Returns 0 if the specified index is out of range.
	/// You can check the validity of the index by using the
	///  function pointerIsValid().
	///
	/// @param index   the zero base index of the buffer's
	///                 element to retrieve
	/// @return the value of the data element referenced by
	///          the index, transformed into an unsigned long,
	///          or 0 if the index is out of range
	///
	///////////////////////////////////////////////////////////
	virtual imbxUint32 getUnsignedLong(const imbxUint32 index) const =0;

	/// \brief Retrieve the buffer's element referenced by the
	///         zero-based index specified in the parameter and
	///         returns it as a double floating point value.
	///
	/// Returns 0 if the specified index is out of range.
	/// You can check the validity of the index by using the
	///  function pointerIsValid().
	///
	/// @param index   the zero base index of the buffer's
	///                 element to retrieve
	/// @return the value of the data element referenced by
	///          the index, transformed into a double floating
	///          point, or 0 if the index is out of range
	///
	///////////////////////////////////////////////////////////
	virtual double getDouble(const imbxUint32 index) const=0;

	/// \brief Retrieve the buffer's element referenced by the
	///         zero-based index specified in the parameter and
	///         returns it as a string value.
	///
	/// Returns 0 if the specified index is out of range.
	/// You can check the validity of the index by using the
	///  function pointerIsValid().
	///
	/// @param index   the zero base index of the buffer's
	///                 element to retrieve
	/// @return the value of the data element referenced by
	///          the index, transformed into a string, or
	///          0 if the index is out of range
	///
	///////////////////////////////////////////////////////////
	virtual std::string getString(const imbxUint32 index) const= 0;

	/// \brief Retrieve the buffer's element referenced by the
	///         zero-based index specified in the parameter and
	///         returns it as an unicode string value.
	///
	/// Returns 0 if the specified index is out of range.
	/// You can check the validity of the index by using the
	///  function pointerIsValid().
	///
	/// @param index   the zero base index of the buffer's
	///                 element to retrieve
	/// @return the value of the data element referenced by
	///          the index, transformed into an unicode string,
	///          or 0 if the index is out of range
	///
	///////////////////////////////////////////////////////////
	virtual std::wstring getUnicodeString(const imbxUint32 index) const = 0;

	/// \brief Retrieve the buffer's element referenced by the
	///         zero-based index specified in the parameter and
	///         returns it as a date/time value.
	///
	/// Returns all zeros if the specified index is out of
	///  range.
	/// You can check the validity of the index by using the
	///  function pointerIsValid().
	///
	/// @param index   the zero base index of the buffer's
	///                 element to retrieve
	/// @param pYear   a pointer to a value that will be filled
	///                 with the UTC date's year
	/// @param pMonth  a pointer to a value that will be filled
	///                 with the UTC date's month
	/// @param pDay    a pointer to a value that will be filled
	///                 with the UTC date's day of the month
	/// @param pHour   a pointer to a value that will be filled
	///                 with the UTC hour
	/// @param pMinutes a pointer to a value that will be 
	///                 filled with the UTC minutes
	/// @param pSeconds a pointer to a value that will be 
	///                 filled with the UTC seconds
	/// @param pNanoseconds a pointer to a value that will be 
	///                 filled with the UTC nanosecods
	/// @param pOffsetHours a pointer to a value that will be 
	///                 filled with the difference between the
	///                 date time zone and the UTC time zone
	/// @param pOffsetMinutes a pointer to a value that will be 
	///                 filled with the difference between the
	///                 date time zone and the UTC time zone
	///
	///////////////////////////////////////////////////////////
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

	/// \brief Set the buffer's element referenced by the
	///         zero-based index specified in the parameter
	///         to a date/time value.
	///
	/// Does nothing if the specified index is out of range
	/// You can check the validity of the index by using the
	///  function pointerIsValid(), you can resize the buffer
	///  by using the function setSize().
	///
	/// @param index   the zero base index of the buffer's
	///                 element to be set
	/// @param year   the UTC date's year
	/// @param month  the UTC date's month
	/// @param day    the UTC date's day of the month
	/// @param hour   the UTC hour
	/// @param minutes the UTC minutes
	/// @param seconds the UTC seconds
	/// @param nanoseconds the UTC nanosecods
	/// @param offsetHours the difference between the date time 
	///                zone and the UTC time zone
	/// @param offsetMinutes the difference between the date
	///                time zone and the UTC time zone
	///
	///////////////////////////////////////////////////////////
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

	/// \brief Set the buffer's element referenced by the
	///         zero-based index specified in the parameter
	///         to a signed long value.
	///
	/// Does nothing if the specified index is out of range
	/// You can check the validity of the index by using the
	///  function pointerIsValid(), you can resize the buffer
	///  by using the function setSize().
	///
	/// @param index   the zero base index of the buffer's
	///                 element to be set
	/// @param value the value to write into the active
	///				  %data element.
	///
	///////////////////////////////////////////////////////////
	virtual void setSignedLong(const imbxUint32 index, const imbxInt32 value) =0;

	/// \brief Set the buffer's element referenced by the
	///         zero-based index specified in the parameter
	///         to an unsigned long value.
	///
	/// Does nothing if the specified index is out of range
	/// You can check the validity of the index by using the
	///  function pointerIsValid(), you can resize the buffer
	///  by using the function setSize().
	///
	/// @param index   the zero base index of the buffer's
	///                 element to be set
	/// @param value the value to write into the active
	///				  %data element.
	///
	///////////////////////////////////////////////////////////
	virtual void setUnsignedLong(const imbxUint32 index, const imbxUint32 value) =0;

	/// \brief Set the buffer's element referenced by the
	///         zero-based index specified in the parameter
	///         to a double floating point value.
	///
	/// Does nothing if the specified index is out of range
	/// You can check the validity of the index by using the
	///  function pointerIsValid(), you can resize the buffer
	///  by using the function setSize().
	///
	/// @param index   the zero base index of the buffer's
	///                 element to be set
	/// @param value the value to write into the active
	///				  %data element.
	///
	///////////////////////////////////////////////////////////
	virtual void setDouble(const imbxUint32 index, const double value) =0;

	/// \brief Set the buffer's element referenced by the
	///         zero-based index specified in the parameter
	///         to a string value. See also setUnicodeString().
	///
	/// Does nothing if the specified index is out of range
	/// You can check the validity of the index by using the
	///  function pointerIsValid(), you can resize the buffer
	///  by using the function setSize().
	///
	/// @param index   the zero base index of the buffer's
	///                 element to be set
	/// @param value the value to write into the active
	///				  %data element.
	///////////////////////////////////////////////////////////
	virtual void setString(const imbxUint32 index, const std::string& value) =0;

	/// \brief Set the buffer's element referenced by the
	///         zero-based index specified in the parameter
	///         to a string value. See also seString().
	///
	/// Does nothing if the specified index is out of range
	/// You can check the validity of the index by using the
	///  function pointerIsValid(), you can resize the buffer
	///  by using the function setSize().
	///
	/// @param index   the zero base index of the buffer's
	///                 element to be set
	/// @param value the value to write into the active
	///				  %data element.
	///////////////////////////////////////////////////////////
	virtual void setUnicodeString(const imbxUint32 index, const std::wstring& value) =0;

	//@}


protected:
	// true if the buffer has been committed
	///////////////////////////////////////////////////////////
	bool m_bCommitted;

	// Pointer to the connected buffer
	///////////////////////////////////////////////////////////
	ptr<buffer> m_buffer;

	std::string m_bufferType;

	charsetsList::tCharsetsList m_charsetsList;
};

} // namespace handlers

} // namespace imebra

} // namespace puntoexe

#endif // !defined(imebraDataHandler_6F85D344_DEF8_468d_BF73_AC5BB17FD22A__INCLUDED_)
