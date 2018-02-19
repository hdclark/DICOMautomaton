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

/*! \file colorTransformsFactory.cpp
    \brief Implementation of the colorTransformsFactory class.

*/

#include "../../base/include/exception.h"
#include "../include/colorTransformsFactory.h"
#include "../include/transformsChain.h"
#include "../include/image.h"

namespace puntoexe
{

namespace imebra
{

namespace transforms
{

namespace colorTransforms
{

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
//
// colorTransformsFactory
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
// Force the construction of the factory before main()
//  starts
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static colorTransformsFactory::forceColorTransformsFactoryConstruction forceConstruction;


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Register a color transform
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void colorTransformsFactory::registerTransform(ptr<colorTransform> newColorTransform)
{
	PUNTOEXE_FUNCTION_START(L"colorTransformsFactory::registerTransform");

	lockObject lockAccess(this);

	m_transformsList.push_back(newColorTransform);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Get a pointer to the colorTransformsFactory instance
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
ptr<colorTransformsFactory> colorTransformsFactory::getColorTransformsFactory()
{
	static ptr<colorTransformsFactory> m_transformFactory(new colorTransformsFactory);
	return m_transformFactory;
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Normalize the color space name
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////;
std::wstring colorTransformsFactory::normalizeColorSpace(std::wstring colorSpace)
{
	PUNTOEXE_FUNCTION_START(L"colorTransformsFactory::normalizeColorSpace");

	std::wstring normalizedColorSpace;

        size_t c42position = colorSpace.find(L"_42");
	if(c42position != colorSpace.npos)
		normalizedColorSpace=colorSpace.substr(0, c42position);
	else
		normalizedColorSpace=colorSpace;

	// Colorspace transformed to uppercase
	///////////////////////////////////////////////////////////
	for(wchar_t & adjustColorSpace : normalizedColorSpace)
	{
		if(adjustColorSpace >= L'a' && adjustColorSpace <= L'z')
			adjustColorSpace -= L'a'-L'A';
	}

	return normalizedColorSpace;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Returns true if the color space name represents a
//  monochrome color space
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
bool colorTransformsFactory::isMonochrome(std::wstring colorSpace)
{
	PUNTOEXE_FUNCTION_START(L"colorTransformsFactory::isMonochrome");

	std::wstring normalizedColorSpace = normalizeColorSpace(colorSpace);
	return (normalizedColorSpace == L"MONOCHROME1" || normalizedColorSpace == L"MONOCHROME2");

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Returns true if the color space name represents an
//  horizontally subsampled color space
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
bool colorTransformsFactory::isSubsampledX(std::wstring colorSpace)
{
	PUNTOEXE_FUNCTION_START(L"colorTransformsFactory::isSubsampledX");

	return (colorSpace.find(L"_42")!=colorSpace.npos);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Returns true if the color space name represents a
//  vertically subsampled color space
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
bool colorTransformsFactory::isSubsampledY(std::wstring colorSpace)
{
	PUNTOEXE_FUNCTION_START(L"colorTransformsFactory::isSubsampledY");

	return (colorSpace.find(L"_420")!=colorSpace.npos);

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Returns true if the specified color space can be
//  subsampled
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
bool colorTransformsFactory::canSubsample(std::wstring colorSpace)
{
	PUNTOEXE_FUNCTION_START(L"colorTransformsFactory::canSubsample");

	std::wstring normalizedColorSpace = normalizeColorSpace(colorSpace);
	return normalizedColorSpace.find(L"YBR_") == 0;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Subsample a color space name
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
std::wstring colorTransformsFactory::makeSubsampled(std::wstring colorSpace, bool bSubsampleX, bool bSubsampleY)
{
	PUNTOEXE_FUNCTION_START(L"colorTransformsFactory::makeSubsampled");

	std::wstring normalizedColorSpace = normalizeColorSpace(colorSpace);
	if(!canSubsample(normalizedColorSpace))
	{
		return normalizedColorSpace;
	}
	if(bSubsampleY)
	{
		return normalizedColorSpace + L"_420";
	}
	if(bSubsampleX)
	{
		return normalizedColorSpace + L"_422";
	}
	return normalizedColorSpace;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Returns the number of channels for the specified
//  color space
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
imbxUint32 colorTransformsFactory::getNumberOfChannels(std::wstring colorSpace)
{
	PUNTOEXE_FUNCTION_START(L"colorTransformsFactory::getNumberOfChannels");

	std::wstring normalizedColorSpace = normalizeColorSpace(colorSpace);

	struct sColorSpace
	{
		sColorSpace(const wchar_t* colorSpace, imbxUint8 channelsNumber): m_colorSpace(colorSpace), m_channelsNumber(channelsNumber){}
		std::wstring m_colorSpace;
		imbxUint8 m_channelsNumber;
	};

	static sColorSpace imbxColorSpaces[]=
	{
		sColorSpace(L"RGB", 0x3),
		sColorSpace(L"YBR_FULL", 0x3),
		sColorSpace(L"YBR_PARTIAL", 0x3),
		sColorSpace(L"YBR_RCT", 0x3),
		sColorSpace(L"YBR_ICT", 0x3),
		sColorSpace(L"PALETTE COLOR", 0x1),
		sColorSpace(L"CMYK", 0x4),
		sColorSpace(L"CMY", 0x3),
		sColorSpace(L"MONOCHROME2", 0x1),
		sColorSpace(L"MONOCHROME1", 0x1),
		sColorSpace(L"", 0x0)
	};

	for(imbxUint8 findColorSpace = 0; imbxColorSpaces[findColorSpace].m_channelsNumber != 0x0; ++findColorSpace)
	{
		if(imbxColorSpaces[findColorSpace].m_colorSpace == normalizedColorSpace)
		{
			return imbxColorSpaces[findColorSpace].m_channelsNumber;
		}
	}

	return 0;

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
//
//
// Returns a transform that can convert a color space into
//  another
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
ptr<colorTransform> colorTransformsFactory::getTransform(std::wstring startColorSpace, std::wstring endColorSpace)
{
	PUNTOEXE_FUNCTION_START(L"colorTransformsFactory::getTransform");

	lockObject lockAccess(this);

	std::wstring normalizedStartColorSpace = normalizeColorSpace(startColorSpace);
	std::wstring normalizedEndColorSpace = normalizeColorSpace(endColorSpace);

	if(normalizedStartColorSpace == normalizedEndColorSpace)
	{
		return ptr<colorTransform>(nullptr);
	}

	for(auto & scanSingleTransform : m_transformsList)
	{
		if( scanSingleTransform->getInitialColorSpace() == normalizedStartColorSpace && 
			scanSingleTransform->getFinalColorSpace() == normalizedEndColorSpace)
		{
			ptr<colorTransform> newTransform = scanSingleTransform->createColorTransform();
			return newTransform;
		}
	}

	for(tTransformsList::iterator scanMultipleTransforms = m_transformsList.begin(); scanMultipleTransforms != m_transformsList.end(); ++scanMultipleTransforms)
	{
		if( (*scanMultipleTransforms)->getInitialColorSpace() != normalizedStartColorSpace)
		{
			continue;
		}

		for(auto & secondTransform : m_transformsList)
		{
			if( secondTransform->getFinalColorSpace() != normalizedEndColorSpace ||
				secondTransform->getInitialColorSpace() != (*scanMultipleTransforms)->getFinalColorSpace())
			{
				continue;
			}

			ptr<colorTransform> newTransform0 = (*scanMultipleTransforms)->createColorTransform();
			ptr<colorTransform> newTransform1 = secondTransform->createColorTransform();

			ptr<transformsChain> chain(new transformsChain);
			chain->addTransform(newTransform0);
			chain->addTransform(newTransform1);

			return chain;
		}
	}

	PUNTOEXE_THROW(colorTransformsFactoryExceptionNoTransform, "There isn't any transform that can convert between the specified color spaces");

	PUNTOEXE_FUNCTION_END();
}

} // namespace colorTransforms

} // namespace transforms

} // namespace imebra

} // namespace puntoexe
