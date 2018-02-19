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

/*! \file huffmanTable.cpp
    \brief Implementation of the huffman codec.

*/

#include "../include/exception.h"
#include "../include/huffmanTable.h"
#include "../include/streamReader.h"
#include "../include/streamWriter.h"

#include <list>
#include <cstring>

namespace puntoexe
{

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// huffmanTable
//
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Constructor
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
huffmanTable::huffmanTable(imbxUint32 maxValueLength)
{
	m_numValues=(1L<<(maxValueLength))+1L;

	reset();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Reset the class' attributes
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void huffmanTable::reset()
{
	PUNTOEXE_FUNCTION_START(L"huffmanTable::reset");

	m_valuesFreq.clear();
	m_valuesFreq.resize(m_numValues);

	m_orderedValues.resize(m_numValues);
	::memset(&(m_orderedValues[0]), 0, m_numValues*sizeof(m_orderedValues[0]));

	m_valuesToHuffman.resize(m_numValues);
	::memset(&(m_valuesToHuffman[0]), 0, m_numValues*sizeof(m_valuesToHuffman[0]));

	m_valuesToHuffmanLength.resize(m_numValues);
	::memset(&(m_valuesToHuffmanLength[0]), 0, m_numValues*sizeof(m_valuesToHuffmanLength[0]));

	::memset(m_valuesPerLength, 0, sizeof(m_valuesPerLength));
	m_firstValidLength = 0;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Increase the frequency of one value
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void huffmanTable::incValueFreq(const imbxUint32 value)
{
	++(m_valuesFreq[value].m_freq);
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Remove the last huffman code from the lengths table
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void huffmanTable::removeLastCode()
{
	// Find the number of codes
	///////////////////////////////////////////////////////////
	imbxUint32 codes = 0;
	imbxUint32 lastLength = 0;
	for(imbxUint32 scanLengths = 0; scanLengths < sizeof(m_valuesPerLength)/sizeof(m_valuesPerLength[0]); ++scanLengths)
	{
		if(m_valuesPerLength[scanLengths] == 0)
		{
			continue;
		}
		codes += m_valuesPerLength[scanLengths];
		lastLength = scanLengths;
	}
	if(lastLength == 0)
	{
		return;
	}
	m_valuesPerLength[lastLength]--;
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Calculate the codes length
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void huffmanTable::calcHuffmanCodesLength(const imbxUint32 maxCodeLength)
{
	PUNTOEXE_FUNCTION_START(L"huffmanTable::calcHuffmanCodesLength");

	// Order the values by their frequency
	typedef std::map<huffmanTable::freqValue, bool, huffmanTable::freqValueCompare> tFreqOrderedMap;
	tFreqOrderedMap freqOrderedValues;

	for(size_t scanValues = 0; scanValues < m_valuesFreq.size(); ++scanValues)
	{
		if(m_valuesFreq[scanValues].m_freq != 0)
		{
			huffmanTable::freqValue key(m_valuesFreq[scanValues].m_freq, (imbxUint32)scanValues);
			freqOrderedValues[key] = true;
		}
	}

	while(freqOrderedValues.size() > 1)
	{
		huffmanTable::freqValue key0(freqOrderedValues.begin()->first);
		freqOrderedValues.erase(freqOrderedValues.begin());
		huffmanTable::freqValue key1(freqOrderedValues.begin()->first);
		freqOrderedValues.erase(freqOrderedValues.begin());

		key0.m_freq += key1.m_freq;
		m_valuesFreq[key0.m_value].m_freq = key0.m_freq;
		m_valuesFreq[key1.m_value].m_freq = 0;
		m_valuesFreq[key0.m_value].m_codeLength++;

		freqOrderedValues[key0] = true;

		imbxUint32 chainedValue;
		for(chainedValue = key0.m_value; m_valuesFreq[chainedValue].m_nextCode != -1; /* empty */)
		{
			chainedValue = (imbxUint32)m_valuesFreq[chainedValue].m_nextCode;
			m_valuesFreq[chainedValue].m_codeLength++;
		}
		m_valuesFreq[chainedValue].m_nextCode = key1.m_value;
		while(m_valuesFreq[chainedValue].m_nextCode != -1)
		{
			chainedValue = (imbxUint32)m_valuesFreq[chainedValue].m_nextCode;
			m_valuesFreq[chainedValue].m_codeLength++;
		}
	}

	typedef std::map<huffmanTable::lengthValue, bool, huffmanTable::lengthValueCompare> tLengthOrderedMap;
	tLengthOrderedMap lengthOrderedValues ;
	for(size_t findValuesPerLength = 0; findValuesPerLength < m_valuesFreq.size(); ++findValuesPerLength)
	{
		if(m_valuesFreq[findValuesPerLength].m_codeLength != 0)
		{
			huffmanTable::lengthValue key(m_valuesFreq[findValuesPerLength].m_codeLength, (imbxUint32)findValuesPerLength);
			lengthOrderedValues[key] = true;
			m_valuesPerLength[m_valuesFreq[findValuesPerLength].m_codeLength]++;
		}
	}

	long insertPosition = 0;
	for(auto & lengthOrderedValue : lengthOrderedValues)
	{
		m_orderedValues[insertPosition++] = lengthOrderedValue.first.m_value;
	}

	// Reduce the size of the codes' lengths
	for(imbxUint32 reduceLengths=sizeof(m_valuesPerLength)/sizeof(m_valuesPerLength[0]) - 1; reduceLengths>maxCodeLength; --reduceLengths)
	{
		while(m_valuesPerLength[reduceLengths] != 0)
		{
			imbxInt32 reduceLengths1;
			for(reduceLengths1=reduceLengths-2; reduceLengths1 != -1 && m_valuesPerLength[reduceLengths1] == 0; --reduceLengths1){}

			if(reduceLengths1==-1)
			{
				break;
			}
			m_valuesPerLength[reduceLengths]-=2;
			m_valuesPerLength[reduceLengths-1]++;
			m_valuesPerLength[reduceLengths1+1]+=2;
			m_valuesPerLength[reduceLengths1]--;
		}
	}

	// Find the first available length
	for(m_firstValidLength = 1; m_firstValidLength != sizeof(m_valuesPerLength) / sizeof(m_valuesPerLength[0]); ++m_firstValidLength)
	{
		if(m_valuesPerLength[m_firstValidLength] != 0)
		{
			break;
		}
	}

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Calculate the huffman codes from the codes length
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void huffmanTable::calcHuffmanTables()
{
	PUNTOEXE_FUNCTION_START(L"huffmanTable::calcHuffmanTables");

	imbxUint32 huffmanCode = 0;

	imbxUint32 valueIndex = 0;

	::memset(m_minValuePerLength, 0xffffffff, sizeof(m_minValuePerLength));
	::memset(m_maxValuePerLength, 0xffffffff, sizeof(m_maxValuePerLength));

	m_firstValidLength = 0;
	for(imbxUint32 codeLength=1L; codeLength != sizeof(m_valuesPerLength)/sizeof(imbxUint32); ++codeLength)
	{
		if(m_valuesPerLength[codeLength] != 0 && m_firstValidLength == 0)
		{
			m_firstValidLength = codeLength;
		}
		for(imbxUint32 generateCodes = 0; generateCodes<m_valuesPerLength[codeLength]; ++generateCodes)
		{
			if(generateCodes == 0)
			{
				m_minValuePerLength[codeLength]=huffmanCode;
			}
			m_maxValuePerLength[codeLength]=huffmanCode;
			m_valuesToHuffman[m_orderedValues[valueIndex]]=huffmanCode;
			m_valuesToHuffmanLength[m_orderedValues[valueIndex]]=codeLength;
			++valueIndex;
			++huffmanCode;
		}

		huffmanCode<<=1;

	}

	PUNTOEXE_FUNCTION_END();
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Read an Huffman code
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint32 huffmanTable::readHuffmanCode(streamReader* pStream)
{
	PUNTOEXE_FUNCTION_START(L"huffmanTable::readHuffmanCode");

	// Read initial number of bits
	imbxUint32 readBuffer(pStream->readBits(m_firstValidLength));

	// Validate the current Huffman code. If it's OK, then
	//  return the ordered value
	///////////////////////////////////////////////////////////
	if(readBuffer<=m_maxValuePerLength[m_firstValidLength])
	{
		return m_orderedValues[readBuffer - m_minValuePerLength[m_firstValidLength]];
	}

	imbxUint32 orderedValue(m_valuesPerLength[m_firstValidLength]);

	// Scan all the codes sizes
	///////////////////////////////////////////////////////////
	for(imbxUint8 scanSize(m_firstValidLength + 1), missingBits(0); scanSize != sizeof(m_valuesPerLength)/sizeof(m_valuesPerLength[0]); ++scanSize)
	{
		++missingBits;

		// If the active length is empty, then continue the loop
		///////////////////////////////////////////////////////////
		if(m_valuesPerLength[scanSize] == 0)
		{
			continue;
		}

		readBuffer <<= missingBits;
		if(missingBits == 1)
		{
			readBuffer |= pStream->readBit();
		}
		else
		{
			readBuffer |= pStream->readBits(missingBits);
		}

		// Validate the current Huffman code. If it's OK, then
		//  return the ordered value
		///////////////////////////////////////////////////////////
		if(readBuffer<=m_maxValuePerLength[scanSize])
		{
			return m_orderedValues[orderedValue + readBuffer - m_minValuePerLength[scanSize]];
		}

		orderedValue += m_valuesPerLength[scanSize];

		// Reset the number of bits to read in one go
		///////////////////////////////////////////////////////////
		missingBits = 0;

	}

	PUNTOEXE_THROW(huffmanExceptionRead, "Invalid huffman code found while reading from a stream");

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Write an Huffman code
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void huffmanTable::writeHuffmanCode(const imbxUint32 code, streamWriter* pStream)
{
	PUNTOEXE_FUNCTION_START(L"huffmanTable::writeHuffmanCode");

	if(m_valuesToHuffmanLength[code] == 0)
	{
		PUNTOEXE_THROW(huffmanExceptionWrite, "Trying to write an invalid huffman code");
	}
	pStream->writeBits(m_valuesToHuffman[code], m_valuesToHuffmanLength[code]);

	PUNTOEXE_FUNCTION_END();
}

} // namespace puntoexe

