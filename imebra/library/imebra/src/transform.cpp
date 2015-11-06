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

/*! \file transform.cpp
    \brief Implementation of the base class used by the transforms.

*/

#include "../../base/include/exception.h"
#include "../include/transform.h"
#include "../include/image.h"
#include "../include/transformHighBit.h"


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
// Declare an input parameter
//
//
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
bool transform::isEmpty()
{
	return false;
}


void transformHandlers::runTransform(
            const ptr<image>& inputImage,
            imbxUint32 inputTopLeftX, imbxUint32 inputTopLeftY, imbxUint32 inputWidth, imbxUint32 inputHeight,
            const ptr<image>& outputImage,
            imbxUint32 outputTopLeftX, imbxUint32 outputTopLeftY)
{
	imbxUint32 rowSize, numPixels, channels;
	ptr<handlers::dataHandlerNumericBase> inputHandler(inputImage->getDataHandler(false, &rowSize, &numPixels, &channels));
	imbxUint32 inputImageWidth, inputImageHeight;
	inputImage->getSize(&inputImageWidth, &inputImageHeight);
	ptr<palette> inputPalette(inputImage->getPalette());
	std::wstring inputColorSpace(inputImage->getColorSpace());
	imbxUint32 inputHighBit(inputImage->getHighBit());
	imbxUint32 inputNumValues((imbxUint32)1 << (inputHighBit + 1));
	imbxInt32 inputMinValue(0);
	image::bitDepth inputDepth(inputImage->getDepth());
	if(inputDepth == image::depthS16 || inputDepth == image::depthS8)
	{
		inputMinValue -= (imbxInt32)(inputNumValues >> 1);
	}

	ptr<handlers::dataHandlerNumericBase> outputHandler(outputImage->getDataHandler(false, &rowSize, &numPixels, &channels));
	imbxUint32 outputImageWidth, outputImageHeight;
	outputImage->getSize(&outputImageWidth, &outputImageHeight);
	ptr<palette> outputPalette(outputImage->getPalette());
	std::wstring outputColorSpace(outputImage->getColorSpace());
	imbxUint32 outputHighBit(outputImage->getHighBit());
	imbxUint32 outputNumValues((imbxUint32)1 << (outputHighBit + 1));
	imbxInt32 outputMinValue(0);
	image::bitDepth outputDepth(outputImage->getDepth());
	if(outputDepth == image::depthS16 || outputDepth == image::depthS8)
	{
		outputMinValue -= (imbxInt32)(outputNumValues >> 1);
	}

	if(isEmpty())
	{
		ptr<transformHighBit> emptyTransform(new transformHighBit);
		emptyTransform->runTransformHandlers(inputHandler, inputImageWidth, inputColorSpace, inputPalette, inputMinValue, inputNumValues,
											 inputTopLeftX, inputTopLeftY, inputWidth, inputHeight,
											 outputHandler, outputImageWidth, outputColorSpace, outputPalette, outputMinValue, outputNumValues,
											 outputTopLeftX, outputTopLeftY);
		return;
	}

	runTransformHandlers(inputHandler, inputImageWidth, inputColorSpace, inputPalette, inputMinValue, inputNumValues,
		inputTopLeftX, inputTopLeftY, inputWidth, inputHeight,
		outputHandler, outputImageWidth, outputColorSpace, outputPalette, outputMinValue, outputNumValues,
		outputTopLeftX, outputTopLeftY);

}


} // namespace transforms

} // namespace imebra

} // namespace puntoexe
