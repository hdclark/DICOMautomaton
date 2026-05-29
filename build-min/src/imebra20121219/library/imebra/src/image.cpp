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

/*! \file image.cpp
    \brief Implementation of the class image.

*/

#include "../../base/include/exception.h"
#include "../include/image.h"
#include "../include/colorTransformsFactory.h"

namespace puntoexe
{

namespace imebra
{

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// image
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
// Create an image with the specified size, colorspace and
//  bit depth
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
ptr<handlers::dataHandlerNumericBase> image::create(
						const imbxUint32 sizeX, 
						const imbxUint32 sizeY, 
						const bitDepth depth, 
						std::wstring inputColorSpace, 
						const imbxUint8 highBit)
{
	PUNTOEXE_FUNCTION_START(L"image::create");

	lockObject lockAccess(this);

	if(sizeX == 0 || sizeY == 0)
	{
		PUNTOEXE_THROW(imageExceptionInvalidSize, "An invalid image's size has been specified");
	}

	// Normalize the color space (remove _420 & _422 and
	//  make it uppercase).
	///////////////////////////////////////////////////////////
	m_colorSpace=transforms::colorTransforms::colorTransformsFactory::normalizeColorSpace(inputColorSpace);

	// Find the number of channels to allocate
	///////////////////////////////////////////////////////////
	m_channelsNumber = transforms::colorTransforms::colorTransformsFactory::getNumberOfChannels(inputColorSpace);
	if(m_channelsNumber == 0)
	{
		PUNTOEXE_THROW(imageExceptionUnknownColorSpace, "Cannot recognize the specified color space");
	}

	// Find the datatype to use to allocate the
	//  buffer (datatypes are in Dicom standard, plus SB
	//  for signed bytes).
	///////////////////////////////////////////////////////////
	m_channelPixelSize = 0;
	imbxUint8 defaultHighBit = 0;

	std::string bufferDataType;

	switch(depth)
	{
	case depthU8:
		bufferDataType = "OB";
		defaultHighBit=7;
		break;
	case depthS8:
		bufferDataType = "SB";
		defaultHighBit=7;
		break;
	case depthU16:
		bufferDataType = "US";
		defaultHighBit=15;
		break;
	case depthS16:
		bufferDataType = "SS";
		defaultHighBit=15;
		break;
	case depthU32:
		bufferDataType = "UL";
		defaultHighBit=31;
		break;
	case depthS32:
		bufferDataType = "SL";
		defaultHighBit=31;
		break;
	default:
		PUNTOEXE_THROW(imageExceptionUnknownDepth, "Unknown depth");
	}

	// Adjust the high bit value
	///////////////////////////////////////////////////////////
	if(highBit == 0 || highBit>defaultHighBit)
		m_highBit=defaultHighBit;
	else
		m_highBit=highBit;

	// If a valid buffer with the same data type is already
	//  allocated then use it.
	///////////////////////////////////////////////////////////
	if(m_buffer == nullptr || !(m_buffer->isReferencedOnce()) )
	{
		ptr<buffer> tempBuffer(new buffer(this, bufferDataType));
		m_buffer = tempBuffer;
	}

	m_sizeX = m_sizeY = 0;
	
	ptr<handlers::dataHandler> imageHandler(m_buffer->getDataHandler(true, sizeX * sizeY * (imbxUint32)m_channelsNumber) );
	if(imageHandler != nullptr)
	{
		m_rowLength = m_channelsNumber*sizeX;
		
		imageHandler->setSize(m_rowLength*sizeY);
		m_channelPixelSize = imageHandler->getUnitSize();

		// Set the attributes
		///////////////////////////////////////////////////////////
		m_imageDepth=depth;
		m_sizeX=sizeX;
		m_sizeY=sizeY;
	}

	return ptr<handlers::dataHandlerNumericBase>(dynamic_cast<handlers::dataHandlerNumericBase*>(imageHandler.get()) );

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Set the depth
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void image::setHighBit(imbxUint32 highBit)
{
	PUNTOEXE_FUNCTION_START(L"image::setHighBit");

	lockObject lockAccess(this);

	m_highBit = highBit;

	PUNTOEXE_FUNCTION_END();
}

void image::setPalette(ptr<palette> imagePalette)
{
	PUNTOEXE_FUNCTION_START(L"image::getPalette");

	lockObject lockAccess(this);

	m_palette = imagePalette;

	PUNTOEXE_FUNCTION_END();
}



///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Retrieve an handler to the image's buffer.
// The image's components are interleaved.
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
ptr<handlers::dataHandlerNumericBase> image::getDataHandler(const bool bWrite, imbxUint32* pRowSize, imbxUint32* pChannelPixelSize, imbxUint32* pChannelsNumber)
{
	PUNTOEXE_FUNCTION_START(L"image::getDataHandler");

	lockObject lockAccess(this);

	if(m_buffer == nullptr)
	{
		return ptr<handlers::dataHandlerNumericBase>(nullptr);
	}

	*pRowSize=m_rowLength;
	*pChannelPixelSize=m_channelPixelSize;
	*pChannelsNumber=m_channelsNumber;

	ptr<handlers::dataHandler> imageHandler(m_buffer->getDataHandler(bWrite, m_sizeX * m_sizeY * m_channelsNumber));

	return ptr<handlers::dataHandlerNumericBase>(dynamic_cast<handlers::dataHandlerNumericBase*>(imageHandler.get()) );

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Return the bit depth
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
image::bitDepth image::getDepth()
{
	PUNTOEXE_FUNCTION_START(L"image::getDepth");

	lockObject lockAccess(this);
	return m_imageDepth;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Return the high bit
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint32 image::getHighBit()
{
	PUNTOEXE_FUNCTION_START(L"image::getHighBit");

	lockObject lockAccess(this);
	return m_highBit;

	PUNTOEXE_FUNCTION_END();
}

ptr<palette> image::getPalette()
{
	PUNTOEXE_FUNCTION_START(L"image::getPalette");

	lockObject lockAccess(this);

	return m_palette;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Returns the colorspace
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
std::wstring image::getColorSpace()
{
	PUNTOEXE_FUNCTION_START(L"image::getColorSpace");

	lockObject lockAccess(this);
	return m_colorSpace;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Returns the numbero of allocated channels
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint32 image::getChannelsNumber()
{
	PUNTOEXE_FUNCTION_START(L"image::getChannelsNumber");

	lockObject lockAccess(this);
	return m_channelsNumber;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Returns the image's size in pixels
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void image::getSize(imbxUint32* pSizeX, imbxUint32* pSizeY)
{
	PUNTOEXE_FUNCTION_START(L"image::getSize");

	lockObject lockAccess(this);

	if(pSizeX)
		*pSizeX=m_sizeX;

	if(pSizeY)
		*pSizeY=m_sizeY;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Returns the image's size in millimeters
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void image::getSizeMm(double* pSizeMmX, double* pSizeMmY)
{
	PUNTOEXE_FUNCTION_START(L"image::getSizeMm");

	lockObject lockAccess(this);

	if(pSizeMmX)
		*pSizeMmX=m_sizeMmX;

	if(pSizeMmY)
		*pSizeMmY=m_sizeMmY;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Set the image's size in millimeters
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void image::setSizeMm(const double sizeMmX, const double sizeMmY)
{
	PUNTOEXE_FUNCTION_START(L"image::setSizeMm");

	lockObject lockAccess(this);

	if(sizeMmX)
		m_sizeMmX=sizeMmX;

	if(sizeMmY)
		m_sizeMmY=sizeMmY;

	PUNTOEXE_FUNCTION_END();
}

} // namespace imebra

} // namespace puntoexe
