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

/*! \file charsetsList.h
    \brief Declaration of the the base class for the classes that need to be
            aware of the Dicom charsets.

*/

#if !defined(imebraCharsetsList_DE3F98A9_664E_47c0_A29B_B681F9AEB118__INCLUDED_)
#define imebraCharsetsList_DE3F98A9_664E_47c0_A29B_B681F9AEB118__INCLUDED_

#include <stdexcept>
#include <string>
#include <list>

namespace puntoexe
{

namespace imebra
{

/// \addtogroup group_dataset
///
/// @{


/// \name charsetsList
/// \brief The classes used to convert between different
///         charsets are declared in this namespace.
///////////////////////////////////////////////////////////
namespace charsetsList
{

/// \typedef std::list<std::wstring> tCharsetsList
/// \brief Defines a list of widechar strings.
///
/// It is used to set or retrieve a list of charsets
///
///////////////////////////////////////////////////////////
typedef std::list<std::wstring> tCharsetsList;

void updateCharsets(const tCharsetsList* pCharsetsList, tCharsetsList* pDestinationCharsetsList);
void copyCharsets(const tCharsetsList* pSourceCharsetsList, tCharsetsList* pDestinationCharsetsList);

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief This is the base class for the exceptions thrown
///         by the class charsetsList.
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class charsetsListException: public std::runtime_error
{
public:
	charsetsListException(std::string message): std::runtime_error(message){}
};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief This exception is thrown when a conversion from
///         an unicode string causes the dicom dataSet
///         to change its default charset.
///
/// For instace, the default charset is ISO IR 6 but a
///  value written by the application in one tag causes
///  the default charset to switch to ISO 2022 IR 100.
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class charsetListExceptionDiffDefault: public charsetsListException
{
public:
	charsetListExceptionDiffDefault(std::string message): charsetsListException(message){}
};

} // namespace charsetsList

/// @}

} // namespace imebra

} // namespace puntoexe

#endif // !defined(imebraCharsetsList_DE3F98A9_664E_47c0_A29B_B681F9AEB118__INCLUDED_)

