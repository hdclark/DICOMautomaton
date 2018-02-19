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

/*! \file nullStream.h
    \brief Declaration of the nullStream class.

*/

#if !defined(imebraNullStream_3146DA5A_5276_4804_B9AB_A3D54C6B123A__INCLUDED_)
#define imebraNullStream_3146DA5A_5276_4804_B9AB_A3D54C6B123A__INCLUDED_

#include "baseStream.h"

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
///////////////////////////////////////////////////////////
/// \brief This class derives from the baseStream 
///         class and implements a null stream.
///
/// The null stream doesn't write or read anything: it's
///  just used to update the position counter.
/// The null stream can be used to calculate the tags
///  offsets before they are written to the definitive
///  stream.
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class nullStream : public baseStream
{

public:
	///////////////////////////////////////////////////////////
	//
	// Virtual stream's functions
	//
	///////////////////////////////////////////////////////////
	void write(imbxUint32, const imbxUint8*, imbxUint32) override{}
	imbxUint32 read(imbxUint32 , imbxUint8* , imbxUint32 ) override{return 0;}
};

///@}

} // namespace puntoexe

#endif // !defined(imebraNullStream_3146DA5A_5276_4804_B9AB_A3D54C6B123A__INCLUDED_)
