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

/*! \file transformsChain.cpp
    \brief Implementation of the class transformsChain.

*/

#include "../../base/include/exception.h"
#include "../include/transformsChain.h"
#include "../include/image.h"
#include "../include/dataSet.h"
#include "../include/transformHighBit.h"

namespace puntoexe
{

namespace imebra
{

namespace transforms
{



transformsChain::transformsChain():
        m_inputWidth(0),
        m_inputHeight(0),
	m_inputDepth(image::endOfDepths),
	m_inputHighBit(0),
	m_outputDepth(image::endOfDepths),
	m_outputHighBit(0)
{}

///////////////////////////////////////////////////////////
//
// Add a new transform to the chain
//
///////////////////////////////////////////////////////////
void transformsChain::addTransform(ptr<transform> pTransform)
{
	PUNTOEXE_FUNCTION_START(L"transformsChain::addTransform");

        if(pTransform != 0 && !pTransform->isEmpty())
        {
            // Store the transform in the chain.
            ///////////////////////////////////////////////////////////
            m_transformsList.push_back(pTransform);
        }

	PUNTOEXE_FUNCTION_END();
}


///////////////////////////////////////////////////////////
//
// Returns true if no transform has been defined
//
///////////////////////////////////////////////////////////
bool transformsChain::isEmpty()
{
	return m_transformsList.empty();
}


void transformsChain::runTransform(
            const ptr<image>& inputImage,
            imbxUint32 inputTopLeftX, imbxUint32 inputTopLeftY, imbxUint32 inputWidth, imbxUint32 inputHeight,
            const ptr<image>& outputImage,
            imbxUint32 outputTopLeftX, imbxUint32 outputTopLeftY)
{
	if(isEmpty())
	{
		ptr<transformHighBit> highBit(new transformHighBit);
		highBit->runTransform(inputImage, inputTopLeftX, inputTopLeftY, inputWidth, inputHeight, outputImage, outputTopLeftX, outputTopLeftY);
		return;
	}

	if(m_transformsList.size() == 1)
	{
		m_transformsList.front()->runTransform(inputImage, inputTopLeftX, inputTopLeftY, inputWidth, inputHeight,
			outputImage, outputTopLeftX, outputTopLeftY);
		return;
	}

	// Get the position of the last transform
	///////////////////////////////////////////////////////////
	tTransformsList::iterator lastTransform(m_transformsList.end());
	--lastTransform;

	std::wstring inputColorSpace(inputImage->getColorSpace());
	image::bitDepth inputDepth(inputImage->getDepth());
	imbxUint32 inputHighBit(inputImage->getHighBit());
	std::wstring outputColorSpace(outputImage->getColorSpace());
	image::bitDepth outputDepth(outputImage->getDepth());
	imbxUint32 outputHighBit(outputImage->getHighBit());
	imbxUint32 allocateRows = 65536 / inputWidth;
	if(allocateRows < 1)
	{
		allocateRows = 1;
	}
	if(allocateRows > inputHeight)
	{
		allocateRows = inputHeight;
	}

	// Allocate temporary images
	///////////////////////////////////////////////////////////
	if(
		m_inputWidth != inputWidth ||
		m_inputHeight != inputHeight ||
		m_inputColorSpace != inputColorSpace ||
		m_inputDepth != inputDepth ||
		m_inputHighBit != inputHighBit ||
		m_outputColorSpace != outputColorSpace ||
		m_outputDepth != outputDepth ||
		m_outputHighBit != outputHighBit)
	{
		m_inputWidth = inputWidth;
		m_inputHeight = inputHeight;
		m_inputColorSpace = inputColorSpace;
		m_inputDepth = inputDepth;
		m_inputHighBit = inputHighBit;
		m_outputColorSpace = outputColorSpace;
		m_outputDepth = outputDepth;
		m_outputHighBit = outputHighBit;

		m_temporaryImages.clear();
		tTransformsList::iterator scanTransforms(m_transformsList.begin());
		m_temporaryImages.push_back((*scanTransforms)->allocateOutputImage(inputImage, inputWidth, allocateRows));
		while(++scanTransforms != lastTransform)
		{
			m_temporaryImages.push_back((*scanTransforms)->allocateOutputImage(m_temporaryImages.back(), inputWidth, allocateRows));
		}
	}

	// Run all the transforms. Split the images into several
	//  parts
	///////////////////////////////////////////////////////////
	while(inputHeight != 0)
	{
		imbxUint32 rows = allocateRows;
		if(rows > inputHeight)
		{
			rows = inputHeight;
		}
		inputHeight -= rows;
		
		tTransformsList::iterator scanTransforms(m_transformsList.begin());
		tTemporaryImagesList::iterator scanTemporaryImages(m_temporaryImages.begin());
		
		(*scanTransforms)->runTransform(inputImage, inputTopLeftX, inputTopLeftY, inputWidth, rows, *scanTemporaryImages, 0, 0);
		inputTopLeftY += rows;

		while(++scanTransforms != lastTransform)
		{
			ptr<image> temporaryInput(*(scanTemporaryImages++));
			ptr<image> temporaryOutput(*scanTemporaryImages);

			(*scanTransforms)->runTransform(temporaryInput, 0, 0, inputWidth, rows, temporaryOutput, 0, 0);
		}

		m_transformsList.back()->runTransform(*scanTemporaryImages, 0, 0, inputWidth, rows, outputImage, outputTopLeftX, outputTopLeftY);
		outputTopLeftY += rows;
	}
}


ptr<image> transformsChain::allocateOutputImage(ptr<image> pInputImage, imbxUint32 width, imbxUint32 height)
{
	if(isEmpty())
	{
		ptr<image> newImage(new image);
		newImage->create(width, height, pInputImage->getDepth(), pInputImage->getColorSpace(), pInputImage->getHighBit());
		return newImage;
	}

	if(m_transformsList.size() == 1)
	{
		return m_transformsList.front()->allocateOutputImage(pInputImage, width, height);
	}

	// Get the position of the last transform
	///////////////////////////////////////////////////////////
	tTransformsList::iterator lastTransform(m_transformsList.end());
	--lastTransform;

	ptr<image> temporaryImage;

	for(tTransformsList::iterator scanTransforms(m_transformsList.begin()); scanTransforms != lastTransform; ++scanTransforms)
	{
		if(scanTransforms == m_transformsList.begin())
		{
			temporaryImage = (*scanTransforms)->allocateOutputImage(pInputImage, 1, 1);
		}
		else
		{
			ptr <image> newImage( (*scanTransforms)->allocateOutputImage(temporaryImage, 1, 1) );
			temporaryImage = newImage;
		}
	}
	return (*lastTransform)->allocateOutputImage(temporaryImage, width, height);


}


} // namespace transforms

} // namespace imebra

} // namespace puntoexe
