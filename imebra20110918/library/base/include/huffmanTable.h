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

/*! \file huffmanTable.h
    \brief Declaration of the huffman codec

*/

#if !defined(imebraHuffmanTable_3146DA5A_5276_4804_B9AB_A3D54C6B123A__INCLUDED_)
#define imebraHuffmanTable_3146DA5A_5276_4804_B9AB_A3D54C6B123A__INCLUDED_

#include "baseObject.h"
#include <vector>
#include <map>

///////////////////////////////////////////////////////////
//
// Everything is in the namespace puntoexe
//
///////////////////////////////////////////////////////////
namespace puntoexe
{
class streamReader;
class streamWriter;

/// \addtogroup group_baseclasses
///
/// @{

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
/// \brief This class calculates the huffman table from
///         a set of values and their frequency, and
///         can read or write huffman codes from/to a
///         baseStream object.
///
///////////////////////////////////////////////////////////
class huffmanTable: public baseObject
{
public:
	///////////////////////////////////////////////////////////
	/// \name Initialization
	///
	///////////////////////////////////////////////////////////
	//@{

	/// \brief Construct the huffman table and specify the
	///         maximum numbed of bits used by the values to
	///         be encoded.
	///
	/// @param maxValueLength the maximum lenght, in bits, of
	///                        the value that must be encoded.
	///                       Please note that this is not
	///                        the length of the huffman values
	///
	///////////////////////////////////////////////////////////
	huffmanTable(imbxUint32 maxValueLength);

	/// \brief Reset the internal data of the huffmanTable
	///         class.
	///
	/// The function removes all the calculated tables.
	/// The I/O functions readHuffmanCode() and 
	///  writeHuffmanCode() will not work until the tables are
	///  calculated by calcHuffmanCodesLength and 
	///  calcHuffmanTables().
	///
	///////////////////////////////////////////////////////////
	void reset();

	//@}

	
	///////////////////////////////////////////////////////////
	/// \name Huffman table generation
	///
	/// First, call incValueFreq() the right number of times
	///  for each value that appears in the stream, then
	///  call calcHuffmanCodesLength() and then
	///  calcHuffmanTables().
	///
	///////////////////////////////////////////////////////////
	//@{

	/// \brief Increase the frequency of a value that will be
	///         huffman encoded.
	///
	/// This function must be called for each time a value
	///  appears in a stream. Values with an higher frequency
	///  will have a shorted huffman code.
	///
	/// After the function has been called the right amount of
	///  times for every value that must be encoded, call 
	///  calcHuffmanCodesLength() and then calcHuffmanTables().
	///
	/// @param value   the value for wich the frequency must
	///                 be increased.
	///                The number of bits that form the value
	///                 must be less or equal to the number
	///                 of bits specified in the constructor
	///                 huffmanTable()
	///
	///////////////////////////////////////////////////////////
	void incValueFreq(const imbxUint32 value);

	/// \brief Calculates the length of the huffman codes.
	///
	/// This function must be called after incValueFreq() has
	///  been called to set the frequency of the values to be
	///  encoded.
	///
	/// After this function calculates the codes length, call
	///  calcHuffmanTables() to calculate the huffman codes 
	///  used by readHuffmanCode() and writeHuffmanCode().
	///
	/// @param maxCodeLength the maximum length in bits of the
	///                       generated huffman codes.
	///
	///////////////////////////////////////////////////////////
	void calcHuffmanCodesLength(const imbxUint32 maxCodeLength);
	
	/// \brief Generates the huffman table used by 
	///         readHuffmanCode() and writeHuffmanCode().
	///
	/// This function need the codes length generated by
	///  calcHuffmanCodesLength(): it will not work if the
	///  code lengths are not available.
	///
	///////////////////////////////////////////////////////////
	void calcHuffmanTables();
	
