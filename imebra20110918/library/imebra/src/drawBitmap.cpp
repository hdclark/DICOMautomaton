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

/*! \file drawBitmap.cpp
    \brief Implementation of the transform drawBitmap.

*/

#include "../include/drawBitmap.h"
#include "../include/image.h"
#include "../include/colorTransformsFactory.h"
#include "../include/transformHighBit.h"

namespace puntoexe
{

namespace imebra
{


drawBitmap::drawBitmap(ptr<image> sourceImage, ptr<transforms::transformsChain> transformsChain):
	m_image(sourceImage), m_transformsChain(new transforms::transformsChain)
{
	if(transformsChain != 0 && !transformsChain->isEmpty())
	{
		m_transformsChain->addTransform(transformsChain);
	}

	// Allocate the transforms that obtain an RGB24 image
	std::wstring initialColorSpace;
	if(m_transformsChain->isEmpty())
	{
		initialColorSpace = m_image->getColorSpace();
	}
	else
	{
		ptr<image> startImage(m_transformsChain->allocateOutputImage(m_image, 1, 1));
		initialColorSpace = startImage->getColorSpace();
	}
	transforms::colorTransforms::colorTransformsFactory* pColorTransformsFactory(transforms::colorTransforms::colorTransformsFactory::getColorTransformsFactory());
	ptr<transforms::colorTransforms::colorTransform> rgbColorTransform(pColorTransformsFactory->getTransform(initialColorSpace, L"RGB"));
	if(rgbColorTransform != 0)
	{
		m_transformsChain->addTransform(rgbColorTransform);
	}

	imbxUint32 width, height;
	m_image->getSize(&width, &height);
	if(m_transformsChain->isEmpty())
	{
		m_finalImage = m_image;
	}
	else
	{
		m_finalImage = m_transformsChain->allocateOutputImage(m_image, width, 1);
	}

	if(m_finalImage->getDepth() != image::depthU8 || m_finalImage->getHighBit() != 7)
	{
		m_finalImage = new image;
		m_finalImage->create(width, 1, image::depthU8, L"RGB", 7);
		m_transformsChain->addTransform(new transforms::transformHighBit);
	}
}


} // namespace imebra

} // namespace puntoexe
