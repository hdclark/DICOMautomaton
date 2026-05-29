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

/*! \file YBRFULLToRGB.h
    \brief Declaration of the class YBRFULLToRGB.

*/

#if !defined(imebraYBRFULLToRGB_E27C63E7_A907_4899_9BD3_8026AD7D110C__INCLUDED_)
#define imebraYBRFULLToRGB_E27C63E7_A907_4899_9BD3_8026AD7D110C__INCLUDED_

#include "colorTransform.h"


///////////////////////////////////////////////////////////
//
// Everything is in the namespace puntoexe::imebra
//
///////////////////////////////////////////////////////////
namespace puntoexe
{

namespace imebra
{

namespace transforms
{

namespace colorTransforms
{

/// \addtogroup group_transforms
///
/// @{

///////////////////////////////////////////////////////////
/// \brief Transforms an image from the colorspace 
///         YBR_FULL into the color space RGB.
///
/// The input image has to have the colorspace YBR_FULL,
///  while the output image is created by the transform
///  and will have the colorspace RGB.
///
///////////////////////////////////////////////////////////
class YBRFULLToRGB: public colorTransform
{
public:
	std::wstring getInitialColorSpace() override;
	std::wstring getFinalColorSpace() override;
	ptr<colorTransform> createColorTransform() override;

        DEFINE_RUN_TEMPLATE_TRANSFORM;

        template <class inputType, class outputType>
        void templateTransform(
            inputType* inputHandlerData, size_t /* inputHandlerSize */, imbxUint32 inputHandlerWidth, const std::wstring& inputHandlerColorSpace,
            ptr<palette> /* inputPalette */,
            imbxInt32 inputHandlerMinValue, imbxUint32 inputHandlerNumValues,
            imbxInt32 inputTopLeftX, imbxInt32 inputTopLeftY, imbxInt32 inputWidth, imbxInt32 inputHeight,
            outputType* outputHandlerData, size_t /* outputHandlerSize */, imbxInt32 outputHandlerWidth, const std::wstring& outputHandlerColorSpace,
            ptr<palette> /* outputPalette */,
            imbxInt32 outputHandlerMinValue, imbxUint32 outputHandlerNumValues,
            imbxInt32 outputTopLeftX, imbxInt32 outputTopLeftY)

