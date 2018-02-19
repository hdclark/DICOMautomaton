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

/*! \file dataHandlerStringPN.h
    \brief Declaration of the class dataHandlerStringPN.

*/

#if !defined(imebraDataHandlerStringPN_367AAE47_6FD7_4107_AB5B_25A355C5CB6E__INCLUDED_)
#define imebraDataHandlerStringPN_367AAE47_6FD7_4107_AB5B_25A355C5CB6E__INCLUDED_

#include "dataHandlerStringUnicode.h"


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

/*!
\brief Handles the Dicom type "PN" (person name).

This class separates the component groups of the name.

The person name can be formed by three groups:
 one or more groups can be absent.
- the first components group contains a character
  representation of the person name
- the second components group contains an ideographic
  representation of the person name
- the third components group contains a phonetic
  representation of the patient name

Inside a components group, the name components
 (first name, middle name, surname, etc) must be
 separated by a ^.

This class doesn't insert or parse the ^ separator
 which must be inserted and handled by the calling
 application, but handles the = separator which
 separates the components groups.\n
This means that to read or set all the patient name
 you have to move the pointer to the internal element
 by using setPointer(), incPointer() or skip().

For instance, to set the name "Left^Arrow" both
 with a character and an ideographic representation you
 have to use the following code:

\code
myDataSet->getDataHandler(group, 0, tag, 0, true, "PN");
myDataSet->setSize(2);
myDataSet->setUnicodeString(L"Left^Arrow");
myDataSet->incPointer();
myDataSet->setUnicodeString(L"<-"); // :-)
\endcode

*/
class dataHandlerStringPN : public dataHandlerStringUnicode
{
public:
	imbxUint8 getPaddingByte() const override;

	imbxUint32 getUnitSize() const override;

protected:
	// Return the maximum string's length
	///////////////////////////////////////////////////////////
	imbxUint32 maxSize() const override;

	// Returns the separator =
	///////////////////////////////////////////////////////////
	wchar_t getSeparator() const override;
};

} // namespace handlers

} // namespace imebra

} // namespace puntoexe

#endif // !defined(imebraDataHandlerStringPN_367AAE47_6FD7_4107_AB5B_25A355C5CB6E__INCLUDED_)
