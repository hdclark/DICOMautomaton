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

/*! \file dataHandlerStringUnicode.h
    \brief Declaration of the base class used by the string handlers that need to work
	        with different charsets.

*/

#if !defined(imebraDataHandlerStringUnicode_367AAE47_6FD7_4107_AB5B_25A355C5CB6E__INCLUDED_)
#define imebraDataHandlerStringUnicode_367AAE47_6FD7_4107_AB5B_25A355C5CB6E__INCLUDED_

#include "../../base/include/charsetConversion.h"
#include "dataHandlerString.h"
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

namespace handlers
{

struct dicomCharsetInformation
{
	dicomCharsetInformation(const wchar_t* dicomName, const char* escapeSequence, const char* isoRegistration):
		m_dicomName(dicomName), m_escapeSequence(escapeSequence), m_isoRegistration(isoRegistration){}
	const wchar_t* m_dicomName;
	std::string m_escapeSequence;
	std::string m_isoRegistration;
};

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief This is the base class for all the data handlers
///         that manage strings that need to be converted
///         from different unicode charsets
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class dataHandlerStringUnicode : public dataHandlerString
{
public:
	/// \internal
	/// \brief Defines the charset used by the strings encoded
	///         in the tag.
	///
	/// @param pCharsetsList the list of dicom charsets used by
	///                      the string
	/// 
	///////////////////////////////////////////////////////////
	void setCharsetsList(charsetsList::tCharsetsList* pCharsetsList) override;
	
	/// \internal
	/// \brief Retrieve the charset used in the encoded string
	///
	/// @param pCharsetsList a list that will be filled with the
	///                      dicom charsets used in the string
	///
	///////////////////////////////////////////////////////////
	void getCharsetsList(charsetsList::tCharsetsList* pCharsetsList) const override;

protected:
	// Convert a string to unicode, using the dicom charsets
	///////////////////////////////////////////////////////////
	std::wstring convertToUnicode(const std::string& value) const override;

	// Convert a string from unicode, using the dicom charsets
	///////////////////////////////////////////////////////////
	std::string convertFromUnicode(const std::wstring& value, charsetsList::tCharsetsList* pCharsetsList) const override;

	charsetConversion m_charsetConversion;
	charsetConversion m_localeCharsetConversion;

	dicomCharsetInformation* getCharsetInfo(const std::wstring& dicomName) const;
};

class dataHandlerStringUnicodeException: public std::runtime_error
{
public:
	dataHandlerStringUnicodeException(const std::string& message): std::runtime_error(message){}
};

class dataHandlerStringUnicodeExceptionUnknownCharset: public dataHandlerStringUnicodeException
{
public:
	dataHandlerStringUnicodeExceptionUnknownCharset(const std::string& message): dataHandlerStringUnicodeException(message){}
};

} // namespace handlers

} // namespace imebra

} // namespace puntoexe

#endif // !defined(imebraDataHandlerStringUnicode_367AAE47_6FD7_4107_AB5B_25A355C5CB6E__INCLUDED_)