	/// \brief Remove the code with the higher value and the
	///         longer code from the code lengths table.
	///
	/// This function is usefull when extra data has been
	///  inserted through incValueFreq() but must not generate
	///  an huffman code.
	///
	/// E.g.: the jpeg coded insert an extra value with low
	///       frequency to reserve the last generated huffman 
	///       code, so the reserver huffman code will never
	///       be generated
	///
	///////////////////////////////////////////////////////////
	void removeLastCode();

	//@}


	///////////////////////////////////////////////////////////
	/// \name Huffman I/O
	///
	///////////////////////////////////////////////////////////
	//@{
	
	/// \brief Read and decode an huffman code from the 
	///         specified stream.
	///
	/// The function throws a huffmanExceptionRead exception
	///  if the read code cannot be decoded.
	///
	/// @param pStream  a pointer to the stream reader used to
	///                  read the code
        /// @return the decoded value
	///
	///////////////////////////////////////////////////////////
	imbxUint32 readHuffmanCode(streamReader* pStream);

	/// \brief Write an huffman code to the specified stream.
	///
	/// The function throws a huffmanExceptionWrite exception
	///  if the specified value cannot be encoded.
	///
	/// @param code     the value to be encoded and written to
	///                  the stream
	/// @param pStream  a pointer to the stream writer used to
	///                  write the code
	///
	///////////////////////////////////////////////////////////
	void writeHuffmanCode(const imbxUint32 code, streamWriter* pStream);

	//@}

protected:
	class valueObject
	{
	public:
		valueObject():m_freq(0), m_codeLength(0), m_nextCode(-1){}
		valueObject(const valueObject& right):m_freq(right.m_freq), m_codeLength(right.m_codeLength), m_nextCode(right.m_nextCode){}
		imbxUint32 m_freq;
		imbxUint32 m_codeLength;
		imbxInt32 m_nextCode;
	};

	class freqValue
	{
	public:
		freqValue(imbxUint32 freq = 0, imbxUint32 value = 0):m_freq(freq), m_value(value){}
		freqValue(const freqValue& right):m_freq(right.m_freq), m_value(right.m_value){}

		imbxUint32 m_freq;
		imbxUint32 m_value;
	};
	struct freqValueCompare
	{
                bool operator()(const freqValue& left, const freqValue& right) const
		{
			return left.m_freq < right.m_freq || (left.m_freq == right.m_freq && left.m_value > right.m_value);
		}
	};

	class lengthValue
	{
	public:
		lengthValue(imbxUint32 length = 0, imbxUint32 value = 0):m_length(length), m_value(value){}
		lengthValue(const lengthValue& right):m_length(right.m_length), m_value(right.m_value){}

		imbxUint32 m_length;
		imbxUint32 m_value;
	};
	struct lengthValueCompare
	{
                bool operator()(const lengthValue& left, const lengthValue& right) const
		{
			return left.m_length < right.m_length || (left.m_length == right.m_length && left.m_value < right.m_value);
		}
	};

	imbxUint32 m_numValues;

	// Values' frequency
	std::vector<valueObject> m_valuesFreq;
	
public:
	// Used to calculate the huffman codes
	std::vector<imbxUint32> m_orderedValues;
	imbxUint32 m_valuesPerLength[128];
	imbxUint8 m_firstValidLength;
	imbxUint32 m_minValuePerLength[128];
	imbxUint32 m_maxValuePerLength[128];

	// Final huffman table
	std::vector<imbxUint32> m_valuesToHuffman;
	std::vector<imbxUint32> m_valuesToHuffmanLength;

};

class huffmanException: public std::runtime_error
{
public:
	huffmanException(const std::string& message): std::runtime_error(message){}
};

class huffmanExceptionRead : public huffmanException
{
public:
	huffmanExceptionRead(const std::string& message): huffmanException(message){}
};

class huffmanExceptionWrite : public huffmanException
{
public:
	huffmanExceptionWrite(const std::string& message): huffmanException(message){}
};

///@}

} // namespace puntoexe


#endif // !defined(imebraHuffmanTable_3146DA5A_5276_4804_B9AB_A3D54C6B123A__INCLUDED_)
