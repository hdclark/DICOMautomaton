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

/*! \file LUT.cpp
    \brief Implementation of the class lut.

*/

#include "../../base/include/exception.h"
#include "../include/LUT.h"
#include "../include/dataHandlerNumeric.h"
#include <cstring>

namespace puntoexe
{

namespace imebra
{

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Destructor
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
lut::~lut()
{
	if(m_pMappedValues)
	{
		delete m_pMappedValues;
	}
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Create a LUT from a data handler
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void lut::setLut(ptr<handlers::dataHandler> pDescriptor, ptr<handlers::dataHandler> pData, std::wstring description)
{
	PUNTOEXE_FUNCTION_START(L"lut::setLut");

	if(pDescriptor->getSize() < 3)
	{
		PUNTOEXE_THROW(lutExceptionCorrupted, "The LUT is corrupted");
	}
	imbxInt32 lutSize=pDescriptor->getSignedLong(0);
	if(lutSize == 0)
		lutSize=0x00010000;
	if(lutSize < 0)
		lutSize&=0x0000FFFF;

	imbxInt32 lutFirstMapped=pDescriptor->getSignedLong(1);
	imbxUint32 lutBits=pDescriptor->getUnsignedLong(2);

	if(pData == nullptr || (imbxUint32)lutSize != pData->getSize())
	{
		PUNTOEXE_THROW(lutExceptionCorrupted, "The LUT is corrupted");
	}

	create(lutSize, lutFirstMapped, (imbxUint8)lutBits, description);

    dynamic_cast<handlers::dataHandlerNumericBase*>(pData.get())->copyTo(m_pMappedValues, lutSize);

    m_bChecked = false; // LUT has to be checked again

	PUNTOEXE_FUNCTION_END();
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Create a LUT
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void lut::create(imbxUint32 size, imbxInt32 firstMapped, imbxUint8 bits, std::wstring description)
{
	PUNTOEXE_FUNCTION_START(L"lut::create");

	// If some values were previously allocated, then remove
	//  them
	///////////////////////////////////////////////////////////
	if(m_pMappedValues)
	{
		delete m_pMappedValues;
		m_pMappedValues= nullptr;
	}
	m_mappedValuesRev.clear();

	m_bChecked = false;

	m_size = 0;

	m_description = description;

	if(size)
	{
		m_size=size;
		m_firstMapped=firstMapped;
		m_bits=bits;
		m_pMappedValues=new imbxInt32[m_size];
	}

	PUNTOEXE_FUNCTION_END();
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Fill the data handlers in a dataset with the lut
//  information
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void lut::fillHandlers(ptr<handlers::dataHandler> pDescriptor, ptr<handlers::dataHandler> pData)
{
	PUNTOEXE_FUNCTION_START(L"lut::fillHandlers");

	pDescriptor->setSize(3);
	imbxUint32 lutSize = getSize();
	if(lutSize == 0x00010000)
	{
		pDescriptor->setSignedLong(0, 0);
	}
	else
	{
		pDescriptor->setUnsignedLong(0, lutSize);
	}

	imbxInt32 lutFirstMapped = getFirstMapped();
	pDescriptor->setSignedLong(1, lutFirstMapped);

	imbxUint8 bits = getBits();
	pDescriptor->setUnsignedLong(2, bits);

	pData->setSize(lutSize);
	dynamic_cast<handlers::dataHandlerNumericBase*>(pData.get())->copyFrom(m_pMappedValues, lutSize);

	PUNTOEXE_FUNCTION_END();
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Retrieve the number of values stored into the LUT
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint32 lut::getSize()
{
	return m_size;
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Check the validity of the data in the LUT
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
bool lut::checkValidDataRange()
{
    if(m_bChecked)
    {
        return m_bValid;
    }
    imbxInt32 maxValue(65535);
    imbxInt32 minValue(-32768);
    if(m_bits == 8)
    {
        maxValue = 255;
        minValue = -128;
    }
    imbxInt32* pScanValues(m_pMappedValues);
    for(imbxUint32 checkData(0); checkData != m_size; ++checkData)
    {
        if(*pScanValues < minValue || *pScanValues > maxValue)
        {
            m_bValid = false;
            m_bChecked = true;
            return m_bValid;
        }
        ++pScanValues;
    }
    m_bValid = true;
    m_bChecked = true;
    return m_bValid;
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Retrieve the first mapped index
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxInt32 lut::getFirstMapped()
{
	return m_firstMapped;
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Insert a new value into the LUT
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void lut::setLutValue(imbxInt32 startValue, imbxInt32 lutValue)
{
	PUNTOEXE_FUNCTION_START(L"lut::setLutValue");

	if(startValue<m_firstMapped)
	{
		PUNTOEXE_THROW(lutExceptionWrongIndex, "The start index is below the first mapped index");
	}
	startValue-=m_firstMapped;
	if(startValue<(imbxInt32)m_size)
	{
		m_pMappedValues[startValue]=lutValue;
		m_mappedValuesRev.clear();
	}

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Retrieve the lut's description.
// The returned value must be removed by the client
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
std::wstring lut::getDescription()
{
	return m_description;
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Return the lut's bits
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint8 lut::getBits()
{
	return m_bits;
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Lookup the requested value
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxInt32 lut::mappedValue(imbxInt32 id)
{
	// The LUT's size is zero, return
	///////////////////////////////////////////////////////////
	if(m_size == 0)
	{
		return 0;
	}

	// Subtract the first mapped value
	///////////////////////////////////////////////////////////
	id -= m_firstMapped;

	if(id <= 0)
	{
		return m_pMappedValues[0];
	}
	if(id < (imbxInt32)m_size)
	{
		return m_pMappedValues[id];
	}
	return m_pMappedValues[m_size-1];
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Copy the palette's data to an array of imbxInt32
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void lut::copyToInt32(imbxInt32* pDestination, imbxUint32 destSize, imbxInt32* pFirstMapped)
{
	if(destSize > m_size)
	{
		destSize = m_size;
	}
	::memcpy(pDestination, m_pMappedValues, destSize*sizeof(imbxInt32));
	*pFirstMapped = m_firstMapped;
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Inverse Lookup table
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxInt32 lut::mappedValueRev(imbxInt32 lutValue)
{
	if(m_size == 0)
	{
		return 0;
	}

	if(m_mappedValuesRev.size() == 0)
	{
		for(imbxUint32 reverseLUT = 0; reverseLUT < m_size; ++reverseLUT)
		{
			m_mappedValuesRev[m_pMappedValues[reverseLUT]]=reverseLUT;
		}
	}

	if(lutValue<=m_mappedValuesRev.begin()->first)
		return m_mappedValuesRev.begin()->second;

	std::map<imbxInt32, imbxInt32>::iterator lutIterator;
	lutIterator=m_mappedValuesRev.end();
	--lutIterator;
	if(lutValue>=lutIterator->first)
		return lutIterator->second;

	lutIterator=m_mappedValuesRev.lower_bound(lutValue);

	if(lutIterator!=m_mappedValuesRev.end())
		return lutIterator->second;

	return 0;
}


palette::palette(ptr<lut> red, ptr<lut> green, ptr<lut> blue):
m_redLut(red), m_greenLut(green), m_blueLut(blue)
{}

void palette::setLuts(ptr<lut> red, ptr<lut> green, ptr<lut> blue)
{
	m_redLut = red;
	m_greenLut = green;
	m_blueLut = blue;
}

ptr<lut> palette::getRed()
{
	return m_redLut;
}

ptr<lut> palette::getGreen()
{
	return m_greenLut;
}

ptr<lut> palette::getBlue()
{
	return m_blueLut;
}

} // namespace imebra

} // namespace puntoexe
