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

/*! \file charsetConversion.h
    \brief Declaration of the class used to convert a string between different
	        charsets.

The class hides the platform specific implementations and supplies a common
 interface for the charsets translations.

*/

#if !defined(imebraCharsetConversion_3146DA5A_5276_4804_B9AB_A3D54C6B123A__INCLUDED_)
#define imebraCharsetConversion_3146DA5A_5276_4804_B9AB_A3D54C6B123A__INCLUDED_

#include "configuration.h"
#include "baseObject.h"
#include <string>
#include <stdexcept>

#if PUNTOEXE_POSIX
#define PUNTOEXE_USEICONV 1
#endif

#if defined(PUNTOEXE_USEICONV)
#include <iconv.h>
#include <errno.h>
#else
#include "windows.h"
#endif

///////////////////////////////////////////////////////////
//
// Everything is in the namespace puntoexe
//
///////////////////////////////////////////////////////////
namespace puntoexe
{

/// \addtogroup group_baseclasses
///
/// @{

///////////////////////////////////////////////////////////
/// \internal
/// \brief Stores the information related to a single
///         charset.
///
///////////////////////////////////////////////////////////
struct charsetInformation
{
	const char* m_isoRegistration;   ///< ISO name for the charset
	const char* m_iconvName;         ///< Name used by the iconv function
	unsigned long m_codePage;  ///< codePage used by Windows
	bool m_bZeroFlag;          ///< needs flags=0 in Windows
};

///////////////////////////////////////////////////////////
/// \internal
/// \brief This class converts a string from multibyte to
///         unicode and viceversa.
///
/// The class uses the function iconv on Posix systems and
///  the functions MultiByteToWideChars and
///  WideCharsToMultiByte on Windows systems.
///
///////////////////////////////////////////////////////////
class charsetConversion
{
public:
	charsetConversion();
	virtual ~charsetConversion();

	/// \brief Initialize the charsetConversion object.
	///
	/// This function must be called before any other
	///  function's method can be called.
	///
	/// @param tableName the ISO name of the charset that will
	///                   be used for the conversion
	///
	///////////////////////////////////////////////////////////
	void initialize(const std::string& tableName);

	/// \brief Retrieve the ISO name of the charset currently
	///         used for the conversion.
	///
	/// @return the ISO name of the active charset
	///
	///////////////////////////////////////////////////////////
	std::string getIsoCharset() const;

	/// \brief Transform a multibyte string into an unicode
	///         string using the charset declared with the
	///         method initialize().
	///
	/// initialize() must have been called before calling this
	///  method.
	///
	/// @param unicodeStr
	///
	///////////////////////////////////////////////////////////
	std::string fromUnicode(const std::wstring& unicodeString) const;

	/// \brief Transform a multibyte string into an unicode
	///         string using the charset declared with the
	///         method initialize().
	///
	/// initialize() must have been called before calling this
	///  method.
	///
	/// @param asciiString the multibyte string that will be
	///                     converted to unicode
	/// @return            the converted unicode string
	///
	///////////////////////////////////////////////////////////
	std::wstring toUnicode(const std::string& asciiString) const;

protected:
	void close();

	int findTable(const std::string& tableName) const;

	std::string m_isoCharset;

#if defined(PUNTOEXE_USEICONV)

#if defined(WIN32)
	static std::string myIconv(iconv_t context, const char* inputString, size_t inputStringLengthBytes);
#else
	static std::string myIconv(iconv_t context, char* inputString, size_t inputStringLengthBytes);
#endif
	iconv_t m_iconvToUnicode;
	iconv_t m_iconvFromUnicode;
#else
	unsigned long m_codePage;
	bool m_bZeroFlag;
#endif
};


///////////////////////////////////////////////////////////
/// \internal
/// \brief
///
/// Save the state of a charsetConversion object.
/// The saved state is restored by the destructor of the
///  class.
///
///////////////////////////////////////////////////////////
class saveCharsetConversionState
{
public:
	/// \brief Constructor. Save the state of the
	///         charsetConversion object specified in the
	///         parameter.
	///
	/// @param pConversion a pointer to the charsetConversion
	///                     object that need to be saved
	///
	///////////////////////////////////////////////////////////
	saveCharsetConversionState(charsetConversion* pConversion)
	{
		m_pConversion = pConversion;
		m_savedState = pConversion->getIsoCharset();
	}

	/// \brief Destructor. Restore the saved state to the
	///         charsetConversion object specified during
	///         the construction.
	///
	///////////////////////////////////////////////////////////
	virtual ~saveCharsetConversionState()
	{
		m_pConversion->initialize(m_savedState);
	}

protected:
	std::string m_savedState;
	charsetConversion* m_pConversion;
};


///////////////////////////////////////////////////////////
/// \brief Base class for the exceptions thrown by
///         charsetConversion.
///
///////////////////////////////////////////////////////////
class charsetConversionException: public std::runtime_error
{
public:
	charsetConversionException(const std::string& message): std::runtime_error(message){}
};


///////////////////////////////////////////////////////////
/// \brief Exception thrown when the requested charset
///         is not supported by the DICOM standard.
///
///////////////////////////////////////////////////////////
class charsetConversionExceptionNoTable: public charsetConversionException
{
public:
	charsetConversionExceptionNoTable(const std::string& message): charsetConversionException(message){}
};


///////////////////////////////////////////////////////////
/// \brief Exception thrown when the requested charset
///         is not supported by the system.
///
///////////////////////////////////////////////////////////
class charsetConversionExceptionNoSupportedTable: public charsetConversionException
{
public:
	charsetConversionExceptionNoSupportedTable(const std::string& message): charsetConversionException(message){}
};


///////////////////////////////////////////////////////////
/// \brief Exception thrown when the system doesn't have
///         a supported size for wchar_t
///
///////////////////////////////////////////////////////////
class charsetConversionExceptionUtfSizeNotSupported: public charsetConversionException
{
public:
	charsetConversionExceptionUtfSizeNotSupported(const std::string& message): charsetConversionException(message){}
};

///@}

} // namespace puntoexe



#endif // !defined(imebraCharsetConversion_3146DA5A_5276_4804_B9AB_A3D54C6B123A__INCLUDED_)
