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

/*! \file baseStream.h
    \brief Declaration of the the base class for the streams (memory, file, ...)
            used by the puntoexe library.

*/

#if !defined(imebraBaseStream_3146DA5A_5276_4804_B9AB_A3D54C6B123A__INCLUDED_)
#define imebraBaseStream_3146DA5A_5276_4804_B9AB_A3D54C6B123A__INCLUDED_

#include "baseObject.h"
#include "../include/exception.h"
#include <vector>
#include <map>
#include <stdexcept>

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
/// \brief This class represents a stream.
///
/// Specialized classes derived from this class can
///  read/write from/to files stored on the computer's
///  disks, on the network or in memory.
///
/// The application can read or write into the stream
///  by using the streamReader or the streamWriter.
///
/// While this class can be used across several threads,
///  the streamReader and the streamWriter can be used in
///  one thread only. This is not a big deal, since one
///  stream can be connected to several streamReaders and
///  streamWriters.
///
/// The library supplies two specialized streams derived
///  from this class:
/// - puntoexe::stream (used to read or write into physical
///    files)
/// - puntoexe::memoryStream (used to read or write into
///    puntoexe::memory objects)
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class baseStream : public baseObject
{

public:
	/// \brief Writes raw data into the stream.
	///
	/// The function is multithreading-safe and is called by
	///  the streamWriter class when its buffer has to be
	///  flushed.
	///
	/// @param startPosition  the position in the file where
	///                        the data has to be written
	/// @param pBuffer        pointer to the data that has to
	///                        be written
	/// @param bufferLength   number of bytes in the data 
	///                        buffer that has to be written
	///
	///////////////////////////////////////////////////////////
	virtual void write(imbxUint32 startPosition, const imbxUint8* pBuffer, imbxUint32 bufferLength) = 0;
	
	/// \brief Read raw data from the stream.
	///
	/// The function is multithreading-safe and is called by
	///  the streamReader class when its buffer has to be
	///  refilled.
	///
	/// @param startPosition  the position in the file from
	///                        which the data has to be read
	/// @param pBuffer        a pointer to the memory where the
	///                        read data has to be placed
	/// @param bufferLength   the number of bytes to read from
	///                        the file
	/// @return the number of bytes read from the file. When
	///          it is 0 then the end of the file has been
	///          reached
	///
	///////////////////////////////////////////////////////////
	virtual imbxUint32 read(imbxUint32 startPosition, imbxUint8* pBuffer, imbxUint32 bufferLength) = 0;
};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief The base exception for all the exceptions
///         thrown by the function in baseStream.
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class streamException: public std::runtime_error
{
public:
	streamException(const std::string& message): std::runtime_error(message){}
};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief Exception thrown when the stream cannot be
///         open.
///
///////////////////////////////////////////////////////////
class streamExceptionOpen : public streamException
{
public:
	streamExceptionOpen(const std::string& message): streamException(message){}
};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief Exception thrown when there is an error during
///         the read phase.
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class streamExceptionRead : public streamException
{
public:
	streamExceptionRead(const std::string& message): streamException(message){}
};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief Exception thrown when there is an error during
///         the write phase.
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class streamExceptionWrite : public streamException
{
public:
	streamExceptionWrite(const std::string& message): streamException(message){}
};


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief Exception thrown when there are problems during
///         the closure of the stream.
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class streamExceptionClose : public streamException
{
public:
	streamExceptionClose(const std::string& message): streamException(message){}
};

///@}

} // namespace puntoexe


#endif // !defined(imebraBaseStream_3146DA5A_5276_4804_B9AB_A3D54C6B123A__INCLUDED_)