        {
            checkColorSpaces(inputHandlerColorSpace, outputHandlerColorSpace);

            inputType* pInputMemory(inputHandlerData);
            outputType* pOutputMemory(outputHandlerData);

            pInputMemory += (inputTopLeftY * inputHandlerWidth + inputTopLeftX) * 3;
            pOutputMemory += (outputTopLeftY * outputHandlerWidth + outputTopLeftX) * 3;
            
            imbxInt32 inputMiddleValue(inputHandlerMinValue + inputHandlerNumValues / 2);
            imbxInt32 sourceY, sourceB, sourceR, destination;



            if(inputHandlerNumValues == outputHandlerNumValues)
            {
                for(; inputHeight != 0; --inputHeight)
                {
                    for(int scanPixels(inputWidth); scanPixels != 0; --scanPixels)
                    {
                        sourceY = (imbxInt32) (*(pInputMemory++));
                        sourceB = (imbxInt32) (*(pInputMemory++)) - inputMiddleValue;
                        sourceR = (imbxInt32) (*(pInputMemory++)) - inputMiddleValue;

                        destination = sourceY + ((22970 * sourceR + 8192) >> 14);
                        if(destination < 0)
                        {
                            *(pOutputMemory++) = (outputType)outputHandlerMinValue;
                        }
                        else if (destination >= (imbxInt32)inputHandlerNumValues)
                        {
                            *(pOutputMemory++) = (outputType)(outputHandlerMinValue + outputHandlerNumValues - 1);
                        }
                        else
                        {
                            *(pOutputMemory++) = (outputType)(outputHandlerMinValue + destination);
                        }

                        destination = sourceY - ((5638 * sourceB + 11700 * sourceR + 8192) >> 14);
                        if(destination < 0)
                        {
                            *(pOutputMemory++) = (outputType)outputHandlerMinValue;
                        }
                        else if (destination >= (imbxInt32)inputHandlerNumValues)
                        {
                            *(pOutputMemory++) = (outputType)(outputHandlerMinValue + outputHandlerNumValues - 1);
                        }
                        else
                        {
                            *(pOutputMemory++) = (outputType)(outputHandlerMinValue + destination);
                        }

                        destination = sourceY + ((29032 * sourceB + 8192) >> 14);
                        if(destination < 0)
                        {
                            *(pOutputMemory++) = (outputType)outputHandlerMinValue;
                        }
                        else if (destination >= (imbxInt32)inputHandlerNumValues)
                        {
                            *(pOutputMemory++) = (outputType)(outputHandlerMinValue + outputHandlerNumValues - 1);
                        }
                        else
                        {
                            *(pOutputMemory++) = (outputType)(outputHandlerMinValue + destination);
                        }
                    }
                    pInputMemory += (inputHandlerWidth - inputWidth) * 3;
                    pOutputMemory += (outputHandlerWidth - inputWidth) * 3;
                }
            }
            else
            {
                for(; inputHeight != 0; --inputHeight)
                {
                    for(int scanPixels(inputWidth); scanPixels != 0; --scanPixels)
                    {
                        sourceY = (imbxInt32)( *(pInputMemory++) );
                        sourceB = (imbxInt32)( *(pInputMemory++) ) - inputMiddleValue;
                        sourceR = (imbxInt32)( *(pInputMemory++) ) - inputMiddleValue;

                        destination = sourceY + ((22970 * sourceR + 8192) >> 14);
                        if(destination < 0)
                        {
                            *(pOutputMemory++) = (outputType)outputHandlerMinValue;
                        }
                        else if (destination >= (imbxInt32)inputHandlerNumValues)
                        {
                            *(pOutputMemory++) = (outputType)(outputHandlerMinValue + outputHandlerNumValues - 1);
                        }
                        else
                        {
                            *(pOutputMemory++) = (outputType)(outputHandlerMinValue + (destination * outputHandlerNumValues) / inputHandlerNumValues);
                        }

                        destination = sourceY - ((5638 * sourceB + 11700 * sourceR + 8192) >> 14);
                        if(destination < 0)
                        {
                            *(pOutputMemory++) = (outputType)outputHandlerMinValue;
                        }
                        else if (destination >= (imbxInt32)inputHandlerNumValues)
                        {
                            *(pOutputMemory++) = (outputType)(outputHandlerMinValue + outputHandlerNumValues - 1);
                        }
                        else
                        {
                            *(pOutputMemory++) = (outputType)(outputHandlerMinValue + (destination * outputHandlerNumValues) / inputHandlerNumValues);
                        }

                        destination = sourceY + ((29032 * sourceB + 8192) >> 14);
                        if(destination < 0)
                        {
                            *(pOutputMemory++) = (outputType)outputHandlerMinValue;
                        }
                        else if (destination >= (imbxInt32)inputHandlerNumValues)
                        {
                            *(pOutputMemory++) = (outputType)(outputHandlerMinValue + outputHandlerNumValues - 1);
                        }
                        else
                        {
                            *(pOutputMemory++) = (outputType)(outputHandlerMinValue + (destination * outputHandlerNumValues) / inputHandlerNumValues);
                        }
                    }
                    pInputMemory += (inputHandlerWidth - inputWidth) * 3;
                    pOutputMemory += (outputHandlerWidth - inputWidth) * 3;
                }
            }
        }
};

/// @}

} // namespace colorTransforms

} // namespace transforms

} // namespace imebra

} // namespace puntoexe

#endif // !defined(imebraYBRFULLToRGB_E27C63E7_A907_4899_9BD3_8026AD7D110C__INCLUDED_)
