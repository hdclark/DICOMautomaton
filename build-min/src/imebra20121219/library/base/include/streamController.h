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

/*! \file streamController.h
    \brief Declaration of the the class used to control the streams.

*/

#if !defined(imebraStreamController_00B3C824_CD0D_4D99_8436_A41FCE9E4D6B__INCLUDED_)
#define imebraStreamController_00B3C824_CD0D_4D99_8436_A41FCE9E4D6B__INCLUDED_

#include "baseObject.h"
#include "baseStream.h"
#include <memory>

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
/// \brief This class represents a stream controller.
///        A stream controller can read or write data
///         from/to a stream.
///
/// Do not use this class directly: use
///  puntoexe::streamWriter or puntoexe::streamReader
///  instead.
///
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
class streamController : public baseObject
{

#if(!defined IMEBRA_STREAM_CONTROLLER_MEMORY_SIZE)
	#define IMEBRA_STREAM_CONTROLLER_MEMORY_SIZE 4096
#endif

public:
	/// \brief Construct the stream controller and connect it
	///         to a stream.
	///
	/// The stream controller can be used to control only a
	///  portion of the connected stream.
	///
	/// @param pControlledStream pointer to the controlled
	///                           stream
	/// @param virtualStart      position in the stream that
	///                           is considered as the position
	///                           0 by the stream controller
	/// @param virtualLength     the number of bytes in the
	///                           connected stream that the
	///                           controller will use.
	///                          An EOF will be issued if the
	///                           application tries to read
	///                           beyond the virtual length
	///
	///////////////////////////////////////////////////////////
	streamController(ptr<baseStream> pControlledStream, imbxUint32 virtualStart = 0, imbxUint32 virtualLength = 0);

	/// \brief Get the stream's position relative to the
	///         virtual start position specified in the
	///         constructor.
	///
	/// The position is measured in bytes from the beginning
	///  of the stream.
	///
	/// The position represents the byte in the stream that
	///  the next reading or writing function will use.
	///
	/// If the virtual start position has been set in the
	///  constructor, then the returned position is relative
	///  to the virtual start position.
	///
	/// @return the stream's read position, in bytes from the
	///                  beginning of the stream or the virtual
	///                  start position set in the constructor
	///
	///////////////////////////////////////////////////////////
	imbxUint32 position();

	/// \brief Return a pointer to the controlled stream.
	///
	/// @return a pointer to the controlled stream
	///
	///////////////////////////////////////////////////////////
	ptr<baseStream> getControlledStream();

	/// \brief Return the position in bytes from the beginning
	///         of the stream.
	///
	/// It acts like the function position(), but it doesn't
	///  adjust the position to the virtual stream position
	///  specified in the constructor.
	///
	/// @return the stream's read position, in bytes from the
	///                  beginning of the stream
	///
	///////////////////////////////////////////////////////////
	imbxUint32 getControlledStreamPosition();

	///////////////////////////////////////////////////////////
	/// \name Byte ordering
	///
	///////////////////////////////////////////////////////////
	//@{

	/// \brief Specify a byte ordering
	///
	///////////////////////////////////////////////////////////
	enum tByteOrdering
	{
		lowByteEndian=1,  ///< the byte ordering is low endian: the least significant bytes of a word are stored first
		highByteEndian=2  ///< the byte ordering is high endian: the most significant bytes of a word are stored first
	};

	/// \brief Adjust the pBuffer's content according to the
	///         specified byte ordering
	///
	/// If the specified byte ordering is different from the
	///  platform's byte ordering, then the content of
	///  pBuffer is modified to reflect the desidered byte
	///  ordering.
	///
	/// @param pBuffer  a pointer to the buffer that stores
	///                  the data to order
	/// @param wordLength the size, in bytes, of the elements
	///                  stored in the buffer pointed by
	///                  pBuffer.
	///                 E.g.: if the buffer stores a collection
	///                  of words, this parameter will be 2.
	///                 If the buffer stores a collection of
	///                  double words, then this parameter will
	///                  be 4.
	/// @param endianType the desidered byte ordering.
	///                 If it differs from the platform's byte
	///                  ordering, then the content of the
	///                  memory pointed by pBuffer will be
	///                  modified.
	/// @param words    The number of elements stored in the
	///                  buffer pointed by pBuffer.
	///                 This value represents the number of
	///                  element that will be byte ordered.
	///                 The total size of the buffer should be
	///                  equal to words*wordLength
	///
	///////////////////////////////////////////////////////////
	static void adjustEndian(imbxUint8* pBuffer, const imbxUint32 wordLength, const tByteOrdering endianType, const imbxUint32 words = 1);

	//@}

public:

	/// \brief true writeByte() must write all 0xff as
        ///         0xff, 0x00 anf readByte() as to convert all
        ///         0xff,0x00 to 0xff, as in jpeg streams.
	///
	///////////////////////////////////////////////////////////
	bool m_bJpegTags;


protected:
	/// \brief Stream controlled by the stream controller.
	///
	///////////////////////////////////////////////////////////
	ptr<baseStream> m_pControlledStream;

	/// \brief Used for buffered IO
	///
	///////////////////////////////////////////////////////////
	std::unique_ptr<imbxUint8[]> m_dataBuffer;

	/// \brief Byte in the stream that represents the byte 0
	///         in the stream controller.
	///
	///////////////////////////////////////////////////////////
	imbxUint32 m_virtualStart;

	/// \brief Max number of bytes that the stream controller
	///         can control in the controlled stream. An EOF
	///         signal will be raised when trying to read
	///         past the maximum length.
	///
	/// If this value is 0 then there are no limits on the
	///  maximum length.
	///
	///////////////////////////////////////////////////////////
	imbxUint32 m_virtualLength;

	imbxUint32 m_dataBufferStreamPosition;
	imbxUint8* m_pDataBufferStart;
	imbxUint8* m_pDataBufferCurrent;
	imbxUint8* m_pDataBufferEnd;
	imbxUint8* m_pDataBufferMaxEnd;
};

/// \brief Exception thrown when an attempt to read past
///         the end of the file is made.
///
///////////////////////////////////////////////////////////
class streamExceptionEOF : public streamException
{
public:
	streamExceptionEOF(std::string message): streamException(message){}
};

///@}

} // namespace puntoexe

#endif // !defined(imebraStreamController_00B3C824_CD0D_4D99_8436_A41FCE9E4D6B__INCLUDED_)

