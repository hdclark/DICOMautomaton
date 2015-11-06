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

/*! \file modalityVOILUT.cpp
    \brief Implementation of the class modalityVOILUT.

*/

#include "../../base/include/exception.h"
#include "../include/modalityVOILUT.h"
#include "../include/dataSet.h"

namespace puntoexe
{

namespace imebra
{

namespace transforms
{


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Modality VOILUT transform
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
modalityVOILUT::modalityVOILUT(ptr<dataSet> pDataSet):
		m_pDataSet(pDataSet), m_voiLut(pDataSet->getLut(0x0028, 0x3000, 0)), m_rescaleIntercept(pDataSet->getDouble(0x0028, 0, 0x1052, 0x0)), m_rescaleSlope(1.0), m_bEmpty(true)

{
	ptr<handlers::dataHandler> rescaleHandler(m_pDataSet->getDataHandler(0x0028, 0, 0x1053, 0x0, false));
	if(rescaleHandler != 0)
	{
		m_rescaleSlope = rescaleHandler->getDouble(0);
		m_bEmpty = false;
	}
	if(m_voiLut != 0 && m_voiLut->getSize() != 0)
	{
		m_bEmpty = false;
	}

}

bool modalityVOILUT::isEmpty()
{
	return m_bEmpty;
}


ptr<image> modalityVOILUT::allocateOutputImage(ptr<image> pInputImage, imbxUint32 width, imbxUint32 height)
{
	if(isEmpty())
	{
		ptr<image> newImage(new image);
		newImage->create(width, height, pInputImage->getDepth(), pInputImage->getColorSpace(), pInputImage->getHighBit());
		return newImage;
	}

	// LUT
	///////////////////////////////////////////////////////////
	if(m_voiLut != 0 && m_voiLut->getSize() != 0 && m_voiLut->checkValidDataRange())
	{
		imbxUint8 bits(m_voiLut->getBits());

		// Look for negative outputs
		bool bNegative(false);
		for(imbxInt32 index(m_voiLut->getFirstMapped()), size(m_voiLut->getSize()); !bNegative && size != 0; --size, ++index)
		{
			bNegative = (m_voiLut->mappedValue(index) < 0);
		}

		image::bitDepth depth;
		if(bNegative)
		{
			depth = bits > 8 ? image::depthS16 : image::depthS8;
		}
		else
		{
			depth = bits > 8 ? image::depthU16 : image::depthU8;
		}
		ptr<image> returnImage(new image);
		returnImage->create(width, height, depth, L"MONOCHROME2", bits - 1);
		return returnImage;
	}

	// Rescale
	///////////////////////////////////////////////////////////
	if(m_rescaleSlope == 0)
	{
		ptr<image> returnImage(new image);
		returnImage->create(width, height, pInputImage->getDepth(), L"MONOCHROME2", pInputImage->getHighBit());
		return returnImage;
	}

	image::bitDepth inputDepth(pInputImage->getDepth());
	imbxUint32 highBit(pInputImage->getHighBit());

	imbxInt32 value0 = 0;
	imbxInt32 value1 = ((imbxInt32)1 << (highBit + 1)) - 1;
	if(inputDepth == image::depthS16 || inputDepth == image::depthS8)
	{
		value0 = ((imbxInt32)(-1) << highBit);
		value1 = ((imbxInt32)1 << highBit);
	}
	imbxInt32 finalValue0((imbxInt32) ((double)value0 * m_rescaleSlope + m_rescaleIntercept + 0.5) );
	imbxInt32 finalValue1((imbxInt32) ((double)value1 * m_rescaleSlope + m_rescaleIntercept + 0.5) );

	imbxInt32 minValue, maxValue;
	if(finalValue0 < finalValue1)
	{
		minValue = finalValue0;
		maxValue = finalValue1;
	}
	else
	{
		minValue = finalValue1;
		maxValue = finalValue0;
	}

	ptr<image> returnImage(new image);
	if(minValue >= 0 && maxValue <= 255)
	{
		returnImage->create(width, height, image::depthU8, L"MONOCHROME2", 7);
		return returnImage;
	}
	if(minValue >= -128 && maxValue <= 127)
	{
		returnImage->create(width, height, image::depthS8, L"MONOCHROME2", 7);
		return returnImage;
	}
	if(minValue >= 0 && maxValue <= 65535)
	{
		returnImage->create(width, height, image::depthU16, L"MONOCHROME2", 15);
		return returnImage;
	}
	if(minValue >= -32768 && maxValue <= 32767)
	{
		returnImage->create(width, height, image::depthS16, L"MONOCHROME2", 15);
		return returnImage;
	}
	returnImage->create(width, height, image::depthS32, L"MONOCHROME2", 31);
	return returnImage;
}



} // namespace transforms

} // namespace imebra

} // namespace puntoexe
