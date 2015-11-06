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

/*! \file dataHandlerString.h
    \brief Declaration of the base class used by the string handlers.

*/

#if !defined(imebraDataHandlerString_367AAE47_6FD7_4107_AB5B_25A355C5CB6E__INCLUDED_)
#define imebraDataHandlerString_367AAE47_6FD7_4107_AB5B_25A355C5CB6E__INCLUDED_

#include "dataHandler.h"
#include <vector>
#include <string>


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
/// \brief This is the base class for all the data handlers
///         that manage strings.
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class dataHandlerString : public dataHandler
{
public:
	// Returns true if the pointer is valid
	///////////////////////////////////////////////////////////
	virtual bool pointerIsValid(const imbxUint32 index) const;

	// Get the data element as a signed long
	///////////////////////////////////////////////////////////
	virtual imbxInt32 getSignedLong(const imbxUint32 index) const;

	// Get the data element as an unsigned long
	///////////////////////////////////////////////////////////
	virtual imbxUint32 getUnsignedLong(const imbxUint32 index) const;
	
	// Get the data element as a double
	///////////////////////////////////////////////////////////
	virtual double getDouble(const imbxUint32 index) const;

	// Get the data element as a string
	///////////////////////////////////////////////////////////
	virtual std::string getString(const imbxUint32 index) const;

	// Get the data element as an unicode string
	///////////////////////////////////////////////////////////
	virtual std::wstring getUnicodeString(const imbxUint32 index) const;

	// Retrieve the data element as a string
	///////////////////////////////////////////////////////////
	virtual imbxUint32 getSize() const;
	
	// Set the data element as a signed long
	///////////////////////////////////////////////////////////
	virtual void setSignedLong(const imbxUint32 index, const imbxInt32 value);
	
	// Set the data element as an unsigned long
	///////////////////////////////////////////////////////////
	virtual void setUnsignedLong(const imbxUint32 index, const imbxUint32 value);
	
	// Set the data element as a double
	///////////////////////////////////////////////////////////
	virtual void setDouble(const imbxUint32 index, const double value);
	
	// Set the data element as a string
	///////////////////////////////////////////////////////////
	virtual void setString(const imbxUint32 index, const std::string& value);

	// Set the data element as an unicode string
	///////////////////////////////////////////////////////////
	virtual void setUnicodeString(const imbxUint32 index, const std::wstring& value);
	
	// Set the buffer's size, in data elements
	///////////////////////////////////////////////////////////
	virtual void setSize(const imbxUint32 elementsNumber);

	// Parse the buffer
	///////////////////////////////////////////////////////////
	virtual void parseBuffer(const ptr<memory>& memoryBuffer);

	// Build the buffer
	///////////////////////////////////////////////////////////
	virtual void buildBuffer(const ptr<memory>& memoryBuffer);

protected:
	// Convert a string to unicode, without using the dicom 
	//  charsets
	///////////////////////////////////////////////////////////
	virtual std::wstring convertToUnicode(const std::string& value) const;

	// Convert a string from unicode, without using the dicom 
	//  charsets
	///////////////////////////////////////////////////////////
	virtual std::string convertFromUnicode(const std::wstring& value, charsetsList::tCharsetsList* pCharsetsList) const;

	// Return the maximum string's length
	///////////////////////////////////////////////////////////
	virtual imbxUint32 maxSize() const;

	// Return the separator
	///////////////////////////////////////////////////////////
	virtual wchar_t getSeparator() const;

	imbxUint32 m_elementNumber;

	std::vector<std::wstring> m_strings;

};


} // namespace handlers

} // namespace imebra

} // namespace puntoexe

#endif // !defined(imebraDataHandlerString_367AAE47_6FD7_4107_AB5B_25A355C5CB6E__INCLUDED_)
